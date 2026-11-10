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
    float slew;                                  /* the time's glide, a factor a line sample (0.0005: ~180 ms) */
    float width;                                 /* the right side's extra delay, line samples (0: mono); P1.5 */
    int diffuse;                                 /* 1: the in-loop all-passes (off at the Seven-inch tier) */
    float drive;                                 /* the tape's saturation: 1, more at Seven-inch */
    int pingpong;                                /* P3.1: the loop from 2T (gain g^2), odd repeats left, even right */
    float wow;                                   /* 6.3: the tape's wow and flutter, 0..1 (0 none) */
    float head3;                                 /* 7.1: the 3T head's level in the output (beyond the tape: its end) */
    int bbd;                                     /* 7.9: a bucket brigade: the loop and the return through its filters */
} echo_cfg_t;

void echo_init(void);
void echo_clear(void);                           /* tails off (5.2): the tape emptied, its time and settings kept */
void echo_config(const echo_cfg_t *c);           /* the audio interrupt, before echo_run */
/* 44.1 kHz: n samples of send in, the return out, left and right. Width: the right reads the tape later, and only
 * the difference above ~120 Hz reaches it (the low end stays mono: design principles) */
void echo_run(const float *in, float *outl, float *outr, uint32_t n);
float echo_gr(void);                             /* the return limiter's gain now (1: not limiting) */
float echo_t_now(void);                          /* the slewed time, line samples */
int echo_idle(void);                             /* 9.1: 1 the tape empty and nothing coming in: it costs nothing */
void echo_wow_start(void);                       /* 6.3: the wow from its start (a replay the same) */
