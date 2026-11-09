/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* The song (roadmap P1.8; spec 4.2): a list of bars. A bar may hold a scene to recall, the mutes and solos, the
 * Big Knob, and a section marker; an empty bar changes nothing. Capture (P_SONG_REC while playing) writes the desk
 * into each bar at its line; playback (P_SONG_MODE) applies each bar at its line. One table, shared: the engine
 * writes it while capturing and reads it while playing; the UI edits markers and length, copies it for undo. */
#pragma once
#include <stdint.h>
#include "params.h"
#include "riddim.h"

#define SONG_BARS 512                            /* 28 min at 72 BPM, 15 at 140 */
enum { SONG_OFF, SONG_ONCE, SONG_LOOP, SONG_N };
enum { BAR_SET = 1, BAR_SECTION = 2, BAR_PATS = 4 };   /* song_bar_t.flags */
#define SECTIONS 15                              /* the black keys' 11, and HOME + black 1-4: the dub-plate words */
#define SECTION_KEYS 11
typedef struct {                                 /* 7 bytes: 512 bars in one store object (PLAT_STORE_MAX) */
    uint8_t flags : 3;                           /* BAR_SET: the desk below is the bar's; BAR_SECTION: a marker;
                                                  * BAR_PATS: the patterns below are the bar's (4.8) */
    uint8_t section : 5;                         /* the marker's name (0..SECTIONS-1) */
    uint8_t scene;                               /* 0 none, else scene + 1 recalled at the line */
    uint8_t mute, solo;                          /* a bit a channel */
    int8_t hpf;                                  /* the Big Knob's step */
    uint8_t pats[2];                             /* each channel's pattern, 2 bits a channel (channel 1 lowest) */
} song_bar_t;
typedef char song_bar_7[sizeof(song_bar_t) == 7 ? 1 : -1];
static inline uint32_t bar_pat(const song_bar_t *b, uint32_t ch) { return (uint32_t)(b->pats[ch / 4u] >> (ch % 4u * 2u)) & 3u; }
extern song_bar_t song[SONG_BARS];
const char *section_name(int32_t s);             /* "INTRO", "VERSE" .. */

/* engine, at a bar's line: write the desk into it (capture), or apply it (playback) */
void song_capture(uint32_t bar, const int16_t *vals, int32_t scene_this_bar);
int song_apply(uint32_t bar, int16_t *vals);     /* -1 nothing, else the scene to recall (0..15) or 16: none */
