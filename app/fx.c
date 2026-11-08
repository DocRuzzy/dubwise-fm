/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* The FX page (FX; docs/interface-spec.md 4): the effects' settings, four to a bank, KNOB 1-4; FX again for the next
 * bank. Echo, Spring, Cross-sends (P1.3; phaser / width join later: spec 4). Part of the unity build:
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
    {"TAPE", {P_DIVE_DEPTH, P_DIVE_TIME, 0, 0}, {"DIVE", "DIVE TIME", "", ""}},
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
    ctl_set(b->id[i], ctl_get(b->id[i]) + (b->id[i] == P_ECHO_TIME || b->id[i] == P_ECHO_FB ? e : 2 * e));
    set_dirty = 1;
    fx_value(b->id[i], t);
    app_say(b->label[i], t);
}

static void fx_next(int same_page)
{
    fx_bank = same_page ? (fx_bank + 1u) % FX_NBANKS : 0u;
    fx_sig = 0;
    app_say("FX", FX_BANK[fx_bank].title);
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
    cv_begin(240, PAGE_H, C_BLACK);
    for (i = 0; i < FX_NBANKS; i++)              /* the banks, this one lit */
        cv_text(6 + (int32_t)i * 70, 2, &FONT_XS, FX_BANK[i].title, i == fx_bank ? C_HI : C_DIM);
    for (i = 0; i < 4u; i++) {
        int32_t y = 26 + (int32_t)i * 28;
        if (!b->id[i])
            continue;
        cv_text(10, y + 4, &FONT_XS, b->label[i], C_GRAY);
        fx_value(b->id[i], t);
        cv_text(110, y, &FONT_B, t, C_WHITE);
        cv_text(220, y + 4, &FONT_T, i == 0u ? "K1" : i == 1u ? "K2" : i == 2u ? "K3" : "K4", C_DIM);
    }
    cv_blit(0, STRIP_Y);
    lcd_fill(0, STRIP_Y + PAGE_H, 240, FOOT_Y - STRIP_Y - PAGE_H, C_BLACK);
    cv_begin(240, FOOT_H, C_BLACK);
    cv_rect(0, 0, 240, 1, C_LINE);
    for (i = 0; i < 4u; i++) {
        int32_t x = 6 + (int32_t)i * 59;
        cv_text(x, 4, &FONT_XS, b->label[i], C_GRAY);
        fx_value(b->id[i], t);
        cv_text(x, 18, &FONT_XS, t, C_HI);
    }
    cv_blit(0, FOOT_Y);
}
