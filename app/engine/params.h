/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* One parameter ID space (roadmap: engine architecture, plumbing). Every value the UI hands the engine is a
 * parameter: an ID and an int32 value. Mutes, solos, sends and throws are parameters too, because the desk is the
 * instrument: scenes, the song and MIDI store them by ID.
 *
 * IDs are stable: saved scenes and songs store them. Append within a range, never renumber, never reuse.
 *   0x000-0x03F  global
 *   0x040-0x0BF  channel strips: 8 channels x 16 fields (P_CH)
 *   0x0C0-0x0FF  bus modules: echo, spring, Big Knob HPF, phaser, master (P0.3, P1.x)
 *   0x100-0x1FF  voices: 8 channels x 32 fields (P_VOICE)
 * Commands (cmdq.h) carry an opcode beside the ID, so UI codes never share this space (prior-art lesson 9). */
#pragma once
#include <stdint.h>

enum { CH_KICK, CH_SNARE, CH_HATS, CH_BASS, CH_SKANK, CH_ORGAN, CH_LEAD, CH_VOX, NCH };   /* spec 3 */

/* channel strip fields (append only, at most CH_STRIDE) */
enum { CHF_LEVEL, CHF_MUTE, CHF_SOLO, CHF_ECHO_SEND, CHF_SPRING_SEND, CHF_THROW, CHF_THROW_DEST, CHF_LEN, CHF_THROW_AMT,
       CHF_PAT, CHF_N };   /* (CHF_LEN: retired in the session, 4.8; a pattern's length is its own, riddim.h pmeta_t) */
enum { DEST_ECHO, DEST_SPRING, DEST_BOTH, DEST_N };   /* a throw's destination (spec 3) */
/* voice fields, each channel its own (append only, at most VOICE_STRIDE; voices.h) */
/* (roadmap 5.3 appended fields after each voice's own: 0 is the voice as before, so a save from before loads as it
 * sounded; the bipolar ones (-64..63) move either way from it) */
enum { VK_TUNE, VK_SWEEP, VK_PTIME, VK_DECAY, VK_CLICK, VK_DRIVE, VK_CTONE, VK_N };     /* kick */
enum { VR_TUNE, VR_DECAY, VR_NOISE, VR_INDEX, VR_DROP, VR_HPF, VR_N };                  /* rim / snare */
enum { VH_TONE, VH_CLOSED, VH_OPEN, VH_NOISE, VH_HPF, VH_N };                           /* hats */
enum { VB_INDEX, VB_IDECAY, VB_DECAY, VB_CUTOFF, VB_RATIO, VB_SUB, VB_SUSTAIN, VB_FEEDBACK, VB_DRIVE, VB_GLIDE,
       VB_N };                                                                          /* bass */
enum { VS_INDEX, VS_DECAY, VS_TONE, VS_RATIO, VS_ATTACK, VS_N };                        /* skank (P1.6) */
enum { VO_DRAWBAR, VO_DECAY, VO_CLICK, VO_DETUNE, VO_N };                               /* organ bubble */
enum { VX_PITCH, VX_RATE, VX_DEPTH, VX_WAVE, VX_LIVE, VX_N };   /* vox / fx: the sirens (P2.1); LIVE: the input (P2.8) */
enum { VL_INST, VL_BRIGHT, VL_ATTACK, VL_RELEASE, VL_VIBRATO, VL_DETUNE, VL_N };        /* lead: melodica / brass (P2.2) */
/* the MAIN macros (roadmap 5.3, spec 4.9): four a channel (not the vox: its MAIN is the siren's own four), stored as
 * the voice's last four fields; -64..63, 0 every target at its default. The engine never reads them: turning one
 * rewrites its raw targets (sound.c) */
/* A field appended to a voice must sound neutral at 0: an older save (OBJ_SOUND) or patch (5.4) holds 0 there, and
 * loads as it was made. One that can't be needs a version and a converter, planned first (CLAUDE.md). Every voice
 * field's range fits int8 (a patch keeps them so: patches.txt checks, voice_int8). */
#define VM_BASE (VOICE_STRIDE - 4u)
#define VM_N 4u
enum { INST_MELODICA, INST_BRASS, INST_N };
/* the chord channels' steps hold a voicing, not a note: the notes are the chord lane's (P_CHORD0..) */
enum { VOICING_LO, VOICING_MID, VOICING_HI, VOICING_N };
/* a chord code (root x 8 + quality) voiced: three notes from base up (MIDI), the root's octave by the voicing */
void chord_notes(int32_t code, int32_t voicing, int32_t base, int32_t *notes3);

#define P_GLOBAL_BASE 0x000u
#define P_CH_BASE 0x040u
#define CH_STRIDE 16u
#define P_BUS_BASE 0x0C0u
#define P_VOICE_BASE 0x100u
#define VOICE_STRIDE 32u
#define P_NIDS 0x200u

#define P_CH(ch, f) ((uint16_t)(P_CH_BASE + (uint32_t)(ch) * CH_STRIDE + (uint32_t)(f)))
#define P_VOICE(ch, f) ((uint16_t)(P_VOICE_BASE + (uint32_t)(ch) * VOICE_STRIDE + (uint32_t)(f)))

/* globals (append only) */
enum {
    P_TEMPO = 0x000,          /* BPM x 10 (750 = 75.0) */
    P_PLAY,                   /* the transport: 1 playing (a start counts from bar 1, beat 1) */
    P_QUANT,                  /* launch quantise: QUANT_BEAT .. QUANT_32 (the UI's setting; cues carry beats) */
    P_LEAD,                   /* a quantised action fires this many ms before the line ("a fraction early") */
    P_CYCLE,                  /* the cycle (spec 5.4): CYCLE_8 .. CYCLE_64 bars, from the start */
    P_RUNWAY,                 /* the runway: its last 1, 2, 4 or 8 bars (RUNWAY_*) */
    P_RUNWAY_KEYS,            /* 1: the white keys flash the last bar's beats */
    P_RUNWAY_NUM,             /* 1: the big countdown numeral */
    P_TICK,                   /* 1: an audible tick on the runway's beats */
    P_REDUCED,                /* 1: reduced motion (no pulses, no flash) */
    P_KEY_ROOT,               /* the key's root, 0 C .. 11 B (P0.8) */
    P_SCALE,                  /* SCALE_* */
    P_LOCK,                   /* LOCK_OFF, LOCK_SCALE, LOCK_CHORD: out-of-set keys silent (never remapped) */
    P_KB_OCT,                 /* the keyboard's octave: 0..5 = -3..+2 (the bass plays at -2) */
    P_LOCK_MIDI,              /* 1: the lock filters MIDI in too */
    P_CHORD_LEN,              /* the chord lane's bars, 1..16 */
    P_SONG_MODE,              /* the song (P1.8): SONG_OFF, SONG_ONCE, SONG_LOOP */
    P_SONG_LEN,               /* its bars, 1..SONG_BARS (512) */
    P_SONG_REC,               /* 1: capturing (each bar's desk written at its line while playing) */
    P_TIER_LOCK,              /* the guard tier (P1.9): 0 automatic, else locked to TIER_* + 1 */
    P_THROW_VEL,              /* 1: a throw as hard as the key was hit (P2.9; CHF_THROW_AMT), 0: always in full */
    P_SAVER,                  /* the screen saver: SAVER_OFF, SAVER_1 .. SAVER_15 (minutes untouched, stopped) */
    P_ROOTS,                  /* 1: ROOTS NAMES, the spoken homages (hidden: SETUP, black 11 three times) */
    P_HPF_RES,                /* the Big Knob's steps a turn: HPF_RES_10 (the Altec's), HPF_RES_20, HPF_RES_40 */
    P_CLOCK,                  /* MIDI clock: CLOCK_OFF, CLOCK_IN (follow a computer or a drum machine), CLOCK_OUT (lead) */
    P_AUTOSAVE,               /* 1: saved by itself, a few seconds after the last change while stopped and untouched */
    P_REC_QUANT,              /* live recording (4.8): REC_FEEL (as played), REC_HALF (halfway to the grid), REC_GRID */
    P_REC_MODE,               /* REC_OVERDUB (adds), REC_REPLACE (what the playhead passes is cleared, bar what is played) */
    P_REC_OFFSET,             /* ms taken off a played note's time: the sound's way out (the device's buffers, a browser's) */
    P_REC_CLICK,              /* the click: CLICK_COUNT_IN (the bar before recording), CLICK_RECORDING (and while recording),
                               * CLICK_OFF */
    P_USB_STEMS,              /* the USB recording's own channels 3-10 (QoL 4): STEMS_CHANNELS (the eight strips, dry,
                               * post-fader), STEMS_BUSES (drums, bass, chords, lead + vox, the echo's and the spring's
                               * returns L R), STEMS_OFF */
    P_USB_PORT,               /* USBPORT_COMPUTER, USBPORT_KEYBOARD (the USB MIDI host: a keyboard into the FM-1; QoL 6) */
    P_NGLOBAL,
    P_CHORD0 = 0x020,         /* the chord lane: 16 bars, each root x 8 + CHORD_* (P_CHORD0 + bar) */
    P_CHORD_END = 0x030,      /* (the globals above must stay under P_CHORD0: params.c checks) */
    P_CLICK = 0x030,          /* the click on every beat now (the UI's: a count-in, a recording; never saved) */
    P_DICE_LINE = 0x031,      /* 1: a Dice fire waits for the quantise line (5.6; SETUP; saved in the settings' spare byte) */
    P_DICE_SEED = 0x032,      /* Dice's generator seeded with this (tests; 0: from the clock; never saved) */
    P_DEV_LOAD = 0x03E,       /* development: this much % added to the measured load (the guard's host tests) */
    P_DEV_TONES = 0x03F,      /* development: a test tone on every channel (host tests of the mixer) */
};
enum { QUANT_BEAT, QUANT_1, QUANT_2, QUANT_4, QUANT_8, QUANT_16, QUANT_32, QUANT_CYCLE, QUANT_N };
enum { CYCLE_8, CYCLE_16, CYCLE_32, CYCLE_64, CYCLE_N };
enum { RUNWAY_1, RUNWAY_2, RUNWAY_4, RUNWAY_8, RUNWAY_N };
enum { SCALE_AEOLIAN, SCALE_DORIAN, SCALE_MIN_PENT, SCALE_MIN_BLUES, SCALE_IONIAN, SCALE_MIXOLYDIAN,
       SCALE_MAJ_PENT, SCALE_HARM_MIN, SCALE_PHRYGIAN, SCALE_N };
enum { CHORD_MIN, CHORD_MAJ, CHORD_7, CHORD_M7, CHORD_MAJ7, CHORD_SUS4, CHORD_DIM, CHORD_N };
enum { LOCK_OFF, LOCK_SCALE, LOCK_CHORD, LOCK_N };
enum { TIER_STUDIO, TIER_FOUR, TIER_SEVEN, TIER_N };
enum { SAVER_OFF, SAVER_1, SAVER_5, SAVER_15, SAVER_N };
enum { HPF_RES_10, HPF_RES_20, HPF_RES_40, HPF_RES_N };
enum { REC_FEEL, REC_HALF, REC_GRID, REC_QUANT_N };
enum { STEMS_CHANNELS, STEMS_BUSES, STEMS_OFF, STEMS_N };
enum { USBPORT_COMPUTER, USBPORT_KEYBOARD, USBPORT_N };
enum { REC_OVERDUB, REC_REPLACE, REC_MODE_N };
enum { CLICK_COUNT_IN, CLICK_RECORDING, CLICK_OFF, CLICK_N };
enum { CLOCK_OFF, CLOCK_IN, CLOCK_OUT, CLOCK_N };   /* a detent: 4, 2 or 1 fine steps */   /* the guard tiers (design principles 5) */
const char *tier_name(int32_t t);                    /* "STUDIO", "FOUR-TRACK", "SEVEN-INCH" */
const char *scale_name(int32_t s);                   /* "NATURAL MINOR" */
const char *chord_name(int32_t q);                   /* "MINOR" */
const char *chord_short(int32_t q);                  /* "m" */
const char *pitch_name(int32_t pc);                  /* "C", "C#" .. */
uint32_t scale_mask(int32_t root, int32_t s);        /* the allowed pitch classes, a bit each (bit 0 = C) */
uint32_t chord_mask(int32_t code);                   /* a chord lane entry's pitch classes */
/* the lock's allowed set now, from the values given (vals[id]): all 12 when off. bar: the bar playing (the chord
 * lane follows it) */
uint32_t lock_mask(const int16_t *vals, uint32_t bar);

/* bus modules (append only, within 0x0C0-0x0FF) */
enum {
    P_ECHO_TIME = 0x0C0,      /* a division of the beat: ECHO_DIV_* (the longest that fits the line at this tempo) */
    P_ECHO_FB,                /* feedback as T60: 0.1 s x 2^(v/10), 0..100 (100: about 100 s) */
    P_ECHO_RET,               /* the return's level, 0..127 (send law) */
    P_ECHO_TONE,              /* the loop's low-pass, 0..127: 800 Hz .. 5 kHz */
    P_ECHO_HEAD2,             /* the 2T head in the return, 0..127 */
    P_ECHO_FREEZE,            /* 1: the input closed, the loop held */
    P_ECHO_PUSH,              /* 1: the loop past unity (capped): "dub push" */
    P_ECHO_DIVE,              /* 1: the tape dive (P1.4): the time glides to DEPTH x T, the repeats bend down */
    P_DIVE_DEPTH,             /* 0..127: 1 .. 2 x the time */
    P_DIVE_TIME,              /* 0..127: the glide, 0.1 .. 3 s */
    P_ECHO_WIDTH,             /* 0 mono; 1..127: the right side 8 .. 16 ms later (P1.5) */
    P_ECHO_PP,                /* 1: ping-pong (P3.1): odd repeats left, even right (above 150 Hz; the lows centred) */
    P_ECHO_END,
    P_SPRING_RET = 0x0D0,     /* the spring's return (P1.2), 0..127 */
    P_SPRING_DECAY,           /* its decay: 0.5 .. 6 s */
    P_SPRING_TONE,            /* its damping: 2 .. 7 kHz */
    P_SPRING_KICK,            /* 0 -> 1: the tank struck (the crash) */
    P_SPRING_END,
    P_HPF_STEP = 0x0E0,       /* the Big Knob (P1.1): 0 off, 1..40 = HPF_HZ (the Altec's on 4, 8 .. 40) */
    P_HPF_ON,                 /* where it sits: HPF_ON_MIX (the master), HPF_ON_ECHO (the echo's return) */
    P_HPF_END,
    P_WET = 0x0E4,            /* 1: drop to wet (black 5, held): the dry desk out, the returns on (spec 4.1) */
    P_DNB,                    /* 1: drum and bass (black 11, held): skank, organ, lead and vox out, sends too */
    P_SPIN,                   /* 0 -> 1: the spin-back (P3.5): the last 1.6 s backwards, then from the top */
    P_DESK_END,
    P_XS_E2S = 0x0E8,         /* cross-sends (P1.3): the echo's return into the spring, 0..127 */
    P_XS_S2E,                 /* the spring's return into the echo, 0..127 (the two together capped: a loop) */
    P_XS_END,
    P_SIREN = 0x0EC,          /* the siren held (black 6 / 7, roadmap 5.1): 0 none, else (SIREN_A or B) + 1 | velocity << 4;
                               * a parameter, so the song's motion captures it (a momentary, as a throw) */
    P_SHOT,                   /* a one-shot fired (black 8-10): shot (0..7, + 8 backwards) | velocity << 4 | a toggle << 11
                               * (each press a new value, so two of the same are two) */
    P_VOX_END,
    P_TAILS = 0x0EE,          /* tails off (roadmap 5.2): every set fades the returns over 20 ms, then empties the echo's
                               * tape, the spring and the phaser. Not captured, not undone, not saved */
    P_TAILS_END,
    P_PH_ON = 0x0F0,          /* the phaser (P2.3): PH_OFF, PH_HATS, PH_SKANK, PH_ECHO, PH_MIX */
    P_PH_RATE,                /* 0..127: 0.1 .. 18 Hz */
    P_PH_DEPTH,               /* 0..127 */
    P_PH_FB,                  /* 0..127: feedback 0 .. 0.7 */
    P_PH_SHAPE,               /* 0 sine, 1 square */
    P_PH_MODE,                /* the dual Bi-Phase (P3.2): PHM_SINGLE, PHM_CASCADE (A then B), PHM_STEREO (B reversed) */
    P_PH_RATE2,               /* phaser B's own rate (cascade), 0..127: 0.1 .. 18 Hz */
    P_PH_END,
    P_DRIVE_ON = 0x0F8,       /* tape drive (P2.4): DRIVE_OFF, DRIVE_BASS, DRIVE_MIX */
    P_DRIVE_AMT,              /* 0..127: into the tape 0 .. +14 dB */
    P_DRIVE_MIX,              /* 0..127: the driven part, blended with the dry */
    P_DRIVE_END,
    P_SGATE_ON = 0x0FC,       /* the snare-keyed spring gate (P2.5): 0 off, 1 on */
    P_SGATE_HOLD,             /* 0..127: 10 .. 400 ms open after a snare */
    P_SGATE_REL,              /* 0..127: 5 .. 300 ms to close */
    P_SGATE_END,
};
enum { ECHO_DIV_16, ECHO_DIV_8T, ECHO_DIV_16D, ECHO_DIV_8, ECHO_DIV_4T, ECHO_DIV_8D, ECHO_DIV_4, ECHO_DIV_2T,
       ECHO_DIV_4D, ECHO_DIV_2, ECHO_DIV_N };
float echo_div_beats(int32_t d);
const char *echo_div_name(int32_t d);                 /* "1/8." */
float echo_t60(int32_t fb);                          /* seconds */
enum { HPF_ON_MIX, HPF_ON_ECHO, HPF_ON_N };
enum { PH_OFF, PH_HATS, PH_SKANK, PH_ECHO, PH_MIX, PH_N };
enum { PHM_SINGLE, PHM_CASCADE, PHM_STEREO, PHM_N };   /* STEREO on the echo's return only; elsewhere CASCADE */
enum { DRIVE_OFF, DRIVE_BASS, DRIVE_MIX, DRIVE_N };
#define HPF_STEPS 40                             /* the fine grid; the Altec's ten on every 4th (HPF_ALTEC) */
#define HPF_ALTEC 4
float hpf_hz(int32_t step);                          /* a step's cutoff, Hz (0: off) */
const char *hpf_name(int32_t step);                  /* "500 HZ", "1K", "OFF" */
int32_t hpf_detent(int32_t res);                     /* fine steps a detent moves at HPF_RES_* (4, 2, 1) */
uint32_t quant_beats(int32_t q);                     /* 1, 4, 8 .. 128 (4/4); QUANT_CYCLE: the UI's (cycle) */
uint32_t cycle_bars(int32_t c);                      /* 8, 16, 32, 64 */
uint32_t runway_bars(int32_t r);                     /* 1, 2, 4, 8 */
const char *quant_name(int32_t q);                   /* "BEAT", "1 BAR", "2 BARS" .. */

typedef struct {
    const char *name;         /* "level"; a channel's is shown as "ch4.level" (param_name) */
    int16_t min, max, def;
} pdesc_t;

const pdesc_t *param_desc(uint32_t id);              /* NULL: no parameter has this ID */
int32_t param_clamp(uint32_t id, int32_t v);
int32_t param_def(uint32_t id);                     /* the default (some differ by channel) */
void param_name(uint32_t id, char *buf, uint32_t n);   /* "ch4.mute", "dev.tones", "" if none */
int param_find(const char *name);                    /* the ID, or -1 */
