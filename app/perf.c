/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* The PERF page (roadmap 1.9; design principles 5): what each part of the engine costs, as a share of the device's
 * block, the load against the guard's lines, and the tier. Opened from SETUP (PERFORMANCE). On the device the
 * figures are the device's; in the simulator they are the host's times x 80 (an estimate, marked so). Part of the
 * unity build: dub.c includes it. */

static uint32_t perf_t;
static uint32_t perf_pk[8], perf_pki;               /* 9.11: the costliest block in each of the last eight draws (2 s) */

#ifdef PLAT_HOST
#define PERF_SCALE 80u                           /* the host runs ~80x faster than the device */
#define PERF_NOTE "HOST x80 ESTIMATE"
#else
#define PERF_SCALE 1u
#define PERF_NOTE "THIS FM-1"
#endif

static uint32_t part_pct10(uint32_t part)        /* tenths of a % of the block */
{
    return eng_prof(part).avg / 16u * PERF_SCALE * 1000u / ENG_BLOCK_TICKS;
}

static void perf_draw(int all)
{
    static const uint8_t SHOW[] = {PART_CH0, PART_CH0 + 1, PART_CH0 + 2, PART_CH0 + 3, PART_CH0 + 4, PART_CH0 + 5,
                                   PART_MIX, PART_ECHO, PART_SPRING, PART_OUT};
    static const char *const NAME[] = {"KICK", "RIM", "HATS", "BASS", "SKANK", "ORGAN", "MIX", "ECHO", "SPRING", "OUT"};
    uint32_t i, tot = 0;
    char t[24], n[8];
    if (!all && plat_ms() - perf_t < 250u)       /* 4 times a second */
        return;
    perf_t = plat_ms();
    CV_BANDS(PAGE_H, C_BLACK) {                 /* (in two halves: the canvas holds 76 rows) */
        cv_text(6, 2, &FONT_XS, "PERF", C_GRAY);
        cv_text(234 - text_w(&FONT_T, PERF_NOTE), 4, &FONT_T, PERF_NOTE, C_DIM);
        for (i = 0; i < NPARTS; i++)
            tot += part_pct10(i);
        for (i = 0; i < sizeof SHOW; i++) {          /* a bar a part, 25 % full width */
            uint32_t p = part_pct10(SHOW[i]);
            int32_t y = 18 + (int32_t)i * 10, w = (int32_t)(p * 150u / 250u);
            cv_text(6, y - 2, &FONT_T, NAME[i], C_GRAY);
            cv_rect(56, y, w > 150 ? 150 : w, 7, C_HI);
            int_text((int32_t)(p / 10u), t);
            text_cat(t, ".");
            int_text((int32_t)(p % 10u), n);
            text_cat(t, n);
            cv_text(212, y - 2, &FONT_T, t, C_AMB);
        }
        {                                            /* 9.11: the peak block over 2 s, the dropouts, the clock */
            uint32_t k, pk = 0, khz = plat_cpu_khz();
            perf_pk[perf_pki++ & 7u] = eng_block_peak_take() * PERF_SCALE * 1000u / ENG_BLOCK_TICKS;
            for (k = 0; k < 8u; k++)
                pk = perf_pk[k] > pk ? perf_pk[k] : pk;
            t[0] = 0;
            text_cat(t, "PEAK ");
            int_text((int32_t)(pk / 10u), n);
            text_cat(t, n);
            text_cat(t, "%");
            cv_text(6, 134, &FONT_T, t, pk > 1000u ? C_RED : pk > 750u ? C_AMB : C_GRAY);
            t[0] = 0;
            text_cat(t, "DROPOUTS ");
            int_text((int32_t)plat_xruns(), n);
            text_cat(t, n);
            cv_text(80, 134, &FONT_T, t, plat_xruns() ? C_RED : C_GRAY);
            t[0] = 0;
            if (khz) {
                int_text((int32_t)(khz / 1000u), n);
                text_cat(t, n);
                text_cat(t, " MHZ");
            } else
                text_cat(t, "CLOCK ?");
            cv_text(170, 134, &FONT_T, t, khz && khz != 360000u ? C_AMB : C_GRAY);
        }
        {                                            /* the total, the guard's lines, the tier */
            int32_t y = 119, w = (int32_t)(tot * 220u / 1000u);
            cv_rect(6, y, 220, 10, C_LINE);
            cv_rect(6, y, w > 220 ? 220 : w, 10, tot > 750u ? C_RED : tot > 450u ? C_AMB : C_HI);
            cv_rect(6 + 220 * 45 / 100, y - 2, 1, 14, C_GRAY);
            cv_rect(6 + 220 * 75 / 100, y - 2, 1, 14, C_RED);
        }
        CV_BAND_BLIT(STRIP_Y);
    }
    lcd_fill(0, STRIP_Y + PAGE_H, 240, FOOT_Y - STRIP_Y - PAGE_H, C_BLACK);
    cv_begin(240, FOOT_H, C_BLACK);
    cv_rect(0, 0, 240, 1, C_LINE);
    cv_text(6, 4, &FONT_XS, "TOTAL", C_GRAY);
    int_text((int32_t)(tot / 10u), t);
    text_cat(t, "%");
    cv_text(6, 18, &FONT_B, t, C_HI);
    cv_text(80, 4, &FONT_XS, ctl_get(P_TIER_LOCK) ? "TIER (LOCKED)" : "TIER (AUTO)", C_GRAY);
    cv_text(80, 18, &FONT_B, tier_name(eng_tier()), eng_tier() ? C_AMB : C_HI);
    {                                            /* 9.17: the stacks' least room (bytes never used), M the main loop's,
                                                  * I the interrupts' (the audio's): since this start (fm1.toml
                                                  * crash_history), or the least ever where debug_stats keeps it; "?" not
                                                  * measured (the simulator). Amber under 512 B, red under 256 */
        static const char *const SK[] = {"M ", "I "};
        int k;
        cv_text(180, 4, &FONT_XS, "STACK", C_GRAY);
        for (k = 0; k < 2; k++) {
            int32_t v = plat_stack_least_ever(k == 0 ? PLAT_STK_MAIN : PLAT_STK_IRQ);
            if (v < 0)
                v = plat_stack_left(k == 0 ? PLAT_STK_MAIN : PLAT_STK_IRQ);
            t[0] = 0;
            text_cat(t, SK[k]);
            if (v >= 0) {
                int_text(v, n);
                text_cat(t, n);
            } else
                text_cat(t, "?");
            cv_text(180, 18 + 9 * k, &FONT_T, t, v < 0 ? C_DIM : v < 256 ? C_RED : v < 512 ? C_AMB : C_GRAY);
        }
    }
    cv_blit(0, FOOT_Y);
}
