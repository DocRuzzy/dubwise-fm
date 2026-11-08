/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* tape.h. Compiled on its own at -O2 (fm1.toml o2). Floats only. */
#include "tape.h"
#include "fastmath.h"
#include "dsp.h"

static tape_cfg_t cfg = {1.0f, 0.0f};
static float lp[2];
#define TAPE_LP 0.82f                            /* the highs eased: a one-pole at ~12 kHz */

void tape_config(const tape_cfg_t *c) { cfg = *c; }
void tape_reset(void) { lp[0] = lp[1] = 0.0f; }

static void side_run(float *x, uint32_t n, float *lps)
{
    uint32_t i;
    float g = cfg.gain, back = 1.0f / g, mix = cfg.mix;   /* (quiet sounds at the same level: drive, not volume) */
    for (i = 0; i < n; i++) {
        float d = fm_tanhf(g * x[i]) * back;
        *lps = undenorm(*lps + TAPE_LP * (d - *lps));
        x[i] += mix * (*lps - x[i]);
    }
}

void tape_run(float *l, float *r, uint32_t n)
{
    side_run(l, n, &lp[0]);
    if (r)
        side_run(r, n, &lp[1]);
}
