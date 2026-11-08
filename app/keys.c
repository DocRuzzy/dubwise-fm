/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* The KEY & CHORDS page (ARP; docs/interface-spec.md 4.5), P0.8: the key and its lock ("can't play a wrong note"),
 * and the chord lane. Part of the unity build: dub.c includes it.
 *   bank 1 (ARP)        KNOB 1 root, KNOB 2 scale, KNOB 3 lock (OFF / SCALE / CHORD), KNOB 4 the keyboard's octave
 *   bank 2 (ARP again)  KNOB 1 the lane's bar, KNOB 2 its chord's root, KNOB 3 its quality, KNOB 4 the lane's length
 *   HOME + ARP          the lock on / off, from any page
 * The lock is an input filter only: keys outside the set are silent, never remapped (the keyboard keeps a
 * piano's spacing); what is recorded plays as recorded. The keys light the set: root bright, the rest of it dim. */

static struct {
    uint8_t bank;                        /* 0: key, 1: chord lane */
    int32_t slot;                        /* the lane's bar being edited */
    int32_t last_mode;                   /* the mode HOME + ARP brings back */
    uint32_t sig;
} ks = {0, 0, LOCK_SCALE, 0};

static uint32_t ui_bar(void) { return eng_playing() ? eng_beat() / 4u : 0u; }
static uint32_t ui_lock(void) { return lock_mask(ctl_vals(), ui_bar()); }
static int note_ok(int32_t n) { return (int)(ui_lock() >> (((n % 12) + 12) % 12) & 1u); }

static int32_t chord_now(void)           /* the lane's chord at this bar */
{
    int32_t len = ctl_get(P_CHORD_LEN);
    return ctl_get(P_CHORD0 + (int32_t)(ui_bar() % (uint32_t)(len > 0 ? len : 1)));
}

static void chord_text(int32_t code, char *t)   /* "Am", "D7" */
{
    t[0] = 0;
    text_cat(t, pitch_name(code / 8));
    text_cat(t, chord_short(code & 7));
}

static void lock_say(void)               /* "LOCK ON A NATURAL MINOR", "CHORD LOCK A MINOR", "LOCK OFF" */
{
    char t[32];
    int32_t m = ctl_get(P_LOCK), c = chord_now();
    t[0] = 0;
    if (m == LOCK_SCALE) {
        text_cat(t, pitch_name(ctl_get(P_KEY_ROOT)));
        text_cat(t, " ");
        text_cat(t, scale_name(ctl_get(P_SCALE)));
        app_say("LOCK ON", t);
    } else if (m == LOCK_CHORD) {
        text_cat(t, pitch_name(c / 8));
        text_cat(t, " ");
        text_cat(t, chord_name(c & 7));
        app_say("CHORD LOCK", t);
    } else
        app_say("LOCK", "OFF");
}

static void lock_toggle(void)            /* HOME + ARP */
{
    int32_t m = ctl_get(P_LOCK);
    if (m != LOCK_OFF)
        ks.last_mode = m;
    ctl_set(P_LOCK, m == LOCK_OFF ? ks.last_mode : LOCK_OFF);
    set_dirty = 1;
    dub.dirty_hdr = 1;
    lock_say();
}

static void keys_knob(uint32_t i, int32_t e)
{
    char t[24], n[8];
    set_dirty = 1;
    dub.dirty_hdr = 1;
    if (ks.bank == 0u) {
        if (i == 0u) {
            ctl_set(P_KEY_ROOT, (ctl_get(P_KEY_ROOT) + e % 12 + 12) % 12);
            app_say("KEY", pitch_name(ctl_get(P_KEY_ROOT)));
        } else if (i == 1u) {
            ctl_set(P_SCALE, ctl_get(P_SCALE) + e);
            app_say("SCALE", scale_name(ctl_get(P_SCALE)));
        } else if (i == 2u) {
            ctl_set(P_LOCK, ctl_get(P_LOCK) + e);
            lock_say();
        } else {
            ctl_set(P_KB_OCT, ctl_get(P_KB_OCT) + e);
            int_text(ctl_get(P_KB_OCT) - 3, n);
            t[0] = 0;
            text_cat(t, ctl_get(P_KB_OCT) >= 3 ? "+" : "");
            text_cat(t, n);
            app_say("OCTAVE", t);
        }
        return;
    }
    if (i == 0u) {                       /* the lane's bar */
        int32_t len = ctl_get(P_CHORD_LEN);
        ks.slot = (ks.slot + e % len + len) % len;
        int_text(ks.slot + 1, n);
        chord_text(ctl_get(P_CHORD0 + ks.slot), t);
        text_cat(t, " BAR ");
        text_cat(t, n);
        app_say("CHORD", t);
    } else if (i == 1u || i == 2u) {     /* its root, its quality */
        int32_t c = ctl_get(P_CHORD0 + ks.slot), r = c / 8, q = c & 7;
        if (i == 1u)
            r = (r + e % 12 + 12) % 12;
        else
            q = (q + e % CHORD_N + CHORD_N) % CHORD_N;
        ctl_set(P_CHORD0 + ks.slot, r * 8 + q);
        t[0] = 0;
        text_cat(t, pitch_name(r));
        text_cat(t, " ");
        text_cat(t, chord_name(q));
        app_say("CHORD", t);
    } else {
        ctl_set(P_CHORD_LEN, ctl_get(P_CHORD_LEN) + e);
        if (ks.slot >= ctl_get(P_CHORD_LEN))
            ks.slot = ctl_get(P_CHORD_LEN) - 1;
        int_text(ctl_get(P_CHORD_LEN), n);
        app_say("LANE BARS", n);
    }
}

static uint32_t kb_lights(uint32_t *dim)   /* the keyboard's 27 keys: the lock's set (root bright) */
{
    uint32_t k, bright = 0, m = ui_lock();
    int32_t root = ctl_get(P_LOCK) == LOCK_CHORD ? chord_now() / 8 : ctl_get(P_KEY_ROOT);
    for (k = 0; k < NKEYS; k++) {
        int32_t pc = (53 + (int32_t)k) % 12;   /* key 0 is F */
        if (ctl_get(P_LOCK) != LOCK_OFF && pc == root)
            bright |= 1u << k;
        else if (m >> pc & 1u)
            *dim |= 1u << k;
    }
    return bright;
}

static uint32_t keys_sig(void)
{
    uint32_t h = ks.bank * 7u + (uint32_t)ks.slot * 131u + ui_bar() % 16u * 1031u, i;
    for (i = P_KEY_ROOT; i < P_NGLOBAL; i++)
        h = h * 31u + (uint32_t)ctl_get(i);
    for (i = P_CHORD0; i < P_CHORD_END; i++)
        h = h * 33u + (uint32_t)ctl_get(i);
    return h;
}

static void keys_draw(int all)
{
    static const char *const MODE[LOCK_N] = {"OFF", "SCALE", "CHORD"};
    uint32_t i, sig = keys_sig(), m = ui_lock();
    int32_t root = ctl_get(P_LOCK) == LOCK_CHORD ? chord_now() / 8 : ctl_get(P_KEY_ROOT);
    char t[24], n[8];
    if (!all && sig == ks.sig)
        return;
    ks.sig = sig;
    cv_begin(240, PAGE_H, C_BLACK);      /* (a canvas holds 144 rows: the rest of the area below, cleared) */
    cv_text(6, 2, &FONT_XS, ks.bank ? "CHORD LANE" : "KEY & LOCK", C_GRAY);
    /* the twelve pitch classes: root bright, the allowed set amber, the rest dark */
    for (i = 0; i < 12u; i++) {
        int32_t x = 6 + (int32_t)i * 19, on = (int32_t)(m >> i & 1u);
        uint16_t c = ctl_get(P_LOCK) != LOCK_OFF && (int32_t)i == root ? C_HI : on ? C_AMB : C_LINE;
        cv_rect(x, 20, 17, 16, c);
        cv_text(x + (17 - text_w(&FONT_T, pitch_name((int32_t)i))) / 2, 22, &FONT_T, pitch_name((int32_t)i),
                on ? C_BLACK : C_DIM);
    }
    if (!ks.bank) {
        static const char *const L[4] = {"KEY", "SCALE", "LOCK", "OCTAVE"};
        for (i = 0; i < 4u; i++) {
            int32_t y = 46 + (int32_t)i * 26;
            t[0] = 0;
            if (i == 0u)
                text_cat(t, pitch_name(ctl_get(P_KEY_ROOT)));
            else if (i == 1u)
                text_cat(t, scale_name(ctl_get(P_SCALE)));
            else if (i == 2u)
                text_cat(t, MODE[ctl_get(P_LOCK)]);
            else {
                text_cat(t, ctl_get(P_KB_OCT) >= 3 ? "+" : "");
                int_text(ctl_get(P_KB_OCT) - 3, n);
                text_cat(t, n);
            }
            cv_text(8, y, &FONT_XS, L[i], C_GRAY);
            cv_text(80, y - 2, &FONT_B, t, i == 2u && ctl_get(P_LOCK) ? C_HI : C_WHITE);
        }
    } else {
        int32_t len = ctl_get(P_CHORD_LEN), cur = (int32_t)(ui_bar() % (uint32_t)len);
        for (i = 0; i < (uint32_t)len; i++) {   /* the lane: 8 a row */
            int32_t x = 6 + (int32_t)(i % 8u) * 29, y = 46 + (int32_t)(i / 8u) * 30;
            chord_text(ctl_get(P_CHORD0 + (int32_t)i), t);
            if ((int32_t)i == cur && eng_playing())
                cv_rect(x, y, 27, 24, C_LINE);
            if ((int32_t)i == ks.slot) {
                cv_rect(x, y, 27, 1, C_WHITE);
                cv_rect(x, y + 23, 27, 1, C_WHITE);
            }
            cv_text(x + 2, y + 4, &FONT_XS, t, (int32_t)i == cur ? C_HI : C_GRAY);
        }
        cv_text(8, 112, &FONT_XS, "NOW", C_GRAY);
        chord_text(chord_now(), t);
        cv_text(40, 108, &FONT_B, t, C_HI);   /* (FONT_M has capitals only) */
    }
    cv_blit(0, STRIP_Y);
    lcd_fill(0, STRIP_Y + PAGE_H, 240, FOOT_Y - STRIP_Y - PAGE_H, C_BLACK);
    cv_begin(240, FOOT_H, C_BLACK);
    cv_rect(0, 0, 240, 1, C_LINE);
    {
        static const char *const K1[4] = {"ROOT", "SCALE", "LOCK", "OCTAVE"}, *const K2[4] = {"BAR", "ROOT", "TYPE", "BARS"};
        for (i = 0; i < 4u; i++) {
            int32_t x = 6 + (int32_t)i * 59;
            cv_text(x, 4, &FONT_XS, ks.bank ? K2[i] : K1[i], C_GRAY);
            t[0] = 0;
            if (ks.bank) {
                int32_t c = ctl_get(P_CHORD0 + ks.slot);
                if (i == 0u)
                    int_text(ks.slot + 1, t);
                else if (i == 1u)
                    text_cat(t, pitch_name(c / 8));
                else if (i == 2u)
                    text_cat(t, chord_short(c & 7)[0] ? chord_short(c & 7) : "maj");
                else
                    int_text(ctl_get(P_CHORD_LEN), t);
            } else
                text_cat(t, "");
            cv_text(x, 18, &FONT_B, t, C_HI);
        }
    }
    cv_blit(0, FOOT_Y);
}

#ifdef PLAT_HOST
static int keys_expect(const char *what, int *got)
{
    if (!strcmp(what, "lock_mask"))      /* the allowed pitch classes, a bit each (bit 0 = C) */
        *got = (int)ui_lock();
    else if (!strcmp(what, "keys_bank"))
        *got = ks.bank;
    else
        return 0;
    return 1;
}
#endif
