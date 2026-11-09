/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* chordv.h. Compiled on its own at -O2 (fm1.toml o2). Floats only. */
#include "chordv.h"
#include "params.h"
#include "fastmath.h"
#include "dsp.h"
#include "plat.h"                                /* PLAT_POOL */

#define SR 44100.0f
#define INV_SR (1.0f / 44100.0f)
#define NV 3

static inline float t60k(float ms) { return fm_exp2f(-9.966f / (ms * 0.001f * SR)); }
static inline float midi_hz(int32_t n) { return 440.0f * fm_exp2f((float)(n - 69) * (1.0f / 12.0f)); }
static inline float wrap(float p) { return p >= 1.0f ? p - 1.0f : p; }
static inline float lin(int16_t v, float lo, float hi) { return lo + (hi - lo) * (float)v * (1.0f / 127.0f); }

typedef struct {
    float pc, pm, f, ienv, amp;
} cv_t;

static struct {                                  /* the skank */
    cv_t v[NV];
    float vel, index, ik, ak, ratio, hp_a, lp_a, hx, hy, ly, aenv, arate;   /* aenv: ATTACK's swell (5.3) */
    int on;
} sk;

static struct {                                  /* the organ */
    cv_t v[NV];
    float p2[NV], p3[NV], p4[NV], pk[NV];        /* the partials' phases */
    float vel, w2, w3, w4, ak, click, cenv, ck, det;   /* det: DETUNE's spread (5.3), 0 none */
    int on;
} og;

/* ----------------------------------------------------------------------------------------- the B-3 (5.15) --- */
/* Each drawbar a whole multiple of the 16' pitch, so a registration is one periodic wave: three band-limited tables
 * (all nine harmonics; up to 8x; up to 4x) a note reads at its 16' pitch, interpolated. Percussion (2nd or 3rd,
 * single-trigger), the scanner (vibrato / chorus: a modulated ~1 ms delay), drive, then the rotary: an 800 Hz split,
 * the horn a Doppler delay with AM (two mics: the side for the stereo horn), the drum AM, each rotor gliding to its
 * speed. All at the organ's half rate (22,050 Hz) */
#define B3_N 512u
#define B3_HR (SR * 0.5f)
static PLAT_POOL int16_t b3tab[2][3][B3_N];
static volatile uint32_t b3cur;                  /* the pair's half the render reads */
static float b3gain = 1.0f;
static const uint8_t B3_H[9] = {1, 3, 2, 4, 6, 8, 10, 12, 16};   /* 16' 5 1/3' 8' 4' 2 2/3' 2' 1 3/5' 1 1/3' 1' */

void chordv_b3_build(const int8_t bars[9])      /* (main loop) the registration into the half not being read */
{
    static PLAT_POOL float acc[B3_N];
    uint32_t b, i, k, w = b3cur ^ 1u;
    float lv[9], sum = 0.0f;
    for (k = 0; k < 9u; k++) {                   /* a step about 3 dB, 8 full, 0 out */
        lv[k] = bars[k] > 0 ? fm_exp2f((float)(bars[k] - 8) * 0.5f) : 0.0f;
        sum += lv[k];
    }
    for (b = 0; b < 3u; b++) {
        uint32_t top = b == 0u ? 16u : b == 1u ? 8u : 4u;
        float peak = 1e-6f;
        for (i = 0; i < B3_N; i++) {
            float s = 0.0f, x = (float)i * (1.0f / (float)B3_N);
            for (k = 0; k < 9u; k++)
                if (lv[k] > 0.0f && B3_H[k] <= top)
                    s += lv[k] * fm_sin_turns(x * (float)B3_H[k] - (float)(int32_t)(x * (float)B3_H[k]));
            acc[i] = s;
            peak = fm_maxf(peak, fm_fabsf(s));
        }
        for (i = 0; i < B3_N; i++)
            b3tab[w][b][i] = (int16_t)(acc[i] / peak * 32000.0f);
    }
    b3gain = sum > 0.0f ? fm_minf(1.0f, 0.35f + sum * 0.12f) : 0.0f;   /* (more drawbars out: louder, as on the
                                                                         * real one) */
    __asm__ volatile("" ::: "memory");
    b3cur = w;
}

static struct {
    int model;
    int held;
    uint32_t pn;                                 /* percussion strikes (tests) */                                    /* the sequencer holds the chord (its gate); a pad's dies at once */
    float ph[NV], pph[NV], penv, pk, plev, pmul;    /* the notes' phases; the percussion's */
    int perc;                                    /* PERC_* */
    float vdepth, vmix;                          /* the scanner: depth (samples), 1 vibrato / 0.5 chorus, 0 off */
    float vph, drive;
    int rot;                                     /* ROT_* */
    float hf, df, hph, dph, lp, hk, dk, sl1, sl2;          /* the rotors' speeds (Hz), phases; the split; glides */
    int stereo, doppler, scanner;                /* (the tier's) */
} b3;
#define B3_DL 64u
#define PERC_LEV 0.9f                            /* the percussion against a full table (as loud as a bar at 8) */
static PLAT_POOL float b3v[B3_DL], b3hl[B3_DL];             /* the scanner's and the horn's delay lines */
static uint32_t b3vw, b3hw;
static PLAT_POOL float og_side[256];                       /* (ENG_BLOCK) the horn's side, this block */
static int og_side_on;
static float og_side_last;                       /* (its sample before: the half rate's in-between) */

static inline float b3_read(const float *line, uint32_t w, float d)   /* d samples back, interpolated */
{
    float p = (float)w - d;
    int32_t i0;
    float f;
    if (p < 0.0f)                                /* (d is under the line's length) */
        p += (float)B3_DL;
    i0 = (int32_t)p;
    f = p - (float)i0;
    return line[(uint32_t)i0 & (B3_DL - 1u)] * (1.0f - f) + line[(uint32_t)(i0 + 1) & (B3_DL - 1u)] * f;
}

void chordv_init(void)
{
    static const int8_t REG[9] = ORG_REG;
    sk.on = og.on = 0;
    chordv_b3_build(REG);                        /* (the default registration, until the main loop's) */
    chordv_b3_build(REG);
}

const float *chordv_side(void) { return og_side_on ? og_side : 0; }

static uint32_t nv_skank = NV, organ_full = 1;

void chordv_config(const int16_t *ev, int tier)
{
    nv_skank = tier >= TIER_SEVEN ? 2u : NV;
    organ_full = tier < TIER_SEVEN;
    float tone = lin(ev[P_VOICE(CH_SKANK, VS_TONE)], 600.0f, 2400.0f);
    sk.index = lin(ev[P_VOICE(CH_SKANK, VS_INDEX)], 0.0f, 4.0f);
    sk.ik = t60k(30.0f);
    sk.ak = t60k(lin(ev[P_VOICE(CH_SKANK, VS_DECAY)], 60.0f, 200.0f));
    sk.ratio = ev[P_VOICE(CH_SKANK, VS_RATIO)] ? 3.0f : 1.0f;
    sk.arate = ev[P_VOICE(CH_SKANK, VS_ATTACK)]   /* 5.3: 0 a chop (at once) .. 30 ms in, a half-rate sample's step */
                   ? 2.0f / (lin(ev[P_VOICE(CH_SKANK, VS_ATTACK)], 0.5f, 30.0f) * 0.001f * SR)
                   : 1.0f;
    sk.hp_a = fm_expf(-6.2831853f * (tone * 0.5f) * INV_SR);        /* the band: tone / 2 .. tone x 2 */
    sk.lp_a = 1.0f - fm_expf(-6.2831853f * (tone * 2.0f) * INV_SR);
    {
        float d = lin(ev[P_VOICE(CH_ORGAN, VO_DRAWBAR)], 0.0f, 1.0f);
        og.w2 = 0.6f * d;
        og.w3 = 0.4f * d;
        og.w4 = 0.25f * d;
    }
    og.ak = t60k(lin(ev[P_VOICE(CH_ORGAN, VO_DECAY)], 40.0f, 300.0f));
    og.click = lin(ev[P_VOICE(CH_ORGAN, VO_CLICK)], 0.0f, 0.5f);
    og.ck = t60k(5.0f);
    og.det = lin(ev[P_VOICE(CH_ORGAN, VO_DETUNE)], 0.0f, 0.004f);   /* 5.3: +-7 cents at most */
    b3.model = ev[P_VOICE(CH_ORGAN, VO_MODEL)];
    b3.perc = ev[P_VOICE(CH_ORGAN, VO_PERC)];
    b3.pk = t60k(ev[P_VOICE(CH_ORGAN, VO_PFAST)] ? 220.0f : 900.0f);
    b3.pk *= b3.pk;                              /* (a half-rate sample's) */
    b3.pmul = b3.perc == PERC_3RD ? 6.0f : 4.0f;  /* the 2nd (4') or the 3rd (2 2/3'), over the 16' */
    {
        int v = ev[P_VOICE(CH_ORGAN, VO_VIBCH)];
        static const float D[4] = {0.0f, 5.5f, 11.0f, 20.0f};   /* V1-3, C1-3: ~0.25, 0.5, 0.9 ms of depth */
        b3.vdepth = v > VIB_OFF ? D[v > VIB_V3 ? v - VIB_V3 : v] : 0.0f;
        b3.vmix = v >= VIB_C1 ? 0.5f : v > VIB_OFF ? 1.0f : 0.0f;
    }
    b3.drive = lin(ev[P_VOICE(CH_ORGAN, VO_ODRIVE)], 0.0f, 4.0f);
    b3.rot = ev[P_VOICE(CH_ORGAN, VO_ROTARY)];
    b3.hk = -1.0f / (0.6f * B3_HR);              /* the glides (a sample's log): the horn ~0.6 s to its speed, the drum
                                                  * ~3 s */
    b3.dk = -1.0f / (3.0f * B3_HR);
    b3.stereo = tier == TIER_STUDIO;             /* (the stereo horn: Studio only, open question 5) */
    b3.doppler = tier < TIER_SEVEN;              /* (Seven-inch: the rotary's AM only, no scanner) */
    b3.scanner = tier < TIER_SEVEN;
}

void chordv_trig(uint32_t ch, int32_t chord, int32_t voicing, uint32_t vel)
{
    int32_t notes[3];
    uint32_t i;
    float v = (float)(vel > 127u ? 127u : vel) * (1.0f / 127.0f);
    if (ch == CH_SKANK) {
        if (!sk.on)                              /* from silence: its band from rest (5.1) */
            sk.hx = sk.hy = sk.ly = 0.0f;
        chord_notes(chord, voicing, 60, notes);  /* mid: from C4 */
        for (i = 0; i < NV; i++) {
            sk.v[i].f = midi_hz(notes[i]);
            sk.v[i].ienv = 1.0f;
            sk.v[i].amp = 1.0f;
            sk.v[i].pc = sk.v[i].pm = 0.0f;
        }
        sk.vel = v;
        sk.aenv = sk.arate >= 1.0f ? 1.0f : 0.0f;   /* (ATTACK 0: at once, as built) */
        sk.on = 1;
    } else if (ch == CH_ORGAN) {
        if (b3.model == ORG_B3 && b3.perc && (!og.on || og.v[0].amp < 0.05f))
            b3.penv = 1.0f, b3.pn++;             /* (percussion single-triggered: not over a chord still sounding) */
        chord_notes(chord, voicing, 55, notes);  /* mid: from G3 */
        for (i = 0; i < NV; i++) {
            og.v[i].f = midi_hz(notes[i]);
            og.v[i].amp = 1.0f;
            if (!og.on) {                        /* from silence: the partials from 0 (5.1; while it rings they run
                                                  * on: no click) */
                og.v[i].pc = og.p2[i] = og.p3[i] = og.p4[i] = og.pk[i] = 0.0f;
                b3.ph[i] = b3.pph[i] = 0.0f;
            }
        }
        og.cenv = 1.0f;
        og.vel = v;
        og.on = 1;
        b3.held = 0;                             /* (the engine holds it after, for a step's gate) */
    }
}

float chordv_rotor(int drum) { return drum ? b3.df : b3.hf; }
uint32_t chordv_perc_n(void) { return b3.pn; }
void chordv_hold(uint32_t ch)
{
    if (ch == CH_ORGAN)
        b3.held = 1;
}
void chordv_release(uint32_t ch)
{
    if (ch == CH_ORGAN)
        b3.held = 0;
}

/* The skank and the organ at half rate (22,050 Hz, the memory plan's rate for tonal voices; 2026-11 review): a sample
 * of theirs every other output sample, the one between the two's mean (a sample's fixed delay). Their constants for
 * the half rate: decays squared, steps doubled, the one-poles' coefficients as two steps of theirs. (The skank's band
 * tops out at 4.8 kHz, the organ's click at 12 x its fundamental: both well under 11 kHz) */
static float sk_last, og_last;
static int sk_odd, og_odd;

static int render_skank(float *o, uint32_t n)
{
    uint32_t i, k;
    float inv2 = 2.0f * INV_SR, ik2 = sk.ik * sk.ik, ak2 = sk.ak * sk.ak, hp2 = sk.hp_a * sk.hp_a;
    float lp2 = 1.0f - (1.0f - sk.lp_a) * (1.0f - sk.lp_a);
    if (!sk.on) {
        sk_last = 0.0f;
        sk_odd = 0;
        return 0;
    }
    for (i = 0; i < n; i++) {
        float s = 0.0f, y;
        if (!sk_odd) {
            o[i] = sk_last;
            sk_odd = 1;
            continue;
        }
        sk_odd = 0;
        for (k = 0; k < nv_skank; k++) {
            cv_t *v = &sk.v[k];
            v->pm = wrap(v->pm + v->f * sk.ratio * inv2);
            v->pc = wrap(v->pc + v->f * inv2);
            s += fm_sin_turns(v->pc + sk.index * v->ienv * 0.15915494f * fm_sin_turns(v->pm)) * v->amp;
            v->ienv *= ik2;
            v->amp *= ak2;
        }
        y = hp2 * (sk.hy + s - sk.hx);           /* the band: a high-pass, then a low-pass */
        sk.hx = s;
        sk.hy = undenorm(y);
        sk.ly = undenorm(sk.ly + lp2 * (y - sk.ly));
        y = sk.ly * sk.vel * 0.22f;
        if (sk.aenv < 1.0f) {                    /* ATTACK (5.3): the chop swelled in */
            y *= sk.aenv;
            sk.aenv = fm_minf(1.0f, sk.aenv + sk.arate);
        }
        o[i] = 0.5f * (sk_last + y);
        sk_last = y;
    }
    for (k = 0; k < NV; k++)
        sk.v[k].ienv = undenorm(sk.v[k].ienv);
    if (sk.v[0].amp < 1e-4f)
        sk.on = 0;
    return 1;
}

static int render_b3(float *o, uint32_t n)       /* the B-3 (5.15), at half rate like the bubble */
{
    uint32_t i, k, cur = b3cur;
    float ak2 = og.ak * og.ak, ck2 = og.ck * og.ck, inv = 1.0f / B3_HR;
    float th = b3.rot == ROT_FAST ? 6.7f : b3.rot == ROT_SLOW ? 0.83f : 0.0f;   /* the rotors' targets (BRAKE: 0) */
    float td = b3.rot == ROT_FAST ? 5.8f : b3.rot == ROT_SLOW ? 0.67f : 0.0f;
    float lpa = 1.0f - fm_expf(-6.2831853f * 800.0f * inv), inc[NV];
    float sha = 1.0f - fm_expf(-6.2831853f * 500.0f * inv);   /* the side's high-pass: the low end mono (Width) */
    const int16_t *tab[NV];
    float gain = b3gain * og.vel * 0.2f * (1.0f / 32768.0f);
    og_side_on = b3.rot != ROT_OFF && b3.stereo && n <= 256u;
    float m = (float)(n / 2u), hc, hs, dc, ds, hrc, hrs, drc, drs;
    b3.hf = th + (b3.hf - th) * fm_expf(b3.hk * m);           /* the rotors to their speeds (a block's glide; silent too: a cabinet
                                                  * spins on) */
    b3.df = td + (b3.df - td) * fm_expf(b3.dk * m);
    hc = fm_sin_turns(b3.hph + 0.25f), hs = fm_sin_turns(b3.hph);   /* each rotor a phasor, turned a sample at a time */
    dc = fm_sin_turns(b3.dph + 0.25f), ds = fm_sin_turns(b3.dph);
    hrc = fm_sin_turns(b3.hf * inv + 0.25f), hrs = fm_sin_turns(b3.hf * inv);
    drc = fm_sin_turns(b3.df * inv + 0.25f), drs = fm_sin_turns(b3.df * inv);
    b3.hph = wrap(b3.hph + b3.hf * inv * m);
    b3.dph = wrap(b3.dph + b3.df * inv * m);
    if (!og.on) {
        og_last = 0.0f;
        og_odd = 0;
        og_side_on = 0;
        return 0;
    }
    for (k = 0; k < NV; k++) {                   /* each note's step and band (its harmonics under ~10 kHz), a block */
        float f16 = og.v[k].f * 0.5f * (1.0f + og.det * (float)((int32_t)k - 1));
        inc[k] = f16 * inv;
        tab[k] = b3tab[cur][f16 * 16.0f < 10000.0f ? 0u : f16 * 8.0f < 10000.0f ? 1u : 2u];
    }
    for (i = 0; i < n; i++) {
        float s = 0.0f, y, side = 0.0f;
        int perc = b3.perc && b3.penv > 1e-4f, click = og.cenv > 1e-4f;
        if (!og_odd) {
            o[i] = og_last;
            if (og_side_on)
                og_side[i] = og_side_last;
            og_odd = 1;
            continue;
        }
        og_odd = 0;
        for (k = 0; k < NV; k++) {               /* the notes: a table read at the 16' pitch */
            const int16_t *t = tab[k];
            float p, fr, a;
            uint32_t j;
            b3.ph[k] = wrap(b3.ph[k] + inc[k]);
            p = b3.ph[k] * (float)B3_N;
            j = (uint32_t)p;
            fr = p - (float)j;
            a = (float)t[j & (B3_N - 1u)];
            s += (a + ((float)t[(j + 1u) & (B3_N - 1u)] - a) * fr) * og.v[k].amp;
            if (perc) {                          /* the percussion: its harmonic, decaying */
                b3.pph[k] = wrap(b3.pph[k] + inc[k] * b3.pmul);
                s += PERC_LEV * 32768.0f * b3.penv * fm_sin_turns(b3.pph[k]);
            }
            if (click) {                         /* the key click (as the bubble's) */
                og.pk[k] = wrap(og.pk[k] + 24.0f * inc[k]);
                s += 32768.0f * og.click * og.cenv * fm_sin_turns(og.pk[k]);
            }
            if (!b3.held)                        /* (held: the tonewheels sound on; let go: DECAY) */
                og.v[k].amp *= ak2;
        }
        s *= gain;
        b3.penv = undenorm(b3.penv * b3.pk);
        og.cenv = undenorm(og.cenv * ck2);
        if (b3.vmix > 0.0f && b3.scanner) {      /* the scanner: vibrato, or chorus (with the dry) */
            float v;
            b3v[b3vw] = s;
            b3.vph = wrap(b3.vph + 6.9f * inv);
            v = b3_read(b3v, b3vw, 1.0f + b3.vdepth * (1.0f - 2.0f * fm_fabsf(b3.vph - 0.5f)));   /* (a triangle,
                                                  * as the scanner's sweep across its taps) */
            b3vw = (b3vw + 1u) & (B3_DL - 1u);
            s = b3.vmix >= 1.0f ? v : 0.5f * (s + v);
        }
        if (b3.drive > 0.0f)                     /* drive into the cabinet */
            s = s * (1.0f + b3.drive) / (1.0f + b3.drive * fm_fabsf(s));
        if (b3.rot != ROT_OFF) {                 /* the rotary: drum below 800 Hz, horn above */
            float drum, horn, am, l, r, t;
            t = hc * hrc - hs * hrs;             /* (the phasors a sample on) */
            hs = hs * hrc + hc * hrs;
            hc = t;
            t = dc * drc - ds * drs;
            ds = ds * drc + dc * drs;
            dc = t;
            b3.lp += lpa * (s - b3.lp);
            b3.lp = undenorm(b3.lp);
            drum = b3.lp * (1.0f + 0.2f * ds);
            horn = s - b3.lp;
            am = hs;
            b3hl[b3hw] = horn;
            if (b3.doppler) {                    /* two mics facing each other across the horn: its delay and level
                                                  * move opposite ways in each */
                l = b3_read(b3hl, b3hw, 16.0f + 11.0f * am) * (1.0f + 0.3f * am);
                r = b3_read(b3hl, b3hw, 16.0f - 11.0f * am) * (1.0f - 0.3f * am);
            } else
                l = r = horn * (1.0f + 0.3f * am);
            b3hw = (b3hw + 1u) & (B3_DL - 1u);
            s = drum + 0.5f * (l + r);
            if (og_side_on) {                    /* (the split's one pole leaves lows in the horn: two more on its
                                                  * side, 12 dB/oct under 500 Hz) */
                side = 0.5f * (l - r);
                b3.sl1 = undenorm(b3.sl1 + sha * (side - b3.sl1));
                side -= b3.sl1;
                b3.sl2 = undenorm(b3.sl2 + sha * (side - b3.sl2));
                side -= b3.sl2;
            }
        }
        y = s;
        o[i] = 0.5f * (og_last + y);
        og_last = y;
        if (og_side_on) {
            og_side[i] = 0.5f * (og_side_last + side);
            og_side_last = side;
        }
    }
    if (og.v[0].amp < 1e-4f && b3.penv < 1e-4f)
        og.on = 0;
    return 1;
}

static int render_organ(float *o, uint32_t n)
{
    if (b3.model == ORG_B3)
        return render_b3(o, n);
    og_side_on = 0;
    uint32_t i, k;
    float ak2 = og.ak * og.ak, ck2 = og.ck * og.ck;
    if (!og.on) {
        og_last = 0.0f;
        og_odd = 0;
        return 0;
    }
    for (i = 0; i < n; i++) {
        float s = 0.0f, y;
        if (!og_odd) {
            o[i] = og_last;
            og_odd = 1;
            continue;
        }
        og_odd = 0;
        for (k = 0; k < NV; k++) {
            float f = og.v[k].f * (2.0f * INV_SR) * (1.0f + og.det * (float)((int32_t)k - 1));   /* DETUNE (5.3) */
            og.v[k].pc = wrap(og.v[k].pc + f);
            og.p2[k] = wrap(og.p2[k] + 2.0f * f);
            og.p3[k] = wrap(og.p3[k] + 3.0f * f);
            og.p4[k] = wrap(og.p4[k] + 4.0f * f);
            og.pk[k] = wrap(og.pk[k] + 12.0f * f);   /* the key click: a high partial, ~5 ms */
            if (organ_full)
                s += (fm_sin_turns(og.v[k].pc) + og.w2 * fm_sin_turns(og.p2[k]) + og.w3 * fm_sin_turns(og.p3[k])
                      + og.w4 * fm_sin_turns(og.p4[k]) + og.click * og.cenv * fm_sin_turns(og.pk[k])) * og.v[k].amp;
            else                                 /* (Seven-inch: the fundamental and the second, no click) */
                s += (fm_sin_turns(og.v[k].pc) + og.w2 * fm_sin_turns(og.p2[k])) * og.v[k].amp;
            og.v[k].amp *= ak2;
        }
        og.cenv = undenorm(og.cenv * ck2);
        y = s * og.vel * 0.12f;
        o[i] = 0.5f * (og_last + y);
        og_last = y;
    }
    if (og.v[0].amp < 1e-4f)
        og.on = 0;
    return 1;
}

int chordv_render(uint32_t ch, float *o, uint32_t n)
{
    return ch == CH_SKANK ? render_skank(o, n) : ch == CH_ORGAN ? render_organ(o, n) : 0;
}
