/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors
 *
 * Adapted from setBfree-fm1's Leslie (firmware/src/organ_fx.h, "B3 ORGAN" for the FM-1 by R-Jeb,
 * https://github.com/R-Jeb/setBfree-fm1, GPL-2.0-or-later), itself after setBfree's b_whirl/whirl.c
 * (https://github.com/pantherb/setBfree, Fredrik Kilander, Will Panther, Robin Gareus, Ken Restivo and others,
 * GPL-2.0-or-later). Rewritten in floats for Dubwise FM; this file is GPL-3.0-only, as the firmware. */
/* The B-3's FULL Leslie (v0.3; leslie.h). Horn and drum turn; each sound reaches the left and the right microphone on
 * three paths (direct, a first and a second reflection), delayed by the rotor's distance to that microphone (the
 * Doppler shift), and the horn's also filtered by its angle (it's a beam: the level swing). The horn gets the part
 * above ~300 Hz through two filters, the drum all of it, low-passed at the microphones. As setBfree, except: the
 * drum is mono (the paths of both microphones summed, half each: the low end stays mono, the Width rule), it runs at
 * the organ's half rate (22,050 Hz: tools/organ/mkleslie.py moves the tables there; the horn's filter 3 taps), the
 * tables at 512 angles (every other row), the second reflections left out unless LES_PATHS is 3 (setBfree-fm1: about
 * 30 % of the work), and floats. Compiled on its own at -O2. */
#include "leslie.h"
#include "leslie_data.h"
#include "fastmath.h"
#include "dsp.h"
#include "plat.h"                                /* PLAT_POOL */

#ifndef LES_PATHS
#define LES_PATHS 2u                             /* direct and first reflection (3: the second too, as setBfree) */
#endif
#define LS_N 256u                                /* the lines to the microphones: the longest path is ~180 samples */
#define LS_M (LS_N - 1u)
#define FS 22050.0f

static PLAT_POOL uint16_t ls_disp[4][LES_ANG];   /* (from the flash at boot: the render never reads it) */
static PLAT_POOL int16_t ls_beam[2][LES_ANG * 3u];
static PLAT_POOL float ls_hl[LS_N], ls_hr[LS_N], ls_dm[LS_N];   /* horn left, horn right, the drum (mono) */
static uint32_t ls_pos, ls_hph, ls_dph, ls_hinc, ls_dinc;   /* the output's place; the rotors' angles (of 2^32),
                                                  * their steps a sample */
static float ls_hf, ls_df;                       /* their speeds, Hz */
static float ls_h[3][2];                         /* the three horn signals' last two samples */
static float ls_z[4];                            /* the reflections' filters (0.4 x + 0.4 the last x) */
typedef struct {
    float x1, x2, y1, y2;
} bq_t;
static bq_t ls_ha, ls_hb, ls_df_q;

static float bq_run(bq_t *s, const float *c, float x)   /* c: b0 b1 b2 a1 a2 */
{
    float y = c[0] * x + c[1] * s->x1 + c[2] * s->x2 - c[3] * s->y1 - c[4] * s->y2;
    s->x2 = s->x1, s->x1 = x;
    s->y2 = s->y1, s->y1 = undenorm(y);
    return s->y1;
}

void les_init(void)
{
    uint32_t i, k;
    for (k = 0; k < 4u; k++)
        for (i = 0; i < LES_ANG; i++)
            ls_disp[k][i] = LES_DISP[k][i];
    for (k = 0; k < 2u; k++)
        for (i = 0; i < LES_ANG * 3u; i++)
            ls_beam[k][i] = LES_BEAM[k][i];
    les_reset();
}

void les_reset(void)
{
    uint32_t i;
    for (i = 0; i < LS_N; i++)
        ls_hl[i] = ls_hr[i] = ls_dm[i] = 0.0f;
    for (i = 0; i < 6u; i++)
        ls_h[i / 2u][i % 2u] = 0.0f;
    for (i = 0; i < 4u; i++)
        ls_z[i] = 0.0f;
    ls_ha = ls_hb = ls_df_q = (bq_t){0.0f, 0.0f, 0.0f, 0.0f};
}

float les_rotor(int drum) { return drum ? ls_df : ls_hf; }

static inline void ls_put(float *buf, float x, float t)   /* x lands t samples from now (split between two) */
{
    uint32_t r = ls_pos + (uint32_t)t;
    float q = x * (t - (float)(uint32_t)t);
    buf[r & LS_M] += x - q;
    buf[(r + 1u) & LS_M] += q;
}

static inline float ls_delay(const uint16_t *d, uint32_t ph)   /* the delay at this angle, interpolated, samples */
{
    uint32_t ix = ph >> 23, fr = (ph >> 7) & 0xFFFFu;          /* (512 angles: the top 9 bits) */
    float d0 = (float)d[ix], d1 = (float)d[(ix + 1u) & (LES_ANG - 1u)];
    return (d0 + (d1 - d0) * (float)fr * (1.0f / 65536.0f)) * (1.0f / 16.0f);
}

/* one horn path: delayed by the angle, filtered by it (the beam: the last three samples) */
static inline void ls_horn(float *buf, uint32_t ph, float sp, const uint16_t *disp, const int16_t *bw, float x,
                           const float *h)
{
    const int16_t *b = bw + 3u * (((ph + (1u << 22)) >> 23) & (LES_ANG - 1u));
    float xa = ((float)b[0] * x + (float)b[1] * h[0] + (float)b[2] * h[1]) * (1.0f / 32768.0f);
    ls_put(buf, xa, sp + ls_delay(disp, ph));
}

static inline void ls_hist(float *h, float x) { h[1] = h[0], h[0] = x; }

static inline float ls_refl(uint32_t k, float x)   /* the reflections' filter: 0.4 x + 0.4 the last x */
{
    float y = 0.4f * (x + ls_z[k]);
    ls_z[k] = x;
    return y;
}

static float ls_glide(float f, float target, float up, float down, uint32_t n)   /* a block of the rotor's pace */
{
    float tc = target > f ? up : down;
    return target + (f - target) * fm_expf(-(float)n / (FS * tc));
}

void les_block(uint32_t n, int speed)
{
    ls_hf = ls_glide(ls_hf, speed == 2 ? 7.056f : speed == 1 ? 0.672f : 0.0f, 0.161f, 0.321f, n);   /* setBfree's */
    ls_df = ls_glide(ls_df, speed == 2 ? 5.955f : speed == 1 ? 0.600f : 0.0f, 4.127f, 1.371f, n);
    ls_hinc = (uint32_t)(ls_hf * (4294967296.0f / FS));
    ls_dinc = (uint32_t)(ls_df * (4294967296.0f / FS));
}

void les_spin(uint32_t n)
{
    ls_hph += ls_hinc * n;
    ls_dph += ls_dinc * n;
}

void les_tick(float x, float *l, float *r)
{
    static const float SP = 1.0f / 16.0f;
    float xh, xd, leak, y;
    uint32_t o = ls_pos, p;
    xh = bq_run(&ls_hb, LES_HB, bq_run(&ls_ha, LES_HA, x));   /* the horn: above ~300 Hz */
    leak = xh * LES_LEAK;
    xd = x * 0.5f;
    for (p = 0; p < LES_PATHS; p++) {            /* direct, first (and second) reflection: each to both microphones */
        const uint32_t a = p * 2u;
        const int fwd = p != 1u;                 /* (setBfree: the first reflection reaches each the other way) */
        ls_horn(ls_hl, ls_hph + (uint32_t)LES_HPH[a] * (1u << 23), (float)LES_HSP[a] * SP, ls_disp[fwd ? 0 : 1],
                ls_beam[fwd ? 1 : 0], xh, ls_h[p]);
        ls_horn(ls_hr, ls_hph + (uint32_t)LES_HPH[a + 1u] * (1u << 23), (float)LES_HSP[a + 1u] * SP,
                ls_disp[fwd ? 1 : 0], ls_beam[fwd ? 0 : 1], xh, ls_h[p]);
        ls_hist(ls_h[p], xh);
        /* the drum: all of it, delayed; both microphones' paths into one line (mono) */
        ls_put(ls_dm, xd, (float)LES_DSP[a] * SP + ls_delay(ls_disp[fwd ? 2 : 3], ls_dph + (uint32_t)LES_DPH[a] * (1u << 23)));
        ls_put(ls_dm, xd, (float)LES_DSP[a + 1u] * SP
                              + ls_delay(ls_disp[fwd ? 3 : 2], ls_dph + (uint32_t)LES_DPH[a + 1u] * (1u << 23)));
        if (p + 1u < LES_PATHS) {
            xh = ls_refl(p, xh);
            xd = ls_refl(2u + p, xd);
        }
    }
    y = bq_run(&ls_df_q, LES_DF, ls_dm[o]);      /* what reaches the microphones now */
    *l = y + ls_hl[o] * LES_HORN_LEVEL + leak;
    *r = y + ls_hr[o] * LES_HORN_LEVEL + leak;
    ls_hl[o] = ls_hr[o] = ls_dm[o] = 0.0f;
    ls_pos = (o + 1u) & LS_M;
    ls_hph += ls_hinc;
    ls_dph += ls_dinc;
}
