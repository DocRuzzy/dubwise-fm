/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors
 *
 * The 808 drum models (roadmap 7.6), adapted to C, floats and 44.1 kHz from Mutable Instruments Plaits'
 * plaits/dsp/drums/analog_bass_drum.h, analog_snare_drum.h and hi_hat.h, and stmlib's Svf, under this notice:
 *
 *   Copyright 2016 Emilie Gillet.
 *   Author: Emilie Gillet (emilie.o.gillet@gmail.com)
 *   Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
 *   documentation files (the "Software"), to deal in the Software without restriction, including without limitation
 *   the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and
 *   to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *   The above copyright notice and this permission notice shall be included in all copies or substantial portions of
 *   the Software.
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
 *   THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 *   CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 *   IN THE SOFTWARE.
 *
 * Changes from Plaits: 44.1 kHz (its per-sample decays, made at 48 kHz, raised to 48/44.1), no sustain mode, the
 * snare's and hat's filters set once a hit, a hit's end detected so the voice stops costing, our own noise. Compiled
 * on its own at -O2 (fm1.toml o2). */
#include "d808.h"
#include "fastmath.h"
#include "dsp.h"

#define SR 44100.0f
#define K48 1.0884354f                           /* 48,000 / 44,100: a 48 kHz per-sample decay c is c^K48 here */
#define QUIET 3e-6f                              /* (a hit over: its block's largest sample under this, the pulses done) */

typedef struct { float g, r, h, s1, s2; } svf_t;   /* stmlib's Svf (TPT, Zavalishin) */
static inline void svf_set(svf_t *s, float f, float res, int accurate)   /* f in cycles a sample, res: Q */
{
    float w = 3.14159265f * f;
    s->g = accurate ? fm_tanf(w) : w * (1.0f + w * w * (1.0f / 3.0f));   /* (Plaits' "dirty" tan: its series) */
    s->r = 1.0f / res;
    s->h = 1.0f / (1.0f + s->r * s->g + s->g * s->g);
}
static inline float svf_run(svf_t *s, float in, float *lp)   /* the band-pass out (and the low-pass in *lp) */
{
    float hp = (in - s->r * s->s1 - s->g * s->s1 - s->s2) * s->h, bp = s->g * hp + s->s1, l;
    s->s1 = s->g * hp + bp;
    l = s->g * bp + s->s2;
    s->s2 = s->g * bp + l;
    if (lp)
        *lp = l;
    return bp;
}
static inline float svf_hp(svf_t *s, float in)
{
    float hp = (in - s->r * s->s1 - s->g * s->s1 - s->s2) * s->h, bp = s->g * hp + s->s1;
    s->s1 = s->g * hp + bp;
    s->s2 = s->g * bp + (s->g * bp + s->s2);
    return hp;
}
static inline float diode(float x)
{
    if (x >= 0.0f)
        return x;
    x *= 2.0f;
    return 0.7f * x / (1.0f + fm_fabsf(x));
}
static inline float soft_clip(float x)          /* stmlib's SoftClip */
{
    if (x < -3.0f)
        return -1.0f;
    if (x > 3.0f)
        return 1.0f;
    return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
}
static uint32_t rng = 0x6C078965u;
static inline float rnd01(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return (float)(rng >> 8) * (1.0f / 16777216.0f);
}

/* ------------------------------------------------------------ the kick (AnalogBassDrum) --- */
static struct {
    int on, pulse_n, fm_n;
    float pulse, height, pulse_lp, fm_lp, retrig, lp_out, tone_lp;
    float f0, q, scale, tone_f, leak, attack_fm, self_fm;
    svf_t res;
} kk;

void d808_init(void)
{
    kk.on = 0;
}

void d808_kick_trig(const d808_kick_cfg_t *c, float accent, float tmul, float dinv)
{
    float f0 = c->f0 * tmul / SR;
    kk.f0 = f0;
    kk.q = 1500.0f * fm_exp2f(c->decay * (80.0f / 12.0f)) / dinv;   /* (a DECAY lock: the ring x its time) */
    kk.scale = 0.001f / f0;
    kk.tone_f = fm_minf(4.0f * f0 * fm_exp2f(c->tone * 9.0f), 1.0f);
    kk.leak = 0.08f * (c->tone + 0.25f);
    kk.attack_fm = c->attack_fm;
    kk.self_fm = c->self_fm;
    kk.pulse_n = (int)(1.0e-3f * SR);
    kk.fm_n = (int)(6.0e-3f * SR);
    kk.height = 3.0f + 7.0f * accent;
    kk.lp_out = 0.0f;
    if (!kk.on) {                                /* (from silence: from rest, so a replay is the same) */
        kk.pulse = kk.pulse_lp = kk.fm_lp = kk.retrig = kk.tone_lp = 0.0f;
        kk.res.s1 = kk.res.s2 = 0.0f;
    }
    kk.on = 1;
}

int d808_kick_render(float *o, uint32_t n)
{
    uint32_t i;
    float pk = 0.0f;
    if (!kk.on)
        return 0;
    for (i = 0; i < n; i++) {
        float pulse, fm_pulse = 0.0f, punch, f, bp;
        (void)f;
        if (kk.pulse_n) {                        /* Q39 / Q40: the trigger's pulse, then its tail */
            --kk.pulse_n;
            pulse = kk.pulse_n ? kk.height : kk.height - 1.0f;
            kk.pulse = pulse;
        } else {
            kk.pulse = undenorm(kk.pulse * (1.0f - 1.0f / (0.2e-3f * SR)));   /* (flushed each sample: these
                                                                                 * pulses fall to denormals in
                                                                                 * a few hundred, 30x the cost) */
            pulse = kk.pulse;
        }
        kk.pulse_lp = undenorm(kk.pulse_lp + (pulse - kk.pulse_lp) * (1.0f / (0.1e-3f * SR)));   /* C40 / R163 / D83 */
        pulse = diode((pulse - kk.pulse_lp) + pulse * 0.044f);
        if (kk.fm_n) {                           /* Q41 / Q42: the attack's FM pulse, 6 ms */
            --kk.fm_n;
            fm_pulse = 1.0f;
            kk.retrig = kk.fm_n ? 0.0f : -0.8f;
        } else
            kk.retrig = undenorm(kk.retrig * (1.0f - 1.0f / (0.05f * SR)));
        kk.fm_lp = undenorm(kk.fm_lp + (fm_pulse - kk.fm_lp) * (1.0f / (0.1e-3f * SR)));
        punch = 0.7f + diode(10.0f * kk.lp_out - 1.0f);   /* Q43 and R170's leak */
        if (!(i & 3u)) {                         /* (the pitch every 4 samples, 0.09 ms: it moves smoothly; half the
                                                  * cost of every sample) */
            f = kk.f0 * (1.0f + kk.fm_lp * 1.7f * kk.attack_fm + punch * 0.08f * kk.self_fm);
            f = fm_clampf(f, 0.0f, 0.4f);
            svf_set(&kk.res, f, 1.0f + kk.q * f, 0);
        }
        bp = svf_run(&kk.res, (pulse - kk.retrig * 0.2f) * kk.scale, &kk.lp_out);
        kk.tone_lp += (pulse * kk.leak + bp - kk.tone_lp) * kk.tone_f;
        o[i] = kk.tone_lp;
        pk = fm_maxf(pk, fm_fabsf(kk.tone_lp));
    }
    kk.res.s1 = undenorm(kk.res.s1);
    kk.res.s2 = undenorm(kk.res.s2);
    kk.tone_lp = undenorm(kk.tone_lp);
    kk.retrig = undenorm(kk.retrig);
    kk.pulse = undenorm(kk.pulse);
    if (!kk.pulse_n && !kk.fm_n && pk < QUIET)
        kk.on = 0;
    return 1;
}

/* ------------------------------------------------------------ the snare (AnalogSnareDrum) --- */
#define NMODES 5
static const float MODE_F[NMODES] = {1.00f, 2.00f, 3.18f, 4.16f, 5.62f};
static struct {
    int on, pulse_n;
    float pulse, height, pulse_lp, nenv, ndecay, leak, snappy;
    float gain[NMODES];
    svf_t res[NMODES], nf;
} sn;

void d808_snare_trig(const d808_snare_cfg_t *c, float accent, float tmul, float dinv)
{
    float f0 = c->f0 * tmul / SR, d = c->decay, dxt = d * (1.0f + d * (d - 1.0f)), q, tone = c->tone, snappy = c->snappy,
          fn;
    uint32_t i;
    q = 2000.0f * fm_exp2f(dxt * 7.0f) / dinv;   /* (84 semitones; a DECAY lock: x its time) */
    sn.ndecay = fm_powf(1.0f - 0.0017f * fm_exp2f(-d * (50.0f + snappy * 10.0f) * (1.0f / 12.0f)), K48);
    sn.leak = snappy * (2.0f - snappy) * 0.1f;
    sn.snappy = fm_clampf(snappy * 1.1f - 0.05f, 0.0f, 1.0f);
    for (i = 0; i < NMODES; i++) {
        float f = fm_minf(f0 * MODE_F[i], 0.499f);
        svf_set(&sn.res[i], f, 1.0f + f * (i == 0 ? q : q * 0.25f), 1);
    }
    if (tone < 0.666667f) {                      /* 808-style: two modes */
        tone *= 1.5f;
        sn.gain[0] = 1.5f + (1.0f - tone) * (1.0f - tone) * 4.5f;
        sn.gain[1] = 2.0f * tone + 0.15f;
        for (i = 2; i < NMODES; i++)
            sn.gain[i] = 0.0f;
    } else {                                     /* "what the 808 could have been": the other modes in */
        tone = (tone - 0.666667f) * 3.0f;
        sn.gain[0] = 1.5f - tone * 0.5f;
        sn.gain[1] = 2.15f - tone * 0.7f;
        for (i = 2; i < NMODES; i++) {
            sn.gain[i] = tone;
            tone *= tone;
        }
    }
    fn = fm_minf(f0 * 16.0f, 0.499f);
    svf_set(&sn.nf, fn, 1.0f + fn * 1.5f, 1);
    sn.pulse_n = (int)(1.0e-3f * SR);
    sn.height = 3.0f + 7.0f * accent;
    sn.nenv = 2.0f;
    if (!sn.on) {
        sn.pulse = sn.pulse_lp = 0.0f;
        for (i = 0; i < NMODES; i++)
            sn.res[i].s1 = sn.res[i].s2 = 0.0f;
        sn.nf.s1 = sn.nf.s2 = 0.0f;
    }
    sn.on = 1;
}

int d808_snare_render(float *o, uint32_t n)
{
    uint32_t i, k;
    float pk = 0.0f;
    if (!sn.on)
        return 0;
    for (i = 0; i < n; i++) {
        float pulse, shell = 0.0f, noise;
        if (sn.pulse_n) {                        /* Q45 / Q46 */
            --sn.pulse_n;
            pulse = sn.pulse_n ? sn.height : sn.height - 1.0f;
            sn.pulse = pulse;
        } else {
            sn.pulse = undenorm(sn.pulse * (1.0f - 1.0f / (0.1e-3f * SR)));
            pulse = sn.pulse;
        }
        sn.pulse_lp = undenorm(sn.pulse_lp + (pulse - sn.pulse_lp) * 0.75f);
        for (k = 0; k < NMODES; k++) {           /* the shell's modes, struck */
            float e = k == 0 ? (pulse - sn.pulse_lp) + 0.006f * pulse : 0.026f * pulse;
            if (sn.gain[k] != 0.0f)
                shell += sn.gain[k] * (svf_run(&sn.res[k], e, 0) + e * sn.leak);
        }
        shell = soft_clip(shell);
        noise = 2.0f * rnd01() - 1.0f;           /* C56 / R194 / Q48: rectified noise, its envelope, band-passed */
        if (noise < 0.0f)
            noise = 0.0f;
        sn.nenv = undenorm(sn.nenv * sn.ndecay);
        noise = svf_run(&sn.nf, noise * sn.nenv * sn.snappy * 2.0f, 0);
        o[i] = noise + shell * (1.0f - sn.snappy);
        pk = fm_maxf(pk, fm_fabsf(o[i]));
    }
    for (k = 0; k < NMODES; k++) {
        sn.res[k].s1 = undenorm(sn.res[k].s1);
        sn.res[k].s2 = undenorm(sn.res[k].s2);
    }
    sn.nf.s1 = undenorm(sn.nf.s1);
    sn.nf.s2 = undenorm(sn.nf.s2);
    sn.nenv = undenorm(sn.nenv);
    sn.pulse = undenorm(sn.pulse);
    if (!sn.pulse_n && pk < QUIET)
        sn.on = 0;
    return 1;
}

/* ------------------------------------------------------------ the hat (HiHat<SquareNoise, SwingVCA, true, false>) --- */
static const float HAT_R[6] = {1.0f, 1.304f, 1.466f, 1.787f, 1.932f, 2.536f};
static struct {
    int on;
    uint32_t ph[6], inc[6];
    float env, k, noisiness, nclock, nf, nsample;
    svf_t col, hpf;
} hh;

void d808_hat_trig(const d808_hat_cfg_t *c, float accent)
{
    uint32_t i;
    float cut = fm_minf(150.0f * fm_exp2f(c->tone * 6.0f), 16000.0f) / SR, nz = c->noisiness * c->noisiness;
    for (i = 0; i < 6; i++) {
        float f = fm_minf(c->f0 / SR * HAT_R[i], 0.499f);
        hh.inc[i] = (uint32_t)(f * 4294967296.0f);
    }
    svf_set(&hh.col, cut, 3.0f + 3.0f * c->tone, 1);   /* the metal's colour: a resonant band-pass */
    svf_set(&hh.hpf, cut, 0.5f, 1);
    hh.noisiness = nz;
    hh.nf = fm_minf(c->f0 / SR * (16.0f + 16.0f * (1.0f - nz)), 0.5f);
    hh.k = c->k;
    hh.env = 1.75f * (0.3f + 0.7f * accent);
    if (!hh.on) {
        for (i = 0; i < 6; i++)
            hh.ph[i] = 0u;
        hh.col.s1 = hh.col.s2 = hh.hpf.s1 = hh.hpf.s2 = 0.0f;
        hh.nclock = hh.nsample = 0.0f;
    }
    hh.on = 1;
}

int d808_hat_render(float *o, uint32_t n)
{
    uint32_t i, k;
    if (!hh.on)
        return 0;
    for (i = 0; i < n; i++) {
        uint32_t cnt = 0;
        float s;
        for (k = 0; k < 6; k++) {                /* six squares, summed */
            hh.ph[k] += hh.inc[k];
            cnt += hh.ph[k] >> 31;
        }
        s = svf_run(&hh.col, 0.33f * (float)cnt - 1.0f, 0);
        hh.nclock += hh.nf;                      /* the noise: sampled and held at a clock of its own */
        if (hh.nclock >= 1.0f) {
            hh.nclock -= 1.0f;
            hh.nsample = rnd01() - 0.5f;
        }
        s += hh.noisiness * (hh.nsample - s);
        hh.env *= hh.k;
        s *= s > 0.0f ? 4.0f : 0.1f;             /* the swing VCA */
        s = s / (1.0f + fm_fabsf(s));
        s = (s + 0.1f) * hh.env;
        o[i] = svf_hp(&hh.hpf, s);
    }
    hh.col.s1 = undenorm(hh.col.s1);
    hh.col.s2 = undenorm(hh.col.s2);
    hh.hpf.s1 = undenorm(hh.hpf.s1);
    hh.hpf.s2 = undenorm(hh.hpf.s2);
    if (hh.env < 1e-5f)
        hh.on = 0;
    return 1;
}
