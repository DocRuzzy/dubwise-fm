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
#define PAGE_H 144                       /* a full page, from STRIP_Y */
/* The canvas holds 76 rows (app.h, APP_CANVAS_PX; 144 until 2026-11-10: 32.6 KB of the pool back). A full page or the
 * strips are drawn in two halves: the same drawing twice, each pass offset by its half (gfx.c's cv_oy) and clipped to
 * it, then blitted. A body run twice must only draw: whatever it reads that can change (a meter) is read before */
#define CV_ROWS 76
#define CV_BANDS(h, bg)                                                                                                  \
    for (int32_t cvb_ = 0, cvh_ = ((int32_t)(h) + 1) / 2; cvb_ < 2; cvb_++)                                             \
        if ((cv_begin(240u, (uint32_t)cvh_, (bg)), cv_oy = -cvb_ * cvh_, cv_y0 = 0, cv_y1 = cvh_, 1))
#define CV_BAND_BLIT(y) cv_blit(0, (uint32_t)((y) + cvb_ * cvh_))
typedef char cv_bands_fit[(PAGE_H + 1) / 2 <= CV_ROWS && (STRIP_H + 1) / 2 <= CV_ROWS && CENTRE_H <= CV_ROWS ? 1 : -1];

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
static uint32_t undo_seq_next(void);             /* (undo.c: undo and redo) */
static void undo_key(int redo);
static void undo_mark(uint32_t tag, uint32_t now);
static void undo_reset(void);
static void undo_live_set(int on);
static void undo_join(void);
static int help_input(uint32_t *keys, uint32_t *btn, uint32_t now);   /* (help.c: help cards, tap tempo) */
static void help_turned(uint32_t enc);
static int help_ate_release(uint32_t k);
static void tap_tempo(uint32_t now);
static void slot_sx_tick(void);
static void steppers_load(void);
static void new_project(void);
static int pl_rename_done(void);                  /* (patches.c: a rename let go, v0.2) */
static uint32_t ml_count(void);                    /* (midilearn.c: MIDI learn) */
static void ml_learn_start(void);
static void ml_forget_all(void);
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
    uint32_t fill_t;                     /* 6.7: OCT+'s press (0: not held) */
    uint8_t fill_on, fill_latch, fill_skip;   /* FILL held by OCT+; latched; this release ignored */
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

static void upper(const char *a, char *t, uint32_t n)   /* a parameter's name as the screen shows it */
{
    uint32_t i;
    for (i = 0; a[i] && i + 1 < n; i++)
        t[i] = a[i] >= 'a' && a[i] <= 'z' ? (char)(a[i] - 32) : a[i];
    t[i] = 0;
}

#include "sound.c"                       /* the MAIN macros (GPL-3.0-only; roadmap 5.3) */

/* the SOUND view's banks (spec 4.9): 1 MAIN (the macros; the vox has none: its first raw bank), then the raw
 * fields four a bank */
static uint32_t sound_banks(int32_t ch)
{
    uint32_t nf = voice_fields(ch);
    return nf ? (uint32_t)has_main(ch) + (nf + 3u) / 4u + 2u : 0u;   /* (+ DICE (5.6), then STRIP, the last: 8.1) */
}
static int strip_bank(void)              /* 8.1: the SOUND view on the STRIP bank (the channel's EQ), the last */
{
    return dub.sound && voice_fields(dub.focus) && dub.sound == sound_banks(dub.focus);
}
static const uint8_t STRIP_F[4] = {CHF_LO, CHF_MID, CHF_MID_F, CHF_HI};
static int strip_id(uint32_t id)         /* 1: a channel's EQ field (8.1) */
{
    uint32_t f = (id - P_CH_BASE) % CH_STRIDE;
    return id >= P_CH_BASE && id < P_CH_BASE + NCH * CH_STRIDE && f >= CHF_LO && f <= CHF_FILT;
}
static const char *pan_name(int32_t v)   /* 8.9: "CENTRE", "L 20", "R 63" */
{
    static char t[12];
    char b[6];
    t[0] = 0;
    if (!v) {
        text_cat(t, "CENTRE");
        return t;
    }
    text_cat(t, v < 0 ? "L " : "R ");
    int_text(v < 0 ? -v : v, b);
    text_cat(t, b);
    return t;
}
static const char *chfilt_name(int32_t v)   /* a channel's filter: "OFF", "HP 500 HZ", "LP 2.5K" */
{
    static char t[16];
    t[0] = 0;
    if (!v) {
        text_cat(t, "OFF");
        return t;
    }
    text_cat(t, v > 0 ? "HP " : "LP ");
    text_cat(t, hz_name(v > 0 ? hpf_hz(v) : lpf_hz(-v)));
    return t;
}
static void strip_value(uint32_t id, char *t)   /* an EQ field as the screen says it: "+7 DB", "1.3K", "HP 500 HZ" */
{
    uint32_t f = (id - P_CH_BASE) % CH_STRIDE;
    int32_t v = ctl_get(id);
    t[0] = 0;
    if (f == CHF_FILT)
        text_cat(t, chfilt_name(v));
    else if (f == CHF_MID_F)
        text_cat(t, hz_name(chaneq_mid_hz(v)));
    else {
        char b[8];
        int32_t d = (v * 150 + (v >= 0 ? 32 : -32)) / 64, a = d < 0 ? -d : d;   /* (tenths of a dB, rounded) */
        if (!v) {
            text_cat(t, "FLAT");
            return;
        }
        text_cat(t, d > 0 ? "+" : "-");
        int_text(a / 10, b);
        text_cat(t, b);
        if (a < 100) {                   /* (under 10 dB: a tenth too) */
            b[0] = '.';
            b[1] = (char)('0' + a % 10);
            b[2] = 0;
            text_cat(t, b);
        }
        text_cat(t, " DB");
    }
}

static int32_t sound_id(uint32_t i)      /* the SOUND view's knob i: its parameter, -1 none */
{
    uint32_t f;
    if (!dub.sound)
        return -1;
    if (has_main(dub.focus) && dub.sound == 1u)
        return (int32_t)P_VOICE(dub.focus, VM_BASE + i);
    if (strip_bank())                    /* 8.1: LO, MID, FREQ, HI */
        return (int32_t)P_CH(dub.focus, STRIP_F[i & 3u]);
    f = ((uint32_t)dub.sound - 1u - (uint32_t)has_main(dub.focus)) * 4u + i;
    return f < voice_fields(dub.focus) ? (int32_t)P_VOICE(dub.focus, f) : -1;   /* (the DICE bank: none) */
}
#include "dice.c"                        /* Dice, the sound randomiser (GPL-3.0-only; roadmap 5.6) */

enum { PAGE_DUB, PAGE_STEPS, PAGE_SETUP, PAGE_KEY, PAGE_FX, PAGE_ARRANGE, PAGE_PERF };
#include "keys.c"                        /* the KEY & CHORDS page (GPL-3.0-only) */
#include "envview.c"                     /* the live envelope view (GPL-3.0-only; roadmap 5.10) */
/* "press again": an action that can't be undone asks once more (the 2026-11 QoL review 5). The same action again
 * within 3 s does it; anything else meanwhile is its own action, and the ask lapses */
static struct {
    uint32_t what, until;
} cf;
static int confirm(uint32_t what, const char *name)   /* 1: confirmed (the second ask) */
{
    uint32_t now = plat_ms();
    if (cf.what == what && now < cf.until) {
        cf.what = 0;
        return 1;
    }
    cf.what = what;
    cf.until = now + 3000u;
    app_say(name, "AGAIN TO CONFIRM");
    return 0;
}
enum { CF_MOTION = 1, CF_DEMO, CF_STEPPERS, CF_NEW, CF_FORGET, CF_SHOTS, CF_SLOT_SAVE = 0x10, CF_SLOT_LOAD = 0x20,
       CF_PATCH_FOREIGN = 0x30, CF_PATCH_OVER = 0x40, CF_KIT = 0x80 };

static uint32_t blink_on(void);
#include "kits.c"                        /* factory kits (GPL-3.0-only; roadmap 5.5) */
#include "steps.c"                       /* the STEPS page (GPL-3.0-only) */
#include "setup.c"                       /* the SETUP page (GPL-3.0-only) */
static void fx_next(int same_page);      /* (fx.c, included further down: it needs the echo's helpers) */
static void arr_mark(void);              /* (arrange.c: its page redrawn) */

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

/* 6.7: OCT+ (on every page but STEPS) is the quantise's step on a tap, at its let-go; held 300 ms it's FILL, until its
 * let-go. OCT- tapped while FILL is held latches it (a double tap would catch the quantise's quick taps); then a tap of
 * OCT+ lets the latch go */
#define FILL_HOLD_MS 300u
static void fill_set(int on)
{
    if ((ctl_get(P_FILL) != 0) == (on != 0))
        return;
    ctl_set(P_FILL, on ? 1 : 0);
    dub.dirty_hdr = 1;
    app_say("FILL", on ? "ON" : "OFF");
}
static void fill_press(uint32_t now)
{
    dub.fill_t = now ? now : 1u;
    dub.fill_skip = 0;
    if (dub.fill_latch) {                /* a tap lets the latch go */
        dub.fill_latch = 0;
        dub.fill_skip = 1;
        fill_set(0);
    }
}
static int fill_latch_now(void)          /* OCT- pressed: 1 if it latched FILL (OCT+ holding it) */
{
    if (!dub.fill_on)
        return 0;
    dub.fill_latch = 1;
    app_say("FILL", "LATCHED");
    return 1;
}
static void fill_tick(uint32_t now)      /* (each input) OCT+ held long enough: FILL */
{
    static int32_t seen;
    if (ctl_get(P_FILL) != seen) {       /* (FILL from a learned CC or the song's motion: the header too) */
        seen = ctl_get(P_FILL);
        dub.dirty_hdr = 1;
    }
    if (dub.fill_t && !dub.fill_on && !dub.fill_skip && now - dub.fill_t >= FILL_HOLD_MS) {
        dub.fill_on = 1;
        fill_set(1);
    }
}
static void fill_release(void)
{
    if (!dub.fill_t)
        return;
    dub.fill_t = 0;
    if (dub.fill_on) {                   /* a hold: FILL let go, unless latched */
        dub.fill_on = 0;
        if (!dub.fill_latch)
            fill_set(0);
    } else if (!dub.fill_skip)           /* a tap: the quantise's step */
        quant_step(1);
    dub.fill_skip = 0;
}

static const char *bk_name(int32_t step)          /* the Big Knob's step as it sounds now: a high-pass's or (7.5) a
                                                  * low-pass's cutoff */
{
    return ctl_get(P_HPF_LP) && step > 0 && step <= HPF_STEPS ? hz_name(lpf_hz(step)) : hpf_name(step);
}

static int32_t held_mute(void);          /* (below: the mute key held, -1 none) */
static uint32_t held_mutes(void);        /* (below, 8.1: every mute key held, a bit a channel) */
static void hold_edits(uint32_t m);      /* (the held keys for editing: their mutes as they were) */
static void chfilt_off(uint32_t m);      /* (8.1: the held channels' filters out) */
static void button_press(uint32_t k)
{
    if ((k == B_OCTDN || k == B_OCTUP) && dub.page == PAGE_STEPS)
        steps_oct(k == B_OCTUP ? 1 : -1);   /* the STEPS page: its page of 16 */
    else if (k == B_OCTDN && !fill_latch_now())
        quant_step(-1);
    else if (k == B_OCTUP)
        fill_press(plat_ms());
    else if (k == B_SEQ && (dub.btn >> B_HOME & 1u)) {   /* HOME + SEQ, tapped: tap tempo */
        dub.home_used = 1;
        tap_tempo(plat_ms());
    } else if (k == B_SEQ && dub.page == PAGE_STEPS && st.kb) {   /* SEQ on the keyboard: the steps again */
        kb_off();
        app_say("STEPS", CH_NAME[st.ch]);
    } else if (k == B_SEQ && dub.page == PAGE_DUB && held_mute() >= 0) {   /* a mute key held + SEQ: STEPS on that
                                                                         * channel's patterns (5.11, spec 4.11) */
        int32_t ch = held_mute();
        if (!dub.dbl[ch]) {              /* (the hold is for choosing: the mute stays as it was) */
            dub.dbl[ch] = 1;
            ctl_cue(P_CH(ch, CHF_MUTE), dub.m0[ch], act_beats());
        }
        st.ch = ch;
        st.last = -1;
        dub.page = PAGE_STEPS;
        app_say("STEPS", CH_NAME[ch]);
    } else if (k == B_SEQ) {
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
    } else if (k == B_REC && dub.page == PAGE_STEPS && !(dub.btn >> B_HOME & 1u)) {
        steps_rec();
    } else if (k == B_REC && (dub.btn >> B_SAVE & 1u)
               && (dub.page == PAGE_DUB || dub.page == PAGE_ARRANGE)) {   /* SAVE + REC: the motion cleared (P2.7) */
        dub.save_used = 1;
        if (!confirm(CF_MOTION, "CLEAR MOTION"))
            return;
        ctl_motion_clear();
        set_dirty = 1;
        arr_mark();
        app_say("MOTION", "CLEARED");
    } else if (k == B_REC && (dub.btn >> B_HOME & 1u)) {   /* HOME + REC: undo (the last song capture, if latest) */
        dub.home_used = 1;
        undo_key(0);
    } else if (k == B_GLO && (dub.btn >> B_HOME & 1u)) {   /* HOME + GLO: redo */
        dub.home_used = 1;
        undo_key(1);
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
    else if (k == B_EDIT && (dub.btn >> B_HOME & 1u)) {   /* HOME + EDIT: Dice on the focused channel (5.6) */
        dub.home_used = 1;
        dice_fire((uint32_t)dub.focus);
    } else if (k == B_ENV && dub.page == PAGE_STEPS && (dub.btn >> B_HOME & 1u)) {   /* HOME + ENV: RESEED (5.9) */
        dub.home_used = 1;
        dc.env_used = 1;
        steps_reseed();
    } else if (k == B_ENV)               /* (released: Dice fires; held + a knob: a hold; on STEPS + a white: REPEAT) */
        dc.env_used = 0;
    else if (k == B_EDIT && dub.page == PAGE_DUB && held_mute() >= 0 && voice_fields(held_mute())) {
        hold_edits(held_mutes());        /* 8.1: a channel's key held + EDIT: its STRIP at once (no banks to step
                                          * through); KNOB 3 then sweeps the mid */
        dub.focus = held_mute();
        dub.sound = (uint8_t)sound_banks(dub.focus);
        dub.dirty_foot = dub.dirty_centre = dub.dirty_strips = 1;
        {
            char t[20];
            t[0] = 0;
            text_cat(t, CH_NAME[dub.focus]);
            text_cat(t, " STRIP");
            app_say("SOUND", t);
        }
    } else if (k == B_EDIT) {              /* the SOUND view, a bank at a time, then back */
        uint32_t nf = voice_fields(dub.focus);
        dub.sound = (uint8_t)(dub.sound + 1u);
        if (dub.sound > sound_banks(dub.focus))
            dub.sound = 0;
        dub.dirty_foot = dub.dirty_centre = 1;
        if (dub.sound) {                 /* "SOUND" "KICK MAIN" / "KICK 2" (spec 4.9) */
            char t[20], b[4];
            t[0] = 0;
            text_cat(t, CH_NAME[dub.focus]);
            if (dub.sound == 1u && has_main(dub.focus))
                text_cat(t, " MAIN");
            else if (dice_bank())
                text_cat(t, " DICE");
            else if (strip_bank())
                text_cat(t, " STRIP");   /* (8.1) */
            else {
                int_text((int32_t)dub.sound - has_main(dub.focus), b);
                text_cat(t, " ");
                text_cat(t, b);
            }
            app_say("SOUND", t);
        }
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
    if (k == B_OCTUP)
        fill_release();                  /* (never a help card: held, OCT+ is FILL) */
    if (k == B_SEL && dub.page == PAGE_DUB && held_mute() >= 0) {   /* 8.1: with channel keys held: their filters
                                                                      * out at once (the snap back to bypass) */
        hold_edits(held_mutes());
        chfilt_off(held_mutes());
        return;
    }
    if (k == B_SEL) {                    /* 7.5: the Big Knob pressed: high-pass or low-pass (the 9068B) */
        ctl_set(P_HPF_LP, !ctl_get(P_HPF_LP));
        set_dirty = 1;
        dub.dirty_centre = 1;
        app_say(roots_name("BIG KNOB"), ctl_get(P_HPF_LP) ? "LOW-PASS" : "HIGH-PASS");
        return;
    }
    if (help_ate_release(k))             /* its help card shown: nothing more */
        return;
    if (k == B_ENV && !dc.env_used) {    /* ENV: Dice fires, on its bank (5.6) */
        if (dice_bank())
            dice_fire((uint32_t)dub.focus);
        else if (dub.page == PAGE_STEPS)
            app_say("ENV", "HOLD + A WHITE KEY: REPEAT");
        else
            app_say("ENV", "DICE: EDIT TO ITS BANK");
        return;
    }
    if (k == B_PLAY && !dub.play_used) {
        static uint32_t stop_t;          /* when PLAY last stopped the transport, 0: not just now */
        uint32_t now = plat_ms();
        int on = !ctl_get(P_PLAY);
        if (on && stop_t && now - stop_t < 400u) {   /* stop twice (roadmap 5.2, spec 4.1.3): tails off, no start */
            stop_t = 0;
            ctl_set(P_TAILS, 1);
            app_say("TAILS OFF", "");
            return;
        }
        ctl_set(P_PLAY, on);
        stop_t = on ? 0u : (now ? now : 1u);
        app_say(on ? "PLAY" : "STOP", "");
    } else if (k == B_HOME && !dub.home_used) {   /* HOME: the DUB page; again: ARRANGE (spec 4) */
        dub.page = dub.page == PAGE_DUB ? PAGE_ARRANGE : PAGE_DUB;
        arr_mark();
        app_say(dub.page == PAGE_ARRANGE ? "ARRANGE" : "DUB", "PAGE");
    } else if (k == B_SAVE && pl_rename_done()) {   /* (a rename let go: kept, v0.2) */
    } else if (k == B_SAVE && !dub.save_used && dub.page == PAGE_SETUP && setup_slot_row() >= 0) {
        slot_action(setup_slot_row());   /* SETUP's slot rows: SAVE does it */
    } else if (k == B_SAVE && !dub.save_used && dub.page == PAGE_SETUP && su_cur == SU_KIT) {
        if (!set_dirty || confirm(CF_KIT, "LOAD KIT OVER CHANGES"))   /* (5.5: asked twice over unsaved ones) */
            kit_load(kit_sel % NKITS, kit_bus);
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
    if (dub.sound && tc < 0 && ch < 0 && dice_bank()) {   /* the DICE bank (5.6) */
        dice_value(i, t);
        return;
    }
    if (dub.sound && tc < 0 && ch < 0) { /* the SOUND view */
        int32_t id = sound_id(i);
        if (id >= 0 && strip_id((uint32_t)id))
            strip_value((uint32_t)id, t);   /* (8.1) */
        else if (id >= 0 && model_name((uint32_t)id, ctl_get((uint32_t)id)))
            text_cat(t, model_name((uint32_t)id, ctl_get((uint32_t)id)));   /* (a model by its name: 5.7) */
        else if (id >= 0 && organ_bar((uint32_t)id, ctl_get((uint32_t)id), t))
            ;                                    /* (a drawbar as the bar, 0..8: 5.15) */
        else if (id >= 0)
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
            text_cat(t, chfilt_name(ctl_get(P_CH(ch, CHF_FILT))));   /* 8.1: the channel's filter */
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
#include "patches.c"                     /* sound patches (GPL-3.0-only; roadmap 5.4) */
#include "shots.c"                       /* your own one-shots (GPL-3.0-only; roadmap 5.14) */

static void sound_title(char *t2)        /* the SOUND view's title: the patch shown (* edited since) and the bank */
{
    patch_t pp;
    t2[0] = 0;
    if (pl_get((uint32_t)dub.focus, pl_pos[dub.focus], &pp)) {
        pl_name(&pp, t2);
        if (pl_edited((uint32_t)dub.focus))
            text_cat(t2, "*");
    } else
        text_cat(t2, CH_NAME[dub.focus]);
    if (dub.sound == 1u && has_main(dub.focus))
        text_cat(t2, "  MAIN");
    else if (dice_bank())
        text_cat(t2, "  DICE");
    else if (strip_bank())
        text_cat(t2, "  STRIP");
    else {
        char b[4];
        int_text((int32_t)dub.sound - has_main(dub.focus), b);
        text_cat(t2, "  BANK ");
        text_cat(t2, b);
    }
}
#include "undo.c"                        /* undo and redo (GPL-3.0-only) */
#include "help.c"                        /* help cards and tap tempo (GPL-3.0-only) */
#include "midilearn.c"                   /* MIDI learn and the USB MIDI host (GPL-3.0-only) */

static uint32_t held_mutes(void)          /* 8.1: every mute key held, a bit a channel */
{
    uint32_t ch, m = 0;
    for (ch = 0; ch < NCH; ch++)
        if (dub.keys >> WK[ch] & 1u)
            m |= 1u << ch;
    return m;
}
static void hold_edits(uint32_t m)       /* the held keys are for editing: their mutes stay as they were */
{
    uint32_t ch;
    for (ch = 0; ch < NCH; ch++)
        if ((m >> ch & 1u) && !dub.dbl[ch]) {
            dub.dbl[ch] = 1;
            ctl_cue(P_CH(ch, CHF_MUTE), dub.m0[ch], act_beats());
        }
}
static void chfilt_say(uint32_t m)       /* "BASS FILTER" / "3 FILTERS", then the first one's setting */
{
    uint32_t ch, n = 0, first = 0;
    char t[24], b[4];
    for (ch = NCH; ch-- > 0;)
        if (m >> ch & 1u) {
            n++;
            first = ch;
        }
    t[0] = 0;
    if (n == 1u)
        text_cat(t, CH_NAME[first]);
    else {
        int_text((int32_t)n, b);
        text_cat(t, b);
    }
    text_cat(t, n == 1u ? " FILTER" : " FILTERS");
    app_say(t, chfilt_name(ctl_get(P_CH(first, CHF_FILT))));
    dub.dirty_foot = 1;
    dub.dirty_centre = 1;
}
static void chfilt_step(uint32_t m, int32_t e)   /* 8.1: the held channels' filters a detent: up the high-pass, down
                                                 * through off into the low-pass (closing as it goes down). Held
                                                 * together (the kick, rim and hats): the drums' filter, as one */
{
    int32_t s = hpf_detent(ctl_get(P_HPF_RES));
    uint32_t ch;
    for (ch = 0; ch < NCH; ch++)
        if (m >> ch & 1u) {
            int32_t v = ctl_get(P_CH(ch, CHF_FILT));
            v = e > 0 ? (v >= 0 ? (v / s + e) * s : -(((-v) + s - 1) / s - e) * s)
                      : (v > 0 ? ((v + s - 1) / s + e) * s : -((-v) / s - e) * s);
            ctl_set(P_CH(ch, CHF_FILT), v);
        }
    chfilt_say(m);
}
static void chfilt_off(uint32_t m)       /* 8.1: the held channels' filters out at once (the Big Knob pressed) */
{
    uint32_t ch;
    for (ch = 0; ch < NCH; ch++)
        if (m >> ch & 1u)
            ctl_set(P_CH(ch, CHF_FILT), 0);
    chfilt_say(m);
}

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
    if (dub.sound && ch < 0 && (dub.btn >> B_SAVE & 1u) && i < 2u) {   /* SAVE held + KNOB 1 / 2: rename (v0.2) */
        pl_rename_knob(i, e);
        return;
    }
    if (dub.sound && ch < 0 && dice_bank()) {   /* the DICE bank (5.6) */
        dice_knob(i, e);
        return;
    }
    if (dub.sound && ch < 0 && (dub.btn >> B_ENV & 1u)) {   /* ENV held + a knob: hold its field (5.6) */
        dice_hold_knob(i);
        return;
    }
    if (dub.sound && ch < 0) {           /* the SOUND view: the focused voice */
        char l[12];
        int32_t sid = sound_id(i);
        if (sid < 0)
            return;
        ctl_set((uint32_t)sid, ctl_get((uint32_t)sid) + e);
        {
            int32_t mch;
            uint32_t mk;
            if (macro_of((uint32_t)sid, &mch, &mk))   /* a macro: its raw targets along its curves */
                macro_apply(mch, mk);
        }
        set_dirty = 1;
        dub.dirty_centre = 1;            /* (the envelopes drawn again: 5.10) */
        label_of((uint32_t)sid, l, sizeof l);
        int_text(ctl_get((uint32_t)sid), t);
        organ_bar((uint32_t)sid, ctl_get((uint32_t)sid), t);
        if (strip_id((uint32_t)sid))
            strip_value((uint32_t)sid, t);   /* (8.1) */
        app_say(l, model_name((uint32_t)sid, ctl_get((uint32_t)sid)) ? model_name((uint32_t)sid, ctl_get((uint32_t)sid)) : t);
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
        else {                           /* 8.1: KNOB 4, the channel's own filter (every key held: theirs) */
            hold_edits(held_mutes());
            chfilt_step(held_mutes(), e);
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
    } else if (k == EN_SELECT && dub.page == PAGE_DUB && held_mute() >= 0) {   /* 8.1: channel keys held: their own
                                                                               * filters, on the Big Knob's steps */
        hold_edits(held_mutes());        /* (the hold is for editing: the mutes stay as they were) */
        chfilt_step(held_mutes(), e);    /* (several held: their filters together, the drums' as one) */
    } else if (k == EN_SELECT && dub.page != PAGE_SETUP) {   /* the Big Knob: a detent a step, at once */
        int32_t s = hpf_detent(ctl_get(P_HPF_RES)), v = ctl_get(P_HPF_STEP);
        v = e > 0 ? (v / s + e) * s : ((v + s - 1) / s + e) * s;   /* (off the setting's grid: to it first) */
        ctl_set(P_HPF_STEP, v);
        dub.dirty_centre = 1;
        app_say(roots_name("BIG KNOB"), bk_name(ctl_get(P_HPF_STEP)));
    } else if (k == EN_ALGO && dub.page == PAGE_DUB && held_mute() >= 0) {   /* 8.9: a channel's key held + FOCUS:
                                                                            * its pan (every key held: theirs) */
        uint32_t m = held_mutes(), ch, first = NCH;
        char t[24];
        hold_edits(m);
        for (ch = 0; ch < NCH; ch++)
            if (m >> ch & 1u) {
                ctl_set(P_CH(ch, CHF_PAN), ctl_get(P_CH(ch, CHF_PAN)) + 2 * e);
                if (first == NCH)
                    first = ch;
            }
        t[0] = 0;
        text_cat(t, CH_NAME[first]);
        text_cat(t, " PAN");
        app_say(t, pan_name(ctl_get(P_CH(first, CHF_PAN))));
        dub.dirty_strips = 1;
    } else if (k == EN_ALGO) {
        int strip = strip_bank();        /* (8.1: on STRIP, the next channel's STRIP: EQ across the desk) */
        dub.focus = (dub.focus + e % (int32_t)NCH + (int32_t)NCH) % (int32_t)NCH;
        if (dub.sound)
            dub.sound = !voice_fields(dub.focus) ? 0u : strip ? (uint8_t)sound_banks(dub.focus) : 1u;
        dub.dirty_strips = 1;
        app_say("FOCUS", CH_NAME[dub.focus]);
    } else if (dub.page == PAGE_ARRANGE && k >= EN_K1 && k <= EN_K4) {
        arr_knob(k - EN_K1, e);
    } else if (k == EN_PRESET && dub.page == PAGE_DUB && dub.sound && held_mute() < 0) {   /* the SOUND view: patches
                                                                                          * (5.4); SAVE held: store */
        if (dub.btn >> B_SAVE & 1u) {
            dub.save_used = 1;
            pl_save_key();
        } else if (dice_bank() && (dub.btn >> B_ENV & 1u)) {   /* (the DICE bank, ENV held: MORPH's B from the
                                                          * library, v0.2) */
            dc.env_used = 1;
            pl_morph_pick(e);
        }
        else
            pl_browse(e);
    } else if (k == EN_PRESET && dub.page == PAGE_DUB && held_mute() >= 0) {   /* a mute key held + PRESET: that
                                                                                * channel's pattern, on the line (4.8) */
        int32_t ch = held_mute();
        if (!dub.dbl[ch]) {              /* (the hold is for choosing: the mute stays as it was) */
            dub.dbl[ch] = 1;
            ctl_cue(P_CH(ch, CHF_MUTE), dub.m0[ch], act_beats());
        }
        pat_launch(ch, (uint32_t)(((int32_t)pat_now(ch) + e % (int32_t)NPAT + (int32_t)NPAT) % (int32_t)NPAT), 0);
        dub.dirty_strips = 1;
    } else if (k == EN_PRESET && dub.page == PAGE_DUB) {   /* PRESETS on the DUB page: cue the next scene (spec 4.1) */
        arr_step(e);
    } else if (dub.page == PAGE_FX && k >= EN_K1 && k <= EN_K4) {
        fx_knob(k - EN_K1, e);
    } else if (dub.page == PAGE_KEY && k >= EN_K1 && k <= EN_K4) {
        keys_knob(k - EN_K1, e);
    } else if (dub.page == PAGE_SETUP && k == EN_K3 && su_cur == SU_KIT) {   /* LOAD KIT: VOICES / + BUS (5.5) */
        kit_bus = (uint8_t)(e > 0);
        setup_say();
    } else if (dub.page == PAGE_SETUP && k == EN_K3 && su_cur == SU_SLOT_LOAD) {   /* LOAD SLOT: WHOLE / AS NEW */
        slot_fresh = (uint8_t)(e > 0);
        setup_say();
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

#include "smc.c"                         /* the SMC-Mixer as the desk (GPL-3.0-only; roadmap 6.5) */

static void dub_input_hands(void);
void dub_input(void)                     /* (what the hands change goes into the undo history) */
{
    undo_live_set(1);
    dub_input_hands();
    smc_tick();                          /* (the SMC-Mixer's: as the hands', 6.5) */
    undo_live_set(0);
}

static void dub_input_hands(void)
{
    uint32_t keys, btn = plat_buttons(), k, now = plat_ms();
    int32_t e;
    touch_poll();                        /* the sensed keys' presses and velocities */
    keys = touch_keys();
    rec_tick();                          /* live recording (steps.c): the pass's line and end, the click */
    if (saver_input(&keys, &btn, now))   /* the screen saver up: a touch only wakes it */
        return;
    help_input(&keys, &btn, now);        /* a button held alone: its help card (help.c) */
    fill_tick(now);
    if ((keys & ~dub.keys) || (btn & ~dub.btn)) {   /* a press: a new step of the undo history */
        static uint32_t presses;
        undo_mark(0x1000u + ++presses, now);
    }
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
                {                        /* a parameter (5.1): the song's motion captures it; the toggle makes two
                                          * presses of the same shot two changes */
                    uint32_t vel = touch_vel(BK[k]) ? touch_vel(BK[k]) : 100u;
                    int32_t t = (ctl_get(P_SHOT) >> 11 & 1) ^ 1;
                    ctl_set(P_SHOT, (int32_t)(k - 7u) + (rev ? 8 : 0) + (int32_t)((vel & 127u) << 4) + (t << 11));
                }
                app_say(rev ? "REVERSE" : "ONE SHOT", fxv_shot_name(k - 7u));
                continue;
            }
            if (k == 5u || k == 6u)      /* the sirens A, B, while held (P2.1): a parameter, captured (5.1) */
                ctl_set(P_SIREN, (k == 5u ? SIREN_A : SIREN_B) + 1
                                     + (int32_t)(((touch_vel(BK[k]) ? touch_vel(BK[k]) : 100u) & 127u) << 4));
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
            if ((ctl_get(P_SIREN) & 15) == (k == 5u ? SIREN_A : SIREN_B) + 1)
                ctl_set(P_SIREN, 0);             /* (the other siren, if it took over, plays on) */
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
            help_turned(k);
            undo_mark(0x200u + k, now);  /* (a knob's turns run together) */
            encoder(k, e);
        }
    if (keys != dub.keys)
        dub.dirty_strips = 1;
    dub.keys = keys;
}

/* ----------------------------------------------------------------- lights --- */
/* one LED language (spec 2.7): bright = on / playing, dim = available / muted, slow blink = cued */
static uint32_t blink_on(void) { return (plat_ms() / BLINK_MS) & 1u; }

static int led_play;                     /* (tests) the PLAY lights: 1 green, 2 the other, 3 both */
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
    if (eng_playing()) {                 /* PLAY flashes on the beat (a visual metronome; QoL review 5): green for a
                                          * fifth of each beat, both colours on a bar's first */
        int32_t f = eng_fine();
        if (f >= 0 && f % (int32_t)PPQ < (int32_t)PPQ / 5) {
            bb |= LED_PLAY_GREEN;
            if (f % (4 * (int32_t)PPQ) < (int32_t)PPQ / 5)
                bb |= 1u << B_PLAY;
        }
    }
    if (set_dirty)
        bb |= 1u << B_SAVE;
    led_play = ((bb & LED_PLAY_GREEN) ? 1 : 0) | ((bb >> B_PLAY & 1u) ? 2 : 0);
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

static int bat_now = -1;                 /* plat_battery, read once a second (dub_draw) */
static void outline(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c);
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
        if (ctl_get(P_FILL)) {           /* FILL held (6.7) */
            cv_rect(x + 3, 5, text_w(&FONT_T, "FILL") + 4, 14, C_HI);
            x = cv_text(x + 5, 6, &FONT_T, "FILL", C_BLACK);
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
        if (bat_now >= 0 && bat_now <= 3) {   /* the battery (QoL review 5): three bars; red when empty. On USB's power
                                               * (4) or not read (-1): none */
            int32_t bx2 = 236 - text_w(&FONT_T, t) - 19, k;
            uint16_t c = bat_now == 0 ? C_RED : C_GRAY;
            outline(bx2, 9, 13, 8, c);
            cv_rect(bx2 + 13, 11, 2, 4, c);
            for (k = 0; k < bat_now; k++)
                cv_rect(bx2 + 2 + k * 3, 11, 2, 4, C_GRAY);
            if (bat_now == 0)
                cv_rect(bx2 + 2, 11, 1, 4, C_RED);
        }
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
    int solo_any = any_solo(1), blink = blink_on();
    float pk[NCH];                       /* (read once: both halves draw the same meters) */
    for (ch = 0; ch < NCH; ch++)
        pk[ch] = eng_ch_peak(ch);
    CV_BANDS(STRIP_H, C_BLACK) {                 /* (in two halves: the canvas holds 76 rows) */
        for (ch = 0; ch < NCH; ch++) {
            int32_t x = 1 + (int32_t)ch * 30, bx = x + 5, by = 30, bw = 18, bh = 46;
            int on = audible(ch), soloed = eng_param(P_CH(ch, CHF_SOLO)) != 0;
            int thrown = eng_param(P_CH(ch, CHF_THROW)) != 0;
            char num[2] = {(char)('1' + ch), 0};
            const char *dg = DEST_NAME[eng_param(P_CH(ch, CHF_THROW_DEST))];
            if (thrown)                      /* a throw flashes its channel (spec 5.2) */
                cv_rect(x + 4, 0, 20, 17, C_HI);
            cv_text(x + (28 - text_w(&FONT_B, num)) / 2, 0, &FONT_B, num, thrown ? C_BLACK : on ? C_WHITE : C_GRAY);
            {                            /* 8.1: the strip's EQ or filter in: its name amber, underlined */
                uint32_t f;
                int32_t nw = text_w(&FONT_T, CH_SHORT[ch]);
                for (f = CHF_LO; f <= CHF_FILT; f++)
                    if (eng_param(P_CH(ch, f)))
                        break;
                cv_text(x + (28 - nw) / 2, 17, &FONT_T, CH_SHORT[ch], f <= CHF_FILT ? C_AMB : on ? C_GRAY : C_DIM);
                if (f <= CHF_FILT)
                    cv_rect(x + (28 - nw) / 2, 26, nw, 1, C_AMB);
            }
            {                                /* its pattern (4.8): amber while one launched waits for its line */
                uint32_t pw = (uint32_t)ctl_get(P_CH(ch, CHF_PAT)) & 3u;
                char l[2] = {(char)('A' + pw), 0};
                cv_text(x + 21, 3, &FONT_T, l, eng_pat(ch) != pw ? C_AMB : pw ? C_GRAY : C_DIM);
            }
            if (on) {                        /* filled = playing; the meter brighter over it */
                int32_t m = meter_px(pk[ch], bh);
                cv_rect(bx, by, bw, bh, C_LINE);
                if (m > 0)
                    cv_rect(bx, by + bh - (m > bh ? bh : m), bw, m > bh ? bh : m, C_HI);
            } else if (solo_any && !soloed && !eng_param(P_CH(ch, CHF_MUTE))) {
                hatch(bx, by, bw, bh, C_DIM);    /* hatched = solo-muted */
                outline(bx, by, bw, bh, C_DIM);
            } else {
                outline(bx, by, bw, bh, C_GRAY);   /* outlined = muted */
            }
            if (cued(ch) && blink)
                outline(bx - 2, by - 2, bw + 4, bh + 4, C_WHITE);
            cv_text(x + 3, by + bh + 3, &FONT_T, dg, thrown ? C_HI : C_DIM);   /* the throw's destination */
            if (soloed)
                cv_text(x + 19, by + bh + 3, &FONT_T, "S", C_AMB);

            if ((int32_t)ch == dub.focus)
                cv_rect(x + 3, STRIP_H - 2, 22, 2, C_GRAY);
            {                            /* 8.9: the pan, a tick along the strip's foot (none at centre) */
                int32_t pn = eng_param(P_CH(ch, CHF_PAN));
                if (pn)
                    cv_rect(x + 3 + (pn + 64) * 19 / 127, STRIP_H - 2, 3, 2, C_AMB);
            }
        }
        CV_BAND_BLIT(STRIP_Y);
    }
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
    if (env_view()) {                    /* the SOUND view: the focused voice's envelopes (5.10) */
        env_draw();
        cv_rect(0, 0, 240, 1, C_LINE);
        cv_blit(0, CENTRE_Y);
        return;
    }
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
        cv_text(126, 4, &FONT_XS, ctl_get(P_HPF_ON) == HPF_ON_ECHO ? (ctl_get(P_HPF_LP) ? "BIG KNOB LP: ECHO"
                                    : "BIG KNOB: ECHO") : ctl_get(P_HPF_LP) ? "BIG KNOB LP" : "BIG KNOB", C_GRAY);
        for (k = 1; k <= nd; k++) {
            float a = 3.14159265f * (1.0f - (float)(k - 1) / (float)(nd - 1));   /* left to right */
            int32_t x = 180 + (int32_t)(24.0f * fm_cosf(a)), y = 42 - (int32_t)(22.0f * fm_sinf(a));
            cv_rect(x - dot / 2, y - dot / 2, dot, dot, k * s <= stp ? C_HI : C_LINE);
        }
        cv_text(180 - text_w(&FONT_B, bk_name(stp)) / 2, 44, &FONT_B, bk_name(stp), stp ? C_WHITE : C_DIM);
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
            int32_t sid = sound_id(i), mch;
            uint32_t mk;
            l[0] = 0;
            if (dice_bank())
                dice_label(i, l);
            else if (sid >= 0)
                label_of((uint32_t)sid, l, sizeof l);
            {
                int32_t lx = cv_text(x, 4, &FONT_XS, l, C_AMB);
                if (sid >= 0 && macro_of((uint32_t)sid, &mch, &mk) && macro_moved(mch, mk))
                    cv_rect(lx + 2, 9, 3, 3, C_WHITE);   /* a dot: a target turned since (spec 4.9) */
                if (!dice_bank() && dice_held(i)) {   /* a lock: Dice leaves it alone (5.6) */
                    cv_rect(lx + 7, 7, 5, 4, C_HI);
                    cv_rect(lx + 8, 4, 1, 3, C_HI);
                    cv_rect(lx + 10, 4, 1, 3, C_HI);
                    cv_rect(lx + 8, 4, 3, 1, C_HI);
                }
            }
        } else
            cv_text(x, 4, &FONT_XS, ch >= 0 ? FOCUS_LABEL[i] : KNOB_LABEL[i], ch >= 0 ? C_AMB : C_GRAY);
        knob_value(i, ch, t);
        cv_text(x, 18, &FONT_B, t, i == 1u && ch < 0 && eng_echo_g() >= 1.0f ? C_RED : C_HI);
    }
    if (ch >= 0)
        cv_text(234 - text_w(&FONT_T, CH_NAME[ch]), 4, &FONT_T, CH_NAME[ch], C_AMB);
    cv_blit(0, FOOT_Y);
}

static uint32_t foot_sig(void)           /* changes whenever what the footer shows does */
{
    uint32_t h = ((uint32_t)(held_mute() + 1) * 17u + (uint32_t)(held_throw() + 1)) * 7u + dub.sound * 131u
                 + (uint32_t)dub.focus * 1031u + (uint32_t)(pl_pos[dub.focus] + 1) * 7919u, i;
    for (i = 0; i < 4u; i++) {
        char t[16], *p;
        knob_value(i, held_mute(), t);
        for (p = t; *p; p++)
            h = h * 31u + (uint8_t)*p;
        if (dub.sound == 1u && has_main(dub.focus))
            h = h * 3u + (uint32_t)macro_moved(dub.focus, i);
    }
    if (dub.sound && pl_pos[dub.focus] >= 0)
        h = h * 5u + (uint32_t)pl_edited((uint32_t)dub.focus);
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
    desk_tick();                         /* (8.1: a desk move since the save lights SAVE) */
    slot_tick();                         /* (a loaded slot's, the same way) */
    slot_sx_tick();                      /* (a slot going to a computer) */
    sh_sx_tick();                        /* (the one-shots going to one: 5.14) */
    env_tick(now);                       /* (the envelope view's playhead, 5.10) */
    organ_tick();                        /* (the B-3's tables, when its drawbars moved: 5.15) */
    pl_sx_tick();                        /* (a channel's patches going to a computer) */
    song_sync();
    if (saver_draw(now)) {               /* the screen saver: the whole screen its own */
        lights();
        return;
    }
    {                                    /* the battery, once a second: the header when it changes; empty: said once */
        static uint32_t bat_t;
        static uint8_t warned;
        if (now - bat_t >= 1000u) {
            int b = plat_battery();
            bat_t = now;
            if (b != bat_now) {
                bat_now = b;
                dub.dirty_hdr = 1;
                if (b == 0 && !warned) {
                    warned = 1;
                    app_say("BATTERY LOW", "CHARGE IT SOON");
                }
                if (b > 0)
                    warned = 0;
            }
        }
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
        if (dub.page == PAGE_DUB) {      /* (the rows DUB doesn't draw: what another page left there, cleared; 5.11) */
            lcd_fill(0, STRIP_Y + STRIP_H, 240, CENTRE_Y - STRIP_Y - STRIP_H, C_BLACK);
            lcd_fill(0, CENTRE_Y + CENTRE_H, 240, FOOT_Y - CENTRE_Y - CENTRE_H, C_BLACK);
        }
    }
    if (help.up) {                       /* a help card in the page's place */
        drew |= !help.drawn;
        help_draw();
        dub.dirty_strips = dub.dirty_centre = dub.dirty_foot = 0;
    } else if (dub.page == PAGE_STEPS) {
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
    else if (!strcmp(what, "led_play"))
        *got = led_play;
    else if (!strcmp(what, "battery"))   /* the battery as the header shows it (-1: none) */
        *got = bat_now;
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
    else if (steps_expect(what, got) || keys_expect(what, got) || arr_expect(what, got) || saver_expect(what, got)
             || undo_expect(what, got) || help_expect(what, got) || ml_expect(what, got) || sound_expect(what, got)
             || pl_expect(what, got) || dice_expect(what, got) || env_expect(what, got) || sh_expect(what, got))
        return 1;
    else if (!strcmp(what, "roots"))     /* 1: the cycle bar red, gold and green (72.0 BPM's first bar) */
        *got = roots_now();
    else
        return setup_expect(what, got);
    return 1;
}
#endif
