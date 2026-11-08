/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* The parameters' ranges, defaults and names (params.h). Part of the unity build: app.c includes it. */
#include "params.h"
#include "fastmath.h"

typedef char p_ch_fits[(CHF_N <= CH_STRIDE && P_CH_BASE + NCH * CH_STRIDE <= P_BUS_BASE) ? 1 : -1];
typedef char p_globals_fit[(P_NGLOBAL <= P_CHORD0 && P_CHORD_END <= P_DEV_LOAD) ? 1 : -1];
typedef char p_voice_fits[(VB_N <= VOICE_STRIDE && P_VOICE_BASE + NCH * VOICE_STRIDE <= P_NIDS) ? 1 : -1];

static const pdesc_t CH_FIELDS[CHF_N] = {
    [CHF_LEVEL] = {"level", 0, 127, 100},           /* 100 = unity (gain (v/100)^2: +4 dB at 127) */
    [CHF_MUTE] = {"mute", 0, 1, 0},
    [CHF_SOLO] = {"solo", 0, 1, 0},
    [CHF_ECHO_SEND] = {"echo", 0, 127, 0},          /* post-fader, post-mute (throws are pre-fader: P0.2) */
    [CHF_SPRING_SEND] = {"spring", 0, 127, 0},
    [CHF_THROW] = {"throw", 0, 1, 0},               /* pre-fader, pre-mute: a muted channel can be thrown (P0.2) */
    [CHF_THROW_DEST] = {"dest", 0, DEST_N - 1, DEST_ECHO},   /* per channel: param_def */
    [CHF_LEN] = {"len", 1, 64, 16},                 /* the pattern's length, sixteenth steps (P0.4) */
};
/* the voices' parameters, 0..127 unless said (voices.c maps them) */
static const pdesc_t KICK_F[VK_N] = {
    [VK_TUNE] = {"tune", 0, 127, 42},               /* 40..70 Hz: where the sweep lands */
    [VK_SWEEP] = {"sweep", 0, 127, 40},             /* 80..300 Hz: where it starts */
    [VK_PTIME] = {"ptime", 0, 127, 30},             /* 10..120 ms */
    [VK_DECAY] = {"decay", 0, 127, 40},             /* 100..1000 ms */
    [VK_CLICK] = {"click", 0, 127, 40},
};
static const pdesc_t RIM_F[VR_N] = {
    [VR_TUNE] = {"tune", 0, 127, 37},               /* 300..1200 Hz */
    [VR_DECAY] = {"decay", 0, 127, 25},             /* 20..200 ms */
    [VR_NOISE] = {"noise", 0, 127, 50},
    [VR_INDEX] = {"index", 0, 127, 70},
};
static const pdesc_t HAT_F[VH_N] = {
    [VH_TONE] = {"tone", 0, 127, 55},               /* 300..800 Hz, the squares' base */
    [VH_CLOSED] = {"closed", 0, 127, 30},           /* 20..150 ms */
    [VH_OPEN] = {"open", 0, 127, 50},               /* 150..800 ms */
    [VH_NOISE] = {"noise", 0, 127, 50},
};
static const pdesc_t BASS_F[VB_N] = {
    [VB_INDEX] = {"index", 0, 127, 50},             /* 0..2.5 */
    [VB_IDECAY] = {"idecay", 0, 127, 30},           /* 10..150 ms */
    [VB_DECAY] = {"decay", 0, 127, 20},             /* 80..1500 ms */
    [VB_CUTOFF] = {"cutoff", 0, 127, 35},           /* 300..2500 Hz */
    [VB_RATIO] = {"ratio", 0, 1, 0},                /* 1:1, 2:1 */
    [VB_SUB] = {"sub", 0, 127, 0},
    [VB_SUSTAIN] = {"sustain", 0, 127, 25},
};
static const pdesc_t SKANK_F[VS_N] = {
    [VS_INDEX] = {"index", 0, 127, 70},             /* the attack's brightness */
    [VS_DECAY] = {"decay", 0, 127, 40},             /* 60 .. 200 ms */
    [VS_TONE] = {"tone", 0, 127, 60},               /* the band-pass's centre */
    [VS_RATIO] = {"ratio", 0, 1, 1},                /* 1:1, 1:3 (nasal) */
};
static const pdesc_t ORGAN_F[VO_N] = {
    [VO_DRAWBAR] = {"drawbar", 0, 127, 60},         /* how much of the upper partials */
    [VO_DECAY] = {"decay", 0, 127, 30},             /* the gate: 40 .. 300 ms */
    [VO_CLICK] = {"click", 0, 127, 50},             /* the key click */
};
static const pdesc_t VOX_F[VX_N] = {
    [VX_PITCH] = {"pitch", 0, 127, 70},             /* the sirens: 200 Hz .. 1.2 kHz */
    [VX_RATE] = {"rate", 0, 127, 50},               /* 0.5 .. 12 Hz */
    [VX_DEPTH] = {"depth", 0, 127, 36},             /* 1 .. 24 semitones */
    [VX_WAVE] = {"wave", 0, 1, 0},                  /* sine, triangle */
};
static const pdesc_t LEAD_F[VL_N] = {
    [VL_INST] = {"inst", 0, INST_N - 1, INST_MELODICA},
    [VL_BRIGHT] = {"bright", 0, 127, 64},           /* the FM index, with velocity */
    [VL_ATTACK] = {"attack", 0, 127, 30},           /* 10 .. 150 ms */
    [VL_RELEASE] = {"release", 0, 127, 40},         /* 30 .. 600 ms */
    [VL_VIBRATO] = {"vibrato", 0, 127, 40},         /* 0 .. 40 cents at 4.5 Hz, after 300 ms */
};
static const pdesc_t *const VOICE_T[NCH] = {KICK_F, RIM_F, HAT_F, BASS_F, SKANK_F, ORGAN_F, LEAD_F, VOX_F};
static const uint8_t VOICE_N[NCH] = {VK_N, VR_N, VH_N, VB_N, VS_N, VO_N, VL_N, VX_N};
static const pdesc_t GLOBALS[P_NGLOBAL] = {
    [P_TEMPO] = {"tempo", 400, 1800, 750},          /* 40.0 .. 180.0 BPM */
    [P_PLAY] = {"play", 0, 1, 0},
    [P_QUANT] = {"quant", 0, QUANT_N - 1, QUANT_1},
    [P_LEAD] = {"lead", 0, 50, 10},                 /* ms (spec: launch quantise "fired slightly early") */
    [P_CYCLE] = {"cycle", 0, CYCLE_N - 1, CYCLE_16},
    [P_RUNWAY] = {"runway", 0, RUNWAY_N - 1, RUNWAY_4},
    [P_RUNWAY_KEYS] = {"runway.keys", 0, 1, 0},
    [P_RUNWAY_NUM] = {"runway.num", 0, 1, 1},
    [P_TICK] = {"tick", 0, 1, 0},
    [P_REDUCED] = {"reduced", 0, 1, 0},
    [P_KEY_ROOT] = {"key", 0, 11, 9},               /* A */
    [P_SCALE] = {"scale", 0, SCALE_N - 1, SCALE_AEOLIAN},
    [P_LOCK] = {"lock", 0, LOCK_N - 1, LOCK_OFF},
    [P_KB_OCT] = {"kb.oct", 0, 5, 1},               /* -2: the bass's register */
    [P_LOCK_MIDI] = {"lock.midi", 0, 1, 0},
    [P_CHORD_LEN] = {"chord.len", 1, 16, 4},
    [P_SONG_MODE] = {"song", 0, 2, 0},
    [P_SONG_LEN] = {"song.len", 1, 192, 16},
    [P_SONG_REC] = {"song.rec", 0, 1, 0},
    [P_TIER_LOCK] = {"tier.lock", 0, TIER_N, 0},
};
static const pdesc_t CHORD_SLOT = {"chord", 0, 95, 9 * 8 + CHORD_MIN};   /* per slot: param_def */
static const pdesc_t ECHO[P_ECHO_END - P_ECHO_TIME] = {
    [P_ECHO_TIME - P_ECHO_TIME] = {"echo.time", 0, ECHO_DIV_N - 1, ECHO_DIV_8D},
    [P_ECHO_FB - P_ECHO_TIME] = {"echo.fb", 0, 100, 45},
    [P_ECHO_RET - P_ECHO_TIME] = {"echo.ret", 0, 127, 100},
    [P_ECHO_TONE - P_ECHO_TIME] = {"echo.tone", 0, 127, 70},
    [P_ECHO_HEAD2 - P_ECHO_TIME] = {"echo.head2", 0, 127, 0},
    [P_ECHO_FREEZE - P_ECHO_TIME] = {"echo.freeze", 0, 1, 0},
    [P_ECHO_PUSH - P_ECHO_TIME] = {"echo.push", 0, 1, 0},
    [P_ECHO_DIVE - P_ECHO_TIME] = {"echo.dive", 0, 1, 0},
    [P_DIVE_DEPTH - P_ECHO_TIME] = {"dive.depth", 0, 127, 64},
    [P_DIVE_TIME - P_ECHO_TIME] = {"dive.time", 0, 127, 40},
    [P_ECHO_WIDTH - P_ECHO_TIME] = {"echo.width", 0, 127, 0},
};
static const pdesc_t SPRING[P_SPRING_END - P_SPRING_RET] = {
    {"spring.ret", 0, 127, 90},
    {"spring.decay", 0, 127, 50},
    {"spring.tone", 0, 127, 60},
    {"spring.kick", 0, 1, 0},
};
static const pdesc_t PH[P_PH_END - P_PH_ON] = {
    {"ph.on", 0, PH_N - 1, PH_OFF},
    {"ph.rate", 0, 127, 30},
    {"ph.depth", 0, 127, 90},
    {"ph.fb", 0, 127, 50},
    {"ph.shape", 0, 1, 0},
};
static const pdesc_t XS[P_XS_END - P_XS_E2S] = {
    {"xs.e2s", 0, 127, 0},
    {"xs.s2e", 0, 127, 0},
};
static const pdesc_t HPF[P_HPF_END - P_HPF_STEP] = {
    {"hpf", 0, HPF_STEPS, 0},
    {"hpf.on", 0, HPF_ON_N - 1, HPF_ON_MIX},
};
static const pdesc_t DEV_TONES = {"dev.tones", 0, 1, 0};
static const pdesc_t DEV_LOAD = {"dev.load", 0, 100, 0};
const char *tier_name(int32_t t)
{
    static const char *const N[TIER_N] = {"STUDIO", "FOUR-TRACK", "SEVEN-INCH"};
    return t >= 0 && t < TIER_N ? N[t] : "";
}

float echo_div_beats(int32_t d)
{
    static const float B[ECHO_DIV_N] = {0.25f, 1.0f / 3.0f, 0.375f, 0.5f, 2.0f / 3.0f, 0.75f, 1.0f, 4.0f / 3.0f,
                                        1.5f, 2.0f};
    return d >= 0 && d < ECHO_DIV_N ? B[d] : 0.75f;
}
const char *echo_div_name(int32_t d)
{
    static const char *const N[ECHO_DIV_N] = {"1/16", "1/8T", "1/16.", "1/8", "1/4T", "1/8.", "1/4", "1/2T",
                                              "1/4.", "1/2"};
    return d >= 0 && d < ECHO_DIV_N ? N[d] : "";
}
float echo_t60(int32_t fb) { return 0.1f * fm_exp2f((float)fb * 0.1f); }

/* the Altec 9069B's steps, as the replicas and emulations list them (mastery guide B1, C3) */
static const float HPF_HZ[HPF_STEPS + 1] = {0.0f, 70.0f, 100.0f, 150.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 3000.0f,
                                            5000.0f, 7500.0f};
float hpf_hz(int32_t step) { return step >= 0 && step <= HPF_STEPS ? HPF_HZ[step] : 0.0f; }
const char *hpf_name(int32_t step)
{
    static const char *const N[HPF_STEPS + 1] = {"OFF", "70 HZ", "100 HZ", "150 HZ", "250 HZ", "500 HZ", "1K", "2K",
                                                 "3K", "5K", "7.5K"};
    return step >= 0 && step <= HPF_STEPS ? N[step] : "";
}

uint32_t quant_beats(int32_t q)
{
    static const uint8_t B[QUANT_N - 1] = {1, 4, 8, 16, 32, 64, 128};
    return q >= 0 && q < QUANT_N - 1 ? B[q] : 4u;
}
const char *quant_name(int32_t q)
{
    static const char *const N[QUANT_N] = {"BEAT", "1 BAR", "2 BARS", "4 BARS", "8 BARS", "16 BARS", "32 BARS",
                                           "CYCLE"};
    return q >= 0 && q < QUANT_N ? N[q] : "";
}
uint32_t cycle_bars(int32_t c) { return 8u << (c >= 0 && c < CYCLE_N ? c : CYCLE_16); }
uint32_t runway_bars(int32_t r) { return 1u << (r >= 0 && r < RUNWAY_N ? r : RUNWAY_4); }

const pdesc_t *param_desc(uint32_t id)
{
    if (id < P_NGLOBAL)
        return &GLOBALS[id];
    if (id >= P_CHORD0 && id < P_CHORD_END)
        return &CHORD_SLOT;
    if (id == P_DEV_TONES)
        return &DEV_TONES;
    if (id == P_DEV_LOAD)
        return &DEV_LOAD;
    if (id >= P_ECHO_TIME && id < P_ECHO_END)
        return &ECHO[id - P_ECHO_TIME];
    if (id >= P_SPRING_RET && id < P_SPRING_END)
        return &SPRING[id - P_SPRING_RET];
    if (id >= P_HPF_STEP && id < P_HPF_END)
        return &HPF[id - P_HPF_STEP];
    if (id >= P_XS_E2S && id < P_XS_END)
        return &XS[id - P_XS_E2S];
    if (id >= P_PH_ON && id < P_PH_END)
        return &PH[id - P_PH_ON];
    if (id >= P_CH_BASE && id < P_CH_BASE + NCH * CH_STRIDE && (id - P_CH_BASE) % CH_STRIDE < CHF_N)
        return &CH_FIELDS[(id - P_CH_BASE) % CH_STRIDE];
    if (id >= P_VOICE_BASE && id < P_VOICE_BASE + NCH * VOICE_STRIDE) {
        uint32_t ch = (id - P_VOICE_BASE) / VOICE_STRIDE, f = (id - P_VOICE_BASE) % VOICE_STRIDE;
        return f < VOICE_N[ch] ? &VOICE_T[ch][f] : 0;
    }
    return 0;
}

int32_t param_def(uint32_t id)
{
    static const uint8_t DEST[NCH] = {DEST_ECHO, DEST_BOTH, DEST_ECHO, DEST_ECHO, DEST_SPRING, DEST_SPRING,
                                      DEST_ECHO, DEST_ECHO};   /* spec 3's throw destinations */
    const pdesc_t *d = param_desc(id);
    if (!d)
        return 0;
    if (id >= P_CH_BASE && id < P_BUS_BASE && (id - P_CH_BASE) % CH_STRIDE == CHF_THROW_DEST)
        return DEST[(id - P_CH_BASE) / CH_STRIDE];
    if (id == P_CH(CH_SKANK, CHF_SPRING_SEND))   /* the skank to the spring by default (D3) */
        return 40;
    if (id == P_CH(CH_VOX, CHF_ECHO_SEND))       /* the sirens into the echo by default (D3) */
        return 80;
    if (id == P_CH(CH_LEAD, CHF_ECHO_SEND))      /* the melodica, heavy echo (D2) */
        return 60;
    if (id >= P_CHORD0 && id < P_CHORD_END) {     /* the lane: Am Dm Am Em, then Am */
        static const uint8_t L[4] = {9 * 8 + CHORD_MIN, 2 * 8 + CHORD_MIN, 9 * 8 + CHORD_MIN, 4 * 8 + CHORD_MIN};
        return (id - P_CHORD0) < 4u ? L[id - P_CHORD0] : 9 * 8 + CHORD_MIN;
    }
    return d->def;
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
    if (id >= P_CHORD0 && id < P_CHORD_END) {     /* "chord1" .. "chord16" */
        uint32_t k = id - P_CHORD0 + 1u;
        const char *c = "chord";
        for (; *c && i + 1 < n; c++)
            buf[i++] = *c;
        if (k >= 10u && i + 1 < n)
            buf[i++] = '1';
        if (i + 1 < n)
            buf[i++] = (char)('0' + k % 10u);
        buf[i] = 0;
        return;
    }
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

/* ------------------------------------------------------------------ keys --- */
static const uint16_t SCALE_BITS[SCALE_N] = {   /* intervals from the root, a bit each */
    0x5AD,   /* aeolian 0 2 3 5 7 8 10 */
    0x6AD,   /* dorian 0 2 3 5 7 9 10 */
    0x4A9,   /* minor pentatonic 0 3 5 7 10 */
    0x4E9,   /* minor blues 0 3 5 6 7 10 */
    0xAB5,   /* ionian 0 2 4 5 7 9 11 */
    0x6B5,   /* mixolydian 0 2 4 5 7 9 10 */
    0x295,   /* major pentatonic 0 2 4 7 9 */
    0x9AD,   /* harmonic minor 0 2 3 5 7 8 11 */
    0x5AB,   /* phrygian 0 1 3 5 7 8 10 */
};
static const uint16_t CHORD_BITS[CHORD_N] = {0x089, 0x091, 0x491, 0x489, 0x891, 0x0A1, 0x049};

const char *scale_name(int32_t s)
{
    static const char *const N[SCALE_N] = {"NATURAL MINOR", "DORIAN", "MINOR PENTATONIC", "MINOR BLUES", "MAJOR",
                                           "MIXOLYDIAN", "MAJOR PENTATONIC", "HARMONIC MINOR", "PHRYGIAN"};
    return s >= 0 && s < SCALE_N ? N[s] : "";
}
const char *chord_name(int32_t q)
{
    static const char *const N[CHORD_N] = {"MINOR", "MAJOR", "SEVENTH", "MINOR SEVENTH", "MAJOR SEVENTH", "SUS FOUR",
                                           "DIMINISHED"};
    return q >= 0 && q < CHORD_N ? N[q] : "";
}
const char *chord_short(int32_t q)
{
    static const char *const N[CHORD_N] = {"m", "", "7", "m7", "M7", "s4", "o"};
    return q >= 0 && q < CHORD_N ? N[q] : "";
}
const char *pitch_name(int32_t pc)
{
    static const char *const N[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    return N[((pc % 12) + 12) % 12];
}
static uint32_t rot(uint32_t bits, int32_t root)    /* intervals -> pitch classes */
{
    uint32_t r = (uint32_t)(((root % 12) + 12) % 12);
    return ((bits << r) | (bits >> (12u - r))) & 0xFFFu;
}
uint32_t scale_mask(int32_t root, int32_t s) { return rot(SCALE_BITS[s >= 0 && s < SCALE_N ? s : 0], root); }
uint32_t chord_mask(int32_t code) { return rot(CHORD_BITS[(code & 7) < CHORD_N ? code & 7 : 0], code / 8); }

uint32_t lock_mask(const int16_t *vals, uint32_t bar)
{
    int32_t mode = vals[P_LOCK];
    if (mode == LOCK_SCALE)
        return scale_mask(vals[P_KEY_ROOT], vals[P_SCALE]);
    if (mode == LOCK_CHORD) {
        uint32_t len = (uint32_t)(vals[P_CHORD_LEN] > 0 ? vals[P_CHORD_LEN] : 1);
        return chord_mask(vals[P_CHORD0 + bar % len]);
    }
    return 0xFFFu;
}

void chord_notes(int32_t code, int32_t voicing, int32_t base, int32_t *notes3)
{
    uint32_t m = chord_mask(code), k = 0, i;
    int32_t root = code / 8, start = base + 12 * (voicing - VOICING_MID);
    int32_t r = start + ((root - start) % 12 + 12) % 12;   /* the root at or above the voicing's base */
    for (i = 0; i < 12u && k < 3u; i++) {       /* the chord's first three tones from the root up */
        int32_t pc = (root + (int32_t)i) % 12;
        if (m >> pc & 1u)
            notes3[k++] = r + (int32_t)i;
    }
    while (k < 3u) {
        notes3[k] = notes3[k - 1] + 12;
        k++;
    }
}
