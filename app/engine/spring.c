/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* spring.h. Compiled on its own at -O2 (fm1.toml o2). Floats only. The dispersion after the stretched all-pass
 * cascades of Parker and Bilbao's spring models (an idea, no code taken). */
#include "spring.h"
#include "fastmath.h"
#include "plat.h"                                /* PLAT_POOL */
#include "dsp.h"

#define NSPR 2
#define NAPS 12                                  /* all-passes a spring */
#define APK 2                                    /* their stretch: z^-2 */
#define AP_A 0.62f                               /* their coefficient: the chirp's strength */
static const uint16_t LEN[NSPR] = {816, 948};    /* 37 and 43 ms at 22,050 Hz */
#define LINE_MAX 948

static PLAT_POOL float line[NSPR][LINE_MAX];
static PLAT_POOL float ap_x[NSPR][NAPS][APK], ap_y[NSPR][NAPS][APK];
static uint16_t pos[NSPR], ap_i;
static float lp[NSPR], fb[NSPR];
static spring_cfg_t cfg = {0.8f, 0.5f};
static float hp_x, hp_y;                         /* the input's high-pass (~200 Hz, at 22 kHz) */
static float dec, up_prev, up_cur;               /* half rate: the decimator's state, the interpolator's two */
static uint32_t phase;
static float crash_env, crash_amp;               /* the strike: its burst */
static uint32_t rng = 0x9E3779B9u;

static inline float noise(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return (float)(int32_t)rng * (1.0f / 2147483648.0f);
}

void spring_init(void)
{
    uint32_t s, i, k;
    for (s = 0; s < NSPR; s++) {
        for (i = 0; i < LINE_MAX; i++)
            line[s][i] = 0.0f;
        for (i = 0; i < NAPS; i++)
            for (k = 0; k < APK; k++)
                ap_x[s][i][k] = ap_y[s][i][k] = 0.0f;
        pos[s] = 0;
        lp[s] = fb[s] = 0.0f;
    }
    ap_i = 0;
    hp_x = hp_y = dec = up_prev = up_cur = 0.0f;
    phase = 0;
    crash_env = crash_amp = 0.0f;
}

void spring_config(const spring_cfg_t *c) { cfg = *c; }

void spring_crash(float amount)
{
    crash_amp = amount;
    crash_env = 1.0f;
}

static float half_step(float x)                  /* one 22,050 Hz sample: in, the return out */
{
    uint32_t s, i;
    float out = 0.0f, y;
    /* the input: high-passed, plus a strike's burst (noise and a thump, ~6 ms) */
    y = 0.945f * (hp_y + x - hp_x);
    hp_x = x;
    hp_y = undenorm(y);
    if (crash_env > 1e-4f) {
        y += crash_amp * crash_env * (noise() + 0.6f);
        crash_env *= 0.9925f;
    }
    for (s = 0; s < NSPR; s++) {
        float v = y + cfg.g * fb[s];             /* into the spring: the input, and its own last pass */
        for (i = 0; i < NAPS; i++) {             /* the stretched all-passes: y = a x + x[n-K] - a y[n-K] */
            float xo = ap_x[s][i][ap_i], yo = ap_y[s][i][ap_i], w = AP_A * v + xo - AP_A * yo;
            ap_x[s][i][ap_i] = v;
            ap_y[s][i][ap_i] = undenorm(w);
            v = w;
        }
        line[s][pos[s]] = v;
        if (++pos[s] == LEN[s])
            pos[s] = 0;
        lp[s] = undenorm(lp[s] + cfg.damp * (line[s][pos[s]] - lp[s]));   /* the oldest, damped */
        fb[s] = lp[s];
        out += lp[s];
    }
    ap_i = (uint16_t)((ap_i + 1u) % APK);
    return out * 0.5f;
}

void spring_run(const float *in, float *out, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++) {
        dec = undenorm(dec + 0.6f * (in[i] - dec));   /* a gentle low-pass, then every other sample */
        if (++phase == 2u) {
            phase = 0;
            up_prev = up_cur;
            up_cur = half_step(dec);
        }
        out[i] = up_prev + (up_cur - up_prev) * (phase ? 0.5f : 0.0f);
    }
}
