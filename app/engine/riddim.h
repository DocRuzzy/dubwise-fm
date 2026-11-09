/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* Patterns and riddim templates (roadmap P0.4, the session 4.8; mastery guide D1). Each channel has four patterns
 * (A..D), one playing; a pattern is up to 64 steps on its grid (eighths to thirty-seconds, triplets too); a step
 * has a level (off, ghost, normal, accent), a note (the drum's GM note, the bass's MIDI note), where it lands off
 * the grid line, and how long it holds.
 * Templates are written as ordinary steps: an editable starting point, never a generator (design principle 1).
 * Shared by the engine (which plays its own copy) and the UI (which edits a mirror and sends each change). */
#pragma once
#include <stdint.h>
#include "params.h"

#define NSTEPS 64u
#define NPAT 4u                                  /* patterns a channel: A..D (the session: one plays, on the line) */
enum { LVL_OFF, LVL_GHOST, LVL_NORMAL, LVL_ACCENT, LVL_N };
/* a step: its level and note, where it lands against its grid line (off: fine ticks, within half a step either
 * way: played or nudged off the grid), and how long a note holds (gate: fine ticks / 4; 0: the channel's own, the
 * bass a step, the lead two) */
typedef struct {
    uint8_t lvl;
    int8_t note;
    int8_t off;
    uint8_t gate;
} step_t;

/* the sequencer's fine clock: 240 a beat (60 a sixteenth; BEAT_UNITS / 240 exact). A pattern's grid sets its step
 * (GRID_16: a sixteenth, 60; GRID_16T: a sixteenth triplet, 40 ..) */
#define PPQ 240u
enum { GRID_8, GRID_8T, GRID_16, GRID_16T, GRID_32, GRID_N };
static inline int32_t grid_fine(uint32_t g)
{
    static const uint8_t F[GRID_N] = {120, 80, 60, 40, 30};
    return F[g < GRID_N ? g : GRID_16];
}
/* a pattern's own settings: its length in steps (1..64), its grid, its swing (50..75 %: every other step late) */
typedef struct {
    uint8_t len, grid, swing, rsv;
} pmeta_t;
#define SWING_MIN 50
#define SWING_MAX 75
static inline pmeta_t pmeta_def(void)
{
    pmeta_t m = {16, GRID_16, 50, 0};
    return m;
}
static inline pmeta_t pmeta_ok(pmeta_t m)          /* clamped */
{
    if (m.len < 1u || m.len > NSTEPS)
        m.len = 16;
    if (m.grid >= GRID_N)
        m.grid = GRID_16;
    if (m.swing < SWING_MIN || m.swing > SWING_MAX)
        m.swing = 50;
    m.rsv = 0;
    return m;
}
static inline int32_t swing_fine(pmeta_t m, int32_t n)   /* step n's lateness from the swing */
{
    int32_t sl = grid_fine(m.grid);
    return (n & 1) ? sl * (2 * (int32_t)m.swing - 100) / 100 : 0;
}
enum { TPL_ONE_DROP, TPL_ROCKERS, TPL_STEPPERS, TPL_FLYERS, TPL_UKSTEP, TPL_N };   /* (append only: saved) */
const char *template_name(int32_t t);
void template_fill(int32_t t, uint32_t ch, step_t *dst);   /* NSTEPS steps of channel ch (empty if it has none) */
int8_t default_note(uint32_t ch);                /* the note a new step gets on ch */
uint32_t lvl_vel(uint32_t lvl);                  /* a level's velocity */

/* a step as one command value (cmdq.h CMD_STEP): level 2 bits, note 7, off 8, gate 8 */
static inline int32_t step_pack(step_t s)
{
    return (int32_t)(s.lvl & 3u) | (int32_t)(s.note & 0x7F) << 2 | (int32_t)(uint8_t)s.off << 9
           | (int32_t)s.gate << 17;
}
static inline step_t step_unpack(int32_t v)
{
    step_t s;
    s.lvl = (uint8_t)(v & 3);
    s.note = (int8_t)((v >> 2) & 0x7F);
    s.off = (int8_t)(uint8_t)((v >> 9) & 0xFF);
    s.gate = (uint8_t)((v >> 17) & 0xFF);
    return s;
}
static inline int32_t pmeta_pack(pmeta_t m) { return (int32_t)m.len | (int32_t)m.grid << 7 | (int32_t)m.swing << 10; }
static inline pmeta_t pmeta_unpack(int32_t v)
{
    pmeta_t m;
    m.len = (uint8_t)(v & 0x7F);
    m.grid = (uint8_t)((v >> 7) & 7);
    m.swing = (uint8_t)((v >> 10) & 0x7F);
    m.rsv = 0;
    return pmeta_ok(m);
}
/* CMD_STEP's id: a step of a channel's pattern, or (i == NSTEPS) the pattern's settings */
#define STEP_ID(ch, p, i) ((((uint32_t)(ch)) * NPAT + (uint32_t)(p)) * (NSTEPS + 1u) + (uint32_t)(i))
#define STEP_IDS (NCH * NPAT * (NSTEPS + 1u))
