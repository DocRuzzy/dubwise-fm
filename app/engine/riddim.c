/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* riddim.h. The templates are the mastery guide's D1 grids, 16 steps (one bar of 4/4, "1" = step 1). */
#include "riddim.h"

const char *template_name(int32_t t)
{
    static const char *const N[TPL_N] = {"ONE DROP", "ROCKERS", "STEPPERS", "FLYERS", "DUB TECHNO"};
    return t >= 0 && t < TPL_N ? N[t] : "";
}

int8_t default_note(uint32_t ch)
{
    static const int8_t N[NCH] = {36, 37, 42, 33, VOICING_MID, VOICING_MID, 69, 60};   /* chord channels: a voicing */
    return ch < NCH ? N[ch] : 60;
}

uint32_t lvl_vel(uint32_t lvl)
{
    static const uint8_t V[LVL_N] = {0, 48, 96, 127};
    return lvl < LVL_N ? V[lvl] : 0;
}

/* a bar as text, one char a step: '.' off, 'g' ghost, 'x' normal, 'X' accent; hats: 'o' / 'O' open */
typedef struct {
    const char *kick, *rim, *hat;
    int8_t bass[16];                             /* 0: no step */
    const char *skank, *organ;                   /* 0: the reggae ones (SKANK, BUBBLE) */
} tpl_t;
static const char SKANK[] = "....x.......x...";
static const char BUBBLE[] = "..lh..lh..lh..lh";

static const tpl_t TPL[TPL_N] = {
    /* one drop: kick and cross-stick on beat 3 only; hats in eighths */
    {"........X.......", "........X.......", "x.x.x.x.x.x.x.x.",
     {0, 0, 33, 33, 0, 0, 36, 0, 0, 0, 40, 38, 0, 0, 36, 0}, 0, 0},
    /* rockers: kick on 1 and 3 with a pickup, rim on 3 with Sly's double tap, busier hats */
    {"X.......X.....x.", ".......gX.......", "x.xgx.xgx.xgx.xg",
     {0, 0, 33, 0, 33, 0, 36, 0, 0, 0, 40, 0, 38, 0, 36, 31}, 0, 0},
    /* steppers: kick on every beat, rim on 3, hats on the off-beats */
    {"X...X...X...X...", "........X.......", "..x...x...x...x.",
     {33, 0, 0, 33, 0, 0, 36, 0, 33, 0, 0, 40, 0, 38, 0, 0}, 0, 0},
    /* flyers: the flying cymbal: open on the "and", choked by a closed hit on the beat; one drop underneath */
    {"........X.......", "........X.......", "x.o.x.o.x.o.x.o.",
     {0, 0, 33, 33, 0, 0, 36, 0, 0, 0, 40, 38, 0, 0, 36, 0}, 0, 0},
    /* dub techno (P3.4; the mastery guide's Basic Channel lineage): the kick on every beat, a ghost rim on 2 and 4,
     * open hats on the off-beats, the chord stab on the off-beats too (into a long echo: the stab is the dub), a sub
     * bass on 1 and the "and" of 3; no bubble */
    {"X...X...X...X...", "....g.......g...", "..o...o...o...o.",
     {33, 0, 0, 0, 0, 0, 0, 0, 0, 0, 33, 0, 0, 0, 0, 0}, "..x...x...x...x.", "................"},
};

static uint8_t lvl_of(char c)
{
    return c == 'X' || c == 'O' ? LVL_ACCENT : c == 'x' || c == 'o' ? LVL_NORMAL : c == 'g' ? LVL_GHOST : LVL_OFF;
}

void template_fill(int32_t t, uint32_t ch, step_t *dst)
{
    uint32_t i;
    const tpl_t *p = &TPL[t >= 0 && t < TPL_N ? t : 0];
    for (i = 0; i < NSTEPS; i++) {
        uint32_t k = i % 16u;
        dst[i].lvl = LVL_OFF;
        dst[i].note = default_note(ch);
        if (ch == CH_KICK)
            dst[i].lvl = lvl_of(p->kick[k]);
        else if (ch == CH_SNARE)
            dst[i].lvl = lvl_of(p->rim[k]);
        else if (ch == CH_HATS) {
            dst[i].lvl = lvl_of(p->hat[k]);
            dst[i].note = p->hat[k] == 'o' || p->hat[k] == 'O' ? 46 : 42;
        } else if (ch == CH_BASS && p->bass[k]) {
            dst[i].lvl = LVL_NORMAL;
            dst[i].note = p->bass[k];
        } else if (ch == CH_SKANK) {             /* the skank on 2 and 4 (D2), a chop */
            dst[i].lvl = lvl_of((p->skank ? p->skank : SKANK)[k]);
        } else if (ch == CH_ORGAN) {             /* the bubble: low on the off-beat eighths, high on the sixteenth after */
            const char *o = p->organ ? p->organ : BUBBLE;
            dst[i].lvl = o[k] == '.' ? LVL_OFF : o[k] == 'h' ? LVL_GHOST : LVL_NORMAL;
            dst[i].note = o[k] == 'h' ? VOICING_HI : VOICING_LO;
        }
    }
}
