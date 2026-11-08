/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* engine.h. Compiled on its own at -O2 (fm1.toml o2). Floats only: single-precision FPU (no double). */
#include "engine.h"
#include "fastmath.h"
#include "plat.h"                                /* plat_ticks24: the profiler */
#include "echo.h"
#include "voices.h"
#include "riddim.h"
#include "bigknob.h"
#include "spring.h"
#include "dsp.h"

cmdq_t eng_q;

/* ------------------------------------------------------------------ ramps --- */
/* A gain that moves to its target in a straight line over RAMP_N samples (2.9 ms): a mute, solo or level change
 * never clicks, and lands at a known time. Each strip owns its ramps (no state shared between modules). */
#define RAMP_N 128u
typedef struct {
    float g, step, target;
    uint32_t left;
} ramp_t;

static void ramp_to(ramp_t *r, float t)
{
    if (t == r->target)
        return;
    r->target = t;
    r->step = (t - r->g) * (1.0f / (float)RAMP_N);
    r->left = RAMP_N;
}
static inline float ramp_next(ramp_t *r)
{
    if (r->left) {
        r->g += r->step;
        if (!--r->left)
            r->g = r->target;
    }
    return r->g;
}
static void ramp_jump(ramp_t *r)                 /* to the target at once (a silent source: no one hears it) */
{
    r->g = r->target;
    r->left = 0;
}

/* ------------------------------------------------------------------ state --- */
typedef struct {
    ramp_t dry, echo, spring;                    /* post-fader, post-mute gains */
    float tone_ph;                               /* P_DEV_TONES: this channel's test tone */
    volatile float peak;                         /* post-fader, the last block's */
} strip_t;

static int16_t ev[P_NIDS];                       /* the applied values (the main loop reads them back); 16 bits: pdesc_t */
static strip_t strip[NCH];
static float buf[ENG_BLOCK], dry[ENG_BLOCK], ebus[ENG_BLOCK], sbus[ENG_BLOCK], eret[ENG_BLOCK], sret[ENG_BLOCK];
static volatile float echo_peak, spring_peak, echo_ret_peak, spring_ret_peak;
static ramp_t echo_ret, spring_ret;              /* the returns' levels */
static int16_t kick_last;
static float bus_echo_g;                         /* the echo's loop gain this block (the cross-sends' cap) */
static float sret_last[ENG_BLOCK];               /* the spring's return a block ago: into the echo (cross-send) */
static uint32_t sret_last_n;
static ramp_t xs_e2s, xs_s2e;                    /* the cross-sends' levels (P1.3) */
#define XS_LOOP_CAP 0.5f                         /* echo -> spring -> echo: their product at most this */                        /* P_SPRING_KICK as it was: a strike on 0 -> 1 */
static volatile uint32_t crashes;
static volatile int32_t echo_div_now;
static volatile uint32_t lock_now = 0xFFFu;
static bigknob_t bk;                             /* the Big Knob (P1.1) */
static int32_t bk_step = -1;                     /* its step set */
static volatile float out_peak;                  /* the output's, before MASTER (tests, meters)      /* the key lock's allowed pitch classes (P0.8), this block's            /* the division in use (the longest that fits, at this tempo) */
static volatile uint32_t cmds;
static float gain_now;                           /* MASTER, glided: no step at a block's edge */

/* the transport: an exact integer clock. Position = t_beat + t_phase / BEAT_UNITS beats; each sample adds the tempo
 * (BPM x 10) to t_phase, so a beat is BEAT_UNITS / tempo samples, with no rounding that adds up (no drift) */
#define BEAT_UNITS 26460000u                     /* 600 x 44100 */
static uint32_t t_beat, t_phase, t_samples;      /* beats and samples since the start */
static int t_playing;
static volatile uint32_t t_fire_at, t_fires;     /* the last cue's sample (since the start), cues fired */

/* cued commands (CMD_CUE): each fires once the position reaches (fb, fp), in the order they came */
#define NCUES 32u
typedef struct {
    uint32_t fb, fp;
    uint16_t id;
    int16_t v;
    uint8_t op;                                  /* CMD_SET (a parameter) or CMD_TEMPLATE */
} cue_t;
static cue_t cues[NCUES];
static uint32_t ncues;
static volatile uint32_t cue_overflows;

/* the sequencer (P0.4): each channel's pattern, played a sixteenth at a time from the transport */
#define Q16 (BEAT_UNITS / 4u)                    /* a sixteenth in beat units: 6,615,000 (exact) */
static step_t pat[NCH][NSTEPS];
static int32_t last_tick = -1;                   /* the sixteenth last played since the start */
static int32_t seq_bass = -1;                    /* the bass note the sequencer holds */
static volatile uint32_t seq_hits;               /* steps played since boot (tests) */
static volatile uint32_t seq_hit_at[NCH];        /* each channel's last hit, samples since the start (tests) */

/* the runway's audible tick (P_TICK, P0.6): a short sine blip on each of its beats, higher on a bar's first */
static float tick_ph, tick_inc, tick_amp;
static volatile uint32_t ticks;                  /* blips since boot (tests) */
static void runway_tick(uint32_t beat)
{
    uint32_t cyc = cycle_bars(ev[P_CYCLE]), bar = (beat / 4u) % cyc, left = cyc - bar;
    if (!ev[P_TICK] || left > runway_bars(ev[P_RUNWAY]))
        return;
    tick_inc = (beat % 4u == 0u ? 1760.0f : 1320.0f) / 44100.0f;
    tick_ph = 0.0f;
    tick_amp = 0.25f;
    ticks++;
}          /* a cue the full list could not hold fired at once instead */


/* -------------------------------------------------------------- profiler --- */
static uint32_t prof_acc[NPARTS];                /* this block's ticks, per part */
static eng_prof_t prof[NPARTS];
static uint32_t prof_t;                          /* the last mark */
static inline void prof_mark(uint32_t part)      /* the time since the last mark goes to part */
{
    uint32_t t = plat_ticks24();
    prof_acc[part] += t - prof_t;
    prof_t = t;
}
static void prof_block(void)                     /* the block done: averages and maxima */
{
    uint32_t i;
    for (i = 0; i < NPARTS; i++) {
        uint32_t a = prof_acc[i];
        prof[i].avg = prof[i].avg - (prof[i].avg >> 4) + a;   /* (x16: 1/16-tick resolution) */
        if (a > prof[i].max)
            prof[i].max = a;
        prof_acc[i] = 0;
    }
}
eng_prof_t eng_prof(uint32_t part) { return part < NPARTS ? prof[part] : prof[0]; }
void eng_prof_reset(void)
{
    uint32_t i;
    for (i = 0; i < NPARTS; i++)
        prof[i].max = 0;
}
const char *eng_part_name(uint32_t part)
{
    static const char *const N[NPARTS] = {"cmd", "ch1", "ch2", "ch3", "ch4", "ch5", "ch6", "ch7", "ch8", "mix",
                                          "echo", "spring", "out"};
    return part < NPARTS ? N[part] : "";
}

/* ------------------------------------------------------------- transport --- */
static int cue_due(const cue_t *c) { return t_beat > c->fb || (t_beat == c->fb && t_phase >= c->fp); }

static void set_param(uint32_t id, int32_t v)
{
    if (id >= P_NIDS || !param_desc(id))
        return;
    ev[id] = (int16_t)param_clamp(id, v);
}

static void load_template(int32_t t, uint32_t mask)
{
    uint32_t ch;
    for (ch = 0; ch < NCH; ch++)
        if (mask >> ch & 1u)
            template_fill(t, ch, pat[ch]);
}

static void fire_due(int all)                    /* the cues now due (all: every one), in order */
{
    uint32_t i, k = 0;
    for (i = 0; i < ncues; i++) {
        if (all || cue_due(&cues[i])) {
            if (cues[i].op == CMD_TEMPLATE)
                load_template(cues[i].id, (uint16_t)cues[i].v);
            else
                set_param(cues[i].id, cues[i].v);
            t_fire_at = t_samples;
            t_fires++;
        } else {
            cues[k++] = cues[i];
        }
    }
    ncues = k;
}

static void cue_add(uint32_t op, uint32_t id, int32_t v, uint32_t beats)
{
    cue_t *c;
    uint32_t b, lead;
    if (!t_playing || !beats || ncues == NCUES) {   /* no clock: now (a full list: late is better than never) */
        if (t_playing && beats)
            cue_overflows++;
        if (op == CMD_TEMPLATE)
            load_template((int32_t)id, (uint32_t)v);
        else
            set_param(id, v);
        return;
    }
    b = (t_beat / beats + 1u) * beats;           /* the next line */
    lead = (uint32_t)ev[P_LEAD] * 441u / 10u * (uint32_t)ev[P_TEMPO];   /* ms -> samples -> beat units */
    c = &cues[ncues++];
    c->id = (uint16_t)id;
    c->v = (int16_t)v;
    c->op = (uint8_t)op;
    if (op == CMD_TEMPLATE)                      /* a pattern change lands on the line itself, not early */
        lead = 0;
    if (lead) {                                  /* (lead < a beat: 50 ms at 180 BPM is a sixth of one) */
        c->fb = b - 1u;
        c->fp = BEAT_UNITS - lead;
    } else {
        c->fb = b;
        c->fp = 0;
    }
}

static inline void t_step(uint32_t n)            /* the position n samples on */
{
    t_samples += n;
    t_phase += (uint32_t)ev[P_TEMPO] * n;        /* (1800 x 256 at most: no overflow) */
    while (t_phase >= BEAT_UNITS) {
        t_phase -= BEAT_UNITS;
        t_beat++;
    }
}

/* --------------------------------------------------------------- commands --- */
static float fader(int32_t v)                    /* 0..127, 100 = unity */
{
    float x = (float)v * 0.01f;
    return x * x;
}
static float send(int32_t v)                     /* 0..127, 127 = unity */
{
    float x = (float)v * (1.0f / 127.0f);
    return x * x;
}

static void strips_update(void)                  /* the targets from the applied values */
{
    uint32_t ch;
    int any_solo = 0;
    for (ch = 0; ch < NCH; ch++)
        any_solo |= ev[P_CH(ch, CHF_SOLO)] != 0;
    for (ch = 0; ch < NCH; ch++) {
        strip_t *s = &strip[ch];
        /* solo is additive; a soloed channel that is muted stays muted. Solo-muted = muted, sends too */
        int audible = !ev[P_CH(ch, CHF_MUTE)] && (!any_solo || ev[P_CH(ch, CHF_SOLO)]);
        float g = audible ? fader(ev[P_CH(ch, CHF_LEVEL)]) : 0.0f;
        /* a throw (P0.2): the channel to its destination in full, pre-fader and pre-mute, on top of its sends */
        int thr = ev[P_CH(ch, CHF_THROW)] != 0, dest = ev[P_CH(ch, CHF_THROW_DEST)];
        float te = thr && dest != DEST_SPRING ? 1.0f : 0.0f, ts = thr && dest != DEST_ECHO ? 1.0f : 0.0f;
        ramp_to(&s->dry, g);
        ramp_to(&s->echo, g * send(ev[P_CH(ch, CHF_ECHO_SEND)]) + te);
        ramp_to(&s->spring, g * send(ev[P_CH(ch, CHF_SPRING_SEND)]) + ts);
    }
}

static int echo_thrown(void)                     /* a throw into the echo held: the loop lifted */
{
    uint32_t ch;
    for (ch = 0; ch < NCH; ch++)
        if (ev[P_CH(ch, CHF_THROW)] && ev[P_CH(ch, CHF_THROW_DEST)] != DEST_SPRING)
            return 1;
    return 0;
}

/* a throw lifts the loop's gain halfway to unity, capped (DUB-1's "throws add gain up to a cap"), and brings in
 * the 2T head (its "second head only during throws"), so a thrown word rings longer and thicker */
#define THROW_G_CAP 0.95f
static float throw_lift(float g)
{
    float l = g + (1.0f - g) * 0.5f;
    return g >= THROW_G_CAP ? g : l > THROW_G_CAP ? THROW_G_CAP : l;
}

static float echo_line_t(int32_t d)              /* a division's time in line samples at this tempo */
{
    return echo_div_beats(d) * 6615000.0f / (float)ev[P_TEMPO];   /* beats x 60 / BPM x 11025 */
}

static void bus_update(void)                     /* the echo's and the voices' settings from the applied values */
{
    echo_cfg_t c;
    int32_t d = ev[P_ECHO_TIME];
    float t, t60, tone_hz;
    while (d > 0 && echo_line_t(d) > ECHO_T_MAX)  /* too long for the line at this tempo: the next shorter */
        d--;
    echo_div_now = d;
    t = echo_line_t(d) - 1.0f;                   /* (the decimator and interpolator are about a line sample) */
    c.t = t;
    t60 = echo_t60(ev[P_ECHO_FB]);
    c.g = fm_exp2f(-9.966f * (t / ECHO_RATE) / t60);   /* -60 dB after T60 */
    if (echo_thrown())
        c.g = throw_lift(c.g);
    bus_echo_g = c.g;
    c.freeze = ev[P_ECHO_FREEZE] != 0;
    if (c.freeze)
        c.g = 1.0f;
    if (ev[P_ECHO_PUSH])
        c.g = 1.15f;                             /* past unity (the loop's filters lose ~5 % at best), capped */
    tone_hz = 800.0f * fm_exp2f((float)ev[P_ECHO_TONE] * (2.64f / 127.0f));
    c.tone = 1.0f - fm_expf(-6.2831853f * tone_hz / ECHO_RATE);
    c.head2 = send(ev[P_ECHO_HEAD2]);
    if (echo_thrown() && c.head2 < 0.35f)
        c.head2 = 0.35f;
    echo_config(&c);
    ramp_to(&echo_ret, send(ev[P_ECHO_RET]));
    voices_config(ev);
    {                                            /* the spring: its decay as loop feedback (~40 ms a pass), damping */
        spring_cfg_t sc;
        float t60 = 0.5f + 5.5f * (float)ev[P_SPRING_DECAY] * (1.0f / 127.0f);
        float hz = 2000.0f + 5000.0f * (float)ev[P_SPRING_TONE] * (1.0f / 127.0f);
        sc.g = fm_exp2f(-9.966f * 0.04f / t60);
        sc.damp = 1.0f - fm_expf(-6.2831853f * hz / 22050.0f);
        spring_config(&sc);
        ramp_to(&spring_ret, send(ev[P_SPRING_RET]));
        {                                        /* the cross-sends, their loop capped */
            /* echo -> spring -> echo is a loop through two resonant loops: each one's peaks are about 1 / (1 - g)
             * (the spring's 22x at its longest decay), so the sends' product is held under the cap times both
             * (1 - g). The echo's g counts up to 0.9: past that its own saturation and limiter hold it */
            float a = send(ev[P_XS_E2S]), b = send(ev[P_XS_S2E]);
            float ge = fm_minf(bus_echo_g, 0.9f), lim = XS_LOOP_CAP * (1.0f - ge) * (1.0f - sc.g);
            if (a * b > lim)
                b = a > 0.0f ? lim / a : b;
            ramp_to(&xs_e2s, a);
            ramp_to(&xs_s2e, b);
        }
        if (ev[P_SPRING_KICK] && !kick_last) {
            spring_crash(0.7f);
            crashes++;
        }
        kick_last = ev[P_SPRING_KICK];
    }
    lock_now = lock_mask(ev, t_playing ? t_beat / 4u : 0u);
    if (ev[P_HPF_STEP] != bk_step) {             /* a step: at once, unsmoothed (the detents are the sound) */
        bk_step = ev[P_HPF_STEP];
        bigknob_step(&bk, hpf_hz(bk_step));
    }
}

static void drain(void)                          /* every queued command, at the start of the block */
{
    cmd_t c;
    while (cmdq_pop(&eng_q, &c)) {
        if (c.op == CMD_SET)
            set_param(c.id, c.v);
        else if (c.op == CMD_CUE)
            cue_add(CMD_SET, c.id, c.v, c.beats);
        else if (c.op == CMD_STEP && c.id < NCH * NSTEPS)
            pat[c.id / NSTEPS][c.id % NSTEPS] = step_unpack(c.v);
        else if (c.op == CMD_TEMPLATE)
            cue_add(CMD_TEMPLATE, c.id, c.v, c.beats);
        else if (c.op == CMD_NOTE && c.id < NCH) {
            int32_t note = c.v & 0x7F, vel = (c.v >> 8) & 0x7F;
            if (vel)
                voice_trig(c.id, note, (uint32_t)vel);
            else
                voice_release(c.id, note);
        }
        cmds++;
    }
    if (ev[P_PLAY] && !t_playing) {              /* start: bar 1, beat 1, at this block's first sample */
        t_playing = 1;
        t_beat = t_phase = t_samples = 0;
        last_tick = -1;
    } else if (!ev[P_PLAY] && t_playing) {       /* stop: what was cued happens now; the bass let go */
        t_playing = 0;
        fire_due(1);
        if (seq_bass >= 0)
            voice_release(CH_BASS, seq_bass);
        seq_bass = -1;
    }
}

void eng_init(void)
{
    uint32_t id, ch;
    for (id = 0; id < P_NIDS; id++) {
        ev[id] = (int16_t)param_def(id);
    }
    eng_q.r = eng_q.w = 0;
    for (ch = 0; ch < NCH; ch++) {
        strip_t *s = &strip[ch];
        s->dry.g = s->echo.g = s->spring.g = 0.0f;
        s->dry.target = s->echo.target = s->spring.target = 0.0f;
        s->dry.left = s->echo.left = s->spring.left = 0;
        s->tone_ph = 0.0f;
        s->peak = 0.0f;
    }
    strips_update();
    echo_init();
    voices_init();
    bigknob_init(&bk);
    bk_step = -1;
    spring_init();
    xs_e2s.g = xs_e2s.target = xs_s2e.g = xs_s2e.target = 0.0f;
    xs_e2s.left = xs_s2e.left = 0;
    sret_last_n = 0;
    spring_ret.g = spring_ret.target = 0.0f;
    spring_ret.left = 0;
    kick_last = 0;
    echo_ret.g = echo_ret.target = 0.0f;
    echo_ret.left = 0;
    bus_update();
    cmds = 0;
    t_playing = 0;
    t_beat = t_phase = t_samples = 0;
    ncues = 0;
    load_template(TPL_ONE_DROP, 0xFFu);          /* a riddim to start from (editable steps) */
    last_tick = -1;
    seq_bass = -1;
}

/* ------------------------------------------------------------- sequencer --- */
static void seq_play(uint32_t tick)              /* sixteenth `tick` since the start: every channel's step */
{
    uint32_t ch;
    if (seq_bass >= 0) {                         /* the bass: a step long (staccato), unless the next one sounds */
        voice_release(CH_BASS, seq_bass);
        seq_bass = -1;
    }
    if (tick % 4u == 0u)
        runway_tick(tick / 4u);
    for (ch = 0; ch < NCH; ch++) {
        uint32_t len = (uint32_t)ev[P_CH(ch, CHF_LEN)];
        step_t st = pat[ch][tick % (len ? len : 16u)];
        if (!st.lvl)
            continue;
        voice_trig(ch, st.note, lvl_vel(st.lvl));
        seq_hits++;
        seq_hit_at[ch] = t_samples;
        if (ch == CH_BASS)
            seq_bass = st.note;
    }
}

static uint32_t until_16th(void)                 /* samples to the next sixteenth's line */
{
    uint32_t b = (t_phase / Q16 + 1u) * Q16, tempo = (uint32_t)ev[P_TEMPO];
    return (b - t_phase + tempo - 1u) / tempo;
}

static uint32_t until_cue(void)                  /* samples to the first cue's moment (0: due) */
{
    uint32_t i, best = 0xFFFFFFFFu, tempo = (uint32_t)ev[P_TEMPO];
    for (i = 0; i < ncues; i++) {
        const cue_t *c = &cues[i];
        uint32_t d;
        if (cue_due(c))
            return 0;
        if (c->fb == t_beat)
            d = (c->fp - t_phase + tempo - 1u) / tempo;
        else if (c->fb == t_beat + 1u)
            d = (BEAT_UNITS - t_phase + c->fp + tempo - 1u) / tempo;
        else
            continue;                            /* (further than a beat: the sixteenths come first) */
        if (d < best)
            best = d;
    }
    return best;
}

/* ---------------------------------------------------------------- sources --- */
void eng_trig(uint32_t ch, int32_t note, uint32_t vel) { voice_trig(ch, note, vel); }
void eng_release(uint32_t ch, int32_t note) { voice_release(ch, note); }
int32_t eng_note(void) { return voice_note(CH_BASS); }

static int source(uint32_t ch, float *o, uint32_t n)   /* the channel's sound, mono; 0: silent */
{
    uint32_t i;
    int any = 0;
    any = voice_render(ch, o, n);
    if (ev[P_DEV_TONES]) {                       /* a sine per channel: 110 Hz x (channel + 1), -20 dBFS */
        strip_t *s = &strip[ch];
        float inc = 110.0f * (float)(ch + 1u) / 44100.0f;
        for (i = 0; i < n; i++) {
            float t = 0.1f * fm_sin_turns(s->tone_ph);
            s->tone_ph += inc;
            if (s->tone_ph >= 1.0f)
                s->tone_ph -= 1.0f;
            o[i] = any ? o[i] + t : t;
        }
        any = 1;
    }
    return any;
}

/* ----------------------------------------------------------------- render --- */
static void render_seg(int32_t *lr, uint32_t n, float gain)   /* n samples, the parameters as they are now */
{
    uint32_t ch, i;
    float ep = 0.0f, sp = 0.0f;
    if (!n)
        return;
    for (i = 0; i < n; i++)
        dry[i] = ebus[i] = sbus[i] = 0.0f;
    prof_mark(PART_MIX);
    for (ch = 0; ch < NCH; ch++) {               /* a fixed order: the same sum on one core or two */
        strip_t *s = &strip[ch];
        float pk = 0.0f;
        int on = source(ch, buf, n);
        prof_mark(PART_CH0 + ch);
        if (!on) {
            ramp_jump(&s->dry);
            ramp_jump(&s->echo);
            ramp_jump(&s->spring);
            s->peak = 0.0f;
            continue;
        }
        if (s->dry.left | s->echo.left | s->spring.left) {   /* a gain moving: per sample */
            for (i = 0; i < n; i++) {
                float x = buf[i], d = ramp_next(&s->dry) * x;
                dry[i] += d;
                ebus[i] += ramp_next(&s->echo) * x;
                sbus[i] += ramp_next(&s->spring) * x;
                pk = fm_maxf(pk, fm_fabsf(d));
            }
        } else {                                 /* (most blocks) still: constant gains, a send at 0 skipped */
            float gd = s->dry.g, ge = s->echo.g, gs = s->spring.g;
            if (gd != 0.0f)
                for (i = 0; i < n; i++) {
                    float d = gd * buf[i];
                    dry[i] += d;
                    pk = fm_maxf(pk, fm_fabsf(d));
                }
            if (ge != 0.0f)
                for (i = 0; i < n; i++)
                    ebus[i] += ge * buf[i];
            if (gs != 0.0f)
                for (i = 0; i < n; i++)
                    sbus[i] += gs * buf[i];
        }
        s->peak = pk;                            /* (the block's last part: a meter, not a measurement) */
        prof_mark(PART_MIX);
    }
    if (sret_last_n) {                           /* the spring's return into the echo (a block late) */
        uint32_t m = sret_last_n < n ? sret_last_n : n;
        for (i = 0; i < m; i++)
            ebus[i] += ramp_next(&xs_s2e) * sret_last[i];
    }
    echo_run(ebus, eret, n);
    prof_mark(PART_ECHO);
    if (xs_e2s.g != 0.0f || xs_e2s.left)         /* the echo's return into the spring */
        for (i = 0; i < n; i++)
            sbus[i] += ramp_next(&xs_e2s) * eret[i];
    if (ev[P_HPF_ON] == HPF_ON_ECHO)             /* the Big Knob on the echo's return */
        bigknob_run(&bk, eret, n);
    spring_run(sbus, sret, n);
    prof_mark(PART_SPRING);
    for (i = 0; i < n; i++)
        sret_last[i] = sret[i];
    sret_last_n = xs_s2e.g != 0.0f || xs_s2e.left ? n : 0u;
    {
        float rp = 0.0f, op = 0.0f, srp = 0.0f;
        for (i = 0; i < n; i++) {
            float r = ramp_next(&echo_ret) * eret[i], sr = ramp_next(&spring_ret) * sret[i];
            ep = fm_maxf(ep, fm_fabsf(ebus[i]));
            sp = fm_maxf(sp, fm_fabsf(sbus[i]));
            rp = fm_maxf(rp, fm_fabsf(r));
            srp = fm_maxf(srp, fm_fabsf(sr));
            dry[i] += r + sr;                    /* mono dry and the returns (stereo returns: P1.5) */
        }
        spring_ret_peak = srp;
        if (ev[P_HPF_ON] == HPF_ON_MIX)          /* the Big Knob on the whole mix */
            bigknob_run(&bk, dry, n);
        for (i = 0; i < n; i++) {
            float y = dry[i];
            op = fm_maxf(op, fm_fabsf(y));
            gain_now = undenorm(gain_now + (gain - gain_now) * 0.004f);   /* ~5 ms: MASTER does not click */
            if (tick_amp > 1e-4f) {              /* the runway's tick, not sent anywhere */
                y += tick_amp * fm_sin_turns(tick_ph);
                tick_ph = tick_ph + tick_inc >= 1.0f ? tick_ph + tick_inc - 1.0f : tick_ph + tick_inc;
                tick_amp *= 0.9985f;             /* ~15 ms */
            }
            y *= gain_now;
            lr[2 * i] = (int32_t)y;
            lr[2 * i + 1] = (int32_t)y;
        }
        echo_ret_peak = rp;
        out_peak = op;
    }
    echo_peak = ep;
    spring_peak = sp;
    prof_mark(PART_OUT);
}

void eng_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    uint32_t pos = 0;
    float gain = (float)master_q12 * (8300000.0f / 4096.0f);
    if (n > ENG_BLOCK)
        n = ENG_BLOCK;
    prof_t = plat_ticks24();
    drain();
    strips_update();
    bus_update();
    prof_mark(PART_CMD);
    /* from event to event: a sixteenth's line (the sequencer) or a cue (launch quantise), each on its sample */
    for (;;) {
        uint32_t d = n - pos;
        if (t_playing) {
            uint32_t tick = t_beat * 4u + t_phase / Q16, u;
            if ((int32_t)tick != last_tick) {
                last_tick = (int32_t)tick;
                seq_play(tick);
            }
            if (ncues && until_cue() == 0) {
                fire_due(0);
                strips_update();
                bus_update();
            }
            u = until_16th();
            if (u < d)
                d = u;
            if (ncues) {
                u = until_cue();
                if (u < d)
                    d = u;
            }
        }
        prof_mark(PART_CMD);
        if (pos >= n)
            break;
        render_seg(lr + 2u * pos, d, gain);
        if (t_playing)
            t_step(d);
        pos += d;
    }
    prof_block();
}

void eng_panic(void) { voices_panic(); }

int32_t eng_param(uint32_t id) { return id < P_NIDS ? ev[id] : 0; }
float eng_ch_peak(uint32_t ch) { return ch < NCH ? strip[ch].peak : 0.0f; }
float eng_echo_peak(void) { return echo_peak; }
float eng_spring_peak(void) { return spring_peak; }
uint32_t eng_cmds(void) { return cmds; }
uint32_t eng_seq_hits(void) { return seq_hits; }
uint32_t eng_ticks(void) { return ticks; }
float eng_spring_ret_peak(void) { return spring_ret_peak; }
uint32_t eng_crashes(void) { return crashes; }
float eng_out_peak(void) { return out_peak; }
int eng_midi_allowed(int32_t note)               /* MIDI in, when the lock covers it (P_LOCK_MIDI) */
{
    return !ev[P_LOCK_MIDI] || (lock_now >> (note % 12) & 1u);
}
uint32_t eng_frac16(void) { return (t_phase % Q16) / (Q16 / 256u); }
int32_t eng_tick(void) { return t_playing ? last_tick : -1; }
uint32_t eng_seq_hit_at(uint32_t ch) { return ch < NCH ? seq_hit_at[ch] : 0; }
step_t eng_step(uint32_t ch, uint32_t i) { return pat[ch % NCH][i % NSTEPS]; }
float eng_echo_ret_peak(void) { return echo_ret_peak; }
float eng_echo_gr(void) { return echo_gr(); }
int32_t eng_echo_div(void) { return echo_div_now; }
float eng_echo_t(void) { return echo_t_now(); }
float eng_echo_g(void)
{
    float t = echo_line_t(echo_div_now) - 1.0f;
    if (ev[P_ECHO_PUSH])
        return 1.15f;
    if (ev[P_ECHO_FREEZE])
        return 1.0f;
    {
        float g = fm_exp2f(-9.966f * (t / ECHO_RATE) / echo_t60(ev[P_ECHO_FB]));
        return echo_thrown() ? throw_lift(g) : g;
    }
}
int eng_playing(void) { return t_playing; }
uint32_t eng_beat(void) { return t_beat; }
uint32_t eng_fire_at(void) { return t_fire_at; }
uint32_t eng_fires(void) { return t_fires; }
uint32_t eng_cues(void) { return ncues; }
uint32_t eng_cue_overflows(void) { return cue_overflows; }
