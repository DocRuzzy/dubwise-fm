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
    uint8_t lvl;                                 /* bits 0-1 the level; 2-3 CHANCE (5.9); 4-6 REPEAT (5.8) */
    int8_t note;
    int8_t off;
    uint8_t gate;                                /* a drum's: its TUNE (low nibble) and DECAY (high) locks (5.8) */
} step_t;
/* 5.8, 5.9 (docs/plan-5.8-5.9-steps.md): in a step's spare bits, 0 in every older save (100 %, no repeat). A drum's
 * gate means nothing to its sound, so a drum step keeps its two locks there, -8..+7 each, 0 none */
enum { CHANCE_100, CHANCE_75, CHANCE_50, CHANCE_25, CHANCE_N };
enum { REP_OFF, REP_FLAM, REP_2, REP_3, REP_4, REP_N };
#define LVL(s) ((uint32_t)(s).lvl & 3u)
#define STEP_CHANCE(s) (((uint32_t)(s).lvl >> 2) & 3u)
#define STEP_REPEAT(s) (((uint32_t)(s).lvl >> 4) & 7u)
#define STEP_LVL_SET(s, l) ((s).lvl = (uint8_t)(((s).lvl & ~3u) | ((uint32_t)(l) & 3u)))
#define STEP_CHANCE_SET(s, c) ((s).lvl = (uint8_t)(((s).lvl & ~0x0Cu) | (((uint32_t)(c) & 3u) << 2)))
#define STEP_REPEAT_SET(s, r) ((s).lvl = (uint8_t)(((s).lvl & ~0x70u) | (((uint32_t)(r) & 7u) << 4)))
static inline int is_drum(uint32_t ch) { return ch == CH_KICK || ch == CH_SNARE || ch == CH_HATS; }
static inline int32_t lock_tune(step_t s) { return (int32_t)((int8_t)(s.gate << 4)) >> 4; }   /* -8..+7 */
static inline int32_t lock_decay(step_t s) { return (int32_t)((int8_t)(s.gate & 0xF0u)) >> 4; }
/* (shared by the engine, which plays it, and the STEPS page, which marks the steps that will skip this loop) */
/* 5.9: a step's CHANCE, decided by its pattern's SEED, the step and the loop since the launch: the same launches play
 * the same notes, and a captured song replays as it was */
static inline int chance_plays(uint32_t ch, uint32_t p, uint32_t i, uint32_t loop, uint32_t c, uint32_t seed)
{
    static const uint8_t PCT[CHANCE_N] = {100, 75, 50, 25};
    uint32_t h;
    if (c == CHANCE_100)
        return 1;
    h = (seed + 1u) * 2654435761u ^ (ch * 64u + p * 16u + i) * 2246822519u ^ (loop + 1u) * 3266489917u;
    h ^= h >> 15;
    h *= 2246822519u;
    h ^= h >> 13;
    return h % 100u < PCT[c & 3u];
}
static inline uint8_t locks_pack(int32_t tune, int32_t decay) { return (uint8_t)(((uint32_t)decay & 15u) << 4 | ((uint32_t)tune & 15u)); }

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
        m.swing = 50;                            /* (rsv: the SEED, 5.9: any value) */
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

/* a step as one command value (cmdq.h CMD_STEP): level 2 bits, note 7, off 8, gate 8, CHANCE 2 and REPEAT 3 (5.8, 5.9) */
static inline int32_t step_pack(step_t s)
{
    return (int32_t)(s.lvl & 3u) | (int32_t)(s.note & 0x7F) << 2 | (int32_t)(uint8_t)s.off << 9
           | (int32_t)s.gate << 17 | (int32_t)((s.lvl >> 2) & 0x1Fu) << 25;
}
static inline step_t step_unpack(int32_t v)
{
    step_t s;
    s.lvl = (uint8_t)((v & 3) | ((v >> 25) & 0x1F) << 2);
    s.note = (int8_t)((v >> 2) & 0x7F);
    s.off = (int8_t)(uint8_t)((v >> 9) & 0xFF);
    s.gate = (uint8_t)((v >> 17) & 0xFF);
    return s;
}
static inline int32_t pmeta_pack(pmeta_t m)   /* len 7, grid 3, swing 7, the SEED 8 (5.9: pmeta.rsv) */
{ return (int32_t)m.len | (int32_t)m.grid << 7 | (int32_t)m.swing << 10 | (int32_t)m.rsv << 17; }
static inline pmeta_t pmeta_unpack(int32_t v)
{
    pmeta_t m;
    m.len = (uint8_t)(v & 0x7F);
    m.grid = (uint8_t)((v >> 7) & 7);
    m.swing = (uint8_t)((v >> 10) & 0x7F);
    m.rsv = (uint8_t)((v >> 17) & 0xFF);         /* the SEED (5.9) */
    return pmeta_ok(m);
}
/* CMD_STEP's id: a step of a channel's pattern, or (i == NSTEPS) the pattern's settings */
#define STEP_ID(ch, p, i) ((((uint32_t)(ch)) * NPAT + (uint32_t)(p)) * (NSTEPS + 1u) + (uint32_t)(i))
#define STEP_IDS (NCH * NPAT * (NSTEPS + 1u))
