/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* Help cards and tap tempo (the 2026-11 QoL review 3; interface spec 4.6 and 5: "hold any button for help").
 *
 * HELP CARDS: a button held on its own for a second (no other button, key or knob with it) shows a card of what it
 * does on this page, in the page's place (the header stays), and a screen reader hears it. The card stays after the
 * button is let go, so it can be read, until the next press or turn, or 12 s. The press that closes it is swallowed
 * once the button has been let go; while the button is still held it goes through (a combo held a while to think,
 * HOME + SELECT or PLAY + a white key, still works). Letting go of the button that showed a card does nothing more
 * (HOME doesn't flip the page, PLAY doesn't start, SAVE doesn't save). Cost: UI only, flash ~4 KB of text, RAM ~40 B.
 *
 * TAP TEMPO: HOME held, SEQ tapped. From the second tap, the tempo is the average of the last (up to) four gaps;
 * a gap over 2 s starts again. The taps are one step of the undo history. With MIDI clock in, the clock has the
 * tempo: the taps are refused. Part of the unity build: dub.c includes it. */

#define HELP_HOLD_MS 1000u
#define HELP_SHOW_MS 12000u
#define HELP_NONE 0xFFu
#define HELP_LINES 9

typedef struct {
    uint8_t btn, page;                   /* the button; the page it is for (HELP_NONE: any page) */
    const char *title;
    const char *line[HELP_LINES];
} help_card_t;

static const help_card_t HELP[] = {
    {B_HOME, HELP_NONE, "HOME",
     {"TAP: DUB <-> ARRANGE.", "HELD, IT SHIFTS:", "+ SELECT: TEMPO", "+ SEQ, TAPPED: TAP TEMPO", "+ REC: UNDO.  + GLO: REDO",
      "+ PLAY: REWIND, COME AGAIN", "+ ARP: KEY LOCK ON / OFF", "+ WHITE 1-8: SOLO", "+ BLACK 8-10: BACKWARDS"}},
    {B_PLAY, HELP_NONE, "PLAY",
     {"TAP: START / STOP (ON LET GO).", "HELD + A WHITE KEY: THAT CHANGE", "NOW, NOT ON THE LINE.",
      "HOME + PLAY: THE REWIND.", "THE SPIN-BACK, THEN BAR 1."}},
    {B_REC, PAGE_STEPS, "REC  (STEPS)",
     {"PLAYING: RECORD AS YOU PLAY.", "EMPTY PATTERN: FROM THE ONE,", "ONE PASS, THEN IT LOOPS.",
      "ELSE AT ONCE, UNTIL REC.", "DRUMS: WHITE 1-4 ARE PADS.", "STOPPED: A STEP A NOTE;",
      "THEN PLAY COUNTS IN A BAR.", "SEQ: BACK TO THE STEPS.", "SETUP: REC QUANTISE, MODE."}},
    {B_REC, HELP_NONE, "REC",
     {"SONG CAPTURE ON / OFF:", "PLAY, AND EVERY BAR AND", "MOVE IS KEPT AS THE SONG.", "HOME + REC: UNDO.",
      "HOME + GLO: REDO.", "SAVE + REC: CLEAR THE MOVES."}},
    {B_SAVE, PAGE_ARRANGE, "SAVE  (ARRANGE)",
     {"TAP: SAVE THE PROJECT.", "HOLD + WHITE 1-16: STORE THE", "DESK IN THAT SCENE",
      "(K4: WITH THE PATTERNS).", "SAVE + REC: CLEAR THE MOVES."}},
    {B_SAVE, PAGE_SETUP, "SAVE  (SETUP)",
     {"ON SAVE TO SLOT / LOAD SLOT:", "DOES IT (K2 PICKS THE SLOT).", "ELSEWHERE: SAVE THE PROJECT."}},
    {B_SAVE, HELP_NONE, "SAVE",
     {"TAP: SAVE THE PROJECT.", "LIT: CHANGES NOT SAVED YET.", "AUTOSAVE (SETUP) SAVES BY", "ITSELF WHEN STOPPED.",
      "SAVE + REC: CLEAR THE MOVES."}},
    {B_SEQ, HELP_NONE, "SEQ",
     {"STEPS: FOUR PATTERNS A CHANNEL.", "BLACK 1-8: THE CHANNEL.", "WHITE 1-16: STEPS ON / OFF.",
      "HOME + WHITE 1-4: PLAY A-D.", "SAVE + WHITE 1-4: COPY TO A-D.", "K1 LENGTH (HOME: GRID).",
      "K2 NOTE, K3 SWING, K4 RIDDIM.", "A STEP HELD: K3 NUDGE, K4 GATE.", "HOME + SEQ, TAPPED: TAP TEMPO."}},
    {B_ARP, HELP_NONE, "ARP",
     {"KEY & CHORDS: KEY, SCALE,", "LOCK, OCTAVE.", "ARP AGAIN: THE CHORD LANE.", "HOME + ARP: LOCK ON / OFF."}},
    {B_FX, HELP_NONE, "FX",
     {"THE EFFECTS, A BANK AT A TIME:", "ECHO, SPRING, CROSS, TAPE,", "PHASER, DRIVE, GATE, WIDTH,",
      "BI-PHASE. FX AGAIN: NEXT BANK.", "K1-K4: THE BANK'S FOUR."}},
    {B_EDIT, HELP_NONE, "EDIT",
     {"THE FOCUSED CHANNEL'S SOUND,", "FOUR KNOBS A BANK.", "EDIT AGAIN: NEXT BANK, THEN", "BACK TO THE ECHO KNOBS.",
      "ALGORITHM: THE FOCUS."}},
    {B_GLO, HELP_NONE, "GLO",
     {"SETUP: RUNWAY, SAVER, CLOCK,", "SLOTS, AUTOSAVE, DEMO ...", "K1 / SELECT: THE ROW.", "K2 / PRESET: ITS VALUE.",
      "HOME + GLO: REDO."}},
    {B_OCTDN, PAGE_STEPS, "OCT- / OCT+  (STEPS)", {"THE PAGE OF 16 STEPS.", "HOME + OCT+: DOUBLE IT.", "HOME + OCT-: CLEAR IT."}},
    {B_OCTUP, PAGE_STEPS, "OCT- / OCT+  (STEPS)", {"THE PAGE OF 16 STEPS.", "HOME + OCT+: DOUBLE IT.", "HOME + OCT-: CLEAR IT."}},
    {B_SAVE, PAGE_STEPS, "SAVE  (STEPS)",
     {"TAP: SAVE THE PROJECT.", "SAVE + WHITE 1-4: COPY THE", "PATTERN INTO A-D."}},
    {B_OCTDN, HELP_NONE, "OCT- / OCT+",
     {"QUANTISE: WHERE A CHANGE LANDS.", "BEAT, 1-32 BARS, CYCLE.", "THE RUNWAY COUNTS DOWN TO IT."}},
    {B_OCTUP, HELP_NONE, "OCT- / OCT+",
     {"QUANTISE: WHERE A CHANGE LANDS.", "BEAT, 1-32 BARS, CYCLE.", "THE RUNWAY COUNTS DOWN TO IT."}},
    {B_SEL, HELP_NONE, "SELECT",
     {"TURN: THE BIG KNOB, A STEPPED", "HIGH-PASS (10, 20 OR 40", "STEPS: SETUP).", "HOME + TURN: TEMPO.",
      "IN SETUP: THE ROW."}},
    {B_ENV, HELP_NONE, "ENV", {"NO JOB ON DUBWISE YET."}},
    {B_LFO, HELP_NONE, "LFO", {"NO JOB ON DUBWISE YET."}},
};

static struct {
    uint8_t k, up, drawn, ate;           /* held alone (timing); a card up; drawn; its button's let-go swallowed */
    uint8_t card;                        /* HELP[] */
    uint32_t t, shown;
    uint32_t kswallow, bswallow;         /* presses that closed a card: kept from the page till let go */
} help = {HELP_NONE, 0, 0, HELP_NONE, 0, 0, 0, 0, 0};

static int help_find(uint32_t k)
{
    uint32_t i, any = HELP_NONE;
    for (i = 0; i < sizeof HELP / sizeof HELP[0]; i++)
        if (HELP[i].btn == k) {
            if (HELP[i].page == dub.page)
                return (int)i;
            if (HELP[i].page == HELP_NONE && any == HELP_NONE)
                any = i;
        }
    return any == HELP_NONE ? -1 : (int)any;
}

static void help_close(void)
{
    if (!help.up)
        return;
    help.up = 0;
    dub_init();
    dub.page_drawn = 0xFFu;              /* the page again, all of it */
}

static void help_show(uint32_t k, uint32_t now)
{
    int c = help_find(k);
    uint32_t i;
    if (c < 0)
        return;
    help.card = (uint8_t)c;
    help.up = 1;
    help.drawn = 0;
    help.shown = now;
    help.ate = (uint8_t)k;
    if (ann_on()) {                      /* the card, to a screen reader */
        char t[320];
        t[0] = 0;
        text_cat(t, HELP[c].title);
        for (i = 0; i < HELP_LINES && HELP[c].line[i]; i++) {
            text_cat(t, ". ");
            text_cat(t, HELP[c].line[i]);
        }
        ann_say(t, 0, ANN_NOW);
    }
}

static void help_turned(void)            /* a knob turned: not a hold on its own; a card closes (the turn goes on) */
{
    help.k = HELP_NONE;
    help.ate = HELP_NONE;
    help_close();
}

/* dub_input: 1 = nothing else to do this frame. Called with the hands' keys and buttons before the page sees them */
static int help_input(uint32_t *keys, uint32_t *btn, uint32_t now)
{
    uint32_t kp = *keys & ~dub.keys, bp = *btn & ~dub.btn;
    if (help.up && (kp || bp)) {         /* a press closes the card */
        int held = help.ate != HELP_NONE && (*btn >> help.ate & 1u);
        help_close();
        help.ate = HELP_NONE;
        if (!held) {                     /* the card's button let go: the press only closed it */
            help.kswallow |= kp;
            help.bswallow |= bp;
        }
    } else if (help.up && now - help.shown >= HELP_SHOW_MS) {
        help_close();
    }
    help.kswallow &= *keys;
    help.bswallow &= *btn;
    *keys &= ~help.kswallow;
    *btn &= ~help.bswallow;
    kp = *keys & ~dub.keys;
    bp = *btn & ~dub.btn;
    if (bp && !kp && *btn == bp && !*keys && !(bp & (bp - 1u))) {   /* one button, on its own */
        uint32_t k = 0;
        while (!(bp >> k & 1u))
            k++;
        help.k = (uint8_t)k;
        help.t = now;
    } else if (kp || bp || (help.k != HELP_NONE && *btn != (1u << help.k))) {
        help.k = HELP_NONE;
    }
    if (kp || bp)                        /* (anything else pressed: the let-go is the button's again) */
        help.ate = help.up ? help.ate : HELP_NONE;
    if (help.k != HELP_NONE && !help.up && now - help.t >= HELP_HOLD_MS && *btn == (1u << help.k) && !*keys) {
        help_show(help.k, now);
        help.k = HELP_NONE;
    }
    return 0;
}

static int help_ate_release(uint32_t k)  /* button_release: 1 = the card's button let go (nothing more to do) */
{
    if (help.ate != k)
        return 0;
    help.ate = HELP_NONE;
    return 1;
}

static void help_draw(void)              /* in the page's place (the header stays) */
{
    const help_card_t *c = &HELP[help.card];
    uint32_t i;
    if (help.drawn)
        return;
    help.drawn = 1;
    cv_begin(240, PAGE_H, C_BLACK);
    cv_rect(0, 0, 240, 1, C_LINE);
    cv_text(6, 4, &FONT_XS, "HELP", C_GRAY);
    cv_text(6, 18, &FONT_B, c->title, C_HI);
    for (i = 0; i < HELP_LINES && c->line[i]; i++)
        cv_text(8, 42 + (int32_t)i * 11, &FONT_T, c->line[i], C_WHITE);
    cv_blit(0, STRIP_Y);
    lcd_fill(0, STRIP_Y + PAGE_H, 240, FOOT_Y - STRIP_Y - PAGE_H, C_BLACK);
    cv_begin(240, FOOT_H, C_BLACK);
    cv_rect(0, 0, 240, 1, C_LINE);
    cv_text(6, 12, &FONT_XS, "ANY PRESS: BACK", C_GRAY);
    cv_blit(0, FOOT_Y);
}

/* ------------------------------------------------------------------------------------------- tap tempo --- */
static struct {
    uint32_t t[5];
    uint8_t n;
} tap;

static void tap_tempo(uint32_t now)      /* HOME + SEQ */
{
    char s[16];
    uint32_t i, sum;
    int32_t v;
    if (ctl_get(P_CLOCK) == CLOCK_IN) {
        app_say("TEMPO", "FROM MIDI CLOCK");
        return;
    }
    if (tap.n && now - tap.t[tap.n - 1u] > 2000u)
        tap.n = 0;                       /* a long gap: a new count */
    if (tap.n == 5u) {                   /* the last five taps: four gaps */
        for (i = 0; i < 4u; i++)
            tap.t[i] = tap.t[i + 1u];
        tap.n = 4;
    }
    tap.t[tap.n++] = now;
    if (tap.n < 2u) {
        app_say("TAP", "AGAIN ON THE BEAT");
        return;
    }
    sum = tap.t[tap.n - 1u] - tap.t[0];
    v = (int32_t)((600000u * (tap.n - 1u) + sum / 2u) / (sum ? sum : 1u));   /* BPM x 10 */
    if (v < 400)
        v = 400;
    if (v > 1800)
        v = 1800;
    if (tap.n > 2u)
        undo_join();                     /* (the taps: one step to undo) */
    ctl_set(P_TEMPO, v);
    set_dirty = 1;
    dub.dirty_hdr = 1;
    tempo_text(s);
    text_cat(s, " BPM");
    app_say("TAP TEMPO", s);
}

#ifdef PLAT_HOST
static int help_expect(const char *what, int *got)
{
    if (!strcmp(what, "help"))
        *got = help.up ? (int)HELP[help.card].btn : -1;
    else
        return 0;
    return 1;
}
#endif
