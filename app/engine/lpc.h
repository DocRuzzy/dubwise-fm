/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* The deejay voice (roadmap 6.6): shouts said by an LPC speech chip, after SPELLBOX's (credits in lpc.c) */
#pragma once
#include <stdint.h>

void lpc_init(void);                             /* (boot) its tables into the pool */
/* shout k said (the render, or the main loop before it plays): vel 0..127, semis -12..12, stutter 0..127 (a frame
 * played again now and then), crush 0..5 (the DAC's low bits off) */
void lpc_say(int32_t k, uint32_t vel, int semis, int stutter, int crush);
int lpc_render(float *o, uint32_t n, int add);   /* 44.1 kHz into o (added, or written); 1: it sounded */
const char *lpc_name(int32_t k);                 /* "SELECTA" .. */
uint32_t lpc_count(void);
int lpc_talking(void);
