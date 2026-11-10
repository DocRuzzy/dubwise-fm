/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* chordv.h. Compiled on its own at -O2 (fm1.toml o2). Floats only. */
#include "chordv.h"
#include "params.h"
#include "fastmath.h"
#include "dsp.h"
#include "plat.h"                                /* PLAT_POOL */
#include "leslie.h"                              /* the FULL Leslie (v0.3) */

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
    int pluck;                                   /* 7.8: MODEL PLUCK */
    uint32_t mute_at, t;                         /* (PLUCK: the half-rate sample its strings are damped, this one) */
} sk;
/* 7.8: PLUCK, Karplus-Strong strings after the STK's Plucked (Perry R. Cook and Gary P. Scavone, MIT-style; its pluck
 * and loop, in C): a string a note of the chord, filled at the pluck with noise through a one-pole (pole 0.999 - 0.15
 * x the velocity: harder, brighter), 0.6 of its own output fed back as it fills; then its loop: a two-point average at
 * 0.995 + f x 5e-6 a pass. DECAY is how long they ring before the fretting hand mutes them (the loop's gain to 0.6):
 * the chop. Half rate, as the FM skank; the strings int16 in the pool (a pluck starts loud: no tiny values to lose) */
#define KS_N 256u                                /* 22,050 / 256: down to 86 Hz */
static PLAT_POOL int16_t ks_line[NV][KS_N];
static struct {
    uint32_t w;
    float d, g, x1;
} ks[NV];
static uint32_t ks_rng = 0x1F123BB5u;

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
static float b3gain = 1.0f, b3unit = 1.0f;     /* (b3unit: a drawbar at 8 in the table's units: the percussion's) */
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
        if (b == 0u)
            b3unit = (32000.0f / 32768.0f) / peak;   /* (a drawbar at 8, in the table's units: the percussion's) */
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
    float vdepth, vmix, voff;                    /* the scanner: depth and offset (samples), 1 vibrato / 0.71 chorus, 0 off */
    float vph;
    int rot, full;                               /* ROT_*; the FULL Leslie (setBfree's: Studio only) */
    float hf, df, hph, dph, lp, sl1, sl2;        /* the LIGHT rotary: the rotors' speeds (Hz), phases; the split */
    int stereo, doppler, scanner;                /* (the tier's) */
    float pdec, normal;                          /* percussion's decay a sample; the sound with it on */
    float atk[NV][64], aprev[NV];                /* the key click: each note's contact-bounce envelope (setBfree's) */
    uint32_t ai[NV], rng;
    float clickd;                                /* how much they bounce (CLICK) */
    int od;                                      /* the overdrive: on (DRIVE above 0) */
    float od_chr, od_fb, od_fb2, od_sag, od_z, od_z1, od_gf;   /* setBfree's preamp: character, its two bass
                                                  * filters' poles, the sag, the filters' memories, the feedback */
} b3;
#define B3_DL 64u
#define LES_GAIN 0.8f                            /* the FULL Leslie against the LIGHT one, by level (measured: organ.txt, FAST) */
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
    b3.rng = 0x1F123BB5u;
    les_init();
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
    sk.pluck = ev[P_VOICE(CH_SKANK, VS_MODEL)] != 0;   /* 7.8 */
    sk.mute_at = (uint32_t)(lin(ev[P_VOICE(CH_SKANK, VS_DECAY)], 60.0f, 200.0f) * 0.001f * SR * 0.5f);
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
    /* percussion as setBfree's (setBfree-fm1's measurements, v0.3): its decay per sample of the plug-in, 0.9999 (slow)
     * or 0.9995 (fast) at 48 kHz: -41.7 / -208 dB a second; "normal" volume: the whole sound 0.605 while it's on */
    b3.pdec = ev[P_VOICE(CH_ORGAN, VO_PFAST)] ? 0.998912f : 0.999782f;   /* (a half-rate sample's) */
    b3.normal = b3.perc ? 0.60512f : 1.0f;
    b3.pmul = b3.perc == PERC_3RD ? 6.0f : 4.0f;  /* the 2nd (4') or the 3rd (2 2/3'), over the 16' */
    {   /* the scanner as setBfree's: a delay of 1 + A + A sin, A = 3, 6, 9 samples of 48 kHz, at 7.25 Hz; the chorus
         * mixes with 1 / sqrt(2) (here in samples of the half rate) */
        int v = ev[P_VOICE(CH_ORGAN, VO_VIBCH)];
        static const float A[4] = {0.0f, 1.378f, 2.756f, 4.134f};
        b3.vdepth = v > VIB_OFF ? A[v > VIB_V3 ? v - VIB_V3 : v] : 0.0f;
        b3.voff = 0.459f;
        b3.vmix = v >= VIB_C1 ? 0.70711f : v > VIB_OFF ? 1.0f : 0.0f;
    }
    {   /* the overdrive: setBfree's preamp, its character the DRIVE (0 off); its two bass filters' poles at the half
         * rate (squared) */
        int32_t c = ev[P_VOICE(CH_ORGAN, VO_ODRIVE)];
        float fb, fb2;
        b3.od = c > 0;
        if (c < 32)
            fb = 0.58212f, fb2 = 0.99899f + (0.58212f - 0.99899f) * (float)c * (1.0f / 31.0f);
        else if (c < 64)
            fb = 0.58212f + (0.99899f - 0.58212f) * (float)(c - 32) * (1.0f / 31.0f), fb2 = 0.58212f;
        else
            fb = 0.99899f, fb2 = 0.58212f + (0.99899f - 0.58212f) * (float)(c - 64) * (1.0f / 63.0f);
        b3.od_fb = fb * fb;
        b3.od_fb2 = fb2 * fb2;
    }
    b3.clickd = fm_minf(1.0f, og.click * 4.0f);  /* CLICK: how much the contacts bounce */
    b3.rot = ev[P_VOICE(CH_ORGAN, VO_ROTARY)];
    b3.full = ev[P_VOICE(CH_ORGAN, VO_LESLIE)] == LES_FULL && tier == TIER_STUDIO;
    b3.stereo = tier == TIER_STUDIO;             /* (the stereo horn: Studio only, open question 5) */
    b3.doppler = tier < TIER_SEVEN;              /* (Seven-inch: the rotary's AM only, no scanner) */
    b3.scanner = tier < TIER_SEVEN;
}

static uint32_t b3_rand(void)                    /* (the key click's: xorshift, the same clicks on a replay) */
{
    b3.rng ^= b3.rng << 13;
    b3.rng ^= b3.rng >> 17;
    b3.rng ^= b3.rng << 5;
    return b3.rng;
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
        if (sk.pluck) {                          /* 7.8: each string plucked: filled with the pick's noise */
            float pole = 0.999f - v * 0.15f, gain = v * 0.5f;
            sk.t = 0;
            for (i = 0; i < NV; i++) {
                float f = sk.v[i].f, len = 22050.0f / f - 0.5f, y = 0.0f, last = 0.0f;
                uint32_t k, n;
                if (len > (float)(KS_N - 2u))
                    len = (float)(KS_N - 2u);
                ks[i].d = len;
                ks[i].g = fm_minf(0.995f + f * 0.000005f, 0.99999f);
                ks[i].x1 = 0.0f;
                n = (uint32_t)len;
                for (k = 0; k < KS_N; k++)
                    ks_line[i][k] = 0;
                for (k = 0; k < n; k++) {        /* (as the STK: the line run once round, the pick's noise in) */
                    float nz;
                    ks_rng ^= ks_rng << 13;
                    ks_rng ^= ks_rng >> 17;
                    ks_rng ^= ks_rng << 5;
                    nz = (float)(int32_t)ks_rng * (1.0f / 2147483648.0f);
                    y = gain * (1.0f - pole) * nz + pole * y;
                    last = fm_clampf(0.6f * last + y, -1.99f, 1.99f);
                    ks_line[i][k] = (int16_t)(last * 16384.0f);
                }
                ks[i].w = n % KS_N;
            }
        }
    } else if (ch == CH_ORGAN) {
        if (b3.model == ORG_B3 && b3.perc && (!og.on || og.v[0].amp < 0.05f))
            b3.penv = 1.0f, b3.pn++;             /* (percussion single-triggered: not over a chord still sounding) */
        if (b3.model == ORG_B3)
            for (i = 0; i < NV; i++) {           /* the key click as setBfree's (tonegen.c, attack "click"): each
                                                  * note's contacts close after a random wait and bounce for 16..80
                                                  * samples (of 44.1 kHz) at 1 - 0.5 random, then a two-point average;
                                                  * CLICK scales the bounce and the wait */
                uint32_t burst = 8u + b3_rand() % 32u, start = (uint32_t)((float)(b3_rand() % (64u - burst)) * b3.clickd), j;
                float e, prev = 0.0f;
                for (j = 0; j < 64u; j++) {
                    e = j < start ? 0.0f : j < start + burst ? 1.0f - 0.5f * b3.clickd * (float)(b3_rand() & 0xFFFFu) * (1.0f / 65536.0f) : 1.0f;
                    b3.atk[i][j] = 0.5f * (prev + e);
                    prev = b3.atk[i][j];
                }
                b3.ai[i] = 0;
                b3.aprev[i] = 0.0f;
            }
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
        if (sk.pluck) {                          /* 7.8: the strings */
            float g_mute = sk.t >= sk.mute_at ? 0.6f : 1.0f;
            sk.t++;
            for (k = 0; k < nv_skank; k++) {
                float d = ks[k].d, fr, x, y2;
                int32_t di = (int32_t)d;
                uint32_t a, b;
                fr = d - (float)di;
                a = (ks[k].w + KS_N - (uint32_t)di) % KS_N;
                b = a == 0u ? KS_N - 1u : a - 1u;
                x = ((float)ks_line[k][a] + fr * (float)(ks_line[k][b] - ks_line[k][a])) * (1.0f / 16384.0f);
                s += x;
                x *= ks[k].g * g_mute;
                y2 = 0.5f * (x + ks[k].x1);       /* the loop's two-point average */
                ks[k].x1 = x;
                ks_line[k][ks[k].w] = (int16_t)(fm_clampf(y2, -1.99f, 1.99f) * 16384.0f);
                ks[k].w = ks[k].w + 1u == KS_N ? 0u : ks[k].w + 1u;
            }
            s *= 6.4f;                           /* (FM's loudness) */
            sk.v[0].amp = sk.t >= sk.mute_at + 2205u ? 0.0f : 1.0f;   /* (over 0.1 s after the mute: done) */
        } else
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

static inline float od_t(float x) { return 1.0f / (1.0f + x * x); }
/* the overdrive (v0.3): setBfree's preamp (b_overdrive/overdrive.c), after setBfree-fm1's integer port (organ_od.h,
 * R-Jeb, GPL-2.0-or-later), in floats: the input gain and its sag, a bass filter, the transfer curve 1 / (1 + x^2)
 * around a bias that sags with the level, a second bass filter, the global feedback. Its two FIRs (interpolation and
 * decimation, setBfree's oversampling) are left out at the half rate: their DC gain (0.646 each) is kept */
static float b3_od(float x)
{
    float xin = x * 3.5675f, bias, nrm, u, v, ex, tm;
    b3.od_sag = fm_minf(1024.0f, b3.od_sag * 0.9910f + fm_fabsf(xin));
    bias = 0.5347f - 0.0094f * b3.od_sag;
    nrm = 1.0f - od_t(bias);
    u = 0.646f * xin + 0.6214f * b3.od_gf;
    tm = u - b3.od_z;
    b3.od_z = undenorm(u + b3.od_z * b3.od_fb);
    ex = tm;
    v = ex < 0.0f ? od_t(ex - bias) - 1.0f + nrm : 1.0f - nrm - od_t(ex + bias);
    tm = v + b3.od_z1 * b3.od_fb2;
    v = tm - b3.od_z1;
    b3.od_z1 = undenorm(tm);
    b3.od_gf = v;
    return v * (0.646f * 0.8795f);
}

static int render_b3(float *o, uint32_t n)       /* the B-3 (5.15), at half rate like the bubble */
{
    uint32_t i, k, cur = b3cur;
    float ak2 = og.ak * og.ak, inv = 1.0f / B3_HR;
    /* the LIGHT rotary's rotors: setBfree's speeds, each at its own pace (horn up 0.16 s, down 0.32 s; drum up 4.1 s,
     * down 1.4 s; v0.3, setBfree-fm1's figures). BRAKE: to a stop */
    float th = b3.rot == ROT_FAST ? 7.056f : b3.rot == ROT_SLOW ? 0.672f : 0.0f;
    float td = b3.rot == ROT_FAST ? 5.955f : b3.rot == ROT_SLOW ? 0.600f : 0.0f;
    float lpa = 1.0f - fm_expf(-6.2831853f * 800.0f * inv), inc[NV], plev[NV];
    float sha = 1.0f - fm_expf(-6.2831853f * 500.0f * inv);   /* the side's high-pass: the low end mono (Width) */
    const int16_t *tab[NV];
    float gain = b3gain * og.vel * 0.2f;
    int full = b3.full && b3.rot != ROT_OFF;
    float m = (float)(n / 2u), hc, hs, dc, ds, hrc, hrs, drc, drs, vc, vs, vrc, vrs;
    og_side_on = b3.rot != ROT_OFF && b3.stereo && n <= 256u;
    b3.hf = th + (b3.hf - th) * fm_expf(-m * inv / (th > b3.hf ? 0.161f : 0.321f));   /* (a block's glide; silent
                                                  * too: a cabinet spins on) */
    b3.df = td + (b3.df - td) * fm_expf(-m * inv / (td > b3.df ? 4.127f : 1.371f));
    hc = fm_sin_turns(b3.hph + 0.25f), hs = fm_sin_turns(b3.hph);   /* each rotor a phasor, turned a sample at a time */
    dc = fm_sin_turns(b3.dph + 0.25f), ds = fm_sin_turns(b3.dph);
    hrc = fm_sin_turns(b3.hf * inv + 0.25f), hrs = fm_sin_turns(b3.hf * inv);
    drc = fm_sin_turns(b3.df * inv + 0.25f), drs = fm_sin_turns(b3.df * inv);
    vc = fm_sin_turns(b3.vph + 0.25f), vs = fm_sin_turns(b3.vph);   /* the scanner's sine, 7.25 Hz */
    vrc = fm_sin_turns(7.25f * inv + 0.25f), vrs = fm_sin_turns(7.25f * inv);
    b3.hph = wrap(b3.hph + b3.hf * inv * m);
    b3.dph = wrap(b3.dph + b3.df * inv * m);
    b3.vph = wrap(b3.vph + 7.25f * inv * m);
    if (full)
        les_block(n / 2u, b3.rot == ROT_FAST ? 2 : b3.rot == ROT_SLOW ? 1 : 0);
    if (!og.on) {
        if (full)
            les_spin(n / 2u);                    /* (its rotors spin on) */
        og_last = 0.0f;
        og_odd = 0;
        og_side_on = 0;
        return 0;
    }
    for (k = 0; k < NV; k++) {                   /* each note's step and band (its harmonics under ~10 kHz), a block */
        float f16 = og.v[k].f * 0.5f * (1.0f + og.det * (float)((int32_t)k - 1));
        inc[k] = f16 * inv;
        tab[k] = b3tab[cur][f16 * 16.0f < 10000.0f ? 0u : f16 * 8.0f < 10000.0f ? 1u : 2u];
        /* percussion as setBfree's: 11 x its harmonic at a drawbar's level, through a first difference at 48 kHz
         * (the higher the note, the louder): 2 sin(pi f / 48000) */
        plev[k] = 11.0f * b3unit * 2.0f * fm_sin_turns(fm_minf(0.25f, f16 * b3.pmul * (0.5f / 48000.0f)));
    }
    for (i = 0; i < n; i++) {
        float s = 0.0f, y, side = 0.0f;
        int perc = b3.perc && b3.penv > 1e-4f;
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
            float p, fr, a, e = b3.ai[k] < 64u ? b3.atk[k][b3.ai[k]++] : 1.0f;   /* (the key click's bounce) */
            uint32_t j;
            b3.ph[k] = wrap(b3.ph[k] + inc[k]);
            p = b3.ph[k] * (float)B3_N;
            j = (uint32_t)p;
            fr = p - (float)j;
            a = (float)t[j & (B3_N - 1u)];
            s += (a + ((float)t[(j + 1u) & (B3_N - 1u)] - a) * fr) * (1.0f / 32768.0f) * og.v[k].amp * e;
            if (perc) {                          /* the percussion: its harmonic, decaying; and, as setBfree's, the
                                                  * first difference of the key's bounce under it: a click */
                float ps;
                b3.pph[k] = wrap(b3.pph[k] + inc[k] * b3.pmul);
                ps = fm_sin_turns(b3.pph[k]) * b3.penv;
                s += plev[k] * e * ps + 11.0f * b3unit * 0.5f * (e - b3.aprev[k]) * ps;
            }
            b3.aprev[k] = e;
            if (!b3.held)                        /* (held: the tonewheels sound on; let go: DECAY) */
                og.v[k].amp *= ak2;
        }
        s *= gain * b3.normal;
        b3.penv = undenorm(b3.penv * b3.pdec);
        if (b3.vmix > 0.0f && b3.scanner) {      /* the scanner (setBfree's): vibrato, or chorus with the dry */
            float v, t = vc * vrc - vs * vrs;
            vs = vs * vrc + vc * vrs;
            vc = t;
            b3v[b3vw] = s;
            v = b3_read(b3v, b3vw, b3.voff + b3.vdepth + b3.vdepth * vs);
            b3vw = (b3vw + 1u) & (B3_DL - 1u);
            s = b3.vmix >= 1.0f ? v : b3.vmix * (s + v);
        }
        if (b3.od)                               /* into the preamp (its level as setBfree's mono: x 3.5) */
            s = b3_od(s * 3.5f) * (1.0f / 3.5f);
        if (full) {                              /* the FULL Leslie (setBfree's whirl: Studio only) */
            float l, r;
            les_tick(s, &l, &r);
            s = 0.5f * (l + r) * LES_GAIN;
            side = 0.5f * (l - r) * LES_GAIN;    /* (the drum is mono: only the horn here; and its lows off) */
            b3.sl1 = undenorm(b3.sl1 + sha * (side - b3.sl1));
            side -= b3.sl1;
            b3.sl2 = undenorm(b3.sl2 + sha * (side - b3.sl2));
            side -= b3.sl2;
        } else if (b3.rot != ROT_OFF) {          /* the LIGHT rotary: drum below 800 Hz, horn above */
            float drum, horn, am, l, r, t, gl, gr;
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
            gl = 0.2f + 0.4f * (1.0f + am);      /* the horn a beam: 14 dB between facing a microphone and away */
            gr = 0.2f + 0.4f * (1.0f - am);
            b3hl[b3hw] = horn;
            if (b3.doppler) {                    /* two mics facing each other across the horn: its delay and level
                                                  * move opposite ways in each */
                l = b3_read(b3hl, b3hw, 16.0f + 11.0f * am) * gl;
                r = b3_read(b3hl, b3hw, 16.0f - 11.0f * am) * gr;
            } else
                l = horn * gl, r = horn * gr;
            b3hw = (b3hw + 1u) & (B3_DL - 1u);
            if (b3.stereo) {                     /* (Studio: both microphones; else one, as a 7-inch heard it) */
                s = drum + 0.5f * (l + r);
                if (og_side_on) {                /* (the split's one pole leaves lows in the horn: two more on its
                                                  * side, 12 dB/oct under 500 Hz) */
                    side = 0.5f * (l - r);
                    b3.sl1 = undenorm(b3.sl1 + sha * (side - b3.sl1));
                    side -= b3.sl1;
                    b3.sl2 = undenorm(b3.sl2 + sha * (side - b3.sl2));
                    side -= b3.sl2;
                }
            } else
                s = drum + l;
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
