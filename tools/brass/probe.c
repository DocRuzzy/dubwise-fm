/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors
 * Roadmap 7.7: pitch across lip tension, bore mode and breath; cc -O2 -o probe probe.c -lm (lipsim_run.h: lipsim.c up to
 * its main) */
#include "lipsim_run.h"
int main(void)
{
    float breaths[6] = {0.2f, 0.35f, 0.5f, 0.65f, 0.8f, 0.95f};
    for (int note = 55; note <= 76; note += 3) {
        float ft = 440.0f * powf(2.0f, (note - 69) / 12.0f);
        printf("note %d (%.0f Hz):\n", note, ft);
        for (float tens = 0.6f; tens <= 2.01f; tens += 0.1f) {
            printf("  t %.1f:", tens);
            for (int mode = 1; mode <= 2; mode++) {
                float dmul = mode == 1 ? 0.5f * (2 * SR / ft) / (2 * SR / ft + 3) : 1.0f;
                printf("  m%d", mode);
                for (int k = 0; k < 6; k++) {
                    res_t r = run(ft, tens, breaths[k], dmul, 30, 16000);
                    if (r.speaks) printf(" %+5.0f", 1200 * log2f(r.f / ft)); else printf("     -");
                }
            }
            printf("\n");
        }
    }
    return 0;
}
