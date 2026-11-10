/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* The FX page (FX; docs/interface-spec.md 4): the effects' settings, four to a bank, KNOB 1-4; FX again for the next
 * bank. Echo, Spring, Cross-sends (P1.3), Tape (P1.4), Width (P1.5; the phaser joins it later: spec 4). Part of the unity build:
 * dub.c includes it. */

typedef struct {
    const char *title;
    uint16_t id[4];                              /* 0: none */
    const char *label[4];
} fx_bank_t;

static const fx_bank_t FX_BANK[] = {
    {"ECHO", {P_ECHO_TIME, P_ECHO_FB, P_ECHO_TONE, P_ECHO_HEAD2}, {"TIME", "FB", "TONE", "2T HEAD"}},
    {"SPRING", {P_SPRING_DECAY, P_SPRING_TONE, P_SPRING_RET, 0}, {"DECAY", "TONE", "RETURN", ""}},
    {"CROSS", {P_XS_E2S, P_XS_S2E, 0, 0}, {"ECHO>SPR", "SPR>ECHO", "", ""}},
    {"TAPE", {P_DIVE_DEPTH, P_DIVE_TIME, P_ECHO_WOW, P_ECHO_HEAD3}, {"DIVE", "DIVE TIME", "WOW", "3T HEAD"}},
    {"PHASER", {P_PH_ON, P_PH_RATE, P_PH_DEPTH, P_PH_FB}, {"ON", "RATE", "DEPTH", "FEEDBACK"}},
    {"DRIVE", {P_DRIVE_ON, P_DRIVE_AMT, P_DRIVE_MIX, 0}, {"ON", "DRIVE", "BLEND", ""}},
    {"GATE", {P_SGATE_ON, P_SGATE_HOLD, P_SGATE_REL, 0}, {"SPRING GATE", "HOLD", "RELEASE", ""}},
    {"WIDTH", {P_ECHO_WIDTH, P_PH_SHAPE, P_ECHO_PP, 0}, {"ECHO WIDTH", "PH SHAPE", "PING-PONG", ""}},
    {"BI-PHASE", {P_PH_MODE, P_PH_RATE2, 0, 0}, {"MODE", "RATE 2", "", ""}},
    {"ECHO TYPE", {P_ECHO_TYPE, 0, 0, 0}, {"TYPE", "", "", ""}},   /* 7.9 */
};
#define FX_NBANKS (sizeof FX_BANK / sizeof FX_BANK[0])
static uint32_t fx_bank, fx_sig;

static void fx_value(uint16_t id, char *t)       /* a value as the page shows it */
{
    int32_t v = ctl_get(id);
    char n[12];
    t[0] = 0;
    if (!id)
        return;
    if (id == P_ECHO_TIME) {
        text_cat(t, echo_div_name(v));
    } else if (id == P_ECHO_FB) {
        t60_text(t);
    } else if (id == P_PH_ON) {
        static const char *const ON[PH_N] = {"OFF", "HATS", "SKANK", "ECHO", "MIX"};
        text_cat(t, ON[v]);
    } else if (id == P_SGATE_ON) {
        text_cat(t, v ? "SNARE" : "OFF");
    } else if (id == P_SGATE_HOLD || id == P_SGATE_REL) {   /* ms */
        int_text(id == P_SGATE_HOLD ? 10 + v * 390 / 127 : 5 + v * 295 / 127, n);
        text_cat(t, n);
        text_cat(t, "MS");
    } else if (id == P_DRIVE_ON) {
        static const char *const ON[DRIVE_N] = {"OFF", "BASS", "MIX"};
        text_cat(t, ON[v]);
    } else if (id == P_DRIVE_AMT) {              /* +0 .. +14 dB */
        text_cat(t, "+");
        int_text(v * 14 / 127, n);
        text_cat(t, n);
        text_cat(t, "DB");
    } else if (id == P_PH_MODE) {
        static const char *const M[PHM_N] = {"SINGLE", "CASCADE", "STEREO"};
        text_cat(t, M[v]);
    } else if (id == P_ECHO_PP) {
        text_cat(t, v ? "ON" : "OFF");
    } else if (id == P_ECHO_TYPE) {              /* 7.9 */
        text_cat(t, v == ECHO_BBD ? "BBD" : "TAPE");
    } else if (id == P_PH_SHAPE) {
        text_cat(t, v ? "SQUARE" : "SINE");
    } else if (id == P_PH_RATE || id == P_PH_RATE2) {   /* 0.1 .. 18 Hz */
        int32_t d = (int32_t)(fm_exp2f((float)v * (7.49f / 127.0f)) + 0.5f);   /* x 0.1 Hz */
        int_text(d / 10, n);
        text_cat(t, n);
        text_cat(t, ".");
        int_text(d % 10, n);
        text_cat(t, n);
        text_cat(t, "HZ");
    } else if (id == P_ECHO_WIDTH) {             /* OFF, 8 .. 16 ms */
        if (!v)
            text_cat(t, "MONO");
        else {
            int_text(8 + (v - 1) * 8 / 126, n);
            text_cat(t, n);
            text_cat(t, " MS");
        }
    } else if (id == P_DIVE_DEPTH) {             /* x 1.00 .. 2.00 */
        int32_t d = 100 + v * 100 / 127;
        text_cat(t, "x");
        int_text(d / 100, n);
        text_cat(t, n);
        text_cat(t, ".");
        n[0] = (char)('0' + d % 100 / 10);
        n[1] = (char)('0' + d % 10);
        n[2] = 0;
        text_cat(t, n);
    } else if (id == P_DIVE_TIME) {              /* 0.1 .. 3 s */
        int32_t d = 1 + v * 29 / 127;
        int_text(d / 10, n);
        text_cat(t, n);
        text_cat(t, ".");
        int_text(d % 10, n);
        text_cat(t, n);
        text_cat(t, "s");
    } else if (id == P_SPRING_DECAY) {           /* 0.5 .. 6 s */
        int32_t d = 5 + v * 55 / 127;
        int_text(d / 10, n);
        text_cat(t, n);
        text_cat(t, ".");
        int_text(d % 10, n);
        text_cat(t, n);
        text_cat(t, "s");
    } else
        int_text(v, t);
}

static void fx_knob(uint32_t i, int32_t e)
{
    const fx_bank_t *b = &FX_BANK[fx_bank];
    char t[16];
    if (!b->id[i])
        return;
    const pdesc_t *d = param_desc(b->id[i]);
    int one = b->id[i] == P_ECHO_TIME || b->id[i] == P_ECHO_FB || (d && d->max - d->min < 16);   /* (a choice: a step) */
    ctl_set(b->id[i], ctl_get(b->id[i]) + (one ? e : 2 * e));
    set_dirty = 1;
    fx_value(b->id[i], t);
    app_say(roots_name(b->label[i]), t);
}

static void fx_next(int same_page)
{
    fx_bank = same_page ? (fx_bank + 1u) % FX_NBANKS : 0u;
    fx_sig = 0;
    app_say("FX", roots_name(FX_BANK[fx_bank].title));
}

static uint32_t fx_sigf(void)
{
    uint32_t h = fx_bank, i;
    for (i = 0; i < 4u; i++)
        h = h * 131u + (uint32_t)ctl_get(FX_BANK[fx_bank].id[i]);
    return h;
}

static void fx_draw(int all)
{
    const fx_bank_t *b = &FX_BANK[fx_bank];
    uint32_t i, sig = fx_sigf();
    char t[16];
    if (!all && sig == fx_sig)
        return;
    fx_sig = sig;
    CV_BANDS(PAGE_H, C_BLACK) {                 /* (in two halves: the canvas holds 76 rows) */
        {                                            /* the banks, this one lit, in view (the tabs scroll: 5.11) */
            uint32_t first = 0;
            int32_t x, w;
            for (;;) {                               /* (the first tab from which this one fits on the screen) */
                for (x = 6, i = first; i <= fx_bank; i++)
                    x += text_w(&FONT_XS, FX_BANK[i].title) + 12;
                if (x - 12 <= 234 || first == fx_bank)
                    break;
                first++;
            }
            if (first)
                cv_text(0, 2, &FONT_XS, "<", C_DIM);
            for (x = 6, i = first; i < FX_NBANKS && x < 234; i++) {
                w = text_w(&FONT_XS, FX_BANK[i].title);
                if (x + w > 234) {
                    cv_text(234, 2, &FONT_XS, ">", C_DIM);
                    break;
                }
                cv_text(x, 2, &FONT_XS, FX_BANK[i].title, i == fx_bank ? C_HI : C_DIM);
                x += w + 12;
            }
        }
        for (i = 0; i < 4u; i++) {
            int32_t y = 26 + (int32_t)i * 28;
            if (!b->id[i])
                continue;
            cv_text(10, y + 4, &FONT_XS, b->label[i], C_GRAY);
            fx_value(b->id[i], t);
            cv_text(110, y, &FONT_B, t, C_WHITE);
            cv_text(220, y + 4, &FONT_T, i == 0u ? "K1" : i == 1u ? "K2" : i == 2u ? "K3" : "K4", C_DIM);
        }
        CV_BAND_BLIT(STRIP_Y);
    }
    lcd_fill(0, STRIP_Y + PAGE_H, 240, FOOT_Y - STRIP_Y - PAGE_H, C_BLACK);
    cv_begin(240, FOOT_H, C_BLACK);
    cv_rect(0, 0, 240, 1, C_LINE);
    for (i = 0; i < 4u; i++) {
        int32_t x = 6 + (int32_t)i * 59;
        cv_text(x, 4, &FONT_XS, b->label[i], kc_or(KC_KNOB(i), C_GRAY));
        fx_value(b->id[i], t);
        cv_text(x, 18, &FONT_B, t, C_HI);   /* (a value: the page font, never the smallest; 5.11) */
    }
    cv_blit(0, FOOT_Y);
}
