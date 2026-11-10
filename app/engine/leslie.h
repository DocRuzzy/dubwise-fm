/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* The B-3's FULL Leslie (v0.3): setBfree's whirl model, after setBfree-fm1's port (organ_fx.h, R-Jeb,
 * GPL-2.0-or-later; credits in leslie.c and CREDITS.md). Floats, the audio render only. */
#pragma once
#include <stdint.h>

void les_init(void);                             /* (boot) its tables from the flash into the pool */
void les_reset(void);                            /* its lines and filters silent (a switch to it, the stop twice) */
/* at the organ's half rate (22,050 Hz): les_block once a block (n: its samples there) moves the rotors toward speed
 * (0 stop, 1 slow, 2 fast: setBfree's speeds, each rotor at its own pace); les_tick then takes each sample through the
 * cabinet (l and r: the two microphones; the drum is mono, the same in both); les_spin turns the rotors on through a
 * silent block */
void les_block(uint32_t n, int speed);
void les_tick(float x, float *l, float *r);
void les_spin(uint32_t n);
float les_rotor(int drum);                       /* a rotor's speed now, Hz (tests) */
