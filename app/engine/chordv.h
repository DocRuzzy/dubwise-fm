/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* The chord voices (roadmap P1.6; mastery guide D2, D3): the skank and the organ bubble, three voices each, playing
 * the chord lane's chord in a step's voicing (low, mid, high). Mono, 44.1 kHz, floats.
 *   skank   2-op FM (1:1 or 1:3), its index gone in ~30 ms (a bright, nasal attack), 60-200 ms, band-passed
 *           ~400 Hz-3 kHz: the short, muted chop on 2 and 4 ("the piston of a steam engine")
 *   organ   partials 1, 2, 3, 4 like drawbars and a key-click partial, a short gate: the bubble */
#pragma once
#include <stdint.h>

void chordv_init(void);
void chordv_config(const int16_t *ev);
void chordv_trig(uint32_t ch, int32_t chord, int32_t voicing, uint32_t vel);   /* ch: CH_SKANK or CH_ORGAN */
int chordv_render(uint32_t ch, float *o, uint32_t n);   /* 1: wrote n samples; 0: silent */
