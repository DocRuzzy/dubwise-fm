/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* The phaser (roadmap P2.3; mastery guide C6): six first-order all-passes swept by an LFO (0.1-18 Hz, sine or
 * square, like the Bi-Phase's), 200 Hz .. 4 kHz, with feedback, mixed half and half with the dry: the moving
 * notches. An insert on the hats, the skank, the echo's return or the whole mix (both sides through the same
 * sweep: no stereo added, the low end stays mono). Off from the Four-track tier down (design principles 5). */
#pragma once
#include <stdint.h>

typedef struct {
    float rate, depth, fb;                       /* Hz; 0..1 of the sweep; 0..0.7 */
    int square;
    int mode;                                    /* PHM_* (P3.2: the dual Bi-Phase) */
    float rate2;                                 /* phaser B's, Hz (cascade) */
} phaser_cfg_t;

typedef struct {
    float x[6], y[6], fbs;                       /* a side's state */
} phaser_side_t;

void phaser_config(const phaser_cfg_t *c);
/* n samples in place; two sides (r may be 0: one side). The LFO moves once a call. */
void phaser_run(float *l, float *r, uint32_t n);
/* P3.2, the dual Bi-Phase: a second six-stage phaser (B). CASCADE: A then B, B at its own rate (twelve stages, two
 * sweeps crossing); STEREO (the Bi-Phase's reverse sync): the left through A, the right through B on A's sweep
 * reversed. STEREO needs two sides (the echo's return); given one, or on the mix (the dry path stays mono), the
 * engine asks for CASCADE. */
void phaser_reset(void);
