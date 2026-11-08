/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* The SETUP page (GLO; docs/interface-spec.md 4, 5.4), first version: the cycle and its runway, the cue lead,
 * reduced motion and the test tones. KNOB 1 (or SELECT) moves through the list, KNOB 2 (or PRESET) changes the
 * item. Part of the unity build: dub.c includes it. Saved with SAVE (but the test tones). */

enum { SU_CYCLE, SU_RUNWAY, SU_KEYS, SU_NUM, SU_TICK, SU_REDUCED, SU_LEAD, SU_LOCK_MIDI, SU_TONES, SU_N };
static const char *const SU_NAME[SU_N] = {"CYCLE", "RUNWAY", "RUNWAY ON KEYS", "COUNTDOWN NUMERALS", "CUE TICK",
                                          "REDUCED MOTION", "CUE LEAD", "LOCK MIDI IN", "TEST TONES"};
static const uint16_t SU_ID[SU_N] = {P_CYCLE, P_RUNWAY, P_RUNWAY_KEYS, P_RUNWAY_NUM, P_TICK, P_REDUCED, P_LEAD,
                                     P_LOCK_MIDI, P_DEV_TONES};
static int32_t su_cur;
static uint32_t su_sig;

static void su_value(uint32_t i, char *t)
{
    int32_t v = ctl_get(SU_ID[i]);
    char n[12];
    t[0] = 0;
    if (i == SU_CYCLE || i == SU_RUNWAY) {
        int_text((int32_t)(i == SU_CYCLE ? cycle_bars(v) : runway_bars(v)), n);
        text_cat(t, n);
        text_cat(t, v == 0 && i == SU_RUNWAY ? " BAR" : " BARS");
    } else if (i == SU_LEAD) {
        int_text(v, n);
        text_cat(t, n);
        text_cat(t, " MS");
    } else
        text_cat(t, v ? "ON" : "OFF");
}

static void setup_say(void)
{
    char t[16];
    su_value((uint32_t)su_cur, t);
    app_say(SU_NAME[su_cur], t);
}

static void setup_move(int32_t e)
{
    su_cur = (su_cur + e % SU_N + SU_N) % SU_N;
    setup_say();
}

static void setup_change(int32_t e)
{
    uint32_t id = SU_ID[su_cur];
    ctl_set(id, ctl_get(id) + e);
    if (su_cur != SU_TONES)
        set_dirty = 1;
    dub.dirty_hdr = 1;
    setup_say();
}

static uint32_t setup_sig(void)
{
    uint32_t h = (uint32_t)su_cur, i;
    for (i = 0; i < SU_N; i++)
        h = h * 31u + (uint32_t)ctl_get(SU_ID[i]);
    return h;
}

static void setup_draw(int all)          /* the list in the strips' and centre's place; the footer the knobs */
{
    uint32_t i, sig = setup_sig();
    char t[16];
    if (!all && sig == su_sig)
        return;
    su_sig = sig;
    cv_begin(240, PAGE_H, C_BLACK);      /* (a canvas holds 144 rows: the rest of the area below, cleared) */
    cv_text(6, 2, &FONT_XS, "SETUP", C_GRAY);
    for (i = 0; i < SU_N; i++) {
        int32_t y = 17 + (int32_t)i * 14, cur = (int32_t)i == su_cur;
        if (cur)
            cv_rect(0, y - 1, 240, 14, C_LINE);
        cv_text(10, y, &FONT_T, SU_NAME[i], cur ? C_WHITE : C_GRAY);
        su_value(i, t);
        cv_text(232 - text_w(&FONT_T, t), y, &FONT_T, t, cur ? C_HI : C_AMB);
    }
    cv_blit(0, STRIP_Y);
    lcd_fill(0, STRIP_Y + PAGE_H, 240, FOOT_Y - STRIP_Y - PAGE_H, C_BLACK);
    cv_begin(240, FOOT_H, C_BLACK);
    cv_rect(0, 0, 240, 1, C_LINE);
    cv_text(6, 4, &FONT_XS, "ITEM", C_GRAY);
    cv_text(6, 18, &FONT_XS, "K1 / SELECT", C_HI);
    cv_text(124, 4, &FONT_XS, "VALUE", C_GRAY);
    cv_text(124, 18, &FONT_XS, "K2 / PRESET", C_HI);
    cv_blit(0, FOOT_Y);
}

#ifdef PLAT_HOST
static int setup_expect(const char *what, int *got)
{
    if (!strcmp(what, "setup_item"))
        *got = su_cur;
    else
        return 0;
    return 1;
}
#endif
