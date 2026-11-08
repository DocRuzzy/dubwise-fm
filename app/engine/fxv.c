/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* fxv.h. Compiled on its own at -O2 (fm1.toml o2). Floats only. */
#include "fxv.h"
#include "params.h"
#include "fastmath.h"
#include "dsp.h"

#define INV_SR (1.0f / 44100.0f)
static inline float lin(int16_t v, float lo, float hi) { return lo + (hi - lo) * (float)v * (1.0f / 127.0f); }

static struct {
    int gate, which;
    float ph, lfo, amp, vel;
    float f, rate, ratio, tri;
} sr;

void fxv_init(void)
{
    sr.gate = 0;
    sr.amp = 0.0f;
}

void fxv_config(const int16_t *ev)
{
    sr.f = 200.0f * fm_exp2f(lin(ev[P_VOICE(CH_VOX, VX_PITCH)], 0.0f, 2.585f));   /* 200 Hz .. 1.2 kHz */
    sr.rate = 0.5f * fm_exp2f(lin(ev[P_VOICE(CH_VOX, VX_RATE)], 0.0f, 4.585f));   /* 0.5 .. 12 Hz */
    sr.ratio = fm_exp2f(lin(ev[P_VOICE(CH_VOX, VX_DEPTH)], 1.0f, 24.0f) * (1.0f / 12.0f));
    sr.tri = ev[P_VOICE(CH_VOX, VX_WAVE)] ? 1.0f : 0.0f;
}

void fxv_on(int32_t which, uint32_t vel)
{
    sr.which = which;
    sr.vel = (float)(vel > 127u ? 127u : vel) * (1.0f / 127.0f);
    if (!sr.gate)
        sr.lfo = 0.0f;
    sr.gate = 1;
}

void fxv_off(int32_t which)
{
    if (sr.which == which)
        sr.gate = 0;
}

int fxv_render(float *o, uint32_t n)
{
    uint32_t i;
    if (!sr.gate && sr.amp < 1e-5f) {
        sr.amp = 0.0f;
        return 0;
    }
    for (i = 0; i < n; i++) {
        float m, f, s;
        sr.lfo += sr.rate * INV_SR;
        if (sr.lfo >= 1.0f)
            sr.lfo -= 1.0f;
        /* A: two tones, the square's halves; B: a ramp up through DEPTH, again and again */
        m = sr.which == SIREN_A ? (sr.lfo < 0.5f ? 1.0f : sr.ratio) : fm_exp2f(sr.lfo * fm_log2f(sr.ratio));
        f = sr.f * m;
        sr.ph += f * INV_SR;
        if (sr.ph >= 1.0f)
            sr.ph -= 1.0f;
        s = sr.tri > 0.5f ? 4.0f * fm_fabsf(sr.ph - 0.5f) - 1.0f : fm_sin_turns(sr.ph);
        sr.amp += ((sr.gate ? 1.0f : 0.0f) - sr.amp) * (sr.gate ? 0.005f : 0.0008f);   /* ~5 ms in, ~30 ms out */
        sr.amp = undenorm(sr.amp);
        o[i] = s * sr.amp * sr.vel * 0.35f;
    }
    return 1;
}
