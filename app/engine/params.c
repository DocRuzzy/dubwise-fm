/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* The parameters' ranges, defaults and names (params.h). Part of the unity build: app.c includes it. */
#include "params.h"

typedef char p_ch_fits[(CHF_N <= CH_STRIDE && P_CH_BASE + NCH * CH_STRIDE <= P_BUS_BASE) ? 1 : -1];
typedef char p_voice_fits[(VF_N <= VOICE_STRIDE && P_VOICE_BASE + NCH * VOICE_STRIDE <= P_NIDS) ? 1 : -1];

static const pdesc_t CH_FIELDS[CHF_N] = {
    [CHF_LEVEL] = {"level", 0, 127, 100},           /* 100 = unity (gain (v/100)^2: +4 dB at 127) */
    [CHF_MUTE] = {"mute", 0, 1, 0},
    [CHF_SOLO] = {"solo", 0, 1, 0},
    [CHF_ECHO_SEND] = {"echo", 0, 127, 0},          /* post-fader, post-mute (throws are pre-fader: P0.2) */
    [CHF_SPRING_SEND] = {"spring", 0, 127, 0},
};
static const pdesc_t VOICE_FIELDS[VF_N] = {
    [VF_WAVE] = {"wave", 0, 1, 0},                  /* the template's key voice: 0 sine, 1 triangle */
};
static const pdesc_t GLOBALS[P_NGLOBAL] = {
    [P_TEMPO] = {"tempo", 400, 1800, 750},          /* 40.0 .. 180.0 BPM */
    [P_PLAY] = {"play", 0, 1, 0},
    [P_QUANT] = {"quant", 0, QUANT_N - 1, QUANT_1},
    [P_LEAD] = {"lead", 0, 50, 10},                 /* ms (spec: launch quantise "fired slightly early") */
};
static const pdesc_t DEV_TONES = {"dev.tones", 0, 1, 0};

uint32_t quant_beats(int32_t q)
{
    static const uint8_t B[QUANT_N] = {1, 4, 8, 16, 32, 64, 128};
    return q >= 0 && q < QUANT_N ? B[q] : 4u;
}
const char *quant_name(int32_t q)
{
    static const char *const N[QUANT_N] = {"BEAT", "1 BAR", "2 BARS", "4 BARS", "8 BARS", "16 BARS", "32 BARS"};
    return q >= 0 && q < QUANT_N ? N[q] : "";
}

const pdesc_t *param_desc(uint32_t id)
{
    if (id < P_NGLOBAL)
        return &GLOBALS[id];
    if (id == P_DEV_TONES)
        return &DEV_TONES;
    if (id >= P_CH_BASE && id < P_CH_BASE + NCH * CH_STRIDE && (id - P_CH_BASE) % CH_STRIDE < CHF_N)
        return &CH_FIELDS[(id - P_CH_BASE) % CH_STRIDE];
    if (id >= P_VOICE_BASE && id < P_VOICE_BASE + NCH * VOICE_STRIDE && (id - P_VOICE_BASE) % VOICE_STRIDE < VF_N)
        return &VOICE_FIELDS[(id - P_VOICE_BASE) % VOICE_STRIDE];
    return 0;
}

int32_t param_clamp(uint32_t id, int32_t v)
{
    const pdesc_t *d = param_desc(id);
    if (!d)
        return 0;
    return v < d->min ? d->min : v > d->max ? d->max : v;
}

void param_name(uint32_t id, char *buf, uint32_t n)
{
    const pdesc_t *d = param_desc(id);
    uint32_t i = 0;
    const char *s;
    if (!n)
        return;
    buf[0] = 0;
    if (!d)
        return;
    if (id >= P_CH_BASE && id < P_BUS_BASE) {        /* "ch4." or "v4." before a channel's own */
        uint32_t ch = (id - P_CH_BASE) / CH_STRIDE;
        const char *pre = "ch";
        for (; *pre && i + 1 < n; pre++)
            buf[i++] = *pre;
        if (i + 2 < n) {
            buf[i++] = (char)('1' + ch);
            buf[i++] = '.';
        }
    } else if (id >= P_VOICE_BASE && id < P_NIDS) {
        uint32_t ch = (id - P_VOICE_BASE) / VOICE_STRIDE;
        if (i + 3 < n) {
            buf[i++] = 'v';
            buf[i++] = (char)('1' + ch);
            buf[i++] = '.';
        }
    }
    for (s = d->name; *s && i + 1 < n; s++)
        buf[i++] = *s;
    buf[i] = 0;
}

static int name_eq(const char *a, const char *b)   /* (no libc on the device) */
{
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

int param_find(const char *name)
{
    uint32_t id;
    char t[24];
    for (id = 0; id < P_NIDS; id++) {
        param_name(id, t, sizeof t);
        if (t[0] && name_eq(t, name))
            return (int)id;
    }
    return -1;
}
