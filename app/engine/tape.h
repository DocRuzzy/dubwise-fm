/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* Tape drive (roadmap P2.4; mastery guide C7): up to +14 dB into a soft saturation (tanh, scaled back by the same:
 * quiet sounds keep their level, loud ones flatten), the highs eased off as tape does, blended with the dry (parallel). On the bass, or on the whole
 * mix (both sides the same curve: no stereo added). */
#pragma once
#include <stdint.h>

typedef struct {
    float gain;                                  /* into the tape: 1 .. 5 (+14 dB) */
    float mix;                                   /* the driven part: 0 .. 1 */
} tape_cfg_t;

void tape_config(const tape_cfg_t *c);
void tape_run(float *l, float *r, uint32_t n);   /* in place; r may be 0 */
void tape_reset(void);
