/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* phaser.h. Compiled on its own at -O2 (fm1.toml o2). Floats only. */
#include "phaser.h"
#include "params.h"                              /* PHM_* */
#include "fastmath.h"
#include "dsp.h"

static phaser_cfg_t cfg = {0.5f, 0.7f, 0.3f, 0, 0, 0.3f};
static phaser_side_t side[2], side_b[2];         /* A's two sides; B's (P3.2) */
static float lfo, lfo2;

void phaser_config(const phaser_cfg_t *c) { cfg = *c; }

void phaser_reset(void)
{
    uint32_t s, k;
    for (s = 0; s < 2u; s++) {
        for (k = 0; k < 6u; k++)
            side[s].x[k] = side[s].y[k] = side_b[s].x[k] = side_b[s].y[k] = 0.0f;
        side[s].fbs = side_b[s].fbs = 0.0f;
    }
}

static void run_side(phaser_side_t *ps, float *x, uint32_t n, float a0, float a1)
{
    uint32_t i, k;
    float a = a0, da = (a1 - a0) / (float)n, fb = cfg.fb, fbs = ps->fbs;   /* (one divide a call, not a sample) */
    float x0 = ps->x[0], x1 = ps->x[1], x2 = ps->x[2], x3 = ps->x[3], x4 = ps->x[4], x5 = ps->x[5];
    float y0 = ps->y[0], y1 = ps->y[1], y2 = ps->y[2], y3 = ps->y[3], y4 = ps->y[4], y5 = ps->y[5];
    (void)k;
    for (i = 0; i < n; i++, a += da) {           /* the coefficient glides a0 -> a1 over the call (no zipper) */
        float v = x[i] + fb * fbs, y;            /* each stage: y = a (v - y[n-1]) + x[n-1] (registers, not memory) */
        y = a * (v - y0) + x0; x0 = v; y0 = y; v = y;
        y = a * (v - y1) + x1; x1 = v; y1 = y; v = y;
        y = a * (v - y2) + x2; x2 = v; y2 = y; v = y;
        y = a * (v - y3) + x3; x3 = v; y3 = y; v = y;
        y = a * (v - y4) + x4; x4 = v; y4 = y; v = y;
        y = a * (v - y5) + x5; x5 = v; y5 = y; v = y;
        fbs = v;
        x[i] = 0.5f * (x[i] + v);                /* half and half: the notches */
    }
    ps->x[0] = x0, ps->x[1] = x1, ps->x[2] = x2, ps->x[3] = x3, ps->x[4] = x4, ps->x[5] = x5;
    ps->y[0] = undenorm(y0), ps->y[1] = undenorm(y1), ps->y[2] = undenorm(y2);   /* (denormals: once a call) */
    ps->y[3] = undenorm(y3), ps->y[4] = undenorm(y4), ps->y[5] = undenorm(y5);
    ps->fbs = undenorm(fbs);
}

static float coef(float l)                       /* the LFO (0..1) to an all-pass coefficient: 200 Hz .. 4 kHz */
{
    float f = 200.0f * fm_exp2f(4.32f * l), t = fm_tanf(3.14159265f * f / 44100.0f);
    return (t - 1.0f) / (t + 1.0f);
}

static float shape(float ph) { return cfg.square ? (ph < 0.5f ? 1.0f : 0.0f) : 0.5f + 0.5f * fm_sin_turns(ph); }

static float step(float *ph, float rate, uint32_t n, float *s0)   /* the LFO's value now and after n samples */
{
    *s0 = shape(*ph);
    *ph += rate * (float)n / 44100.0f;
    if (*ph >= 1.0f)
        *ph -= 1.0f;
    return shape(*ph);
}

static float sweep(float s) { return coef(1.0f - cfg.depth + cfg.depth * s); }

void phaser_run(float *l, float *r, uint32_t n)
{
    float s0, s1 = step(&lfo, cfg.rate, n, &s0), a0 = sweep(s0), a1 = sweep(s1);
    if (cfg.mode == PHM_STEREO && r) {           /* reverse sync: the right on the sweep turned over */
        run_side(&side[0], l, n, a0, a1);
        run_side(&side_b[1], r, n, sweep(1.0f - s0), sweep(1.0f - s1));
        return;
    }
    run_side(&side[0], l, n, a0, a1);
    if (r)
        run_side(&side[1], r, n, a0, a1);
    if (cfg.mode != PHM_SINGLE) {                /* cascade: then through B, at its own rate */
        float t0, t1 = step(&lfo2, cfg.rate2, n, &t0), b0 = sweep(t0), b1 = sweep(t1);
        run_side(&side_b[0], l, n, b0, b1);
        if (r)
            run_side(&side_b[1], r, n, b0, b1);
    }
}
