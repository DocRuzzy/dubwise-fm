/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* The engine: the audio interrupt's side. Each block it applies the queued commands, renders each channel's source
 * into its own buffer, and mixes the eight channels in a fixed order into the mono dry path and the echo and
 * spring send buses (whose effects are P0.3 and P1.2: their returns are silent for now). Stereo is only for the
 * returns (design principle 6): the dry path goes to both sides.
 *
 * The render never allocates, never waits and never touches the flash. UI commands take effect at the start of
 * the next block (at most 5.8 ms: player input); sequencer events will carry sample offsets (P0.6). */
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
uint32_t eng_cmds(void);                     /* commands applied since boot */

#define ENG_BLOCK 256u
