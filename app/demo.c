/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* The demo song, RIGHTEOUS DUB: an original riddim and its dub in the manner of a 1970s Kingston mix (mastery guide,
 * Part F stage 4, "Tubby"), to show the firmware off and to learn from. Not a recreation of any record: its own bass
 * line, melodica line and form. 72 BPM, A minor; ONE DROP with a four-bar bass line on the chord lane (Am Dm Am Em),
 * beat 1 left empty (D2); the rim into the echo, the skank into the spring. 32 bars (1 min 47 s):
 *   INTRO 1-4      drum and bass             VERSE 5-12     the band and the melodica
 *   VERSION 13-16  the melodica out          DUB 17-20      drum and bass; the hats out, the Big Knob climbs
 *   DROP 21-24     the bass out              VERSE 25-28    the band and the melodica again
 *   OUTRO 29-31    the band                  END 32         everything out but the echo, dropped to wet
 * And the engineer's hands, as the song's motion (P2.7): snare and skank throws, spring crashes, a freeze into the
 * DUB, an echo swell, the Big Knob stepping up, a dub push, a tape dive, the drop to wet at the end.
 * Loaded from SETUP (DEMO SONG): the patterns, sounds, chord lane, scenes, song and motion are replaced (not saved
 * until SAVE). Part of the unity build: dub.c includes it. */

/* the bass, four bars of sixteenths: 0 rest, else the MIDI note (A1 33, C2 36, D2 38, E2 40, G1 31, F1 29, B1 35) */
static const int8_t DEMO_BASS[64] = {
    0, 0, 33, 0, 36, 0, 33, 0, 0, 40, 0, 38, 36, 0, 31, 0,      /* Am */
    0, 0, 38, 0, 41, 0, 38, 0, 0, 45, 0, 41, 38, 0, 36, 0,      /* Dm */
    0, 0, 33, 0, 36, 0, 33, 0, 0, 40, 0, 38, 36, 0, 31, 0,      /* Am */
    0, 0, 40, 0, 43, 0, 40, 0, 0, 35, 0, 38, 40, 0, 43, 0,      /* Em */
};

/* the song's bars: scene (0 none, else 1-16), mutes (a bit a channel), the Big Knob, a section (-1 none) */
typedef struct {
    uint8_t scene, mute;
    int8_t hpf, section;
} demo_bar_t;
#define M(ch) (1u << (ch))
#define DB_REST (M(CH_SKANK) | M(CH_ORGAN) | M(CH_LEAD))   /* drum and bass */
#define DEMO_BARS 32
static const demo_bar_t DEMO_SONG[DEMO_BARS] = {
    {2, DB_REST, 0, 0},                                           /* INTRO: drum and bass */
    {0, DB_REST, 0, -1}, {0, DB_REST, 0, -1}, {0, DB_REST, 0, -1},
    {1, 0, 0, 1},                                                 /* VERSE: the band, the melodica */
    {0, 0, 0, -1}, {0, 0, 0, -1}, {0, 0, 0, -1}, {0, 0, 0, -1}, {0, 0, 0, -1}, {0, 0, 0, -1}, {0, 0, 0, -1},
    {0, M(CH_LEAD), 0, 11},                                       /* VERSION: the melodica out */
    {0, M(CH_LEAD), 0, -1}, {0, M(CH_LEAD), 0, -1}, {0, M(CH_LEAD), 0, -1},
    {2, DB_REST, 0, 3},                                           /* DUB: drum and bass */
    {0, DB_REST, 0, -1},
    {0, DB_REST | M(CH_HATS), 0, -1},                             /* the hats out */
    {0, DB_REST | M(CH_HATS), 0, -1},                             /* (the Big Knob climbs: motion) */
    {0, M(CH_BASS) | M(CH_LEAD), 0, 4},                           /* DROP: the bass out, the band back */
    {0, M(CH_BASS) | M(CH_LEAD), 0, -1}, {0, M(CH_BASS) | M(CH_LEAD), 0, -1}, {0, M(CH_BASS) | M(CH_LEAD), 0, -1},
    {1, 0, 0, 1},                                                 /* VERSE */
    {0, 0, 0, -1}, {0, 0, 0, -1}, {0, 0, 0, -1},
    {0, M(CH_LEAD), 0, 9},                                        /* OUTRO */
    {0, M(CH_LEAD), 0, -1}, {0, M(CH_LEAD), 0, -1},
    {0, 0xFFu, 0, 10},                                            /* END: all out, the echo rings on */
};

/* the melodica, four bars over the chord lane (Am Dm Am Em); 0 rest; each note held two steps */
static const int8_t DEMO_LEAD[64] = {
    69, 0, 0, 0, 72, 0, 76, 0, 0, 0, 74, 0, 72, 0, 0, 0,       /* Am */
    74, 0, 0, 0, 77, 0, 81, 0, 0, 0, 79, 0, 77, 0, 0, 0,       /* Dm */
    76, 0, 0, 0, 72, 0, 69, 0, 0, 0, 72, 0, 71, 0, 0, 0,       /* Am */
    76, 0, 0, 0, 79, 0, 76, 0, 0, 0, 74, 0, 71, 0, 0, 0,       /* Em */
};

/* the engineer's hands (the song's motion): bar (from 0), tick in it (480 a beat), the parameter, its value */
typedef struct {
    uint16_t bar, tick, id;
    int16_t v;
} demo_move_t;
#define TH(ch) P_CH(ch, CHF_THROW)
static const demo_move_t DEMO_MOVES[] = {
    {3, 960, TH(CH_SNARE), 1}, {3, 1200, TH(CH_SNARE), 0},       /* bar 4: the rim thrown */
    {7, 960, TH(CH_SNARE), 1}, {7, 1200, TH(CH_SNARE), 0},
    {11, 480, TH(CH_SKANK), 1}, {11, 720, TH(CH_SKANK), 0},      /* bar 12: the skank into the spring */
    {12, 0, P_SPRING_KICK, 1}, {12, 120, P_SPRING_KICK, 0},      /* VERSION: the tank struck */
    {13, 480, TH(CH_ORGAN), 1}, {13, 960, TH(CH_ORGAN), 0},
    {15, 960, TH(CH_SNARE), 1}, {15, 1200, TH(CH_SNARE), 0},
    {15, 1440, P_ECHO_FREEZE, 1},                                /* held into the DUB .. */
    {16, 0, P_ECHO_FREEZE, 0}, {16, 0, P_SPRING_KICK, 1}, {16, 120, P_SPRING_KICK, 0},   /* .. and the crash */
    {16, 960, TH(CH_SNARE), 1}, {16, 1440, TH(CH_SNARE), 0},     /* a long one */
    {17, 0, P_ECHO_FB, 52}, {17, 480, P_ECHO_FB, 62}, {17, 960, P_ECHO_FB, 72}, {17, 1440, P_ECHO_FB, 80},   /* swell */
    {18, 0, P_ECHO_FB, 52},
    {19, 0, P_HPF_STEP, 8}, {19, 480, P_HPF_STEP, 16}, {19, 960, P_HPF_STEP, 24}, {19, 1440, P_HPF_STEP, 32},  /* climb */
    {20, 0, P_HPF_STEP, 0},
    {21, 960, TH(CH_SNARE), 1}, {21, 1200, TH(CH_SNARE), 0},
    {21, 1200, P_ECHO_PUSH, 1}, {22, 0, P_ECHO_PUSH, 0},          /* a dub push */
    {23, 0, P_ECHO_DIVE, 1}, {23, 1440, P_ECHO_DIVE, 0},         /* a tape dive */
    {27, 960, TH(CH_SNARE), 1}, {27, 1440, TH(CH_SNARE), 0},
    {27, 0, P_ECHO_FB, 52}, {27, 960, P_ECHO_FB, 72}, {28, 0, P_ECHO_FB, 52},
    {30, 960, P_WET, 1},                                         /* the drop to wet: held to the end */
};
#define DEMO_NMOVES (sizeof DEMO_MOVES / sizeof DEMO_MOVES[0])
static uint8_t demo_moves_due;                   /* the moves to write once the engine has handed motion over */

static void demo_tick(void)                      /* every frame: the demo's moves, written while motion is ours */
{
    uint32_t i;
    if (!demo_moves_due || !mot_suspended())
        return;
    mot_clear();
    for (i = 0; i < DEMO_NMOVES; i++)
        mot_put(DEMO_MOVES[i].bar, DEMO_MOVES[i].tick, DEMO_MOVES[i].id, DEMO_MOVES[i].v);
    demo_moves_due = 0;
    ctl_motion(2);                               /* back to the engine */
}

static void demo_load(void)
{
    uint32_t ch, i, b;
    pats_clear_all();                            /* (A playing, B..D empty) */
    /* the desk and the sounds */
    ctl_set(P_TEMPO, 720);
    ctl_set(P_KEY_ROOT, 9);
    ctl_set(P_SCALE, SCALE_AEOLIAN);
    ctl_set(P_CHORD_LEN, 4);
    ctl_set(P_CHORD0 + 0, 9 * 8 + CHORD_MIN);
    ctl_set(P_CHORD0 + 1, 2 * 8 + CHORD_MIN);
    ctl_set(P_CHORD0 + 2, 9 * 8 + CHORD_MIN);
    ctl_set(P_CHORD0 + 3, 4 * 8 + CHORD_MIN);
    ctl_set(P_ECHO_TIME, ECHO_DIV_8D);
    ctl_set(P_ECHO_FB, 52);
    ctl_set(P_ECHO_RET, 96);
    ctl_set(P_ECHO_WIDTH, 80);
    ctl_set(P_SPRING_RET, 80);
    ctl_set(P_HPF_STEP, 0);
    for (ch = 0; ch < NCH; ch++) {
        ctl_set(P_CH(ch, CHF_MUTE), 0);
        ctl_set(P_CH(ch, CHF_SOLO), 0);
        pat_len_set((int32_t)ch, ch == CH_BASS || ch == CH_LEAD ? 64 : 16);
    }
    ctl_set(P_CH(CH_KICK, CHF_LEVEL), 104);
    ctl_set(P_CH(CH_SNARE, CHF_ECHO_SEND), 60);
    ctl_set(P_CH(CH_HATS, CHF_LEVEL), 80);
    ctl_set(P_CH(CH_BASS, CHF_LEVEL), 110);
    ctl_set(P_CH(CH_SKANK, CHF_SPRING_SEND), 56);
    ctl_set(P_CH(CH_SKANK, CHF_LEVEL), 92);
    ctl_set(P_CH(CH_ORGAN, CHF_LEVEL), 84);
    ctl_set(P_CH(CH_LEAD, CHF_LEVEL), 88);
    ctl_set(P_CH(CH_LEAD, CHF_ECHO_SEND), 50);
    ctl_set(P_VOICE(CH_LEAD, VL_INST), INST_MELODICA);
    /* the patterns: ONE DROP, the bass line and the melodica over it (all in A) */
    ctl_template(TPL_ONE_DROP, 0xFFu, 0);
    for (i = 0; i < 64u; i++) {
        step_t s;
        s.lvl = DEMO_BASS[i] ? LVL_NORMAL : LVL_OFF;
        s.note = DEMO_BASS[i] ? DEMO_BASS[i] : 33;
        ctl_step(CH_BASS, i, s);
        s.lvl = DEMO_LEAD[i] ? LVL_NORMAL : LVL_OFF;
        s.note = DEMO_LEAD[i] ? DEMO_LEAD[i] : 69;
        ctl_step(CH_LEAD, i, s);
    }
    /* scene 1: the full band (with the patterns); scene 2: drum and bass */
    ctl_scene_store(0, SCOPE_PATTERNS);
    ctl_set(P_CH(CH_SKANK, CHF_MUTE), 1);
    ctl_set(P_CH(CH_ORGAN, CHF_MUTE), 1);
    ctl_set(P_CH(CH_LEAD, CHF_MUTE), 1);
    ctl_scene_store(1, SCOPE_DESK);
    ctl_set(P_CH(CH_SKANK, CHF_MUTE), 0);
    ctl_set(P_CH(CH_ORGAN, CHF_MUTE), 0);
    ctl_set(P_CH(CH_LEAD, CHF_MUTE), 0);
    scenes_dirty |= 3u;
    /* the song */
    for (b = 0; b < SONG_BARS; b++)
        song[b].flags = 0;
    for (b = 0; b < DEMO_BARS; b++) {
        const demo_bar_t *d = &DEMO_SONG[b];
        song[b].flags = BAR_SET | (d->section >= 0 ? BAR_SECTION : 0);
        song[b].scene = d->scene;
        song[b].mute = d->mute;
        song[b].solo = 0;
        song[b].hpf = d->hpf;
        song[b].section = (uint8_t)(d->section >= 0 ? d->section : 0);
    }
    ctl_set(P_SONG_LEN, DEMO_BARS);
    ctl_set(P_SONG_MODE, SONG_ONCE);
    ctl_set(P_SONG_REC, 0);
    ctl_motion(1);                               /* the song's motion handed over; demo_tick writes its moves */
    demo_moves_due = 1;
    set_dirty = 1;
    undo_reset();
    app_say("RIGHTEOUS DUB", "LOADED: PRESS PLAY");
}

/* UK steppers (P3.4): a desk for a sound-system steppers riddim, not a song (the song left as it is, its mode OFF).
 * 140 BPM (half time on the snare; a starting point, not checked against records: set it by ear), A minor; the sub
 * bass up front and through the tape; the rim into the echo, the skank chops into the spring; siren A ready on
 * channel 8. Mono returns (no width, no ping-pong): sound-system weight. */
static void steppers_load(void)
{
    uint32_t ch;
    ctl_set(P_TEMPO, 1400);
    ctl_set(P_KEY_ROOT, 9);
    ctl_set(P_SCALE, SCALE_AEOLIAN);
    ctl_set(P_CHORD_LEN, 2);
    ctl_set(P_CHORD0, 9 * 8 + CHORD_MIN);
    ctl_set(P_CHORD0 + 1, 2 * 8 + CHORD_MIN);
    ctl_set(P_ECHO_TIME, ECHO_DIV_8D);
    ctl_set(P_ECHO_FB, 58);
    ctl_set(P_ECHO_TONE, 60);
    ctl_set(P_ECHO_RET, 100);
    ctl_set(P_ECHO_PP, 0);
    ctl_set(P_ECHO_WIDTH, 0);
    ctl_set(P_SPRING_RET, 70);
    ctl_set(P_HPF_STEP, 0);
    ctl_set(P_PH_ON, PH_OFF);
    ctl_set(P_DRIVE_ON, DRIVE_BASS);
    ctl_set(P_DRIVE_AMT, 60);
    ctl_set(P_DRIVE_MIX, 70);
    for (ch = 0; ch < NCH; ch++) {
        ctl_set(P_CH(ch, CHF_MUTE), 0);
        ctl_set(P_CH(ch, CHF_SOLO), 0);
        pat_len_set((int32_t)ch, 16);
        ctl_set(P_CH(ch, CHF_ECHO_SEND), 0);
        ctl_set(P_CH(ch, CHF_SPRING_SEND), 0);
    }
    ctl_set(P_CH(CH_KICK, CHF_LEVEL), 92);
    ctl_set(P_CH(CH_SNARE, CHF_LEVEL), 90);
    ctl_set(P_CH(CH_SNARE, CHF_ECHO_SEND), 70);
    ctl_set(P_CH(CH_HATS, CHF_LEVEL), 70);
    ctl_set(P_CH(CH_BASS, CHF_LEVEL), 92);
    ctl_set(P_CH(CH_SKANK, CHF_LEVEL), 76);
    ctl_set(P_CH(CH_SKANK, CHF_SPRING_SEND), 60);
    ctl_set(P_CH(CH_VOX, CHF_ECHO_SEND), 80);
    ctl_set(P_VOICE(CH_BASS, VB_SUB), 100);      /* the sub up front .. */
    ctl_set(P_VOICE(CH_BASS, VB_CUTOFF), 20);    /* .. and little else */
    ctl_set(P_VOICE(CH_BASS, VB_DECAY), 50);
    ctl_set(P_VOICE(CH_SKANK, VS_DECAY), 30);    /* the chop: short and bright */
    ctl_set(P_VOICE(CH_SKANK, VS_TONE), 70);
    ctl_template(TPL_UKSTEP, 0xFFu, 0);
    ctl_set(P_SONG_MODE, SONG_OFF);
    ctl_set(P_SONG_REC, 0);
    set_dirty = 1;
    undo_reset();
    app_say("UK STEPPERS", "LOADED: PRESS PLAY");
}

/* NEW PROJECT (SETUP; the 2026-11 QoL review 5): the project as at the first switch-on. The parameters a slot keeps
 * (slots.c project_param: the desk, effects, sounds, key, chords, tempo, song settings) to their defaults, every
 * pattern empty but ONE DROP in A, the scenes, the song and its motion cleared. The device's own preferences (SETUP)
 * stay; the song slots too. Asked for twice (confirm). Undo starts again */
static int project_param(uint32_t id);
static void new_project(void)
{
    uint32_t id, ch, b, s;
    for (id = 0; id < P_NIDS; id++)
        if (project_param(id))
            ctl_set(id, param_def(id));
    pats_clear_all();
    for (ch = 0; ch < NCH; ch++)
        ctl_meta_set(ch, 0, pmeta_def());
    ctl_template(TPL_ONE_DROP, 0xFFu, 0);
    for (s = 0; s < NSCENES; s++)
        scenes[s].used = 0;
    scenes_dirty = 0xFFFFu;
    for (b = 0; b < SONG_BARS; b++)
        song[b].flags = 0;
    song_dirty = 1;
    ctl_set(P_SONG_REC, 0);
    ctl_motion_clear();
    set_dirty = 1;
    arr.sig = 0;
    undo_reset();
    app_say("NEW PROJECT", "ONE DROP, 75 BPM");
}
