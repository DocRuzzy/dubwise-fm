/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* fxv.h. Compiled on its own at -O2 (fm1.toml o2). Floats only. */
#include "fxv.h"
#include "params.h"
#include "fastmath.h"
#include "dsp.h"
#include "plat.h"                                /* PLAT_POOL */
#include "oneshots_data.h"

#define SHOT_TOTAL (sizeof SHOT_BIM + sizeof SHOT_HORN + sizeof SHOT_LASER)
static PLAT_POOL uint8_t shot_ram[SHOT_TOTAL];   /* the one-shots, in RAM: the render never reads the flash */
static const uint8_t *shot_at[NSHOTS];
static float ulaw[256];                          /* mu-law -> -1..1 */
static struct {
    uint32_t pos, end;                           /* in half samples (22,050 Hz data at 44.1 kHz) */
    float vel;
    int on, rev;                                 /* rev: backwards (P3.3), from its end to its start */
} shot[NSHOTS];
#define INV_SR (1.0f / 44100.0f)
static inline float lin(int16_t v, float lo, float hi) { return lo + (hi - lo) * (float)v * (1.0f / 127.0f); }

static struct {
    int gate, which;
    float ph, lfo, amp, vel;
    float f, rate, ratio, tri;
} sr;

void fxv_init(void)                             /* (at boot: copies the one-shots into RAM) */
{
    uint32_t k, i, off = 0;
    sr.gate = 0;
    sr.amp = 0.0f;
    for (i = 0; i < 256u; i++) {                 /* the generator's mu-law: y in -1..1 over the codes */
        float y = (float)i * (1.0f / 127.5f) - 1.0f, a = fm_fabsf(y);
        float x = (fm_exp2f(a * 8.0f) - 1.0f) * (1.0f / 255.0f);   /* (1 + 255)^|y| - 1, over 255 */
        ulaw[i] = y < 0.0f ? -x : x;
    }
    for (k = 0; k < NSHOTS; k++) {
        for (i = 0; i < SHOT_LEN[k]; i++)
            shot_ram[off + i] = SHOT_DATA[k][i];
        shot_at[k] = shot_ram + off;
        off += SHOT_LEN[k];
        shot[k].on = 0;
    }
}

void fxv_shot(uint32_t k, uint32_t vel) { fxv_shot_dir(k, vel, 0); }

void fxv_shot_dir(uint32_t k, uint32_t vel, int rev)
{
    if (k >= NSHOTS)
        return;
    shot[k].rev = rev;
    shot[k].pos = 0;
    shot[k].end = (SHOT_LEN[k] - 1u) * 2u;
    shot[k].vel = (float)(vel > 127u ? 127u : vel) * (1.0f / 127.0f);
    shot[k].on = 1;
}

const char *fxv_shot_name(uint32_t k) { return k < NSHOTS ? SHOT_NAME[k] : ""; }

static int shots_render(float *o, uint32_t n, int add)   /* the one-shots into o (added, or written) */
{
    uint32_t k, i;
    int any = 0;
    for (k = 0; k < NSHOTS; k++) {
        const uint8_t *d = shot_at[k];
        if (!shot[k].on)
            continue;
        for (i = 0; i < n; i++) {
            uint32_t p = shot[k].rev ? shot[k].end - shot[k].pos : shot[k].pos, j = p >> 1;
            float v;
            if (p & 1u) {                        /* x2: the half-sample point, 4-point (-1 9 9 -1)/16 */
                float a = j ? ulaw[d[j - 1u]] : 0.0f, b = ulaw[d[j]], c = ulaw[d[j + 1u]];
                float e = j + 2u < SHOT_LEN[k] ? ulaw[d[j + 2u]] : 0.0f;
                v = (9.0f * (b + c) - a - e) * (1.0f / 16.0f);
            } else
                v = ulaw[d[j]];
            v *= shot[k].vel * 0.5f;
            o[i] = (add || any) ? o[i] + v : v;
            if (++shot[k].pos >= shot[k].end) {
                shot[k].on = 0;
                for (i++; i < n; i++)
                    if (!(add || any))
                        o[i] = 0.0f;
                break;
            }
        }
        any = 1;
    }
    return any;
}

void fxv_config(const int16_t *ev)
{
    sr.f = 200.0f * fm_exp2f(lin(ev[P_VOICE(CH_VOX, VX_PITCH)], 0.0f, 2.585f));   /* 200 Hz .. 1.2 kHz */
    sr.rate = 0.5f * fm_exp2f(lin(ev[P_VOICE(CH_VOX, VX_RATE)], 0.0f, 4.585f));   /* 0.5 .. 12 Hz */
    sr.ratio = fm_exp2f(lin(ev[P_VOICE(CH_VOX, VX_DEPTH)], 1.0f, 24.0f) * (1.0f / 12.0f));
    sr.tri = ev[P_VOICE(CH_VOX, VX_WAVE)] ? 1.0f : 0.0f;
}

void fxv_on(int32_t which, uint32_t vel)
{
    sr.which = which;
    sr.vel = (float)(vel > 127u ? 127u : vel) * (1.0f / 127.0f);
    if (!sr.gate)
        sr.lfo = 0.0f;
    sr.gate = 1;
}

void fxv_off(int32_t which)
{
    if (sr.which == which)
        sr.gate = 0;
}

int fxv_render(float *o, uint32_t n)
{
    uint32_t i;
    if (!sr.gate && sr.amp < 1e-5f) {
        sr.amp = 0.0f;
        return shots_render(o, n, 0);
    }
    for (i = 0; i < n; i++) {
        float m, f, s;
        sr.lfo += sr.rate * INV_SR;
        if (sr.lfo >= 1.0f)
            sr.lfo -= 1.0f;
        /* A: two tones, the square's halves; B: a ramp up through DEPTH, again and again */
        m = sr.which == SIREN_A ? (sr.lfo < 0.5f ? 1.0f : sr.ratio) : fm_exp2f(sr.lfo * fm_log2f(sr.ratio));
        f = sr.f * m;
        sr.ph += f * INV_SR;
        if (sr.ph >= 1.0f)
            sr.ph -= 1.0f;
        s = sr.tri > 0.5f ? 4.0f * fm_fabsf(sr.ph - 0.5f) - 1.0f : fm_sin_turns(sr.ph);
        sr.amp += ((sr.gate ? 1.0f : 0.0f) - sr.amp) * (sr.gate ? 0.005f : 0.0008f);   /* ~5 ms in, ~30 ms out */
        sr.amp = undenorm(sr.amp);
        o[i] = s * sr.amp * sr.vel * 0.35f;
    }
    shots_render(o, n, 1);
    return 1;
}
