/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* The spring's half rate (app/engine/spring.c): no aliasing or imaging the ear would hear as grit. A sine into
 * spring_run: what comes back at 22,050 - f (where a fold or an image lands) must be far below what comes back at f
 * itself (or, for an input the spring should not pass, below a floor). Run by tests/run_tests.sh.
 * (2026-11-09: the one-pole decimator gave an 18 kHz input back at 4 kHz 21 dB louder than itself.) */
#include <math.h>
#include <stdio.h>
#include "../app/engine/spring.c"

static float buf[44100];

static double level(const float *x, int n, double f)   /* dB of the tone at f (a Hann-windowed DFT bin) */
{
    double re = 0, im = 0;
    for (int i = 0; i < n; i++) {
        double w = 0.5 - 0.5 * cos(2 * M_PI * i / n);
        re += w * x[i] * cos(2 * M_PI * f * i / 44100);
        im += w * x[i] * sin(2 * M_PI * f * i / 44100);
    }
    return 10 * log10(re * re + im * im + 1e-30);
}

static void run(double f)
{
    spring_cfg_t k = {0.5f, 0.9f, 12, 1};
    float in[64], l[64], r[64];
    spring_init();
    spring_config(&k);
    for (int b = 0; b < 2 * 44100 / 64; b++) {
        for (int i = 0; i < 64; i++)
            in[i] = 0.05f * sinf((float)(2 * M_PI * f * (b * 64 + i) / 44100));
        spring_run(in, l, r, 64);
        if (b * 64 >= 44100)                     /* the second second: settled */
            for (int i = 0; i < 64; i++)
                buf[b * 64 + i - 44100] = l[i];
    }
}

int main(void)
{
    static const double PASS[] = {1000, 3000, 6000}, STOP[] = {15000, 18000};
    int fails = 0;
    for (unsigned i = 0; i < sizeof PASS / sizeof PASS[0]; i++) {   /* images: 60 dB under the tone */
        run(PASS[i]);
        double s = level(buf, 44100, PASS[i]), im = level(buf, 44100, 22050 - PASS[i]);
        int ok = s - im > 60;
        printf("  %s spring image of %.0f Hz: %.1f dB under it\n", ok ? "ok" : "FAIL", PASS[i], s - im);
        fails += !ok;
    }
    run(1000);
    double ref = level(buf, 44100, 1000);
    for (unsigned i = 0; i < sizeof STOP / sizeof STOP[0]; i++) {   /* folds: 60 dB under a 1 kHz tone's return */
        run(STOP[i]);
        double al = level(buf, 44100, 22050 - STOP[i]);
        int ok = ref - al > 60;
        printf("  %s spring fold of %.0f Hz: %.1f dB under a 1 kHz return\n", ok ? "ok" : "FAIL", STOP[i], ref - al);
        fails += !ok;
    }
    return fails != 0;
}
