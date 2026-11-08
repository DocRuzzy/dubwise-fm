/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* riddim.h. The templates are the mastery guide's D1 grids, 16 steps (one bar of 4/4, "1" = step 1). */
#include "riddim.h"

const char *template_name(int32_t t)
{
    static const char *const N[TPL_N] = {"ONE DROP", "ROCKERS", "STEPPERS", "FLYERS"};
    return t >= 0 && t < TPL_N ? N[t] : "";
}

int8_t default_note(uint32_t ch)
{
    static const int8_t N[NCH] = {36, 37, 42, 33, 60, 60, 60, 60};
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
} tpl_t;

static const tpl_t TPL[TPL_N] = {
    /* one drop: kick and cross-stick on beat 3 only; hats in eighths */
    {"........X.......", "........X.......", "x.x.x.x.x.x.x.x.",
     {0, 0, 33, 33, 0, 0, 36, 0, 0, 0, 40, 38, 0, 0, 36, 0}},
    /* rockers: kick on 1 and 3 with a pickup, rim on 3 with Sly's double tap, busier hats */
    {"X.......X.....x.", ".......gX.......", "x.xgx.xgx.xgx.xg",
     {0, 0, 33, 0, 33, 0, 36, 0, 0, 0, 40, 0, 38, 0, 36, 31}},
    /* steppers: kick on every beat, rim on 3, hats on the off-beats */
    {"X...X...X...X...", "........X.......", "..x...x...x...x.",
     {33, 0, 0, 33, 0, 0, 36, 0, 33, 0, 0, 40, 0, 38, 0, 0}},
    /* flyers: the flying cymbal: open on the "and", choked by a closed hit on the beat; one drop underneath */
    {"........X.......", "........X.......", "x.o.x.o.x.o.x.o.",
     {0, 0, 33, 33, 0, 0, 36, 0, 0, 0, 40, 38, 0, 0, 36, 0}},
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
        }
    }
}
