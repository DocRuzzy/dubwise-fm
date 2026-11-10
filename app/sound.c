/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* The MAIN macros (roadmap 5.3, spec 4.9): four a channel, bank 1 of the SOUND view. A macro is a stored voice field
 * (VM_BASE.., -64..63, 0: every target at its default). Turning it rewrites its raw targets along fixed curves: a
 * straight line from the target's value at -64 to its default at 0, and another on to its value at +63. The engine
 * reads only the raw fields; turning a raw field afterwards leaves the macro where it was (MAIN shows a dot by a
 * macro whose targets have since moved). The curves are bounded on purpose: the kick lands 40-60 Hz, the decays
 * stop short of their ends, the bass's index stays under its cap so the fundamental stays sine-like (sound.txt).
 * A macro's turn is one step of the undo history with its targets (one knob run), and the song's motion captures
 * the macro and its targets alike. Part of the unity build: dub.c includes it. */

typedef struct {
    uint8_t f;                                   /* a raw field (0xFF: none) */
    int8_t lo, hi;                               /* its value at -64 and at +63 (the default at 0) */
} mtarget_t;
#define MT 3u                                    /* targets a macro, at most */
#define MT_NONE {0xFFu, 0, 0}

static const mtarget_t MACRO_T[NCH - 1u][VM_N][MT] = {
    {   /* kick: PITCH · PUNCH · DECAY · DRIVE */
        {{VK_TUNE, 10, 85}, {VK_SWEEP, 20, 70}, MT_NONE},          /* the landing 42 .. 60 Hz, the start with it */
        {{VK_SWEEP, 15, 100}, {VK_PTIME, 70, 10}, {VK_CLICK, 10, 100}},
        {{VK_DECAY, 5, 110}, MT_NONE, MT_NONE},                    /* (short of the 1 s end) */
        {{VK_DRIVE, 0, 110}, {VK_CTONE, -30, 30}, MT_NONE},
    },
    {   /* rim: TUNE · SNAP · DECAY · BODY */
        {{VR_TUNE, 0, 110}, MT_NONE, MT_NONE},
        {{VR_NOISE, 10, 110}, {VR_INDEX, 30, 110}, MT_NONE},
        {{VR_DECAY, 5, 100}, MT_NONE, MT_NONE},
        {{VR_HPF, 40, -50}, {VR_DROP, 0, 70}, MT_NONE},            /* more body: the high-pass down, a drop */
    },
    {   /* hats: TONE · CLOSED · OPEN · HPF */
        {{VH_TONE, 10, 110}, {VH_NOISE, 80, 30}, MT_NONE},
        {{VH_CLOSED, 0, 110}, MT_NONE, MT_NONE},
        {{VH_OPEN, 0, 110}, MT_NONE, MT_NONE},
        {{VH_HPF, -60, 60}, MT_NONE, MT_NONE},
    },
    {   /* bass: TONE · PLUCK · LENGTH · WEIGHT */
        {{VB_CUTOFF, 5, 100}, {VB_INDEX, 20, 90}, MT_NONE},        /* (the index under its cap) */
        {{VB_IDECAY, 5, 80}, {VB_INDEX, 30, 80}, MT_NONE},
        {{VB_DECAY, 0, 100}, {VB_SUSTAIN, 0, 90}, MT_NONE},
        {{VB_SUB, 0, 100}, {VB_DRIVE, 0, 70}, MT_NONE},
    },
    {   /* skank: TONE · LENGTH · COLOUR · ATTACK */
        {{VS_TONE, 10, 110}, MT_NONE, MT_NONE},
        {{VS_DECAY, 0, 110}, MT_NONE, MT_NONE},
        {{VS_INDEX, 20, 120}, MT_NONE, MT_NONE},
        {{VS_ATTACK, 0, 100}, MT_NONE, MT_NONE},
    },
    {   /* organ: DRAWBAR · LENGTH · CLICK · DETUNE */
        {{VO_DRAWBAR, 0, 127}, MT_NONE, MT_NONE},
        {{VO_DECAY, 0, 120}, MT_NONE, MT_NONE},
        {{VO_CLICK, 0, 127}, MT_NONE, MT_NONE},
        {{VO_DETUNE, 0, 100}, MT_NONE, MT_NONE},
    },
    {   /* lead: BRIGHT · ATTACK · RELEASE · VIBRATO */
        {{VL_BRIGHT, 10, 120}, MT_NONE, MT_NONE},
        {{VL_ATTACK, 0, 110}, MT_NONE, MT_NONE},
        {{VL_RELEASE, 0, 100}, MT_NONE, MT_NONE},                  /* (short of the 600 ms end) */
        {{VL_VIBRATO, 0, 110}, MT_NONE, MT_NONE},
    },
};

static int macro_busy;                           /* macro_apply writing the targets (MIDI learn keeps the macro) */

static int has_main(int32_t ch) { return ch >= 0 && ch < (int32_t)NCH && ch != CH_VOX; }

/* the B-3's MAIN (5.15): REGISTRATION (a list of common ones, soft to bright: 0 the default, full upper), PERC,
 * ROTARY, DRIVE, in the place of the bubble's macros while the organ's MODEL is B-3 */
static const char B3_REGS[8][10] = {"008000000", "808000000", "868000000", "888000000", "888600000", "888800000",
                                    "888888000", "888888888"};
static int b3_main(int32_t ch) { return ch == CH_ORGAN && ctl_get(P_VOICE(CH_ORGAN, VO_MODEL)) == ORG_B3; }
static int32_t b3_target(uint32_t k, int32_t m, uint32_t bar)   /* the B-3 macro k's target value at macro m */
{
    static const int8_t REG[9] = ORG_REG;
    if (k == 0u) {                               /* REGISTRATION: the bar's offset in the registration m picks */
        int32_t r = m >= 0 ? 3 + m * 4 / 63 : 3 + m * 3 / 64;
        return (int32_t)(B3_REGS[r][bar] - '0') - REG[bar];
    }
    if (k == 1u)
        return m <= 0 ? PERC_OFF : m < 32 ? PERC_2ND : PERC_3RD;
    if (k == 2u)
        return m <= -22 ? ROT_OFF : m < 22 ? ROT_SLOW : ROT_FAST;
    return m <= 0 ? 0 : m * 2;
}

static int macro_of(uint32_t id, int32_t *ch, uint32_t *k)   /* id a macro: its channel and number */
{
    uint32_t f;
    if (id < P_VOICE_BASE || id >= P_VOICE_BASE + NCH * VOICE_STRIDE)
        return 0;
    *ch = (int32_t)((id - P_VOICE_BASE) / VOICE_STRIDE);
    f = (id - P_VOICE_BASE) % VOICE_STRIDE;
    if (f < VM_BASE || !has_main(*ch))
        return 0;
    *k = f - VM_BASE;
    return 1;
}

static int32_t macro_curve(uint32_t ch, const mtarget_t *t, int32_t m)   /* the target's value at macro m */
{
    int32_t def = param_def(P_VOICE(ch, t->f));
    if (m < 0)
        return def + ((int32_t)t->lo - def) * -m / 64;
    return def + ((int32_t)t->hi - def) * m / 63;
}

static void macro_apply(int32_t ch, uint32_t k)  /* the macro's value into its targets (a turn, MIDI learn) */
{
    uint32_t j;
    int32_t m = ctl_get(P_VOICE(ch, VM_BASE + k));
    if (b3_main(ch)) {                           /* the B-3's own (5.15) */
        static const uint8_t F[4] = {VO_DB0, VO_PERC, VO_ROTARY, VO_ODRIVE};
        macro_busy = 1;
        if (k == 0u)
            for (j = 0; j < 9u; j++)
                ctl_set(P_VOICE(ch, VO_DB0 + j), b3_target(0, m, j));
        else
            ctl_set(P_VOICE(ch, F[k]), b3_target(k, m, 0));
        macro_busy = 0;
        return;
    }
    macro_busy = 1;
    for (j = 0; j < MT; j++) {
        const mtarget_t *t = &MACRO_T[ch][k][j];
        if (t->f != 0xFFu)
            ctl_set(P_VOICE(ch, t->f), macro_curve((uint32_t)ch, t, m));
    }
    macro_busy = 0;
}

static int macro_moved(int32_t ch, uint32_t k)   /* 1: a target turned since (MAIN's dot) */
{
    uint32_t j;
    int32_t m = ctl_get(P_VOICE(ch, VM_BASE + k));
    if (b3_main(ch)) {                           /* (the B-3's) */
        static const uint8_t F[4] = {VO_DB0, VO_PERC, VO_ROTARY, VO_ODRIVE};
        if (k == 0u) {
            for (j = 0; j < 9u; j++)
                if (ctl_get(P_VOICE(ch, VO_DB0 + j)) != b3_target(0, m, j))
                    return 1;
            return 0;
        }
        return ctl_get(P_VOICE(ch, F[k])) != b3_target(k, m, 0);
    }
    for (j = 0; j < MT; j++) {
        const mtarget_t *t = &MACRO_T[ch][k][j];
        if (t->f != 0xFFu && ctl_get(P_VOICE(ch, t->f)) != macro_curve((uint32_t)ch, t, m))
            return 1;
    }
    return 0;
}

static void organ_tick(void)                     /* every frame: the B-3's registration into its tables when it moved
                                                  * (knobs, patches, Dice, the web editor, motion: the mirror's) */
{
    static const int8_t REG[9] = ORG_REG;
    static int8_t last[9] = {-1, -1, -1, -1, -1, -1, -1, -1, -1};
    int8_t b[9];
    uint32_t k;
    int moved = 0;
    for (k = 0; k < 9u; k++) {
        int32_t v = REG[k] + ctl_get(P_VOICE(CH_ORGAN, VO_DB0 + k));
        b[k] = (int8_t)(v < 0 ? 0 : v > 8 ? 8 : v);
        if (k == 8u && ctl_get(P_VOICE(CH_ORGAN, VO_PERC)))
            b[k] = 0;                            /* (percussion takes the 1' drawbar's place, as on the organ: v0.3) */
        moved |= b[k] != last[k];
    }
    if (!moved)
        return;
    for (k = 0; k < 9u; k++)
        last[k] = b[k];
    chordv_b3_build(b);
}

static int organ_bar(uint32_t id, int32_t v, char *t)   /* a drawbar's field shown as the bar (0..8), not its offset */
{
    static const int8_t REG[9] = ORG_REG;
    if (id < P_VOICE(CH_ORGAN, VO_DB0) || id > P_VOICE(CH_ORGAN, VO_DB8))
        return 0;
    int_text(REG[id - P_VOICE(CH_ORGAN, VO_DB0)] + v, t);
    return 1;
}

static const char *model_name(uint32_t id, int32_t v)   /* a MODEL field's value as its name (5.7), 0: not one */
{
    static const char *const K[KICK_MODELS] = {"SINE", "THUMP", "BOOM", "808"}, *const R[RIM_MODELS] = {"CROSS", "SNARE",
                                                                                                    "SYNDRUM", "808"};
    static const char *const H[HAT_MODELS] = {"METAL", "NOISE", "808"}, *const B[BASS_MODELS] = {"ROUND", "PLUCK", "HOLLOW",
                                                                                          "STEPPERS"};
    if (v < 0)
        return 0;
    if (id == P_VOICE(CH_KICK, VK_MODEL) && v < KICK_MODELS)
        return K[v];
    if (id == P_VOICE(CH_SNARE, VR_MODEL) && v < RIM_MODELS)
        return R[v];
    if (id == P_VOICE(CH_HATS, VH_MODEL) && v < HAT_MODELS)
        return H[v];
    if (id == P_VOICE(CH_BASS, VB_MODEL) && v < BASS_MODELS)
        return B[v];
    if (id == P_VOICE(CH_ORGAN, VO_MODEL) && v < ORG_MODELS)
        return v ? "B-3" : "BUBBLE";
    if (id == P_VOICE(CH_ORGAN, VO_PERC) && v < PERC_N) {
        static const char *const P[PERC_N] = {"OFF", "2ND", "3RD"};
        return P[v];
    }
    if (id == P_VOICE(CH_ORGAN, VO_VIBCH) && v < VIB_N) {
        static const char *const V[VIB_N] = {"OFF", "V1", "V2", "V3", "C1", "C2", "C3"};
        return V[v];
    }
    if (id == P_VOICE(CH_ORGAN, VO_ROTARY) && v < ROT_N) {
        static const char *const R[ROT_N] = {"OFF", "SLOW", "FAST", "BRAKE"};
        return R[v];
    }
    if (id == P_VOICE(CH_ORGAN, VO_PFAST))
        return v ? "FAST" : "SLOW";
    if (id == P_VOICE(CH_VOX, VX_SAY))           /* (6.6: the shout's name; OFF: black 8 is BIM) */
        return v ? lpc_name(v - 1) : "OFF";
    if (id == P_VOICE(CH_LEAD, VL_INST) && v < INST_N) {   /* (7.3: REED) */
        static const char *const I[INST_N] = {"MELODICA", "BRASS", "REED", "HORN", "FM+HORN"};
        return I[v];
    }
    if (id == P_VOICE(CH_SKANK, VS_MODEL))       /* (7.8) */
        return v ? "PLUCK" : "FM";
    if (id == P_VOICE(CH_LEAD, VL_HORN) && v < HORN_N)   /* (7.7) */
        return v ? "SAX" : "TRUMPET";
    if (id == P_VOICE(CH_VOX, VX_SHAPE))         /* (7.1) */
        return v ? "NJD" : "CLASSIC";
    if (id == P_VOICE(CH_BASS, VB_SUBTONE))      /* (6.1) */
        return v ? "CLASSIC" : "SINE";
    if (id == P_VOICE(CH_ORGAN, VO_LESLIE))      /* (v0.3: FULL is setBfree's, Studio only) */
        return v ? "FULL" : "LIGHT";
    if (id == P_VOICE(CH_ORGAN, VO_ODRIVE) && v == 0)
        return "OFF";
    return 0;
}

static void label_of(uint32_t id, char *l, uint32_t n)   /* a voice field's name as the screen shows it ("m." off) */
{
    const char *s = param_desc(id)->name;
    if (id >= P_VOICE(CH_ORGAN, VM_BASE) && id < P_VOICE(CH_ORGAN, VM_BASE + VM_N) && b3_main(CH_ORGAN)) {
        static const char *const B[VM_N] = {"REGIST", "PERC", "ROTARY", "DRIVE"};   /* (the B-3's MAIN: 5.15) */
        upper(B[id - P_VOICE(CH_ORGAN, VM_BASE)], l, n);
        return;
    }
    if (s[0] == 'm' && s[1] == '.')
        s += 2;
    upper(s, l, n);
}

#ifdef PLAT_HOST
static int sound_expect(const char *what, int *got)
{
    if (!strncmp(what, "moved", 5) && what[5] >= '1' && what[5] <= '7' && what[6] == '_')
        *got = macro_moved(what[5] - '1', (uint32_t)(atoi(what + 7) - 1) % VM_N);   /* movedC_K */
    else if (!strcmp(what, "sound_bank"))
        *got = dub.sound;
    else
        return 0;
    return 1;
}
#endif
