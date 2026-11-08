/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* chordv.h. Compiled on its own at -O2 (fm1.toml o2). Floats only. */
#include "chordv.h"
#include "params.h"
#include "fastmath.h"
#include "dsp.h"

#define SR 44100.0f
#define INV_SR (1.0f / 44100.0f)
#define NV 3

static inline float t60k(float ms) { return fm_exp2f(-9.966f / (ms * 0.001f * SR)); }
static inline float midi_hz(int32_t n) { return 440.0f * fm_exp2f((float)(n - 69) * (1.0f / 12.0f)); }
static inline float wrap(float p) { return p >= 1.0f ? p - 1.0f : p; }
static inline float lin(int16_t v, float lo, float hi) { return lo + (hi - lo) * (float)v * (1.0f / 127.0f); }

typedef struct {
    float pc, pm, f, ienv, amp;
} cv_t;

static struct {                                  /* the skank */
    cv_t v[NV];
    float vel, index, ik, ak, ratio, hp_a, lp_a, hx, hy, ly;
    int on;
} sk;

static struct {                                  /* the organ */
    cv_t v[NV];
    float p2[NV], p3[NV], p4[NV], pk[NV];        /* the partials' phases */
    float vel, w2, w3, w4, ak, click, cenv, ck;
    int on;
} og;

void chordv_init(void)
{
    sk.on = og.on = 0;
}

void chordv_config(const int16_t *ev)
{
    float tone = lin(ev[P_VOICE(CH_SKANK, VS_TONE)], 600.0f, 2400.0f);
    sk.index = lin(ev[P_VOICE(CH_SKANK, VS_INDEX)], 0.0f, 4.0f);
    sk.ik = t60k(30.0f);
    sk.ak = t60k(lin(ev[P_VOICE(CH_SKANK, VS_DECAY)], 60.0f, 200.0f));
    sk.ratio = ev[P_VOICE(CH_SKANK, VS_RATIO)] ? 3.0f : 1.0f;
    sk.hp_a = fm_expf(-6.2831853f * (tone * 0.5f) * INV_SR);        /* the band: tone / 2 .. tone x 2 */
    sk.lp_a = 1.0f - fm_expf(-6.2831853f * (tone * 2.0f) * INV_SR);
    {
        float d = lin(ev[P_VOICE(CH_ORGAN, VO_DRAWBAR)], 0.0f, 1.0f);
        og.w2 = 0.6f * d;
        og.w3 = 0.4f * d;
        og.w4 = 0.25f * d;
    }
    og.ak = t60k(lin(ev[P_VOICE(CH_ORGAN, VO_DECAY)], 40.0f, 300.0f));
    og.click = lin(ev[P_VOICE(CH_ORGAN, VO_CLICK)], 0.0f, 0.5f);
    og.ck = t60k(5.0f);
}

void chordv_trig(uint32_t ch, int32_t chord, int32_t voicing, uint32_t vel)
{
    int32_t notes[3];
    uint32_t i;
    float v = (float)(vel > 127u ? 127u : vel) * (1.0f / 127.0f);
    if (ch == CH_SKANK) {
        chord_notes(chord, voicing, 60, notes);  /* mid: from C4 */
        for (i = 0; i < NV; i++) {
            sk.v[i].f = midi_hz(notes[i]);
            sk.v[i].ienv = 1.0f;
            sk.v[i].amp = 1.0f;
            sk.v[i].pc = sk.v[i].pm = 0.0f;
        }
        sk.vel = v;
        sk.on = 1;
    } else if (ch == CH_ORGAN) {
        chord_notes(chord, voicing, 55, notes);  /* mid: from G3 */
        for (i = 0; i < NV; i++) {
            og.v[i].f = midi_hz(notes[i]);
            og.v[i].amp = 1.0f;
        }
        og.cenv = 1.0f;
        og.vel = v;
        og.on = 1;
    }
}

static int render_skank(float *o, uint32_t n)
{
    uint32_t i, k;
    if (!sk.on)
        return 0;
    for (i = 0; i < n; i++) {
        float s = 0.0f, y;
        for (k = 0; k < NV; k++) {
            cv_t *v = &sk.v[k];
            v->pm = wrap(v->pm + v->f * sk.ratio * INV_SR);
            v->pc = wrap(v->pc + v->f * INV_SR);
            s += fm_sin_turns(v->pc + sk.index * v->ienv * 0.15915494f * fm_sin_turns(v->pm)) * v->amp;
            v->ienv *= sk.ik;
            v->amp *= sk.ak;
        }
        y = sk.hp_a * (sk.hy + s - sk.hx);       /* the band: a high-pass, then a low-pass */
        sk.hx = s;
        sk.hy = undenorm(y);
        sk.ly = undenorm(sk.ly + sk.lp_a * (y - sk.ly));
        o[i] = sk.ly * sk.vel * 0.22f;
    }
    for (k = 0; k < NV; k++)
        sk.v[k].ienv = undenorm(sk.v[k].ienv);
    if (sk.v[0].amp < 1e-4f)
        sk.on = 0;
    return 1;
}

static int render_organ(float *o, uint32_t n)
{
    uint32_t i, k;
    if (!og.on)
        return 0;
    for (i = 0; i < n; i++) {
        float s = 0.0f;
        for (k = 0; k < NV; k++) {
            float f = og.v[k].f * INV_SR;
            og.v[k].pc = wrap(og.v[k].pc + f);
            og.p2[k] = wrap(og.p2[k] + 2.0f * f);
            og.p3[k] = wrap(og.p3[k] + 3.0f * f);
            og.p4[k] = wrap(og.p4[k] + 4.0f * f);
            og.pk[k] = wrap(og.pk[k] + 12.0f * f);   /* the key click: a high partial, ~5 ms */
            s += (fm_sin_turns(og.v[k].pc) + og.w2 * fm_sin_turns(og.p2[k]) + og.w3 * fm_sin_turns(og.p3[k])
                  + og.w4 * fm_sin_turns(og.p4[k]) + og.click * og.cenv * fm_sin_turns(og.pk[k])) * og.v[k].amp;
            og.v[k].amp *= og.ak;
        }
        og.cenv = undenorm(og.cenv * og.ck);
        o[i] = s * og.vel * 0.12f;
    }
    if (og.v[0].amp < 1e-4f)
        og.on = 0;
    return 1;
}

int chordv_render(uint32_t ch, float *o, uint32_t n)
{
    return ch == CH_SKANK ? render_skank(o, n) : ch == CH_ORGAN ? render_organ(o, n) : 0;
}
