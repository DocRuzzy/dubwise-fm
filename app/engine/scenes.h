/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* Scenes (roadmap P1.7; mastery guide C2, spec 4.2): sixteen snapshots of the desk (each channel's level, mute,
 * solo, sends and throw destination; the Big Knob; the returns), or of the desk and the patterns (a "P" scene).
 * One table, in the pool, shared: the main loop stores into it (scene_capture), the engine recalls from it at the
 * line (scene_apply, a CMD_SCENE cue), so a recall is one command, not hundreds. Stored rarely, recalled often: a
 * scene is never stored while it is being recalled (the UI makes sure). */
#pragma once
#include <stdint.h>
#include "params.h"
#include "riddim.h"

#define NSCENES 16
enum { SCOPE_DESK, SCOPE_PATTERNS, SCOPE_N };

/* the desk's parameters a scene keeps */
#define SCENE_CH_FIELDS 6                        /* level, mute, solo, echo, spring, dest */
#define SCENE_GLOBALS 3                          /* the Big Knob, the echo's return, the spring's */
#define SCENE_DESK (NCH * SCENE_CH_FIELDS + SCENE_GLOBALS)

typedef struct {
    uint8_t used, scope;
    int16_t desk[SCENE_DESK];
    uint8_t len[NCH];                            /* SCOPE_PATTERNS: the patterns and their lengths */
    step_t pat[NCH][NSTEPS];
} scene_t;

extern scene_t scenes[NSCENES];

uint32_t scene_param(uint32_t k);                /* the parameter at desk[k] */
/* main loop: store the values (by ID) and, for SCOPE_PATTERNS, the patterns (pat[ch][i], len by CHF_LEN) */
void scene_capture(uint32_t s, uint32_t scope, const int16_t *vals, const step_t (*pat)[NSTEPS]);
/* engine: the scene into the applied values and the patterns */
void scene_apply(uint32_t s, int16_t *vals, step_t (*pat)[NSTEPS]);
