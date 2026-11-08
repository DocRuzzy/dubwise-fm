/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* Small DSP helpers shared by the engine's units. */
#pragma once

/* A decaying state would sink into denormals, which some FPUs (the host's) take ~30x as long over (measured on the
 * echo: 61 us a block against 2). Adding and taking away a tiny constant rounds anything that small to exactly 0.
 * Needs -ffp-contract=off and no -ffast-math (the build's flags); volatile keeps the compiler from folding it. */
static inline float undenorm(float x)
{
    volatile float k = 1e-18f;
    return (x + k) - k;
}

/* a steep high-pass for a side (L - R) signal, so the low end stays mono: four one-pole high-passes in a row,
 * 24 dB/oct. a = exp(-2 pi fc / fs) */
typedef struct {
    float x[4], y[4];
} hp4_t;
static inline float hp4_run(hp4_t *h, float a, float in)
{
    int k;
    for (k = 0; k < 4; k++) {
        float y = a * (h->y[k] + in - h->x[k]);
        h->x[k] = in;
        h->y[k] = undenorm(y);
        in = y;
    }
    return in;
}
static inline void hp4_clear(hp4_t *h)
{
    int k;
    for (k = 0; k < 4; k++)
        h->x[k] = h->y[k] = 0.0f;
}
