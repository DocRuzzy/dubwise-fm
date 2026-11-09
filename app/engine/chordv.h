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
void chordv_config(const int16_t *ev, int tier);   /* tier: TIER_SEVEN thins them (2 skank voices, organ 1+2) */
void chordv_trig(uint32_t ch, int32_t chord, int32_t voicing, uint32_t vel);   /* ch: CH_SKANK or CH_ORGAN */
float chordv_rotor(int drum);                      /* the B-3's rotor speed now, Hz (0 the horn, 1 the drum; tests) */
uint32_t chordv_perc_n(void);                     /* .. its percussion strikes so far (tests) */
void chordv_hold(uint32_t ch);                     /* the B-3 (5.15): the chord just struck held until released */
void chordv_release(uint32_t ch);                  /* .. let go: it dies over DECAY */
int chordv_render(uint32_t ch, float *o, uint32_t n);   /* 1: wrote n samples; 0: silent */
/* 5.15, the B-3 (docs/plan-5.15-organ.md): the main loop sums a registration (nine drawbars 0..8) into the tables the
 * render reads, into the pair's other half, then swaps them: never half-written */
void chordv_b3_build(const int8_t bars[9]);
/* the organ's rotary horn's side (L - R) / 2 for the block just rendered (the stereo horn, Studio tier), 0 none */
const float *chordv_side(void);
