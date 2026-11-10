/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* The 808 drum models (roadmap 7.6): KICK 808, RIM 808 (the snare), HAT 808, after Mutable Instruments Plaits'
 * analog drums (Emilie Gillet, MIT; credits in d808.c). 44.1 kHz, floats. Each model's knobs are 0..1 */
#pragma once
#include <stdint.h>

typedef struct {
    float f0, tone, decay, attack_fm, self_fm;   /* f0 in Hz; the rest 0..1 */
} d808_kick_cfg_t;
typedef struct {
    float f0, tone, decay, snappy;
} d808_snare_cfg_t;
typedef struct {
    float f0, tone, noisiness, k;                /* k: the envelope's decay a sample (the closed or the open hat's) */
} d808_hat_cfg_t;

void d808_init(void);
void d808_kick_trig(const d808_kick_cfg_t *c, float accent, float tmul, float dinv);
int d808_kick_render(float *o, uint32_t n, float gain, int add);   /* x gain, added or written; 1: it sounded */
void d808_snare_trig(const d808_snare_cfg_t *c, float accent, float tmul, float dinv);
int d808_snare_render(float *o, uint32_t n, float gain, int add);
void d808_hat_trig(const d808_hat_cfg_t *c, float accent);
int d808_hat_render(float *o, uint32_t n, float gain, int add);
