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
#include "chordv.h"
#include "scenes.h"
#include "motion.h"
#include "song.h"
#include "fxv.h"
#include "phaser.h"
#include "tape.h"
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
static float dryr[ENG_BLOCK], eretr[ENG_BLOCK], sretr[ENG_BLOCK];   /* the right side (returns are stereo: P1.5) */
static volatile float echo_peak, spring_peak, echo_ret_peak, spring_ret_peak;
static ramp_t echo_ret, spring_ret;              /* the returns' levels */
static int16_t kick_last;
static float bus_echo_g;
static int dive_back;
static int ph_on, ph_mode;
static int drive_on;
/* the snare-keyed spring gate (P2.5; C13): a snare opens it for HOLD, then it closes over RELEASE */
static float sg_env;                             /* 0 closed .. 1 open */
static uint32_t sg_hold, sg_hold_n;              /* samples left open; HOLD in samples */
static float sg_rel = 0.999f;                    /* the closing's factor a sample */
static void sgate_key(void) { sg_hold = sg_hold_n; }
static void evlog_ev(uint32_t kind, uint32_t id, int32_t v);   /* (tests: the event log, host only) */
static PLAT_POOL float org_side[ENG_BLOCK];                /* the B-3's horn side, at the organ strip's level (5.15) */
static uint32_t org_side_n;
static volatile uint32_t trig_n[NCH];            /* each channel's hits so far (the envelope view's playhead, 5.10) */
static void trig(uint32_t ch, int32_t note, uint32_t vel)   /* a voice struck: and a snare keys the gate */
{
    evlog_ev(1, ch, note | (int32_t)vel << 8);
    if (ch < NCH)
        trig_n[ch]++;
    voice_trig(ch, note, vel);
    if (ch == CH_SNARE)
        sgate_key();
}                             /* where the tape drive is                                /* where the phaser is this block (PH_OFF when the tier says)                            /* the dive let go: gliding back at its own pace                         /* the echo's loop gain this block (the cross-sends' cap) */
static float sret_last[ENG_BLOCK];               /* the spring's return a block ago: into the echo (cross-send) */
static uint32_t sret_last_n;
static ramp_t xs_e2s, xs_s2e;                    /* the cross-sends' levels (P1.3) */
#define XS_LOOP_CAP 0.5f                         /* echo -> spring -> echo: their product at most this */                        /* P_SPRING_KICK as it was: a strike on 0 -> 1 */
static volatile uint32_t crashes;
static volatile int32_t echo_div_now;
static volatile uint32_t lock_now = 0xFFFu;
static bigknob_t bk, bkr;                        /* the Big Knob (P1.1), left and right */
static int32_t bk_step = -1, bk_lp;              /* its step set; 7.5: a low-pass */
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
#define NCUES 96u                                /* (hand moves wait for their motion tick too: 5.1) */
typedef struct {
    uint32_t fb, fp;
    uint16_t id;
    int32_t v;
    uint8_t op;                                  /* CMD_SET (a parameter), CMD_TEMPLATE or CMD_SCENE */
} cue_t;
static cue_t cues[NCUES];
static uint32_t ncues;
static volatile uint32_t cue_overflows;

/* the sequencer (P0.4; the session, 4.8): each channel's four patterns, the one playing (CHF_PAT) from where it was
 * launched. Time is counted in fine ticks, 240 a beat (riddim.h PPQ): each step lands at its grid line, plus its
 * swing and its own offset (played or nudged off the line), each on its own sample. At each sixteenth's line the
 * steps that land within that sixteenth are gathered (a step played early, before its line, is gathered with the
 * sixteenth before), and the render is split where each falls, as for a cue */
#define Q16 (BEAT_UNITS / 4u)                    /* a sixteenth in beat units: 6,615,000 (exact) */
#define FU (BEAT_UNITS / PPQ)                    /* a fine tick in beat units: 110,250 (exact) */
typedef char fu_exact[BEAT_UNITS % PPQ == 0u ? 1 : -1];
#define F16 (PPQ / 4u)                           /* a sixteenth in fine ticks: 60 */
static PLAT_POOL step_t pat[NCH][NPAT][NSTEPS];  /* (32 KB: the pool) */
static pmeta_t pmeta[NCH][NPAT];
typedef struct {
    int32_t org;                                 /* the fine tick its step 0 fell on (a launch's line) */
    uint8_t p;                                   /* the pattern */
} ppos_t;
static ppos_t pcur[NCH], pnext[NCH];             /* playing; launched for a line to come (pnext_on) */
static uint8_t pnext_on[NCH];
#define NSEV 48u
typedef struct {
    int32_t t;                                   /* its fine tick */
    uint8_t ch, p, i;
    uint8_t sub;                                 /* 0 the step; 1..3 a repeat (5.8); 0x80 a flam's ghost */
} sev_t;
static sev_t sev[NSEV];                          /* this sixteenth's steps, in time order */
static uint32_t nsev;
/* 7.4: DESK MCI, after Airwindows' ConsoleMC (Chris Johnson, MIT): each strip after its fader (MCI's fader came
 * before its drive: "overdrives easily") through sin(), clamped to +-pi/2, a little hot and its level put back: a loud
 * channel thickens, a quiet one passes as it is. Console's asin decode of the sum, and its Sinew, were built first and
 * measured: -72 dB from CLEAN on the demo at the headroom they need (inaudible) for +9.5 % of a block, so left out.
 * The returns pass clean. Studio only */
#define DESK_DRIVE 1.6f
static int desk_mci;
static inline float desk_enc(float x)
{
    return fm_sin_pi(fm_clampf(DESK_DRIVE * x, -1.5707963f, 1.5707963f)) * (1.0f / DESK_DRIVE);
}
static int32_t held_note[NCH], off_at[NCH];      /* the note the sequencer holds (bass, lead, the B-3), its end */
static int32_t last_tick = -1;                   /* the sixteenth last gathered since the start */
static void seq_start(void);
static volatile uint32_t seq_hits;               /* steps played since boot (tests) */
static volatile uint32_t seq_hit_at[NCH];        /* each channel's last hit, samples since the start (tests) */
static volatile int32_t chord_last[NCH];         /* the chord a chord channel last played (tests) */
static int32_t scene_fired = -1;                 /* the scene recalled at this line (the song's capture) */
static volatile uint32_t song_bar_now;           /* the song's bar playing

/* the runway's audible tick (P_TICK, P0.6): a short sine blip on each of its beats, higher on a bar's first */
static float tick_ph, tick_inc, tick_amp;
static volatile uint32_t ticks;                  /* blips since boot (tests) */
static void runway_tick(uint32_t beat)
{
    uint32_t cyc = cycle_bars(ev[P_CYCLE]), bar = (beat / 4u) % cyc, left = cyc - bar;
    if (!ev[P_CLICK] && (!ev[P_TICK] || left > runway_bars(ev[P_RUNWAY])))
        return;                                  /* (the click: every beat, a count-in or a recording's) */
    tick_inc = (beat % 4u == 0u ? 1760.0f : 1320.0f) / 44100.0f;
    tick_ph = 0.0f;
    tick_amp = 0.25f;
    ticks++;
}          /* a cue the full list could not hold fired at once instead */


/* -------------------------------------------------------------- profiler --- */
static uint32_t prof_acc[NPARTS];                /* this block's ticks, per part */
static eng_prof_t prof[NPARTS];
/* the guard (P1.9): the load against the device's block, the tier that follows from it (or the one locked) */
static volatile int tier_now = TIER_STUDIO;
static volatile uint32_t load_pct, tier_changes;
static uint32_t over_blocks, under_blocks;
#define TIER_DOWN_PCT 75u                        /* over this for 8 blocks (46 ms): a tier down */
#define TIER_UP_PCT 45u                          /* under this for 2 s: a tier up */
#define UNDER_BLOCKS (2u * 44100u / 256u)
static uint32_t prof_t;                          /* the last mark */
static inline void prof_mark(uint32_t part)      /* the time since the last mark goes to part */
{
    uint32_t t = plat_ticks24();
    prof_acc[part] += t - prof_t;
    prof_t = t;
}
static void tier_track(uint32_t total16);
static void prof_block(void)                     /* the block done: averages and maxima */
{
    uint32_t i, total = 0;
    for (i = 0; i < NPARTS; i++) {
        uint32_t a = prof_acc[i];
        prof[i].avg = prof[i].avg - (prof[i].avg >> 4) + a;   /* (x16: 1/16-tick resolution) */
        if (a > prof[i].max)
            prof[i].max = a;
        prof_acc[i] = 0;
        total += prof[i].avg;
    }
    tier_track(total);
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

/* ----------------------------------------------------------------- guard --- */
static void tier_track(uint32_t total16)         /* the load (avg ticks x 16) -> the tier, with some patience */
{
    uint32_t pct = total16 / 16u * 100u / ENG_BLOCK_TICKS + (uint32_t)ev[P_DEV_LOAD];   /* (32 bits: no 64-bit multiply) */
    int t = tier_now;
    load_pct = pct;
    if (ev[P_TIER_LOCK]) {                       /* locked: a sound, chosen */
        t = ev[P_TIER_LOCK] - 1;
        over_blocks = under_blocks = 0;
    } else if (pct > TIER_DOWN_PCT) {
        under_blocks = 0;
        if (++over_blocks >= 8u && t < TIER_SEVEN) {
            t++;
            over_blocks = 0;
        }
    } else if (pct < TIER_UP_PCT) {
        over_blocks = 0;
        if (++under_blocks >= UNDER_BLOCKS && t > TIER_STUDIO) {
            t--;
            under_blocks = 0;
        }
    } else
        over_blocks = under_blocks = 0;
    if (t != tier_now) {
        tier_now = t;
        tier_changes++;
    }
}

/* ------------------------------------------------------------- transport --- */
static int cue_due(const cue_t *c) { return t_beat > c->fb || (t_beat == c->fb && t_phase >= c->fp); }

/* tails off (roadmap 5.2, spec 4.1.3: PLAY tapped twice): the echo's and the spring's returns fade to nothing over
 * 20 ms (a gain on both, no click), then the echo's tape, the spring and the phaser are emptied (tails_clear). The
 * dry voices ring out as after any stop */
#define TAILS_N 882u                             /* 20 ms */
static uint32_t tails_left;                      /* samples of the fade left, 0: none */
static int tails_done;                           /* faded out: emptied at the block's end */
static void set_param(uint32_t id, int32_t v)
{
    if (id >= P_NIDS || !param_desc(id))
        return;
    v = param_clamp(id, v);
    if (id == P_TAILS) {                         /* tails off (5.2): the returns fade, then the effects are emptied */
        if (tails_left == 0u)
            tails_left = TAILS_N;
        return;
    }
    if (id == P_SHOT) {                          /* a one-shot: every set fires it, the same value again too (a
                                                  * replay's first, when the pass left it there: 5.1) */
        evlog_ev(0, id, v);
        fxv_shot_dir((uint32_t)v & 7u, (uint32_t)(v >> 4) & 127u, (v & 8) != 0);
        ev[id] = (int16_t)v;
        return;
    }
    if (ev[id] == v)
        return;
    evlog_ev(0, id, v);
    if (id == P_SIREN) {                         /* the siren (5.1: a parameter, so motion captures it) */
        if (ev[id])
            fxv_off((ev[id] & 15) - 1);
        if (v) {
            fxv_on((v & 15) - 1, (uint32_t)(v >> 4) & 127u);
            trig_n[CH_VOX]++;
        }
    }
    ev[id] = (int16_t)v;
}

/* (tests) the event log: every parameter that changes and every note, at its sample since the start; the PC
 * simulator's only (roadmap 5.1, replay.txt). On the device, nothing */
#ifdef PLAT_HOST
static void (*evlog_f)(uint32_t t, uint32_t kind, uint32_t id, int32_t v);
static int16_t evsnap[P_NIDS];
void eng_evlog(void (*f)(uint32_t t, uint32_t kind, uint32_t id, int32_t v)) { evlog_f = f; }
static void evlog_ev(uint32_t kind, uint32_t id, int32_t v)
{
    if (evlog_f && t_playing)
        evlog_f(t_samples, kind, id, v);
}
static void evlog_snap(void)                     /* before a scene or a song bar is applied straight into ev */
{
    uint32_t i;
    if (evlog_f)
        for (i = 0; i < P_NIDS; i++)
            evsnap[i] = ev[i];
}
static void evlog_diff(void)                     /* .. and what it changed */
{
    uint32_t i;
    if (evlog_f)
        for (i = 0; i < P_NIDS; i++)
            if (ev[i] != evsnap[i])
                evlog_ev(0, i, ev[i]);
}
#else
static inline void evlog_ev(uint32_t kind, uint32_t id, int32_t v) { (void)kind, (void)id, (void)v; }
static inline void evlog_snap(void) {}
static inline void evlog_diff(void) {}
#endif
static void vrel(uint32_t ch, int32_t note)
{
    evlog_ev(2, ch, note);
    if (ch == CH_ORGAN)
        chordv_release(ch);                      /* (the B-3's held chord: 5.15) */
    else
        voice_release(ch, note);
}
static void ctrig(uint32_t ch, int32_t chord, int32_t voicing, uint32_t vel)
{
    evlog_ev(3, ch, chord | voicing << 8 | (int32_t)vel << 16);
    if (ch < NCH)
        trig_n[ch]++;
    chordv_trig(ch, chord, voicing, vel);
}

/* motion (P2.7): the tick in the bar (480 a beat); a move by hand recorded (while capturing) */
#define MOT_UNITS (BEAT_UNITS / MOT_TPB)        /* 55,125 (exact) */
static uint32_t mot_hold;                        /* momentaries motion holds down (let go at the stop) */
static uint32_t bar_tick(void) { return (t_beat & 3u) * MOT_TPB + t_phase / MOT_UNITS; }

static void set_by_hand(uint32_t id, int32_t v)
{
    int k;
    set_param(id, v);
    if (!t_playing || id >= P_NIDS)
        return;
    k = mot_momentary(id);
    if (k >= 0)
        mot_hold &= ~(1u << (uint32_t)k);
    mot_user(id, ev[id], bar_tick(), ev[P_SONG_REC] != 0);
}

static int spin_on, spin_last;                   /* the spin-back (P3.5): 1 spinning, 2 the gap after */
static void spin_start(void);
static void transport_stop(void);
static void clk_push(uint32_t pos, uint32_t st);
static int32_t clk_last = -1;

static void mot_release(void)                    /* the stop: what motion held down, let go */
{
    uint32_t k;
    for (k = 0; k < MOT_NMOM; k++)
        if (mot_hold >> k & 1u) {
            uint32_t id = mot_mom_id(k);
            set_param(id, 0);                    /* (through set_param: a siren let go) */
            mot_fb_push(id, 0);
        }
    mot_hold = 0;
    mot_line(-1, 0, ev);
}

static int mot_play(void)                        /* the moves due now (1: any) */
{
    mot_ev_t e;
    int any = 0;
    while (mot_due(bar_tick(), &e)) {
        int k = mot_momentary(e.id);
        set_param(e.id, e.v);
        if (k >= 0)
            mot_hold = ev[e.id] ? mot_hold | 1u << (uint32_t)k : mot_hold & ~(1u << (uint32_t)k);
        mot_fb_push(e.id, ev[e.id]);
        any = 1;
    }
    return any;
}

static uint32_t until_motion(void)               /* samples to the next move (0xFFFFFFFF: none in this bar) */
{
    int32_t t = mot_next_tick();
    uint32_t at, now, tempo = (uint32_t)ev[P_TEMPO];
    if (t < 0)
        return 0xFFFFFFFFu;
    at = (uint32_t)t * MOT_UNITS;
    now = (t_beat & 3u) * BEAT_UNITS + t_phase;
    return at > now ? (at - now + tempo - 1u) / tempo : 1u;
}

static void load_template(int32_t t, uint32_t v)  /* v: the channels (a mask) | their patterns << 8, 2 bits each */
{
    uint32_t ch;
    for (ch = 0; ch < NCH; ch++)
        if (v >> ch & 1u) {
            uint32_t p = (v >> (8u + 2u * ch)) & 3u;
            template_fill(t, ch, pat[ch][p]);
            pmeta[ch][p].grid = GRID_16;         /* (a riddim is written in sixteenths) */
        }
}

static void pnext_set(uint32_t ch, uint32_t p, int32_t line)   /* a pattern launched for that line */
{
    if (p == pcur[ch].p && !pnext_on[ch])
        return;
    pnext[ch].p = (uint8_t)p;
    pnext[ch].org = line;
    pnext_on[ch] = 1;
}

static void fire_due(int all)                    /* the cues now due (all: every one), in order */
{
    uint32_t i, k = 0;
    for (i = 0; i < ncues; i++) {
        if (all || cue_due(&cues[i])) {
            if (cues[i].op == CMD_TEMPLATE)
                load_template(cues[i].id, (uint16_t)cues[i].v);
            else if (cues[i].op == CMD_SCENE) {
                evlog_snap();
                scene_apply(cues[i].id, ev);
                evlog_diff();
                scene_fired = cues[i].id;
            } else
                set_by_hand(cues[i].id, cues[i].v);
            t_fire_at = t_samples;
            t_fires++;
        } else {
            cues[k++] = cues[i];
        }
    }
    ncues = k;
}

/* a hand's move, while playing (roadmap 5.1): applied on the next motion tick (MOT_UNITS: 73.5 samples at 75 BPM, so
 * at most 1.7 ms later than at the block's start), where the song's motion stamps it and a replay lands it: the pass
 * and its replay are then the same, sample for sample. (A full cue list: now, as before) */
static void cue_tick(uint32_t id, int32_t v)
{
    uint32_t fp = (t_phase + MOT_UNITS - 1u) / MOT_UNITS * MOT_UNITS, fb = t_beat;
    cue_t *c;
    if (ncues == NCUES) {
        cue_overflows++;
        set_by_hand(id, v);
        return;
    }
    if (fp >= BEAT_UNITS) {
        fp = 0;
        fb++;
    }
    c = &cues[ncues++];
    c->fb = fb;
    c->fp = fp;
    c->id = (uint16_t)id;
    c->v = v;
    c->op = CMD_SET;
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
        else if (op == CMD_SCENE) {
            evlog_snap();
            scene_apply(id, ev);
            evlog_diff();
            scene_fired = (int32_t)id;
        } else
            set_param(id, v);
        return;
    }
    b = (t_beat / beats + 1u) * beats;           /* the next line */
    lead = (uint32_t)ev[P_LEAD] * 441u / 10u * (uint32_t)ev[P_TEMPO];   /* ms -> samples -> beat units */
    c = &cues[ncues++];
    c->id = (uint16_t)id;
    c->v = v;
    c->op = (uint8_t)op;
    if (op == CMD_TEMPLATE || op == CMD_SCENE || (op == CMD_SET && id >= P_CH_BASE && id < P_CH_BASE + NCH * CH_STRIDE
                                                  && (id - P_CH_BASE) % CH_STRIDE == CHF_PAT))
        lead = 0;                                /* a pattern change lands on the line itself, not early */
    if (op == CMD_SET && lead == 0u && id >= P_CH_BASE && id < P_CH_BASE + NCH * CH_STRIDE)   /* (a launch: known ahead,
                                                                                               * for the steps played
                                                                                               * early onto its line) */
        pnext_set((id - P_CH_BASE) / CH_STRIDE, (uint32_t)v & 3u, (int32_t)(b * PPQ));
    if (op == CMD_SCENE && scenes[id % NSCENES].used && scenes[id % NSCENES].scope == SCOPE_PATTERNS) {
        uint32_t ch;
        for (ch = 0; ch < NCH; ch++)
            pnext_set(ch, scenes[id % NSCENES].pat[ch] & 3u, (int32_t)(b * PPQ));
    }
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
        int dnb_out = ev[P_DNB] && ch >= CH_SKANK;   /* drum and bass: the rest out (sends too) */
        int audible = !ev[P_CH(ch, CHF_MUTE)] && (!any_solo || ev[P_CH(ch, CHF_SOLO)]) && !dnb_out;
        float g = audible ? fader(ev[P_CH(ch, CHF_LEVEL)]) : 0.0f;
        /* a throw (P0.2): the channel to its destination in full, pre-fader and pre-mute, on top of its sends */
        int thr = ev[P_CH(ch, CHF_THROW)] != 0, dest = ev[P_CH(ch, CHF_THROW_DEST)];
        float a = (float)ev[P_CH(ch, CHF_THROW_AMT)] * (1.0f / 127.0f);   /* (P2.9: as hard as it was hit) */
        float te = thr && dest != DEST_SPRING ? a : 0.0f, ts = thr && dest != DEST_ECHO ? a : 0.0f;
        ramp_to(&s->dry, ev[P_WET] ? 0.0f : g); /* drop to wet: the dry out, its sends kept */
        ramp_to(&s->echo, g * send(ev[P_CH(ch, CHF_ECHO_SEND)]) + te);
        ramp_to(&s->spring, g * send(ev[P_CH(ch, CHF_SPRING_SEND)]) + ts);
    }
}

static float thr_env;                            /* 6.4: 1 while a throw into the echo is held, then easing out (DUB-1) */

/* 6.2 BASS+ (after Felucca's spk_bass and master low-cut, Leo Kuroshita / Hugelton Instruments, GPL-3.0-only, as
 * daurade carries them): what the FM-1's speaker can't play (it gives almost nothing under ~50 Hz:
 * docs/hardware.md), heard through its harmonics. The mix's lows under ~150 Hz (four one-poles) clipped at their own
 * envelope (odd harmonics), band-passed ~220 Hz .. ~900 Hz and added, x1.5, to both sides; and the sides themselves
 * cut 12 dB/oct under ~220 Hz (the speaker's cone flapping at nothing). A setting: LINE (off) for headphones, the
 * line out and USB */
static float sb_lp[4], sb_env, sb_h1, sb_h2, sb_hl, lc_l[2], lc_r[2];
static void speaker_run(float *l, float *r, uint32_t n)
{
    uint32_t i, k;
    for (i = 0; i < n; i++) {
        float m = 0.5f * (l[i] + r[i]), a, t, u;
        sb_lp[0] += (m - sb_lp[0]) * 0.02112f;   /* (692 / 32768: ~148 Hz) */
        for (k = 1; k < 4u; k++)
            sb_lp[k] += (sb_lp[k - 1u] - sb_lp[k]) * 0.02112f;
        a = fm_fabsf(sb_lp[3]);
        sb_env = a > sb_env ? sb_env + 0.25f * (a - sb_env) : undenorm(sb_env * (1.0f - 1.0f / 2048.0f));
        t = fm_maxf(-sb_env, fm_minf(sb_env, 8.0f * sb_lp[3]));
        sb_h1 += (t - sb_h1) * (1.0f / 32.0f);
        u = t - sb_h1;
        sb_h2 += (u - sb_h2) * (1.0f / 32.0f);
        u -= sb_h2;
        sb_hl += (u - sb_hl) * 0.125f;
        lc_l[0] += (l[i] - lc_l[0]) * (1.0f / 32.0f);
        l[i] -= lc_l[0];
        lc_l[1] += (l[i] - lc_l[1]) * (1.0f / 32.0f);
        l[i] = l[i] - lc_l[1] + 1.5f * sb_hl;
        lc_r[0] += (r[i] - lc_r[0]) * (1.0f / 32.0f);
        r[i] -= lc_r[0];
        lc_r[1] += (r[i] - lc_r[1]) * (1.0f / 32.0f);
        r[i] = r[i] - lc_r[1] + 1.5f * sb_hl;
    }
    for (k = 0; k < 4u; k++)
        sb_lp[k] = undenorm(sb_lp[k]);
    sb_h1 = undenorm(sb_h1), sb_h2 = undenorm(sb_h2), sb_hl = undenorm(sb_hl);
    lc_l[0] = undenorm(lc_l[0]), lc_l[1] = undenorm(lc_l[1]), lc_r[0] = undenorm(lc_r[0]), lc_r[1] = undenorm(lc_r[1]);
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
    c.slew = 0.0005f;
    if (ev[P_ECHO_DIVE] || dive_back) {          /* the tape dive: slower to DEPTH x T, then back as slowly */
        float secs = 0.1f + 2.9f * (float)ev[P_DIVE_TIME] * (1.0f / 127.0f);
        c.slew = 1.0f - fm_expf(-1.0f / (secs * 0.33f * ECHO_RATE));   /* ~95 % of the way in secs */
        if (ev[P_ECHO_DIVE])
            c.t = fm_minf(t * (1.0f + (float)ev[P_DIVE_DEPTH] * (1.0f / 127.0f)), ECHO_T_MAX);
        dive_back = ev[P_ECHO_DIVE] || fm_fabsf(echo_t_now() - t) > 2.0f;
    }
    t60 = echo_t60(ev[P_ECHO_FB]);
    c.g = fm_exp2f(-9.966f * (t / ECHO_RATE) / t60);   /* -60 dB after T60 */
    if (echo_thrown())
        thr_env = 1.0f;
    c.g += (throw_lift(c.g) - c.g) * thr_env;    /* (6.4: the lift eases out after the throw, ~0.5 s, as DUB-1's
                                                  * st_throw: the thrown word's repeats don't change mid-ring) */
    bus_echo_g = c.g;
    c.freeze = ev[P_ECHO_FREEZE] != 0;
    if (c.freeze)
        c.g = 1.0f;
    if (ev[P_ECHO_PUSH])
        c.g = 1.15f;                             /* past unity (the loop's filters lose ~5 % at best), capped */
    tone_hz = 800.0f * fm_exp2f((float)ev[P_ECHO_TONE] * (2.64f / 127.0f));
    c.tone = 1.0f - fm_expf(-6.2831853f * tone_hz / ECHO_RATE);
    c.head2 = send(ev[P_ECHO_HEAD2]);
    c.width = ev[P_ECHO_WIDTH] && tier_now == TIER_STUDIO   /* (mono returns from the Four-track tier down) */
              ? (8.0f + 8.0f * (float)(ev[P_ECHO_WIDTH] - 1) * (1.0f / 126.0f)) * (ECHO_RATE / 1000.0f)
              : 0.0f;
    c.pingpong = ev[P_ECHO_PP] && tier_now == TIER_STUDIO;   /* (stereo: Studio only, as the width) */
    c.diffuse = tier_now < TIER_SEVEN;
    c.drive = tier_now >= TIER_SEVEN ? 1.8f : 1.0f;   /* Seven-inch: heavier tape saturation (the sound of it) */
    if (c.head2 < 0.35f * thr_env)               /* (the 2T head with it) */
        c.head2 = 0.35f * thr_env;
    c.wow = (float)ev[P_ECHO_WOW] * (1.0f / 127.0f);   /* 6.3 */
    c.head3 = send(ev[P_ECHO_HEAD3]);           /* 7.1 */
    c.bbd = ev[P_ECHO_TYPE] == ECHO_BBD;         /* 7.9 */
    desk_mci = ev[P_DESK] == DESK_MCI && tier_now == TIER_STUDIO;   /* 7.4 (Studio only) */
    echo_config(&c);
    ramp_to(&echo_ret, send(ev[P_ECHO_RET]));
    voices_config(ev, tier_now);
    chordv_config(ev, tier_now);
    fxv_config(ev);
    {                                            /* the spring: its decay as loop feedback (~40 ms a pass), damping */
        spring_cfg_t sc;
        float t60 = 0.5f + 5.5f * (float)ev[P_SPRING_DECAY] * (1.0f / 127.0f);
        float hz = 2000.0f + 5000.0f * (float)ev[P_SPRING_TONE] * (1.0f / 127.0f);
        sc.g = fm_exp2f(-9.966f * 0.04f / t60);
        sc.damp = 1.0f - fm_expf(-6.2831853f * hz / 22050.0f);
        sc.naps = tier_now >= TIER_SEVEN ? 6 : tier_now >= TIER_FOUR ? 12 : 24;   /* (7.2) */
        sc.mono = tier_now >= TIER_FOUR;
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
    {                                            /* the phaser: off from the Four-track tier down */
        phaser_cfg_t pc;
        pc.rate = 0.1f * fm_exp2f((float)ev[P_PH_RATE] * (7.49f / 127.0f));   /* 0.1 .. 18 Hz */
        pc.depth = (float)ev[P_PH_DEPTH] * (1.0f / 127.0f);
        pc.fb = (float)ev[P_PH_FB] * (0.7f / 127.0f);
        pc.square = ev[P_PH_SHAPE] != 0;
        pc.rate2 = 0.1f * fm_exp2f((float)ev[P_PH_RATE2] * (7.49f / 127.0f));
        {
            int on = tier_now == TIER_STUDIO ? ev[P_PH_ON] : PH_OFF;
            pc.mode = ev[P_PH_MODE];
            if (pc.mode == PHM_STEREO && on != PH_ECHO)   /* stereo on a return only: the dry path stays mono */
                pc.mode = PHM_CASCADE;
            if (on != ph_on || pc.mode != ph_mode)   /* moved: both sides from the same rest (no stereo from the past) */
                phaser_reset();
            ph_on = on;
            ph_mode = pc.mode;
        }
        phaser_config(&pc);
    }
    sg_hold_n = (uint32_t)((10.0f + 390.0f * (float)ev[P_SGATE_HOLD] * (1.0f / 127.0f)) * 44.1f);
    sg_rel = fm_exp2f(-9.966f / ((5.0f + 295.0f * (float)ev[P_SGATE_REL] * (1.0f / 127.0f)) * 44.1f));
    {                                            /* tape drive */
        tape_cfg_t tc;
        tc.gain = fm_exp2f((float)ev[P_DRIVE_AMT] * (2.32f / 127.0f));   /* x1 .. x5 (+14 dB) */
        tc.mix = (float)ev[P_DRIVE_MIX] * (1.0f / 127.0f);
        tape_config(&tc);
        if (ev[P_DRIVE_ON] != drive_on)
            tape_reset();
        drive_on = ev[P_DRIVE_ON];
    }
    lock_now = lock_mask(ev, t_playing ? t_beat / 4u : 0u);
    if (ev[P_HPF_STEP] != bk_step || ev[P_HPF_LP] != bk_lp) {   /* a step: at once, unsmoothed (the detents are the
                                                                 * sound); 7.5: or the other filter */
        float hz;
        bk_step = ev[P_HPF_STEP];
        bk_lp = ev[P_HPF_LP];
        hz = bk_lp ? lpf_hz(bk_step) : hpf_hz(bk_step);
        bigknob_step(&bk, hz, bk_lp);
        bigknob_step(&bkr, hz, bk_lp);
    }
}

static void drain(void)                          /* every queued command, at the start of the block */
{
    cmd_t c;
    while (cmdq_pop(&eng_q, &c)) {
        if (c.op == CMD_SET && t_playing && mot_recordable(c.id))
            cue_tick(c.id, c.v);                 /* a hand's move: on its motion tick (5.1: as a replay lands it) */
        else if (c.op == CMD_SET)
            set_by_hand(c.id, c.v);
        else if (c.op == CMD_MOTION && c.id == 0)
            mot_clear();
        else if (c.op == CMD_MOTION)
            mot_suspend(c.id == 1);
        else if (c.op == CMD_CUE)
            cue_add(CMD_SET, c.id, c.v, c.beats);
        else if (c.op == CMD_STEP && c.id < STEP_IDS) {   /* a step, or (i == NSTEPS) a pattern's settings */
            uint32_t i = c.id % (NSTEPS + 1u), cp = c.id / (NSTEPS + 1u), ch = cp / NPAT, p = cp % NPAT;
            if (i < NSTEPS)
                pat[ch][p][i] = step_unpack(c.v);
            else {
                pmeta_t m = pmeta_unpack(c.v);
                if (t_playing && p == pcur[ch].p && m.grid != pmeta[ch][p].grid)   /* a new grid: from the bar */
                    pcur[ch].org = (int32_t)((t_beat / 4u) * 4u * PPQ);
                pmeta[ch][p] = m;
            }
        }
        else if (c.op == CMD_TEMPLATE)
            cue_add(CMD_TEMPLATE, c.id, c.v, c.beats);
        else if (c.op == CMD_SCENE)
            cue_add(CMD_SCENE, c.id, 0, c.beats);
        else if (c.op == CMD_NOTE && c.id == CH_VOX) {   /* channel 8: the sirens, held (note: which); 16+: shots */
            int32_t which = c.v & 0x7F, vel = (c.v >> 8) & 0x7F;
            if (which >= 16) {                   /* 16+: a one-shot; 24+: backwards (P3.3) */
                if (vel)
                {
                    evlog_ev(6, (uint32_t)which, vel);
                    fxv_shot_dir((uint32_t)((which - 16) & 7), (uint32_t)vel, which >= 24);
                }
            } else if (vel)
            {
                evlog_ev(4, (uint32_t)which, vel);
                fxv_on(which, (uint32_t)vel);
            } else {
                evlog_ev(5, (uint32_t)which, 0);
                fxv_off(which);
            }
        } else if (c.op == CMD_NOTE && (c.id == CH_SKANK || c.id == CH_ORGAN)) {   /* a pad: the lane's chord now */
            int32_t vel = (c.v >> 8) & 0x7F, lane = ev[P_CHORD_LEN] > 0 ? ev[P_CHORD_LEN] : 1;
            if (vel)
                ctrig(c.id, ev[P_CHORD0 + (int32_t)((t_playing ? t_beat / 4u : 0u) % (uint32_t)lane)],
                            c.v & 0x7F, (uint32_t)vel);
        } else if (c.op == CMD_NOTE && c.id < NCH) {
            int32_t note = c.v & 0x7F, vel = (c.v >> 8) & 0x7F;
            if (vel)
                trig(c.id, note, (uint32_t)vel);
            else
                vrel(c.id, note);
        }
        cmds++;
    }
    if (ev[P_SPIN] && !spin_last && !spin_on)    /* the spin-back (P3.5) */
        spin_start();
    spin_last = ev[P_SPIN];
    if (ev[P_PLAY] && !t_playing && !spin_on) {  /* start: bar 1, beat 1, at this block's first sample */
        t_playing = 1;
        t_beat = t_phase = t_samples = 0;
        last_tick = -1;
        clk_last = -1;
        seq_start();
        if (ev[P_CLOCK] == CLOCK_OUT)
            clk_push(0, 0xFAu);                  /* MIDI Start, then the clocks from this sample */
    } else if (!ev[P_PLAY] && t_playing) {       /* stop: what was cued happens now; the bass let go */
        transport_stop();
    }
}

static void spin_init(void);                     /* (the spin-back's ring, below) */
void eng_init(void)
{
    uint32_t id, ch;
    spin_init();
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
    chordv_init();
    fxv_init();
    mot_init();
    mot_hold = 0;
    bigknob_init(&bk);
    bigknob_init(&bkr);
    phaser_reset();
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
    {                                            /* a riddim to start from (editable steps) in A, B..D empty */
        uint32_t p, i;
        for (ch = 0; ch < NCH; ch++)
            for (p = 0; p < NPAT; p++) {
                pmeta[ch][p] = pmeta_def();
                for (i = 0; i < NSTEPS; i++) {
                    pat[ch][p][i].lvl = LVL_OFF;
                    pat[ch][p][i].note = default_note(ch);
                    pat[ch][p][i].off = 0;
                    pat[ch][p][i].gate = 0;
                }
            }
    }
    load_template(TPL_ONE_DROP, 0xFFu);
    last_tick = -1;
    seq_start();
}

/* ------------------------------------------------------------- sequencer --- */
static void song_line(uint32_t bar)             /* a bar's line: the song captured or played */
{
    uint32_t len = (uint32_t)ev[P_SONG_LEN], b = bar;
    if (ev[P_SONG_REC]) {                        /* capture: this bar holds what the desk is now */
        if (bar < SONG_BARS) {                   /* (a pass from the top is the song: its length, what it took) */
            song_capture(bar, ev, scene_fired);
            if (bar == 0u || bar + 1u > len)
                ev[P_SONG_LEN] = (int16_t)(bar + 1u);
            song_bar_now = bar;
        }
        mot_line(bar < SONG_BARS ? (int32_t)bar : -1, 1, ev);
    } else if (ev[P_SONG_MODE] != SONG_OFF) {   /* playback */
        int sc;
        if (bar >= len) {
            if (ev[P_SONG_MODE] == SONG_ONCE) {  /* the end: stop (the UI follows) */
                t_playing = 0;
                ev[P_PLAY] = 0;
                fire_due(1);
                mot_release();
                return;
            }
            b = bar % len;
        }
        song_bar_now = b;
        mot_line((int32_t)b, 0, ev);
        evlog_snap();
        sc = song_apply(b, ev);                  /* the bar's mutes, solos, Big Knob */
        if (sc >= 0 && sc < NSCENES) {           /* its scene first, then the bar's own desk over it */
            scene_apply((uint32_t)sc, ev);
            song_apply(b, ev);
        }
        evlog_diff();
        if (b + 1u < len || ev[P_SONG_MODE] == SONG_LOOP) {   /* the next bar's patterns: known a bar ahead */
            uint32_t nb = b + 1u < len ? b + 1u : 0u, ch;
            const song_bar_t *s = &song[nb % SONG_BARS];
            int32_t line = (int32_t)((bar + 1u) * 4u * PPQ);
            if ((s->flags & BAR_SET) && s->scene && s->scene <= NSCENES && scenes[s->scene - 1u].used
                && scenes[s->scene - 1u].scope == SCOPE_PATTERNS)
                for (ch = 0; ch < NCH; ch++)
                    pnext_set(ch, scenes[s->scene - 1u].pat[ch] & 3u, line);
            if ((s->flags & BAR_SET) && (s->flags & BAR_PATS))
                for (ch = 0; ch < NCH; ch++)
                    pnext_set(ch, bar_pat(s, ch), line);
        }
    } else
        mot_line(-1, 0, ev);
    scene_fired = -1;
    strips_update();
    bus_update();
}

static int32_t fine_now(void) { return (int32_t)(t_beat * PPQ + t_phase / FU); }

static int seq_due(int32_t t)                    /* the fine tick t reached */
{
    uint32_t tb, tp;
    if (t < 0)
        return 1;
    tb = (uint32_t)t / PPQ;
    tp = ((uint32_t)t % PPQ) * FU;
    return tb < t_beat || (tb == t_beat && tp <= t_phase);
}

static uint32_t until_fine(int32_t t)            /* samples to the fine tick t (0: reached) */
{
    uint32_t tb, tp, d, tempo = (uint32_t)ev[P_TEMPO];
    if (seq_due(t))
        return 0;
    tb = (uint32_t)t / PPQ;
    tp = ((uint32_t)t % PPQ) * FU;
    if (tb - t_beat > 100u)
        return 0xFFFFFFFFu;
    d = (tb - t_beat) * BEAT_UNITS + tp - t_phase;   /* (100 beats at most: under 2^32) */
    return (d + tempo - 1u) / tempo;
}

static void seq_start(void)                      /* the transport's start (or the boot): each pattern from the top */
{
    uint32_t ch;
    voices_start();                              /* (the noise from its seed: a replay the same, 5.1) */
    spring_start();                              /* (the crash's noise too) */
    echo_wow_start();                            /* (6.3: the wow too) */
    for (ch = 0; ch < NCH; ch++) {
        pcur[ch].p = (uint8_t)(ev[P_CH(ch, CHF_PAT)] & 3);
        pcur[ch].org = 0;
        pnext_on[ch] = 0;
        held_note[ch] = -1;
        off_at[ch] = -1;
    }
    nsev = 0;
}

static void pat_sync(void)                       /* CHF_PAT changed (a cue, a scene, the song, by hand): it plays */
{
    uint32_t ch;
    for (ch = 0; ch < NCH; ch++) {
        uint32_t p = (uint32_t)ev[P_CH(ch, CHF_PAT)] & 3u;
        if (p == pcur[ch].p) {
            if (pnext_on[ch] && pnext[ch].p == p && t_playing && seq_due(pnext[ch].org) && pnext[ch].org > pcur[ch].org)
                pcur[ch] = pnext[ch], pnext_on[ch] = 0;   /* (relaunched: from the top) */
            continue;
        }
        if (pnext_on[ch] && pnext[ch].p == p)
            pcur[ch] = pnext[ch];                /* launched on its line: from there */
        else {
            pcur[ch].p = (uint8_t)p;             /* by hand, at once: from this beat */
            pcur[ch].org = t_playing ? (int32_t)(t_beat * PPQ) : 0;
        }
        pnext_on[ch] = 0;
    }
}

static void sev_add(int32_t t, uint32_t ch, uint32_t p, uint32_t i, uint32_t sb)   /* into the list, in time order */
{
    uint32_t k;
    if (nsev == NSEV)
        return;
    for (k = nsev; k > 0u && sev[k - 1u].t > t; k--)
        sev[k] = sev[k - 1u];
    sev[k].t = t;
    sev[k].ch = (uint8_t)ch;
    sev[k].p = (uint8_t)p;
    sev[k].i = (uint8_t)i;
    sev[k].sub = (uint8_t)sb;
    nsev++;
}


static uint8_t cond_pre[NCH];                    /* 6.7: PRE, each channel's last conditional step played (1) or not */
/* channel ch's steps that land in [w0, w1), for those whose grid line is in [lo, hi), played from s */
static void gather(uint32_t ch, const ppos_t *s, int32_t lo, int32_t hi, int32_t w0, int32_t w1)
{
    pmeta_t m = pmeta[ch][s->p];
    int32_t sl = grid_fine(m.grid), len = m.len ? (int32_t)m.len : 16, a = w0 - s->org - sl, b = w1 - s->org + sl / 2, n;
    if (b < 0)
        return;
    for (n = a < 0 ? 0 : a / sl; n <= b / sl; n++) {
        int32_t line = s->org + n * sl, t, i = n % len;
        step_t st;
        if (line < lo || line >= hi)
            continue;
        st = pat[ch][s->p][i];
        if (n == 0)
            cond_pre[ch] = 1;                    /* (a launch: the first PRE plays) */
        if (!LVL(st))
            continue;
        {                                        /* its CONDITION (6.7): PRE follows the last conditional step */
            uint32_t c = STEP_COND(st);
            int ok = cond_plays(ch, s->p, (uint32_t)i, (uint32_t)(n / len), c, m.rsv, ev[P_FILL] != 0, cond_pre[ch]);
            if (c != COND_NONE)
                cond_pre[ch] = (uint8_t)ok;
            if (!ok)
                continue;
        }
        t = line + swing_fine(m, n) + st.off;
        if (t < w0 && line >= w0 && w0 == 0)     /* (at the very start, nothing before: early is on the one) */
            t = 0;
        if (t >= w0 && t < w1)
            sev_add(t, ch, s->p, (uint32_t)i, 0);
        {                                        /* REPEAT (5.8): a flam's ghost 20 ms early, or 2-4 within the step */
            uint32_t r = STEP_REPEAT(st), k;
            if (r == REP_FLAM) {
                int32_t tf = t - ev[P_TEMPO] / 125;   /* (20 ms: tempo x 10 / 1250 of a beat's 240 ticks) */
                if (tf >= w0 && tf < w1 && tf >= 0)
                    sev_add(tf, ch, s->p, (uint32_t)i, 0x80u);
            } else if (r >= REP_2 && r <= REP_4)
                for (k = 1; k < r; k++) {
                    int32_t tk = t + (int32_t)k * sl / (int32_t)r;
                    if (tk >= w0 && tk < w1)
                        sev_add(tk, ch, s->p, (uint32_t)i, k);
                }
        }
    }
}

static void seq_play(uint32_t tick)              /* sixteenth `tick`'s line: the song, the runway, its steps gathered */
{
    uint32_t ch;
    int32_t w0 = (int32_t)(tick * F16), w1 = w0 + (int32_t)F16;
    if (tick % 16u == 0u) {
        song_line(tick / 16u);
        if (!t_playing)
            return;
        pat_sync();
    }
    if (tick % 4u == 0u)
        runway_tick(tick / 4u);
    nsev = 0;
    for (ch = 0; ch < NCH; ch++) {
        if (pnext_on[ch] && pnext[ch].org >= w0 - (int32_t)PPQ) {   /* a launch to come: each its own steps */
            gather(ch, &pcur[ch], -0x7FFFFFFF, pnext[ch].org, w0, w1);
            gather(ch, &pnext[ch], pnext[ch].org, 0x7FFFFFFF, w0, w1);
        } else
            gather(ch, &pcur[ch], -0x7FFFFFFF, 0x7FFFFFFF, w0, w1);
    }
}

static void seq_hit(const sev_t *e)              /* a step, now */
{
    uint32_t ch = e->ch;
    step_t st = pat[ch][e->p][e->i];
    pmeta_t m = pmeta[ch][e->p];
    uint32_t vel = lvl_vel(e->sub == 0x80u ? LVL_GHOST : LVL(st));   /* (a flam's ghost: softer) */
    if (!LVL(st))
        return;                                  /* (cleared since it was gathered) */
    if (is_drum(ch))                             /* its TUNE and DECAY locks, for this hit (5.8) */
        voice_locks(ch, lock_tune(st), lock_decay(st));
    if (ch == CH_SKANK || ch == CH_ORGAN) {      /* the chord lane's chord at this bar, in the step's voicing */
        int32_t lane = ev[P_CHORD_LEN] > 0 ? ev[P_CHORD_LEN] : 1;
        int32_t chord = ev[P_CHORD0 + (int32_t)(((uint32_t)(e->t > 0 ? e->t : 0) / (4u * PPQ)) % (uint32_t)lane)];
        if (held_note[ch] >= 0) {                /* (a B-3 chord still held: let go first) */
            vrel(ch, held_note[ch]);
            held_note[ch] = -1;
        }
        ctrig(ch, chord, STEP_NOTE(st), vel);
        chord_last[ch] = chord;
        if (ch == CH_ORGAN && ev[P_VOICE(CH_ORGAN, VO_MODEL)] == ORG_B3) {   /* the B-3 held for its gate (5.15): a
                                                  * step, or the gate's length; the bubble a stab, as built */
            chordv_hold(ch);
            held_note[ch] = 0;
            off_at[ch] = e->t + (st.gate ? 4 * (int32_t)st.gate : grid_fine(m.grid));
        }
    } else {
        if ((ch == CH_BASS || ch == CH_LEAD) && held_note[ch] >= 0) {   /* one note at a time */
            vrel(ch, held_note[ch]);
            held_note[ch] = -1;
        }
        trig(ch, STEP_NOTE(st), vel);
        if (ch == CH_BASS || ch == CH_LEAD) {    /* held for its gate: the bass a step (staccato), the lead two */
            held_note[ch] = (int8_t)STEP_NOTE(st);
            off_at[ch] = e->t + (st.gate ? 4 * (int32_t)st.gate : grid_fine(m.grid) * (ch == CH_LEAD ? 2 : 1));
        }
    }
    seq_hits++;
    seq_hit_at[ch] = t_samples;
}

static void seq_fire(void)                       /* the notes ending now, then the steps landing now */
{
    uint32_t ch;
    for (ch = CH_BASS; ch <= CH_LEAD; ch++)
        if (held_note[ch] >= 0 && seq_due(off_at[ch])) {
            vrel(ch, held_note[ch]);
            held_note[ch] = -1;
        }
    while (nsev && seq_due(sev[0].t)) {
        sev_t e = sev[0];
        uint32_t k;
        for (k = 1; k < nsev; k++)
            sev[k - 1u] = sev[k];
        nsev--;
        seq_hit(&e);
    }
}

static uint32_t until_seq(void)                  /* samples to the next step or note end */
{
    uint32_t d = nsev ? until_fine(sev[0].t) : 0xFFFFFFFFu, ch, u;
    for (ch = CH_BASS; ch <= CH_LEAD; ch++)
        if (held_note[ch] >= 0 && (u = until_fine(off_at[ch])) < d)
            d = u;
    return d;
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

/* ------------------------------------------------------------- spin-back --- */
/* P3.5: the output's last 1.6 s kept (11,025 Hz, mono, 8-bit mu-law since review-2026-11 5.4: 17.3 KB in the pool, was
 * 35 KB as int16; a rewind is lo-fi anyway); HOME + PLAY plays it backwards,
 * the "record" dragged back faster and faster (1x .. 2.6x) and fading over 0.8 s, then a beat of silence with the
 * effects wiped, then the tune again from bar 1 ("pull up, come again"), if it was playing */
#define SPIN_N 17664u
#define SPIN_SECS 0.8f
static PLAT_POOL uint8_t spin_ring[SPIN_N];      /* G.711 mu-law */
static PLAT_POOL int16_t spin_dec[256];          /* mu-law -> int16 */
#define MULAW_SILENT 0xFFu

static uint8_t mulaw_enc(int32_t v)              /* int16 -> G.711 mu-law */
{
    uint32_t sign = 0, mag, exp = 7, mant;
    if (v < 0) {
        sign = 0x80u;
        v = -v;
    }
    mag = (uint32_t)(v > 32635 ? 32635 : v) + 0x84u;
    while (exp > 0u && !(mag & (0x4000u >> (7u - exp))))
        exp--;
    mant = (mag >> (exp + 3u)) & 0x0Fu;
    return (uint8_t)~(sign | exp << 4 | mant);
}

static void spin_init(void)                      /* the decoder's table; the ring silent */
{
    uint32_t i;
    for (i = 0; i < 256u; i++) {
        uint32_t u = ~i & 0xFFu, exp = (u >> 4) & 7u, mant = u & 0x0Fu;
        int32_t mag = (int32_t)(((mant << 3) + 0x84u) << exp) - 0x84;
        spin_dec[i] = (int16_t)(u & 0x80u ? -mag : mag);
    }
    for (i = 0; i < SPIN_N; i++)
        spin_ring[i] = MULAW_SILENT;
}
static uint32_t spin_w, spin_acc_n, spin_gap, spin_again;
static int32_t spin_acc;
static float spin_pos, spin_t;

/* ------------------------------------------------------------ MIDI clock --- */
/* OUT: 24 clocks a beat on the transport's own clock. The render notes each one's moment (this block's start on
 * plat_ticks24's clock, plus its sample); the 1 kHz tick sends it when that moment comes (app_tick: eng_clock_pop), so
 * a clock goes out within a millisecond of its time, not bunched a block at a time. Start at the transport's start,
 * Stop at its stop.
 * IN: Start / Continue start the transport (from bar 1), Stop stops it; the clocks set the tempo (their spacing over
 * the last beat) and keep the bar in phase (the tempo nudged by the beats we are ahead or behind, at most 5 %) */
#define Q24 (BEAT_UNITS / 24u)                   /* a clock in beat units: 1,102,500 (exact) */
#define CQ 64u
static struct {
    uint32_t due;
    uint8_t st;
} cq[CQ];
static volatile uint32_t cq_w, cq_r;
static uint32_t blk_t24, clk_sent;
static uint32_t ext_t, ext_dt[24], ext_n, ext_ticks;
static int ext_have;

uint32_t eng_clock_now(void)                     /* the clock's time base, 24 MHz: the device's ticks; the host's
                                                  * simulated milliseconds (its plat_ticks24 is the computer's clock) */
{
#ifdef PLAT_HOST
    return plat_ms() * 24000u;
#else
    return plat_ticks24();
#endif
}

static void clk_push(uint32_t pos, uint32_t st)
{
    uint32_t w = cq_w;
    if (w - cq_r >= CQ)
        return;
    cq[w % CQ].due = blk_t24 + pos * 5442u / 10u;   /* (24 MHz ticks: 544.2 a sample) */
    cq[w % CQ].st = (uint8_t)st;
    __asm__ volatile("" ::: "memory");
    cq_w = w + 1u;
}

int eng_clock_pop(uint32_t now, uint32_t *st)    /* the 1 kHz tick: the next clock message now due (1) */
{
    uint32_t r = cq_r;
    if (r == cq_w || (int32_t)(now - cq[r % CQ].due) < 0)
        return 0;
    *st = cq[r % CQ].st;
    if (*st == 0xF8u)
        clk_sent++;
    __asm__ volatile("" ::: "memory");
    cq_r = r + 1u;
    return 1;
}

uint32_t eng_clocks_sent(void) { return clk_sent; }

static uint32_t until_clock(void)                /* samples to the next clock's moment */
{
    uint32_t b = (t_phase / Q24 + 1u) * Q24, tempo = (uint32_t)ev[P_TEMPO];
    return (b - t_phase + tempo - 1u) / tempo;
}

void eng_clock_in(uint32_t st, uint32_t t24)     /* the audio interrupt (app_render's MIDI in), before eng_render */
{
    if (ev[P_CLOCK] != CLOCK_IN)
        return;
    if (st == 0xFAu || st == 0xFBu) {            /* Start, Continue: from the top (the next block) */
        ev[P_PLAY] = 1;
        mot_fb_push(P_PLAY, 1);
        ext_ticks = 0;
        return;
    }
    if (st == 0xFCu) {
        ev[P_PLAY] = 0;
        mot_fb_push(P_PLAY, 0);
        return;
    }
    if (st != 0xF8u)
        return;
    if (ext_have) {
        uint32_t dt = t24 - ext_t;
        if (dt > 6000u && dt < 4000000u) {       /* 10 .. 600 BPM's spacing: a real clock */
            ext_dt[ext_n % 24u] = dt;
            ext_n++;
        }
    }
    ext_t = t24;
    ext_have = 1;
    ext_ticks++;
    if (ext_n >= 6u) {
        uint32_t k, m = ext_n < 24u ? ext_n : 24u;
        float sum = 0.0f, tempo, err;
        int32_t v;
        for (k = 0; k < m; k++)
            sum += (float)ext_dt[k];
        tempo = 600000000.0f * (float)m / sum;   /* BPM x 10: 60 s x 10 x 24 MHz / (24 a beat x the spacing) */
        if (t_playing && ext_ticks) {            /* the phase: our beats against the clocks' */
            err = ((float)t_beat + (float)t_phase * (1.0f / (float)BEAT_UNITS)) - (float)(ext_ticks - 1u) * (1.0f / 24.0f);
            err = err > 0.2f ? 0.2f : err < -0.2f ? -0.2f : err;
            tempo *= 1.0f - 0.25f * err;         /* ahead: slower; behind: faster (at most 5 %) */
        }
        v = param_clamp(P_TEMPO, (int32_t)(tempo + 0.5f));
        if (v != ev[P_TEMPO]) {
            ev[P_TEMPO] = (int16_t)v;
            mot_fb_push(P_TEMPO, v);             /* (the screen follows) */
        }
    }
}

static void transport_stop(void)                 /* what a stop does: the cues now, motion's holds and the notes let go */
{
    if (ev[P_CLOCK] == CLOCK_OUT)
        clk_push(0, 0xFCu);
    t_playing = 0;
    fire_due(1);
    mot_release();
    {
        uint32_t ch;
        for (ch = 0; ch < NCH; ch++)
            if (held_note[ch] >= 0) {
                vrel(ch, held_note[ch]);
                held_note[ch] = -1;
            }
    }
    nsev = 0;
    pat_sync();
}

static void spin_start(void)
{
    spin_again = t_playing;
    if (t_playing)
        transport_stop();
    spin_on = 1;
    spin_t = 0.0f;
    spin_pos = (float)(spin_w ? spin_w - 1u : SPIN_N - 1u);
}

static void spin_block(int32_t *lr, uint32_t n)  /* after the render: the ring fed, or the spin played over it */
{
    uint32_t i;
    if (!spin_on) {
        for (i = 0; i < n; i++) {                /* 4 samples averaged to one (a rewind is muffled anyway) */
            spin_acc += (lr[2u * i] >> 9) + (lr[2u * i + 1u] >> 9);
            if (++spin_acc_n == 4u) {
                int32_t v = spin_acc / 4;
                spin_ring[spin_w] = mulaw_enc(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
                spin_w = spin_w + 1u == SPIN_N ? 0u : spin_w + 1u;
                spin_acc = 0;
                spin_acc_n = 0;
            }
        }
        return;
    }
    for (i = 0; i < n; i++) {
        float out = 0.0f;
        if (spin_on == 1) {
            float sp = 1.0f + 2.0f * spin_t, amp, fr, a, b;
            uint32_t k0, k1;
            spin_pos -= sp * 0.25f;
            if (spin_pos < 0.0f)
                spin_pos += (float)SPIN_N;
            k0 = (uint32_t)spin_pos;
            k1 = k0 + 1u == SPIN_N ? 0u : k0 + 1u;
            fr = spin_pos - (float)k0;
            a = (float)spin_dec[spin_ring[k0]];
            b = (float)spin_dec[spin_ring[k1]];
            amp = spin_t < SPIN_SECS - 0.25f ? 1.0f : (SPIN_SECS - spin_t) * 4.0f;
            out = (a + (b - a) * fr) * (amp > 0.0f ? amp : 0.0f) * 256.0f;   /* (stored as (L + R) / 512) */
            spin_t += 1.0f / 44100.0f;
            if (spin_t >= SPIN_SECS) {           /* stopped: the tails wiped, a beat's silence */
                spin_on = 2;
                spin_gap = BEAT_UNITS / (uint32_t)ev[P_TEMPO];
                voices_panic();
                echo_init();
                spring_init();
                phaser_reset();
            }
        } else if (spin_gap) {
            spin_gap--;
        } else {
            spin_on = 0;                         /* (drain starts it again from bar 1 if PLAY is still on) */
            if (!spin_again)
                ev[P_PLAY] = 0;
        }
        lr[2u * i] = lr[2u * i + 1u] = (int32_t)out;
    }
}

int eng_spinning(void) { return spin_on != 0; }

/* ---------------------------------------------------------------- sources --- */
static const int32_t *live_lr;                   /* the live input this block (P2.8), its frames, its scale */
static uint32_t live_n, live_at;
static float live_k, live_dc;

void eng_live_in(const int32_t *lr, uint32_t n, float full)
{
    live_lr = lr;
    live_n = lr ? n : 0u;
    live_k = full > 0.0f ? 0.5f / full : 0.0f;   /* (L + R) / 2, to -1..1 */
}

static int live_add(float *o, uint32_t n, int any)   /* the live input into channel 8 (after a DC block) */
{
    uint32_t i;
    float g;
    int32_t v = ev[P_VOICE(CH_VOX, VX_LIVE)];
    if (!v || !live_lr || live_at >= live_n)
        return any;
    g = (float)v * (1.0f / 100.0f);
    g *= g * live_k;                             /* a square law: 100 as it comes */
    for (i = 0; i < n; i++) {
        uint32_t k = live_at + i < live_n ? live_at + i : live_n - 1u;
        float x = (float)(live_lr[2u * k] + live_lr[2u * k + 1u]) * g;
        live_dc = undenorm(live_dc + (x - live_dc) * 0.0028f);   /* ~20 Hz: no DC on the low end */
        o[i] = any ? o[i] + x - live_dc : x - live_dc;
    }
    return 1;
}

void eng_trig(uint32_t ch, int32_t note, uint32_t vel) { trig(ch, note, vel); }
void eng_release(uint32_t ch, int32_t note) { vrel(ch, note); }
int32_t eng_note(void) { return voice_note(CH_BASS); }
int32_t eng_voice_note(uint32_t ch) { return voice_note(ch); }

static int source(uint32_t ch, float *o, uint32_t n)   /* the channel's sound, mono; 0: silent */
{
    uint32_t i;
    int any = 0;
    any = ch == CH_SKANK || ch == CH_ORGAN ? chordv_render(ch, o, n) : ch == CH_VOX ? fxv_render(o, n)
                                                                                  : voice_render(ch, o, n);
    if (ch == CH_VOX)
        any = live_add(o, n, any);
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
#ifdef PLAT_HOST
static float src_pk[NCH], src_last[NCH];         /* (tests) each voice's own peak: this block's, the last block's */
#endif
/* the stems (QoL 4: the USB recording's channels 3-10, eng_stems): eight a frame, the block's, full scale 1.0 */
#define NSTEM 8u
static PLAT_POOL float stemf[ENG_BLOCK * NSTEM];
static uint32_t stem_mode = STEMS_OFF, stem_at;   /* (this block's; render_seg's segment from stem_at) */
static inline void stem_add(uint32_t i, uint32_t k, float x) { stemf[(stem_at + i) * NSTEM + k] += x; }
static uint32_t stem_col(uint32_t ch)            /* the channel's stem: its own, or its bus's */
{
    static const uint8_t BUS[NCH] = {0, 0, 0, 1, 2, 2, 3, 3};   /* drums, bass, chords, lead and vox */
    return stem_mode == STEMS_CHANNELS ? ch : BUS[ch];
}

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
#ifdef PLAT_HOST
        if (on)                                  /* (tests) the voice's own level, before its strip */
            for (i = 0; i < n; i++)
                src_pk[ch] = fm_maxf(src_pk[ch], fm_fabsf(buf[i]));
#endif
        if (on && ((ph_on == PH_HATS && ch == CH_HATS) || (ph_on == PH_SKANK && ch == CH_SKANK)))
            phaser_run(buf, 0, n);               /* the phaser on a channel (before its strip) */
        if (on && drive_on == DRIVE_BASS && ch == CH_BASS)
            tape_run(buf, 0, n);                 /* the bass through the tape (C7) */
        prof_mark(PART_CH0 + ch);
        if (ch == CH_ORGAN) {                    /* the B-3's stereo horn (5.15): its side, at the strip's level */
            const float *sd = on ? chordv_side() : 0;
            org_side_n = 0;
            if (sd && s->dry.g != 0.0f) {
                float g = s->dry.g;
                for (i = 0; i < n; i++)
                    org_side[i] = g * sd[i];
                org_side_n = n;
            }
        }
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
                dry[i] += desk_mci ? desk_enc(d) : d;
                ebus[i] += ramp_next(&s->echo) * x;
                sbus[i] += ramp_next(&s->spring) * x;
                pk = fm_maxf(pk, fm_fabsf(d));
                if (stem_mode != STEMS_OFF)
                    stem_add(i, stem_col(ch), d);
            }
        } else {                                 /* (most blocks) still: constant gains, a send at 0 skipped */
            float gd = s->dry.g, ge = s->echo.g, gs = s->spring.g;
            if (gd != 0.0f) {
                uint32_t k = stem_col(ch);
                if (desk_mci)
                    for (i = 0; i < n; i++) {
                        float d = gd * buf[i];
                        dry[i] += desk_enc(d);
                        pk = fm_maxf(pk, fm_fabsf(d));
                    }
                else
                    for (i = 0; i < n; i++) {
                        float d = gd * buf[i];
                        dry[i] += d;
                        pk = fm_maxf(pk, fm_fabsf(d));
                    }
                if (stem_mode != STEMS_OFF)
                    for (i = 0; i < n; i++)
                        stem_add(i, k, gd * buf[i]);
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
    echo_run(ebus, eret, eretr, n);
    prof_mark(PART_ECHO);
    if (xs_e2s.g != 0.0f || xs_e2s.left)         /* the echo's return into the spring */
        for (i = 0; i < n; i++)
            sbus[i] += ramp_next(&xs_e2s) * eret[i];
    if (ph_on == PH_ECHO)                        /* the phaser on the echo's return */
        phaser_run(eret, eretr, n);
    if (ev[P_HPF_ON] == HPF_ON_ECHO) {           /* the Big Knob on the echo's return */
        bigknob_run(&bk, eret, n);
        bigknob_run(&bkr, eretr, n);
    }
    spring_run(sbus, sret, sretr, n);
    prof_mark(PART_SPRING);
    for (i = 0; i < n; i++)
        sret_last[i] = 0.5f * (sret[i] + sretr[i]);
    sret_last_n = xs_s2e.g != 0.0f || xs_s2e.left ? n : 0u;
    {
        float rp = 0.0f, op = 0.0f, srp = 0.0f;
        for (i = 0; i < n; i++) {
            float ge = ramp_next(&echo_ret), gs = ramp_next(&spring_ret);
            if (ev[P_SGATE_ON]) {                /* the gate: open (in ~1 ms) while held, then closing */
                if (sg_hold) {
                    sg_hold--;
                    sg_env = fm_minf(1.0f, sg_env + 0.0227f);
                } else
                    sg_env = undenorm(sg_env * sg_rel);
                gs *= sg_env;
            }
            float r, rr, sr, srr;
            if (tails_left) {                    /* tails off: the returns down to nothing (a straight line, 20 ms) */
                float g = (float)--tails_left * (1.0f / (float)TAILS_N);
                ge *= g;
                gs *= g;
                if (!tails_left)
                    tails_done = 1;
            } else if (tails_done) {             /* (the rest of the block: silent until they are emptied) */
                ge = 0.0f;
                gs = 0.0f;
            }
            r = ge * eret[i];
            rr = ge * eretr[i];
            sr = gs * sret[i];
            srr = gs * sretr[i];
            ep = fm_maxf(ep, fm_fabsf(ebus[i]));
            sp = fm_maxf(sp, fm_fabsf(sbus[i]));
            rp = fm_maxf(rp, fm_maxf(fm_fabsf(r), fm_fabsf(rr)));
            srp = fm_maxf(srp, fm_maxf(fm_fabsf(sr), fm_fabsf(srr)));
            dryr[i] = dry[i] + rr + srr;         /* the mono dry path to both sides; the returns stereo */
            dry[i] += r + sr;
            if (org_side_n) {                    /* (the organ's horn, its side only: above 800 Hz, so the low end
                                                  * stays mono; 5.15) */
                dry[i] += org_side[i];
                dryr[i] -= org_side[i];
            }
            if (stem_mode == STEMS_BUSES) {      /* the returns, L R: the echo's on 5-6, the spring's on 7-8 */
                stem_add(i, 4, r);
                stem_add(i, 5, rr);
                stem_add(i, 6, sr);
                stem_add(i, 7, srr);
            }
        }
        spring_ret_peak = srp;
        if (ph_on == PH_MIX)                     /* the phaser on the whole mix: both sides the same sweep */
            phaser_run(dry, dryr, n);
        if (drive_on == DRIVE_MIX)               /* the whole mix through the tape: the glue */
            tape_run(dry, dryr, n);
        if (ev[P_HPF_ON] == HPF_ON_MIX) {        /* the Big Knob on the whole mix */
            bigknob_run(&bk, dry, n);
            bigknob_run(&bkr, dryr, n);
        }
        if (ev[P_SPEAKER] == OUT_SPEAKER)       /* 6.2: BASS+ for the FM-1's own speaker */
            speaker_run(dry, dryr, n);
        for (i = 0; i < n; i++) {
            float y = dry[i], yr = dryr[i];
            op = fm_maxf(op, fm_maxf(fm_fabsf(y), fm_fabsf(yr)));
            gain_now = undenorm(gain_now + (gain - gain_now) * 0.004f);   /* ~5 ms: MASTER does not click */
            if (tick_amp > 1e-4f) {              /* the runway's tick, not sent anywhere */
                float tk = tick_amp * fm_sin_turns(tick_ph);
                y += tk;
                yr += tk;
                tick_ph = tick_ph + tick_inc >= 1.0f ? tick_ph + tick_inc - 1.0f : tick_ph + tick_inc;
                tick_amp *= 0.9985f;             /* ~15 ms */
            }
            lr[2 * i] = (int32_t)(y * gain_now);
            lr[2 * i + 1] = (int32_t)(yr * gain_now);
        }
        echo_ret_peak = rp;
        out_peak = op;
    }
    echo_peak = ep;
    spring_peak = sp;
    prof_mark(PART_OUT);
}

const float *eng_stems(int want)                 /* before eng_render: 1 = a computer records (the stems for this block)
                                                  * (0: none made); the block's frames, NSTEM floats each */
{
    uint32_t i;
    stem_mode = want ? (uint32_t)ev[P_USB_STEMS] : STEMS_OFF;
    if (stem_mode == STEMS_OFF)
        return 0;
    for (i = 0; i < ENG_BLOCK * NSTEM; i++)
        stemf[i] = 0.0f;
    return stemf;
}

#ifdef PLAT_HOST
float eng_src_peak(uint32_t ch) { return ch < NCH ? src_last[ch] : 0.0f; }
#endif

void eng_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    uint32_t pos = 0;
    float gain = (float)master_q12 * (8300000.0f / 4096.0f);
    if (n > ENG_BLOCK)
        n = ENG_BLOCK;
    prof_t = plat_ticks24();
    blk_t24 = eng_clock_now();
    if (!echo_thrown())                          /* 6.4: the throw's lift eases out, ~0.5 s (a time constant of 0.2) */
        thr_env = undenorm(thr_env * fm_expf(-(float)n / (0.2f * 44100.0f)));
    drain();
    pat_sync();
    strips_update();
    bus_update();
    prof_mark(PART_CMD);
    /* from event to event: a sixteenth's line (the sequencer) or a cue (launch quantise), each on its sample */
    for (;;) {
        uint32_t d = n - pos;
        if (t_playing) {
            uint32_t tick = t_beat * 4u + t_phase / Q16, u;
            if (ncues && until_cue() == 0) {     /* the cues at this sample first: a scene on the line is there */
                fire_due(0);                     /* for the line's step, and for the song's capture */
                strips_update();
                bus_update();
            }
            pat_sync();
            if ((int32_t)tick != last_tick) {
                last_tick = (int32_t)tick;
                seq_play(tick);
            }
            if (t_playing)
                seq_fire();
            if (!t_playing)                      /* (a song played once has ended: the rest rings out) */
                goto render;
            if (mot_play()) {                    /* the bar's motion: the moves due at this sample */
                strips_update();
                bus_update();
            }
            if (ev[P_CLOCK] == CLOCK_OUT) {      /* MIDI clock out: this sample's clock, timed for the tick */
                int32_t ci = (int32_t)(t_beat * 24u + t_phase / Q24);
                if (ci != clk_last) {
                    clk_last = ci;
                    clk_push(pos, 0xF8u);
                }
                u = until_clock();
                if (u < d)
                    d = u;
            }
            u = until_16th();
            if (u < d)
                d = u;
            u = until_seq();
            if (u < d)
                d = u;
            u = until_motion();
            if (u < d)
                d = u;
            if (ncues) {
                u = until_cue();
                if (u < d)
                    d = u;
            }
        }
    render:
        prof_mark(PART_CMD);
        if (pos >= n)
            break;
        live_at = pos;
        stem_at = pos;
        render_seg(lr + 2u * pos, d, gain);
        if (t_playing)
            t_step(d);
        pos += d;
    }
    if (tails_done) {                            /* tails off: faded, so the effects emptied (silent: no click) */
        tails_done = 0;
        echo_clear();
        spring_init();
        if (ph_on == PH_ECHO || ph_on == PH_OFF)
            phaser_reset();                      /* (on the dry path it rings out with the voices: emptied there, a
                                                  * voice still sounding through it would click) */
        sret_last_n = 0;
    }
#ifdef PLAT_HOST
    {
        uint32_t ch;
        for (ch = 0; ch < NCH; ch++) {
            src_last[ch] = src_pk[ch];
            src_pk[ch] = 0.0f;
        }
    }
#endif
    spin_block(lr, n);
    prof_block();
}

void eng_panic(void) { voices_panic(); }

int32_t eng_param(uint32_t id) { return id < P_NIDS ? ev[id] : 0; }
float eng_ch_peak(uint32_t ch) { return ch < NCH ? strip[ch].peak : 0.0f; }
uint32_t eng_trig_count(uint32_t ch) { return ch < NCH ? trig_n[ch] : 0u; }
float eng_echo_peak(void) { return echo_peak; }
float eng_spring_peak(void) { return spring_peak; }
uint32_t eng_cmds(void) { return cmds; }
uint32_t eng_seq_hits(void) { return seq_hits; }
uint32_t eng_ticks(void) { return ticks; }
int eng_tier(void) { return tier_now; }
uint32_t eng_load_pct(void) { return load_pct; }
uint32_t eng_tier_changes(void) { return tier_changes; }
uint32_t eng_song_bar(void) { return song_bar_now; }
int32_t eng_chord_last(uint32_t ch) { return ch < NCH ? chord_last[ch] : 0; }
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
step_t eng_step(uint32_t ch, uint32_t i) { return pat[ch % NCH][pcur[ch % NCH].p][i % NSTEPS]; }
step_t eng_step_at(uint32_t ch, uint32_t p, uint32_t i) { return pat[ch % NCH][p % NPAT][i % NSTEPS]; }
pmeta_t eng_pmeta(uint32_t ch, uint32_t p) { return pmeta[ch % NCH][p % NPAT]; }
uint32_t eng_pat(uint32_t ch) { return pcur[ch % NCH].p; }
int32_t eng_fine(void) { return t_playing ? fine_now() : -1; }
int32_t eng_pat_org(uint32_t ch) { return pcur[ch % NCH].org; }
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
        return g + (throw_lift(g) - g) * (echo_thrown() ? 1.0f : thr_env);
    }
}
int eng_playing(void) { return t_playing; }
uint32_t eng_beat(void) { return t_beat; }
uint32_t eng_fire_at(void) { return t_fire_at; }
uint32_t eng_fires(void) { return t_fires; }
uint32_t eng_cues(void) { return ncues; }
uint32_t eng_cue_overflows(void) { return cue_overflows; }
