/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* phaser.h. Compiled on its own at -O2 (fm1.toml o2). Floats only. */
#include "phaser.h"
#include "fastmath.h"
#include "dsp.h"

static phaser_cfg_t cfg = {0.5f, 0.7f, 0.3f, 0};
static phaser_side_t side[2];
static float lfo;

void phaser_config(const phaser_cfg_t *c) { cfg = *c; }

void phaser_reset(void)
{
    uint32_t s, k;
    for (s = 0; s < 2u; s++) {
        for (k = 0; k < 6u; k++)
            side[s].x[k] = side[s].y[k] = 0.0f;
        side[s].fbs = 0.0f;
    }
}

static void run_side(phaser_side_t *ps, float *x, uint32_t n, float a0, float a1)
{
    uint32_t i, k;
    for (i = 0; i < n; i++) {                    /* the coefficient glides a0 -> a1 over the call (no zipper) */
        float a = a0 + (a1 - a0) * (float)i / (float)n, v = x[i] + cfg.fb * ps->fbs;
        for (k = 0; k < 6u; k++) {               /* y = a v + x[n-1] - a y[n-1] */
            float y = a * v + ps->x[k] - a * ps->y[k];
            ps->x[k] = v;
            ps->y[k] = undenorm(y);
            v = y;
        }
        ps->fbs = v;
        x[i] = 0.5f * (x[i] + v);                /* half and half: the notches */
    }
}

static float coef(float l)                       /* the LFO (0..1) to an all-pass coefficient: 200 Hz .. 4 kHz */
{
    float f = 200.0f * fm_exp2f(4.32f * l), t = fm_tanf(3.14159265f * f / 44100.0f);
    return (t - 1.0f) / (t + 1.0f);
}

void phaser_run(float *l, float *r, uint32_t n)
{
    float s0, s1, a0, a1;
    s0 = cfg.square ? (lfo < 0.5f ? 1.0f : 0.0f) : 0.5f + 0.5f * fm_sin_turns(lfo);
    lfo += cfg.rate * (float)n / 44100.0f;
    if (lfo >= 1.0f)
        lfo -= 1.0f;
    s1 = cfg.square ? (lfo < 0.5f ? 1.0f : 0.0f) : 0.5f + 0.5f * fm_sin_turns(lfo);
    a0 = coef(1.0f - cfg.depth + cfg.depth * s0);
    a1 = coef(1.0f - cfg.depth + cfg.depth * s1);
    run_side(&side[0], l, n, a0, a1);
    if (r)
        run_side(&side[1], r, n, a0, a1);
}
