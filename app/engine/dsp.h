/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* Small DSP helpers shared by the engine's units. */
#pragma once

/* A decaying state would sink into denormals, which some FPUs (the host's) take ~30x as long over (measured on the
 * echo: 61 us a block against 2). Adding and taking away a tiny constant rounds anything that small to exactly 0.
 * Needs -ffp-contract=off and no -ffast-math (the build's flags); volatile keeps the compiler from folding it. */
static inline float undenorm(float x)
{
    volatile float k = 1e-18f;
    return (x + k) - k;
}
