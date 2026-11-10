/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* voices.h. Compiled on its own at -O2 (fm1.toml o2). Floats only. */
#include "voices.h"
#include "params.h"
#include "fastmath.h"
#include "dsp.h"
#include "d808.h"                                /* 7.6: the 808 models, after Plaits */
#include "plat.h"                                /* PLAT_POOL (7.3: the reed's tables) */
#include "horn_data.h"                           /* 7.7: HORN's bell and horns, fitted (tools/brass/) */

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
    float ph, penv, amp, cenv, vel, pm, tenv;    /* state (pm: the beater's own phase, CLICK TONE off 0) */
    float f1, f0, pk, ak, ck, click, drive, cr; /* the block's coefficients (drive 0: clean; cr 1: 1:1) */
    float thump, tk;                             /* 5.7 FM THUMP: the body's own 1:1 burst (0: none) */
    float tmul, dinv;                            /* 5.8: this hit's TUNE lock (a pitch factor) and DECAY lock (1 / time x) */
    int on;
} kick;

/* ------------------------------------------------------------------- rim --- */
static struct {
    float pc, pm, p2, ienv, amp, nenv, hp_x, hp_y, vel, denv;
    float f, index, ak, ik, nk, noise_l, hp_a, drop, dk, p2l;   /* (p2l: the 1:3.5 partial's level; 5.7) */
    float tmul, dinv;                            /* 5.8: this hit's locks */
    int on;
} rim;

/* the next drum hit's step locks (5.8): a semitone a step of TUNE, a DECAY time x 2^(lock / 4) (a quarter .. 3.4x) */
static int8_t lk_tune[3], lk_decay[3];
void voice_locks(uint32_t ch, int32_t tune, int32_t decay)
{
    if (ch <= CH_HATS) {
        lk_tune[ch] = (int8_t)tune;
        lk_decay[ch] = (int8_t)decay;
    }
}
static float lk_take(uint32_t ch, float *tmul)   /* (voice_trig) the locks for this hit, used up: 1 / the time's factor */
{
    float d = 1.0f;
    *tmul = 1.0f;
    if (lk_tune[ch])
        *tmul = fm_exp2f((float)lk_tune[ch] * (1.0f / 12.0f));
    if (lk_decay[ch])
        d = fm_exp2f(-(float)lk_decay[ch] * 0.25f);
    lk_tune[ch] = lk_decay[ch] = 0;
    return d;
}

/* 7.6: the 808 models (d808.c). Their knobs, from each channel's own: KICK TUNE (the pitch, 40..70 Hz), SWEEP (the
 * attack's FM), DECAY (up to ~1 s), CLICK (the tone), DRIVE (the self-FM's punch); RIM TUNE (150..350 Hz), INDEX (the
 * tone: 2 modes, then 5), DECAY (up to ~0.6 s), NOISE (snappy); HAT TONE (the squares' base, 300..800 Hz), HPF (the
 * colour), NOISE (noisiness), CLOSED / OPEN as now */
static int m808[3];                              /* the kick, rim and hat on their 808 model */
static d808_kick_cfg_t k808;
static d808_snare_cfg_t s808;
static d808_hat_cfg_t h808;

/* ------------------------------------------------------------------- hats --- */
static struct {
    float p[4], amp, h1x, h1y, h2x, h2y, vel;
    float f, kc, ko, k, noise_l, hp_a, gain;
    int model;                                   /* 5.7: HAT_METAL, HAT_NOISE (noise alone: the squares not run) */
    int on;
} hat;
static const float HAT_R[4] = {1.0f, 1.48f, 2.27f, 3.6f};

/* ------------------------------------------------------------------- bass --- */
static struct {
    float pc, pm, ps, ienv, amp, lp1, lp2, vel, f, target, mfb, ft;
    float index, ik, ak, rk, att, ratio, sub, sus, lp_a, fb, drive, gk;
    int classic;                                 /* 6.1: the sub's tone: + 1/3 its 3rd + 1/5 its 5th */
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
    float bright, att, rel, vdepth, spread;
    int inst;                                    /* INST_MELODICA, INST_BRASS, INST_REED (7.3), HORN, LAYER (7.7) */
    int horn;                                    /* (7.7) HORN_TRUMPET, HORN_SAX */
    float blend;                                 /* (7.7) FM+HORN: HORN's share, 0.5 .. 1 */
    int32_t note;                                /* held, -1 none */
    int on;
} lead;
/* 7.3: REED, a free-reed melodica after Puranik & Scavone's harmonium model (DAFx-23, CC-BY: its structure, no code):
 * the reed's air flow, rising while the tongue opens and cut off as it passes the plate (one jump a period, rich in
 * odd and even harmonics; PolyBLEP keeps the jump clean), into the instrument's body (three resonances; the paper finds
 * the enclosure shapes the tone more than the reed). The breath sets how far the reed swings (louder: a shorter, brighter
 * pulse) and pulls the pitch a hair (a free reed goes sharp as the pressure falls). The body's resonances are estimates
 * (no melodica measurements are published): 1.1, 2.5 and 4.2 kHz */
typedef struct { float b0, b1, b2, a1, a2, z1, z2; } bq_t;
static bq_t reed_bq[3];
static void bq_peak(bq_t *q, float hz, float qf, float db);
static void reed_tables(void);
static void horn_tables(void);
static void horn_config(void);
static void horn_bright(int32_t b);
static void bq_lpf(bq_t *q, float hz, float qf);
static float reed_air;                           /* the breath's noise, low-passed */
static uint32_t reed_rng = 0x2545F491u;
static float dbl[DOUBLE_N];
static uint32_t dbl_pos;

void voices_start(void)                          /* the transport's start: the noise from its seed (5.1) */
{
    rng = 0x12345678u;
}

void voices_init(void)
{
    d808_init();
    reed_tables();
    horn_tables();
    lead.on = 0;
    lead.note = -1;
    lead.amp = 0.0f;
    kick.on = rim.on = hat.on = bass.on = 0;
    kick.tmul = rim.tmul = kick.dinv = rim.dinv = 1.0f;
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
    kick.drive = lin(ev[P_VOICE(CH_KICK, VK_DRIVE)], 0.0f, 6.0f);   /* 5.3: 0 clean */
    kick.cr = ev[P_VOICE(CH_KICK, VK_CTONE)] ? fm_exp2f((float)ev[P_VOICE(CH_KICK, VK_CTONE)] * (1.0f / 32.0f)) : 1.0f;
    kick.thump = 0.0f;
    if (ev[P_VOICE(CH_KICK, VK_MODEL)] == KICK_THUMP) {   /* 5.7 FM THUMP: a 1:1 burst in the body, ~30 ms */
        kick.thump = 2.2f;
        kick.tk = t60k(30.0f);
    } else if (ev[P_VOICE(CH_KICK, VK_MODEL)] == KICK_BOOM) {   /* BOOM: lower, a gentler sweep, long, driven */
        kick.f1 = lin(ev[P_VOICE(CH_KICK, VK_TUNE)], 34.0f, 52.0f);
        kick.f0 = kick.f1 * 1.8f + 20.0f;
        kick.ak = t60k(lin(ev[P_VOICE(CH_KICK, VK_DECAY)], 300.0f, 1600.0f));   /* (capped: 1.6 s) */
        if (kick.drive < 1.5f)
            kick.drive = 1.5f;
    }
    /* rim: TUNE 300..1200 Hz, DECAY 20..200 ms, NOISE 0..1, INDEX 0..6 */
    rim.f = lin(ev[P_VOICE(CH_SNARE, VR_TUNE)], 300.0f, 1200.0f);
    rim.ak = t60k(lin(ev[P_VOICE(CH_SNARE, VR_DECAY)], 20.0f, 200.0f));
    rim.noise_l = lin(ev[P_VOICE(CH_SNARE, VR_NOISE)], 0.0f, 1.0f);
    rim.index = lin(ev[P_VOICE(CH_SNARE, VR_INDEX)], 0.0f, 6.0f);
    rim.ik = t60k(12.0f);
    rim.nk = t60k(6.0f);
    rim.hp_a = fm_expf(-6.2831853f * 300.0f * fm_exp2f((float)ev[P_VOICE(CH_SNARE, VR_HPF)] * (3.0f / 64.0f)) * INV_SR);
    rim.drop = lin(ev[P_VOICE(CH_SNARE, VR_DROP)], 0.0f, 1.0f);   /* 5.3: up to an octave above, falling */
    rim.dk = t60k(40.0f);
    rim.p2l = 0.35f;
    if (ev[P_VOICE(CH_SNARE, VR_MODEL)] == RIM_SNARE) {   /* 5.7 SNARE: a lower FM body, a longer noise */
        rim.f = lin(ev[P_VOICE(CH_SNARE, VR_TUNE)], 150.0f, 450.0f);
        rim.index *= 0.5f;
        rim.p2l = 0.0f;
        rim.nk = t60k(lin(ev[P_VOICE(CH_SNARE, VR_DECAY)], 60.0f, 250.0f));
        if (rim.noise_l < 0.6f)
            rim.noise_l = 0.6f;
    } else if (ev[P_VOICE(CH_SNARE, VR_MODEL)] == RIM_SYNDRUM) {   /* SYNDRUM: a sine, a big slow drop, no noise */
        rim.f = lin(ev[P_VOICE(CH_SNARE, VR_TUNE)], 120.0f, 500.0f);
        rim.index = 0.0f;
        rim.p2l = 0.0f;
        rim.noise_l = 0.0f;
        rim.ak = t60k(lin(ev[P_VOICE(CH_SNARE, VR_DECAY)], 80.0f, 600.0f));
        if (rim.drop < 0.9f)
            rim.drop = 0.9f;
        rim.dk = t60k(150.0f);
    }
    /* hats: TONE 300..800 Hz, CLOSED 20..150 ms, OPEN 150..800 ms, NOISE 0..1 */
    hat.f = lin(ev[P_VOICE(CH_HATS, VH_TONE)], 300.0f, 800.0f);
    hat.kc = t60k(lin(ev[P_VOICE(CH_HATS, VH_CLOSED)], 20.0f, 150.0f));
    hat.ko = t60k(lin(ev[P_VOICE(CH_HATS, VH_OPEN)], 150.0f, 800.0f));
    hat.noise_l = lin(ev[P_VOICE(CH_HATS, VH_NOISE)], 0.0f, 1.0f);
    hat.model = ev[P_VOICE(CH_HATS, VH_MODEL)];
    if (ev[P_VOICE(CH_HATS, VH_HPF)]) {         /* 5.3: the high-passes moved (at 0 exactly as built: 0.37, ~7 kHz) */
        float fc = 7000.0f * fm_exp2f((float)ev[P_VOICE(CH_HATS, VH_HPF)] * (1.0f / 64.0f));
        hat.hp_a = fm_expf(-6.2831853f * fc * INV_SR);
        hat.gain = 2.4f * fm_sqrtf(fc * (1.0f / 7000.0f));   /* (lower: more of the squares through) */
    } else {
        hat.hp_a = 0.37f;
        hat.gain = 2.4f;
    }
    m808[CH_KICK] = ev[P_VOICE(CH_KICK, VK_MODEL)] == KICK_808;   /* 7.6 */
    k808.f0 = lin(ev[P_VOICE(CH_KICK, VK_TUNE)], 40.0f, 70.0f);
    k808.attack_fm = lin(ev[P_VOICE(CH_KICK, VK_SWEEP)], 0.0f, 1.0f);
    k808.decay = lin(ev[P_VOICE(CH_KICK, VK_DECAY)], 0.0f, 0.55f);
    k808.tone = lin(ev[P_VOICE(CH_KICK, VK_CLICK)], 0.0f, 1.0f);
    k808.self_fm = lin(ev[P_VOICE(CH_KICK, VK_DRIVE)], 0.0f, 1.0f);
    m808[CH_SNARE] = ev[P_VOICE(CH_SNARE, VR_MODEL)] == RIM_808;
    s808.f0 = lin(ev[P_VOICE(CH_SNARE, VR_TUNE)], 150.0f, 350.0f);
    s808.tone = lin(ev[P_VOICE(CH_SNARE, VR_INDEX)], 0.0f, 1.0f);
    s808.decay = lin(ev[P_VOICE(CH_SNARE, VR_DECAY)], 0.0f, 0.5f);
    s808.snappy = lin(ev[P_VOICE(CH_SNARE, VR_NOISE)], 0.0f, 1.0f);
    m808[CH_HATS] = ev[P_VOICE(CH_HATS, VH_MODEL)] == HAT_808;
    h808.f0 = hat.f;
    h808.tone = (float)(ev[P_VOICE(CH_HATS, VH_HPF)] + 64) * (1.0f / 127.0f);
    h808.noisiness = hat.noise_l;
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
    bass.fb = lin(ev[P_VOICE(CH_BASS, VB_FEEDBACK)], 0.0f, 0.6f);   /* 5.3: capped (sound.txt: the sine stays) */
    bass.drive = lin(ev[P_VOICE(CH_BASS, VB_DRIVE)], 0.0f, 5.0f);
    bass.classic = ev[P_VOICE(CH_BASS, VB_SUBTONE)] == SUB_CLASSIC;
    switch (ev[P_VOICE(CH_BASS, VB_MODEL)]) {    /* 5.7: the models are settings of the one voice (no cost) */
    case BASS_PLUCK:                             /* a brighter pluck: its index doubled (capped), the filter open */
        bass.index = bass.index * 2.5f > 3.5f ? 3.5f : bass.index * 2.5f;
        {
            float fc = lin(ev[P_VOICE(CH_BASS, VB_CUTOFF)], 300.0f, 2500.0f) * 2.5f;
            bass.lp_a = onepole(fc > 5000.0f ? 5000.0f : fc);
        }
        break;
    case BASS_HOLLOW:                            /* 2:1, whatever RATIO says */
        bass.ratio = 2.0f;
        break;
    case BASS_STEPPERS:                          /* the modulator fed back, the sub under it */
        if (bass.fb < 0.35f)
            bass.fb = 0.35f;
        if (tier < TIER_SEVEN && bass.sub < 0.5f)
            bass.sub = 0.5f;
        break;
    default:
        break;
    }
    bass.gk = ev[P_VOICE(CH_BASS, VB_GLIDE)] ? 1.0f - fm_expf(-1.0f / (lin(ev[P_VOICE(CH_BASS, VB_GLIDE)], 1.0f, 300.0f)
                                                                         * 0.001f * SR * 0.5f * 0.25f))
                                             : 1.0f;            /* (a one-pole a half-rate sample, ~T/4: there in T) */
    lead.inst = ev[P_VOICE(CH_LEAD, VL_INST)];
    lead.horn = ev[P_VOICE(CH_LEAD, VL_HORN)] == HORN_SAX ? HORN_SAX : HORN_TRUMPET;
    lead.blend = 0.5f + 0.5f * (float)ev[P_VOICE(CH_LEAD, VL_BLEND)] * (1.0f / 127.0f);
    horn_config();
    horn_bright(ev[P_VOICE(CH_LEAD, VL_BRIGHT)]);   /* (7.7: BRIGHT is the brass's brightness) */
    lead.bright = lin(ev[P_VOICE(CH_LEAD, VL_BRIGHT)], 0.3f, 3.0f);
    lead.att = 1.0f / (lin(ev[P_VOICE(CH_LEAD, VL_ATTACK)], 10.0f, 150.0f) * 0.001f * SR);
    lead.rel = t60k(lin(ev[P_VOICE(CH_LEAD, VL_RELEASE)], 30.0f, 600.0f));
    lead.spread = ev[P_VOICE(CH_LEAD, VL_DETUNE)] ? 0.0035f * fm_exp2f((float)ev[P_VOICE(CH_LEAD, VL_DETUNE)] * (1.0f / 32.0f))
                                                  : 0.0035f;   /* 5.3: 6 cents at 0 */
    lead.vdepth = lin(ev[P_VOICE(CH_LEAD, VL_VIBRATO)], 0.0f, 40.0f) * (1.0f / 1200.0f);   /* octaves */
    if (lead.inst == INST_REED) {                /* (7.3: the body, fitted 2026-11-10 to 34 melodica notes; BRIGHT
                                                  * slides its low-pass an octave either way and its upper resonance
                                                  * +-4 dB; at 64, as measured) */
        float b = ((float)ev[P_VOICE(CH_LEAD, VL_BRIGHT)] - 64.0f) * (1.0f / 63.0f);
        bq_lpf(&reed_bq[0], 1992.0f * fm_exp2f(b), 2.60f);   /* (refitted with the puff, 12 real notes) */
        bq_peak(&reed_bq[1], 3749.0f * fm_exp2f(0.5f * b), 1.37f, 11.4f + 4.0f * b);
    }
    (void)tier;
}

void voice_trig(uint32_t ch, int32_t note, uint32_t vel)
{
    float v = (float)(vel > 127u ? 127u : vel) * (1.0f / 127.0f);
    switch (ch) {
    case CH_KICK:                                /* phase 0: the sine starts from zero, no click */
        kick.dinv = lk_take(CH_KICK, &kick.tmul);
        if (m808[CH_KICK]) {                     /* 7.6 */
            d808_kick_trig(&k808, v, kick.tmul, kick.dinv);
            break;
        }
        kick.ph = kick.pm = 0.0f;
        kick.penv = kick.cenv = kick.tenv = 1.0f;
        kick.amp = 1.0f;
        kick.vel = v;
        kick.on = 1;
        break;
    case CH_SNARE:
        if (!rim.on)                             /* (from silence: its high-pass from rest too, 5.1) */
            rim.hp_x = rim.hp_y = 0.0f;
        rim.dinv = lk_take(CH_SNARE, &rim.tmul);
        if (m808[CH_SNARE]) {                    /* 7.6 */
            d808_snare_trig(&s808, v, rim.tmul, rim.dinv);
            break;
        }
        rim.pc = rim.pm = rim.p2 = 0.0f;
        rim.denv = 1.0f;
        rim.ienv = rim.amp = rim.nenv = 1.0f;
        rim.vel = v;
        rim.on = 1;
        break;
    case CH_HATS:                                /* a closed hit chokes an open one: the one envelope, restarted */
        if (!hat.on) {                           /* from silence: its squares and filters from rest (5.1: a pass and
                                                  * its replay the same; nothing to click, it was silent) */
            uint32_t k;
            for (k = 0; k < 4u; k++)
                hat.p[k] = 0.0f;
            hat.h1x = hat.h1y = hat.h2x = hat.h2y = 0.0f;
        }
        {                                        /* (its DECAY lock: the TUNE lock is the hats' closed / open) */
            float tm, d = lk_take(CH_HATS, &tm);
            hat.k = note == 46 ? hat.ko : hat.kc;
            if (d != 1.0f)
                hat.k = fm_powf(hat.k, d);
            if (m808[CH_HATS]) {                 /* 7.6 */
                h808.k = hat.k;
                d808_hat_trig(&h808, v);
                break;
            }
        }
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
    case CH_BASS:                                /* mono, the last note wins; the phases run on (no click), but from
                                                  * silence they start at 0 (5.1: a pass and its replay the same) */
        if (!bass.on) {
            bass.pc = bass.pm = bass.ps = bass.mfb = 0.0f;
            bass.lp1 = bass.lp2 = bass.amp = 0.0f;
        }
        bass.note = note;
        bass.ft = midi_hz(note);
        if (!bass.on || bass.gk >= 1.0f)
            bass.f = bass.ft;                    /* (GLIDE: from the last note's pitch; from silence, none) */
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
    float ak;
    if (!kick.on)
        return 0;
    ak = kick.dinv != 1.0f ? fm_powf(kick.ak, kick.dinv) : kick.ak;   /* (a DECAY lock: 5.8) */
    for (i = 0; i < n; i++) {
        float f = (kick.f1 + (kick.f0 - kick.f1) * kick.penv) * kick.tmul, m;
        float y;
        kick.ph = wrap(kick.ph + f * INV_SR);
        if (kick.cr != 1.0f) {                   /* CLICK TONE: the beater's own ratio (5.3) */
            kick.pm = wrap(kick.pm + f * kick.cr * INV_SR);
            m = kick.click * kick.cenv * fm_sin_turns(kick.pm);
        } else
            m = kick.click * kick.cenv * fm_sin_turns(kick.ph);   /* the beater: a short 1:1 index burst */
        if (kick.thump > 0.0f) {                 /* FM THUMP (5.7): the body's own longer burst */
            m += kick.thump * kick.tenv * fm_sin_turns(kick.ph);
            kick.tenv *= kick.tk;
        }
        y = fm_sin_turns(kick.ph + m * 0.15915494f);
        if (kick.drive > 0.0f)                   /* DRIVE (5.3): the sine soft-clipped towards a square, before the
                                                  * envelope (after it, it lifted the tail: tails.txt) */
            y = y * (1.0f + kick.drive) / (1.0f + kick.drive * fm_fabsf(y));
        o[i] = y * kick.amp * kick.vel * 0.7f;
        kick.penv *= kick.pk;
        kick.cenv = undenorm(kick.cenv * kick.ck);
        kick.amp *= ak;
    }
    kick.penv = undenorm(kick.penv);
    kick.tenv = undenorm(kick.tenv);
    if (kick.amp < SILENT)
        kick.on = 0;
    return 1;
}

static int render_rim(float *o, uint32_t n)
{
    uint32_t i;
    float ak;
    if (!rim.on)
        return 0;
    ak = rim.dinv != 1.0f ? fm_powf(rim.ak, rim.dinv) : rim.ak;   /* (a DECAY lock: 5.8) */
    for (i = 0; i < n; i++) {
        float s, f = rim.f * rim.tmul;
        if (rim.drop > 0.0f) {                   /* PITCH DROP (5.3): from up to an octave above, quickly down */
            f *= 1.0f + rim.drop * rim.denv;
            rim.denv *= rim.dk;
        }
        rim.pc = wrap(rim.pc + f * INV_SR);
        if (rim.index > 0.0f) {                  /* (SYNDRUM, 5.7: none: a plain sine) */
            rim.pm = wrap(rim.pm + f * 1.41f * INV_SR);
            s = fm_sin_turns(rim.pc + rim.index * rim.ienv * 0.15915494f * fm_sin_turns(rim.pm));
        } else
            s = fm_sin_turns(rim.pc);
        if (rim.p2l > 0.0f) {                    /* (SNARE, SYNDRUM: none) */
            rim.p2 = wrap(rim.p2 + f * 3.5f * INV_SR);
            s += rim.p2l * rim.ienv * fm_sin_turns(rim.p2);
        }
        if (rim.noise_l > 0.0f)
            s += rim.noise_l * rim.nenv * noise();
        rim.hp_y = undenorm(rim.hp_a * (rim.hp_y + s - rim.hp_x));   /* high-pass ~300 Hz */
        rim.hp_x = s;
        o[i] = rim.hp_y * rim.amp * rim.vel * 0.45f;
        rim.ienv *= rim.ik;
        rim.nenv *= rim.nk;
        rim.amp *= ak;
    }
    rim.ienv = undenorm(rim.ienv);
    rim.nenv = undenorm(rim.nenv);
    rim.denv = undenorm(rim.denv);
    if (rim.amp < SILENT)
        rim.on = 0;
    return 1;
}

static int render_hats(float *o, uint32_t n)
{
    uint32_t i, k;
    const float a = hat.hp_a;                    /* two one-pole high-passes at ~7 kHz (HPF moves them, 5.3) */
    if (!hat.on)
        return 0;
    for (i = 0; i < n; i++) {
        float s = 0.0f, y;
        if (hat.model == HAT_NOISE)              /* NOISE (5.7): noise alone, the squares not run (at METAL's level) */
            s = 0.4f * noise();
        else {
            for (k = 0; k < 4u; k++) {
                hat.p[k] = wrap(hat.p[k] + hat.f * HAT_R[k] * INV_SR);
                s += hat.p[k] < 0.5f ? 0.25f : -0.25f;
            }
            s = s * (1.0f - hat.noise_l) + hat.noise_l * noise();
        }
        y = a * (hat.h1y + s - hat.h1x);
        hat.h1x = s;
        hat.h1y = undenorm(y);
        s = a * (hat.h2y + y - hat.h2x);
        hat.h2x = y;
        hat.h2y = undenorm(s);
        o[i] = s * hat.amp * hat.vel * hat.gain;   /* (the high-passes take most of the squares away) */
        hat.amp *= hat.k;
    }
    if (hat.amp < SILENT)
        hat.on = 0;
    return 1;
}

/* The bass at half rate (22,050 Hz, the memory plan's rate for tonal voices; 2026-11 review): a sample of it every
 * other output sample, the one between them the two's mean (a sample's fixed delay). Its per-sample constants for the
 * half rate: decays squared, steps doubled, the one-poles' coefficients as two steps of theirs */
static float bass_last;
static int bass_odd;

static int render_bass(float *o, uint32_t n)
{
    uint32_t i;
    float inv2 = 2.0f * INV_SR, ik2 = bass.ik * bass.ik, ak2 = bass.ak * bass.ak, rk2 = bass.rk * bass.rk;
    float att2 = 2.0f * bass.att, lp2a = 1.0f - (1.0f - bass.lp_a) * (1.0f - bass.lp_a);
    if (!bass.on) {
        bass_last = 0.0f;
        bass_odd = 0;
        return 0;
    }
    for (i = 0; i < n; i++) {
        float m, s, env_to, y;
        if (!bass_odd) {
            o[i] = bass_last;
            bass_odd = 1;
            continue;
        }
        bass_odd = 0;
        if (bass.gk < 1.0f)                      /* GLIDE (5.3) */
            bass.f += (bass.ft - bass.f) * bass.gk;
        bass.pm = wrap(bass.pm + bass.f * bass.ratio * inv2);
        bass.pc = wrap(bass.pc + bass.f * inv2);
        bass.ps = wrap(bass.ps + bass.f * 0.5f * inv2);
        if (bass.fb > 0.0f) {                    /* FEEDBACK (5.3): the modulator fed back, a growl (capped) */
            float mm = fm_sin_turns(bass.pm + bass.fb * bass.mfb);
            bass.mfb = mm;
            m = (bass.index * bass.ienv + 0.04f) * mm;
        } else
            m = (bass.index * bass.ienv + 0.04f) * fm_sin_turns(bass.pm);   /* a pluck, then nearly a pure sine */
        s = fm_sin_turns(bass.pc + m * 0.15915494f);
        if (bass.drive > 0.0f)                   /* DRIVE (5.3): a soft clip before the filter */
            s = s * (1.0f + bass.drive) / (1.0f + bass.drive * 0.5f * fm_fabsf(s));
        /* the envelope: up in 2 ms, then down to the sustain while held, released in 60 ms */
        if (bass.target > 0.5f) {
            bass.amp += att2;
            if (bass.amp >= 1.0f) {
                bass.amp = 1.0f;
                bass.target = 0.0f;
            }
        } else {
            env_to = bass.note >= 0 ? bass.sus : 0.0f;
            bass.amp = env_to + (bass.amp - env_to) * (bass.note >= 0 ? ak2 : rk2);
        }
        bass.lp1 = undenorm(bass.lp1 + lp2a * (s - bass.lp1));
        bass.lp2 = undenorm(bass.lp2 + lp2a * (bass.lp1 - bass.lp2));
        /* the sub, after the drive and the filter (6.1, after kwasub and rupture: it stays clean, and the drive
         * doesn't beat against it); CLASSIC adds a third of its 3rd and a fifth of its 5th, so a small speaker hears
         * it (rupture's classic sub) */
        s = bass.lp2;
        if (bass.sub > 0.0f) {
            float u = fm_sin_turns(bass.ps);
            if (bass.classic) {
                float p3 = bass.ps * 3.0f, p5 = bass.ps * 5.0f;
                u += (1.0f / 3.0f) * fm_sin_turns(p3 - (float)(int32_t)p3) + 0.2f * fm_sin_turns(p5 - (float)(int32_t)p5);
                u *= 0.75f;                      /* (its peak as the sine's) */
            }
            s += bass.sub * u;
        }
        y = s * bass.amp * bass.vel * 0.5f;
        o[i] = 0.5f * (bass_last + y);
        bass_last = y;
        bass.ienv *= ik2;
    }
    bass.ienv = undenorm(bass.ienv);
    if (bass.note < 0 && bass.amp < SILENT) {
        bass.on = 0;
        bass.amp = 0.0f;
    }
    return 1;
}

static void bq_peak(bq_t *q, float hz, float qf, float db)   /* RBJ's peaking EQ (coefficients only; state kept) */
{
    float a = fm_exp2f(db * (1.0f / 12.0412f)), w = 6.2831853f * hz * INV_SR, al = fm_sinf(w) / (2.0f * qf),
          c = fm_cosf(w), a0 = 1.0f / (1.0f + al / a);
    q->b0 = (1.0f + al * a) * a0;
    q->b1 = -2.0f * c * a0;
    q->b2 = (1.0f - al * a) * a0;
    q->a1 = q->b1;
    q->a2 = (1.0f - al / a) * a0;
}
static void bq_lpf(bq_t *q, float hz, float qf)   /* RBJ's low-pass (7.3's fitted body) */
{
    float w = 6.2831853f * fm_minf(hz, 18000.0f) * INV_SR, al = fm_sinf(w) / (2.0f * qf), c = fm_cosf(w),
          a0 = 1.0f / (1.0f + al);
    q->b0 = 0.5f * (1.0f - c) * a0;
    q->b1 = (1.0f - c) * a0;
    q->b2 = q->b0;
    q->a1 = -2.0f * c * a0;
    q->a2 = (1.0f - al) * a0;
}
static void bq_hpf(bq_t *q, float hz, float qf)   /* RBJ's high-pass (7.7's bell) */
{
    float w = 6.2831853f * hz * INV_SR, al = fm_sinf(w) / (2.0f * qf), c = fm_cosf(w), a0 = 1.0f / (1.0f + al);
    q->b0 = 0.5f * (1.0f + c) * a0;
    q->b1 = -(1.0f + c) * a0;
    q->b2 = q->b0;
    q->a1 = -2.0f * c * a0;
    q->a2 = (1.0f - al) * a0;
}
static void bq_bpf(bq_t *q, float hz, float qf)   /* RBJ's band-pass, 0 dB peak (7.7's breath) */
{
    float w = 6.2831853f * hz * INV_SR, al = fm_sinf(w) / (2.0f * qf), c = fm_cosf(w), a0 = 1.0f / (1.0f + al);
    q->b0 = al * a0;
    q->b1 = 0.0f;
    q->b2 = -al * a0;
    q->a1 = -2.0f * c * a0;
    q->a2 = (1.0f - al) * a0;
}
static inline float bq_run(bq_t *q, float x)    /* transposed direct form II */
{
    float y = q->b0 * x + q->z1;
    q->z1 = undenorm(q->b1 * x - q->a1 * y + q->z2);
    q->z2 = undenorm(q->b2 * x - q->a2 * y);
    return y;
}
/* 7.3, refitted 2026-11-10 by ear and measure: the reed's air comes in a short puff, a half-sine open 8 % (blown hard)
 * to 15 % (soft) of each cycle: its harmonics level to about the 12th, then a cliff (the real ones' brightness and
 * buzz; the first REED's flow, open half the cycle, was dull). The puff band-limited exactly: its Fourier series, in a
 * table an octave of notes (the top note's harmonics under 20 kHz), two widths, built at boot into the pool */
#define REED_GAIN 2.8f
#define HORN_GAIN 1.0f                           /* (7.7: BRASS's loudness; set from measurements) */
#define RT_N 1024u
#define RT_OCT 6u                                /* tables for notes up to 130, 260 .. 4,160 Hz */
static PLAT_POOL int16_t reed_tab[2][RT_OCT][RT_N];
static const float RT_DELTA[2] = {0.15f, 0.08f}; /* soft, hard */
static float reed_sample(float d, uint32_t K, float ph)   /* the puff's Fourier series to harmonic K, at phase ph */
{
    float a = 3.14159265f / d, acc = 0.0f;
    uint32_t k;
    for (k = 1; k <= K; k++) {
        /* c_k = integral over the puff of sin(pi t / d) e^(-i 2 pi k t) = a (1 + e^(-i w d)) / (a^2 - w^2) */
        float wk = 6.2831853f * (float)k, den = a * a - wk * wk, re, im, kp = (float)k * ph;
        if (fm_fabsf(den) < 1e-3f * a * a) {   /* (w = a: the limit, -i d / 2) */
            re = 0.0f;
            im = -0.5f * d;
        } else {
            re = a * (1.0f + fm_cosf(wk * d)) / den;
            im = -a * fm_sinf(wk * d) / den;
        }
        kp -= (float)(int32_t)kp;
        acc += 2.0f * (re * fm_sin_turns(kp + 0.25f >= 1.0f ? kp - 0.75f : kp + 0.25f) - im * fm_sin_turns(kp));
    }
    return acc;
}
static void reed_tables(void)                    /* (boot; no scratch: each sample summed in place, scaled by the
                                                  * puff's own peak, at its middle) */
{
    uint32_t w, o, n;
    for (w = 0; w < 2u; w++)
        for (o = 0; o < RT_OCT; o++) {
            float d = RT_DELTA[w], top = 65.0f * (float)(2u << o);
            uint32_t K = (uint32_t)(20000.0f / top);
            float sc = 27000.0f / reed_sample(d, K, 0.5f * d);
            for (n = 0; n < RT_N; n++) {
                float v = reed_sample(d, K, (float)n * (1.0f / (float)RT_N)) * sc;
                reed_tab[w][o][n] = (int16_t)(v > 32767.0f ? 32767.0f : v < -32768.0f ? -32768.0f : v);
            }
        }
}
static inline float reed_read(const int16_t *t, float ph)
{
    float x = ph * (float)RT_N;
    uint32_t i = (uint32_t)x;
    float fr = x - (float)i, a = (float)t[i & (RT_N - 1u)], b = (float)t[(i + 1u) & (RT_N - 1u)];
    return (a + fr * (b - a)) * (1.0f / 30000.0f);   /* (the scale's 27,000 at the puff's peak; as before, 30,000) */
}
static inline uint32_t reed_oct(float f)         /* the table whose top note is at or over f */
{
    uint32_t o = 0;
    float top = 130.0f;
    while (o + 1u < RT_OCT && f > top) {
        top *= 2.0f;
        o++;
    }
    return o;
}

/* 7.7: HORN, a horn fitted to recordings of real trumpets and saxophones (Freesound; tools/brass/): a source whose
 * harmonics fall off less steeply the harder it is blown (two tables, soft and loud, crossfaded by the breath: the
 * blare), each horn's own uneven pattern of harmonics, into a bell (a high-pass, a resonance, a low-pass, fitted on 17
 * notes); and what makes a held note alive, at the amounts measured on them: the pitch wandering ~3.5 cents (2 Hz) and
 * jittering ~1.2 (30 Hz), the level ~0.9 dB, and breath noise pulsed once a cycle. Tables an octave apart, band-limited,
 * built at boot into the pool. FM+HORN layers it with BRASS (BLEND: 0 half each, 127 all HORN) */
#define HT_N 512u
#define HT_OCT 5u                                /* notes up to 130 .. 2,080 Hz (the lead's top: C7 shares the last) */
static PLAT_POOL int16_t horn_tab[HORN_N][2][HT_OCT][HT_N];
static float horn_rms[HORN_N][2][HT_OCT];
static void horn_tables(void)                    /* (boot) */
{
    uint32_t h, d, o, n, k;
    for (h = 0; h < HORN_N; h++)
        for (d = 0; d < 2u; d++)
            for (o = 0; o < HT_OCT; o++) {
                float top = 65.0f * (float)(2u << o), pk = 0.0f, ms = 0.0f, sc;
                uint32_t K = (uint32_t)(18000.0f / top);
                for (n = 0; n < HT_N; n++) {     /* (a first pass for the peak, a second to write: no scratch) */
                    float v = 0.0f;
                    for (k = 1; k <= K; k++) {
                        float db = -HORN_SLOPE[h][d] * fm_log2f((float)k)
                                   + (float)(k <= HORN_NK ? HORN_RES[2u * h + d][k - 1u] : -15);
                        v += fm_exp2f(db * 0.16609640f) * fm_sin_turns((float)(k * n % HT_N) * (1.0f / (float)HT_N) + 0.25f
                                                                      - (float)((k * n % HT_N) * 4u >= 3u * HT_N));
                    }
                    pk = fm_maxf(pk, fm_fabsf(v));
                }
                sc = 27000.0f / pk;
                for (n = 0; n < HT_N; n++) {
                    float v = 0.0f;
                    for (k = 1; k <= K; k++) {
                        float db = -HORN_SLOPE[h][d] * fm_log2f((float)k)
                                   + (float)(k <= HORN_NK ? HORN_RES[2u * h + d][k - 1u] : -15);
                        v += fm_exp2f(db * 0.16609640f) * fm_sin_turns((float)(k * n % HT_N) * (1.0f / (float)HT_N) + 0.25f
                                                                      - (float)((k * n % HT_N) * 4u >= 3u * HT_N));
                    }
                    v *= sc;
                    horn_tab[h][d][o][n] = (int16_t)v;
                    ms += (v * (1.0f / 27000.0f)) * (v * (1.0f / 27000.0f));
                }
                horn_rms[h][d][o] = fm_sqrtf(ms / (float)HT_N);
            }
}
static struct {
    float ph, ws, wf, ls, nz;                    /* phase; the wander, the jitter, the level's wander (one-poles) */
    uint32_t rng;
    bq_t hp, pk, lp, nbp;                        /* the bell; the breath's band */
    int set;
    float tongue;                                /* the tongue's burst, this sample (render_lead) */
    bq_t blare;                                  /* the brass's blare: a broad lift at 2.5 kHz from BRIGHT (7.7) */
    int32_t blare_b;                             /* the BRIGHT it was set for (-1: none yet) */
} horn = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0x7F4A7C15u, {0}, {0}, {0}, {0}, 0, 0.0f, {0}, -1};
#define HORN_TILT_DB(b) (((float)(b) - 64.0f) * (6.0f / 64.0f) + HORN_TILT0)   /* BRIGHT 0..127: -6 .. +6 dB about
                                                  * the default's */
#define HORN_ATT (1.0f / (0.012f * SR))
#define HORN_TILT0 12.0f                         /* the default BRIGHT's blare, dB: the reggae sections' brightness (Blazing
                                                  * Horns, Black Wa-Da-Da: ~1.85 kHz in tools/studies/hornstat.py) */
#define HORN_BLARE_HZ 4000.0f
#define HORN_BLARE_Q 0.5f
#define HORN_TILT_LEVEL 0.68f                    /* (its loudness put back: HORN as loud as before) */
static void bq_hpf(bq_t *q, float hz, float qf);
static void bq_bpf(bq_t *q, float hz, float qf);
static void horn_bright(int32_t b)               /* BRIGHT: the blare's lift (set when it changes) */
{
    if (b == horn.blare_b)
        return;
    bq_peak(&horn.blare, HORN_BLARE_HZ, HORN_BLARE_Q, HORN_TILT_DB(b));
    horn.blare_b = b;
}
static void horn_config(void)                    /* (7.7: the bell's, fixed; the breath's band) */
{
    if (horn.set)
        return;
    bq_hpf(&horn.hp, HORN_HP_HZ, HORN_HP_Q);
    bq_peak(&horn.pk, HORN_PK_HZ, HORN_PK_Q, HORN_PK_DB);
    bq_lpf(&horn.lp, HORN_LP_HZ, HORN_LP_Q);
    bq_bpf(&horn.nbp, 2500.0f, 0.7f);
    horn.set = 1;
}
static inline float horn_rnd(void)
{
    horn.rng ^= horn.rng << 13;
    horn.rng ^= horn.rng >> 17;
    horn.rng ^= horn.rng << 5;
    return (float)(int32_t)horn.rng * (1.0f / 2147483648.0f);
}
static inline uint32_t horn_oct(float f)
{
    uint32_t o = 0;
    float top = 130.0f;
    while (o + 1u < HT_OCT && f > top) {
        top *= 2.0f;
        o++;
    }
    return o;
}
static float horn_tick(float f, float hard, uint32_t type)   /* one sample of the horn at f (its own movement on top) */
{
    /* white noise through one-poles; (1 - a) / (1 + a)'s root, its spread, divided out: 2 Hz 0.0185, 30 Hz 0.0718, 1.5 Hz
     * 0.0164 (at 44.1 kHz) */
    float cents, lev, x, s, t;
    uint32_t o;
    horn.ws += (horn_rnd() - horn.ws) * 0.000285f;
    horn.wf += (horn_rnd() - horn.wf) * 0.00427f;
    horn.ls += (horn_rnd() - horn.ls) * 0.000214f;
    cents = 3.5f * horn.ws * (1.0f / 0.0119f) + 1.2f * horn.wf * (1.0f / 0.0462f);
    lev = 1.0f + 0.25f * horn.ls * (1.0f / 0.0103f);
    f *= 1.0f + cents * 0.000577623f;            /* (cents to a ratio: ln 2 / 1200) */
    horn.ph = wrap(horn.ph + f * INV_SR);
    o = horn_oct(f);
    x = horn.ph * (float)HT_N;
    {
        uint32_t i = (uint32_t)x;
        float fr = x - (float)i;
        const int16_t *a = horn_tab[type][0][o], *b = horn_tab[type][1][o];
        float sa = (float)a[i & (HT_N - 1u)] + fr * (float)(a[(i + 1u) & (HT_N - 1u)] - a[i & (HT_N - 1u)]);
        float sb = (float)b[i & (HT_N - 1u)] + fr * (float)(b[(i + 1u) & (HT_N - 1u)] - b[i & (HT_N - 1u)]);
        s = ((1.0f - hard) * sa + hard * sb) * (1.0f / 27000.0f);
    }
    t = 0.5f + 0.5f * fm_sin_turns(horn.ph + 0.25f >= 1.0f ? horn.ph - 0.75f : horn.ph + 0.25f);
    horn.nz = horn_rnd() * (t * t + 2.0f * horn.tongue);   /* the breath, pulsed as the lips (the reed) open each
                                                  * cycle; and the tongue's burst at the start */
    s += bq_run(&horn.nbp, horn.nz) * horn_rms[type][1][o] * fm_exp2f(HORN_NOISE_DB[type] * 0.16609640f) * 2.5f;
    s = bq_run(&horn.lp, bq_run(&horn.pk, bq_run(&horn.hp, s)));
    return s * lev;
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
        if (lead.inst == INST_REED) {            /* 7.3: the free reed */
            float hard = env * (0.4f + 0.6f * lead.vel), dt, air;   /* (how hard it's blown: the puff's width) */
            uint32_t oc;
            f *= fm_exp2f(vib + 0.003f * (1.0f - env));   /* (sharp as the breath falls: ~2 Hz at A4) */
            dt = f * INV_SR;
            oc = reed_oct(f * (1.0f + lead.spread));
            lead.pc = wrap(lead.pc + dt);
            lead.pc2 = wrap(lead.pc2 + dt * (1.0f + lead.spread));   /* a second reed a few cents up (DETUNE) */
            s = (1.0f - hard) * (reed_read(reed_tab[0][oc], lead.pc) + reed_read(reed_tab[0][oc], lead.pc2))
                + hard * (reed_read(reed_tab[1][oc], lead.pc) + reed_read(reed_tab[1][oc], lead.pc2));
            s *= 0.3f;
            reed_rng ^= reed_rng << 13;
            reed_rng ^= reed_rng >> 17;
            reed_rng ^= reed_rng << 5;
            air = (float)(int32_t)reed_rng * (1.0f / 2147483648.0f);
            reed_air += (air - reed_air) * 0.3f;  /* the breath past the reed, faint */
            s += 0.012f * reed_air * env;
            s = bq_run(&reed_bq[1], bq_run(&reed_bq[0], s));
            s *= REED_GAIN;                      /* (the FM melodica's loudness) */
        } else if (lead.inst == INST_BRASS || lead.inst == INST_HORN || lead.inst == INST_LAYER) {
            /* brass: a scoop up into the note, the index with the loudness (7.7: HORN, or both layered) */
            lead.scoop *= 0.9994f;               /* ~40 ms */
            f *= fm_exp2f(vib - lead.scoop * (1.0f / 12.0f));
            if (lead.inst != INST_HORN) {
                lead.pc = wrap(lead.pc + f * INV_SR);
                idx = lead.bright * env * (0.4f + 0.6f * lead.vel);
                s = fm_sin_turns(lead.pc + idx * 0.15915494f * fm_sin_turns(lead.pc));
            } else
                s = 0.0f;
            if (lead.inst != INST_BRASS) {       /* the horn: blown as hard as the breath and the velocity */
                /* the tongued attack (the owner, 2026-11-10: "not punchy enough"): a reggae horn section stabs. The
                 * note speaks within ~12 ms, swells ~4 dB over its sustain for ~120 ms (the trumpet takes' overshoot,
                 * 2-5.5 dB), brighter while it does (brass brightens as it gets louder), and the tongue's release is
                 * a short burst of breath. The velocity scales all three */
                float bite = fm_expf(-lead.t * (1.0f / 0.12f)) * lead.vel, tongue = fm_expf(-lead.t * (1.0f / 0.008f));
                float hard = fm_minf(1.0f, env * (0.3f + 0.7f * lead.vel) * lead.bright * (1.0f / 1.66f) + 0.55f * bite);
                float h;
                if (lead.amp < 1.0f && lead.note >= 0)
                    lead.amp = fm_minf(1.0f, lead.amp + HORN_ATT);   /* (on top of ATTACK's: it speaks at once) */
                horn.tongue = tongue * lead.vel;
                h = horn_tick(f, hard, (uint32_t)lead.horn) * HORN_GAIN * (1.0f + 0.6f * bite);
                h = bq_run(&horn.blare, h) * HORN_TILT_LEVEL;   /* the brass's blare (the reggae sections, measured:
                                                  * tools/studies/hornstat.py) */
                s = lead.inst == INST_HORN ? h : 1.4f * ((1.0f - lead.blend) * s + lead.blend * h);   /* (the layer's
                                                  * two only partly alike: 1.4 back to one's loudness) */
            }
            {                                    /* the doubling: a second player, 12 ms behind, with the note's own
                                                  * envelope (it carried the sound before it, and so came in at full
                                                  * level 12 ms in: a click, heard once HORN's attack was hard) */
                float d = dbl[dbl_pos];
                dbl[dbl_pos] = s * env;
                dbl_pos = dbl_pos + 1u == DOUBLE_N ? 0u : dbl_pos + 1u;
                s = 0.7f * s * env + 0.45f * d;
                env = 1.0f;                      /* (applied: the delayed half has its own, 12 ms old) */
            }
        } else {                                 /* melodica: a self-fed modulator, two carriers beating */
            float m;
            f *= fm_exp2f(vib);
            lead.pm = wrap(lead.pm + f * INV_SR);
            m = fm_sin_turns(lead.pm + 0.12f * lead.mprev);   /* feedback: a reedy, saw-like spectrum */
            lead.mprev = m;
            idx = lead.bright * (0.5f + 0.5f * lead.vel);
            lead.pc = wrap(lead.pc + f * INV_SR);
            lead.pc2 = wrap(lead.pc2 + f * (1.0f + lead.spread) * INV_SR);   /* +6 cents (DETUNE, 5.3): the reeds beating */
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

#define KICK_808_GAIN 2.3f                       /* (7.6: levels matched to the other models: SINE's peak, CROSS's, METAL's RMS) */
#define RIM_808_GAIN 0.8f
#define HAT_808_GAIN 0.42f
static int d808_mix(int (*m)(float *, uint32_t, float, int), int (*ours)(float *, uint32_t), float gain, float *o,
                    uint32_t n)                  /* (the 808 added into the channel's own buffer: no scratch of its own) */
{
    int a = ours(o, n);
    return m(o, n, gain, a) | a;
}

int voice_render(uint32_t ch, float *o, uint32_t n)
{
    switch (ch) {
    case CH_KICK:                                /* (7.6: an 808 hit rings on if the model changes under it) */
        return d808_mix(d808_kick_render, render_kick, KICK_808_GAIN, o, n);
    case CH_SNARE:
        return d808_mix(d808_snare_render, render_rim, RIM_808_GAIN, o, n);
    case CH_HATS:
        return d808_mix(d808_hat_render, render_hats, HAT_808_GAIN, o, n);
    case CH_BASS:
        return render_bass(o, n);
    case CH_LEAD:
        return render_lead(o, n);
    default:
        return 0;
    }
}
