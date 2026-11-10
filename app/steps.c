/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* The STEPS page (docs/interface-spec.md 4.3), first version, P0.4. Part of the unity build: dub.c includes it.
 *
 *   White 1-16         toggle the steps on this page of the channel's pattern (a new step: normal, its default note)
 *   Black 1-8          the channel
 *   Black 9 / 10 held + white   that step an accent / a ghost (on, or back to normal)
 *   OCT- / OCT+        the page of 16 steps: 1-16, 17-32, 33-48, 49-64
 *   KNOB 1             the pattern's length, 1..64 steps (HOME held: its grid, 1/8 .. 1/32, triplets too)
 *   KNOB 2             the note of the step held (or the last one touched): a semitone a click; hats: closed / open
 *   KNOB 3             the pattern's swing, 50..75 % (a step held: that step nudged off its line, a tick a click)
 *   KNOB 4             a riddim template for the drums and bass (it lands on the next bar line); a step held: its gate
 * The session (4.8; Ableton Live's session view, the OP-Z, the RC-505): four patterns a channel, A..D, one playing.
 *   HOME + white 1-4   launch the channel's A..D on the line (the quantise; PLAY held: now), from its top
 *   HOME + white 9-12  launch A..D on every channel
 *   SAVE + white 1-4   copy the pattern into A..D
 *   HOME + OCT+ / OCT- double the pattern (its steps again after it) / clear it
 * The pattern edited is the one launched (at once, while the one playing plays on to the line).
 *   REC                the keyboard (P0.8): the bass and the lead, all 27 keys play notes in KEY's octave (under the
 *                      key lock, keys outside the set are silent); the drums, pads: white 1 kick, 2 rim, 3 closed hat,
 *                      4 open hat (each into its own channel); the skank and the organ, white 1-3 the lane's chord
 *                      low, middle, high. Stopped: each note at a cursor that moves on a step; REC again: the steps
 *                      again. Playing, live recording (4.8, the RC-505's way): into an empty pattern it waits for the
 *                      next bar line (the click counts in), records one pass of the pattern and loops; into one with
 *                      steps, it records at once, adding (REC MODE: OVERDUB) or clearing what the playhead passes
 *                      (REPLACE), until REC. A note keeps where it was played (REC QUANTISE: FEEL; HALF: halfway to
 *                      the grid; GRID: on it), less REC OFFSET's ms (the sound's way out); how hard: ghost, normal,
 *                      accent; the bass's and the lead's how long. A pass is one step of the undo history.
 *                      Started while stopped (the keyboard on), PLAY counts in a bar first. SEQ: the steps again
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
    /* live recording (4.8) */
    uint8_t armed, rec, first, joined;   /* waiting for the line; recording; a first pass (it stops); undo joined */
    uint8_t was_playing;
    int32_t rec_from, rec_end;           /* the pass's fine ticks: from its line; the first pass's end */
    int8_t held_ch[NKEYS];               /* the channel each held key plays */
    int8_t held_i[NKEYS];                /* the step it went into (-1 none), and when it was played */
    int32_t held_t[NKEYS];
    uint64_t done[NCH];                  /* REPLACE: the steps played this pass (kept) */
    int32_t swept[NCH];                  /* REPLACE: the last step position cleared */
} st = {CH_KICK, 0, -1, TPL_ONE_DROP, -2, 0, 0, 0, {0}, 0, 0, 0, 0, 0, 0, 0, 0, {0}, {0}, {0}, {0}, {0}};
static int32_t st_pend = -1;                     /* an on step pressed: off at its let-go, unless a knob edited it */
static uint8_t st_knobbed;                       /* (a knob turned while it was held) */

static int tonal(int32_t ch) { return ch == CH_BASS || ch == CH_LEAD; }   /* (the chord channels take voicings) */
static int drum(int32_t ch) { return ch == CH_KICK || ch == CH_SNARE || ch == CH_HATS; }
static int chordch(int32_t ch) { return ch == CH_SKANK || ch == CH_ORGAN; }
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

static const char *const GRID_NAME[GRID_N] = {"1/8", "1/8T", "1/16", "1/16T", "1/32"};
static const char PAT_L[] = "ABCD";

static int32_t pat_at(int32_t ch)        /* the step of the channel's pattern playing now, -1 none (or not this one) */
{
    int32_t f = eng_fine(), d, sl, len;
    uint32_t p = eng_pat((uint32_t)ch);
    pmeta_t m;
    if (f < 0 || p != pat_now(ch))
        return -1;
    m = ctl_meta_get((uint32_t)ch, p);
    sl = grid_fine(m.grid);
    len = m.len ? m.len : 16;
    d = f - eng_pat_org((uint32_t)ch);
    return d < 0 ? -1 : (d / sl) % len;
}

static int steps_skips(uint32_t i, step_t s)     /* 1: this step won't play this loop (its CONDITION; 5.9, 6.7) */
{
    int32_t f = eng_fine(), d, sl, len, k;
    uint32_t ch = (uint32_t)st.ch, p = eng_pat(ch), c = STEP_COND(s), loop;
    int pre = 1;
    pmeta_t m;
    if (c == COND_NONE || f < 0 || p != pat_now(st.ch))
        return 0;
    m = ctl_meta_get(ch, p);
    sl = grid_fine(m.grid);
    len = m.len ? m.len : 16;
    d = f - eng_pat_org(ch);
    if (d < 0)
        return 0;
    loop = (uint32_t)(d / sl / len);
    if (c == COND_PRE || c == COND_NOTPRE) {     /* PRE: the last conditional step before it in this pass, if any */
        for (k = (int32_t)i - 1; k >= 0; k--) {
            step_t q = ctl_get_step_at(ch, p, (uint32_t)k);
            uint32_t qc = STEP_COND(q);
            if (!LVL(q) || qc == COND_NONE)
                continue;
            if (qc == COND_PRE || qc == COND_NOTPRE)
                return 0;                        /* (a chain of PREs: not marked) */
            pre = cond_plays(ch, p, (uint32_t)k, loop, qc, m.rsv, ctl_get(P_FILL) != 0, 1);
            break;
        }
        if (k < 0 && loop)
            return 0;                            /* (it follows the pass before: not known here) */
    }
    return !cond_plays(ch, p, i, loop, c, m.rsv, ctl_get(P_FILL) != 0, pre);
}

static int pat_empty(uint32_t ch, uint32_t p)
{
    uint32_t i;
    for (i = 0; i < NSTEPS; i++)
        if (LVL(ctl_get_step_at(ch, p, i)))
            return 0;
    return 1;
}

static void pat_name(int32_t ch, uint32_t p, char *t)   /* "BASS B" */
{
    char l[2] = {PAT_L[p % NPAT], 0};
    t[0] = 0;
    text_cat(t, CH_NAME[ch]);
    text_cat(t, " ");
    text_cat(t, l);
}

static void pat_launch(int32_t ch, uint32_t p, int quiet)   /* on the line (PLAY held: now) */
{
    uint32_t beats = eng_playing() ? act_beats() : 0u;
    char t[24];
    ctl_cue(P_CH(ch, CHF_PAT), (int32_t)p, beats);
    set_dirty = 1;
    if (!quiet) {
        pat_name(ch, p, t);
        app_say(t, beats ? (pat_empty((uint32_t)ch, p) ? "EMPTY, ON THE LINE" : "ON THE LINE")
                         : pat_empty((uint32_t)ch, p) ? "EMPTY" : "");
    }
}

static void pat_copy(int32_t ch, uint32_t to)   /* the pattern edited into `to` */
{
    uint32_t from = pat_now(ch), i;
    char t[24], l[2] = {PAT_L[from], 0};
    if (from == to) {
        pat_name(ch, to, t);
        app_say(t, "IS THIS ONE");
        return;
    }
    for (i = 0; i < NSTEPS; i++)
        ctl_step_at((uint32_t)ch, to, i, ctl_get_step_at((uint32_t)ch, from, i));
    ctl_meta_set((uint32_t)ch, to, ctl_meta_get((uint32_t)ch, from));
    set_dirty = 1;
    pat_name(ch, to, t);
    text_cat(t, " FROM ");
    text_cat(t, l);
    app_say("COPIED", t);
}

static void pat_double(int32_t ch)       /* its steps again after it: twice as long */
{
    pmeta_t m = ctl_meta_now((uint32_t)ch);
    uint32_t i;
    char t[16];
    if (m.len * 2u > NSTEPS) {
        app_say("DOUBLE", "64 STEPS AT MOST");
        return;
    }
    for (i = 0; i < m.len; i++)
        ctl_step((uint32_t)ch, m.len + i, ctl_get_step((uint32_t)ch, i));
    m.len = (uint8_t)(m.len * 2u);
    ctl_meta_set((uint32_t)ch, pat_now(ch), m);
    set_dirty = 1;
    int_text(m.len, t);
    text_cat(t, " STEPS");
    app_say("DOUBLED", t);
}

static void pat_clear(int32_t ch)
{
    uint32_t i;
    char t[24];
    for (i = 0; i < NSTEPS; i++) {
        step_t z = {LVL_OFF, default_note((uint32_t)ch), 0, 0};
        ctl_step((uint32_t)ch, i, z);
    }
    set_dirty = 1;
    st.last = -1;
    pat_name(ch, pat_now(ch), t);
    app_say(t, "CLEARED");
}

static void gate_text(int32_t ch, step_t s, char *t)   /* "AUTO", or in steps: "1.5" */
{
    int32_t sl = grid_fine(ctl_meta_now((uint32_t)ch).grid), tenths = ((int32_t)s.gate * 40 + sl / 2) / sl;
    char n[8];
    t[0] = 0;
    if (!s.gate) {
        text_cat(t, "AUTO");
        return;
    }
    int_text(tenths / 10, n);
    text_cat(t, n);
    text_cat(t, ".");
    int_text(tenths % 10, n);
    text_cat(t, n);
}

static void nudge_text(step_t s, char *t)   /* "+12 MS" */
{
    int32_t ms = (int32_t)s.off * 2500 / ctl_get(P_TEMPO);   /* a tick: 60,000 / (240 x BPM) ms */
    char n[8];
    t[0] = 0;
    text_cat(t, s.off > 0 ? "+" : "");
    int_text(ms, n);
    text_cat(t, n);
    text_cat(t, " MS");
}

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
        STEP_LVL_SET(s, LVL(s) == want ? LVL_NORMAL : want);
    } else if (LVL(s)) {                 /* on: off when it's let go, unless a knob edits it meanwhile (hold to edit,
                                          * 5.8: a held step's locks, nudge and gate without losing it) */
        st_pend = (int32_t)i;
        st_knobbed = 0;
        st.last = (int32_t)i;
        return;
    } else {
        s.lvl = LVL_NORMAL;
        s.note = (int8_t)STEP_NOTE(s);   /* (a new step: no CONDITION, 6.7) */
        if (st.last >= 0 && st.ch != CH_HATS)   /* a new step takes the last note touched */
            s.note = (int8_t)STEP_NOTE(ctl_get_step((uint32_t)st.ch, (uint32_t)st.last));
    }
    ctl_step((uint32_t)st.ch, i, s);
    st.last = (int32_t)i;
    set_dirty = 1;
    int_text((int32_t)i + 1, n);
    t[0] = 0;
    text_cat(t, n);
    text_cat(t, " ");
    text_cat(t, LVL_NAME[LVL(s)]);
    app_say("STEP", t);
}

static void steps_letgo(uint32_t w)              /* white key w let go: an on step pressed and not edited: off */
{
    uint32_t i = st.page * 16u + w;
    step_t s;
    char t[24], n[8];
    if (st_pend != (int32_t)i)
        return;
    st_pend = -1;
    if (st_knobbed)
        return;
    s = ctl_get_step((uint32_t)st.ch, i);
    s.lvl = LVL_OFF;                             /* off: its CHANCE, REPEAT and locks too */
    if (is_drum((uint32_t)st.ch))
        s.gate = 0;
    ctl_step((uint32_t)st.ch, i, s);
    set_dirty = 1;
    int_text((int32_t)i + 1, n);
    t[0] = 0;
    text_cat(t, n);
    text_cat(t, " OFF");
    app_say("STEP", t);
}

static const char *const COND_NAME[COND_N] = {"ALWAYS", "75%", "50%", "25%", "1ST", "NOT 1ST", "1:2", "2:2", "1:4",
                                              "2:4", "3:4", "4:4", "PRE", "NOT PRE", "FILL", "NOT FILL"};
static void cond_say(uint32_t i, step_t s)
{
    char t[24], n[8];
    int_text((int32_t)i + 1, n);
    t[0] = 0;
    text_cat(t, n);
    text_cat(t, " ");
    text_cat(t, COND_NAME[STEP_COND(s)]);
    app_say("CONDITION", t);
}
static void steps_cond_turn(int32_t e)           /* black 11 held + KNOB 4: the last step's CONDITION, directly (6.7) */
{
    step_t s;
    int32_t c;
    if (st.last < 0)
        return;
    s = ctl_get_step((uint32_t)st.ch, (uint32_t)st.last);
    if (!LVL(s))
        return;
    c = clamp((int32_t)STEP_COND(s) + e, 0, (int32_t)COND_N - 1);
    STEP_COND_SET(s, c);
    ctl_step((uint32_t)st.ch, (uint32_t)st.last, s);
    set_dirty = 1;
    cond_say((uint32_t)st.last, s);
}
static void steps_cycle(uint32_t w, int repeat)  /* black 11 held + a white key: its CONDITION (5.9, 6.7); ENV held: its
                                                  * REPEAT (5.8, the drums) */
{
    static const char *const RN[REP_N] = {"OFF", "FLAM", "X2", "X3", "X4"};
    uint32_t i = st.page * 16u + w;
    step_t s = ctl_get_step((uint32_t)st.ch, i);
    char t[24], n[8];
    if (!LVL(s)) {
        app_say("STEP", "OFF: TURN IT ON FIRST");
        return;
    }
    if (repeat && !is_drum((uint32_t)st.ch)) {
        app_say("REPEAT", "THE DRUMS ONLY");
        return;
    }
    if (repeat)
        STEP_REPEAT_SET(s, (STEP_REPEAT(s) + 1u) % REP_N);
    else
        STEP_COND_SET(s, (STEP_COND(s) + 1u) % COND_N);
    ctl_step((uint32_t)st.ch, i, s);
    st.last = (int32_t)i;
    set_dirty = 1;
    if (!repeat) {
        cond_say(i, s);
        return;
    }
    int_text((int32_t)i + 1, n);
    t[0] = 0;
    text_cat(t, n);
    text_cat(t, " ");
    text_cat(t, RN[STEP_REPEAT(s)]);
    app_say("REPEAT", t);
}

static void steps_reseed(void)                   /* HOME + ENV: the pattern's SEED anew (5.9; one undo step) */
{
    pmeta_t m = ctl_meta_now((uint32_t)st.ch);
    char t[8];
    m.rsv = (uint8_t)(m.rsv + 1u + (plat_ms() * 2654435761u >> 24) % 254u);   /* (never the same one again) */
    ctl_meta_set((uint32_t)st.ch, pat_now(st.ch), m);
    set_dirty = 1;
    int_text(m.rsv, t);
    app_say("RESEED", t);
}

static int32_t rec_offset_fine(void)     /* REC OFFSET's ms in fine ticks: ms x 240 x BPM / 60,000 */
{
    return ctl_get(P_REC_OFFSET) * ctl_get(P_TEMPO) / 2500;
}

static int32_t rec_line(void)            /* the next bar line, fine ticks */
{
    int32_t f = eng_fine(), bar = (int32_t)PPQ * 4;
    return (f / bar + 1) * bar;
}

static void rec_say(const char *a, const char *b) { app_say(a, b); }

static void rec_start(int armed)         /* a pass: now, or from the next bar line */
{
    uint32_t ch;
    pmeta_t m = ctl_meta_get((uint32_t)st.ch, eng_pat((uint32_t)st.ch));
    st.first = (uint8_t)pat_empty((uint32_t)st.ch, eng_pat((uint32_t)st.ch));
    st.joined = 0;
    for (ch = 0; ch < NCH; ch++) {
        st.done[ch] = 0;
        st.swept[ch] = -0x7FFFFFFF;
    }
    if (armed) {
        st.armed = 1;
        st.rec = 0;
        st.rec_from = rec_line();
    } else {
        st.armed = 0;
        st.rec = 1;
        st.rec_from = eng_fine();
    }
    st.rec_end = st.rec_from + (int32_t)m.len * grid_fine(m.grid);
    if (armed)
        rec_say("REC", st.first ? "ON THE ONE: ONE PASS" : "ON THE ONE");
    else
        rec_say("REC", ctl_get(P_REC_MODE) == REC_REPLACE ? "REPLACE" : "OVERDUB");
}

static void kb_off(void)
{
    uint32_t k;
    for (k = 0; k < NKEYS; k++)
        if (st.held[k] >= 0)
            ctl_note((uint32_t)st.held_ch[k], st.held[k], 0);
    for (k = 0; k < NKEYS; k++)
        st.held[k] = -1;
    st.kb = 0;
    st.rec = st.armed = 0;
}

static void steps_rec(void)             /* REC: the keyboard and live recording */
{
    if (!eng_playing()) {                /* stopped: the keyboard at a cursor (and PLAY counts in a pass) */
        if (st.kb) {
            kb_off();
            app_say("KEYBOARD", "OFF");
            return;
        }
        st.kb = 1;
        st.cur = st.last >= 0 ? st.last : 0;
        st.was_playing = 0;
        app_say("KEYBOARD", "ON: PLAY COUNTS IN");
        return;
    }
    if (st.rec || st.armed) {            /* a pass ended by hand: the keyboard plays on */
        st.rec = st.armed = 0;
        app_say("REC", "OFF: REC AGAIN ADDS");
        return;
    }
    st.kb = 1;
    st.was_playing = 1;
    rec_start(pat_empty((uint32_t)st.ch, eng_pat((uint32_t)st.ch)));
}

static int32_t pad_ch(uint32_t k, int32_t *note)   /* a key's channel and note, -1 silent */
{
    uint32_t w;
    for (w = 0; w < 16u && WK[w] != k; w++)
        ;
    if (drum(st.ch)) {                   /* the pads: white 1 kick, 2 rim, 3 closed hat, 4 open hat */
        static const int8_t CH[4] = {CH_KICK, CH_SNARE, CH_HATS, CH_HATS}, NT[4] = {36, 37, 42, 46};
        if (w >= 4u)
            return -1;
        *note = NT[w];
        return CH[w];
    }
    if (chordch(st.ch)) {                /* the lane's chord: low, middle, high */
        if (w >= 3u)
            return -1;
        *note = (int32_t)w;
        return st.ch;
    }
    if (!tonal(st.ch))
        return -1;
    *note = kb_note(k);
    return note_ok(*note) ? st.ch : -1;  /* outside the lock's set: silent, never moved to a neighbour */
}

static uint8_t vel_lvl(uint32_t v)       /* how hard: ghost, normal, accent (no reading: normal) */
{
    return v == 0u ? LVL_NORMAL : v < 45u ? LVL_GHOST : v < 105u ? LVL_NORMAL : LVL_ACCENT;
}

static int32_t rec_note(int32_t ch, int32_t note, uint32_t vel, int32_t *t)   /* a note played: its step (-1 none) */
{
    uint32_t p = eng_pat((uint32_t)ch);
    pmeta_t m = ctl_meta_get((uint32_t)ch, p);
    int32_t sl = grid_fine(m.grid), len = m.len ? m.len : 16, rel, n, off, i, q = ctl_get(P_REC_QUANT);
    step_t s;
    *t = eng_fine() - rec_offset_fine();
    rel = *t - eng_pat_org((uint32_t)ch);
    n = rel >= -sl / 2 ? (rel + sl / 2) / sl : -((-rel + sl / 2 + sl - 1) / sl);   /* the nearest line */
    off = rel - n * sl;
    if (off >= sl / 2)
        off = sl / 2 - 1;                /* (within half a step: its own) */
    if (q == REC_GRID)
        off = 0;
    else if (q == REC_HALF)
        off /= 2;
    i = ((n % len) + len) % len;
    s = ctl_get_step_at((uint32_t)ch, p, (uint32_t)i);
    s.lvl = vel_lvl(vel);
    s.note = (int8_t)note;
    s.off = (int8_t)off;
    s.gate = 0;
    if (st.joined)
        undo_join();                     /* (a pass: one step to undo) */
    st.joined = 1;
    ctl_step_at((uint32_t)ch, p, (uint32_t)i, s);
    st.done[ch] |= (uint64_t)1 << i;
    st.recorded++;
    set_dirty = 1;
    return i;
}

static void rec_gate(int32_t ch, int32_t i, int32_t t0)   /* a held note let go: how long it was held */
{
    uint32_t p = eng_pat((uint32_t)ch);
    int32_t d = eng_fine() - rec_offset_fine() - t0;
    step_t s = ctl_get_step_at((uint32_t)ch, p, (uint32_t)i);
    if (!tonal(ch) || i < 0)
        return;
    s.gate = (uint8_t)clamp((d + 2) / 4, 1, 255);
    undo_join();
    ctl_step_at((uint32_t)ch, p, (uint32_t)i, s);
}

static void kb_keys(uint32_t keys, uint32_t prev)   /* the keyboard: play, and record */
{
    uint32_t k;
    char t[16], n[8];
    for (k = 0; k < NKEYS; k++) {
        uint32_t bit = 1u << k;
        if ((keys & bit) && !(prev & bit)) {
            int32_t note = 0, ch = pad_ch(k, &note), idx, tt = 0;
            uint32_t vel = touch_vel(k);
            if (ch < 0)
                continue;
            st.held[k] = (int8_t)note;
            st.held_ch[k] = (int8_t)ch;
            st.held_i[k] = -1;
            ctl_note((uint32_t)ch, note, vel ? vel : 100u);
            if (eng_playing()) {
                if (st.armed && eng_fine() - rec_offset_fine()
                                    >= st.rec_from - grid_fine(ctl_meta_now((uint32_t)st.ch).grid) / 2) {
                    st.armed = 0;                /* (played early onto the line: the pass has begun) */
                    st.rec = 1;
                }
                if (!st.rec)
                    continue;                    /* (playing along, or counting in) */
                idx = rec_note(ch, note, vel, &tt);
                st.held_i[k] = (int8_t)idx;
                st.held_t[k] = tt;
                continue;
            }
            {                                    /* stopped: at the cursor, which moves on */
                step_t s;
                int32_t len = pat_len(ch);
                idx = st.cur % len;
                st.cur = (idx + 1) % len;
                s = ctl_get_step((uint32_t)ch, (uint32_t)idx);
                s.lvl = vel_lvl(vel);
                s.note = (int8_t)note;
                s.off = 0;
                s.gate = 0;
                ctl_step((uint32_t)ch, (uint32_t)idx, s);
                st.last = idx;
                st.recorded++;
                set_dirty = 1;
                note_text(ch, note, t);
                int_text(idx + 1, n);
                text_cat(t, " STEP ");
                text_cat(t, n);
                app_say("NOTE", t);
            }
        } else if (!(keys & bit) && (prev & bit) && st.held[k] >= 0) {
            ctl_note((uint32_t)st.held_ch[k], st.held[k], 0);
            if (st.held_i[k] >= 0)
                rec_gate(st.held_ch[k], st.held_i[k], st.held_t[k]);
            st.held[k] = -1;
        }
    }
}

static void rec_tick(void)               /* every frame: the pass's line, its end, REPLACE's sweep, the click */
{
    int32_t f = eng_fine();
    uint32_t want = 0;
    if (!eng_playing()) {
        if (st.rec || st.armed)
            st.rec = st.armed = 0;
        st.was_playing = 0;
    } else {
        if (st.kb && !st.was_playing) {  /* PLAY with the keyboard on: a bar's count-in, then the pass */
            st.was_playing = 1;
            rec_start(1);
        }
        if (st.armed && f >= st.rec_from) {
            st.armed = 0;
            st.rec = 1;
            app_say("RECORDING", "");
        }
        if (st.rec && st.first && f >= st.rec_end) {
            st.rec = 0;
            app_say("LOOP RECORDED", "REC AGAIN ADDS");
        }
        if (st.rec && ctl_get(P_REC_MODE) == REC_REPLACE) {   /* what the playhead passes, cleared (not this pass's) */
            int32_t chs[4], nc = 0, c;
            if (drum(st.ch)) {
                chs[nc++] = CH_KICK;
                chs[nc++] = CH_SNARE;
                chs[nc++] = CH_HATS;
            } else
                chs[nc++] = st.ch;
            for (c = 0; c < nc; c++) {
                int32_t ch = chs[c];
                uint32_t p = eng_pat((uint32_t)ch);
                pmeta_t m = ctl_meta_get((uint32_t)ch, p);
                int32_t sl = grid_fine(m.grid), len = m.len ? m.len : 16, pos = (f - eng_pat_org((uint32_t)ch)) / sl;
                if (st.swept[ch] == -0x7FFFFFFF)
                    st.swept[ch] = pos - 1;
                while (st.swept[ch] < pos) {
                    int32_t i = ((++st.swept[ch] % len) + len) % len;
                    step_t s = ctl_get_step_at((uint32_t)ch, p, (uint32_t)i);
                    if (LVL(s) && !(st.done[ch] >> i & 1u)) {
                        s.lvl = LVL_OFF;
                        if (st.joined)
                            undo_join();
                        else
                            undo_mark(0xFFFF0000u, plat_ms());   /* (the pass's first change: a step of its own) */
                        st.joined = 1;
                        ctl_step_at((uint32_t)ch, p, (uint32_t)i, s);
                        set_dirty = 1;
                    }
                }
            }
        }
        want = (st.armed && ctl_get(P_REC_CLICK) != CLICK_OFF)
               || (st.rec && ctl_get(P_REC_CLICK) == CLICK_RECORDING);
    }
    if ((uint32_t)ctl_get(P_CLICK) != want)
        ctl_set(P_CLICK, (int32_t)want);
}

static void steps_keys(uint32_t keys, uint32_t prev)
{
    uint32_t k;
    if (st.kb && !(dub.btn >> B_HOME & 1u) && !(dub.btn >> B_SAVE & 1u)) {   /* (HOME or SAVE held: the session's) */
        kb_keys(keys, prev);
        return;
    }
    for (k = 0; k < 16u; k++)
        if (!(keys >> WK[k] & 1u) && (prev >> WK[k] & 1u))
            steps_letgo(k);
    for (k = 0; k < 16u; k++)
        if ((keys >> WK[k] & 1u) && !(prev >> WK[k] & 1u)) {
            if ((dub.btn >> B_HOME & 1u) && (k < NPAT || (k >= 8u && k < 8u + NPAT))) {   /* the session's launches */
                dub.home_used = 1;
                if (k < NPAT)
                    pat_launch(st.ch, k, 0);
                else {
                    uint32_t ch;
                    char t[16] = "ALL ";
                    char l[2] = {PAT_L[k - 8u], 0};
                    for (ch = 0; ch < NCH; ch++)
                        pat_launch((int32_t)ch, k - 8u, 1);
                    text_cat(t, l);
                    app_say(t, eng_playing() && q_beats() ? "ON THE LINE" : "");
                }
            } else if ((dub.btn >> B_SAVE & 1u) && k < NPAT) {
                dub.save_used = 1;
                pat_copy(st.ch, k);
            } else if ((dub.btn >> B_ENV & 1u) && !st.kb) {   /* ENV held + a white key: its REPEAT (5.8) */
                dc.env_used = 1;
                steps_cycle(k, 1);
            } else if ((keys >> BK[10] & 1u) && !st.kb)   /* black 11 held + a white key: its CHANCE (5.9) */
                steps_cycle(k, 0);
            else if (!(dub.btn >> B_HOME & 1u) && !(dub.btn >> B_SAVE & 1u) && !st.kb)
                steps_key(k);
        }
    if (st.kb)
        return;                          /* (the keyboard's: the channel stays) */
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
    if (dub.btn >> B_HOME & 1u) {        /* HOME + OCT+ / OCT-: double the pattern / clear it */
        dub.home_used = 1;
        if (d > 0)
            pat_double(st.ch);
        else
            pat_clear(st.ch);
        return;
    }
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
    if (i == 3u && (dub.keys >> BK[10] & 1u) && !st.kb) {   /* black 11 held + KNOB 4: the CONDITION (6.7) */
        steps_cond_turn(e);
        return;
    }
    if (steps_held() >= 0)
        st_knobbed = 1;                          /* (the held step was edited: it stays on) */
    if (is_drum((uint32_t)st.ch) && steps_held() >= 0
        && (i == 0u || (i == 1u && st.ch != CH_HATS) || i == 3u)) {   /* a drum step held: its locks (5.8) */
        int32_t k = steps_held(), tu, de;
        step_t s = ctl_get_step((uint32_t)st.ch, (uint32_t)k);
        if (i == 3u) {
            app_say("GATE", "NOT FOR A DRUM");
            return;
        }
        tu = lock_tune(s);
        de = lock_decay(s);
        if (i == 1u)
            tu = clamp(tu + e, -8, 7);
        else
            de = clamp(de + e, -8, 7);
        s.gate = locks_pack(tu, de);
        ctl_step((uint32_t)st.ch, (uint32_t)k, s);
        st.last = k;
        set_dirty = 1;
        int_text(i == 1u ? tu : de, t);
        app_say(i == 1u ? "TUNE LOCK" : "DECAY LOCK", t);
        return;
    }
    if (i == 0u && (dub.btn >> B_HOME & 1u)) {   /* HOME + KNOB 1: the grid */
        pmeta_t m = ctl_meta_now((uint32_t)st.ch);
        dub.home_used = 1;
        m.grid = (uint8_t)clamp((int32_t)m.grid + e, 0, GRID_N - 1);
        ctl_meta_set((uint32_t)st.ch, pat_now(st.ch), m);
        set_dirty = 1;
        app_say("GRID", GRID_NAME[m.grid]);
    } else if (i == 0u) {                /* the length */
        pat_len_set(st.ch, steps_len() + e);
        set_dirty = 1;
        int_text(steps_len(), t);
        app_say("LENGTH", t);
    } else if ((i == 2u || i == 3u) && steps_held() >= 0) {   /* the step held: nudged, or its gate */
        int32_t k = steps_held(), sl = grid_fine(ctl_meta_now((uint32_t)st.ch).grid);
        step_t s = ctl_get_step((uint32_t)st.ch, (uint32_t)k);
        if (i == 2u) {
            s.off = (int8_t)clamp((int32_t)s.off + e, -(sl / 2), sl / 2);
            nudge_text(s, t);
        } else {
            s.gate = (uint8_t)clamp((int32_t)s.gate + 3 * e, 0, 255);
            gate_text(st.ch, s, t);
        }
        ctl_step((uint32_t)st.ch, (uint32_t)k, s);
        st.last = k;
        set_dirty = 1;
        app_say(i == 2u ? "NUDGE" : "GATE", t);
    } else if (i == 2u) {                /* the swing */
        pmeta_t m = ctl_meta_now((uint32_t)st.ch);
        m.swing = (uint8_t)clamp((int32_t)m.swing + e, SWING_MIN, SWING_MAX);
        ctl_meta_set((uint32_t)st.ch, pat_now(st.ch), m);
        set_dirty = 1;
        int_text(m.swing, t);
        text_cat(t, " %");
        app_say("SWING", t);
    } else if (i == 1u) {                /* the note of the step held, or the last touched */
        int32_t k = steps_held() >= 0 ? steps_held() : st.last;
        step_t s;
        if (k < 0) {
            app_say("NOTE", "TOUCH A STEP");
            return;
        }
        s = ctl_get_step((uint32_t)st.ch, (uint32_t)k);
        if (st.ch == CH_HATS)
            STEP_NOTE_SET(s, STEP_NOTE(s) == 46 ? 42 : 46);
        else if (st.ch == CH_SKANK || st.ch == CH_ORGAN)
            STEP_NOTE_SET(s, (STEP_NOTE(s) + e % VOICING_N + VOICING_N) % VOICING_N);
        else if (st.ch == CH_KICK || st.ch == CH_SNARE) {
            app_say("NOTE", "DRUM");
            return;
        }
        else {                           /* a semitone a click, over notes the lock does not allow */
            int32_t nn = STEP_NOTE(s), d = e > 0 ? 1 : -1, left = e > 0 ? e : -e, guard = 0;
            while (left > 0 && guard++ < 48) {
                nn = clamp(nn + d, 24, 96);
                if (note_ok(nn))
                    left--;
            }
            STEP_NOTE_SET(s, nn);
        }
        ctl_step((uint32_t)st.ch, (uint32_t)k, s);
        st.last = k;
        set_dirty = 1;
        note_text(st.ch, STEP_NOTE(s), t);
        app_say("NOTE", t);
    } else if (i == 3u) {                /* a template: the drums and the bass, on the next bar line */
        st.tpl = (st.tpl + e % TPL_N + TPL_N) % TPL_N;
        ctl_template(st.tpl, (1u << CH_KICK) | (1u << CH_SNARE) | (1u << CH_HATS) | (1u << CH_BASS)
                                 | (1u << CH_SKANK) | (1u << CH_ORGAN), 4u);
        set_dirty = 1;
        app_say("RIDDIM", roots_name(template_name(st.tpl)));
    }
}

static uint32_t steps_lights(uint32_t *dim)   /* the keys: steps on bright, off dim; the playhead blinks off */
{
    uint32_t k, bright = 0;
    if (st.kb && (drum(st.ch) || chordch(st.ch))) {   /* the pads: lit, and (recording) the first one bright */
        for (k = 0; k < (drum(st.ch) ? 4u : 3u); k++)
            *dim |= 1u << WK[k];
        return dub.keys | (st.rec && blink_on() ? 1u << WK[0] : 0u);
    }
    if (st.kb)                           /* the keyboard: the lock's set, and what is held */
        return kb_lights(dim) | dub.keys;
    if (dub.btn >> B_HOME & 1u) {        /* HOME held: the patterns (1-4 this channel's: playing bright, the others
                                          * with steps dim; 9-12 every channel's) */
        for (k = 0; k < NPAT; k++) {
            if (k == pat_now(st.ch))
                bright |= 1u << WK[k];
            else if (!pat_empty((uint32_t)st.ch, k))
                *dim |= 1u << WK[k];
            *dim |= 1u << WK[8u + k];
        }
        return bright;
    }
    int32_t len = steps_len(), at = pat_at(st.ch);
    for (k = 0; k < 16u; k++) {
        uint32_t i = st.page * 16u + k, bit = 1u << WK[k];
        int on = LVL(ctl_get_step((uint32_t)st.ch, i)) != LVL_OFF;
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
    uint32_t h = (uint32_t)st.ch * 7u + st.page * 131u + (uint32_t)(steps_held() + 2) * 7919u + st.rec * 13u
                 + st.armed * 17u + st.kb * 19u
                 + (dub.btn >> B_HOME & 1u) * 104729u, ch, i;
    for (ch = 0; ch < NCH; ch++) {
        h = h * 31u + (uint32_t)pmeta_pack(ctl_meta_now(ch)) + pat_now((int32_t)ch) * 977u + eng_pat(ch) * 3571u
            + (uint32_t)(pat_at((int32_t)ch) + 1) * 1031u;
        for (i = 0; i < 16u; i++) {
            step_t s = ctl_get_step(ch, st.page * 16u + i);
            h = h * 33u + (uint32_t)step_pack(s);
        }
    }
    return h;
}

static void steps_draw_grid(void)        /* in the strips' place: every channel's page of 16 */
{
    uint32_t ch, i;
    cv_begin(240, STRIP_H, C_BLACK);
    for (ch = 0; ch < NCH; ch++) {
        int32_t y = (int32_t)ch * 12, len = pat_len((int32_t)ch), at = pat_at((int32_t)ch);
        char l[2] = {PAT_L[pat_now((int32_t)ch)], 0};
        cv_text(2, y, &FONT_T, CH_SHORT[ch], (int32_t)ch == st.ch ? C_WHITE : C_GRAY);
        cv_text(24, y, &FONT_T, l, eng_pat(ch) == pat_now((int32_t)ch) ? C_HI : C_AMB);   /* (amber: launched, to come) */
        for (i = 0; i < 16u; i++) {
            int32_t idx = (int32_t)(st.page * 16u + i), x = 34 + (int32_t)i * 13;
            step_t s = ctl_get_step(ch, (uint32_t)idx);
            uint16_t c = LVL(s) == LVL_ACCENT ? C_HI : LVL(s) == LVL_NORMAL ? C_AMB : LVL(s) == LVL_GHOST ? C_GRAY : C_LINE;
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
    int32_t len = steps_len(), at = pat_at(st.ch), spb = (int32_t)PPQ / grid_fine(ctl_meta_now((uint32_t)st.ch).grid);
    char t[40], n[8];
    cv_begin(240, CENTRE_H, C_BLACK);
    cv_rect(0, 0, 240, 1, C_LINE);
    pat_name(st.ch, pat_now(st.ch), t);
    text_cat(t, "  ");
    text_cat(t, GRID_NAME[ctl_meta_now((uint32_t)st.ch).grid]);
    text_cat(t, st.rec ? "  REC " : st.armed ? "  ARMED " : st.kb ? "  KEYS " : "  STEPS ");
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
        h = LVL(s) == LVL_ACCENT ? 28 : LVL(s) == LVL_NORMAL ? 20 : LVL(s) == LVL_GHOST ? 10 : 2;
        if (LVL(s) && steps_skips((uint32_t)idx, s)) {   /* (5.9: it won't play this loop: hollow) */
            cv_rect(x, 54 - h, 12, 1, C_GRAY);
            cv_rect(x, 53, 12, 1, C_GRAY);
            cv_rect(x, 54 - h, 1, h, C_GRAY);
            cv_rect(x + 11, 54 - h, 1, h, C_GRAY);
        } else
            cv_rect(x, 54 - h, 12, h, LVL(s) ? (idx == st.last ? C_WHITE : C_HI) : C_LINE);
        if (LVL(s) && STEP_COND(s)) {            /* its CONDITION (CHANCE), above it */
            static const char *const CP[COND_N] = {"", "75", "50", "25", "1S", "!1", "12", "22", "14", "24", "34", "44",
                                                   "PR", "!P", "FL", "!F"};
            cv_text(x, 14, &FONT_T, CP[STEP_COND(s)], STEP_COND(s) < COND_1ST ? C_AMB : C_HI);
        }
        if (LVL(s) && STEP_REPEAT(s)) {          /* its REPEAT: F, or a dot a hit */
            uint32_t r = STEP_REPEAT(s), q;
            if (r == REP_FLAM)
                cv_text(x + 3, 44 - h, &FONT_T, "F", C_WHITE);
            else
                for (q = 0; q < r; q++)
                    cv_rect(x + 1 + (int32_t)q * 3, 50 - h, 2, 2, C_WHITE);
        }
        if (LVL(s) && is_drum((uint32_t)st.ch) && s.gate)   /* a lock: a white tick at its foot */
            cv_rect(x + 4, 52, 4, 2, C_WHITE);
        if (idx == at)
            cv_rect(x, 57, 12, 2, C_WHITE);
        if (LVL(s) && (st.ch == CH_BASS || st.ch == CH_HATS || st.ch == CH_SKANK || st.ch == CH_ORGAN)) {
            note_text(st.ch, STEP_NOTE(s), t);
            cv_text(x, 60, &FONT_T, t, C_GRAY);
        }
        if (idx % spb == 0)
            cv_rect(x - 1, 22, 1, 34, C_DIM);   /* the beats (a triplet grid: every 6) */
        if (LVL(s) && s.off)             /* played or nudged off its line: a tick that way */
            cv_rect(x + 6 + s.off * 6 / (grid_fine(ctl_meta_now((uint32_t)st.ch).grid) / 2), 20, 1, 2, C_WHITE);
    }
    cv_blit(0, CENTRE_Y);
}

static void steps_draw_footer(void)
{
    static const char *const L[3][4] = {{"LEN", "NOTE", "SWING", "RIDDIM"}, {"GRID", "NOTE", "SWING", "RIDDIM"},
                                        {"LEN", "NOTE", "NUDGE", "GATE"}};
    uint32_t i, mode = steps_held() >= 0 ? 2u : (dub.btn >> B_HOME & 1u) ? 1u : 0u;
    int32_t k = steps_held() >= 0 ? steps_held() : st.last;
    pmeta_t m = ctl_meta_now((uint32_t)st.ch);
    char t[20];
    cv_begin(240, FOOT_H, C_BLACK);
    cv_rect(0, 0, 240, 1, C_LINE);
    for (i = 0; i < 4u; i++) {
        int32_t x = 6 + (int32_t)i * 59;
        cv_text(x, 4, &FONT_XS, L[mode][i], C_GRAY);
        t[0] = 0;
        if (i == 0u && mode == 1u)
            text_cat(t, GRID_NAME[m.grid]);
        else if (i == 0u)
            int_text(steps_len(), t);
        else if (i == 1u && k >= 0)
            note_text(st.ch, STEP_NOTE(ctl_get_step((uint32_t)st.ch, (uint32_t)k)), t);
        else if (i == 2u && mode == 2u)
            nudge_text(ctl_get_step((uint32_t)st.ch, (uint32_t)k), t);
        else if (i == 2u) {
            int_text(m.swing, t);
            text_cat(t, "%");
        } else if (i == 3u && mode == 2u)
            gate_text(st.ch, ctl_get_step((uint32_t)st.ch, (uint32_t)k), t);
        else if (i == 3u)
            text_cat(t, template_name(st.tpl));
        cv_text(x, 18, i == 3u || (i == 2u && mode == 2u) ? &FONT_XS : &FONT_B, t[0] ? t : "--", C_HI);
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
    {
        char l[4] = {' ', PAT_L[pat_now(st.ch)], 0, 0};
        text_cat(t, l);
    }
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
        *got = (int)LVL(s);
    } else if ((!strncmp(what, "chance", 6) || !strncmp(what, "repeat", 6)) && what[6] >= '1' && what[6] <= '8'
               && what[7] == '_') {              /* chanceC_N (0 100 %, 1 75, 2 50, 3 25), repeatC_N (5.8, 5.9) */
        step_t s = ctl_get_step((uint32_t)(what[6] - '1'), (uint32_t)(atoi(what + 8) - 1));
        *got = what[0] == 'c' ? (int)STEP_COND(s) : (int)STEP_REPEAT(s);   /* (6.7: chance is the CONDITION, 0..15) */
    } else if ((!strncmp(what, "lockt", 5) || !strncmp(what, "lockd", 5)) && what[5] >= '1' && what[5] <= '8'
               && what[6] == '_') {              /* locktC_N, lockdC_N: a drum step's TUNE, DECAY lock */
        step_t s = ctl_get_step((uint32_t)(what[5] - '1'), (uint32_t)(atoi(what + 7) - 1));
        *got = what[4] == 't' ? (int)lock_tune(s) : (int)lock_decay(s);
    } else if (!strcmp(what, "seed")) {         /* the edited channel's pattern's SEED (5.9) */
        *got = ctl_meta_now((uint32_t)st.ch).rsv;
    } else if (!strncmp(what, "cond", 4) && what[4] >= '1' && what[4] <= '8' && what[5] == '_') {   /* condC_N (6.7) */
        *got = (int)STEP_COND(ctl_get_step((uint32_t)(what[4] - '1'), (uint32_t)(atoi(what + 6) - 1)));
    } else if (!strncmp(what, "note", 4) && what[4] >= '1' && what[4] <= '8' && what[5] == '_')
        *got = STEP_NOTE(ctl_get_step((uint32_t)(what[4] - '1'), (uint32_t)(atoi(what + 6) - 1)));
    else if (!strcmp(what, "kb"))
        *got = st.kb;
    else if (!strcmp(what, "fine"))      /* the engine's fine ticks since PLAY, -1 stopped */
        *got = eng_fine();
    else if (!strcmp(what, "rec"))       /* 0 off, 1 armed (waiting for the line), 2 recording */
        *got = st.rec ? 2 : st.armed ? 1 : 0;
    else if (!strncmp(what, "off", 3) && what[3] >= '1' && what[3] <= '8' && what[4] == '_')   /* offC_N: its offset */
        *got = ctl_get_step((uint32_t)(what[3] - '1'), (uint32_t)(atoi(what + 5) - 1)).off;
    else if (!strncmp(what, "gate", 4) && what[4] >= '1' && what[4] <= '8' && what[5] == '_')
        *got = ctl_get_step((uint32_t)(what[4] - '1'), (uint32_t)(atoi(what + 6) - 1)).gate;
    else if (!strcmp(what, "pat_at"))    /* the edited channel's playhead step (from 1), 0 none */
        *got = pat_at(st.ch) + 1;
    else if (!strcmp(what, "recorded"))
        *got = (int)st.recorded;
    else if (!strncmp(what, "eng_step", 8) && what[8] >= '1' && what[8] <= '8' && what[9] == '_')
        *got = (int)LVL(eng_step((uint32_t)(what[8] - '1'), (uint32_t)(atoi(what + 10) - 1)));
    else
        return 0;
    return 1;
}
#endif
