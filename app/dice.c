/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* Dice: the sound randomiser (roadmap 5.6, spec 4.10). A sound-design tool fired only by the player (principle 1,
 * clarified), on the focused channel's voice and nothing else: never levels, sends, the dub bus, patterns, LIVE or
 * another channel, so it can't jump the mix or run a loop away.
 * The SOUND view's last bank, DICE: KNOB 1 AMOUNT (5-100 %, MUTATE's reach), KNOB 2 MODE (ROLL: a new sound inside
 * the channel's bounds; MUTATE: a step away from the current), KNOB 3 MORPH (5.12, to come), KNOB 4 HISTORY (the
 * channel's last 8 results, each played in as it's reached; left there, it's kept). ENV fires (on the DICE bank);
 * HOME + EDIT fires from anywhere. ENV held + KNOB n on MAIN or a raw bank HOLDs that field (a macro: its targets),
 * shown by a lock; Dice leaves held fields alone.
 * Bounds: a table per field, inside the raw ranges and narrower where the sound needs it (the kick lands 40-60 Hz,
 * the bass's index stays under its cap and its fundamental sine-like, the decays stop short of their ends). Each fire
 * is one undo step and goes to the engine as knob turns do, so a song capture keeps it. SETUP's DICE ON LINE makes a
 * fire wait for the quantise line. The generator: xorshift32, seeded from the clock (tests: param dice.seed).
 * History: 8 results a channel in the pool (1.8 KB). Part of the unity build: dub.c includes it after sound.c. */

#define DICE_HIST 8u
#define DB_NONE {1, 0}                           /* (lo >= hi: not Dice's; a field not listed is {0, 0}) */
typedef struct {
    int8_t lo, hi;
} dbound_t;
static const dbound_t DICE_B[NCH][VM_BASE] = {
    {{0, 85}, {15, 100}, {10, 80}, {15, 100}, {0, 100}, {0, 70}, {-40, 40}},   /* kick: TUNE 40..60 Hz */
    {{10, 110}, {5, 90}, {10, 110}, {20, 115}, {0, 70}, {-40, 50}},              /* rim */
    {{10, 115}, {5, 90}, {20, 110}, {20, 110}, {-50, 50}},                      /* hats */
    {{15, 90}, {5, 90}, {5, 90}, {10, 85}, {0, 1}, {0, 100}, {0, 80}, {0, 60}, {0, 60}, {0, 60}},   /* bass: INDEX
                                                                                * under its cap */
    {{30, 120}, {5, 100}, {20, 110}, {0, 1}, {0, 60}},                          /* skank */
    {{10, 120}, {5, 100}, {0, 110}, {0, 90}},                                   /* organ */
    {{0, 1}, {20, 110}, {0, 90}, {5, 100}, {0, 100}, {-40, 40}},                /* lead */
    {{20, 110}, {10, 110}, {10, 90}, {0, 1}, DB_NONE},                          /* vox: not LIVE */
};
static struct {
    uint8_t amount, mode;                        /* 5..100 %; 0 ROLL, 1 MUTATE */
    uint8_t env_used;                            /* ENV held for a hold: its release fires nothing */
    uint8_t n[NCH], pos[NCH];                    /* each channel's history: how many, the one shown */
    uint32_t hold[NCH];                          /* held fields, a bit each */
    uint32_t rng, seeded;
} dc = {30, 0, 0, {0}, {0}, {0}, 0, 0};
static PLAT_POOL int8_t dice_hist[NCH][DICE_HIST][VM_BASE];

static int dice_field(uint32_t ch, uint32_t f)   /* 1: Dice may set field f of channel ch */
{
    return f < voice_fields((int32_t)ch) && f < VM_BASE && DICE_B[ch][f].lo < DICE_B[ch][f].hi;   /* (a field with no
                                                                   * bounds, as a voice's MODEL (5.7): left alone) */
}

static uint32_t dice_rand(void)                  /* xorshift32 */
{
    uint32_t x, s = (uint32_t)ctl_get(P_DICE_SEED);
    if (s && s != dc.seeded) {                   /* (a test's seed: the same rolls every run) */
        dc.seeded = s;
        dc.rng = s * 2654435761u | 1u;
    }
    if (!dc.rng)
        dc.rng = (plat_ms() + 1u) * 2654435761u | 1u;
    x = dc.rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return dc.rng = x;
}

static int dice_bank(void)                       /* 1: the SOUND view is on its DICE bank */
{
    return dub.sound && voice_fields(dub.focus) && dub.sound == sound_banks(dub.focus);
}

static void dice_set(uint32_t ch, uint32_t f, int32_t v)   /* at once, or on the line (SETUP: DICE ON LINE) */
{
    if (ctl_get(P_DICE_LINE) && eng_playing())
        ctl_cue(P_VOICE(ch, f), v, act_beats());
    else
        ctl_set(P_VOICE(ch, f), v);
}

static void dice_fire(uint32_t ch)               /* ENV on the DICE bank, HOME + EDIT */
{
    uint32_t f, k;
    int8_t r[VM_BASE];
    char t[20], n[8];
    if (!voice_fields((int32_t)ch)) {
        app_say("NO VOICE YET", CH_NAME[ch]);
        return;
    }
    for (f = 0; f < VM_BASE; f++) {
        int32_t lo = DICE_B[ch][f].lo, hi = DICE_B[ch][f].hi, cur, v;
        r[f] = 0;
        if (!dice_field(ch, f))
            continue;
        cur = ctl_get(P_VOICE(ch, f));
        v = cur;
        if (!(dc.hold[ch] >> f & 1u)) {
            uint32_t x = dice_rand();
            if (dc.mode == 0u)                   /* ROLL: anywhere inside the bounds */
                v = lo + (int32_t)(x % (uint32_t)(hi - lo + 1));
            else if (hi - lo <= 1)               /* MUTATE, a switch: flipped, as likely as AMOUNT / 2 */
                v = x % 200u < dc.amount ? (cur == lo ? hi : lo) : cur;
            else {                               /* MUTATE: up to AMOUNT % of the range either way */
                int32_t d = ((int32_t)(x % 2001u) - 1000) * (hi - lo) * (int32_t)dc.amount / 100000;
                v = cur + d;
            }
            v = v < lo ? lo : v > hi ? hi : v;
            dice_set(ch, f, v);
        }
        r[f] = (int8_t)v;
    }
    if (dc.n[ch] == DICE_HIST) {                 /* the history: the oldest out */
        for (k = 1; k < DICE_HIST; k++)
            for (f = 0; f < VM_BASE; f++)
                dice_hist[ch][k - 1][f] = dice_hist[ch][k][f];
        dc.n[ch]--;
    }
    for (f = 0; f < VM_BASE; f++)
        dice_hist[ch][dc.n[ch]][f] = r[f];
    dc.pos[ch] = dc.n[ch]++;
    set_dirty = 1;
    dub.dirty_strips = dub.dirty_foot = dub.dirty_centre = 1;
    t[0] = 0;
    text_cat(t, dc.mode ? "MUTATE " : "ROLL");
    if (dc.mode) {
        int_text(dc.amount, n);
        text_cat(t, n);
    }
    app_say(t, CH_NAME[ch]);
}

static void dice_history(int32_t e)              /* KNOB 4: the channel's results, each played in */
{
    uint32_t ch = (uint32_t)dub.focus, f;
    int32_t p;
    char t[12], n[4];
    if (!dc.n[ch]) {
        app_say("HISTORY", "NONE YET");
        return;
    }
    p = (int32_t)dc.pos[ch] + e;
    p = p < 0 ? 0 : p >= (int32_t)dc.n[ch] ? (int32_t)dc.n[ch] - 1 : p;
    dc.pos[ch] = (uint8_t)p;
    for (f = 0; f < VM_BASE; f++)
        if (dice_field(ch, f) && !(dc.hold[ch] >> f & 1u))
            dice_set(ch, f, dice_hist[ch][p][f]);
    set_dirty = 1;
    dub.dirty_foot = dub.dirty_centre = 1;
    int_text(p + 1, t);
    text_cat(t, " OF ");
    int_text(dc.n[ch], n);
    text_cat(t, n);
    app_say("HISTORY", t);
}

static void dice_knob(uint32_t i, int32_t e)     /* the DICE bank's knobs */
{
    char t[8];
    if (i == 0u) {
        int32_t a = (int32_t)dc.amount + 5 * e;
        dc.amount = (uint8_t)(a < 5 ? 5 : a > 100 ? 100 : a);
        int_text(dc.amount, t);
        text_cat(t, "%");
        app_say("AMOUNT", t);
    } else if (i == 1u) {
        dc.mode = (uint8_t)(e > 0);
        app_say("MODE", dc.mode ? "MUTATE" : "ROLL");
    } else if (i == 2u)
        app_say("MORPH", "TO COME");
    else
        dice_history(e);
    dub.dirty_foot = 1;
}

static uint32_t dice_fields_of(uint32_t i)       /* knob i on MAIN or a raw bank: the fields it holds, a bit each */
{
    int32_t sid = sound_id(i), mch;
    uint32_t mk, j, m = 0;
    if (sid < 0)
        return 0;
    if (macro_of((uint32_t)sid, &mch, &mk)) {    /* a macro: its targets */
        for (j = 0; j < MT; j++)
            if (MACRO_T[mch][mk][j].f != 0xFFu)
                m |= 1u << MACRO_T[mch][mk][j].f;
        return m;
    }
    return 1u << (((uint32_t)sid - P_VOICE_BASE) % VOICE_STRIDE);
}

static int dice_held(uint32_t i)                 /* 1: knob i's fields all held (its lock shown) */
{
    uint32_t m = dice_fields_of(i);
    return m && (dc.hold[dub.focus] & m) == m;
}

static void dice_hold_knob(uint32_t i)           /* ENV held + KNOB n: that field held, or let go */
{
    uint32_t m = dice_fields_of(i), ch = (uint32_t)dub.focus;
    char l[12];
    int32_t sid = sound_id(i);
    dc.env_used = 1;
    if (!m)
        return;
    if (dice_held(i))
        dc.hold[ch] &= ~m;
    else
        dc.hold[ch] |= m;
    label_of((uint32_t)sid, l, sizeof l);
    app_say(dice_held(i) ? "HOLD" : "FREE", l);
    dub.dirty_foot = 1;
}

static void dice_label(uint32_t i, char *l)      /* the DICE bank's footer */
{
    static const char *const L[4] = {"AMOUNT", "MODE", "MORPH", "HISTORY"};
    l[0] = 0;
    text_cat(l, L[i]);
}

static void dice_value(uint32_t i, char *t)
{
    char n[4];
    t[0] = 0;
    if (i == 0u) {
        int_text(dc.amount, t);
        text_cat(t, "%");
    } else if (i == 1u)
        text_cat(t, dc.mode ? "MUTATE" : "ROLL");
    else if (i == 2u)
        text_cat(t, "--");
    else if (dc.n[dub.focus]) {
        int_text(dc.pos[dub.focus] + 1, t);
        text_cat(t, "/");
        int_text(dc.n[dub.focus], n);
        text_cat(t, n);
    } else
        text_cat(t, "--");
}

#ifdef PLAT_HOST
static int dice_expect(const char *what, int *got)
{
    if (!strcmp(what, "dice_bank"))
        *got = dice_bank();
    else if (!strncmp(what, "dice_hold", 9))    /* dice_holdC: channel C's held fields, as bits */
        *got = (int)dc.hold[(uint32_t)(atoi(what + 9) - 1) % NCH];
    else if (!strncmp(what, "dice_n", 6))       /* dice_nC: channel C's history */
        *got = dc.n[(uint32_t)(atoi(what + 6) - 1) % NCH];
    else
        return 0;
    return 1;
}
#endif
