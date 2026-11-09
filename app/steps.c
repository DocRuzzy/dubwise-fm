/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* The STEPS page (docs/interface-spec.md 4.3), first version, P0.4. Part of the unity build: dub.c includes it.
 *
 *   White 1-16         toggle the steps on this page of the channel's pattern (a new step: normal, its default note)
 *   Black 1-8          the channel
 *   Black 9 / 10 held + white   that step an accent / a ghost (on, or back to normal)
 *   OCT- / OCT+        the page of 16 steps: 1-16, 17-32, 33-48, 49-64
 *   KNOB 1             the pattern's length, 1..64 sixteenths
 *   KNOB 2             the note of the step held (or the last one touched): a semitone a click; hats: closed / open
 *   KNOB 4             a riddim template for the drums and bass (it lands on the next bar line)
 *   REC (a tonal channel: the bass)   the keyboard (P0.8): all 27 keys play notes in KEY's octave; under the key
 *                      lock, keys outside the set are silent. Playing, a note is recorded at the nearest sixteenth;
 *                      stopped, at a cursor that moves on a step each note. REC again: the steps again
 * The grid of all eight channels is in the strips' place; the channel's own page, bigger, in the centre. */

static struct {
    int32_t ch;                          /* the channel edited */
    uint32_t page;                       /* 0..3: steps page * 16 + 1 .. */
    int32_t last;                        /* the step last touched (an index into the pattern), -1 none */
    int32_t tpl;                         /* the template KNOB 4 points at */
    int32_t last_tick;                   /* for redraws */
    uint32_t sig;                        /* what the grid shows */
    uint8_t kb;                          /* the keyboard (REC) */
    int32_t cur;                         /* its cursor while stopped */
    int8_t held[NKEYS];                  /* the note each held key plays, -1 none */
    uint32_t recorded;                   /* notes recorded (tests) */
} st = {CH_KICK, 0, -1, TPL_ONE_DROP, -2, 0, 0, 0, {0}, 0};

static int tonal(int32_t ch) { return ch == CH_BASS || ch == CH_LEAD; }   /* (the chord channels take voicings) */
static int32_t kb_note(uint32_t k)       /* the key's note: KEY's octave (set for the bass), the lead two up */
{
    return 53 + (int32_t)k + 12 * (ctl_get(P_KB_OCT) - 3) + (st.ch == CH_LEAD ? 24 : 0);
}

static const char *const LVL_NAME[LVL_N] = {"OFF", "GHOST", "ON", "ACCENT"};

static uint32_t pat_now(int32_t ch) { return (uint32_t)ctl_get(P_CH(ch, CHF_PAT)) & 3u; }   /* (as launched) */
static int32_t pat_len(int32_t ch) { return ctl_meta_now((uint32_t)ch).len; }
static void pat_len_set(int32_t ch, int32_t len)
{
    pmeta_t m = ctl_meta_now((uint32_t)ch);
    m.len = (uint8_t)clamp(len, 1, NSTEPS);
    ctl_meta_set((uint32_t)ch, pat_now(ch), m);
}
static void pats_clear_all(void)         /* every channel's B..D empty, A playing (a project loaded over it) */
{
    uint32_t ch, p, i;
    for (ch = 0; ch < NCH; ch++) {
        ctl_set(P_CH(ch, CHF_PAT), 0);
        for (p = 1; p < NPAT; p++) {
            ctl_meta_set(ch, p, pmeta_def());
            for (i = 0; i < NSTEPS; i++) {
                step_t s = {LVL_OFF, default_note(ch), 0, 0};
                ctl_step_at(ch, p, i, s);
            }
        }
    }
}
static int32_t steps_len(void) { return pat_len(st.ch); }

static void note_text(int32_t ch, int32_t n, char *t)   /* "A1"; hats "CL" / "OP" */
{
    static const char *const N[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    char o[8];
    t[0] = 0;
    if (ch == CH_HATS) {
        text_cat(t, n == 46 ? "OP" : "CL");
        return;
    }
    if (ch == CH_SKANK || ch == CH_ORGAN) {      /* a voicing of the lane's chord */
        text_cat(t, n == VOICING_LO ? "LO" : n == VOICING_HI ? "HI" : "MD");
        return;
    }
    if (ch == CH_KICK || ch == CH_SNARE)
        return;
    text_cat(t, N[n % 12]);
    int_text(n / 12 - 1, o);
    text_cat(t, o);
}

static void steps_key(uint32_t w)        /* white key w (0..15) pressed: its step */
{
    uint32_t i = st.page * 16u + w;
    step_t s = ctl_get_step((uint32_t)st.ch, i);
    uint32_t k9 = dub.keys >> BK[8] & 1u, k10 = dub.keys >> BK[9] & 1u;
    char t[24], n[8];
    if (k9 || k10) {                     /* accent / ghost (or back to normal) */
        uint8_t want = k9 ? LVL_ACCENT : LVL_GHOST;
        s.lvl = s.lvl == want ? LVL_NORMAL : want;
    } else {
        s.lvl = s.lvl ? LVL_OFF : LVL_NORMAL;
        if (s.lvl && st.last >= 0 && st.ch != CH_HATS)   /* a new step takes the last note touched */
            s.note = ctl_get_step((uint32_t)st.ch, (uint32_t)st.last).note;
    }
    ctl_step((uint32_t)st.ch, i, s);
    st.last = (int32_t)i;
    set_dirty = 1;
    int_text((int32_t)i + 1, n);
    t[0] = 0;
    text_cat(t, n);
    text_cat(t, " ");
    text_cat(t, LVL_NAME[s.lvl]);
    app_say("STEP", t);
}

static void steps_rec(void)             /* REC: the keyboard on or off */
{
    uint32_t k;
    if (!tonal(st.ch)) {
        app_say("KEYBOARD", "NOT ON DRUMS");
        return;
    }
    st.kb = (uint8_t)!st.kb;
    st.cur = st.last >= 0 ? st.last : 0;
    for (k = 0; k < NKEYS; k++)
        if (st.held[k] >= 0 && st.kb == 0u)
            ctl_note((uint32_t)st.ch, st.held[k], 0);
    for (k = 0; k < NKEYS; k++)
        st.held[k] = -1;
    app_say("KEYBOARD", st.kb ? "ON" : "OFF");
}

static void kb_keys(uint32_t keys, uint32_t prev)   /* the keyboard: play, and record */
{
    uint32_t k;
    char t[16], n[8];
    for (k = 0; k < NKEYS; k++) {
        uint32_t bit = 1u << k;
        if ((keys & bit) && !(prev & bit)) {
            int32_t note = kb_note(k), len = steps_len(), idx;
            step_t s;
            if (!note_ok(note))
                continue;                /* outside the lock's set: silent, never moved to a neighbour */
            st.held[k] = (int8_t)note;
            ctl_note((uint32_t)st.ch, note, touch_vel(k) ? touch_vel(k) : 100u);
            if (eng_playing()) {         /* at the nearest sixteenth */
                idx = (eng_tick() + (eng_frac16() >= 128u ? 1 : 0)) % len;
            } else {                     /* at the cursor, which moves on */
                idx = st.cur % len;
                st.cur = (idx + 1) % len;
            }
            s.lvl = LVL_NORMAL;
            s.note = (int8_t)note;
            ctl_step((uint32_t)st.ch, (uint32_t)idx, s);
            st.last = idx;
            st.recorded++;
            set_dirty = 1;
            note_text(st.ch, note, t);
            int_text(idx + 1, n);
            text_cat(t, " STEP ");
            text_cat(t, n);
            app_say("NOTE", t);
        } else if (!(keys & bit) && (prev & bit) && st.held[k] >= 0) {
            ctl_note((uint32_t)st.ch, st.held[k], 0);
            st.held[k] = -1;
        }
    }
}

static void steps_keys(uint32_t keys, uint32_t prev)
{
    uint32_t k;
    if (st.kb) {
        kb_keys(keys, prev);
        return;
    }
    for (k = 0; k < 16u; k++)
        if ((keys >> WK[k] & 1u) && !(prev >> WK[k] & 1u))
            steps_key(k);
    for (k = 0; k < NCH; k++)
        if ((keys >> BK[k] & 1u) && !(prev >> BK[k] & 1u)) {
            st.ch = (int32_t)k;
            st.last = -1;
            app_say("STEPS", CH_NAME[k]);
        }
}

static int32_t steps_held(void)          /* the step whose key is held (on this page), -1 none */
{
    uint32_t k;
    for (k = 0; k < 16u; k++)
        if (dub.keys >> WK[k] & 1u)
            return (int32_t)(st.page * 16u + k);
    return -1;
}

static void steps_oct(int32_t d)
{
    int32_t pg = (int32_t)st.page + d;
    char t[16], n[8];
    if (pg < 0 || pg > 3)
        return;
    st.page = (uint32_t)pg;
    t[0] = 0;
    int_text(pg * 16 + 1, n);
    text_cat(t, n);
    text_cat(t, "-");
    int_text(pg * 16 + 16, n);
    text_cat(t, n);
    app_say("STEPS", t);
}

static void steps_knob(uint32_t i, int32_t e)
{
    char t[16];
    if (i == 0u) {                       /* the length */
        pat_len_set(st.ch, steps_len() + e);
        set_dirty = 1;
        int_text(steps_len(), t);
        app_say("LENGTH", t);
    } else if (i == 1u) {                /* the note of the step held, or the last touched */
        int32_t k = steps_held() >= 0 ? steps_held() : st.last;
        step_t s;
        if (k < 0) {
            app_say("NOTE", "TOUCH A STEP");
            return;
        }
        s = ctl_get_step((uint32_t)st.ch, (uint32_t)k);
        if (st.ch == CH_HATS)
            s.note = (int8_t)(s.note == 46 ? 42 : 46);
        else if (st.ch == CH_SKANK || st.ch == CH_ORGAN)
            s.note = (int8_t)((s.note + e % VOICING_N + VOICING_N) % VOICING_N);
        else if (st.ch == CH_KICK || st.ch == CH_SNARE) {
            app_say("NOTE", "DRUM");
            return;
        }
        else {                           /* a semitone a click, over notes the lock does not allow */
            int32_t nn = s.note, d = e > 0 ? 1 : -1, left = e > 0 ? e : -e, guard = 0;
            while (left > 0 && guard++ < 48) {
                nn = clamp(nn + d, 24, 96);
                if (note_ok(nn))
                    left--;
            }
            s.note = (int8_t)nn;
        }
        ctl_step((uint32_t)st.ch, (uint32_t)k, s);
        st.last = k;
        set_dirty = 1;
        note_text(st.ch, s.note, t);
        app_say("NOTE", t);
    } else if (i == 3u) {                /* a template: the drums and the bass, on the next bar line */
        st.tpl = (st.tpl + e % TPL_N + TPL_N) % TPL_N;
        ctl_template(st.tpl, (1u << CH_KICK) | (1u << CH_SNARE) | (1u << CH_HATS) | (1u << CH_BASS)
                                 | (1u << CH_SKANK) | (1u << CH_ORGAN), 4u);
        set_dirty = 1;
        app_say("RIDDIM", roots_name(template_name(st.tpl)));
    } else {
        app_say("SWING", "TO COME");
    }
}

static uint32_t steps_lights(uint32_t *dim)   /* the keys: steps on bright, off dim; the playhead blinks off */
{
    uint32_t k, bright = 0;
    if (st.kb)                           /* the keyboard: the lock's set, and what is held */
        return kb_lights(dim) | dub.keys;
    int32_t tick = eng_tick(), len = steps_len(), at = tick >= 0 ? tick % len : -1;
    for (k = 0; k < 16u; k++) {
        uint32_t i = st.page * 16u + k, bit = 1u << WK[k];
        int on = ctl_get_step((uint32_t)st.ch, i).lvl != LVL_OFF;
        if ((int32_t)i >= len)
            continue;                    /* past the end: dark */
        if ((int32_t)i == at)
            on = !on;                    /* the playhead */
        if (on)
            bright |= bit;
        else
            *dim |= bit;
    }
    for (k = 0; k < 11u; k++)
        if (k == (uint32_t)st.ch)
            bright |= 1u << BK[k];
        else
            *dim |= 1u << BK[k];
    return bright;
}

/* --------------------------------------------------------------- drawing --- */
static uint32_t steps_sig(void)          /* changes whenever the grid would */
{
    uint32_t h = (uint32_t)st.ch * 7u + st.page * 131u + (uint32_t)eng_tick() * 1031u, ch, i;
    for (ch = 0; ch < NCH; ch++) {
        h = h * 31u + (uint32_t)pat_len((int32_t)ch) + pat_now((int32_t)ch) * 977u;
        for (i = 0; i < 16u; i++) {
            step_t s = ctl_get_step(ch, st.page * 16u + i);
            h = h * 33u + s.lvl * 131u + (uint8_t)s.note;
        }
    }
    return h;
}

static void steps_draw_grid(void)        /* in the strips' place: every channel's page of 16 */
{
    uint32_t ch, i;
    int32_t tick = eng_tick();
    cv_begin(240, STRIP_H, C_BLACK);
    for (ch = 0; ch < NCH; ch++) {
        int32_t y = (int32_t)ch * 12, len = pat_len((int32_t)ch), at = tick >= 0 ? tick % len : -1;
        cv_text(2, y, &FONT_T, CH_SHORT[ch], (int32_t)ch == st.ch ? C_WHITE : C_GRAY);
        for (i = 0; i < 16u; i++) {
            int32_t idx = (int32_t)(st.page * 16u + i), x = 34 + (int32_t)i * 13;
            step_t s = ctl_get_step(ch, (uint32_t)idx);
            uint16_t c = s.lvl == LVL_ACCENT ? C_HI : s.lvl == LVL_NORMAL ? C_AMB : s.lvl == LVL_GHOST ? C_GRAY : C_LINE;
            if (idx >= len)
                continue;
            cv_rect(x, y + 2, 11, 8, c);
            if (idx == at)
                cv_rect(x, y + 10, 11, 1, C_WHITE);
        }
    }
    cv_blit(0, STRIP_Y);
}

static void steps_draw_centre(void)      /* the channel's page, bigger, with its notes */
{
    uint32_t i;
    int32_t tick = eng_tick(), len = steps_len(), at = tick >= 0 ? tick % len : -1;
    char t[24], n[8];
    cv_begin(240, CENTRE_H, C_BLACK);
    cv_rect(0, 0, 240, 1, C_LINE);
    t[0] = 0;
    text_cat(t, CH_NAME[st.ch]);
    text_cat(t, st.kb ? "  KEYBOARD " : "  STEPS ");
    int_text((int32_t)st.page * 16 + 1, n);
    text_cat(t, n);
    text_cat(t, "-");
    int_text((int32_t)st.page * 16 + 16, n);
    text_cat(t, n);
    cv_text(6, 4, &FONT_XS, t, C_GRAY);
    for (i = 0; i < 16u; i++) {
        int32_t idx = (int32_t)(st.page * 16u + i), x = 4 + (int32_t)i * 14, h;
        step_t s = ctl_get_step((uint32_t)st.ch, (uint32_t)idx);
        if (idx >= len) {
            cv_rect(x, 54, 12, 1, C_LINE);
            continue;
        }
        h = s.lvl == LVL_ACCENT ? 28 : s.lvl == LVL_NORMAL ? 20 : s.lvl == LVL_GHOST ? 10 : 2;
        cv_rect(x, 54 - h, 12, h, s.lvl ? (idx == st.last ? C_WHITE : C_HI) : C_LINE);
        if (idx == at)
            cv_rect(x, 57, 12, 2, C_WHITE);
        if (s.lvl && (st.ch == CH_BASS || st.ch == CH_HATS || st.ch == CH_SKANK || st.ch == CH_ORGAN)) {
            note_text(st.ch, s.note, t);
            cv_text(x, 60, &FONT_T, t, C_GRAY);
        }
        if (i % 4u == 0u)
            cv_rect(x - 1, 22, 1, 34, C_DIM);   /* the beats */
    }
    cv_blit(0, CENTRE_Y);
}

static void steps_draw_footer(void)
{
    static const char *const L[4] = {"LEN", "NOTE", "SWING", "RIDDIM"};
    uint32_t i;
    int32_t k = steps_held() >= 0 ? steps_held() : st.last;
    char t[20];
    cv_begin(240, FOOT_H, C_BLACK);
    cv_rect(0, 0, 240, 1, C_LINE);
    for (i = 0; i < 4u; i++) {
        int32_t x = 6 + (int32_t)i * 59;
        cv_text(x, 4, &FONT_XS, L[i], C_GRAY);
        t[0] = 0;
        if (i == 0u)
            int_text(steps_len(), t);
        else if (i == 1u && k >= 0)
            note_text(st.ch, ctl_get_step((uint32_t)st.ch, (uint32_t)k).note, t);
        else if (i == 2u)
            text_cat(t, "--");
        else if (i == 3u)
            text_cat(t, template_name(st.tpl));
        cv_text(x, 18, i == 3u ? &FONT_XS : &FONT_B, t[0] ? t : "--", C_HI);
    }
    cv_blit(0, FOOT_Y);
}

static void steps_draw(int all)
{
    uint32_t sig = steps_sig();
    if (all || sig != st.sig) {
        st.sig = sig;
        steps_draw_grid();
        steps_draw_centre();
        steps_draw_footer();
    }
}

static void steps_speak(char *t)         /* the page, for a screen reader */
{
    char n[8];
    text_cat(t, ". STEPS, ");
    text_cat(t, CH_NAME[st.ch]);
    if (st.kb)
        text_cat(t, ", KEYBOARD");
    text_cat(t, ", LENGTH ");
    int_text(steps_len(), n);
    text_cat(t, n);
}

#ifdef PLAT_HOST
static int steps_expect(const char *what, int *got)
{
    if (!strcmp(what, "steps_ch"))       /* the channel edited (1-8) */
        *got = st.ch + 1;
    else if (!strncmp(what, "step", 4) && what[4] >= '1' && what[4] <= '8' && what[5] == '_') {
        step_t s = ctl_get_step((uint32_t)(what[4] - '1'), (uint32_t)(atoi(what + 6) - 1));   /* stepC_N: level */
        *got = s.lvl;
    } else if (!strncmp(what, "note", 4) && what[4] >= '1' && what[4] <= '8' && what[5] == '_')
        *got = ctl_get_step((uint32_t)(what[4] - '1'), (uint32_t)(atoi(what + 6) - 1)).note;
    else if (!strcmp(what, "kb"))
        *got = st.kb;
    else if (!strcmp(what, "recorded"))
        *got = (int)st.recorded;
    else if (!strncmp(what, "eng_step", 8) && what[8] >= '1' && what[8] <= '8' && what[9] == '_')
        *got = eng_step((uint32_t)(what[8] - '1'), (uint32_t)(atoi(what + 10) - 1)).lvl;
    else
        return 0;
    return 1;
}
#endif
