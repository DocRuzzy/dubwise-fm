/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* The demo song, RIGHTEOUS DUB: an original riddim and its dub in the manner of a 1970s Kingston mix (mastery guide,
 * Part F stage 4, "Tubby"), to show the firmware off and to learn from: a tour of every voice, model and effect, and
 * of the mixing style. Not a recreation of any record: its own bass line, melody and form. 72 BPM, A minor; ONE DROP
 * with a four-bar bass line on the chord lane (Am Dm Am Em), beat 1 left empty (D2); the rim into the echo, the
 * skank into the spring. 41 bars (2 min 17 s):
 *   INTRO 1-4     drum and bass, the desk CLEAN
 *   VERSE 5-12    the band, the MELODICA (FM); at 9 the desk to MCI (the same band, coloured)
 *   CHORUS 13-16  the REED melodica; the organ a B-3, its Leslie slow, fast, slow
 *   DUB 17-20     drum and bass: a freeze, the crash, siren A (CLASSIC), the 3T head, an echo swell, the Big Knob up
 *   DROP 21-24    the bass and the lead out: the PLUCK skank, the Big Knob a low-pass closing, an NJD siren, a dive
 *   VOCAL 25-32   the 808 drums, phased hats; HORN (trumpet), then FM+HORN (sax); the deejay's shouts
 *   VERSION 33-36 the lead and organ out: the echo a BBD, a dub push
 *   OUTRO 37-40   the band through the tape drive; the desk CLEAN, then MCI again; a FILL; the drop to wet
 *   END 41        everything out but the returns
 * The engineer's hands and the sound changes are the song's motion (P2.7): every one a move anyone can record. Two
 * things are the demo's own, not the song's (demo_tour): the desk's colour (P_DESK is a SETUP preference, not the
 * song's: switched while the demo plays, given back when it stops) and a caption at each change, so it's clear what is
 * heard. Loaded from SETUP (DEMO SONG): the patterns, sounds, chord lane, scenes, song and motion are replaced (not
 * saved until SAVE). Part of the unity build: dub.c includes it. */

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
#define DROP_M (M(CH_BASS) | M(CH_LEAD))
#define VER_M (M(CH_LEAD) | M(CH_ORGAN))
#define DEMO_BARS 41
static const demo_bar_t DEMO_SONG[DEMO_BARS] = {
    {2, DB_REST, 0, 0},                                           /* INTRO: drum and bass */
    {0, DB_REST, 0, -1}, {0, DB_REST, 0, -1}, {0, DB_REST, 0, -1},
    {1, 0, 0, 1},                                                 /* VERSE: the band, the melodica */
    {0, 0, 0, -1}, {0, 0, 0, -1}, {0, 0, 0, -1},
    {0, 0, 0, -1}, {0, 0, 0, -1}, {0, 0, 0, -1}, {0, 0, 0, -1},  /* (the desk MCI: demo_tour) */
    {0, 0, 0, 2},                                                 /* CHORUS: REED, the B-3 */
    {0, 0, 0, -1}, {0, 0, 0, -1}, {0, 0, 0, -1},
    {2, DB_REST, 0, 3},                                           /* DUB: drum and bass */
    {0, DB_REST, 0, -1},
    {0, DB_REST | M(CH_HATS), 0, -1},                             /* the hats out */
    {0, DB_REST | M(CH_HATS), 0, -1},                             /* (the Big Knob climbs: motion) */
    {0, DROP_M, 0, 4},                                            /* DROP: the bass and the lead out */
    {0, DROP_M, 0, -1}, {0, DROP_M, 0, -1}, {0, DROP_M, 0, -1},
    {1, 0, 0, 8},                                                 /* VOCAL: the 808s, HORN, the deejay */
    {0, 0, 0, -1}, {0, 0, 0, -1}, {0, 0, 0, -1},
    {0, 0, 0, -1}, {0, 0, 0, -1}, {0, 0, 0, -1}, {0, 0, 0, -1},  /* (FM+HORN) */
    {0, VER_M, 0, 11},                                            /* VERSION: the BBD */
    {0, VER_M, 0, -1}, {0, VER_M, 0, -1}, {0, VER_M, 0, -1},
    {1, 0, 0, 9},                                                 /* OUTRO */
    {0, 0, 0, -1}, {0, 0, 0, -1}, {0, 0, 0, -1},
    {0, 0xFFu, 0, 10},                                            /* END: all out, the returns ring on */
};

/* the lead, four bars over the chord lane (Am Dm Am Em); 0 rest; each note held two steps */
static const int8_t DEMO_LEAD[64] = {
    69, 0, 0, 0, 72, 0, 76, 0, 0, 0, 74, 0, 72, 0, 0, 0,       /* Am */
    74, 0, 0, 0, 77, 0, 81, 0, 0, 0, 79, 0, 77, 0, 0, 0,       /* Dm */
    76, 0, 0, 0, 72, 0, 69, 0, 0, 0, 72, 0, 71, 0, 0, 0,       /* Am */
    76, 0, 0, 0, 79, 0, 76, 0, 0, 0, 74, 0, 71, 0, 0, 0,       /* Em */
};

/* the engineer's hands and the sound changes (the song's motion): bar (from 0), tick in it (480 a beat), the
 * parameter, its value. Bar 0 sets every sound the song changes back to its start, so a second play starts the same */
typedef struct {
    uint16_t bar, tick, id;
    int16_t v;
} demo_move_t;
#define TH(ch) P_CH(ch, CHF_THROW)
#define V(ch, f) P_VOICE(ch, f)
#define SIREN(s, vel) (((s) + 1) | (vel) << 4)
#define SHOT(k, vel, t) ((k) | (vel) << 4 | (t) << 11)
static const demo_move_t DEMO_MOVES[] = {
    /* the start: the sounds as loaded */
    {0, 0, V(CH_LEAD, VL_INST), INST_MELODICA}, {0, 0, V(CH_LEAD, VL_HORN), HORN_TRUMPET},
    {0, 0, V(CH_ORGAN, VO_MODEL), ORG_BUBBLE}, {0, 0, V(CH_ORGAN, VO_ROTARY), ROT_OFF},
    {0, 0, V(CH_SKANK, VS_MODEL), 0}, {0, 0, V(CH_KICK, VK_MODEL), KICK_SINE},
    {0, 0, V(CH_SNARE, VR_MODEL), RIM_CROSS}, {0, 0, V(CH_HATS, VH_MODEL), HAT_METAL},
    {0, 0, V(CH_VOX, VX_SHAPE), 0}, {0, 0, V(CH_VOX, VX_SAY), 0}, {0, 0, P_ECHO_HEAD3, 0},
    {0, 0, P_ECHO_TYPE, ECHO_TAPE}, {0, 0, P_PH_ON, PH_OFF}, {0, 0, P_DRIVE_ON, DRIVE_OFF}, {0, 0, P_HPF_LP, 0},
    /* INTRO */
    {3, 960, TH(CH_SNARE), 1}, {3, 1200, TH(CH_SNARE), 0},       /* bar 4: the rim thrown */
    /* VERSE */
    {7, 960, TH(CH_SNARE), 1}, {7, 1200, TH(CH_SNARE), 0},
    {11, 480, TH(CH_SKANK), 1}, {11, 720, TH(CH_SKANK), 0},      /* bar 12: the skank into the spring */
    /* CHORUS: the REED; the organ a B-3, its Leslie slow, fast (a long ramp up), slow */
    {12, 0, V(CH_LEAD, VL_INST), INST_REED}, {12, 0, V(CH_ORGAN, VO_MODEL), ORG_B3},
    {12, 0, V(CH_ORGAN, VO_ROTARY), ROT_SLOW}, {12, 0, P_SPRING_KICK, 1}, {12, 120, P_SPRING_KICK, 0},
    {13, 480, TH(CH_ORGAN), 1}, {13, 960, TH(CH_ORGAN), 0},
    {14, 0, V(CH_ORGAN, VO_ROTARY), ROT_FAST}, {15, 960, V(CH_ORGAN, VO_ROTARY), ROT_SLOW},
    {15, 960, TH(CH_SNARE), 1}, {15, 1200, TH(CH_SNARE), 0},
    {15, 1440, P_ECHO_FREEZE, 1},                                /* held into the DUB .. */
    /* DUB */
    {16, 0, P_ECHO_FREEZE, 0}, {16, 0, P_SPRING_KICK, 1}, {16, 120, P_SPRING_KICK, 0},   /* .. and the crash */
    {16, 0, P_ECHO_HEAD3, 90},                                   /* the 3T head in */
    {16, 960, TH(CH_SNARE), 1}, {16, 1440, TH(CH_SNARE), 0},     /* a long one */
    {17, 0, P_SIREN, SIREN(SIREN_A, 60)}, {17, 1440, P_SIREN, 0},   /* siren A, CLASSIC */
    {17, 0, P_ECHO_FB, 52}, {17, 480, P_ECHO_FB, 62}, {17, 960, P_ECHO_FB, 72}, {17, 1440, P_ECHO_FB, 80},   /* swell */
    {18, 0, P_ECHO_FB, 52},
    {19, 0, P_HPF_STEP, 8}, {19, 480, P_HPF_STEP, 16}, {19, 960, P_HPF_STEP, 24}, {19, 1440, P_HPF_STEP, 32},  /* climb */
    /* DROP: the PLUCK skank; the Big Knob a low-pass, closing; an NJD siren; a tape dive */
    {20, 0, P_HPF_STEP, 0}, {20, 0, V(CH_SKANK, VS_MODEL), 1}, {20, 0, P_ECHO_HEAD3, 0},
    {21, 0, P_HPF_LP, 1}, {21, 0, P_HPF_STEP, 8}, {21, 480, P_HPF_STEP, 16}, {21, 960, P_HPF_STEP, 24},
    {21, 1440, P_HPF_STEP, 28}, {21, 960, TH(CH_SNARE), 1}, {21, 1200, TH(CH_SNARE), 0},
    {22, 0, P_HPF_STEP, 0}, {22, 0, P_HPF_LP, 0},
    {22, 0, V(CH_VOX, VX_SHAPE), 1}, {22, 960, P_SIREN, SIREN(SIREN_A, 60)}, {23, 480, P_SIREN, 0},
    {23, 0, P_ECHO_DIVE, 1}, {23, 1440, P_ECHO_DIVE, 0},
    /* VOCAL: the 808s, phased hats, HORN; the deejay's shout (black 8 says it) */
    {24, 0, V(CH_KICK, VK_MODEL), KICK_808}, {24, 0, V(CH_SNARE, VR_MODEL), RIM_808},
    {24, 0, V(CH_HATS, VH_MODEL), HAT_808}, {24, 0, P_PH_ON, PH_HATS}, {24, 0, V(CH_SKANK, VS_MODEL), 0},
    {24, 0, V(CH_LEAD, VL_INST), INST_HORN}, {24, 0, V(CH_VOX, VX_SAY), 2},
    {24, 60, P_SHOT, SHOT(0, 110, 1)},                           /* WHEEL UP */
    {27, 960, TH(CH_SNARE), 1}, {27, 1200, TH(CH_SNARE), 0},
    {28, 0, V(CH_LEAD, VL_INST), INST_LAYER}, {28, 0, V(CH_LEAD, VL_HORN), HORN_SAX},   /* FM+HORN, the sax */
    {28, 0, V(CH_VOX, VX_SAY), 3}, {28, 60, P_SHOT, SHOT(0, 110, 0)},   /* BIG UP */
    {29, 0, P_ECHO_FB, 52}, {29, 960, P_ECHO_FB, 72}, {30, 0, P_ECHO_FB, 52},
    {31, 480, TH(CH_SKANK), 1}, {31, 720, TH(CH_SKANK), 0},
    /* VERSION: the BBD */
    {32, 0, P_ECHO_TYPE, ECHO_BBD}, {32, 0, P_PH_ON, PH_OFF}, {32, 0, P_SPRING_KICK, 1}, {32, 120, P_SPRING_KICK, 0},
    {32, 960, TH(CH_SNARE), 1}, {32, 1440, TH(CH_SNARE), 0},
    {33, 0, P_ECHO_FB, 52}, {33, 480, P_ECHO_FB, 62}, {33, 960, P_ECHO_FB, 72}, {34, 0, P_ECHO_FB, 52},
    {34, 960, TH(CH_SNARE), 1}, {34, 1200, TH(CH_SNARE), 0},
    {34, 1200, P_ECHO_PUSH, 1}, {35, 0, P_ECHO_PUSH, 0},          /* a dub push */
    {35, 960, TH(CH_SKANK), 1}, {35, 1200, TH(CH_SKANK), 0},
    /* OUTRO: the tape echo back, the mix through the tape drive; a FILL; the drop to wet */
    {36, 0, P_ECHO_TYPE, ECHO_TAPE}, {36, 0, P_DRIVE_ON, DRIVE_MIX}, {36, 0, V(CH_VOX, VX_SAY), 7},
    {36, 60, P_SHOT, SHOT(0, 110, 1)},                           /* ONE MORE */
    {38, 960, TH(CH_SNARE), 1}, {38, 1200, TH(CH_SNARE), 0},
    {39, 0, P_FILL, 1}, {39, 1440, P_FILL, 0},                   /* the fill's steps play */
    {39, 1440, P_WET, 1},                                        /* the drop to wet: held to the end */
};
#define DEMO_NMOVES (sizeof DEMO_MOVES / sizeof DEMO_MOVES[0])
static uint8_t demo_moves_due;                   /* the moves to write once the engine has handed motion over */

/* the demo's own (not the song's): the desk's colour at a bar (-1 as it is), and a caption */
typedef struct {
    uint8_t bar;
    int8_t desk;
    const char *a, *b;
} demo_tour_t;
static const demo_tour_t DEMO_TOUR[] = {
    {0, DESK_CLEAN, "INTRO: DRUM AND BASS", "DESK: CLEAN"},
    {4, -1, "VERSE: THE BAND", "LEAD: MELODICA (FM)"},
    {8, DESK_MCI, "DESK: MCI", "THE SAME BAND, COLOURED"},
    {12, -1, "CHORUS: REED", "ORGAN: B-3, LESLIE"},
    {16, -1, "DUB: FREEZE, CRASH", "SIREN A, 3T HEAD"},
    {20, -1, "DROP: PLUCKED SKANK", "BIG KNOB: LOW-PASS"},
    {22, -1, "SIREN: NJD SHAPE", "TAPE DIVE"},
    {24, -1, "VOCAL: 808 DRUMS", "LEAD: HORN (TRUMPET)"},
    {28, -1, "LEAD: FM+HORN (SAX)", "DEEJAY: BIG UP"},
    {32, -1, "VERSION: BBD ECHO", "A DUB PUSH"},
    {36, DESK_CLEAN, "OUTRO: TAPE DRIVE", "DESK: CLEAN"},
    {38, DESK_MCI, "DESK: MCI", "A FILL, THEN WET"},
    {40, -1, "END", "THE RETURNS RING OUT"},
};
#define DEMO_NTOUR (sizeof DEMO_TOUR / sizeof DEMO_TOUR[0])
static uint8_t demo_live;                        /* the demo loaded (and nothing else since): its tour runs */
static int16_t demo_bar = -1;                    /* the song's bar the tour last saw (-1: stopped) */
static int8_t demo_desk_was = -1;                /* the desk's colour before the demo played (given back at the stop) */

static void demo_tour(void)                      /* every frame: the demo's captions and the desk's colour */
{
    uint32_t i, b;
    if (!demo_live || !eng_playing() || ctl_get(P_SONG_MODE) == SONG_OFF) {
        if (demo_desk_was >= 0) {                /* stopped: the desk given back */
            ctl_set(P_DESK, demo_desk_was);
            demo_desk_was = -1;
        }
        demo_bar = -1;
        return;
    }
    b = eng_song_bar();
    if ((int32_t)b == demo_bar)
        return;
    demo_bar = (int16_t)b;
    for (i = 0; i < DEMO_NTOUR; i++)
        if (DEMO_TOUR[i].bar == b) {
            if (DEMO_TOUR[i].desk >= 0) {
                if (demo_desk_was < 0)
                    demo_desk_was = (int8_t)ctl_get(P_DESK);
                ctl_set(P_DESK, DEMO_TOUR[i].desk);
            }
            app_say(DEMO_TOUR[i].a, DEMO_TOUR[i].b);
        }
}

static void demo_tick(void)                      /* every frame: the demo's moves, written while motion is ours */
{
    uint32_t i;
    demo_tour();
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
    ctl_set(P_ECHO_HEAD3, 0);
    ctl_set(P_ECHO_TYPE, ECHO_TAPE);
    ctl_set(P_SPRING_RET, 80);
    ctl_set(P_HPF_STEP, 0);
    ctl_set(P_HPF_LP, 0);
    ctl_set(P_PH_ON, PH_OFF);
    ctl_set(P_PH_RATE, 30);
    ctl_set(P_PH_DEPTH, 90);
    ctl_set(P_DRIVE_ON, DRIVE_OFF);
    ctl_set(P_DRIVE_AMT, 50);
    ctl_set(P_DRIVE_MIX, 60);
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
    ctl_set(P_CH(CH_VOX, CHF_ECHO_SEND), 70);
    ctl_set(P_VOICE(CH_LEAD, VL_INST), INST_MELODICA);
    ctl_set(P_VOICE(CH_LEAD, VL_HORN), HORN_TRUMPET);
    ctl_set(P_VOICE(CH_LEAD, VL_BLEND), 0);
    ctl_set(P_VOICE(CH_ORGAN, VO_MODEL), ORG_BUBBLE);
    ctl_set(P_VOICE(CH_ORGAN, VO_ROTARY), ROT_OFF);
    ctl_set(P_VOICE(CH_SKANK, VS_MODEL), 0);
    ctl_set(P_VOICE(CH_KICK, VK_MODEL), KICK_SINE);
    ctl_set(P_VOICE(CH_SNARE, VR_MODEL), RIM_CROSS);
    ctl_set(P_VOICE(CH_HATS, VH_MODEL), HAT_METAL);
    ctl_set(P_VOICE(CH_VOX, VX_SHAPE), 0);
    ctl_set(P_VOICE(CH_VOX, VX_SAY), 0);
    /* the patterns: ONE DROP, the bass line and the lead over it (all in A) */
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
    for (i = 13; i < 16u; i++) {                 /* the fill (6.7): a rim roll that plays only while FILL is held */
        step_t s;
        s.lvl = i == 15u ? LVL_ACCENT : LVL_NORMAL;
        s.note = default_note(CH_SNARE);
        STEP_COND_SET(s, COND_FILL);
        ctl_step(CH_SNARE, i, s);
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
    demo_live = 1;
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
    demo_live = 0;
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
    demo_live = 0;
    undo_reset();
    app_say("NEW PROJECT", "ONE DROP, 75 BPM");
}
