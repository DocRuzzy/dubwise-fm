/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* The main loop's side of the parameters: what the player asked for (the desired values), sent to the engine
 * through the command queue. When the queue is full, the parameter waits in a pending set, and ctl_flush (every
 * frame) sends its latest value later. Nothing is lost and nothing stale lands: a newer value for a pending
 * parameter only updates what will be sent. Main loop only. */
#pragma once
#include <stdint.h>
#include "params.h"
#include "riddim.h"

void ctl_init(void);                       /* the desired values to the defaults (the engine starts from them too) */
int ctl_set(uint32_t id, int32_t v);       /* clamped; 1 queued, 0 pending (queue full), -1 no such parameter */
int ctl_cue(uint32_t id, int32_t v, uint32_t beats);   /* the same, quantised: at the next multiple of beats (0: now) */
int32_t ctl_get(uint32_t id);              /* the desired value */
void ctl_flush(void);                      /* resend what is pending, in ID order, until the queue is full */
void ctl_follow(void);                     /* every frame: the values the song's motion played back (P2.7) */
void ctl_motion_clear(void);               /* the song's motion, all of it, cleared (in the queue's order) */
void ctl_motion(uint32_t what);            /* CMD_MOTION: 0 clear, 1 handed to the main loop (mot_suspended), 2 back */

/* the patterns (riddim.h): the UI's mirror, each change sent as CMD_STEP (pending and resent like parameters) */
void ctl_step(uint32_t ch, uint32_t i, step_t st);
step_t ctl_get_step(uint32_t ch, uint32_t i);
void ctl_template(int32_t t, uint32_t mask, uint32_t beats);   /* the mirror at once, the engine on the line */

/* a played note (the keyboard): at once, never quantised; vel 0 lets it go. Notes are not resent: a full queue
 * (256 waiting) drops one (counted as refused) */
void ctl_note(uint32_t ch, int32_t note, uint32_t vel);
const int16_t *ctl_vals(void);             /* the desired values, by ID (lock_mask and the like) */

/* scenes (scenes.h): store the desk (and the patterns) as they are asked for now; recall one: the mirror at once,
 * the engine on the line (one CMD_SCENE) */
void ctl_scene_store(uint32_t s, uint32_t scope);
void ctl_scene_recall(uint32_t s, uint32_t beats);

typedef struct {
    uint32_t sent;                         /* commands queued */
    uint32_t refused;                      /* pushes the full queue refused */
    uint32_t resent;                       /* pending parameters sent by ctl_flush */
    uint32_t pending;                      /* waiting now */
} ctl_stats_t;
extern ctl_stats_t ctl_stats;
