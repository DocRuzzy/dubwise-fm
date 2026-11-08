/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* The DUB page, the home of the live desk (docs/interface-spec.md 4.1, 5.1): first version, P0.1 (quantised mutes
 * and solo). Part of the unity build: app.c includes it, after its settings, messages and helpers.
 *
 *   White 1-8      mute channel 1-8: a tap toggles, a hold flips it until released, a double tap solos (additive)
 *   HOME + white   solo, as an alternative
 *   OCT- / OCT+    launch quantise: BEAT, 1, 2, 4, 8, 16, 32 bars
 *   PLAY           start / stop (on release). Held with a white key: that key acts now, not on the line
 *   HOME + SELECT  tempo, 1 BPM a detent
 *   ALGORITHM      the focused channel (its strip underlined; the knobs follow it later)
 *   EDIT           the SOUND view: KNOB 1-4 edit the focused channel's voice, four parameters a bank; EDIT again
 *                  for the next bank, then back to the echo's knobs (P0.5)
 *   SAVE           tempo, quantise, the echo's knobs and the sounds
 *   GLO            the SETUP page (setup.c): cycle, runway, cue tick, reduced motion, cue lead, test tones
 *   ARP            the KEY & CHORDS page (keys.c); HOME + ARP: the key lock on / off from any page
 *   FX             the FX page (fx.c): the echo's and the spring's settings, a bank at a time
 *   HOME again     the ARRANGE page (arrange.c): scenes; PRESET on the DUB page cues the next stored scene
 *   black 1        the spring crash (the tank struck); held with a throw key, that throw goes to the spring too
 *   black 4        the tape dive, while held: the echo's time glides longer (FX page, TAPE: depth, glide)
 *   K1-K4          echo TIME (a division of the beat), FB (feedback as T60), ECHO return, SPRING return
 *   white 1-8 held + K1-K4   that channel's level, echo send, spring send, filter (hold-to-focus, spec 2.3); turning
 *                  a knob keeps the key from flipping the mute
 *   black 2 / 3    echo freeze / dub push, while held
 *   White 9-16     throw channel 1-8 to its destination (P0.2): while held, in full, pre-fader and pre-mute (a muted
 *                  channel can be thrown); the echo's loop lifted. A double tap latches it; a tap unlatches
 *   white 9-16 held + K1   the destination: E (echo), S (spring), ES (both); the throw is cancelled
 * Quantised actions blink their key until they land, and the centre says when. Not here yet: the other black
 * keys' effects (P1.x, P2.x), the channel filter, the runway (P0.6), velocity-scaled throws (P2.9). */

/* four regions, each drawn only when it changes (spec 5.3): header 0-23, strips 26-121, centre 124-199, footer
 * 202-239 */
#define HDR_Y 0
#define HDR_H 24
#define STRIP_Y 26
#define STRIP_H 96
#define CENTRE_Y 124
#define CENTRE_H 76
#define FOOT_Y 202
#define FOOT_H 38
#define PAGE_H 144                       /* a full page's canvas (the most a canvas holds), from STRIP_Y */

#define TAP_MS 300u                      /* shorter than this, a tap; two within it, a double tap */
#define BLINK_MS 250u                    /* slow blink: cued */

static const char *const CH_NAME[NCH] = {"KICK", "SNARE", "HATS", "BASS", "SKANK", "ORGAN", "LEAD", "VOX"};
static const char *const CH_SHORT[NCH] = {"KICK", "SNR", "HAT", "BASS", "SKNK", "ORGN", "LEAD", "VOX"};
static const char *const BLACK_NAME[11] = {"SPRING CRASH", "ECHO HOLD", "DUB PUSH", "TAPE DIVE", "DROP TO WET",
                                           "SIREN A", "SIREN B", "VOX 1", "VOX 2", "VOX 3", "DRUM AND BASS"};
/* ROOTS NAMES (hidden; SETUP: black 11 three times): what the reader says, a tool by the hand or house it is tied
 * to. The page labels stay plain */
static const char *const ROOTS[][2] = {
    {"BIG KNOB", "KING TUBBY KNOB"},             /* the stepped high-pass: Tubby's Altec filter */
    {"BI-PHASE", "BLACK ARK PHASE"},             /* Lee Perry's Black Ark, its Bi-Phase */
    {"PING-PONG", "MAD PROFESSOR PING-PONG"},    /* the Professor's stereo dub */
    {"SIREN A", "SHAKA SIREN A"},                /* Jah Shaka's sound system */
    {"SIREN B", "SHAKA SIREN B"},
    {"DRIVE", "ON-U DRIVE"},                     /* On-U Sound's pushed desk */
    {"STEPPERS", "CHANNEL ONE STEPPERS"},        /* the Revolutionaries at Channel One */
    {"FLYERS", "BUNNY LEE FLYERS"},              /* Bunny Lee's flying cymbal */
};
static int same_text(const char *a, const char *b)
{
    while (*a && *a == *b)
        a++, b++;
    return *a == *b;
}
static const char *roots_name(const char *plain)
{
    uint32_t i;
    if (!ctl_get(P_ROOTS))
        return plain;
    for (i = 0; i < sizeof ROOTS / sizeof ROOTS[0]; i++)
        if (same_text(plain, ROOTS[i][0]))
            return ROOTS[i][1];
    return plain;
}

static const char *const BTN_NAME[NB] = {"FX", "SEL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ",
                                         "PLAY", "REC", "OCT-", "OCT+"};
static const char *const ENC_NAME[NE] = {"BIG KNOB", "FOCUS", "PRESET", "K1", "K2", "K3", "K4"};
static const char *const KNOB_LABEL[4] = {"TIME", "FB", "ECHO", "SPRG"};   /* spec 4.1 */
static const char *const FOCUS_LABEL[4] = {"LEVEL", "ECHO", "SPRG", "FILT"};   /* a mute key held */
static const char *const DEST_NAME[DEST_N] = {"E", "S", "ES"};
static const char *const DEST_WORD[DEST_N] = {"ECHO", "SPRING", "BOTH"};
#define C_RED RGB(232, 76, 60)
/* (forward: the pages' files are included further down) */
static void song_rec_toggle(void);
static void song_undo_last(void);
static void song_sync(void);
static int song_section(uint32_t *from, uint32_t *to);
static void demo_load(void);
static void slot_value(char *t, int load);      /* (slots.c: song slots) */
static void slot_pick(int32_t e);
static void slot_action(int load);
static void slot_tick(void);
static void steppers_load(void);
void dub_init(void);
static const uint8_t WK[16] = PLAT_WHITE_KEYS, BK[11] = PLAT_BLACK_KEYS;

static struct {
    uint32_t keys, btn;                  /* held at the last input */
    uint32_t press_t[NCH], rel_t[NCH];   /* a mute key's last press and release */
    uint8_t m0[NCH];                     /* its mute before the press (a hold, a double tap put it back) */
    uint8_t tapped[NCH], dbl[NCH];       /* released as a tap (a double tap may follow); this press is the second */
    uint8_t vel[NKEYS];                  /* each key's last velocity */
    uint32_t thr_rel_t[NCH];             /* a throw key's last release */
    uint8_t thr_tapped[NCH], thr_latched[NCH], thr_skip[NCH];   /* a tap's release; latched; this release ignored */
    int32_t last_key;
    uint8_t play_used, home_used, save_used;   /* held as a modifier: its own press does nothing on release */
    int32_t focus;                       /* ALGORITHM's channel */
    uint8_t sound;                       /* the SOUND view: 0 off, else its bank (1, 2) */
    uint8_t page, page_drawn;            /* PAGE_DUB, PAGE_STEPS; the one on the screen */
    uint32_t one_until;                  /* the ONE flash, until then */
    uint32_t rw_bar, rw_beat;            /* the runway's bar and beat last seen (bar x 4 + beat + 1; 0 none) */
    uint8_t border, border_drawn;        /* the screen's border: 0 off, 1 amber (a last-bar beat), 2 green (ONE) */
    uint32_t border_until, keys_until;   /* the pulse's end; the keys' flash's end */
    uint32_t pulses, key_flashes;        /* counted (tests) */
    int tier_seen;                       /* the guard tier last shown */
    int32_t spring_add;                  /* the throw black 1 added the spring to, -1 none */
    int8_t dest_was;                     /* its destination before */
    int32_t turns[NE];                   /* detents since boot, per encoder */
    uint8_t dirty_hdr, dirty_strips, dirty_centre, dirty_foot;
    uint32_t foot_sig, centre_t;         /* what the footer shows (redrawn when it changes); the centre's last draw */
    uint32_t last_beat, last_cued, last_blink, last_meter_t;
    int32_t last_lands;
    uint8_t msg_up;
} dub;

/* ------------------------------------------------------------------ state --- */
static int any_solo(int eng)
{
    uint32_t ch;
    for (ch = 0; ch < NCH; ch++)
        if (eng ? eng_param(P_CH(ch, CHF_SOLO)) : ctl_get(P_CH(ch, CHF_SOLO)))
            return 1;
    return 0;
}
static int audible(uint32_t ch)          /* as the engine has it now */
{
    return !eng_param(P_CH(ch, CHF_MUTE)) && (!any_solo(1) || eng_param(P_CH(ch, CHF_SOLO)));
}
static int cued(uint32_t ch)             /* a change asked for that has not landed yet */
{
    return ctl_get(P_CH(ch, CHF_MUTE)) != eng_param(P_CH(ch, CHF_MUTE))
        || ctl_get(P_CH(ch, CHF_SOLO)) != eng_param(P_CH(ch, CHF_SOLO));
}
static uint32_t cued_mask(void)
{
    uint32_t ch, m = 0;
    for (ch = 0; ch < NCH; ch++)
        if (cued(ch))
            m |= 1u << ch;
    return m;
}

static uint32_t q_beats(void)            /* the quantise setting in beats (CYCLE: the cycle's) */
{
    int32_t q = ctl_get(P_QUANT);
    return q == QUANT_CYCLE ? cycle_bars(ctl_get(P_CYCLE)) * 4u : quant_beats(q);
}

/* the runway (spec 5.4): where the transport is in the cycle */
typedef struct {
    uint32_t cyc, bar, left, beat;       /* the cycle's bars, this bar in it (0..), bars to its end, beat in bar */
    int on;                              /* in the runway's bars */
} runway_t;
static runway_t runway_now(void)
{
    runway_t r;
    uint32_t b = eng_beat();
    r.cyc = cycle_bars(ctl_get(P_CYCLE));
    r.bar = eng_playing() ? (b / 4u) % r.cyc : 0u;
    r.left = r.cyc - r.bar;
    r.beat = eng_playing() ? b % 4u : 0u;
    {                                    /* in song mode, the section is the cycle (its markers) */
        uint32_t from, to;
        if (song_section(&from, &to)) {
            r.cyc = to - from;
            r.bar = eng_song_bar() - from;
            r.left = r.cyc - r.bar;
        }
    }
    r.on = eng_playing() && r.left <= runway_bars(ctl_get(P_RUNWAY));
    return r;
}

/* the quantum an action waits for: none with PLAY held (and while stopped the engine acts at once anyway) */
static uint32_t act_beats(void)
{
    if (dub.btn >> B_PLAY & 1u) {
        dub.play_used = 1;
        return 0;
    }
    return q_beats();
}

/* beats until the next line of the quantise setting (0: stopped, nothing to wait for) */
static int32_t lands_in(void)
{
    uint32_t q = q_beats(), b = eng_beat();
    if (!eng_playing())
        return 0;
    return (int32_t)((b / q + 1u) * q - b);
}

static void lands_text(int32_t beats, char *t)   /* "NEXT BEAT", "THIS BAR", "2 BARS" .. */
{
    char n[12];
    t[0] = 0;
    if (beats <= 1)
        text_cat(t, "NEXT BEAT");
    else if (beats <= 4)
        text_cat(t, "1 BAR");
    else {
        int_text((beats + 3) / 4, n);
        text_cat(t, n);
        text_cat(t, " BARS");
    }
}

static void tempo_text(char *t)          /* "75.0" */
{
    int32_t v = ctl_get(P_TEMPO);
    char d[4] = {'.', 0, 0, 0};
    int_text(v / 10, t);
    d[1] = (char)('0' + v % 10);
    text_cat(t, d);
}

static uint32_t voice_fields(int32_t ch)  /* how many parameters the channel's voice has (0: none yet) */
{
    uint32_t n = 0;
    while (n < VOICE_STRIDE && param_desc(P_VOICE(ch, n)))
        n++;
    return n;
}

static int32_t sound_id(uint32_t i)      /* the SOUND view's knob i: its parameter, -1 none */
{
    uint32_t f = (uint32_t)(dub.sound - 1u) * 4u + i;
    return dub.sound && f < voice_fields(dub.focus) ? (int32_t)P_VOICE(dub.focus, f) : -1;
}

static void upper(const char *a, char *t, uint32_t n)   /* a parameter's name as the screen shows it */
{
    uint32_t i;
    for (i = 0; a[i] && i + 1 < n; i++)
        t[i] = a[i] >= 'a' && a[i] <= 'z' ? (char)(a[i] - 32) : a[i];
    t[i] = 0;
}

enum { PAGE_DUB, PAGE_STEPS, PAGE_SETUP, PAGE_KEY, PAGE_FX, PAGE_ARRANGE, PAGE_PERF };
#include "keys.c"                        /* the KEY & CHORDS page (GPL-3.0-only) */
#include "steps.c"                       /* the STEPS page (GPL-3.0-only) */
#include "setup.c"                       /* the SETUP page (GPL-3.0-only) */
static void fx_next(int same_page);      /* (fx.c, included further down: it needs the echo's helpers) */
static void arr_mark(void);              /* (arrange.c: its page redrawn) */
static uint32_t blink_on(void);

/* ------------------------------------------------------------------ input --- */
static void say_mute(uint32_t ch, int muted)
{
    app_say(muted ? "MUTE" : "UNMUTE", CH_NAME[ch]);
}

static void solo_toggle(uint32_t ch, uint32_t beats)
{
    uint32_t id = P_CH(ch, CHF_SOLO);
    int on = !ctl_get(id);
    ctl_cue(id, on, beats);
    app_say(on ? "SOLO" : "UNSOLO", CH_NAME[ch]);
}

static void mute_press(uint32_t ch, uint32_t now)
{
    uint32_t id = P_CH(ch, CHF_MUTE), beats = act_beats();
    if (dub.btn >> B_HOME & 1u) {        /* HOME + white: solo */
        dub.home_used = 1;
        dub.dbl[ch] = 1;                 /* (its release does nothing) */
        solo_toggle(ch, beats);
        return;
    }
    if (dub.tapped[ch] && now - dub.rel_t[ch] < TAP_MS) {   /* the second tap: solo, and the first tap undone */
        dub.tapped[ch] = 0;
        dub.dbl[ch] = 1;
        ctl_cue(id, dub.m0[ch], beats);
        solo_toggle(ch, beats);
        return;
    }
    dub.dbl[ch] = 0;
    dub.press_t[ch] = now;
    dub.m0[ch] = (uint8_t)ctl_get(id);
    ctl_cue(id, !dub.m0[ch], beats);
    say_mute(ch, !dub.m0[ch]);
}

static void mute_release(uint32_t ch, uint32_t now)
{
    if (dub.dbl[ch]) {
        dub.dbl[ch] = 0;
        return;
    }
    if (now - dub.press_t[ch] >= TAP_MS) {   /* a hold: back as it was */
        dub.tapped[ch] = 0;
        ctl_cue(P_CH(ch, CHF_MUTE), dub.m0[ch], act_beats());
        say_mute(ch, dub.m0[ch]);
    } else {
        dub.tapped[ch] = 1;
        dub.rel_t[ch] = now;
    }
}

static void throw_amount(uint32_t ch)    /* P2.9: the throw as hard as its key was hit (SETUP: THROW VELOCITY) */
{
    uint32_t v = dub.vel[WK[NCH + ch]];
    int32_t amt = 127;
    if (ctl_get(P_THROW_VEL) && v)       /* (no reading: in full) the softest 48 (-8 dB), the hardest 127 */
        amt = 48 + (int32_t)((79u * (v > 127u ? 127u : v) + 63u) / 127u);
    if (ctl_get(P_CH(ch, CHF_THROW_AMT)) != amt)
        ctl_set(P_CH(ch, CHF_THROW_AMT), amt);
}

static void throw_press(uint32_t ch, uint32_t now)
{
    uint32_t id = P_CH(ch, CHF_THROW);
    if (dub.thr_latched[ch]) {           /* latched: a tap lets it go */
        dub.thr_latched[ch] = 0;
        dub.thr_skip[ch] = 1;
        ctl_set(id, 0);
        app_say("THROW OFF", CH_NAME[ch]);
        return;
    }
    if (dub.thr_tapped[ch] && now - dub.thr_rel_t[ch] < TAP_MS) {   /* a double tap: latched on */
        dub.thr_tapped[ch] = 0;
        dub.thr_latched[ch] = 1;
        dub.thr_skip[ch] = 1;
        throw_amount(ch);
        ctl_set(id, 1);
        app_say("THROW LATCHED", CH_NAME[ch]);
        return;
    }
    dub.thr_skip[ch] = 0;
    throw_amount(ch);
    ctl_set(id, 1);
    app_say("THROW", CH_NAME[ch]);
}

static void throw_release(uint32_t ch, uint32_t now)
{
    if (dub.thr_skip[ch]) {
        dub.thr_skip[ch] = 0;
        return;
    }
    ctl_set(P_CH(ch, CHF_THROW), 0);
    dub.thr_tapped[ch] = 1;
    dub.thr_rel_t[ch] = now;
}

static void quant_step(int32_t d)
{
    int32_t q = ctl_get(P_QUANT) + d;
    if (q < 0 || q >= QUANT_N)
        return;
    ctl_set(P_QUANT, q);
    set_dirty = 1;
    dub.dirty_hdr = 1;
    app_say("QUANTISE", quant_name(q));
}

static void button_press(uint32_t k)
{
    if ((k == B_OCTDN || k == B_OCTUP) && dub.page == PAGE_STEPS)
        steps_oct(k == B_OCTUP ? 1 : -1);   /* the STEPS page: its page of 16 */
    else if (k == B_OCTDN)
        quant_step(-1);
    else if (k == B_OCTUP)
        quant_step(1);
    else if (k == B_SEQ) {
        dub.page = PAGE_STEPS;
        app_say("STEPS", CH_NAME[st.ch]);
    } else if (k == B_ARP && (dub.btn >> B_HOME & 1u)) {
        dub.home_used = 1;
        lock_toggle();
    } else if (k == B_ARP) {             /* the page; again: its next bank */
        if (dub.page == PAGE_KEY)
            ks.bank = (uint8_t)!ks.bank;
        else
            ks.bank = 0;
        dub.page = PAGE_KEY;
        ks.sig = 0;
        app_say("KEY", ks.bank ? "CHORD LANE" : "AND LOCK");
    } else if (k == B_REC && dub.page == PAGE_STEPS) {
        steps_rec();
    } else if (k == B_REC && (dub.btn >> B_SAVE & 1u)
               && (dub.page == PAGE_DUB || dub.page == PAGE_ARRANGE)) {   /* SAVE + REC: the motion cleared (P2.7) */
        dub.save_used = 1;
        ctl_motion_clear();
        set_dirty = 1;
        arr_mark();
        app_say("MOTION", "CLEARED");
    } else if (k == B_REC && (dub.btn >> B_HOME & 1u)) {   /* HOME + REC: undo the last song capture */
        dub.home_used = 1;
        song_undo_last();
    } else if (k == B_REC && (dub.page == PAGE_DUB || dub.page == PAGE_ARRANGE)) {
        song_rec_toggle();
    } else if (k == B_FX) {
        fx_next(dub.page == PAGE_FX);
        dub.page = PAGE_FX;
    }
    else if (k == B_PLAY && (dub.btn >> B_HOME & 1u)) {   /* HOME + PLAY: the spin-back (P3.5) */
        dub.home_used = dub.play_used = 1;
        static const char *const CALL[3][2] = {{"REWIND", "COME AGAIN"}, {"PULL UP", "SELECTA"}, {"WHEEL UP", ""}};
        static uint8_t call;             /* the selector's calls, in turn */
        ctl_set(P_SPIN, 1);
        app_say(CALL[call][0], CALL[call][1]);
        call = (uint8_t)((call + 1u) % 3u);
    } else if (k == B_PLAY)
        dub.play_used = 0;               /* start / stop on release, unless used as a modifier */
    else if (k == B_HOME)
        dub.home_used = 0;
    else if (k == B_SAVE)
        dub.save_used = 0;               /* saved on release, unless held to store a scene (ARRANGE) */
    else if (k == B_EDIT) {              /* the SOUND view, a bank at a time, then back */
        uint32_t nf = voice_fields(dub.focus);
        dub.sound = (uint8_t)(dub.sound + 1u);
        if ((uint32_t)(dub.sound - 1u) * 4u >= nf || nf == 0u)
            dub.sound = 0;
        dub.dirty_foot = 1;
        if (dub.sound)
            app_say("SOUND", CH_NAME[dub.focus]);
        else
            app_say(nf ? "ECHO" : "NO VOICE YET", nf ? "KNOBS" : CH_NAME[dub.focus]);
    } else if (k == B_GLO) {
        dub.page = PAGE_SETUP;
        app_say("SETUP", SU_NAME[su_cur]);
    } else
        app_say("BUTTON", BTN_NAME[k]);
}

static void button_release(uint32_t k)
{
    if (k == B_PLAY && ctl_get(P_SPIN))
        ctl_set(P_SPIN, 0);
    if (k == B_PLAY && !dub.play_used) {
        int on = !ctl_get(P_PLAY);
        ctl_set(P_PLAY, on);
        app_say(on ? "PLAY" : "STOP", "");
    } else if (k == B_HOME && !dub.home_used) {   /* HOME: the DUB page; again: ARRANGE (spec 4) */
        dub.page = dub.page == PAGE_DUB ? PAGE_ARRANGE : PAGE_DUB;
        arr_mark();
        app_say(dub.page == PAGE_ARRANGE ? "ARRANGE" : "DUB", "PAGE");
    } else if (k == B_SAVE && !dub.save_used && dub.page == PAGE_SETUP && setup_slot_row() >= 0) {
        slot_action(setup_slot_row());   /* SETUP's slot rows: SAVE does it */
    } else if (k == B_SAVE && !dub.save_used) {
        settings_save();
    }
}

static int32_t echo_div_ui(void)         /* the division the engine will use: the longest that fits (engine.c) */
{
    int32_t d = ctl_get(P_ECHO_TIME);
    while (d > 0 && echo_div_beats(d) * 6615000.0f / (float)ctl_get(P_TEMPO) > ECHO_T_MAX)
        d--;
    return d;
}

static int32_t held_mute(void)          /* the mute key held (the lowest), -1 none: its strip is focused */
{
    uint32_t ch;
    for (ch = 0; ch < NCH; ch++)
        if (dub.keys >> WK[ch] & 1u)
            return (int32_t)ch;
    return -1;
}

static int32_t held_throw(void)         /* the throw key held (the lowest), -1 none */
{
    uint32_t ch;
    for (ch = 0; ch < NCH; ch++)
        if (dub.keys >> WK[NCH + ch] & 1u)
            return (int32_t)ch;
    return -1;
}

static void t60_text(char *t)            /* "4.2s", "HOLD", "PUSH", "INF" */
{
    float s = echo_t60(ctl_get(P_ECHO_FB));
    int32_t d;
    char n[12];
    t[0] = 0;
    if (ctl_get(P_ECHO_PUSH)) {
        text_cat(t, "PUSH");
        return;
    }
    if (ctl_get(P_ECHO_FREEZE)) {
        text_cat(t, "HOLD");
        return;
    }
    if (s >= 60.0f) {
        text_cat(t, "INF");
        return;
    }
    d = (int32_t)(s * 10.0f + 0.5f);
    int_text(d / 10, n);
    text_cat(t, n);
    if (d < 100) {
        n[0] = '.';
        n[1] = (char)('0' + d % 10);
        n[2] = 0;
        text_cat(t, n);
    }
    text_cat(t, "s");
}

static void knob_value(uint32_t i, int32_t ch, char *t)   /* knob i's value as the footer shows it */
{
    int32_t tc = held_throw();
    t[0] = 0;
    if (dub.sound && tc < 0 && ch < 0) { /* the SOUND view */
        int32_t id = sound_id(i);
        if (id >= 0)
            int_text(ctl_get((uint32_t)id), t);
        return;
    }
    if (tc >= 0) {                       /* a throw key held: K1 its destination */
        text_cat(t, i == 0u ? DEST_WORD[ctl_get(P_CH(tc, CHF_THROW_DEST))] : "");
        return;
    }
    if (ch >= 0) {
        static const uint8_t F[3] = {CHF_LEVEL, CHF_ECHO_SEND, CHF_SPRING_SEND};
        if (i < 3u)
            int_text(ctl_get(P_CH(ch, F[i])), t);
        else
            text_cat(t, "--");           /* the channel filter: to come */
    } else if (i == 0u)
        text_cat(t, echo_div_name(echo_div_ui()));
    else if (i == 1u)
        t60_text(t);
    else
        int_text(ctl_get(i == 2u ? P_ECHO_RET : P_SPRING_RET), t);
}

#include "fx.c"                          /* the FX page (GPL-3.0-only) */
#include "arrange.c"                     /* the ARRANGE page (GPL-3.0-only) */
#include "demo.c"                        /* the demo song (GPL-3.0-only) */
#include "perf.c"                        /* the PERF page (GPL-3.0-only) */
#include "saver.c"                       /* the boot splash and the screen saver (GPL-3.0-only) */
#include "slots.c"                       /* song slots (GPL-3.0-only) */

static void knob(uint32_t i, int32_t e)  /* K1-K4 */
{
    int32_t ch = held_mute(), id = -1, step = 2, tc = held_throw();
    char t[16];
    if (tc >= 0) {                       /* a throw key held: K1 its destination (the throw cancelled) */
        uint32_t did = P_CH(tc, CHF_THROW_DEST);
        if (i != 0u)
            return;
        if (!dub.thr_skip[tc]) {
            dub.thr_skip[tc] = 1;
            ctl_set(P_CH(tc, CHF_THROW), 0);
        }
        ctl_set(did, (ctl_get(did) + e % DEST_N + DEST_N) % DEST_N);
        app_say("THROW TO", DEST_WORD[ctl_get(did)]);
        dub.dirty_strips = dub.dirty_foot = 1;
        return;
    }
    if (dub.sound && ch < 0) {           /* the SOUND view: the focused voice */
        char l[12];
        int32_t sid = sound_id(i);
        if (sid < 0)
            return;
        ctl_set((uint32_t)sid, ctl_get((uint32_t)sid) + e);
        set_dirty = 1;
        upper(param_desc((uint32_t)sid)->name, l, sizeof l);
        int_text(ctl_get((uint32_t)sid), t);
        app_say(l, t);
        dub.dirty_foot = 1;
        return;
    }
    if (ch >= 0) {                       /* hold-to-focus: the held channel's strip */
        static const uint8_t F[3] = {CHF_LEVEL, CHF_ECHO_SEND, CHF_SPRING_SEND};
        if (!dub.dbl[ch]) {              /* the hold is for editing: the mute stays as it was */
            dub.dbl[ch] = 1;
            ctl_cue(P_CH(ch, CHF_MUTE), dub.m0[ch], act_beats());
        }
        if (i < 3u)
            id = P_CH(ch, F[i]);
        else {
            app_say("FILTER", "TO COME");
            return;
        }
        step = i == 0u ? 1 : 2;
    } else {
        static const uint16_t ID[4] = {P_ECHO_TIME, P_ECHO_FB, P_ECHO_RET, P_SPRING_RET};
        id = ID[i];
        step = i < 2u ? 1 : 2;
    }
    ctl_set((uint32_t)id, ctl_get((uint32_t)id) + step * e);
    if (ch < 0)
        set_dirty = 1;
    knob_value(i, ch, t);
    app_say(ch >= 0 ? FOCUS_LABEL[i] : KNOB_LABEL[i], t);
    dub.dirty_foot = 1;
}

static void encoder(uint32_t k, int32_t e)
{
    char t[16];
    dub.turns[k] += e;
    if (k == EN_SELECT && (dub.btn >> B_HOME & 1u)) {   /* HOME + SELECT: tempo */
        dub.home_used = 1;
        ctl_set(P_TEMPO, ctl_get(P_TEMPO) + 10 * e);
        set_dirty = 1;
        dub.dirty_hdr = 1;
        tempo_text(t);
        app_say("TEMPO", t);
    } else if (k == EN_SELECT && dub.page != PAGE_SETUP) {   /* the Big Knob: a detent a step, at once */
        int32_t s = hpf_detent(ctl_get(P_HPF_RES)), v = ctl_get(P_HPF_STEP);
        v = e > 0 ? (v / s + e) * s : ((v + s - 1) / s + e) * s;   /* (off the setting's grid: to it first) */
        ctl_set(P_HPF_STEP, v);
        dub.dirty_centre = 1;
        app_say(roots_name("BIG KNOB"), hpf_name(ctl_get(P_HPF_STEP)));
    } else if (k == EN_ALGO) {
        dub.focus = (dub.focus + e % (int32_t)NCH + (int32_t)NCH) % (int32_t)NCH;
        if (dub.sound)
            dub.sound = voice_fields(dub.focus) ? 1u : 0u;
        dub.dirty_strips = 1;
        app_say("FOCUS", CH_NAME[dub.focus]);
    } else if (dub.page == PAGE_ARRANGE && k >= EN_K1 && k <= EN_K4) {
        arr_knob(k - EN_K1, e);
    } else if (k == EN_PRESET && dub.page == PAGE_DUB) {   /* PRESETS on the DUB page: cue the next scene (spec 4.1) */
        arr_step(e);
    } else if (dub.page == PAGE_FX && k >= EN_K1 && k <= EN_K4) {
        fx_knob(k - EN_K1, e);
    } else if (dub.page == PAGE_KEY && k >= EN_K1 && k <= EN_K4) {
        keys_knob(k - EN_K1, e);
    } else if (dub.page == PAGE_SETUP && (k == EN_K1 || k == EN_SELECT)) {
        setup_move(e);
    } else if (dub.page == PAGE_SETUP && (k == EN_K2 || k == EN_PRESET)) {
        setup_change(e);
    } else if (k >= EN_K1 && k <= EN_K4 && dub.page == PAGE_STEPS) {
        steps_knob(k - EN_K1, e);
    } else if (k >= EN_K1 && k <= EN_K4) {
        knob(k - EN_K1, e);
    } else {
        int_text(dub.turns[k], t);
        app_say(ENC_NAME[k], t);
    }
}

void dub_input(void)
{
    uint32_t keys, btn = plat_buttons(), k, now = plat_ms();
    int32_t e;
    touch_poll();                        /* the sensed keys' presses and velocities */
    keys = touch_keys();
    if (saver_input(&keys, &btn, now))   /* the screen saver up: a touch only wakes it */
        return;
    for (k = 0; k < NB; k++) {           /* buttons first: a modifier pressed with a key counts */
        uint32_t bit = 1u << k;
        if ((btn & bit) && !(dub.btn & bit)) {
            dub.btn |= bit;
            button_press(k);
        } else if (!(btn & bit) && (dub.btn & bit)) {
            dub.btn &= ~bit;
            button_release(k);
        }
    }
    if (dub.page == PAGE_STEPS) {        /* the STEPS page has the keys */
        steps_keys(keys, dub.keys);
        goto encoders;
    }
    if (dub.page == PAGE_SETUP) {        /* (black 11 three times in 2 s: ROOTS NAMES, the hidden switch) */
        static uint32_t taps, t0;
        uint32_t bit = 1u << BK[10];
        if ((keys & bit) && !(dub.keys & bit)) {
            if (now - t0 > 2000u)
                taps = 0, t0 = now;
            if (++taps == 3u) {
                taps = 0;
                ctl_set(P_ROOTS, !ctl_get(P_ROOTS));
                set_dirty = 1;
                app_say("ROOTS NAMES", ctl_get(P_ROOTS) ? "ON" : "OFF");
            }
        }
        goto encoders;
    }
    if (dub.page == PAGE_KEY)            /* the keys show the lock's set; they play nothing here */
        goto encoders;
    if (dub.page == PAGE_FX || dub.page == PAGE_PERF)
        goto encoders;
    if (dub.page == PAGE_ARRANGE) {
        arr_keys(keys, dub.keys);
        goto encoders;
    }
    for (k = 0; k < 16u; k++) {          /* white keys: 1-8 mutes, 9-16 throws (P0.2) */
        uint32_t bit = 1u << WK[k];
        if ((keys & bit) && !(dub.keys & bit)) {
            dub.vel[WK[k]] = (uint8_t)touch_vel(WK[k]);
            dub.last_key = WK[k];
            if (k < NCH)
                mute_press(k, now);
            else
                throw_press(k - NCH, now);
        } else if (!(keys & bit) && (dub.keys & bit)) {
            if (k < NCH)
                mute_release(k, now);
            else
                throw_release(k - NCH, now);
        }
    }
    for (k = 0; k < 11u; k++) {          /* black keys (spec 4.1): crash, hold, push, dive, wet, sirens, shots, D&B */
        uint32_t bit = 1u << BK[k];
        if ((keys & bit) && !(dub.keys & bit)) {
            dub.vel[BK[k]] = (uint8_t)touch_vel(BK[k]);
            dub.last_key = BK[k];
            if (k >= 7u && k <= 9u) {    /* black 8-10: the one-shots (P2.6), to their end; HOME held: backwards */
                int rev = (dub.btn >> B_HOME & 1u) != 0;
                if (rev)
                    dub.home_used = 1;
                ctl_note(CH_VOX, (rev ? 24 : 16) + (int32_t)(k - 7u), touch_vel(BK[k]) ? touch_vel(BK[k]) : 100u);
                app_say(rev ? "REVERSE" : "ONE SHOT", fxv_shot_name(k - 7u));
                continue;
            }
            if (k == 5u || k == 6u)      /* the sirens A, B, while held (P2.1) */
                ctl_note(CH_VOX, k == 5u ? SIREN_A : SIREN_B, touch_vel(BK[k]) ? touch_vel(BK[k]) : 100u);
            if (k == 4u || k == 10u) {   /* drop to wet, drum and bass: while held (spec 4.1) */
                ctl_set(k == 4u ? P_WET : P_DNB, 1);
                dub.dirty_centre = 1;
            }
            if (k == 3u) {               /* the tape dive, while held */
                ctl_set(P_ECHO_DIVE, 1);
                dub.dirty_foot = 1;
            }
            if (k == 1u || k == 2u) {
                ctl_set(k == 1u ? P_ECHO_FREEZE : P_ECHO_PUSH, 1);
                dub.dirty_foot = dub.dirty_centre = 1;
            }
            if (k == 0u) {               /* the crash; with a throw held, the spring added to that throw */
                int32_t tc = held_throw();
                if (tc >= 0) {
                    dub.spring_add = tc;
                    dub.dest_was = (int8_t)ctl_get(P_CH(tc, CHF_THROW_DEST));
                    ctl_set(P_CH(tc, CHF_THROW_DEST), DEST_BOTH);
                    app_say("THROW", "PLUS SPRING");
                    continue;
                }
                ctl_set(P_SPRING_KICK, 1);
            }
            app_say(roots_name(BLACK_NAME[k]), "");
        } else if (!(keys & bit) && (dub.keys & bit) && (k == 5u || k == 6u)) {
            ctl_note(CH_VOX, k == 5u ? SIREN_A : SIREN_B, 0);
        } else if (!(keys & bit) && (dub.keys & bit) && (k == 4u || k == 10u)) {
            ctl_set(k == 4u ? P_WET : P_DNB, 0);
            dub.dirty_centre = 1;
        } else if (!(keys & bit) && (dub.keys & bit) && k == 3u) {
            ctl_set(P_ECHO_DIVE, 0);
        } else if (!(keys & bit) && (dub.keys & bit) && (k == 1u || k == 2u)) {
            ctl_set(k == 1u ? P_ECHO_FREEZE : P_ECHO_PUSH, 0);
            dub.dirty_foot = dub.dirty_centre = 1;
        } else if (!(keys & bit) && (dub.keys & bit) && k == 0u) {
            if (dub.spring_add >= 0) {   /* the throw's destination as it was */
                ctl_set(P_CH(dub.spring_add, CHF_THROW_DEST), dub.dest_was);
                dub.spring_add = -1;
            }
            ctl_set(P_SPRING_KICK, 0);
        }
    }
encoders:
    for (k = 0; k < NE; k++)
        if ((e = plat_enc((int)k)) != 0) {
            saver_touch(now);
            encoder(k, e);
        }
    if (keys != dub.keys)
        dub.dirty_strips = 1;
    dub.keys = keys;
}

/* ----------------------------------------------------------------- lights --- */
/* one LED language (spec 2.7): bright = on / playing, dim = available / muted, slow blink = cued */
static uint32_t blink_on(void) { return (plat_ms() / BLINK_MS) & 1u; }

static void lights(void)
{
    uint32_t ch, bright = 0, dim = 0, bb = dub.btn, k;
    if (dub.page == PAGE_STEPS) {
        bright = steps_lights(&dim);
        goto buttons;
    }
    if (dub.page == PAGE_KEY) {
        bright = kb_lights(&dim);
        goto buttons;
    }
    if (dub.page == PAGE_ARRANGE) {
        bright = arr_lights(&dim);
        goto buttons;
    }
    for (ch = 0; ch < NCH; ch++) {
        uint32_t bit = 1u << WK[ch];
        if (cued(ch))
            bright |= blink_on() ? bit : 0u;
        else if (audible(ch))
            bright |= bit;
        else
            dim |= bit;
    }
    for (k = NCH; k < 16u; k++)          /* throw keys: bright while throwing (or latched) */
        if (ctl_get(P_CH(k - NCH, CHF_THROW)))
            bright |= 1u << WK[k];
        else
            dim |= 1u << WK[k];
    for (k = 0; k < 11u; k++)
        dim |= 1u << BK[k];
buttons:
    if (plat_ms() < dub.keys_until)      /* the runway's last bar on the keys: every white key, each beat */
        for (k = 0; k < 16u; k++)
            bright |= 1u << WK[k];
    if (eng_playing())
        bb |= LED_PLAY_GREEN;
    if (set_dirty)
        bb |= 1u << B_SAVE;
    plat_lights(bb, bright | dub.keys, 0, dim, 1);
}

#ifdef PLAT_HOST
static int led_state(uint32_t ch)        /* 0 off, 1 dim, 2 bright, 3 blinking (cued) */
{
    if (cued(ch))
        return 3;
    return audible(ch) ? 2 : 1;
}
#endif

/* ----------------------------------------------------------------- screen --- */

static void play_glyph(int32_t x, int32_t y, int playing, uint16_t c)
{
    int32_t i;
    if (playing)                         /* a triangle */
        for (i = 0; i < 11; i++)
            cv_rect(x, y + i, (i <= 5 ? i : 10 - i) + 1, 1, c);
    else
        cv_rect(x, y + 1, 9, 9, c);      /* a square */
}

#define C_GREEN RGB(80, 220, 120)

/* 72.0 BPM: the first bar played at it lights the cycle bar red, gold and green (the demo's tempo; an egg) */
static int32_t roots_bar = -1;
static uint8_t roots_armed;
static int roots_now(void)
{
    if (!eng_playing() || ctl_get(P_TEMPO) != 720) {
        roots_armed = 0;
        return 0;
    }
    if (!roots_armed) {
        roots_armed = 1;
        roots_bar = (int32_t)(eng_beat() / 4u);
    }
    return (int32_t)(eng_beat() / 4u) == roots_bar;
}

static void draw_header(void)            /* tempo, transport, the cycle bar, bar n / cycle, quantise (spec 5.1) */
{
    char t[24], n[12];
    runway_t r = runway_now();
    uint16_t rc = r.on ? C_AMB : C_GRAY;
    int32_t x;
    cv_begin(240, HDR_H, C_BLACK);
    if (plat_ms() < msg.until) {         /* a message, for a moment */
        x = cv_text(6, 4, &FONT_B, msg.text[0], C_WHITE);
        cv_text(x + 8, 4, &FONT_B, msg.text[1], C_HI);
    } else {
        uint32_t i, gaps = r.cyc / 4u - 1u, segw, w = 98u - (ctl_get(P_LOCK) ? 22u : 0u) - (eng_tier() ? 20u : 0u);
        int32_t bx;
        tempo_text(t);
        x = cv_text(4, 4, &FONT_B, t, C_WHITE);
        play_glyph(x + 4, 7, eng_playing(), eng_playing() ? C_HI : C_GRAY);
        bx = x + 18;
        segw = (w - gaps * 2u) / r.cyc;
        if (segw < 1u)
            segw = 1u;
        for (i = 0; i < r.cyc; i++) {    /* a segment a bar, in fours; this bar fills beat by beat */
            int32_t sx = bx + (int32_t)(i * segw + (i / 4u) * 2u), sw = (int32_t)(segw > 1u ? segw - 1u : 1u);
            if (roots_now()) {           /* 72.0 BPM, the demo's tempo: red, gold and green for a bar */
                static const uint16_t RGG[3] = {RGB(220, 40, 40), RGB(250, 200, 40), RGB(40, 170, 70)};
                cv_rect(sx, 8, sw, 10, RGG[i % 3u]);
            } else if (!eng_playing() || i > r.bar)
                cv_rect(sx, 8, sw, 10, C_LINE);
            else if (i < r.bar)
                cv_rect(sx, 8, sw, 10, rc);
            else {
                int32_t h = (int32_t)(r.beat + 1u) * 10 / 4;
                cv_rect(sx, 8, sw, 10, C_LINE);
                cv_rect(sx, 18 - h, sw, h, r.on ? C_AMB : C_HI);
            }
        }
        t[0] = 0;                        /* the phrase counter: this bar / the cycle */
        int_text((int32_t)r.bar + 1, n);
        text_cat(t, n);
        text_cat(t, "/");
        int_text((int32_t)r.cyc, n);
        text_cat(t, n);
        x = cv_text(bx + (int32_t)w + 4, 6, &FONT_XS, t, eng_playing() ? (r.on ? C_AMB : C_HI) : C_GRAY);
        if (eng_tier()) {                /* the guard tier's badge: 4T, 7" */
            const char *c = eng_tier() == TIER_FOUR ? "4T" : "7IN";
            cv_rect(x + 3, 5, text_w(&FONT_T, c) + 4, 14, C_RED);
            x = cv_text(x + 5, 6, &FONT_T, c, C_BLACK);
        }
        if (ctl_get(P_LOCK)) {           /* the key lock's chip: LOCK, or CHD in chord mode */
            const char *c = ctl_get(P_LOCK) == LOCK_CHORD ? "CHD" : "LOCK";
            cv_rect(x + 3, 5, text_w(&FONT_T, c) + 4, 14, C_AMB);
            x = cv_text(x + 5, 6, &FONT_T, c, C_BLACK);
        }
        t[0] = 0;
        text_cat(t, "Q ");
        text_cat(t, quant_name(ctl_get(P_QUANT)));
        cv_text(236 - text_w(&FONT_T, t), 8, &FONT_T, t, C_AMB);
    }
    cv_rect(0, HDR_H - 2, 240, 2, r.on ? C_AMB : C_BLACK);   /* the header amber on the runway */
    cv_rect(0, HDR_H - 1, 240, 1, r.on ? C_AMB : C_LINE);
    cv_blit(0, HDR_Y);
}

static void hatch(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)   /* diagonal lines: solo-muted */
{
    int32_t i, j;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            if (((i + j) & 3) == 0)
                cv_rect(x + i, y + j, 1, 1, c);
}

static void outline(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)
{
    cv_rect(x, y, w, 1, c);
    cv_rect(x, y + h - 1, w, 1, c);
    cv_rect(x, y, 1, h, c);
    cv_rect(x + w - 1, y, 1, h, c);
}

static int32_t meter_px(float pk, int32_t h)     /* -48 .. 0 dBFS over the box's height */
{
    float db;
    if (pk < 0.004f)
        return 0;
    db = 20.0f * fm_log2f(pk) * 0.30103f;
    return (int32_t)((db + 48.0f) * (float)h / 48.0f);
}

static void draw_strips(void)
{
    uint32_t ch;
    int solo_any = any_solo(1);
    cv_begin(240, STRIP_H, C_BLACK);
    for (ch = 0; ch < NCH; ch++) {
        int32_t x = 1 + (int32_t)ch * 30, bx = x + 5, by = 30, bw = 18, bh = 46;
        int on = audible(ch), soloed = eng_param(P_CH(ch, CHF_SOLO)) != 0;
        int thrown = eng_param(P_CH(ch, CHF_THROW)) != 0;
        char num[2] = {(char)('1' + ch), 0};
        const char *dg = DEST_NAME[eng_param(P_CH(ch, CHF_THROW_DEST))];
        if (thrown)                      /* a throw flashes its channel (spec 5.2) */
            cv_rect(x + 4, 0, 20, 17, C_HI);
        cv_text(x + (28 - text_w(&FONT_B, num)) / 2, 0, &FONT_B, num, thrown ? C_BLACK : on ? C_WHITE : C_GRAY);
        cv_text(x + (28 - text_w(&FONT_T, CH_SHORT[ch])) / 2, 17, &FONT_T, CH_SHORT[ch], on ? C_GRAY : C_DIM);
        if (on) {                        /* filled = playing; the meter brighter over it */
            int32_t m = meter_px(eng_ch_peak(ch), bh);
            cv_rect(bx, by, bw, bh, C_LINE);
            if (m > 0)
                cv_rect(bx, by + bh - (m > bh ? bh : m), bw, m > bh ? bh : m, C_HI);
        } else if (solo_any && !soloed && !eng_param(P_CH(ch, CHF_MUTE))) {
            hatch(bx, by, bw, bh, C_DIM);    /* hatched = solo-muted */
            outline(bx, by, bw, bh, C_DIM);
        } else {
            outline(bx, by, bw, bh, C_GRAY);   /* outlined = muted */
        }
        if (cued(ch) && blink_on())
            outline(bx - 2, by - 2, bw + 4, bh + 4, C_WHITE);
        cv_text(x + 3, by + bh + 3, &FONT_T, dg, thrown ? C_HI : C_DIM);   /* the throw's destination */
        if (soloed)
            cv_text(x + 19, by + bh + 3, &FONT_T, "S", C_AMB);
        if ((int32_t)ch == dub.focus)
            cv_rect(x + 3, STRIP_H - 2, 22, 2, C_GRAY);
    }
    cv_blit(0, STRIP_Y);
}

static void draw_echo(void)              /* the left half: the repeats as they fall (amber near unity, red past) */
{
    float g = eng_echo_g(), h = 1.0f, gr = eng_echo_gr();
    uint16_t c = g >= 1.0f ? C_RED : g > 0.9f ? C_AMB : C_HI;
    int32_t k, x = 6;
    char t[16];
    t[0] = 0;
    text_cat(t, "ECHO ");
    text_cat(t, echo_div_name(echo_div_ui()));
    cv_text(6, 4, &FONT_XS, t, C_GRAY);
    for (k = 0; k < 9; k++) {            /* the first repeat, then each one g of the last */
        int32_t bh = (int32_t)(44.0f * (h > 1.0f ? 1.0f : h));
        if (bh > 0)
            cv_rect(x, 66 - bh, 8, bh, c);
        else
            cv_rect(x, 65, 8, 1, C_LINE);
        x += 12;
        h *= g;
    }
    if (gr < 0.99f) {                    /* the return limiter at work: its reduction, a bar under the repeats */
        int32_t w = (int32_t)((1.0f - gr) * 220.0f);
        cv_rect(6, 70, w > 108 ? 108 : w, 3, C_RED);
    }
}

static void draw_centre(void)
{
    char t[24], n[12];
    uint32_t m = cued_mask(), ch;
    cv_begin(240, CENTRE_H, C_BLACK);
    draw_echo();
    cv_rect(120, 4, 1, CENTRE_H - 8, C_LINE);
    if (dub.one_until && plat_ms() < dub.one_until) {   /* the downbeat: ONE */
        cv_text(132, 18, &FONT_L, "ONE", C_GREEN);
    } else if (runway_now().on && ctl_get(P_RUNWAY_NUM)) {   /* the runway: bars to the one, big */
        runway_t r = runway_now();
        int_text((int32_t)r.left, n);
        cv_text(128, 4, &FONT_XS, r.left == 1u ? "LAST BAR" : "BARS TO THE ONE", C_AMB);
        cv_text(132, 22, &FONT_L, n, C_AMB);
        if (m)
            cv_text(176, 40, &FONT_XS, "CUED", C_WHITE);
    } else if (m && eng_playing()) {     /* what is cued, and when it lands */
        int32_t x = cv_text(126, 4, &FONT_XS, "CUED", C_GRAY) + 8;
        for (ch = 0; ch < NCH; ch++)
            if (m >> ch & 1u) {
                n[0] = (char)('1' + ch);
                n[1] = 0;
                x = cv_text(x, 4, &FONT_XS, n, C_WHITE) + 5;
            }
        cv_text(126, 22, &FONT_XS, "LANDS IN", C_GRAY);
        lands_text(lands_in(), t);
        cv_text(126, 38, &FONT_M, t, C_AMB);
    } else {                             /* the Big Knob: its detents as an arc (10, 20 or 40), lit to the step */
        int32_t stp = eng_param(P_HPF_STEP), k, s = hpf_detent(ctl_get(P_HPF_RES)), nd = HPF_STEPS / s;
        int32_t dot = nd > 20 ? 3 : 5;
        cv_text(126, 4, &FONT_XS, ctl_get(P_HPF_ON) == HPF_ON_ECHO ? "BIG KNOB: ECHO" : "BIG KNOB", C_GRAY);
        for (k = 1; k <= nd; k++) {
            float a = 3.14159265f * (1.0f - (float)(k - 1) / (float)(nd - 1));   /* left to right */
            int32_t x = 180 + (int32_t)(24.0f * fm_cosf(a)), y = 42 - (int32_t)(22.0f * fm_sinf(a));
            cv_rect(x - dot / 2, y - dot / 2, dot, dot, k * s <= stp ? C_HI : C_LINE);
        }
        cv_text(180 - text_w(&FONT_B, hpf_name(stp)) / 2, 44, &FONT_B, hpf_name(stp), stp ? C_WHITE : C_DIM);
        lands_text(lands_in(), t);
        cv_text(126, 63, &FONT_T, eng_playing() ? "NEXT LINE" : "STOPPED: AT ONCE", C_DIM);
        if (eng_playing())
            cv_text(184, 63, &FONT_T, t, C_GRAY);
    }
    cv_rect(0, 0, 240, 1, C_LINE);
    cv_blit(0, CENTRE_Y);
}

static void draw_footer(void)            /* the four knobs: the echo's, or the held channel's strip */
{
    uint32_t i;
    int32_t ch = held_mute();
    char t[16];
    cv_begin(240, FOOT_H, C_BLACK);
    cv_rect(0, 0, 240, 1, C_LINE);
    for (i = 0; i < 4u; i++) {
        int32_t x = 6 + (int32_t)i * 59;
        if (held_throw() >= 0)
            cv_text(x, 4, &FONT_XS, i == 0u ? "DEST" : "", C_AMB);
        else if (dub.sound && ch < 0) {
            char l[12];
            int32_t sid = sound_id(i);
            l[0] = 0;
            if (sid >= 0)
                upper(param_desc((uint32_t)sid)->name, l, sizeof l);
            cv_text(x, 4, &FONT_XS, l, C_AMB);
        } else
            cv_text(x, 4, &FONT_XS, ch >= 0 ? FOCUS_LABEL[i] : KNOB_LABEL[i], ch >= 0 ? C_AMB : C_GRAY);
        knob_value(i, ch, t);
        cv_text(x, 18, &FONT_B, t, i == 1u && ch < 0 && eng_echo_g() >= 1.0f ? C_RED : C_HI);
    }
    if (ch >= 0)
        cv_text(234 - text_w(&FONT_T, CH_NAME[ch]), 4, &FONT_T, CH_NAME[ch], C_AMB);
    else if (dub.sound && held_throw() < 0) {   /* which voice, which bank */
        char t2[20];
        t2[0] = 0;
        text_cat(t2, CH_NAME[dub.focus]);
        text_cat(t2, dub.sound == 1u ? " 1" : " 2");
        cv_text(234 - text_w(&FONT_T, t2), 4, &FONT_T, t2, C_AMB);
    }
    cv_blit(0, FOOT_Y);
}

static uint32_t foot_sig(void)           /* changes whenever what the footer shows does */
{
    uint32_t h = ((uint32_t)(held_mute() + 1) * 17u + (uint32_t)(held_throw() + 1)) * 7u + dub.sound * 131u
                 + (uint32_t)dub.focus * 1031u, i;
    for (i = 0; i < 4u; i++) {
        char t[16], *p;
        knob_value(i, held_mute(), t);
        for (p = t; *p; p++)
            h = h * 31u + (uint8_t)*p;
    }
    return h;
}

static void runway_track(uint32_t now)   /* beat edges: the count to the reader, the pulses, the ONE */
{
    runway_t r = runway_now();
    uint32_t pos = eng_playing() ? (eng_beat() + 1u) : 0u;
    int reduced = ctl_get(P_REDUCED) != 0;
    if (pos == dub.rw_beat)
        return;
    dub.rw_beat = pos;
    dub.dirty_centre = 1;
    if (!pos)
        return;
    if (r.bar == 0u && r.beat == 0u) {   /* the one */
        dub.one_until = now + 600u;
        if (!reduced) {
            dub.border = 2;
            dub.border_until = now + 200u;
        }
        if (ann_on())
            ann_say("ONE", 0, ANN_NOW);
        return;
    }
    if (!r.on)
        return;
    if (r.beat == 0u && ann_on()) {      /* each runway bar: the count */
        char t[16], n[8];
        t[0] = 0;
        if (r.left == 1u)
            text_cat(t, "LAST BAR");
        else {
            int_text((int32_t)r.left, n);
            text_cat(t, n);
            text_cat(t, " BARS");
        }
        ann_say(t, 0, ANN_NOW);
    }
    if (r.left == 1u) {                  /* the last bar: a pulse each beat */
        if (!reduced) {
            dub.border = 1;
            dub.border_until = now + 150u;
            dub.pulses++;
        }
        if (ctl_get(P_RUNWAY_KEYS)) {
            dub.keys_until = now + 120u;
            dub.key_flashes++;
        }
    }
}

static void border_draw(int force)       /* the screen's 2 px edge: the runway's pulses */
{
    uint16_t c = dub.border == 2u ? C_GREEN : C_AMB;
    if (!dub.border && dub.border_drawn) {   /* off again: the regions under it repainted */
        dub.border_drawn = 0;
        dub.dirty_hdr = dub.dirty_strips = dub.dirty_centre = dub.dirty_foot = 1;
        if (dub.page == PAGE_STEPS)
            st.sig = 0;
        su_sig = 0;
        ks.sig = 0;
        fx_sig = 0;
        arr.sig = 0;
        perf_t = 0;
        return;
    }
    if (dub.border && (force || dub.border_drawn != dub.border)) {
        lcd_fill(0, 0, 240, 2, c);
        lcd_fill(0, 238, 240, 2, c);
        lcd_fill(0, 0, 2, 240, c);
        lcd_fill(238, 0, 2, 240, c);
        dub.border_drawn = dub.border;
    }
}

void dub_draw(void)
{
    uint32_t now = plat_ms(), b = eng_beat(), cm = cued_mask(), blink = blink_on();
    int drew = 0;
    demo_tick();                         /* (a demo's moves, once the engine has handed motion over) */
    slot_tick();                         /* (a loaded slot's, the same way) */
    song_sync();
    if (saver_draw(now)) {               /* the screen saver: the whole screen its own */
        lights();
        return;
    }
    if (eng_tier() != dub.tier_seen) {   /* the guard changed the tier: a badge and a word (design principles 5) */
        dub.tier_seen = eng_tier();
        dub.dirty_hdr = 1;
        app_say("TIER", tier_name(dub.tier_seen));
    }
    runway_track(now);
    if (dub.border && now >= dub.border_until)
        dub.border = 0;
    border_draw(0);
    int32_t li = lands_in();
    int up = now < msg.until;
    if (msg.changed || up != dub.msg_up) {
        msg.changed = 0;
        dub.msg_up = (uint8_t)up;
        dub.dirty_hdr = 1;
    }
    if (b != dub.last_beat) {
        dub.last_beat = b;
        dub.dirty_hdr = 1;
    }
    if (cm != dub.last_cued || li != dub.last_lands) {
        dub.last_cued = cm;
        dub.last_lands = li;
        dub.dirty_centre = dub.dirty_strips = 1;
    }
    if (cm && blink != dub.last_blink)
        dub.dirty_strips = 1;
    dub.last_blink = blink;
    if (now - dub.last_meter_t >= 50u) { /* the meters at 20 Hz */
        dub.last_meter_t = now;
        dub.dirty_strips = 1;
    }
    if (now - dub.centre_t >= 100u)      /* the echo panel (its limiter) at 10 Hz */
        dub.dirty_centre = 1;
    if (foot_sig() != dub.foot_sig) {
        dub.foot_sig = foot_sig();
        dub.dirty_foot = 1;
    }
    if (dub.dirty_hdr) {
        dub.dirty_hdr = 0;
        draw_header();
        drew = 1;
    }
    if (dub.page != dub.page_drawn) {    /* a page change: all of it */
        dub.page_drawn = dub.page;
        dub.dirty_strips = dub.dirty_centre = dub.dirty_foot = 1;
        if (dub.page == PAGE_STEPS)
            st.sig = 0;
        su_sig = 0;
        ks.sig = 0;
        fx_sig = 0;
        arr.sig = 0;
        perf_t = 0;
    }
    if (dub.page == PAGE_STEPS) {
        uint32_t sg = st.sig;
        steps_draw(0);
        drew |= sg != st.sig;
        dub.dirty_strips = dub.dirty_centre = dub.dirty_foot = 0;
    } else if (dub.page == PAGE_SETUP) {
        uint32_t sg = su_sig;
        setup_draw(0);
        drew |= sg != su_sig;
        dub.dirty_strips = dub.dirty_centre = dub.dirty_foot = 0;
    } else if (dub.page == PAGE_KEY) {
        uint32_t sg = ks.sig;
        keys_draw(0);
        drew |= sg != ks.sig;
        dub.dirty_strips = dub.dirty_centre = dub.dirty_foot = 0;
    } else if (dub.page == PAGE_FX) {
        uint32_t sg = fx_sig;
        fx_draw(0);
        drew |= sg != fx_sig;
        dub.dirty_strips = dub.dirty_centre = dub.dirty_foot = 0;
    } else if (dub.page == PAGE_ARRANGE) {
        uint32_t sg = arr.sig;
        arr_draw(0);
        drew |= sg != arr.sig;
        dub.dirty_strips = dub.dirty_centre = dub.dirty_foot = 0;
    } else if (dub.page == PAGE_PERF) {
        uint32_t pt = perf_t;
        perf_draw(dub.page_drawn != dub.page);
        drew |= pt != perf_t;
        dub.dirty_strips = dub.dirty_centre = dub.dirty_foot = 0;
    } else {
        if (dub.dirty_strips) {
            dub.dirty_strips = 0;
            draw_strips();
            drew = 1;
        }
        if (dub.dirty_centre) {
            dub.dirty_centre = 0;
            dub.centre_t = now;
            draw_centre();
            drew = 1;
        }
        if (dub.dirty_foot) {
            dub.dirty_foot = 0;
            draw_footer();
            drew = 1;
        }
    }
    if (drew && dub.border)              /* the regions drawn over the border: it again */
        border_draw(1);
    lights();
}

void dub_init(void)
{
    dub.dirty_hdr = dub.dirty_strips = dub.dirty_centre = dub.dirty_foot = 1;
    dub.last_lands = -1;
    dub.spring_add = -1;
}

/* a screen reader that has just started listening (or asks) hears the whole page */
void dub_speak(void)
{
    char t[160], n[16];
    uint32_t ch;
    int first;
    if (!ann_on() || !ann_where())
        return;
    t[0] = 0;
    text_cat(t, APP_NAME ". ");
    tempo_text(n);
    text_cat(t, n);
    text_cat(t, " BPM. ");
    text_cat(t, eng_playing() ? "PLAYING" : "STOPPED");
    if (dub.page == PAGE_STEPS) {
        steps_speak(t);
        ann_say(t, 0, ANN_NOW);
        return;
    }
    if (dub.page == PAGE_KEY) {
        text_cat(t, ". KEY ");
        text_cat(t, pitch_name(ctl_get(P_KEY_ROOT)));
        text_cat(t, " ");
        text_cat(t, scale_name(ctl_get(P_SCALE)));
        text_cat(t, ctl_get(P_LOCK) == LOCK_OFF ? ", LOCK OFF" : ctl_get(P_LOCK) == LOCK_SCALE ? ", LOCK ON" : ", CHORD LOCK");
        ann_say(t, 0, ANN_NOW);
        return;
    }
    if (dub.page == PAGE_SETUP) {
        text_cat(t, ". SETUP, ");
        text_cat(t, SU_NAME[su_cur]);
        ann_say(t, 0, ANN_NOW);
        return;
    }
    text_cat(t, ". QUANTISE ");
    text_cat(t, quant_name(ctl_get(P_QUANT)));
    for (first = 1, ch = 0; ch < NCH; ch++)
        if (eng_param(P_CH(ch, CHF_MUTE))) {
            text_cat(t, first ? ". MUTED " : ", ");
            text_cat(t, CH_NAME[ch]);
            first = 0;
        }
    for (first = 1, ch = 0; ch < NCH; ch++)
        if (eng_param(P_CH(ch, CHF_SOLO))) {
            text_cat(t, first ? ". SOLO " : ", ");
            text_cat(t, CH_NAME[ch]);
            first = 0;
        }
    ann_say(t, 0, ANN_NOW);
}

#ifdef PLAT_HOST
static int dub_expect(const char *what, int *got)
{
    if (!strncmp(what, "led_w", 5))      /* led_wN: white key N's (1-8): 1 dim, 2 bright, 3 blinking (cued) */
        *got = led_state((uint32_t)(atoi(what + 5) - 1) % NCH);
    else if (!strcmp(what, "vel"))       /* the last key's velocity */
        *got = dub.vel[dub.last_key];
    else if (!strcmp(what, "focus"))
        *got = dub.focus + 1;
    else if (!strcmp(what, "lands"))     /* beats to the next line */
        *got = lands_in();
    else if (!strncmp(what, "turns", 5)) /* turnsN: encoder N's count (plat.h EN_*) */
        *got = dub.turns[atoi(what + 5) % NE];
    else if (!strcmp(what, "page"))
        *got = dub.page;
    else if (!strcmp(what, "runway")) {  /* bars to the one while on the runway, else 0 */
        runway_t r = runway_now();
        *got = r.on ? (int)r.left : 0;
    } else if (!strcmp(what, "cycle_bar"))   /* this bar in the cycle, from 1 */
        *got = (int)runway_now().bar + 1;
    else if (!strcmp(what, "border"))    /* 0 off, 1 amber pulse, 2 green (ONE) */
        *got = dub.border;
    else if (!strcmp(what, "one"))       /* the ONE flash up */
        *got = plat_ms() < dub.one_until;
    else if (!strcmp(what, "key_flash"))
        *got = plat_ms() < dub.keys_until;
    else if (!strcmp(what, "pulses"))    /* the border's last-bar pulses so far */
        *got = (int)dub.pulses;
    else if (!strcmp(what, "key_flashes"))
        *got = (int)dub.key_flashes;
    else if (steps_expect(what, got) || keys_expect(what, got) || arr_expect(what, got) || saver_expect(what, got))
        return 1;
    else if (!strcmp(what, "roots"))     /* 1: the cycle bar red, gold and green (72.0 BPM's first bar) */
        *got = roots_now();
    else
        return setup_expect(what, got);
    return 1;
}
#endif
