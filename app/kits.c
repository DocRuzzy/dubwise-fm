/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* Factory kits (roadmap 5.5, spec 4.9): eight voices and a suggested dub-bus setting, after the rooms in the mastery
 * guide (B1-B4: the studios and their gear; D1-D3: the drums, the instruments, the FM notes). Homage, never "the"
 * sound (review-2026-11 section 7): a starting point in the spirit of a room, by ear from here on.
 * SETUP -> LOAD KIT: KNOB 2 picks the kit, KNOB 3 VOICES or VOICES + BUS, SAVE loads it (asked twice over unsaved
 * changes); one undo step. Each voice is set whole (every field to its default, then the kit's), so a kit sounds the
 * same whatever came before it; the bus only sets what it lists. A kit's voices are factory patches too (5.4): the
 * SOUND view's PRESET browses them by the kit's short name. Part of the unity build: dub.c includes it before
 * setup.c (its row) and patches.c (kit_load). */

typedef struct {
    uint16_t id;
    int16_t v;
} kset_t;
typedef struct {
    const char *name;                            /* SETUP's */
    char patch[9];                               /* as a factory patch (8 characters) */
    kset_t s[48];                                /* the voices' fields, then the bus's ({0, 0} ends it) */
} kit_t;
#define K(ch, f, v) {P_VOICE(ch, f), v}
static const kit_t KITS[] = {
    {"TUBBY'S", "TUBBYS", {                      /* B1: the stepped filter and the spring; a round, deep low end */
        K(CH_KICK, VK_TUNE, 34), K(CH_KICK, VK_DECAY, 55), K(CH_KICK, VK_CLICK, 25),
        K(CH_SNARE, VR_TUNE, 55), K(CH_SNARE, VR_DECAY, 18), K(CH_SNARE, VR_INDEX, 60), K(CH_SNARE, VR_HPF, 10),
        K(CH_HATS, VH_TONE, 60), K(CH_HATS, VH_CLOSED, 25), K(CH_HATS, VH_NOISE, 60),
        K(CH_BASS, VB_INDEX, 35), K(CH_BASS, VB_IDECAY, 20), K(CH_BASS, VB_DECAY, 35), K(CH_BASS, VB_CUTOFF, 28),
        K(CH_BASS, VB_SUB, 50), K(CH_BASS, VB_SUSTAIN, 35),
        K(CH_SKANK, VS_DECAY, 25), K(CH_SKANK, VS_TONE, 55), K(CH_SKANK, VS_INDEX, 60),
        K(CH_ORGAN, VO_DRAWBAR, 45), K(CH_ORGAN, VO_DECAY, 25),
        K(CH_LEAD, VL_VIBRATO, 50),
        {P_ECHO_FB, 55}, {P_ECHO_TONE, 55}, {P_SPRING_DECAY, 75}, {P_SPRING_TONE, 45}, {P_PH_ON, PH_OFF},
        {P_DRIVE_ON, DRIVE_OFF}, {0, 0}}},
    {"CHANNEL ONE", "CH.ONE", {                  /* B3, D1: rockers; a punchy kick, Sly's high, hard rim */
        K(CH_KICK, VK_TUNE, 40), K(CH_KICK, VK_SWEEP, 60), K(CH_KICK, VK_PTIME, 22), K(CH_KICK, VK_DECAY, 35),
        K(CH_KICK, VK_CLICK, 65),
        K(CH_SNARE, VR_TUNE, 80), K(CH_SNARE, VR_DECAY, 16), K(CH_SNARE, VR_NOISE, 40), K(CH_SNARE, VR_INDEX, 95),
        K(CH_HATS, VH_TONE, 70), K(CH_HATS, VH_CLOSED, 18), K(CH_HATS, VH_OPEN, 40), K(CH_HATS, VH_NOISE, 45),
        K(CH_BASS, VB_INDEX, 65), K(CH_BASS, VB_IDECAY, 22), K(CH_BASS, VB_DECAY, 28), K(CH_BASS, VB_CUTOFF, 45),
        K(CH_BASS, VB_SUB, 20),
        K(CH_SKANK, VS_INDEX, 90), K(CH_SKANK, VS_DECAY, 30), K(CH_SKANK, VS_TONE, 70),
        K(CH_ORGAN, VO_DRAWBAR, 70), K(CH_ORGAN, VO_DECAY, 20), K(CH_ORGAN, VO_CLICK, 70),
        K(CH_LEAD, VL_INST, INST_BRASS), K(CH_LEAD, VL_BRIGHT, 70),
        {P_ECHO_FB, 40}, {P_ECHO_TONE, 75}, {P_SPRING_DECAY, 40}, {P_SPRING_TONE, 65}, {P_PH_ON, PH_OFF},
        {P_DRIVE_ON, DRIVE_OFF}, {0, 0}}},
    {"BLACK ARK", "BLACKARK", {                  /* B2: the Bi-Phase on the chords, a bright spring, tape pushed */
        K(CH_KICK, VK_TUNE, 38), K(CH_KICK, VK_SWEEP, 35), K(CH_KICK, VK_DECAY, 50), K(CH_KICK, VK_CLICK, 20),
        K(CH_KICK, VK_DRIVE, 25),
        K(CH_SNARE, VR_TUNE, 45), K(CH_SNARE, VR_DECAY, 30), K(CH_SNARE, VR_NOISE, 70), K(CH_SNARE, VR_INDEX, 55),
        K(CH_SNARE, VR_DROP, 20),
        K(CH_HATS, VH_TONE, 45), K(CH_HATS, VH_CLOSED, 35), K(CH_HATS, VH_NOISE, 75), K(CH_HATS, VH_HPF, -15),
        K(CH_BASS, VB_INDEX, 45), K(CH_BASS, VB_IDECAY, 35), K(CH_BASS, VB_DECAY, 40), K(CH_BASS, VB_CUTOFF, 30),
        K(CH_BASS, VB_SUB, 35), K(CH_BASS, VB_DRIVE, 30),
        K(CH_SKANK, VS_DECAY, 45), K(CH_SKANK, VS_TONE, 50), K(CH_SKANK, VS_ATTACK, 10),
        K(CH_ORGAN, VO_DRAWBAR, 55), K(CH_ORGAN, VO_DECAY, 30), K(CH_ORGAN, VO_DETUNE, 50),
        K(CH_LEAD, VL_VIBRATO, 60), K(CH_LEAD, VL_RELEASE, 60),
        {P_ECHO_FB, 65}, {P_ECHO_TONE, 50}, {P_ECHO_HEAD2, 60}, {P_SPRING_DECAY, 65}, {P_SPRING_TONE, 75},
        {P_PH_ON, PH_SKANK}, {P_PH_RATE, 25}, {P_PH_DEPTH, 100}, {P_PH_FB, 70}, {P_DRIVE_ON, DRIVE_MIX},
        {P_DRIVE_AMT, 40}, {0, 0}}},
    {"STUDIO ONE", "STUDIO1", {                  /* the earlier, cleaner room: a piano-like chop, the organ up */
        K(CH_KICK, VK_TUNE, 45), K(CH_KICK, VK_DECAY, 35), K(CH_KICK, VK_CLICK, 35),
        K(CH_SNARE, VR_TUNE, 60), K(CH_SNARE, VR_DECAY, 22), K(CH_SNARE, VR_INDEX, 65),
        K(CH_HATS, VH_CLOSED, 28), K(CH_HATS, VH_OPEN, 55),
        K(CH_BASS, VB_INDEX, 40), K(CH_BASS, VB_DECAY, 45), K(CH_BASS, VB_SUB, 25), K(CH_BASS, VB_SUSTAIN, 40),
        K(CH_SKANK, VS_RATIO, 0), K(CH_SKANK, VS_DECAY, 55), K(CH_SKANK, VS_TONE, 50), K(CH_SKANK, VS_INDEX, 55),
        K(CH_ORGAN, VO_DRAWBAR, 75), K(CH_ORGAN, VO_DECAY, 35), K(CH_ORGAN, VO_CLICK, 40),
        K(CH_LEAD, VL_INST, INST_BRASS), K(CH_LEAD, VL_ATTACK, 40), K(CH_LEAD, VL_BRIGHT, 55),
        {P_ECHO_FB, 35}, {P_ECHO_TONE, 70}, {P_SPRING_DECAY, 55}, {P_SPRING_TONE, 55}, {P_PH_ON, PH_OFF},
        {P_DRIVE_ON, DRIVE_OFF}, {0, 0}}},
    {"SHAKA STEPPERS", "SHAKA", {                /* B4, C11, D1: steppers for the scoops; the sub, the siren */
        K(CH_KICK, VK_TUNE, 28), K(CH_KICK, VK_SWEEP, 50), K(CH_KICK, VK_DECAY, 75), K(CH_KICK, VK_CLICK, 30),
        K(CH_KICK, VK_DRIVE, 45),
        K(CH_SNARE, VR_TUNE, 70), K(CH_SNARE, VR_DECAY, 20), K(CH_SNARE, VR_INDEX, 105), K(CH_SNARE, VR_NOISE, 55),
        K(CH_SNARE, VR_DROP, 25),
        K(CH_HATS, VH_TONE, 65), K(CH_HATS, VH_CLOSED, 20), K(CH_HATS, VH_NOISE, 55),
        K(CH_BASS, VB_INDEX, 35), K(CH_BASS, VB_IDECAY, 18), K(CH_BASS, VB_DECAY, 50), K(CH_BASS, VB_CUTOFF, 22),
        K(CH_BASS, VB_SUB, 100), K(CH_BASS, VB_SUSTAIN, 50), K(CH_BASS, VB_DRIVE, 35),
        K(CH_SKANK, VS_DECAY, 22), K(CH_SKANK, VS_TONE, 75), K(CH_SKANK, VS_INDEX, 85),
        K(CH_ORGAN, VO_DRAWBAR, 50), K(CH_ORGAN, VO_DECAY, 20),
        K(CH_LEAD, VL_VIBRATO, 45),
        K(CH_VOX, VX_RATE, 65), K(CH_VOX, VX_DEPTH, 48),
        {P_ECHO_FB, 60}, {P_ECHO_TONE, 60}, {P_SPRING_DECAY, 60}, {P_SPRING_TONE, 50}, {P_PH_ON, PH_OFF},
        {P_DRIVE_ON, DRIVE_BASS}, {P_DRIVE_AMT, 50}, {0, 0}}},
    {"SLENG TENG", "SLENG", {                    /* the digital turn: a buzzing, bright bass and a tight machine */
        K(CH_KICK, VK_TUNE, 50), K(CH_KICK, VK_SWEEP, 30), K(CH_KICK, VK_PTIME, 12), K(CH_KICK, VK_DECAY, 28),
        K(CH_KICK, VK_CLICK, 10),
        K(CH_SNARE, VR_TUNE, 90), K(CH_SNARE, VR_DECAY, 30), K(CH_SNARE, VR_NOISE, 100), K(CH_SNARE, VR_INDEX, 40),
        K(CH_SNARE, VR_HPF, 20),
        K(CH_HATS, VH_TONE, 90), K(CH_HATS, VH_CLOSED, 15), K(CH_HATS, VH_OPEN, 35), K(CH_HATS, VH_NOISE, 80),
        K(CH_HATS, VH_HPF, 20),
        K(CH_BASS, VB_INDEX, 100), K(CH_BASS, VB_IDECAY, 127), K(CH_BASS, VB_DECAY, 55), K(CH_BASS, VB_CUTOFF, 75),
        K(CH_BASS, VB_SUSTAIN, 70), K(CH_BASS, VB_FEEDBACK, 50),
        K(CH_SKANK, VS_INDEX, 100), K(CH_SKANK, VS_DECAY, 20), K(CH_SKANK, VS_TONE, 85), K(CH_SKANK, VS_RATIO, 0),
        K(CH_ORGAN, VO_DRAWBAR, 90), K(CH_ORGAN, VO_DECAY, 18), K(CH_ORGAN, VO_CLICK, 20),
        K(CH_LEAD, VL_INST, INST_BRASS), K(CH_LEAD, VL_BRIGHT, 90), K(CH_LEAD, VL_ATTACK, 10),
        K(CH_LEAD, VL_VIBRATO, 0),
        {P_ECHO_FB, 30}, {P_ECHO_TONE, 90}, {P_SPRING_DECAY, 30}, {P_SPRING_TONE, 80}, {P_PH_ON, PH_OFF},
        {P_DRIVE_ON, DRIVE_OFF}, {0, 0}}},
    {"ROCKERS UPTOWN", "ROCKERS", {              /* D2: the melodica forward, into the echo */
        K(CH_KICK, VK_TUNE, 42), K(CH_KICK, VK_DECAY, 45), K(CH_KICK, VK_CLICK, 30),
        K(CH_SNARE, VR_TUNE, 65), K(CH_SNARE, VR_DECAY, 20),
        K(CH_HATS, VH_CLOSED, 22), K(CH_HATS, VH_NOISE, 55),
        K(CH_BASS, VB_INDEX, 45), K(CH_BASS, VB_IDECAY, 28), K(CH_BASS, VB_DECAY, 38), K(CH_BASS, VB_CUTOFF, 32),
        K(CH_BASS, VB_SUB, 40),
        K(CH_SKANK, VS_DECAY, 30),
        K(CH_ORGAN, VO_DRAWBAR, 50), K(CH_ORGAN, VO_DECAY, 28),
        K(CH_LEAD, VL_INST, INST_MELODICA), K(CH_LEAD, VL_BRIGHT, 80), K(CH_LEAD, VL_ATTACK, 25),
        K(CH_LEAD, VL_RELEASE, 70), K(CH_LEAD, VL_VIBRATO, 60), K(CH_LEAD, VL_DETUNE, 10),
        {P_ECHO_FB, 60}, {P_ECHO_TONE, 65}, {P_SPRING_DECAY, 50}, {P_SPRING_TONE, 55}, {P_PH_ON, PH_OFF},
        {P_DRIVE_ON, DRIVE_OFF}, {0, 0}}},
};
#undef K
#define NKITS (sizeof KITS / sizeof KITS[0])
static uint8_t kit_sel, kit_bus;                 /* SETUP's pick: the kit, and VOICES (0) or VOICES + BUS (1) */

static int kit_is_voice(uint16_t id) { return id >= P_VOICE_BASE && id < P_VOICE_BASE + NCH * VOICE_STRIDE; }

static int kit_touches(uint32_t k, uint32_t ch)  /* 1: kit k sets something on channel ch */
{
    uint32_t i;
    for (i = 0; KITS[k].s[i].id; i++)
        if (kit_is_voice(KITS[k].s[i].id) && (KITS[k].s[i].id - P_VOICE_BASE) / VOICE_STRIDE == ch)
            return 1;
    return 0;
}

static void kit_load(uint32_t k, int bus);       /* (patches.c: the voices as factory patches, the bus) */

static void kit_value(char *t)                   /* SETUP's LOAD KIT row */
{
    t[0] = 0;
    text_cat(t, KITS[kit_sel % NKITS].name);
    text_cat(t, kit_bus ? " +BUS" : "");
}
