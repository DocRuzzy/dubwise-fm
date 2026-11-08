/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* scenes.h. Compiled on its own (fm1.toml o2). */
#include "scenes.h"
#include "plat.h"                                /* PLAT_POOL */

PLAT_POOL scene_t scenes[NSCENES];

static const uint8_t CH_F[SCENE_CH_FIELDS] = {CHF_LEVEL, CHF_MUTE, CHF_SOLO, CHF_ECHO_SEND, CHF_SPRING_SEND,
                                              CHF_THROW_DEST};
static const uint16_t GL[SCENE_GLOBALS] = {P_HPF_STEP, P_ECHO_RET, P_SPRING_RET};

uint32_t scene_param(uint32_t k)
{
    if (k < NCH * SCENE_CH_FIELDS)
        return P_CH(k / SCENE_CH_FIELDS, CH_F[k % SCENE_CH_FIELDS]);
    return GL[(k - NCH * SCENE_CH_FIELDS) % SCENE_GLOBALS];
}

void scene_capture(uint32_t s, uint32_t scope, const int16_t *vals, const step_t (*pat)[NSTEPS])
{
    scene_t *sc = &scenes[s % NSCENES];
    uint32_t k, ch, i;
    sc->used = 0;                                /* (half-written: not recallable) */
    for (k = 0; k < SCENE_DESK; k++)
        sc->desk[k] = vals[scene_param(k)];
    sc->scope = (uint8_t)(scope < SCOPE_N ? scope : SCOPE_DESK);
    if (sc->scope == SCOPE_PATTERNS)
        for (ch = 0; ch < NCH; ch++) {
            sc->len[ch] = (uint8_t)vals[P_CH(ch, CHF_LEN)];
            for (i = 0; i < NSTEPS; i++)
                sc->pat[ch][i] = pat[ch][i];
        }
    sc->used = 1;
}

void scene_apply(uint32_t s, int16_t *vals, step_t (*pat)[NSTEPS])
{
    const scene_t *sc = &scenes[s % NSCENES];
    uint32_t k, ch, i;
    if (!sc->used)
        return;
    for (k = 0; k < SCENE_DESK; k++)
        vals[scene_param(k)] = (int16_t)param_clamp(scene_param(k), sc->desk[k]);
    if (sc->scope == SCOPE_PATTERNS)
        for (ch = 0; ch < NCH; ch++) {
            vals[P_CH(ch, CHF_LEN)] = (int16_t)param_clamp(P_CH(ch, CHF_LEN), sc->len[ch]);
            for (i = 0; i < NSTEPS; i++)
                if (sc->pat[ch][i].lvl < LVL_N)
                    pat[ch][i] = sc->pat[ch][i];
        }
}
