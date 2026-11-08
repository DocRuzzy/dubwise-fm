/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* The main loop's side of the parameters: what the player asked for (the desired values), sent to the engine
 * through the command queue. When the queue is full, the parameter waits in a pending set, and ctl_flush (every
 * frame) sends its latest value later. Nothing is lost and nothing stale lands: a newer value for a pending
 * parameter only updates what will be sent. Main loop only. */
#pragma once
#include <stdint.h>
#include "params.h"

void ctl_init(void);                       /* the desired values to the defaults (the engine starts from them too) */
int ctl_set(uint32_t id, int32_t v);       /* clamped; 1 queued, 0 pending (queue full), -1 no such parameter */
int ctl_cue(uint32_t id, int32_t v, uint32_t beats);   /* the same, quantised: at the next multiple of beats (0: now) */
int32_t ctl_get(uint32_t id);              /* the desired value */
void ctl_flush(void);                      /* resend what is pending, in ID order, until the queue is full */

typedef struct {
    uint32_t sent;                         /* commands queued */
    uint32_t refused;                      /* pushes the full queue refused */
    uint32_t resent;                       /* pending parameters sent by ctl_flush */
    uint32_t pending;                      /* waiting now */
} ctl_stats_t;
extern ctl_stats_t ctl_stats;
