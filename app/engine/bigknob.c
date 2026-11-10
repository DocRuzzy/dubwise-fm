/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* bigknob.h. Compiled on its own at -O2 (fm1.toml o2). Floats only. */
#include "bigknob.h"
#include "fastmath.h"
#include "dsp.h"

void bigknob_init(bigknob_t *b)
{
    b->s1 = b->ic1 = b->ic2 = 0.0f;
    b->on = 0;
    b->lp = 0;
    b->g1 = b->g = b->k = b->d = 0.0f;
}

void bigknob_step(bigknob_t *b, float hz, int lp)
{
    float g;
    if (hz <= 0.0f) {
        b->on = 0;
        return;
    }
    g = fm_tanf(3.14159265f * hz / 44100.0f);    /* the bilinear transform's prewarped cutoff */
    b->g1 = g / (1.0f + g);                      /* the one-pole */
    b->g = g;
    b->k = 1.0f;                                 /* 1 / Q: Q 1 with the one-pole is a 3rd-order Butterworth */
    b->d = 1.0f / (1.0f + g * (g + b->k));
    if (!b->on || b->lp != lp) {                 /* engaged from off, or the other way round: start from rest */
        b->s1 = b->ic1 = b->ic2 = 0.0f;
    }
    b->lp = lp;
    b->on = 1;
}

void bigknob_run(bigknob_t *b, float *x, uint32_t n)
{
    uint32_t i;
    float g1 = b->g1, g = b->g, k = b->k, d = b->d, s1 = b->s1, ic1 = b->ic1, ic2 = b->ic2;
    if (!b->on)
        return;
    if (b->lp) {                                 /* 7.5: the low-pass: the one-pole's low output into the two-pole's */
        for (i = 0; i < n; i++) {
            float v = (x[i] - s1) * g1, lp = v + s1, hp, v1, v2;
            s1 = lp + v;
            hp = (lp - (g + k) * ic1 - ic2) * d;
            v1 = g * hp + ic1;
            v2 = g * v1 + ic2;
            ic1 = g * hp + v1;
            ic2 = g * v1 + v2;
            x[i] = v2;
        }
        b->s1 = undenorm(s1);
        b->ic1 = undenorm(ic1);
        b->ic2 = undenorm(ic2);
        return;
    }
    for (i = 0; i < n; i++) {
        /* one-pole high-pass: v = (x - s) G; lp = v + s; s = lp + v; hp = x - lp */
        float v = (x[i] - s1) * g1, lp = v + s1, h1, hp, v1, v2;
        s1 = lp + v;
        h1 = x[i] - lp;
        /* the two-pole state-variable filter's high-pass output */
        hp = (h1 - (g + k) * ic1 - ic2) * d;
        v1 = g * hp + ic1;
        v2 = g * v1 + ic2;
        ic1 = g * hp + v1;
        ic2 = g * v1 + v2;
        x[i] = hp;
    }
    b->s1 = undenorm(s1);
    b->ic1 = undenorm(ic1);
    b->ic2 = undenorm(ic2);
}
