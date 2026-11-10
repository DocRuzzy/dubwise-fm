/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* A channel's own EQ and stepped filter (roadmap 8.1; docs/plan-8.1-channel-eq.md; mastery guide C3 (c), C12): a low
 * shelf (100 Hz) and a high shelf (5 kHz), one-pole and blunt as the period's desks were; a peaking mid (Q 1.4) whose
 * frequency is swept smoothly (Scientist's sweeps); and the Big Knob's steps on this channel alone, above 0 the
 * high-pass (the 9069B), below 0 the low-pass (the 9068B), unsmoothed. Every stage flat at 0 is skipped (bit-exact).
 * Mono, in place, before the strip's fader and sends. */
#pragma once
#include <stdint.h>
#include "bigknob.h"

typedef struct {
    float lo_k, lo_g, lo_z;                      /* low shelf: x + lo_g * LP(x) (the one-pole's k, its state) */
    float hi_k, hi_g, hi_z;                      /* high shelf: x + hi_g * HP(x) */
    float b0, b1, b2, a1, a2, z1, z2;            /* the mid's peak (transposed direct form II) */
    float f_now;                                 /* the mid's frequency as it glides (Hz; 0: not yet set) */
    int lo_on, hi_on, mid_on;
    int8_t mid_db, mid_f, filt;                  /* the settings the coefficients are for */
    bigknob_t filt_b;                            /* the channel's stepped filter */
} chaneq_t;

void chaneq_init(chaneq_t *q);
/* the settings (the channel's fields, -64..63, the filter -40..40), once a block; sr the rate it runs at */
void chaneq_config(chaneq_t *q, int32_t lo, int32_t mid, int32_t mid_f, int32_t hi, int32_t filt, float sr);
int chaneq_active(const chaneq_t *q);           /* 1: some stage is in */
void chaneq_run(chaneq_t *q, float *x, uint32_t n);
float chaneq_db(int32_t v);                     /* a gain field's dB: v x 15 / 64 */
float chaneq_mid_hz(int32_t v);                 /* the MID's frequency: 1.26 kHz x 40^(v / 127), 200 Hz .. 7.9 kHz */
