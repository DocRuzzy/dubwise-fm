/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* Channel 8, Vox / FX: the sirens (roadmap P2.1; mastery guide C9, D3), held to sound; and three one-shots (P2.6;
 * C9, C10: BIM, HORN, LASER: original, synthesized by tools/make_oneshots.py), 8-bit mu-law at 22,050 Hz, copied
 * from the flash into the pool at boot (the render reads RAM only), played to their end. Siren A: a square LFO, two
 * tones a step apart (DEPTH); siren B: a rising sweep, over and over. PITCH, RATE, DEPTH, WAVE (sine / triangle).
 * The channel sends to the echo by default (the authentic use). Mono, 44.1 kHz, floats. */
#pragma once
#include <stdint.h>

enum { SIREN_A, SIREN_B };
void fxv_init(void);
void fxv_config(const int16_t *ev);
void fxv_on(int32_t which, uint32_t vel);        /* a siren, while held (vel 1..127) */
void fxv_off(int32_t which);
void fxv_shot(uint32_t k, uint32_t vel);         /* one-shot k (0..2), from the top (main loop or render) */
void fxv_shot_dir(uint32_t k, uint32_t vel, int rev);   /* rev: backwards, its end first (P3.3) */
const char *fxv_shot_name(uint32_t k);
int fxv_render(float *o, uint32_t n);            /* 1: wrote n samples; 0: silent */
