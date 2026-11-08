/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* engine.h. Compiled on its own at -O2 (fm1.toml o2). Floats only: single-precision FPU (no double). */
#include "engine.h"
#include "fastmath.h"
#include "plat.h"                                /* plat_ticks24: the profiler */
#include "echo.h"
#include "voices.h"
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
static float buf[ENG_BLOCK], dry[ENG_BLOCK], ebus[ENG_BLOCK], sbus[ENG_BLOCK], eret[ENG_BLOCK];
static volatile float echo_peak, spring_peak, echo_ret_peak;
static ramp_t echo_ret;                          /* the echo return's level */
static volatile int32_t echo_div_now;            /* the division in use (the longest that fits, at this tempo) */
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
} cue_t;
static cue_t cues[NCUES];
static uint32_t ncues;
static volatile uint32_t cue_overflows;          /* a cue the full list could not hold fired at once instead */


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

static void fire_due(int all)                    /* the cues now due (all: every one), in order */
{
    uint32_t i, k = 0;
    for (i = 0; i < ncues; i++) {
        if (all || cue_due(&cues[i])) {
            set_param(cues[i].id, cues[i].v);
            t_fire_at = t_samples;
            t_fires++;
        } else {
            cues[k++] = cues[i];
        }
    }
    ncues = k;
}

static void cue_add(uint32_t id, int32_t v, uint32_t beats)
{
    cue_t *c;
    uint32_t b, lead;
    if (!t_playing || !beats) {                  /* no clock: now */
        set_param(id, v);
        return;
    }
    if (ncues == NCUES) {                        /* never lost: late is better than never (counted) */
        cue_overflows++;
        set_param(id, v);
        return;
    }
    b = (t_beat / beats + 1u) * beats;           /* the next line */
    lead = (uint32_t)ev[P_LEAD] * 441u / 10u * (uint32_t)ev[P_TEMPO];   /* ms -> samples -> beat units */
    c = &cues[ncues++];
    c->id = (uint16_t)id;
    c->v = (int16_t)v;
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
}

static void drain(void)                          /* every queued command, at the start of the block */
{
    cmd_t c;
    while (cmdq_pop(&eng_q, &c)) {
        if (c.op == CMD_SET)
            set_param(c.id, c.v);
        else if (c.op == CMD_CUE)
            cue_add(c.id, c.v, c.beats);
        cmds++;
    }
    if (ev[P_PLAY] && !t_playing) {              /* start: bar 1, beat 1, at this block's first sample */
        t_playing = 1;
        t_beat = t_phase = t_samples = 0;
    } else if (!ev[P_PLAY] && t_playing) {       /* stop: what was cued happens now */
        t_playing = 0;
        fire_due(1);
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
    echo_ret.g = echo_ret.target = 0.0f;
    echo_ret.left = 0;
    bus_update();
    cmds = 0;
    t_playing = 0;
    t_beat = t_phase = t_samples = 0;
    ncues = 0;
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
    echo_run(ebus, eret, n);
    prof_mark(PART_ECHO);
    {
        float rp = 0.0f;
        for (i = 0; i < n; i++) {
            float y, r = ramp_next(&echo_ret) * eret[i];
            ep = fm_maxf(ep, fm_fabsf(ebus[i]));
            sp = fm_maxf(sp, fm_fabsf(sbus[i]));
            rp = fm_maxf(rp, fm_fabsf(r));
            gain_now = undenorm(gain_now + (gain - gain_now) * 0.004f);   /* ~5 ms: MASTER does not click */
            y = (dry[i] + r) * gain_now;         /* mono dry and the echo's return (stereo returns: P1.5) */
            lr[2 * i] = (int32_t)y;
            lr[2 * i + 1] = (int32_t)y;
        }
        echo_ret_peak = rp;
    }
    echo_peak = ep;
    spring_peak = sp;
    prof_mark(PART_OUT);
}

void eng_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    uint32_t i, seg = 0;
    float gain = (float)master_q12 * (8300000.0f / 4096.0f);
    if (n > ENG_BLOCK)
        n = ENG_BLOCK;
    prof_t = plat_ticks24();
    drain();
    strips_update();
    bus_update();
    prof_mark(PART_CMD);
    if (t_playing && ncues) {                    /* a cue lands on its sample: the block is split there */
        for (i = 0; i < n; i++) {
            uint32_t k;
            int due = 0;
            for (k = 0; k < ncues && !due; k++)
                due = cue_due(&cues[k]);
            if (due) {
                render_seg(lr + 2u * seg, i - seg, gain);
                fire_due(0);
                strips_update();
                bus_update();
                prof_mark(PART_CMD);
                seg = i;
            }
            t_step(1);
        }
    } else if (t_playing) {
        t_step(n);
    }
    prof_mark(PART_CMD);
    render_seg(lr + 2u * seg, n - seg, gain);
    prof_block();
}

void eng_panic(void) { voices_panic(); }

int32_t eng_param(uint32_t id) { return id < P_NIDS ? ev[id] : 0; }
float eng_ch_peak(uint32_t ch) { return ch < NCH ? strip[ch].peak : 0.0f; }
float eng_echo_peak(void) { return echo_peak; }
float eng_spring_peak(void) { return spring_peak; }
uint32_t eng_cmds(void) { return cmds; }
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
