/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* The spring reverb and its crash (roadmap P1.2; mastery guide C5). Two springs at half rate (22,050 Hz), each a
 * delay loop (37 and 43 ms) through twelve stretched all-passes (each pass round, the spring's dispersive chirp
 * grows: its "drip") and a ~4.5 kHz damping low-pass, decaying in DECAY. The input is high-passed at ~200 Hz (a
 * tank carries little bass; the low end stays the dry path's). The crash strikes both springs at once: a 6 ms
 * noise burst and a ringing ~110 Hz thump (no DC), the "thunderclap" of a kicked tank. Stereo since P1.5: a
 * spring each side. Down to and up from half rate through a 39-tap half-band FIR (spring.c). In the pool: ~8 KB. */
#pragma once
#include <stdint.h>

typedef struct {
    float g;                                     /* the loops' feedback (from DECAY) */
    float damp;                                  /* their low-pass coefficient (from TONE) */
    int naps;                                    /* all-passes a spring: 12; 6 at the Four-track tier and down */
    int mono;                                    /* 1: the two springs summed (the Four-track tier and down) */
} spring_cfg_t;

void spring_init(void);
void spring_config(const spring_cfg_t *c);
void spring_crash(float amount);                 /* strike the tank (0..1) */
/* 44.1 kHz in, the return out: the two springs left and right around their middle, the difference above ~120 Hz
 * only (the low end mono) */
void spring_run(const float *in, float *outl, float *outr, uint32_t n);
