/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* The dub tape echo (roadmap P0.3; technique C4, routing B5). One int16 line at a quarter of the rate (11,025 Hz),
 * long enough for two heads at T and 2T (T up to ECHO_T_MAX), so ping-pong (P3.1) is a routing change. The loop
 * from head T: a high-pass at ~80 Hz (no DC, no bass build-up), a tone low-pass, five short all-passes (each repeat a little
 * more diffuse; their delay taken off the loop's read, so repeats stay on time), then tape-like saturation; the outputs read the line raw, so the first repeat is clean and each one
 * after it darker. Feedback from T60, capped; PUSH runs it past unity (1.15, capped; the saturation and the
 * return limiter keep it bounded); FREEZE closes the input and loops the tape as it is, unfiltered. The time slews (~180 ms): changing it
 * bends the pitch, like tape. The return goes through a peak limiter. Ideas after DUB-1's amb.c as described in
 * docs/prior-art.md (no code taken). */
#pragma once
#include <stdint.h>

#define ECHO_RATE 11025.0f
#define ECHO_LINE 18432u                         /* int16 in the pool: 36 KB, 1.67 s */
#define ECHO_T_MAX 8960.0f                       /* T at most (0.81 s): 2T and the slew's margin fit */

typedef struct {
    float t;                                     /* the time, line samples (slewed to) */
    float g;                                     /* loop gain (T60's, or PUSH's, or FREEZE's) */
    float tone;                                  /* the in-loop low-pass coefficient, one-pole at 11 kHz */
    float head2;                                 /* the 2T head's level in the output */
    int freeze;                                  /* 1: the input closed */
} echo_cfg_t;

void echo_init(void);
void echo_config(const echo_cfg_t *c);           /* the audio interrupt, before echo_run */
void echo_run(const float *in, float *out, uint32_t n);   /* 44.1 kHz: n samples of send in, return out */
float echo_gr(void);                             /* the return limiter's gain now (1: not limiting) */
float echo_t_now(void);                          /* the slewed time, line samples */
