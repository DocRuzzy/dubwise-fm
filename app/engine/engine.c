/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* engine.h. Compiled on its own at -O2 (fm1.toml o2). Floats only: single-precision FPU (no double). */
#include "engine.h"
#include "fastmath.h"

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
static float buf[ENG_BLOCK], dry[ENG_BLOCK], ebus[ENG_BLOCK], sbus[ENG_BLOCK];
static volatile float echo_peak, spring_peak;
static volatile uint32_t cmds;
static float gain_now;                           /* MASTER, glided: no step at a block's edge */

/* the key voice: one voice, the last note wins (the template's; P0.5 replaces it with the bass voice) */
static volatile int32_t voice_note = -1;
static volatile float voice_vel = 1.0f;
static float voice_phase, voice_level;

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
        ramp_to(&s->dry, g);
        ramp_to(&s->echo, g * send(ev[P_CH(ch, CHF_ECHO_SEND)]));
        ramp_to(&s->spring, g * send(ev[P_CH(ch, CHF_SPRING_SEND)]));
    }
}

static void drain(void)                          /* every queued command, at the start of the block */
{
    cmd_t c;
    while (cmdq_pop(&eng_q, &c)) {
        if (c.op == CMD_SET && c.id < P_NIDS && param_desc(c.id))
            ev[c.id] = (int16_t)param_clamp(c.id, c.v);
        cmds++;
    }
}

void eng_init(void)
{
    uint32_t id, ch;
    for (id = 0; id < P_NIDS; id++) {
        const pdesc_t *d = param_desc(id);
        ev[id] = (int16_t)(d ? d->def : 0);
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
    cmds = 0;
}

/* ---------------------------------------------------------------- sources --- */
void eng_note_on(int32_t note, uint32_t vel)
{
    voice_vel = (float)vel * (1.0f / 127.0f);
    voice_note = note;
}
void eng_note_off(int32_t note)
{
    if (voice_note == note)
        voice_note = -1;
}
int32_t eng_note(void) { return voice_note; }

static int key_voice(float *o, uint32_t n)       /* 0: silent (nothing written) */
{
    uint32_t i;
    int32_t note = voice_note;
    float target = note >= 0 ? 0.25f * voice_vel : 0.0f;
    float inc = note >= 0 ? 440.0f * fm_exp2f((float)(note - 69) / 12.0f) / 44100.0f : 0.0f;
    int tri = ev[P_VOICE(CH_BASS, VF_WAVE)] != 0;
    if (note < 0 && voice_level < 1e-6f) {
        voice_level = 0.0f;
        return 0;
    }
    for (i = 0; i < n; i++) {
        voice_level += (target - voice_level) * 0.002f;          /* ~10 ms attack and release */
        voice_phase += inc;
        if (voice_phase >= 1.0f)
            voice_phase -= 1.0f;
        o[i] = (tri ? 4.0f * fm_fabsf(voice_phase - 0.5f) - 1.0f : fm_sin_turns(voice_phase)) * voice_level;
    }
    return 1;
}

static int source(uint32_t ch, float *o, uint32_t n)   /* the channel's sound, mono; 0: silent */
{
    uint32_t i;
    int any = 0;
    if (ch == CH_BASS)
        any = key_voice(o, n);
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
void eng_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    uint32_t ch, i;
    float gain = (float)master_q12 * (8300000.0f / 4096.0f), ep = 0.0f, sp = 0.0f;
    if (n > ENG_BLOCK)
        n = ENG_BLOCK;
    drain();
    strips_update();
    for (i = 0; i < n; i++)
        dry[i] = ebus[i] = sbus[i] = 0.0f;
    for (ch = 0; ch < NCH; ch++) {               /* a fixed order: the same sum on one core or two */
        strip_t *s = &strip[ch];
        float pk = 0.0f;
        if (!source(ch, buf, n)) {
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
        s->peak = pk;
    }
    for (i = 0; i < n; i++) {
        float y;
        ep = fm_maxf(ep, fm_fabsf(ebus[i]));
        sp = fm_maxf(sp, fm_fabsf(sbus[i]));
        gain_now += (gain - gain_now) * 0.004f;  /* ~5 ms: turning MASTER does not click */
        y = dry[i] * gain_now;                   /* echo and spring returns join here (P0.3, P1.2) */
        lr[2 * i] = (int32_t)y;
        lr[2 * i + 1] = (int32_t)y;
    }
    echo_peak = ep;
    spring_peak = sp;
}

void eng_panic(void)
{
    voice_note = -1;
    voice_level = 0.0f;
}

int32_t eng_param(uint32_t id) { return id < P_NIDS ? ev[id] : 0; }
float eng_ch_peak(uint32_t ch) { return ch < NCH ? strip[ch].peak : 0.0f; }
float eng_echo_peak(void) { return echo_peak; }
float eng_spring_peak(void) { return spring_peak; }
uint32_t eng_cmds(void) { return cmds; }
