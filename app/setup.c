/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* The SETUP page (GLO; docs/interface-spec.md 4, 5.4), first version: the cycle and its runway, the cue lead,
 * reduced motion and the test tones. KNOB 1 (or SELECT) moves through the list, KNOB 2 (or PRESET) changes the
 * item. Part of the unity build: dub.c includes it. Saved with SAVE (but the test tones). */

enum { SU_CYCLE, SU_RUNWAY, SU_KEYS, SU_NUM, SU_TICK, SU_REDUCED, SU_SAVER, SU_LEAD, SU_LOCK_MIDI, SU_HPF_ON, SU_HPF_RES, SU_THROW_VEL, SU_CLOCK,
       SU_TIER, SU_PERF, SU_AUTOSAVE, SU_REC_QUANT, SU_REC_MODE, SU_REC_OFFSET, SU_REC_CLICK, SU_STEMS, SU_LEARN, SU_USB_PORT, SU_DICE_LINE, SU_HELP, SU_TONES, SU_SHOTS, SU_OUTPUT, SU_DESK, SU_KC0, SU_KC6 = SU_KC0 + KC_N - 1, SU_BKRES, SU_SLOT_SAVE, SU_SLOT_LOAD, SU_KIT, SU_DEMO, SU_STEPPERS, SU_NEW, SU_N };
static const char *const SU_NAME[SU_N] = {"CYCLE", "RUNWAY", "RUNWAY ON KEYS", "COUNTDOWN NUMERALS", "CUE TICK",
                                          "REDUCED MOTION", "SCREEN SAVER", "CUE LEAD", "LOCK MIDI IN", "BIG KNOB ON", "BIG KNOB STEPS",
                                          "THROW VELOCITY", "MIDI CLOCK", "GUARD TIER",
                                          "PERFORMANCE", "AUTOSAVE", "REC QUANTISE", "REC MODE", "REC OFFSET", "CLICK", "USB STEMS", "MIDI LEARN", "USB PORT",
                                          "DICE ON LINE", "HELP", "TEST TONES", "ONE SHOTS", "OUTPUT", "DESK", "COLOUR BIG KNOB", "COLOUR ALGORITHM", "COLOUR PRESET",
                                          "COLOUR KNOB 1", "COLOUR KNOB 2", "COLOUR KNOB 3", "COLOUR KNOB 4", "BIG KNOB RESONANCE",
                                          "SAVE TO SLOT", "LOAD SLOT", "LOAD KIT", "DEMO SONG", "UK STEPPERS",
                                          "NEW PROJECT"};
static const uint16_t SU_ID[SU_N] = {P_CYCLE, P_RUNWAY, P_RUNWAY_KEYS, P_RUNWAY_NUM, P_TICK, P_REDUCED, P_SAVER, P_LEAD,
                                     P_LOCK_MIDI, P_HPF_ON, P_HPF_RES, P_THROW_VEL, P_CLOCK, P_TIER_LOCK, 0, P_AUTOSAVE, P_REC_QUANT, P_REC_MODE,
                                     P_REC_OFFSET, P_REC_CLICK, P_USB_STEMS, 0, P_USB_PORT, P_DICE_LINE, 0, P_DEV_TONES, 0, P_SPEAKER, P_DESK, 0, 0, 0, 0, 0, 0, 0, P_BK_RES, 0, 0, 0, 0, 0, 0};
static int32_t su_cur;
static uint8_t sh_user;                          /* (shots.c: 5.14) */
static int sh_factory(void);
static uint8_t slot_fresh;                       /* (slots.c: LOAD SLOT AS NEW, v0.2) */
static void help_index_step(int32_t e);          /* (help.c) */
static uint32_t su_sig;

static void su_value(uint32_t i, char *t)        /* (t: 24 bytes) */
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
    } else if (i == SU_OUTPUT) {                 /* 6.2: BASS+ for the FM-1's own speaker */
        text_cat(t, v == OUT_SPEAKER ? "SPEAKER (BASS+)" : "LINE");
    } else if (i == SU_BKRES) {                  /* 8.10: OFF (the Altec's), LOW, HIGH */
        static const char *const R[BKRES_N] = {"OFF", "LOW", "HIGH"};
        text_cat(t, R[v < BKRES_N ? v : 0]);
    } else if (i >= SU_KC0 && i <= SU_KC6) {     /* 8.8: a knob's cap colour */
        text_cat(t, KCOL_NAME[kcol(i - SU_KC0)]);
    } else if (i == SU_DESK) {                   /* 7.4: the desk's colour (Studio only) */
        text_cat(t, v == DESK_MCI ? (eng_tier() ? "MCI (STUDIO ONLY)" : "MCI") : "CLEAN");
    } else if (i == SU_SHOTS) {                  /* your own one-shots (5.14) */
        text_cat(t, sh_user ? "YOURS (K2: FACTORY)" : "FACTORY");
    } else if (i == SU_HELP) {                   /* every help card (5.11) */
        text_cat(t, "K2: READ THEM");
    } else if (i == SU_KIT) {                    /* factory kits (5.5; kits.c) */
        kit_value(t);
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
    } else if (i == SU_NEW) {
        text_cat(t, "K2: CLEAR ALL");
    } else if (i == SU_HPF_ON) {
        text_cat(t, v == HPF_ON_ECHO ? "ECHO RETURN" : "MIX");
    } else if (i == SU_REC_QUANT) {              /* live recording (4.8) */
        static const char *const Q[REC_QUANT_N] = {"FEEL", "HALF", "GRID"};
        text_cat(t, Q[v >= 0 && v < REC_QUANT_N ? v : 0]);
    } else if (i == SU_LEARN) {                  /* MIDI learn (midilearn.c) */
        int_text((int32_t)ml_count(), n);
        text_cat(t, n);
        text_cat(t, " LEARNED");
    } else if (i == SU_USB_PORT) {
        text_cat(t, v == USBPORT_KEYBOARD ? "KEYBOARD" : "COMPUTER");
    } else if (i == SU_STEMS) {                  /* the USB recording's channels 3-10 */
        static const char *const M[STEMS_N] = {"CHANNELS", "BUSES", "OFF"};
        text_cat(t, M[v >= 0 && v < STEMS_N ? v : 0]);
    } else if (i == SU_REC_MODE) {
        text_cat(t, v == REC_REPLACE ? "REPLACE" : "OVERDUB");
    } else if (i == SU_REC_CLICK) {
        static const char *const C[CLICK_N] = {"COUNT-IN", "RECORDING", "OFF"};
        text_cat(t, C[v >= 0 && v < CLICK_N ? v : 0]);
    } else if (i == SU_LEAD || i == SU_REC_OFFSET) {
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
    char t[24];
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
    if (su_cur == SU_DEMO) {                     /* the demo song (demo.c), in place of the project */
        if (confirm(CF_DEMO, "LOAD THE DEMO"))
            demo_load();
        return;
    }
    if (su_cur == SU_SHOTS) {                    /* your own one-shots (5.14): any turn offers the factory's back */
        if (!sh_user)
            app_say("ONE SHOTS", "FACTORY ALREADY");
        else if (eng_playing())
            app_say("STOP FIRST", "ONE SHOTS");
        else if (confirm(CF_SHOTS, "FACTORY SHOTS")) {
            sh_factory();
            app_say("ONE SHOTS", "FACTORY");
        }
        return;
    }
    if (su_cur == SU_NEW) {                      /* a new project: the start's desk, riddim, nothing else */
        if (confirm(CF_NEW, "NEW PROJECT"))
            new_project();
        return;
    }
    if (su_cur == SU_SLOT_SAVE || su_cur == SU_SLOT_LOAD) {   /* KNOB 2 picks the slot; SAVE does it */
        slot_pick(e);
        setup_say();
        return;
    }
    if (su_cur == SU_HELP) {                     /* KNOB 2: the help cards, one after another (5.11) */
        help_index_step(e);
        return;
    }
    if (su_cur == SU_KIT) {                      /* KNOB 2 picks the kit (KNOB 3 the bus); SAVE loads it */
        kit_sel = (uint8_t)(((int32_t)kit_sel + e % (int32_t)NKITS + (int32_t)NKITS) % (int32_t)NKITS);
        setup_say();
        return;
    }
    if (su_cur == SU_STEPPERS) {                   /* the UK steppers desk (P3.4; demo.c) */
        if (confirm(CF_STEPPERS, "LOAD STEPPERS"))
            steppers_load();
        return;
    }
    if (su_cur == SU_LEARN) {                    /* up: learn one; down: forget them all (asked twice) */
        if (e > 0)
            ml_learn_start();
        else if (confirm(CF_FORGET, "FORGET ALL CCS"))
            ml_forget_all();
        return;
    }
    if (su_cur >= SU_KC0 && su_cur <= SU_KC6) {  /* 8.8: a knob's cap colour, around the eight */
        uint32_t k = (uint32_t)(su_cur - SU_KC0);
        kcol_set(k, (uint32_t)(((int32_t)kcol(k) + e % (int32_t)KCOL_N + (int32_t)KCOL_N) % (int32_t)KCOL_N));
        dub.dirty_strips = dub.dirty_centre = dub.dirty_foot = dub.dirty_hdr = 1;
        setup_say();
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
    uint32_t h = (uint32_t)su_cur * 7u + ml_count() * 131u + kit_sel * 1009u + kit_bus * 7919u + sh_user * 104729u + slot_fresh * 15485863u
                 + kc_bits() * 2654435761u, i;
    for (i = 0; i < SU_N; i++)
        h = h * 31u + (uint32_t)ctl_get(SU_ID[i]);
    return h;
}

static void setup_draw(int all)          /* the list in the strips' and centre's place; the footer the knobs */
{
    uint32_t i, sig = setup_sig();
    char t[24];
    if (!all && sig == su_sig)
        return;
    su_sig = sig;
    CV_BANDS(PAGE_H, C_BLACK) {                 /* (in two halves: the canvas holds 76 rows) */
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
        CV_BAND_BLIT(STRIP_Y);
    }
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
    else if (!strncmp(what, "kcol", 4) && what[4] >= '0' && what[4] <= '6' && !what[5])
        *got = (int)kcol((uint32_t)(what[4] - '0'));   /* 8.8: kcolK, knob K's colour (0 the Big Knob .. 6 KNOB 4) */
    else
        return 0;
    return 1;
}
#endif
