/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* Patterns and riddim templates (roadmap P0.4; mastery guide D1). A pattern is up to 64 sixteenth-note steps per
 * channel; a step has a level (off, ghost, normal, accent) and a note (the drum's GM note, the bass's MIDI note).
 * Templates are written as ordinary steps: an editable starting point, never a generator (design principle 1).
 * Shared by the engine (which plays its own copy) and the UI (which edits a mirror and sends each change). */
#pragma once
#include <stdint.h>
#include "params.h"

#define NSTEPS 64u
enum { LVL_OFF, LVL_GHOST, LVL_NORMAL, LVL_ACCENT, LVL_N };
typedef struct {
    uint8_t lvl;
    int8_t note;
} step_t;

enum { TPL_ONE_DROP, TPL_ROCKERS, TPL_STEPPERS, TPL_FLYERS, TPL_DUBTECHNO, TPL_N };   /* (append only: saved) */
const char *template_name(int32_t t);
void template_fill(int32_t t, uint32_t ch, step_t *dst);   /* NSTEPS steps of channel ch (empty if it has none) */
int8_t default_note(uint32_t ch);                /* the note a new step gets on ch */
uint32_t lvl_vel(uint32_t lvl);                  /* a level's velocity */

/* a step as one command value (cmdq.h CMD_STEP: id = ch x NSTEPS + step) */
static inline int32_t step_pack(step_t s) { return (int32_t)s.lvl | (int32_t)(uint8_t)s.note << 8; }
static inline step_t step_unpack(int32_t v)
{
    step_t s;
    s.lvl = (uint8_t)(v & 0xFF);
    s.note = (int8_t)(uint8_t)((v >> 8) & 0xFF);
    return s;
}
