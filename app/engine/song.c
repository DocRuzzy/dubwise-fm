/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* song.h. Compiled on its own (fm1.toml o2). */
#include "song.h"
#include "plat.h"

PLAT_POOL song_bar_t song[SONG_BARS];

const char *section_name(int32_t s)
{
    static const char *const N[SECTIONS] = {"INTRO", "VERSE", "CHORUS", "DUB", "DROP", "BREAK", "BRIDGE", "BUILD",
                                            "VOCAL", "OUTRO", "END", "VERSION", "DUBPLATE", "SPECIAL",
                                            "RUB-A-DUB"};
    return s >= 0 && s < SECTIONS ? N[s] : "";
}

void song_capture(uint32_t bar, const int16_t *vals, int32_t scene_this_bar)
{
    song_bar_t *b;
    uint32_t ch;
    if (bar >= SONG_BARS)
        return;
    b = &song[bar];
    b->mute = b->solo = 0;
    for (ch = 0; ch < NCH; ch++) {
        b->mute |= (uint8_t)((vals[P_CH(ch, CHF_MUTE)] != 0) << ch);
        b->solo |= (uint8_t)((vals[P_CH(ch, CHF_SOLO)] != 0) << ch);
    }
    b->hpf = (int8_t)vals[P_HPF_STEP];
    b->pats[0] = b->pats[1] = 0;
    for (ch = 0; ch < NCH; ch++)
        b->pats[ch / 4u] |= (uint8_t)((vals[P_CH(ch, CHF_PAT)] & 3) << (ch % 4u * 2u));
    b->scene = (uint8_t)(scene_this_bar >= 0 ? scene_this_bar + 1 : 0);
    b->flags |= BAR_SET | BAR_PATS;
}

int song_apply(uint32_t bar, int16_t *vals)
{
    const song_bar_t *b;
    uint32_t ch;
    if (bar >= SONG_BARS || !(song[bar].flags & BAR_SET))
        return -1;
    b = &song[bar];
    for (ch = 0; ch < NCH; ch++) {
        vals[P_CH(ch, CHF_MUTE)] = (int16_t)(b->mute >> ch & 1u);
        vals[P_CH(ch, CHF_SOLO)] = (int16_t)(b->solo >> ch & 1u);
    }
    vals[P_HPF_STEP] = (int16_t)param_clamp(P_HPF_STEP, b->hpf);
    if (b->flags & BAR_PATS)
        for (ch = 0; ch < NCH; ch++)
            vals[P_CH(ch, CHF_PAT)] = (int16_t)bar_pat(b, ch);
    return b->scene ? b->scene - 1 : 16;
}
