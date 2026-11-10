/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors
 *
 * After SPELLBOX's speech chip and its text-to-speech intonation (src/speech.c, src/tts.c; GPL-3.0-only,
 * https://github.com/pjac1810/spellbox): the frame decoder, the interpolation, the chirp and noise excitation, the
 * 10-stage lattice and its 8-bit DAC, the STUTTER and CRUSH bends, the falling intonation with stressed vowels up.
 * Its phonemes (lpc_data.h, tools/vox/mklpc.py) are espeak-ng's speech coded by SPELLBOX's encoder (GPL-3.0). */
/* The deejay voice (roadmap 6.6; lpc.h): Dubwise's shouts (SELECTA, WHEEL UP, BIG UP ...) said by a 1978-style LPC
 * speech chip at 8 kHz, each a string of phonemes with an intonation (85 Hz falling to 71 over the shout, a stressed
 * vowel a step up, the last one falling), raised by PITCH semitones. Played on channel 8 in place of black 8's BIM when
 * VOX's SAY picks a shout. All its tables in the pool (copied at boot: the render never reads the flash). Integers, as
 * SPELLBOX's chip (no floats needed until the output). Compiled on its own at -O2 */
#include "lpc.h"
#include "lpc_data.h"
#include "plat.h"                                /* PLAT_POOL */

#define LPC_HZ 8000u                             /* the chip's clock */
#define LPC_PH_MAX 24u                           /* a shout's phonemes at most */
static const uint8_t KBITS[10] = LPC_KBITS;
static PLAT_POOL uint8_t bitsr[sizeof LPC_PH_BITS];   /* (the flash's, copied) */
static PLAT_POOL int16_t ktab[10][32];
static PLAT_POOL uint16_t ener[16], ph_at[43];
static PLAT_POOL uint8_t perd[32], sh_at[LPC_SHOUTS + 1], sh_ph[sizeof LPC_SH_PH];
static PLAT_POOL int8_t chirp[41];
static PLAT_POOL char sh_name[LPC_SHOUTS][9];

static struct {
    uint8_t ph[LPC_PH_MAX];                      /* the shout's phonemes, and each vowel's period (x 16; 0 a consonant) */
    uint16_t per[LPC_PH_MAX];
    uint32_t nph, iph;                           /* how many, the one playing */
    uint32_t bitpos, end;                        /* in bitsr: the frame being read; the phoneme's end */
    uint32_t frame_start[2];
    int32_t tts_per;                             /* the vowel's period x 16 (0: the frame's own) */
    int32_t e_old, e_new, p_old, p_new, k_old[10], k_new[10], e_cur, p_cur, k_cur[10], b[11];
    uint32_t pc, n8, acc, frames;
    uint16_t lfsr;
    int32_t deemph, dac;
    uint8_t talking, stopping;
    uint32_t rng;
    int semis, stutter, crush;
    float vel, hx, hy, lp;                       /* the output: its level, the speaker's high-pass and low-pass */
} v;

/* 2^(-n/12) x 4096, n = -12..12 (SPELLBOX's SEMI) */
static const uint16_t SEMI[25] = {8192, 7732, 7298, 6889, 6502, 6137, 5793, 5468, 5161, 4871, 4598, 4340, 4096,
                                  3866, 3649, 3444, 3251, 3069, 2896, 2734, 2580, 2436, 2299, 2170, 2048};
static PLAT_POOL uint16_t semi[25];

void lpc_init(void)
{
    uint32_t i, k;
    static const int16_t *const K[10] = {LPC_K1, LPC_K2, LPC_K3, LPC_K4, LPC_K5, LPC_K6, LPC_K7, LPC_K8, LPC_K9, LPC_K10};
    for (i = 0; i < sizeof LPC_PH_BITS; i++)
        bitsr[i] = LPC_PH_BITS[i];
    for (k = 0; k < 10u; k++)
        for (i = 0; i < (1u << KBITS[k]); i++)
            ktab[k][i] = K[k][i];
    for (i = 0; i < 16u; i++)
        ener[i] = LPC_ENERGY[i];
    for (i = 0; i < 32u; i++)
        perd[i] = LPC_PERIOD[i];
    for (i = 0; i < 41u; i++)
        chirp[i] = LPC_CHIRP[i];
    for (i = 0; i < 43u; i++)
        ph_at[i] = LPC_PH_AT[i];
    for (i = 0; i <= LPC_SHOUTS; i++)
        sh_at[i] = LPC_SH_AT[i];
    for (i = 0; i < sizeof LPC_SH_PH; i++)
        sh_ph[i] = LPC_SH_PH[i];
    for (i = 0; i < LPC_SHOUTS; i++)
        for (k = 0; k < 9u; k++)
            sh_name[i][k] = LPC_SH_NAME[i][k];
    for (i = 0; i < 25u; i++)
        semi[i] = SEMI[i];
    v.talking = 0;
    v.lfsr = 1u;
}

const char *lpc_name(int32_t k) { return k >= 0 && k < LPC_SHOUTS ? sh_name[k] : ""; }
uint32_t lpc_count(void) { return LPC_SHOUTS; }
int lpc_talking(void) { return v.talking; }

static uint32_t rnd(void)
{
    v.rng ^= v.rng << 13, v.rng ^= v.rng >> 17, v.rng ^= v.rng << 5;
    return v.rng;
}

static uint32_t getbits(int n)
{
    uint32_t x = 0;
    int i;
    for (i = 0; i < n; i++, v.bitpos++)
        x = x << 1 | (uint32_t)(bitsr[v.bitpos >> 3] >> (7u - (v.bitpos & 7u)) & 1u);
    return x;
}

static int ph_start(void)                        /* the next phoneme of the shout; 0: none left */
{
    uint32_t p;
    if (v.iph >= v.nph)
        return 0;
    p = v.ph[v.iph] & 0x3Fu;
    v.bitpos = ph_at[p];
    v.end = ph_at[p + 1u];
    if (v.per[v.iph])
        v.tts_per = v.per[v.iph];
    v.iph++;
    return 1;
}

static int next_frame(void)                      /* 0: it went on; 1: the shout is over */
{
    uint32_t e, rep, p, i;
    for (i = 0; i < 10u; i++)
        v.k_old[i] = v.k_new[i];
    v.e_old = v.e_new, v.p_old = v.p_new;
    if (++v.frames > 76u)                        /* (1.9 s at most, whatever STUTTER does: SOUND SYSTEM, the longest, 1.75 s) */
        return 1;
    if (v.stutter > 0 && v.frame_start[1] && (int32_t)(rnd() >> 24) < v.stutter)
        v.bitpos = v.frame_start[1];             /* STUTTER: back a frame now and then (SPELLBOX's; at most half the
                                                  * frames here, so a shout always ends) */
    v.frame_start[1] = v.frame_start[0];
    v.frame_start[0] = v.bitpos;
    for (;;) {                                   /* (a phoneme's end, or its bits run out: the next one, joined) */
        if (v.bitpos + 4u <= v.end) {
            e = getbits(4);
            if (e != 15u)
                break;
        }
        if (!ph_start())
            return 1;
        v.frame_start[0] = v.bitpos;
    }
    if (e == 0u) {
        v.e_new = 0;
        return 0;
    }
    rep = getbits(1), p = getbits(5);
    v.e_new = ener[e];
    if (p) {
        int32_t per = v.tts_per ? v.tts_per : (int32_t)perd[p] * 16;
        int s = v.semis < -12 ? -12 : v.semis > 12 ? 12 : v.semis;
        per = (per * (int32_t)semi[s + 12]) >> 12;
        v.p_new = per < 32 ? 32 : per;
    } else
        v.p_new = 0;
    if (!rep) {
        int n = p ? 10 : 4;
        for (i = 0; i < 10u; i++)
            v.k_new[i] = (int)i < n ? ktab[i][getbits(KBITS[i])] : 0;
    }
    return 0;
}

static int32_t chip(void)                        /* one sample of the chip at 8 kHz (SPELLBOX's) */
{
    int32_t x, u, d;
    int i;
    uint32_t frame_len = 200u, step_len = 25u;   /* (25 ms frames: tts_say's pace) */
    if (!v.talking)
        return v.dac = 0;
    if (v.n8 == 0u) {
        if (v.stopping) {
            v.talking = 0;
            return v.dac = 0;
        }
        if (next_frame())
            v.e_new = 0, v.p_new = v.p_old, v.stopping = 1;
    }
    if (v.n8 % step_len == 0u) {                 /* interpolated, eight steps a frame (not across voicing) */
        int32_t st = (int32_t)(v.n8 / step_len) + 1;
        int smooth = (v.p_old != 0) == (v.p_new != 0) && v.e_old && v.e_new;
        if (st > 8)
            st = 8;
        if (smooth) {
            v.e_cur = v.e_old + (v.e_new - v.e_old) * st / 8;
            v.p_cur = v.p_old + (v.p_new - v.p_old) * st / 8;
            for (i = 0; i < 10; i++)
                v.k_cur[i] = v.k_old[i] + (v.k_new[i] - v.k_old[i]) * st / 8;
        } else {
            v.e_cur = st >= 2 || !v.e_old ? v.e_new : v.e_old / 2;
            v.p_cur = v.p_new;
            for (i = 0; i < 10; i++)
                v.k_cur[i] = v.k_new[i];
        }
    }
    if (++v.n8 >= frame_len)
        v.n8 = 0;
    if (v.p_cur) {                               /* the excitation: a chirp each pitch period, or noise */
        uint32_t k;
        v.pc += 16u;
        if (v.pc >= (uint32_t)v.p_cur)
            v.pc -= (uint32_t)v.p_cur;
        k = v.pc >> 4;
        x = k < 41u ? (chirp[k] * v.e_cur) >> 7 : 0;
    } else {
        v.lfsr = (uint16_t)((v.lfsr >> 1) ^ (-(v.lfsr & 1u) & 0xB400u));
        x = (v.lfsr & 1u) ? v.e_cur : -v.e_cur;
        v.pc = 0;
    }
    u = x * 8;                                   /* the lattice */
    for (i = 9; i >= 0; i--) {
        int32_t nb;
        u -= (v.k_cur[i] * v.b[i]) >> 14;
        u = u > 65535 ? 65535 : u < -65535 ? -65535 : u;
        nb = v.b[i] + ((v.k_cur[i] * u) >> 14);
        v.b[i + 1] = nb > 65535 ? 65535 : nb < -65535 ? -65535 : nb;
    }
    v.b[0] = u;
    v.deemph = u + ((v.deemph * 29) >> 5);       /* a little of the low end back */
    d = v.deemph >> 8;
    d = d > 127 ? 127 : d < -128 ? -128 : d;     /* the 8-bit DAC; CRUSH: its low bits gone */
    d = (d >> v.crush) * (1 << v.crush);
    return v.dac = d;
}

void lpc_say(int32_t k, uint32_t vel, int semis, int stutter, int crush)
{
    uint32_t a, n, j, np, stressed = 0, s = 0;
    int32_t last = -1;
    if (k < 0 || k >= LPC_SHOUTS)
        return;
    a = sh_at[k], n = sh_at[k + 1] - a;
    np = n > LPC_PH_MAX ? LPC_PH_MAX : n;
    for (j = 0; j < np; j++) {
        v.ph[j] = sh_ph[a + j];
        if (v.ph[j] & LPC_STRESS)
            stressed++, last = (int32_t)j;
    }
    for (j = 0; j < np; j++) {                   /* tts_say's intonation: 85 Hz falling to 71, stressed up a step */
        uint32_t p = v.ph[j] & 0x3Fu;
        v.per[j] = 0;
        if (p < LPC_PH_VOWELS) {
            int32_t hz = 85 - 14 * (int32_t)j / (int32_t)np;
            if (v.ph[j] & LPC_STRESS) {
                hz = (int32_t)j == last && stressed > 1 ? hz * 9 / 10 : hz * 115 / 100 - (int32_t)s * 3;
                s++;
            }
            v.per[j] = (uint16_t)(LPC_HZ * 16u / (uint32_t)hz);
        }
    }
    v.nph = np, v.iph = 0, v.tts_per = 0;
    for (j = 0; j < 10u; j++)
        v.k_new[j] = 0;
    for (j = 0; j < 11u; j++)
        v.b[j] = 0;
    v.frame_start[0] = v.frame_start[1] = 0;
    v.e_new = v.p_new = 0;
    v.n8 = 0, v.acc = 0, v.deemph = 0, v.stopping = 0, v.pc = 0, v.frames = 0;
    v.rng = 0x6C8E9CF5u + (uint32_t)k;           /* (STUTTER's: the same each time: a replay the same) */
    v.semis = semis, v.stutter = stutter, v.crush = crush < 0 ? 0 : crush > 5 ? 5 : crush;
    v.vel = (float)(vel > 127u ? 127u : vel) * (1.0f / 127.0f);
    if (!ph_start())
        return;
    v.talking = 1;
    next_frame();
    v.e_old = 0;
}

int lpc_render(float *o, uint32_t n, int add)    /* 44.1 kHz: the chip held between its 8 kHz samples, then the
                                                  * speaker's high-pass and soft top (SPELLBOX's); 1 if it sounded */
{
    uint32_t i, step = (LPC_HZ << 16) / 44100u;
    if (!v.talking && v.hy == 0.0f && v.lp == 0.0f)
        return 0;
    for (i = 0; i < n; i++) {
        float s, hp;
        v.acc += step;
        while (v.acc >= 65536u)
            v.acc -= 65536u, chip();
        s = (float)v.dac * (220.0f / 32768.0f) * v.vel;
        hp = s - v.hx + v.hy * 0.96741f;         /* (31700 / 32768: no bass) */
        v.hx = s, v.hy = hp;
        v.lp += (hp - v.lp) * 0.521f;            /* ((3000 + 64 x 220) / 32768: its top) */
        if (!v.talking && v.lp < 1e-6f && v.lp > -1e-6f && v.hy < 1e-6f && v.hy > -1e-6f)
            v.hx = v.hy = v.lp = 0.0f;
        o[i] = add ? o[i] + 1.8f * v.lp : 1.8f * v.lp;   /* (as loud as the one-shots) */
    }
    return 1;
}
