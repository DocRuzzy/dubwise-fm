/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* The UI -> engine command queue: one producer (the main loop), one consumer (the audio interrupt, app_render), on
 * the same core. No lock, no allocation, no waiting: a full queue refuses the push, and control.c resends (the
 * parameter's latest value) on a later frame. 256 entries at least (prior-art lesson 8: 64 dropped PLAY in X0X). */
#pragma once
#include <stdint.h>

#define CMDQ_N 256u
typedef char cmdq_n_check[(CMDQ_N >= 256u && (CMDQ_N & (CMDQ_N - 1u)) == 0) ? 1 : -1];

/* opcodes: their own space, apart from the parameter IDs. CMD_SET: now (the block's start). CMD_CUE: at the next
 * multiple of `beats` beats from the transport's start, P_LEAD ms early (at once while stopped) */
enum {
    CMD_NONE,
    CMD_SET,
    CMD_CUE,
    CMD_STEP,                     /* a pattern step: id = ch x NSTEPS + step, v = step_pack (riddim.h); now */
    CMD_TEMPLATE,                 /* a riddim template: id = TPL_*, v = the channels (a mask); quantised as CMD_CUE */
};

typedef struct {
    uint8_t op;
    uint8_t beats;                /* CMD_CUE: the quantum, 1..128 beats */
    uint16_t id;                  /* params.h */
    int32_t v;
} cmd_t;

typedef struct {
    volatile uint32_t w, r;       /* free-running; w - r = entries */
    cmd_t buf[CMDQ_N];
} cmdq_t;

/* the only inline asm the build's register check allows: a compiler barrier (no instruction) */
#define CMDQ_BARRIER() __asm__ volatile("" ::: "memory")

static inline int cmdq_push(cmdq_t *q, cmd_t c)     /* main loop: 1 queued, 0 full */
{
    uint32_t w = q->w;
    if (w - q->r >= CMDQ_N)
        return 0;
    q->buf[w & (CMDQ_N - 1u)] = c;
    CMDQ_BARRIER();               /* the entry is written before the consumer can see it */
    q->w = w + 1u;
    return 1;
}

static inline int cmdq_pop(cmdq_t *q, cmd_t *c)     /* audio interrupt: 1 taken, 0 empty */
{
    uint32_t r = q->r;
    if (r == q->w)
        return 0;
    CMDQ_BARRIER();
    *c = q->buf[r & (CMDQ_N - 1u)];
    CMDQ_BARRIER();               /* the entry is read before the producer may reuse it */
    q->r = r + 1u;
    return 1;
}

static inline uint32_t cmdq_count(const cmdq_t *q) { return q->w - q->r; }
