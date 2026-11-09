/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* The core voices (roadmap P0.5; mastery guide D3): kick, rim/snare, hats, bass, on channels 1-4. Mono, 44.1 kHz,
 * floats. Each has its own state and its own parameters (P_VOICE(ch, ...)); the coefficients are worked out once a
 * block (control rate) from the applied values. A voice that has died away writes nothing (0: silent), so a quiet
 * channel costs nothing.
 *   kick   a sine swept from SWEEP down to TUNE Hz over PTIME, a 1:1 FM burst for the beater (CLICK)
 *   rim    inharmonic FM (1:1.41, plus 1:3.5) with a fast index decay, a 5 ms noise burst, high-passed at 300 Hz
 *   hats   four squares at 1, 1.48, 2.27, 3.6 x TONE and noise, high-passed at 7 kHz; closed (note 42, 44) or open
 *          (46): a closed hit chokes an open one
 *   bass   2-op FM (1:1 or 2:1) with a fast index decay to a near-pure sine, an optional sub an octave down, a
 *          plucky decay to SUSTAIN while held, two one-pole low-passes at CUTOFF */
#pragma once
#include <stdint.h>

void voices_init(void);
void voices_start(void);                         /* the transport's start: the noise from its seed (replays the same) */
void voices_config(const int16_t *ev, int tier); /* once a block: the parameters applied (Seven-inch: no sub) */
void voice_trig(uint32_t ch, int32_t note, uint32_t vel);   /* a hit / a note on (vel 1..127) */
void voice_release(uint32_t ch, int32_t note);   /* a note off (the bass; the drums ring out) */
int voice_render(uint32_t ch, float *o, uint32_t n);   /* 1: wrote n samples; 0: silent, nothing written */
int32_t voice_note(uint32_t ch);                 /* the bass's note held, -1 none */
void voices_panic(void);
