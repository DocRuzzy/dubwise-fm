/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* The engine: the audio interrupt's side. Each block it applies the queued commands, renders each channel's source
 * into its own buffer, and mixes the eight channels in a fixed order into the mono dry path and the echo and
 * spring send buses (whose effects are P0.3 and P1.2: their returns are silent for now). Stereo is only for the
 * returns (design principle 6): the dry path goes to both sides.
 *
 * The render never allocates, never waits and never touches the flash. CMD_SET takes effect at the start of the
 * next block (at most 5.8 ms: player input). CMD_CUE lands on its exact sample: the transport counts beats with
 * integer arithmetic, and the block is split where a cue falls (launch quantise, P0.1). */
#pragma once
#include <stdint.h>
#include "params.h"
#include "cmdq.h"

extern cmdq_t eng_q;                         /* control.c pushes, eng_render pops */

void eng_init(void);                         /* boot: every parameter at its default, everything silent */
void eng_render(int32_t *lr, uint32_t n, uint32_t master_q12);   /* audio interrupt; n <= ENG_BLOCK */
void eng_panic(void);                        /* silence now */

/* the key voice (the template's, on the bass channel until P0.5): main loop or audio interrupt (MIDI in) */
void eng_note_on(int32_t note, uint32_t vel);
void eng_note_off(int32_t note);
int32_t eng_note(void);                      /* the note sounding, -1 none */

/* read back (main loop): what the engine has applied, and its meters */
int32_t eng_param(uint32_t id);
float eng_ch_peak(uint32_t ch);              /* the channel's post-fader peak over the last block, 0..1+ */
float eng_echo_peak(void);                   /* the echo send bus's */
float eng_spring_peak(void);
float eng_echo_ret_peak(void);               /* the echo's return, after its level */
float eng_echo_gr(void);                     /* the return limiter's gain (1: not limiting) */
int32_t eng_echo_div(void);                  /* the division in use (P_ECHO_TIME, or shorter if that does not fit) */
float eng_echo_t(void);                      /* the echo's time now, line samples (11,025 a second) */
float eng_echo_g(void);                      /* the loop gain asked for (1 and over: rings on) */
uint32_t eng_cmds(void);                     /* commands applied since boot */
int eng_playing(void);
uint32_t eng_beat(void);                     /* beats since the start (bar = beat / 4 + 1) */
uint32_t eng_fire_at(void);                  /* the sample (since the start) the last cue landed on */
uint32_t eng_fires(void);                    /* cues fired since boot */
uint32_t eng_cues(void);                     /* cues waiting */
uint32_t eng_cue_overflows(void);

#define ENG_BLOCK 256u

/* per-part profiling (P0.7): plat_ticks24 ticks (24 a microsecond) each part took in a block, a running average
 * (1/16 a block) and the most since the last eng_prof_reset. The device's budget for a block is
 * ENG_BLOCK / 44100 s = 139,320 ticks; the host runs ~80x faster than the device */
enum { PART_CMD, PART_CH0, PART_MIX = PART_CH0 + NCH, PART_ECHO, PART_SPRING, PART_OUT, NPARTS };
typedef struct {
    uint32_t avg;                            /* x16: ticks a block = avg / 16 */
    uint32_t max;                            /* ticks */
} eng_prof_t;
eng_prof_t eng_prof(uint32_t part);
const char *eng_part_name(uint32_t part);
void eng_prof_reset(void);
#define ENG_BLOCK_TICKS 139320u
