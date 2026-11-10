/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* The ARRANGE page (HOME on the DUB page; spec 4.2), first version, P1.7: scenes.
 *   White 1-16          recall that scene: on the line, as a mute would (PLAY held: now)
 *   SAVE held + white   store the desk (and the patterns, by the scope) into that scene
 *   KNOB 4              the scope a store takes: DESK, or DESK + PATTERNS ("P")
 * The song (P1.8): a bar list (engine/song.h).
 *   REC (here or on DUB)   capture on / off: while playing, each bar takes the desk at its line; HOME + REC: undo
 *                          the last capture
 *   Black 1-11          a section marker at the bar playing (stopped: the cursor's); again: removed. HOME + black
 *                       1-4: the dub-plate words, VERSION, DUBPLATE, SPECIAL, RUB-A-DUB
 *   KNOB 1              the cursor (a bar); KNOB 2 the song's length; KNOB 3 OFF / ONCE / LOOP (playback)
 * Motion (P2.7, engine/motion.h): what the hands do inside each bar, captured with REC too; SAVE + REC clears it.
 * A bar with motion has a white mark under it.
 * In song mode the runway counts to the next section. Part of the unity build: dub.c includes it. */

static struct {
    int32_t cur;                         /* the scene last recalled, -1 none */
    uint8_t scope;                       /* the next store's */
    uint32_t sig;
    int32_t cursor;                      /* the song's bar shown / marked while stopped */
    uint8_t has_undo;
} arr = {-1, SCOPE_DESK, 0, 0, 0};
static PLAT_POOL song_bar_t song_undo[SONG_BARS];
static uint32_t arr_cap_seq;                     /* the capture's place among the undo history's steps (undo.c) */          /* the song before the last capture */
static uint8_t song_dirty;

static void song_rec_toggle(void)        /* REC: capture on / off */
{
    int on = !ctl_get(P_SONG_REC);
    uint32_t i;
    if (on) {                            /* a copy first: HOME + REC takes the capture back */
        for (i = 0; i < SONG_BARS; i++)
            song_undo[i] = song[i];
        arr.has_undo = 1;
        arr_cap_seq = undo_seq_next();
    }
    ctl_set(P_SONG_REC, on);
    song_dirty = 1;
    set_dirty = 1;
    arr.sig = 0;
    app_say("SONG CAPTURE", on ? "ON" : "OFF");
}

static void song_undo_last(void)         /* HOME + REC */
{
    uint32_t i;
    if (!arr.has_undo) {
        app_say("SONG", "NOTHING TO UNDO");
        return;
    }
    ctl_set(P_SONG_REC, 0);
    for (i = 0; i < SONG_BARS; i++)
        song[i] = song_undo[i];
    arr.has_undo = 0;
    song_dirty = 1;
    set_dirty = 1;
    arr.sig = 0;
    app_say("SONG", "CAPTURE UNDONE");
}

static uint32_t song_bar_ui(void)        /* the bar marks go to: the one playing, or the cursor */
{
    return eng_playing() && ctl_get(P_SONG_MODE) != SONG_OFF ? eng_song_bar()
           : eng_playing() ? (eng_beat() / 4u) % SONG_BARS : (uint32_t)arr.cursor;
}

static void song_mark(uint32_t sec)      /* black key: a section marker */
{
    uint32_t b = song_bar_ui();
    char t[24], n[8];
    if ((song[b].flags & BAR_SECTION) && song[b].section == sec) {
        song[b].flags &= (uint8_t)~BAR_SECTION;
        app_say("MARKER", "REMOVED");
    } else {
        song[b].flags |= BAR_SECTION;
        song[b].section = (uint8_t)sec;
        t[0] = 0;
        text_cat(t, section_name((int32_t)sec));
        text_cat(t, " AT BAR ");
        int_text((int32_t)b + 1, n);
        text_cat(t, n);
        app_say("MARKER", t);
    }
    song_dirty = 1;
    set_dirty = 1;
    arr.sig = 0;
}

/* the runway in song mode: the section the song is in, and the next marker (or the end): from, to (bars) */
static int song_section(uint32_t *from, uint32_t *to)
{
    uint32_t b = eng_song_bar(), len = (uint32_t)ctl_get(P_SONG_LEN), i, f = 0, t = len, any = 0;
    if (ctl_get(P_SONG_MODE) == SONG_OFF || !eng_playing())
        return 0;
    for (i = 0; i < len && i < SONG_BARS; i++)
        if (song[i].flags & BAR_SECTION) {
            any = 1;
            if (i <= b)
                f = i;
            else if (i < t) {
                t = i;
                break;
            }
        }
    if (!any)
        return 0;
    *from = f;
    *to = t;
    return 1;
}

static void arr_mark(void) { arr.sig = 0; }

static void song_sync(void)              /* the engine's song changes back to the UI: a capture grew it, ONCE ended */
{
    if (eng_param(P_SONG_LEN) != ctl_get(P_SONG_LEN) && ctl_get(P_SONG_REC) && eng_playing())
        ctl_set(P_SONG_LEN, eng_param(P_SONG_LEN));
    static uint8_t seen;                 /* the engine seen playing since the UI's start: only then is a stop its own
                                          * (a song played ONCE ended). Before this, a PLAY whose start the engine had
                                          * not taken up yet (the same frame) read as ended, and was undone */
    if (!ctl_get(P_PLAY))
        seen = 0;
    else if (eng_playing())
        seen = 1;
    if (ctl_get(P_PLAY) && seen && !eng_param(P_PLAY) && !eng_playing() && ctl_get(P_SONG_MODE) == SONG_ONCE) {
        seen = 0;
        ctl_set(P_PLAY, 0);
    }
}

static void arr_scene_name(uint32_t s, char *t)
{
    char n[8];
    t[0] = 0;
    text_cat(t, "SCENE ");
    int_text((int32_t)s + 1, n);
    text_cat(t, n);
}

static void arr_store(uint32_t s)
{
    char t[16];
    ctl_scene_store(s, arr.scope);
    scenes_dirty |= (uint16_t)(1u << s);
    set_dirty = 1;
    arr_scene_name(s, t);
    app_say(t, arr.scope == SCOPE_PATTERNS ? "STORED WITH PATTERNS" : "STORED");
}

static void arr_recall(uint32_t s, uint32_t beats)
{
    char t[16];
    arr_scene_name(s, t);
    if (!scenes[s].used) {
        app_say(t, "EMPTY");
        return;
    }
    ctl_scene_recall(s, beats);
    arr.cur = (int32_t)s;
    if (st.sig)
        st.sig = 0;                      /* (the STEPS grid shows the patterns) */
    app_say(t, scenes[s].scope == SCOPE_PATTERNS ? "WITH PATTERNS" : "");
}

static void arr_step(int32_t d)          /* PRESET on the DUB page: the next stored scene up or down, cued */
{
    int32_t k, s = arr.cur;
    for (k = 0; k < NSCENES; k++) {
        s = (s + (d > 0 ? 1 : -1) + NSCENES) % NSCENES;
        if (scenes[s].used) {
            arr_recall((uint32_t)s, act_beats());
            return;
        }
    }
    app_say("SCENES", "NONE STORED");
}

static void arr_keys(uint32_t keys, uint32_t prev)
{
    uint32_t k;
    for (k = 0; k < 16u; k++)
        if ((keys >> WK[k] & 1u) && !(prev >> WK[k] & 1u)) {
            if (dub.btn >> B_SAVE & 1u) {
                dub.save_used = 1;
                arr_store(k);
            } else
                arr_recall(k, act_beats());
        }
    for (k = 0; k < SECTION_KEYS; k++)
        if ((keys >> BK[k] & 1u) && !(prev >> BK[k] & 1u)) {
            if ((dub.btn >> B_HOME & 1u) && k < SECTIONS - SECTION_KEYS) {   /* HOME + black 1-4: VERSION .. */
                dub.home_used = 1;
                song_mark(SECTION_KEYS + k);
            } else
                song_mark(k);
        }
}

static void arr_knob(uint32_t i, int32_t e)
{
    char t[16];
    static const char *const MODE[SONG_N] = {"OFF", "ONCE", "LOOP"};
    if (i == 3u) {
        arr.scope = (uint8_t)((arr.scope + (e > 0 ? 1 : SCOPE_N - 1)) % SCOPE_N);
        app_say("SCOPE", arr.scope == SCOPE_PATTERNS ? "DESK AND PATTERNS" : "DESK");
    } else if (i == 0u) {
        int32_t len = ctl_get(P_SONG_LEN);
        arr.cursor = ((arr.cursor + e) % len + len) % len;
        int_text(arr.cursor + 1, t);
        app_say("BAR", t);
    } else if (i == 1u) {
        ctl_set(P_SONG_LEN, ctl_get(P_SONG_LEN) + e);
        if (arr.cursor >= ctl_get(P_SONG_LEN))
            arr.cursor = ctl_get(P_SONG_LEN) - 1;
        int_text(ctl_get(P_SONG_LEN), t);
        set_dirty = 1;
        app_say("SONG BARS", t);
    } else {
        ctl_set(P_SONG_MODE, ctl_get(P_SONG_MODE) + e);
        set_dirty = 1;
        app_say("SONG", MODE[ctl_get(P_SONG_MODE)]);
    }
    arr.sig = 0;
}

static uint32_t arr_lights(uint32_t *dim)
{
    uint32_t k, bright = 0;
    for (k = 0; k < 16u; k++) {
        uint32_t bit = 1u << WK[k];
        if ((int32_t)k == arr.cur && scenes[k].used)
            bright |= cued_mask() && blink_on() ? 0u : bit;
        else if (scenes[k].used)
            *dim |= bit;
    }
    return bright;
}

static uint32_t arr_sigf(void)
{
    uint32_t h = (uint32_t)(arr.cur + 1) * 7u + arr.scope * 131u + (cued_mask() ? 1031u * (1u + blink_on()) : 0u)
                 + song_bar_ui() * 65537u + (uint32_t)ctl_get(P_SONG_LEN) * 257u + (uint32_t)ctl_get(P_SONG_MODE) * 3u
                 + (uint32_t)ctl_get(P_SONG_REC) * 5u + mot_total() * 40503u, k;
    for (k = 0; k < NSCENES; k++)
        h = h * 3u + scenes[k].used + scenes[k].scope * 2u;
    return h;
}

static void arr_draw(int all)
{
    uint32_t k, sig = arr_sigf();
    char t[8];
    if (!all && sig == arr.sig)
        return;
    arr.sig = sig;
    cv_begin(240, PAGE_H, C_BLACK);
    cv_text(6, 2, &FONT_XS, "SCENES", C_GRAY);
    if (cued_mask())
        cv_text(70, 2, &FONT_XS, "CUED", C_WHITE);
    for (k = 0; k < NSCENES; k++) {             /* 2 rows of 8: number, P for patterns, the last recalled lit */
        int32_t x = 6 + (int32_t)(k % 8u) * 29, y = 20 + (int32_t)(k / 8u) * 34;
        int used = scenes[k].used, cur = (int32_t)k == arr.cur;
        cv_rect(x, y, 26, 30, cur && used ? C_HI : used ? C_LINE : C_BLACK);
        if (!used) {
            cv_rect(x, y, 26, 1, C_LINE);
            cv_rect(x, y + 29, 26, 1, C_LINE);
        }
        int_text((int32_t)k + 1, t);
        cv_text(x + 3, y + 2, &FONT_XS, t, cur && used ? C_BLACK : used ? C_WHITE : C_DIM);
        if (used && scenes[k].scope == SCOPE_PATTERNS)
            cv_text(x + 16, y + 14, &FONT_T, "P", cur ? C_BLACK : C_AMB);
    }
    {                                    /* the song: 32 bars around where it is, captured bars, markers, here */
        int32_t len = ctl_get(P_SONG_LEN), here = (int32_t)song_bar_ui(), first = here - 8, b;
        static const char *const MODE[SONG_N] = {"OFF", "ONCE", "LOOP"};
        char u[24];
        if (first < 0)
            first = 0;
        u[0] = 0;
        text_cat(u, "SONG ");
        text_cat(u, MODE[ctl_get(P_SONG_MODE)]);
        cv_text(6, 92, &FONT_XS, u, ctl_get(P_SONG_MODE) ? C_HI : C_GRAY);
        if (ctl_get(P_SONG_REC))
            cv_text(100, 92, &FONT_XS, "REC", C_RED);
        int_text(here + 1, t);
        u[0] = 0;
        text_cat(u, "BAR ");
        text_cat(u, t);
        text_cat(u, "/");
        int_text(len, t);
        text_cat(u, t);
        cv_text(234 - text_w(&FONT_XS, u), 92, &FONT_XS, u, C_GRAY);
        for (b = first; b < first + 32 && b < SONG_BARS; b++) {
            int32_t x = 6 + (b - first) * 7, y = 112;
            uint16_t c = b >= len ? C_BLACK : (song[b].flags & BAR_SET) ? C_AMB : C_LINE;
            cv_rect(x, y, 6, 14, c);
            if (song[b].flags & BAR_SECTION)
                cv_rect(x, y - 4, 2, 22, C_WHITE);
            if (mot_n[b] && b < len)        /* motion inside the bar */
                cv_rect(x + 1, y + 6, 4, 2, C_WHITE);
            if (b == here)
                cv_rect(x, y + 16, 6, 3, C_WHITE);
        }
    }
    cv_blit(0, STRIP_Y);
    lcd_fill(0, STRIP_Y + PAGE_H, 240, FOOT_Y - STRIP_Y - PAGE_H, C_BLACK);
    cv_begin(240, FOOT_H, C_BLACK);
    cv_rect(0, 0, 240, 1, C_LINE);
    {
        static const char *const L[4] = {"BAR", "BARS", "SONG", "SCOPE"};
        static const char *const MODE[SONG_N] = {"OFF", "ONCE", "LOOP"};
        uint32_t i;
        for (i = 0; i < 4u; i++) {
            int32_t x = 6 + (int32_t)i * 59;
            cv_text(x, 4, &FONT_XS, L[i], C_GRAY);
            t[0] = 0;
            if (i == 0u)
                int_text(arr.cursor + 1, t);
            else if (i == 1u)
                int_text(ctl_get(P_SONG_LEN), t);
            else if (i == 2u)
                text_cat(t, MODE[ctl_get(P_SONG_MODE)]);
            else
                text_cat(t, arr.scope == SCOPE_PATTERNS ? "DSK+P" : "DESK");
            cv_text(x, 18, &FONT_B, t, C_HI);
        }
    }
    cv_blit(0, FOOT_Y);
}

#ifdef PLAT_HOST
static int arr_expect(const char *what, int *got)
{
    if (!strcmp(what, "scene"))          /* the scene last recalled (1-16), 0 none */
        *got = arr.cur + 1;
    else if (!strncmp(what, "scene_used", 10))
        *got = scenes[(atoi(what + 10) - 1) % NSCENES].used;
    else if (!strncmp(what, "song_mute", 9))   /* song_muteN: bar N's captured mutes (a bit a channel), -1 unset */
        *got = song[(atoi(what + 9) - 1) % SONG_BARS].flags & BAR_SET ? song[(atoi(what + 9) - 1) % SONG_BARS].mute : -1;
    else if (!strncmp(what, "song_mark", 9))   /* song_markN: bar N's section (+1), 0 none */
        *got = song[(atoi(what + 9) - 1) % SONG_BARS].flags & BAR_SECTION ? song[(atoi(what + 9) - 1) % SONG_BARS].section + 1 : 0;
    else if (!strcmp(what, "song_bar"))
        *got = (int)eng_song_bar() + 1;
    else if (!strcmp(what, "mot_total"))       /* the motion's moves, all bars */
        *got = (int)mot_total();
    else if (!strncmp(what, "mot_bar", 7))     /* mot_barN: bar N's moves */
        *got = mot_n[(atoi(what + 7) - 1) % SONG_BARS];
    else if (!strncmp(what, "motv", 4) && strchr(what, '_')) {   /* motvB_I: bar B's move I's value (from 1; 7.x) */
        mot_ev_t e;
        *got = mot_get((uint32_t)atoi(what + 4) - 1u, (uint32_t)atoi(strchr(what, '_') + 1) - 1u, &e) ? e.v : -1;
    } else if (!strcmp(what, "mot_drops"))
        *got = (int)mot_drops;
    else
        return 0;
    return 1;
}
#endif
