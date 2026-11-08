/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* control.h. Part of the unity build: app.c includes it. */
#include "control.h"
#include "engine.h"

static int16_t ctl_val[P_NIDS];             /* every range fits in 16 bits (pdesc_t) */
static uint8_t ctl_beats[P_NIDS];            /* a pending parameter's quantum: 0 now, else CMD_CUE's beats */
static uint32_t ctl_pend[P_NIDS / 32u];      /* a bit per parameter waiting for room in the queue */
ctl_stats_t ctl_stats;

void ctl_init(void)
{
    uint32_t id;
    for (id = 0; id < P_NIDS; id++) {
        ctl_val[id] = (int16_t)param_def(id);
    }
    for (id = 0; id < P_NIDS / 32u; id++)
        ctl_pend[id] = 0;
    ctl_stats.pending = 0;
}

static int ctl_push(uint32_t id)
{
    cmd_t c;
    c.op = ctl_beats[id] ? CMD_CUE : CMD_SET;
    c.beats = ctl_beats[id];
    c.id = (uint16_t)id;
    c.v = ctl_val[id];
    if (!cmdq_push(&eng_q, c)) {
        ctl_stats.refused++;
        return 0;
    }
    ctl_stats.sent++;
    return 1;
}

static void ctl_mark(uint32_t id)
{
    uint32_t bit = 1u << (id & 31u);
    if (!(ctl_pend[id >> 5] & bit)) {
        ctl_pend[id >> 5] |= bit;
        ctl_stats.pending++;
    }
}

int ctl_set(uint32_t id, int32_t v) { return ctl_cue(id, v, 0); }

int ctl_cue(uint32_t id, int32_t v, uint32_t beats)
{
    if (id >= P_NIDS || !param_desc(id))
        return -1;
    ctl_val[id] = (int16_t)param_clamp(id, v);
    ctl_beats[id] = (uint8_t)(beats > 128u ? 128u : beats);
    if (ctl_stats.pending) {                 /* something is waiting: keep the order fair, this joins it */
        ctl_mark(id);
        ctl_flush();
        return !(ctl_pend[id >> 5] >> (id & 31u) & 1u);
    }
    if (ctl_push(id))
        return 1;
    ctl_mark(id);
    return 0;
}

int32_t ctl_get(uint32_t id) { return id < P_NIDS ? ctl_val[id] : 0; }

void ctl_flush(void)
{
    uint32_t w;
    for (w = 0; w < P_NIDS / 32u && ctl_stats.pending; w++) {
        while (ctl_pend[w]) {
            uint32_t b = 0, m = ctl_pend[w];
            while (!(m & 1u)) {
                m >>= 1;
                b++;
            }
            if (!ctl_push(w * 32u + b))
                return;                      /* still full: the rest on the next frame */
            ctl_pend[w] &= ~(1u << b);
            ctl_stats.pending--;
            ctl_stats.resent++;
        }
    }
}
