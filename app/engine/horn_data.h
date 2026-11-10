/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* Made by tools/brass/mkhorn.py (roadmap 7.7): INST HORN's bell and its horns' harmonic patterns, fitted to
 * Freesound recordings (credits in CREDITS.md). Do not edit. */
#pragma once
#define HORN_HP_HZ 366.4f
#define HORN_HP_Q 1.736f
#define HORN_PK_HZ 1823.8f
#define HORN_PK_Q 1.158f
#define HORN_PK_DB 6.89f
#define HORN_LP_HZ 6202.8f
#define HORN_LP_Q 2.500f
#define HORN_NK 32
/* the source's fall, dB an octave of harmonic number: [horn][soft, loud] */
static const float HORN_SLOPE[2][2] = {{15.0f, 8.5f}, {14.0f, 9.5f}};
/* each table's pattern, dB by harmonic (1 ..): TRUMPET soft, TRUMPET loud, SAX soft, SAX loud */
static const signed char HORN_RES[4][HORN_NK] = {
    {2, 2, 1, 2, 1, 2, -2, -10, -8, -4, -10, -6, -2, -7, -8, -13, -15, -8, -10, -15, -15, -15, -15, -15, -15, -15, -12, -9, -4, -7, 0, 0},
    {-6, -4, -1, 1, 6, 5, 4, -5, 2, -5, -3, 0, -3, -8, -8, -8, -15, -12, -14, -15, -15, -15, -15, -15, -15, -15, -15, -15, -15, -15, 0, 0},
    {2, 3, 3, 7, -2, 4, -14, -2, -8, 1, 2, -4, -2, -4, -3, -10, -15, -12, -11, -15, -15, -14, -15, -15, -13, -15, -15, -15, -15, -8, 0, 0},
    {2, 3, 3, 7, -2, 4, -14, -2, -8, 1, 2, -4, -2, -4, -3, -10, -15, -12, -11, -15, -15, -14, -15, -15, -13, -15, -15, -15, -15, -8, 0, 0},
};
static const float HORN_NOISE_DB[2] = {-24.0f, -42.0f};   /* the breath, against the tone: trumpet, sax */
