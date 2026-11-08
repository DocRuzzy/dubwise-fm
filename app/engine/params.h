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
enum { CHF_LEVEL, CHF_MUTE, CHF_SOLO, CHF_ECHO_SEND, CHF_SPRING_SEND, CHF_N };
/* voice fields (append only, at most VOICE_STRIDE) */
enum { VF_WAVE, VF_N };

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
    P_DEV_TONES = 0x03F,      /* development: a test tone on every channel (host tests of the mixer) */
};

typedef struct {
    const char *name;         /* "level"; a channel's is shown as "ch4.level" (param_name) */
    int16_t min, max, def;
} pdesc_t;

const pdesc_t *param_desc(uint32_t id);              /* NULL: no parameter has this ID */
int32_t param_clamp(uint32_t id, int32_t v);
void param_name(uint32_t id, char *buf, uint32_t n);   /* "ch4.mute", "dev.tones", "" if none */
int param_find(const char *name);                    /* the ID, or -1 */
