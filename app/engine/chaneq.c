/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* chaneq.h. Compiled on its own at -O2 (fm1.toml o2). Floats only. The peak is RBJ's Audio EQ Cookbook peaking EQ
 * (public formulas); the shelves are a one-pole split (the low band, or the high band, scaled and added back). */
#include "chaneq.h"
#include "fastmath.h"
#include "dsp.h"
#include "params.h"                              /* hpf_hz, lpf_hz */

#define LO_HZ 100.0f
#define HI_HZ 5000.0f
#define MID_Q 1.4f
#define GLIDE 0.35f                              /* the mid's frequency: this much of the way a block (~15 ms) */

float chaneq_db(int32_t v) { return (float)v * (15.0f / 64.0f); }
float chaneq_mid_hz(int32_t v) { return 1265.0f * fm_exp2f((float)v * (5.3219f / 127.0f)); }   /* 1.26 kHz x 40^(v/127) */

void chaneq_init(chaneq_t *q)
{
    q->lo_z = q->hi_z = q->z1 = q->z2 = 0.0f;
    q->lo_on = q->hi_on = q->mid_on = 0;
    q->lo_g = q->hi_g = 0.0f;
    q->f_now = 0.0f;
    q->mid_db = q->mid_f = q->filt = 0;
    bigknob_init(&q->filt_b);
}

static void mid_coefs(chaneq_t *q, float hz, float db, float sr)
{
    float A = fm_exp2f(db * (3.321928f / 40.0f));   /* 10^(dB/40) */
    float w = 6.2831853f * hz / sr, c = fm_cosf(w), al = fm_sinf(w) / (2.0f * MID_Q);
    float a0 = 1.0f / (1.0f + al / A);
    q->b0 = (1.0f + al * A) * a0;
    q->b1 = -2.0f * c * a0;
    q->b2 = (1.0f - al * A) * a0;
    q->a1 = q->b1;
    q->a2 = (1.0f - al / A) * a0;
}

void chaneq_config(chaneq_t *q, int32_t lo, int32_t mid, int32_t mid_f, int32_t hi, int32_t filt, float sr)
{
    float target;
    /* a shelf: x + (A - 1) x the band. A boost's corner is the shelf's; a cut's corner moves (the low's up by 1/A, the
     * high's down by A), so that a cut is the boost's exact inverse: as deep as the boost is tall */
    if (lo) {
        float A = fm_exp2f(chaneq_db(lo) * (3.321928f / 20.0f)), hz = A < 1.0f ? LO_HZ / A : LO_HZ;
        if (!q->lo_on)
            q->lo_z = 0.0f;
        q->lo_k = 1.0f - fm_expf(-6.2831853f * hz / sr);
        q->lo_g = A - 1.0f;
    }
    q->lo_on = lo != 0;
    if (hi) {
        float A = fm_exp2f(chaneq_db(hi) * (3.321928f / 20.0f)), hz = A < 1.0f ? HI_HZ * A : HI_HZ;
        if (!q->hi_on)
            q->hi_z = 0.0f;
        q->hi_k = 1.0f - fm_expf(-6.2831853f * hz / sr);
        q->hi_g = A - 1.0f;
    }
    q->hi_on = hi != 0;
    target = chaneq_mid_hz(mid_f);
    if (mid) {
        if (!q->mid_on || q->f_now <= 0.0f) {    /* in from flat: at its frequency at once, from rest */
            q->f_now = target;
            q->z1 = q->z2 = 0.0f;
        } else
            q->f_now += GLIDE * (target - q->f_now);
        if (q->f_now > 0.45f * sr)
            q->f_now = 0.45f * sr;
        mid_coefs(q, q->f_now, chaneq_db(mid), sr);
    } else
        q->f_now = target;
    q->mid_on = mid != 0;
    q->mid_db = (int8_t)mid;
    q->mid_f = (int8_t)mid_f;
    if (filt != q->filt) {                       /* a step: at once, unsmoothed, as the Big Knob */
        float hz = filt > 0 ? hpf_hz(filt) : filt < 0 ? lpf_hz(-filt) : 0.0f;
        bigknob_step(&q->filt_b, hz, filt < 0);
        q->filt = (int8_t)filt;
    }
}

int chaneq_active(const chaneq_t *q) { return q->lo_on | q->hi_on | q->mid_on | q->filt_b.on; }

void chaneq_run(chaneq_t *q, float *x, uint32_t n)
{
    uint32_t i;
    if (q->lo_on) {
        float k = q->lo_k, g = q->lo_g, z = q->lo_z;
        for (i = 0; i < n; i++) {
            z += k * (x[i] - z);
            x[i] += g * z;
        }
        q->lo_z = undenorm(z);
    }
    if (q->mid_on) {
        float b0 = q->b0, b1 = q->b1, b2 = q->b2, a1 = q->a1, a2 = q->a2, z1 = q->z1, z2 = q->z2;
        for (i = 0; i < n; i++) {
            float in = x[i], y = b0 * in + z1;
            z1 = b1 * in - a1 * y + z2;
            z2 = b2 * in - a2 * y;
            x[i] = y;
        }
        q->z1 = undenorm(z1);
        q->z2 = undenorm(z2);
    }
    if (q->hi_on) {
        float k = q->hi_k, g = q->hi_g, z = q->hi_z;
        for (i = 0; i < n; i++) {
            z += k * (x[i] - z);
            x[i] += g * (x[i] - z);
        }
        q->hi_z = undenorm(z);
    }
    bigknob_run(&q->filt_b, x, n);
}
