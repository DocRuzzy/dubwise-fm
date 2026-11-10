/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* fxv.h. Compiled on its own at -O2 (fm1.toml o2). Floats only. */
#include "fxv.h"
#include "params.h"
#include "fastmath.h"
#include "dsp.h"
#include "plat.h"                                /* PLAT_POOL */
#include "oneshots_data.h"
#include "lpc.h"                                 /* 6.6: the deejay voice */

#define SHOT_TOTAL (sizeof SHOT_BIM + sizeof SHOT_HORN + sizeof SHOT_LASER)
typedef char shots_fit[SHOT_TOTAL <= FXV_SHOT_MAX ? 1 : -1];
static PLAT_POOL uint8_t shot_ram[FXV_SHOT_MAX];   /* the one-shots, in RAM: the render never reads the flash */
static const uint8_t *shot_at[NSHOTS];
static uint32_t shot_len[NSHOTS];                /* (the factory's, or the owner's: 5.14) */
static char shot_name[NSHOTS][9];
static volatile int shots_held;                  /* 1: being replaced (main loop): none starts or plays */
static float ulaw[256];                          /* mu-law -> -1..1 */
static int say_k, say_semis, say_stutter, say_crush;   /* 6.6: VOX's SAY (0: BIM), and its bends */
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
    int njd;                                     /* 7.1: the NJD siren's shape */
    float x, lp1, lp2, dcx, dcy;                 /* (its RC: the pitch's capacitor, the two output low-passes, the
                                                  * output's coupling capacitor) */
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
        shot_len[k] = SHOT_LEN[k];
        for (i = 0; i < 8u && SHOT_NAME[k][i]; i++)
            shot_name[k][i] = SHOT_NAME[k][i];
        shot_name[k][i] = 0;
        off += SHOT_LEN[k];
        shot[k].on = 0;
    }
    shots_held = 0;
    lpc_init();
}

/* The owner's one-shots (5.14; app/shots.c): fxv_shots_hold stops them and keeps them from starting, the app copies
 * the new data into fxv_shots_ram, then fxv_shots_use gives each its place, length and name, and lets them play
 * (main loop) */
uint8_t *fxv_shots_ram(void) { return shot_ram; }
void fxv_shots_hold(void)
{
    uint32_t k;
    shots_held = 1;
    __asm__ volatile("" ::: "memory");
    for (k = 0; k < NSHOTS; k++)
        shot[k].on = 0;
}
void fxv_shots_use(const uint32_t off[NSHOTS], const uint32_t len[NSHOTS], const char *const name[NSHOTS])
{
    uint32_t k, i;
    for (k = 0; k < NSHOTS; k++) {
        int ok = off[k] <= FXV_SHOT_MAX && len[k] <= FXV_SHOT_MAX - off[k];
        shot_at[k] = shot_ram + (ok ? off[k] : 0u);
        shot_len[k] = ok ? len[k] : 0u;
        for (i = 0; i < 8u && name[k][i]; i++)
            shot_name[k][i] = name[k][i];
        shot_name[k][i] = 0;
    }
    __asm__ volatile("" ::: "memory");
    shots_held = 0;
}
void fxv_shots_factory(void)                     /* (main loop) the built-in ones back */
{
    uint32_t k, i, off = 0, o[NSHOTS], l[NSHOTS];
    const char *nm[NSHOTS];
    fxv_shots_hold();
    for (k = 0; k < NSHOTS; k++) {
        for (i = 0; i < SHOT_LEN[k]; i++)
            shot_ram[off + i] = SHOT_DATA[k][i];
        o[k] = off, l[k] = SHOT_LEN[k], nm[k] = SHOT_NAME[k];
        off += SHOT_LEN[k];
    }
    fxv_shots_use(o, l, nm);
}

void fxv_shot(uint32_t k, uint32_t vel) { fxv_shot_dir(k, vel, 0); }

void fxv_shot_dir(uint32_t k, uint32_t vel, int rev)
{
    if (k == 0u && say_k > 0) {                  /* 6.6: black 8 says the shout SAY picked */
        lpc_say(say_k - 1, vel, say_semis, say_stutter, say_crush);
        return;
    }
    if (k >= NSHOTS || shots_held || shot_len[k] < 2u)   /* (an empty one of the owner's: nothing) */
        return;
    shot[k].rev = rev;
    shot[k].pos = 0;
    shot[k].end = (shot_len[k] - 1u) * 2u;
    shot[k].vel = (float)(vel > 127u ? 127u : vel) * (1.0f / 127.0f);
    shot[k].on = 1;
}

const char *fxv_shot_name(uint32_t k) { return k == 0u && say_k > 0 ? lpc_name(say_k - 1) : k < NSHOTS ? shot_name[k] : ""; }
uint32_t fxv_shot_len(uint32_t k) { return k < NSHOTS ? shot_len[k] : 0u; }

static int shots_render(float *o, uint32_t n, int add)   /* the one-shots into o (added, or written) */
{
    uint32_t k, i;
    int any = 0;
    if (shots_held)
        return 0;
    for (k = 0; k < NSHOTS; k++) {
        const uint8_t *d = shot_at[k];
        if (!shot[k].on)
            continue;
        for (i = 0; i < n; i++) {
            uint32_t p = shot[k].rev ? shot[k].end - shot[k].pos : shot[k].pos, j = p >> 1;
            float v;
            if (p & 1u) {                        /* x2: the half-sample point, 4-point (-1 9 9 -1)/16 */
                float a = j ? ulaw[d[j - 1u]] : 0.0f, b = ulaw[d[j]], c = ulaw[d[j + 1u]];
                float e = j + 2u < shot_len[k] ? ulaw[d[j + 2u]] : 0.0f;
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
    sr.njd = ev[P_VOICE(CH_VOX, VX_SHAPE)] != 0;
    say_k = ev[P_VOICE(CH_VOX, VX_SAY)];
    say_semis = ev[P_VOICE(CH_VOX, VX_SAYPITCH)];
    say_stutter = ev[P_VOICE(CH_VOX, VX_STUTTER)];
    say_crush = ev[P_VOICE(CH_VOX, VX_CRUSH)];
}

void fxv_on(int32_t which, uint32_t vel)
{
    sr.which = which;
    sr.vel = (float)(vel > 127u ? 127u : vel) * (1.0f / 127.0f);
    if (!sr.gate)
        sr.lfo = 0.0f;
    if (!sr.gate && sr.amp < 1e-5f) {
        sr.ph = 0.0f;                            /* from silence: the tone from 0 too (5.1: a replay the same) */
        sr.x = sr.lp1 = sr.lp2 = sr.dcx = sr.dcy = 0.0f;
    }
    sr.gate = 1;
}

void fxv_off(int32_t which)
{
    if (sr.which == which)
        sr.gate = 0;
}

static inline float blep(float t, float dt)     /* PolyBLEP: a step's correction near an edge at t = 0 */
{
    if (t < dt) {
        t /= dt;
        return t + t - t * t - 1.0f;
    }
    if (t > 1.0f - dt) {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

int fxv_render(float *o, uint32_t n)
{
    uint32_t i;
    float kc = 0.0f, kd = 0.0f, lr = 0.0f;
    if (!sr.gate && sr.amp < 1e-5f) {
        int any;
        sr.amp = 0.0f;
        any = shots_render(o, n, 0);
        return lpc_render(o, n, any) | any;      /* (6.6: the voice, on top) */
    }
    if (sr.njd) {                                /* 7.1: the NJD's RC, per block (the rate's time constants) */
        float per = 1.0f / sr.rate;              /* (the LFO's period, s) */
        kc = 1.0f - fm_exp2f(-INV_SR / (0.04f * per) * 1.4427f);   /* charge: fast (a diode), 4 % of the period */
        kd = 1.0f - fm_exp2f(-INV_SR / (0.25f * per) * 1.4427f);   /* fall: slow, through the resistors */
        lr = fm_log2f(sr.ratio);
    }
    for (i = 0; i < n; i++) {
        float m, f, s;
        sr.lfo += sr.rate * INV_SR;
        if (sr.lfo >= 1.0f)
            sr.lfo -= 1.0f;
        if (sr.njd) {
            /* NJD (Zen Instruments' redraw of its circuit): A, the LFO's square charges a capacitor fast and lets it
             * fall slowly (an uneven sweep between the two tones); B, each period the capacitor charges from empty,
             * an RC approach (quick, then easing into the top), not a straight ramp */
            if (sr.which == SIREN_A)
                sr.x += ((sr.lfo < 0.5f ? 1.0f : 0.0f) - sr.x) * (sr.lfo < 0.5f ? kc : kd);
            else
                sr.x = (1.0f - fm_exp2f(-4.328f * sr.lfo)) * 1.0526f;   /* (1 - e^-3t) / (1 - e^-3) */
            m = fm_exp2f(sr.x * lr);
        } else                                   /* A: two tones, the square's halves; B: a ramp up through DEPTH */
            m = sr.which == SIREN_A ? (sr.lfo < 0.5f ? 1.0f : sr.ratio) : fm_exp2f(sr.lfo * fm_log2f(sr.ratio));
        f = sr.f * m;
        sr.ph += f * INV_SR;
        if (sr.ph >= 1.0f)
            sr.ph -= 1.0f;
        if (sr.njd) {                            /* a transistor astable's pulse, its width moving with the pitch, then
                                                  * two RC low-passes (~2.4 kHz) */
            float pw = 0.5f - 0.3f * sr.x, dt = f * INV_SR, q = sr.ph - pw;
            if (q < 0.0f)
                q += 1.0f;
            s = (sr.ph < pw ? 1.0f : -1.0f) + blep(sr.ph, dt) - blep(q, dt);   /* (PolyBLEP: no aliasing edges) */
            sr.lp1 += (s - sr.lp1) * 0.29f;
            sr.lp2 += (sr.lp1 - sr.lp2) * 0.29f;
            sr.dcy = sr.lp2 - sr.dcx + 0.997f * sr.dcy;   /* (the coupling capacitor, ~20 Hz: the pulse's DC out) */
            sr.dcx = sr.lp2;
            s = sr.dcy * 1.2f;
        } else
            s = sr.tri > 0.5f ? 4.0f * fm_fabsf(sr.ph - 0.5f) - 1.0f : fm_sin_turns(sr.ph);
        sr.amp += ((sr.gate ? 1.0f : 0.0f) - sr.amp) * (sr.gate ? 0.005f : 0.0008f);   /* ~5 ms in, ~30 ms out */
        sr.amp = undenorm(sr.amp);
        o[i] = s * sr.amp * sr.vel * 0.35f;
    }
    shots_render(o, n, 1);
    lpc_render(o, n, 1);
    return 1;
}
