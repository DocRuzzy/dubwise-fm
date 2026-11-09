/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* The engine: the audio interrupt's side. Each block it applies the queued commands, renders each channel's source
 * into its own buffer, and mixes the eight channels in a fixed order into the mono dry path and the echo and
 * spring send buses (whose effects are P0.3 and P1.2: their returns are silent for now). Stereo is only for the
 * returns (design principle 6): the dry path goes to both sides.
 *
 * The render never allocates, never waits and never touches the flash. CMD_SET takes effect at the start of the
 * next block (at most 5.8 ms: player input). CMD_CUE lands on its exact sample: the transport counts beats with
 * integer arithmetic, and the block is split where a cue falls (launch quantise, P0.1). */
#pragma once
#include <stdint.h>
#include "params.h"
#include "cmdq.h"
#include "riddim.h"

extern cmdq_t eng_q;                         /* control.c pushes, eng_render pops */

void eng_init(void);                         /* boot: every parameter at its default, everything silent */
void eng_render(int32_t *lr, uint32_t n, uint32_t master_q12);   /* audio interrupt; n <= ENG_BLOCK */
/* the USB recording's own channels (QoL 4; SETUP: USB STEMS): before eng_render, want = a computer records; the
 * block's stems after it, 8 floats a frame (full scale 1.0), NULL when none are made */
const float *eng_stems(int want);
#ifdef PLAT_HOST
/* (tests, the PC simulator only) a voice's own peak over the last block, before its strip (a muted voice still shows);
 * the event log: every parameter change and note while playing, at its sample since the start (roadmap 5.1) */
float eng_src_peak(uint32_t ch);
void eng_evlog(void (*f)(uint32_t t, uint32_t kind, uint32_t id, int32_t v));
#endif
/* the live input (P2.8) for the block about to render: n stereo frames (int32, full scale `full`), on channel 8 as
 * mono, mutable and throwable like any channel; NULL: none. Audio interrupt, before eng_render */
void eng_live_in(const int32_t *lr, uint32_t n, float full);
int eng_spinning(void);
/* MIDI clock (SETUP: MIDI CLOCK): in, from app_render's MIDI; out, popped by the 1 kHz tick when due */
void eng_clock_in(uint32_t st, uint32_t t24);
int eng_clock_pop(uint32_t now, uint32_t *st);
uint32_t eng_clocks_sent(void);
uint32_t eng_clock_now(void);                    /* the clock's time base (24 MHz) */                          /* 1: the spin-back (P3.5) and its gap after */
void eng_panic(void);                        /* silence now */

/* the voices (voices.h): a hit or a note on channel ch, from the audio interrupt (MIDI in, the sequencer) */
void eng_trig(uint32_t ch, int32_t note, uint32_t vel);
void eng_release(uint32_t ch, int32_t note);
int32_t eng_note(void);                      /* the bass's note held, -1 none */
int32_t eng_voice_note(uint32_t ch);         /* the bass's or the lead's note held, -1 none */

/* read back (main loop): what the engine has applied, and its meters */
int32_t eng_param(uint32_t id);
float eng_ch_peak(uint32_t ch);              /* the channel's post-fader peak over the last block, 0..1+ */
uint32_t eng_trig_count(uint32_t ch);        /* the channel's hits so far (a new one: a new count; 5.10's playhead) */
float eng_echo_peak(void);                   /* the echo send bus's */
float eng_spring_peak(void);
float eng_echo_ret_peak(void);               /* the echo's return, after its level */
float eng_echo_gr(void);                     /* the return limiter's gain (1: not limiting) */
int32_t eng_echo_div(void);                  /* the division in use (P_ECHO_TIME, or shorter if that does not fit) */
float eng_echo_t(void);                      /* the echo's time now, line samples (11,025 a second) */
float eng_echo_g(void);                      /* the loop gain asked for (1 and over: rings on) */
uint32_t eng_cmds(void);                     /* commands applied since boot */
int eng_playing(void);
uint32_t eng_beat(void);                     /* beats since the start (bar = beat / 4 + 1) */
uint32_t eng_fire_at(void);                  /* the sample (since the start) the last cue landed on */
uint32_t eng_fires(void);                    /* cues fired since boot */
uint32_t eng_cues(void);                     /* cues waiting */
uint32_t eng_cue_overflows(void);
uint32_t eng_seq_hits(void);
uint32_t eng_ticks(void);
int eng_tier(void);                          /* the guard tier now (TIER_*) */
uint32_t eng_load_pct(void);                 /* the load, % of the device's block (ticks; the host's are its own) */
uint32_t eng_tier_changes(void);
uint32_t eng_song_bar(void);                 /* the song's bar playing (or capturing) */
int32_t eng_chord_last(uint32_t ch);         /* the chord a chord channel last played (root x 8 + quality) */
float eng_spring_ret_peak(void);            /* the spring's return, after its level */
uint32_t eng_crashes(void);                  /* the tank struck, times since boot */
float eng_out_peak(void);                   /* the mix's peak over the last block, before MASTER */
int eng_midi_allowed(int32_t note);           /* the key lock on MIDI in (audio interrupt) */
uint32_t eng_frac16(void);                   /* how far into the sixteenth, 0..255 */                    /* runway tick blips since boot */
int32_t eng_tick(void);                      /* the sixteenth playing since the start, -1 stopped */                 /* steps the sequencer has played since boot */
uint32_t eng_seq_hit_at(uint32_t ch);        /* channel ch's last hit, samples since the start */
step_t eng_step(uint32_t ch, uint32_t i);    /* the engine's copy of a step of the pattern playing (tests) */
step_t eng_step_at(uint32_t ch, uint32_t p, uint32_t i);
pmeta_t eng_pmeta(uint32_t ch, uint32_t p);
uint32_t eng_pat(uint32_t ch);               /* the pattern playing (it changes on its line) */
int32_t eng_fine(void);                      /* fine ticks since the start (riddim.h PPQ), -1 stopped */
int32_t eng_pat_org(uint32_t ch);            /* the fine tick the channel's pattern was launched on (its step 0) */

#define ENG_BLOCK 256u

/* per-part profiling (P0.7): plat_ticks24 ticks (24 a microsecond) each part took in a block, a running average
 * (1/16 a block) and the most since the last eng_prof_reset. The device's budget for a block is
 * ENG_BLOCK / 44100 s = 139,320 ticks; the host runs ~80x faster than the device */
enum { PART_CMD, PART_CH0, PART_MIX = PART_CH0 + NCH, PART_ECHO, PART_SPRING, PART_OUT, NPARTS };
typedef struct {
    uint32_t avg;                            /* x16: ticks a block = avg / 16 */
    uint32_t max;                            /* ticks */
} eng_prof_t;
eng_prof_t eng_prof(uint32_t part);
const char *eng_part_name(uint32_t part);
void eng_prof_reset(void);
#define ENG_BLOCK_TICKS 139320u
