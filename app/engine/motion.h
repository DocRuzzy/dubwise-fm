/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* Motion (roadmap P2.7; prior art 11): what the hands do inside a bar, recorded with the song. While the song
 * captures (P_SONG_REC), every desk move the engine takes (a throw, a mute, a knob: any parameter from the strips up,
 * none of the globals) is written into the bar playing, at its moment (480 ticks a beat). While the song plays (or
 * captures), each bar's motion is played back at its moments.
 *   Punch-in by parameter: the first move of a parameter in a bar removes that parameter's motion from there to the
 *   bar's end (a throw still held at the next line goes on replacing); the other parameters keep playing (overdub).
 *   A hand on a parameter overrides its motion for the rest of the bar, captured or not.
 *   A knob's moves keep one value a sixteenth (the latest); a throw, a mute, a freeze, a push, a dive and the crash
 *   keep every press and release. A bar holds MOT_PER_BAR moves; more are dropped (counted).
 * The table is the engine's (written in the render while capturing); the main loop reads it to save, and clears it
 * only through the queue (CMD_MOTION). */
#pragma once
#include <stdint.h>
#include "params.h"
#include "song.h"

#define MOT_PER_BAR 32u
#define MOT_TPB 480u                             /* ticks a beat (a bar: 1,920) */
typedef struct {
    uint16_t tick, id;                           /* in the bar; the parameter */
    int16_t v;
} mot_ev_t;

extern mot_ev_t motion[SONG_BARS][MOT_PER_BAR];
extern uint8_t mot_n[SONG_BARS];
extern uint32_t mot_drops;
extern volatile uint32_t mot_dirty;              /* set by a capture or a clear; the main loop clears it on a save */

void mot_init(void);                             /* empty (at boot, before the engine runs) */
int mot_recordable(uint32_t id);
int mot_momentary(uint32_t id);                  /* 0..MOT_NMOM-1, or -1 */
#define MOT_NMOM (NCH + 6u)
uint32_t mot_mom_id(uint32_t k);                 /* the momentary's parameter */

/* engine (render) side */
void mot_clear(void);
void mot_line(int32_t bar, int rec, const int16_t *ev);   /* a bar starts: the bar (-1: no motion), capturing? */
void mot_user(uint32_t id, int32_t v, uint32_t tick, int rec_now);   /* a move by hand, at tick in the bar; rec_now:
                                                                    * P_SONG_REC now (off mid-bar: no more taken) */
int mot_due(uint32_t tick, mot_ev_t *e);         /* the next move due by tick (1), else 0 */
int32_t mot_next_tick(void);                     /* the next move's tick in this bar, -1 none */

/* engine -> main loop: the moves played back (so the screen follows) */
void mot_fb_push(uint32_t id, int32_t v);        /* engine */
int mot_fb_pop(uint32_t *id, int32_t *v);        /* main loop: 1 one taken */

/* the main loop: the flash copy (PLAT data area) */
#define MOT_FLASH 0xB0000u                       /* 0xB0000 .. 0xB9FFF, apart from the store objects (0xC8000 up) */
#define MOT_FLASH_SECTORS 10u
int mot_save(void);                              /* 1 written (or nothing to write) */
void mot_load(void);
uint32_t mot_total(void);
