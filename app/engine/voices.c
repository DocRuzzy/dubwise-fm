/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* voices.h. Compiled on its own at -O2 (fm1.toml o2). Floats only. */
#include "voices.h"
#include "params.h"
#include "fastmath.h"
#include "dsp.h"

#define SR 44100.0f
#define INV_SR (1.0f / 44100.0f)
#define SILENT 1e-4f                             /* -80 dB: a voice under this is done */

static inline float t60k(float ms)               /* a per-sample factor that falls 60 dB in ms */
{
    return fm_exp2f(-9.966f / (ms * 0.001f * SR));
}
static inline float onepole(float hz) { return 1.0f - fm_expf(-6.2831853f * hz * INV_SR); }
static inline float midi_hz(int32_t n) { return 440.0f * fm_exp2f((float)(n - 69) * (1.0f / 12.0f)); }
static inline float wrap(float p) { return p >= 1.0f ? p - 1.0f : p; }
static inline float lin(int16_t v, float lo, float hi) { return lo + (hi - lo) * (float)v * (1.0f / 127.0f); }

static uint32_t rng = 0x12345678u;               /* xorshift32: the noise */
static inline float noise(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return (float)(int32_t)rng * (1.0f / 2147483648.0f);
}

/* ------------------------------------------------------------------- kick --- */
static struct {
    float ph, penv, amp, cenv, vel;              /* state */
    float f1, f0, pk, ak, ck, click;             /* the block's coefficients */
    int on;
} kick;

/* ------------------------------------------------------------------- rim --- */
static struct {
    float pc, pm, p2, ienv, amp, nenv, hp_x, hp_y, vel;
    float f, index, ak, ik, nk, noise_l, hp_a;
    int on;
} rim;

/* ------------------------------------------------------------------- hats --- */
static struct {
    float p[4], amp, h1x, h1y, h2x, h2y, vel;
    float f, kc, ko, k, noise_l;
    int on;
} hat;
static const float HAT_R[4] = {1.0f, 1.48f, 2.27f, 3.6f};

/* ------------------------------------------------------------------- bass --- */
static struct {
    float pc, pm, ps, ienv, amp, lp1, lp2, vel, f, target;
    float index, ik, ak, rk, att, ratio, sub, sus, lp_a;
    int32_t note;                                /* held, -1 none (released: ringing out) */
    int on;
} bass;

/* ------------------------------------------------------------------- lead --- */
/* the lead (P2.2; D2, D3): melodica (two carriers a few cents apart, a self-fed modulator: a reed; a breath for an
 * attack, vibrato after 300 ms) or brass (Chowning's: its index follows its loudness, a scoop up into the note, a
 * 12 ms doubling for a section). Mono, the last note wins */
#define DOUBLE_N 530u                            /* 12 ms */
static struct {
    float pc, pc2, pm, mprev, f, amp, vel, vib, scoop, t;
    float inst, bright, att, rel, vdepth;
    int32_t note;                                /* held, -1 none */
    int on;
} lead;
static float dbl[DOUBLE_N];
static uint32_t dbl_pos;

void voices_init(void)
{
    lead.on = 0;
    lead.note = -1;
    lead.amp = 0.0f;
    kick.on = rim.on = hat.on = bass.on = 0;
    bass.note = -1;
    kick.amp = rim.amp = hat.amp = bass.amp = 0.0f;
}

void voices_config(const int16_t *ev, int tier)
{
    /* kick: TUNE 40..70 Hz, SWEEP 80..300 Hz, PTIME 10..120 ms, DECAY 100..1000 ms, CLICK index 0..3 */
    kick.f1 = lin(ev[P_VOICE(CH_KICK, VK_TUNE)], 40.0f, 70.0f);
    kick.f0 = lin(ev[P_VOICE(CH_KICK, VK_SWEEP)], 80.0f, 300.0f);
    kick.pk = t60k(lin(ev[P_VOICE(CH_KICK, VK_PTIME)], 10.0f, 120.0f));
    kick.ak = t60k(lin(ev[P_VOICE(CH_KICK, VK_DECAY)], 100.0f, 1000.0f));
    kick.ck = t60k(6.0f);
    kick.click = lin(ev[P_VOICE(CH_KICK, VK_CLICK)], 0.0f, 3.0f);
    /* rim: TUNE 300..1200 Hz, DECAY 20..200 ms, NOISE 0..1, INDEX 0..6 */
    rim.f = lin(ev[P_VOICE(CH_SNARE, VR_TUNE)], 300.0f, 1200.0f);
    rim.ak = t60k(lin(ev[P_VOICE(CH_SNARE, VR_DECAY)], 20.0f, 200.0f));
    rim.noise_l = lin(ev[P_VOICE(CH_SNARE, VR_NOISE)], 0.0f, 1.0f);
    rim.index = lin(ev[P_VOICE(CH_SNARE, VR_INDEX)], 0.0f, 6.0f);
    rim.ik = t60k(12.0f);
    rim.nk = t60k(6.0f);
    rim.hp_a = fm_expf(-6.2831853f * 300.0f * INV_SR);
    /* hats: TONE 300..800 Hz, CLOSED 20..150 ms, OPEN 150..800 ms, NOISE 0..1 */
    hat.f = lin(ev[P_VOICE(CH_HATS, VH_TONE)], 300.0f, 800.0f);
    hat.kc = t60k(lin(ev[P_VOICE(CH_HATS, VH_CLOSED)], 20.0f, 150.0f));
    hat.ko = t60k(lin(ev[P_VOICE(CH_HATS, VH_OPEN)], 150.0f, 800.0f));
    hat.noise_l = lin(ev[P_VOICE(CH_HATS, VH_NOISE)], 0.0f, 1.0f);
    /* bass: INDEX 0..2.5, IDECAY 10..150 ms, DECAY 80..1500 ms, CUTOFF 300..2500 Hz, RATIO 1:1 / 2:1, SUB, SUSTAIN */
    bass.index = lin(ev[P_VOICE(CH_BASS, VB_INDEX)], 0.0f, 2.5f);
    bass.ik = t60k(lin(ev[P_VOICE(CH_BASS, VB_IDECAY)], 10.0f, 150.0f));
    bass.ak = t60k(lin(ev[P_VOICE(CH_BASS, VB_DECAY)], 80.0f, 1500.0f));
    bass.rk = t60k(60.0f);
    bass.att = 1.0f / (0.002f * SR);             /* 2 ms */
    bass.lp_a = onepole(lin(ev[P_VOICE(CH_BASS, VB_CUTOFF)], 300.0f, 2500.0f));
    bass.ratio = ev[P_VOICE(CH_BASS, VB_RATIO)] ? 2.0f : 1.0f;
    bass.sub = tier >= TIER_SEVEN ? 0.0f : lin(ev[P_VOICE(CH_BASS, VB_SUB)], 0.0f, 1.0f);
    bass.sus = lin(ev[P_VOICE(CH_BASS, VB_SUSTAIN)], 0.0f, 1.0f);
    lead.inst = ev[P_VOICE(CH_LEAD, VL_INST)] ? 1.0f : 0.0f;
    lead.bright = lin(ev[P_VOICE(CH_LEAD, VL_BRIGHT)], 0.3f, 3.0f);
    lead.att = 1.0f / (lin(ev[P_VOICE(CH_LEAD, VL_ATTACK)], 10.0f, 150.0f) * 0.001f * SR);
    lead.rel = t60k(lin(ev[P_VOICE(CH_LEAD, VL_RELEASE)], 30.0f, 600.0f));
    lead.vdepth = lin(ev[P_VOICE(CH_LEAD, VL_VIBRATO)], 0.0f, 40.0f) * (1.0f / 1200.0f);   /* octaves */
    (void)tier;
}

void voice_trig(uint32_t ch, int32_t note, uint32_t vel)
{
    float v = (float)(vel > 127u ? 127u : vel) * (1.0f / 127.0f);
    switch (ch) {
    case CH_KICK:                                /* phase 0: the sine starts from zero, no click */
        kick.ph = 0.0f;
        kick.penv = kick.cenv = 1.0f;
        kick.amp = 1.0f;
        kick.vel = v;
        kick.on = 1;
        break;
    case CH_SNARE:
        rim.pc = rim.pm = rim.p2 = 0.0f;
        rim.ienv = rim.amp = rim.nenv = 1.0f;
        rim.vel = v;
        rim.on = 1;
        break;
    case CH_HATS:                                /* a closed hit chokes an open one: the one envelope, restarted */
        hat.k = note == 46 ? hat.ko : hat.kc;
        hat.amp = 1.0f;
        hat.vel = v;
        hat.on = 1;
        break;
    case CH_LEAD:                                /* mono, the last note wins; the phases run on */
        lead.note = note;
        lead.f = midi_hz(note);
        lead.vel = v;
        lead.t = 0.0f;
        lead.scoop = 1.0f;
        lead.on = 1;
        break;
    case CH_BASS:                                /* mono, the last note wins; the phases run on (no click) */
        bass.note = note;
        bass.f = midi_hz(note);
        bass.ienv = 1.0f;
        bass.vel = v;
        bass.target = 1.0f;                      /* the attack, then the decay to the sustain */
        bass.on = 1;
        break;
    default:
        break;
    }
}

void voice_release(uint32_t ch, int32_t note)
{
    if (ch == CH_LEAD && lead.note == note)
        lead.note = -1;
    if (ch == CH_BASS && bass.note == note)
        bass.note = -1;
}

int32_t voice_note(uint32_t ch) { return ch == CH_BASS ? bass.note : ch == CH_LEAD ? lead.note : -1; }

void voices_panic(void)
{
    voices_init();
}

static int render_kick(float *o, uint32_t n)
{
    uint32_t i;
    if (!kick.on)
        return 0;
    for (i = 0; i < n; i++) {
        float f = kick.f1 + (kick.f0 - kick.f1) * kick.penv, m;
        kick.ph = wrap(kick.ph + f * INV_SR);
        m = kick.click * kick.cenv * fm_sin_turns(kick.ph);       /* the beater: a short 1:1 index burst */
        o[i] = fm_sin_turns(kick.ph + m * 0.15915494f) * kick.amp * kick.vel * 0.7f;
        kick.penv *= kick.pk;
        kick.cenv = undenorm(kick.cenv * kick.ck);
        kick.amp *= kick.ak;
    }
    kick.penv = undenorm(kick.penv);
    if (kick.amp < SILENT)
        kick.on = 0;
    return 1;
}

static int render_rim(float *o, uint32_t n)
{
    uint32_t i;
    if (!rim.on)
        return 0;
    for (i = 0; i < n; i++) {
        float s;
        rim.pc = wrap(rim.pc + rim.f * INV_SR);
        rim.pm = wrap(rim.pm + rim.f * 1.41f * INV_SR);
        rim.p2 = wrap(rim.p2 + rim.f * 3.5f * INV_SR);
        s = fm_sin_turns(rim.pc + rim.index * rim.ienv * 0.15915494f * fm_sin_turns(rim.pm));
        s += 0.35f * rim.ienv * fm_sin_turns(rim.p2);
        s += rim.noise_l * rim.nenv * noise();
        rim.hp_y = undenorm(rim.hp_a * (rim.hp_y + s - rim.hp_x));   /* high-pass ~300 Hz */
        rim.hp_x = s;
        o[i] = rim.hp_y * rim.amp * rim.vel * 0.45f;
        rim.ienv *= rim.ik;
        rim.nenv *= rim.nk;
        rim.amp *= rim.ak;
    }
    rim.ienv = undenorm(rim.ienv);
    rim.nenv = undenorm(rim.nenv);
    if (rim.amp < SILENT)
        rim.on = 0;
    return 1;
}

static int render_hats(float *o, uint32_t n)
{
    uint32_t i, k;
    const float a = 0.37f;                       /* two one-pole high-passes at ~7 kHz */
    if (!hat.on)
        return 0;
    for (i = 0; i < n; i++) {
        float s = 0.0f, y;
        for (k = 0; k < 4u; k++) {
            hat.p[k] = wrap(hat.p[k] + hat.f * HAT_R[k] * INV_SR);
            s += hat.p[k] < 0.5f ? 0.25f : -0.25f;
        }
        s = s * (1.0f - hat.noise_l) + hat.noise_l * noise();
        y = a * (hat.h1y + s - hat.h1x);
        hat.h1x = s;
        hat.h1y = undenorm(y);
        s = a * (hat.h2y + y - hat.h2x);
        hat.h2x = y;
        hat.h2y = undenorm(s);
        o[i] = s * hat.amp * hat.vel * 2.4f;     /* (the high-passes take most of the squares away) */
        hat.amp *= hat.k;
    }
    if (hat.amp < SILENT)
        hat.on = 0;
    return 1;
}

static int render_bass(float *o, uint32_t n)
{
    uint32_t i;
    if (!bass.on)
        return 0;
    for (i = 0; i < n; i++) {
        float m, s, env_to;
        bass.pm = wrap(bass.pm + bass.f * bass.ratio * INV_SR);
        bass.pc = wrap(bass.pc + bass.f * INV_SR);
        bass.ps = wrap(bass.ps + bass.f * 0.5f * INV_SR);
        m = (bass.index * bass.ienv + 0.04f) * fm_sin_turns(bass.pm);   /* a pluck, then nearly a pure sine */
        s = fm_sin_turns(bass.pc + m * 0.15915494f) + bass.sub * fm_sin_turns(bass.ps);
        /* the envelope: up in 2 ms, then down to the sustain while held, released in 60 ms */
        if (bass.target > 0.5f) {
            bass.amp += bass.att;
            if (bass.amp >= 1.0f) {
                bass.amp = 1.0f;
                bass.target = 0.0f;
            }
        } else {
            env_to = bass.note >= 0 ? bass.sus : 0.0f;
            bass.amp = env_to + (bass.amp - env_to) * (bass.note >= 0 ? bass.ak : bass.rk);
        }
        bass.lp1 = undenorm(bass.lp1 + bass.lp_a * (s - bass.lp1));
        bass.lp2 = undenorm(bass.lp2 + bass.lp_a * (bass.lp1 - bass.lp2));
        o[i] = bass.lp2 * bass.amp * bass.vel * 0.5f;
        bass.ienv *= bass.ik;
    }
    bass.ienv = undenorm(bass.ienv);
    if (bass.note < 0 && bass.amp < SILENT) {
        bass.on = 0;
        bass.amp = 0.0f;
    }
    return 1;
}

static int render_lead(float *o, uint32_t n)
{
    uint32_t i;
    if (!lead.on)
        return 0;
    for (i = 0; i < n; i++) {
        float f = lead.f, s, env, idx, vib = 0.0f;
        /* the envelope: up over ATTACK while held, down over RELEASE after */
        if (lead.note >= 0)
            lead.amp = fm_minf(1.0f, lead.amp + lead.att);
        else
            lead.amp *= lead.rel;
        env = lead.amp;
        lead.t += INV_SR;
        if (lead.t > 0.3f)                       /* vibrato, after 300 ms (the player's breath settles) */
            vib = lead.vdepth * fm_sin_turns(lead.vib);
        lead.vib = wrap(lead.vib + 4.5f * INV_SR);
        if (lead.inst > 0.5f) {                  /* brass: a scoop up into the note, the index with the loudness */
            lead.scoop *= 0.9994f;               /* ~40 ms */
            f *= fm_exp2f(vib - lead.scoop * (1.0f / 12.0f));
            lead.pc = wrap(lead.pc + f * INV_SR);
            idx = lead.bright * env * (0.4f + 0.6f * lead.vel);
            s = fm_sin_turns(lead.pc + idx * 0.15915494f * fm_sin_turns(lead.pc));
            {                                    /* the doubling: a second player, 12 ms behind */
                float d = dbl[dbl_pos];
                dbl[dbl_pos] = s;
                dbl_pos = dbl_pos + 1u == DOUBLE_N ? 0u : dbl_pos + 1u;
                s = 0.7f * s + 0.45f * d;
            }
        } else {                                 /* melodica: a self-fed modulator, two carriers beating */
            float m;
            f *= fm_exp2f(vib);
            lead.pm = wrap(lead.pm + f * INV_SR);
            m = fm_sin_turns(lead.pm + 0.12f * lead.mprev);   /* feedback: a reedy, saw-like spectrum */
            lead.mprev = m;
            idx = lead.bright * (0.5f + 0.5f * lead.vel);
            lead.pc = wrap(lead.pc + f * INV_SR);
            lead.pc2 = wrap(lead.pc2 + f * 1.0035f * INV_SR);   /* +6 cents: the free reeds beating */
            s = 0.5f * (fm_sin_turns(lead.pc + idx * 0.15915494f * m) + fm_sin_turns(lead.pc2 + idx * 0.15915494f * m));
        }
        o[i] = s * env * lead.vel * 0.3f;
    }
    lead.amp = undenorm(lead.amp);
    lead.mprev = undenorm(lead.mprev);
    if (lead.note < 0 && lead.amp < 1e-4f) {
        lead.on = 0;
        lead.amp = 0.0f;
    }
    return 1;
}

int voice_render(uint32_t ch, float *o, uint32_t n)
{
    switch (ch) {
    case CH_KICK:
        return render_kick(o, n);
    case CH_SNARE:
        return render_rim(o, n);
    case CH_HATS:
        return render_hats(o, n);
    case CH_BASS:
        return render_bass(o, n);
    case CH_LEAD:
        return render_lead(o, n);
    default:
        return 0;
    }
}
