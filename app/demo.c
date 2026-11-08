/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* The demo song: an original riddim and dub in the manner of a 1970s Kingston mix (mastery guide, Part F stage 4,
 * "Tubby"), to show the firmware off and to learn from. Not a recreation of any record: its own bass line and form.
 * 72 BPM, A minor; ONE DROP with a four-bar bass line on the chord lane (Am Dm Am Em), beat 1 left empty (D2);
 * the rim into the echo, the skank into the spring. The song, 16 bars:
 *   INTRO 1-4 and VERSE 5-8  the full band
 *   DUB 9-12                 drum and bass (skank and organ out), the hats out on 11, the Big Knob up on 12
 *   DROP 13-15               the bass out (and back on 15), the Big Knob back
 *   OUTRO 16                 the full band
 * Loaded from SETUP (DEMO SONG): the patterns, sounds, chord lane, scenes and song are replaced (not saved until SAVE).
 * Part of the unity build: dub.c includes it. */

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
static const demo_bar_t DEMO_SONG[16] = {
    {1, 0, 0, 0},                                                 /* INTRO */
    {0, 0, 0, -1},
    {0, 0, 0, -1},
    {0, 0, 0, -1},
    {0, 0, 0, 1},                                                 /* VERSE */
    {0, 0, 0, -1},
    {0, 0, 0, -1},
    {0, 0, 0, -1},
    {2, M(CH_SKANK) | M(CH_ORGAN), 0, 3},                         /* DUB: drum and bass */
    {0, M(CH_SKANK) | M(CH_ORGAN), 0, -1},
    {0, M(CH_SKANK) | M(CH_ORGAN) | M(CH_HATS), 2, -1},
    {0, M(CH_SKANK) | M(CH_ORGAN) | M(CH_HATS), 5, -1},           /* the Big Knob up: the mix thins */
    {0, M(CH_BASS), 0, 4},                                        /* DROP: the bass out, the band back */
    {0, M(CH_BASS), 0, -1},
    {0, 0, 0, -1},
    {1, 0, 0, 9},                                                 /* OUTRO */
};

static void demo_load(void)
{
    uint32_t ch, i, b;
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
        ctl_set(P_CH(ch, CHF_LEN), ch == CH_BASS ? 64 : 16);
    }
    ctl_set(P_CH(CH_KICK, CHF_LEVEL), 104);
    ctl_set(P_CH(CH_SNARE, CHF_ECHO_SEND), 60);
    ctl_set(P_CH(CH_HATS, CHF_LEVEL), 80);
    ctl_set(P_CH(CH_BASS, CHF_LEVEL), 110);
    ctl_set(P_CH(CH_SKANK, CHF_SPRING_SEND), 56);
    ctl_set(P_CH(CH_SKANK, CHF_LEVEL), 92);
    ctl_set(P_CH(CH_ORGAN, CHF_LEVEL), 84);
    /* the patterns: ONE DROP, the bass line over it */
    ctl_template(TPL_ONE_DROP, 0xFFu, 0);
    for (i = 0; i < 64u; i++) {
        step_t s;
        s.lvl = DEMO_BASS[i] ? LVL_NORMAL : LVL_OFF;
        s.note = DEMO_BASS[i] ? DEMO_BASS[i] : 33;
        ctl_step(CH_BASS, i, s);
    }
    /* scene 1: the full band (with the patterns); scene 2: drum and bass */
    ctl_scene_store(0, SCOPE_PATTERNS);
    ctl_set(P_CH(CH_SKANK, CHF_MUTE), 1);
    ctl_set(P_CH(CH_ORGAN, CHF_MUTE), 1);
    ctl_scene_store(1, SCOPE_DESK);
    ctl_set(P_CH(CH_SKANK, CHF_MUTE), 0);
    ctl_set(P_CH(CH_ORGAN, CHF_MUTE), 0);
    scenes_dirty |= 3u;
    /* the song */
    for (b = 0; b < SONG_BARS; b++)
        song[b].flags = 0;
    for (b = 0; b < 16u; b++) {
        const demo_bar_t *d = &DEMO_SONG[b];
        song[b].flags = BAR_SET | (d->section >= 0 ? BAR_SECTION : 0);
        song[b].scene = d->scene;
        song[b].mute = d->mute;
        song[b].solo = 0;
        song[b].hpf = d->hpf;
        song[b].section = (uint8_t)(d->section >= 0 ? d->section : 0);
    }
    ctl_set(P_SONG_LEN, 16);
    ctl_set(P_SONG_MODE, SONG_ONCE);
    ctl_set(P_SONG_REC, 0);
    set_dirty = 1;
    app_say("DEMO SONG", "LOADED: PRESS PLAY");
}
