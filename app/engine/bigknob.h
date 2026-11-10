/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* The Big Knob (roadmap P1.1; mastery guide C3): a stepped 3rd-order Butterworth high-pass, 18 dB/oct, ten steps
 * from 70 Hz to 7.5 kHz and off, as Tubby's Altec 9069B. A step switches at once, unsmoothed: the detents' "clicky"
 * quality is the sound (C3). A one-pole and a two-pole (Q 1) state-variable high-pass, topology-preserving
 * (Zavalishin's TPT form), which stays stable when its coefficients jump. Mono (the dry path is). */
#pragma once
#include <stdint.h>

typedef struct {
    float g1, g, k, d;                           /* the step's coefficients (g1: the one-pole's G) */
    float s1, ic1, ic2;                          /* state */
    int on;
    int lp;                                      /* 7.5: a low-pass (the 9068B) */
} bigknob_t;

void bigknob_init(bigknob_t *b);
void bigknob_step(bigknob_t *b, float hz, int lp);   /* 0: off (bypass); lp: as a low-pass (7.5) */
void bigknob_run(bigknob_t *b, float *x, uint32_t n);   /* in place */
