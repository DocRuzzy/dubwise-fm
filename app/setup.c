/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* The SETUP page (GLO; docs/interface-spec.md 4, 5.4), first version: the cycle and its runway, the cue lead,
 * reduced motion and the test tones. KNOB 1 (or SELECT) moves through the list, KNOB 2 (or PRESET) changes the
 * item. Part of the unity build: dub.c includes it. Saved with SAVE (but the test tones). */

enum { SU_CYCLE, SU_RUNWAY, SU_KEYS, SU_NUM, SU_TICK, SU_REDUCED, SU_SAVER, SU_LEAD, SU_LOCK_MIDI, SU_HPF_ON, SU_HPF_RES, SU_THROW_VEL, SU_CLOCK,
       SU_TIER, SU_PERF, SU_AUTOSAVE, SU_TONES, SU_SLOT_SAVE, SU_SLOT_LOAD, SU_DEMO, SU_STEPPERS, SU_N };
static const char *const SU_NAME[SU_N] = {"CYCLE", "RUNWAY", "RUNWAY ON KEYS", "COUNTDOWN NUMERALS", "CUE TICK",
                                          "REDUCED MOTION", "SCREEN SAVER", "CUE LEAD", "LOCK MIDI IN", "BIG KNOB ON", "BIG KNOB STEPS",
                                          "THROW VELOCITY", "MIDI CLOCK", "GUARD TIER",
                                          "PERFORMANCE", "AUTOSAVE", "TEST TONES", "SAVE TO SLOT", "LOAD SLOT", "DEMO SONG", "UK STEPPERS"};
static const uint16_t SU_ID[SU_N] = {P_CYCLE, P_RUNWAY, P_RUNWAY_KEYS, P_RUNWAY_NUM, P_TICK, P_REDUCED, P_SAVER, P_LEAD,
                                     P_LOCK_MIDI, P_HPF_ON, P_HPF_RES, P_THROW_VEL, P_CLOCK, P_TIER_LOCK, 0, P_AUTOSAVE, P_DEV_TONES, 0, 0, 0, 0};
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
    } else if (i == SU_SLOT_SAVE || i == SU_SLOT_LOAD) {
        slot_value(t, i == SU_SLOT_LOAD);
    } else if (i == SU_CLOCK) {
        static const char *const C[CLOCK_N] = {"OFF", "IN", "OUT"};
        text_cat(t, C[v >= 0 && v < CLOCK_N ? v : 0]);
    } else if (i == SU_HPF_RES) {
        static const char *const R[HPF_RES_N] = {"10 ALTEC", "20", "40"};
        text_cat(t, R[v >= 0 && v < HPF_RES_N ? v : 0]);
    } else if (i == SU_SAVER) {
        static const char *const S[SAVER_N] = {"OFF", "1 MIN", "5 MIN", "15 MIN"};
        text_cat(t, S[v >= 0 && v < SAVER_N ? v : 0]);
    } else if (i == SU_TIER) {
        text_cat(t, v ? tier_name(v - 1) : "AUTO");
    } else if (i == SU_PERF) {
        text_cat(t, "K2: VIEW");
    } else if (i == SU_DEMO || i == SU_STEPPERS) {
        text_cat(t, "K2: LOAD");
    } else if (i == SU_HPF_ON) {
        text_cat(t, v == HPF_ON_ECHO ? "ECHO RETURN" : "MIX");
    } else if (i == SU_LEAD) {
        int_text(v, n);
        text_cat(t, n);
        text_cat(t, " MS");
    } else
        text_cat(t, v ? "ON" : "OFF");
}

static int setup_slot_row(void)          /* the cursor on a slot row: 1 load, 0 save; -1 neither */
{
    return su_cur == SU_SLOT_LOAD ? 1 : su_cur == SU_SLOT_SAVE ? 0 : -1;
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
    if (su_cur == SU_DEMO) {                     /* the demo song (demo.c) */
        demo_load();
        return;
    }
    if (su_cur == SU_SLOT_SAVE || su_cur == SU_SLOT_LOAD) {   /* KNOB 2 picks the slot; SAVE does it */
        slot_pick(e);
        setup_say();
        return;
    }
    if (su_cur == SU_STEPPERS) {                   /* the UK steppers desk (P3.4; demo.c) */
        steppers_load();
        return;
    }
    if (su_cur == SU_PERF) {                     /* the PERF page */
        dub.page = PAGE_PERF;
        app_say("PERF", tier_name(eng_tier()));
        return;
    }
    ctl_set(id, ctl_get(id) + e);
    if (su_cur != SU_TONES)
        set_dirty = 1;
    dub.dirty_hdr = 1;
    setup_say();
}

#define SU_ROWS 12u                              /* the rows the page shows (the list scrolls) */
static uint32_t su_top(void)                     /* the first row shown: the cursor kept in view, mid-page */
{
    int32_t t = su_cur - (int32_t)SU_ROWS / 2;
    if (t > (int32_t)SU_N - (int32_t)SU_ROWS)
        t = (int32_t)SU_N - (int32_t)SU_ROWS;
    return t < 0 ? 0u : (uint32_t)t;
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
    {                                    /* where in the list: the rows scroll with the cursor */
        char a[8], b[8];
        int_text(su_cur + 1, a);
        int_text(SU_N, b);
        text_cat(a, "/");
        text_cat(a, b);
        cv_text(234 - text_w(&FONT_XS, a), 2, &FONT_XS, a, C_DIM);
    }
    for (i = su_top(); i < SU_N && i < su_top() + SU_ROWS; i++) {
        int32_t y = 16 + (int32_t)(i - su_top()) * 10, cur = (int32_t)i == su_cur;
        if (cur)
            cv_rect(0, y - 1, 240, 10, C_LINE);
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
