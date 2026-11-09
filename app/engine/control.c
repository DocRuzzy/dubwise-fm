/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* control.h. Part of the unity build: app.c includes it. */
#include "control.h"
#include "engine.h"
#include "scenes.h"
#include "motion.h"

static int16_t ctl_val[P_NIDS];             /* every range fits in 16 bits (pdesc_t) */
static uint8_t ctl_beats[P_NIDS];            /* a pending parameter's quantum: 0 now, else CMD_CUE's beats */
static uint32_t ctl_pend[P_NIDS / 32u];      /* a bit per parameter waiting for room in the queue */
ctl_stats_t ctl_stats;
static step_t ctl_pat[NCH][NSTEPS];
static int32_t ctl_scn;
static int scene_push(void);
static uint32_t ctl_step_pend[NCH * NSTEPS / 32u];
static uint32_t ctl_steps_pending;
static int32_t ctl_tpl_t = -1;                   /* a template the full queue refused */
static uint32_t ctl_tpl_mask, ctl_tpl_beats;
/* the undo history's hook (undo.c): each change to the mirror, a parameter (key: its ID) or a step (key: CTL_STEP_KEY
 * | channel x NSTEPS + step; values step_t packed: level << 8 | note) */
static void (*ctl_watch)(uint32_t key, int32_t old, int32_t nw);
void ctl_set_watch(void (*f)(uint32_t key, int32_t old, int32_t nw)) { ctl_watch = f; }
int32_t ctl_step_pack(step_t s) { return (int32_t)((uint32_t)s.lvl << 8 | (uint8_t)s.note); }
step_t ctl_step_unpack(int32_t v)
{
    step_t s;
    s.lvl = (uint8_t)(v >> 8 & 0xFF);
    s.note = (int8_t)(v & 0xFF);
    return s;
}
static step_t ctl_pat_was[NCH][NSTEPS];          /* (a template's or a scene's patterns: the steps before, to diff) */
static void pat_diff(void)
{
    uint32_t ch, i;
    for (ch = 0; ch < NCH; ch++)
        for (i = 0; i < NSTEPS; i++)
            if (ctl_step_pack(ctl_pat_was[ch][i]) != ctl_step_pack(ctl_pat[ch][i]))
                ctl_watch(CTL_STEP_KEY | (ch * NSTEPS + i), ctl_step_pack(ctl_pat_was[ch][i]),
                          ctl_step_pack(ctl_pat[ch][i]));
}
static uint8_t ctl_stamp[P_NIDS];                /* the frame a parameter was last set by hand (ctl_follow) */
static uint8_t ctl_frame;
#define MOT_PEND 4u
static uint8_t ctl_mot[MOT_PEND], ctl_mot_n;     /* motion commands the full queue refused, in order */

static int mot_push(void)                        /* the oldest waiting: 1 sent */
{
    cmd_t c;
    uint32_t i;
    c.op = CMD_MOTION;
    c.beats = 0;
    c.id = ctl_mot[0];
    c.v = 0;
    if (!cmdq_push(&eng_q, c)) {
        ctl_stats.refused++;
        return 0;
    }
    ctl_stats.sent++;
    for (i = 1; i < ctl_mot_n; i++)
        ctl_mot[i - 1u] = ctl_mot[i];
    ctl_mot_n--;
    return 1;
}

void ctl_motion(uint32_t what)                   /* CMD_MOTION: 0 clear, 1 hand over, 2 take back */
{
    if (ctl_mot_n == MOT_PEND)
        return;
    ctl_mot[ctl_mot_n++] = (uint8_t)what;
    while (ctl_mot_n && !ctl_stats.pending && mot_push())
        ;
}

void ctl_motion_clear(void) { ctl_motion(0); }

void ctl_follow(void)                            /* the moves motion played: the desired values follow them */
{
    uint32_t id;
    int32_t v;
    while (mot_fb_pop(&id, &v))
        if (id < P_NIDS && !(ctl_pend[id >> 5] >> (id & 31u) & 1u) && (uint8_t)(ctl_frame - ctl_stamp[id]) > 8u)
            ctl_val[id] = (int16_t)v;            /* (a hand on it in the last 8 frames: the hand's value stays) */
}

void ctl_init(void)
{
    uint32_t id;
    for (id = 0; id < P_NIDS; id++) {
        ctl_val[id] = (int16_t)param_def(id);
    }
    for (id = 0; id < P_NIDS / 32u; id++)
        ctl_pend[id] = 0;
    ctl_stats.pending = 0;
    for (id = 0; id < NCH; id++)
        template_fill(TPL_ONE_DROP, id, ctl_pat[id]);   /* as the engine starts */
    for (id = 0; id < NCH * NSTEPS / 32u; id++)
        ctl_step_pend[id] = 0;
    ctl_steps_pending = 0;
    ctl_tpl_t = -1;
    ctl_scn = -1;
}

static int step_push(uint32_t k)
{
    cmd_t c;
    c.op = CMD_STEP;
    c.beats = 0;
    c.id = (uint16_t)k;
    c.v = step_pack(ctl_pat[k / NSTEPS][k % NSTEPS]);
    if (!cmdq_push(&eng_q, c)) {
        ctl_stats.refused++;
        return 0;
    }
    ctl_stats.sent++;
    return 1;
}

static int tpl_push(void)
{
    cmd_t c;
    c.op = CMD_TEMPLATE;
    c.beats = (uint8_t)ctl_tpl_beats;
    c.id = (uint16_t)ctl_tpl_t;
    c.v = (int32_t)ctl_tpl_mask;
    if (!cmdq_push(&eng_q, c)) {
        ctl_stats.refused++;
        return 0;
    }
    ctl_stats.sent++;
    ctl_tpl_t = -1;
    return 1;
}

void ctl_step(uint32_t ch, uint32_t i, step_t st)
{
    uint32_t k = (ch % NCH) * NSTEPS + i % NSTEPS, bit = 1u << (k & 31u);
    int32_t was = ctl_step_pack(ctl_pat[ch % NCH][i % NSTEPS]);
    ctl_pat[ch % NCH][i % NSTEPS] = st;
    if (ctl_watch && was != ctl_step_pack(st))
        ctl_watch(CTL_STEP_KEY | k, was, ctl_step_pack(st));
    if (ctl_steps_pending || ctl_tpl_t >= 0 || ctl_stats.pending || !step_push(k)) {
        if (!(ctl_step_pend[k >> 5] & bit)) {   /* (a newer value for a waiting step: sent as it is then) */
            ctl_step_pend[k >> 5] |= bit;
            ctl_steps_pending++;
        }
    }
}

step_t ctl_get_step(uint32_t ch, uint32_t i) { return ctl_pat[ch % NCH][i % NSTEPS]; }

void ctl_template(int32_t t, uint32_t mask, uint32_t beats)
{
    uint32_t ch, w;
    if (ctl_watch)
        for (ch = 0; ch < NCH; ch++)
            for (w = 0; w < NSTEPS; w++)
                ctl_pat_was[ch][w] = ctl_pat[ch][w];
    for (ch = 0; ch < NCH; ch++)
        if (mask >> ch & 1u)
            template_fill(t, ch, ctl_pat[ch]);
    if (ctl_watch)
        pat_diff();
    /* steps still waiting for those channels are now the template's: theirs no longer to send */
    for (w = 0; w < NCH * NSTEPS / 32u; w++) {
        uint32_t m = ctl_step_pend[w], b;
        for (b = 0; b < 32u; b++)
            if ((m >> b & 1u) && (mask >> ((w * 32u + b) / NSTEPS) & 1u)) {
                ctl_step_pend[w] &= ~(1u << b);
                ctl_steps_pending--;
            }
    }
    ctl_tpl_t = t;
    ctl_tpl_mask = mask;
    ctl_tpl_beats = beats > 128u ? 128u : beats;
    if (!ctl_stats.pending)
        tpl_push();
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
    int16_t was;
    if (id >= P_NIDS || !param_desc(id))
        return -1;
    was = ctl_val[id];
    ctl_val[id] = (int16_t)param_clamp(id, v);
    if (ctl_watch && was != ctl_val[id])
        ctl_watch(id, was, ctl_val[id]);
    ctl_stamp[id] = ctl_frame;
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
    ctl_frame++;
    while (ctl_mot_n)
        if (!mot_push())
            return;
    if (ctl_tpl_t >= 0 && !tpl_push())
        return;
    if (ctl_scn >= 0 && !scene_push())
        return;
    for (w = 0; w < NCH * NSTEPS / 32u && ctl_steps_pending; w++) {   /* steps waiting, in order */
        while (ctl_step_pend[w]) {
            uint32_t b = 0, m = ctl_step_pend[w];
            while (!(m & 1u)) {
                m >>= 1;
                b++;
            }
            if (!step_push(w * 32u + b))
                return;
            ctl_step_pend[w] &= ~(1u << b);
            ctl_steps_pending--;
            ctl_stats.resent++;
        }
    }
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

void ctl_note(uint32_t ch, int32_t note, uint32_t vel)
{
    cmd_t c;
    c.op = CMD_NOTE;
    c.beats = 0;
    c.id = (uint16_t)ch;
    c.v = (note & 0x7F) | (int32_t)(vel & 0x7Fu) << 8;
    if (cmdq_push(&eng_q, c))
        ctl_stats.sent++;
    else
        ctl_stats.refused++;
}

const int16_t *ctl_vals(void) { return ctl_val; }

/* (ctl_scn: a recall the full queue refused, -1 none) */
static uint32_t ctl_scn_beats;

static int scene_push(void)
{
    cmd_t c;
    c.op = CMD_SCENE;
    c.beats = (uint8_t)ctl_scn_beats;
    c.id = (uint16_t)ctl_scn;
    c.v = 0;
    if (!cmdq_push(&eng_q, c)) {
        ctl_stats.refused++;
        return 0;
    }
    ctl_stats.sent++;
    ctl_scn = -1;
    return 1;
}

void ctl_scene_store(uint32_t s, uint32_t scope) { scene_capture(s, scope, ctl_val, (const step_t (*)[NSTEPS])ctl_pat); }

void ctl_scene_recall(uint32_t s, uint32_t beats)
{
    static int16_t was[SCENE_DESK];
    uint32_t k, ch, i;
    if (ctl_watch) {
        for (k = 0; k < SCENE_DESK; k++)
            was[k] = ctl_val[scene_param(k)];
        for (ch = 0; ch < NCH; ch++)
            for (i = 0; i < NSTEPS; i++)
                ctl_pat_was[ch][i] = ctl_pat[ch][i];
    }
    scene_apply(s, ctl_val, ctl_pat);            /* the mirror at once (the strips show it cued) */
    if (ctl_watch) {
        for (k = 0; k < SCENE_DESK; k++)
            if (was[k] != ctl_val[scene_param(k)])
                ctl_watch(scene_param(k), was[k], ctl_val[scene_param(k)]);
        pat_diff();
    }
    ctl_scn = (int32_t)(s % NSCENES);
    ctl_scn_beats = beats > 128u ? 128u : beats;
    if (!ctl_stats.pending && !ctl_steps_pending)
        scene_push();
}
