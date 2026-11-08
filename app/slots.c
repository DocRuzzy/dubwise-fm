/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* Song slots (2026-11 review): four saved projects beside the one SAVE keeps. A project is everything a song needs to
 * sound as it did: the desk and the effects, the sounds, the key and the chord lane, the patterns, the 16 scenes (with
 * their patterns), the song (512 bars) and its motion. The device's own preferences (the runway, the screen saver,
 * MIDI clock, the guard ...) stay as they are.
 * SETUP: SAVE TO SLOT and LOAD SLOT; KNOB 2 picks the slot (1-4), the SAVE button does it. Stopped only: a flash write
 * stops the sound for a moment. A loaded project is the current one (SAVE keeps it as that).
 * Each slot is 32 KiB of the flash's data area (two free runs: 0x97000 and 0xBD000), a header written last (a save
 * cut short reads as empty), then the parameters, the patterns, the scenes, the song, and the motion (its counts, then
 * its moves, 1,490 at most). Part of the unity build: dub.c includes it. */

#define NSLOTS 4u
#define SLOT_BYTES 0x8000u
static const uint32_t SLOT_AT[NSLOTS] = {0x97000u, 0x9F000u, 0xA7000u, 0xBD000u};
#define SLOT_MAGIC 0x314C5344u                   /* "DSL1" */
typedef struct {
    uint32_t magic, len, sum, moves;
} slot_head_t;
#define SLOT_FIXED (P_NIDS * 2u + NCH * NSTEPS * (uint32_t)sizeof(step_t) + (uint32_t)sizeof scenes \
                    + (uint32_t)sizeof song + SONG_BARS)
#define SLOT_MOVES_MAX ((SLOT_BYTES - (uint32_t)sizeof(slot_head_t) - SLOT_FIXED) / (uint32_t)sizeof(mot_ev_t))
typedef char slot_fits[SLOT_FIXED + sizeof(slot_head_t) < SLOT_BYTES ? 1 : -1];

static int32_t su_slot;                          /* the slot the two rows point at (0..3) */
static const uint8_t *slot_moves;                /* a load's moves, written once the engine hands motion over */
static uint32_t slot_nmoves, slot_counts_off;
static uint8_t slot_buf[256];                    /* (the parameters and the motion go out through it) */

static int project_param(uint32_t id)            /* a project's, not the device's */
{
    if (id == P_TEMPO || id == P_KEY_ROOT || id == P_SCALE || id == P_LOCK || id == P_CHORD_LEN || id == P_SONG_MODE
        || id == P_SONG_LEN)
        return 1;
    if (id >= P_CHORD0 && id < P_CHORD_END)
        return 1;
    return id >= P_CH_BASE && id < P_NIDS && param_desc(id) && mot_momentary(id) < 0 && id != P_SPIN;
}

static const slot_head_t *slot_head(uint32_t s)  /* the slot's header in the flash, 0: empty */
{
    const uint8_t *p = plat_data(SLOT_AT[s]);
    static slot_head_t h;
    uint32_t i;
    if (!p)
        return 0;
    for (i = 0; i < sizeof h; i++)
        ((uint8_t *)&h)[i] = p[i];
    return h.magic == SLOT_MAGIC && h.len <= SLOT_BYTES - sizeof h ? &h : 0;
}

static uint32_t slot_sum(uint32_t h, const uint8_t *p, uint32_t n)
{
    while (n--)
        h = (h ^ *p++) * 16777619u;
    return h;
}

static int slot_put(uint32_t *off, uint32_t *sum, const void *src, uint32_t n)   /* RAM to the slot, summed */
{
    if (plat_data_write(*off, src, n) != 0)
        return 0;
    *sum = slot_sum(*sum, (const uint8_t *)src, n);
    *off += n;
    return 1;
}

static int slot_save(uint32_t s)
{
    slot_head_t h;
    uint32_t off, sum = 2166136261u, i, b, k, total = mot_total(), ch;
    if (total > SLOT_MOVES_MAX)
        return -2;
    for (i = 0; i < SLOT_BYTES / 4096u; i++)
        if (plat_data_erase(SLOT_AT[s] + i * 4096u) != 0)
            return 0;
    off = SLOT_AT[s] + (uint32_t)sizeof h;
    for (i = 0; i < P_NIDS; i += 128u) {         /* the parameters, all of them (a load takes the project's) */
        for (k = 0; k < 128u; k++) {
            int16_t v = (int16_t)ctl_get(i + k);
            slot_buf[2u * k] = (uint8_t)v;
            slot_buf[2u * k + 1u] = (uint8_t)((uint16_t)v >> 8);
        }
        if (!slot_put(&off, &sum, slot_buf, 256u))
            return 0;
    }
    for (ch = 0; ch < NCH; ch++)                 /* the patterns */
        for (i = 0; i < NSTEPS; i += 64u) {
            for (k = 0; k < 64u; k++) {
                step_t st = ctl_get_step(ch, i + k);
                slot_buf[2u * k] = st.lvl;
                slot_buf[2u * k + 1u] = (uint8_t)st.note;
            }
            if (!slot_put(&off, &sum, slot_buf, 128u))
                return 0;
        }
    if (!slot_put(&off, &sum, scenes, sizeof scenes) || !slot_put(&off, &sum, song, sizeof song)
        || !slot_put(&off, &sum, mot_n, SONG_BARS))
        return 0;
    for (b = 0; b < SONG_BARS; b++)              /* the moves, a bar's at a time */
        for (i = 0; i < mot_n[b]; i++) {
            mot_ev_t e;
            mot_get(b, i, &e);
            if (!slot_put(&off, &sum, &e, sizeof e))
                return 0;
        }
    h.magic = SLOT_MAGIC;
    h.len = off - SLOT_AT[s] - (uint32_t)sizeof h;
    h.sum = sum;
    h.moves = total;
    return plat_data_write(SLOT_AT[s], &h, sizeof h) == 0;   /* last */
}

static int slot_load(uint32_t s)
{
    const slot_head_t *hp = slot_head(s);
    const uint8_t *p;
    uint32_t i, ch, b;
    slot_head_t h;
    if (!hp)
        return -1;
    h = *hp;
    p = plat_data(SLOT_AT[s]) + sizeof h;
    if (h.len != SLOT_FIXED + h.moves * (uint32_t)sizeof(mot_ev_t) || slot_sum(2166136261u, p, h.len) != h.sum)
        return 0;
    for (i = 0; i < P_NIDS; i++)                 /* the project's parameters (the device's stay) */
        if (project_param(i))
            ctl_set(i, (int16_t)(p[2u * i] | p[2u * i + 1u] << 8));
    p += P_NIDS * 2u;
    for (ch = 0; ch < NCH; ch++)
        for (i = 0; i < NSTEPS; i++, p += 2) {
            step_t st;
            st.lvl = p[0];
            st.note = (int8_t)p[1];
            ctl_step(ch, i, st);
        }
    for (i = 0; i < sizeof scenes; i++)
        ((uint8_t *)scenes)[i] = *p++;
    for (i = 0; i < sizeof song; i++)
        ((uint8_t *)song)[i] = *p++;
    slot_counts_off = 0;
    slot_moves = p;                              /* the counts, then the moves: written by slot_tick */
    slot_nmoves = h.moves;
    (void)b;
    ctl_motion(1);
    scenes_dirty = 0xFFFFu;
    song_dirty = 1;
    set_dirty = 1;
    arr.sig = 0;
    return 1;
}

static void slot_tick(void)                      /* every frame: a loaded slot's motion, once the engine lets go */
{
    const uint8_t *c, *e;
    uint32_t b, i;
    if (!slot_moves || !mot_suspended())
        return;
    c = slot_moves;
    e = c + SONG_BARS;
    mot_clear();
    for (b = 0; b < SONG_BARS; b++)
        for (i = 0; i < c[b]; i++, e += sizeof(mot_ev_t)) {
            mot_ev_t m;
            uint32_t k;
            for (k = 0; k < sizeof m; k++)
                ((uint8_t *)&m)[k] = e[k];
            mot_put(b, m.tick, m.id, m.v);
        }
    slot_moves = 0;
    (void)slot_nmoves;
    ctl_motion(2);
}

/* SETUP's two rows: the value they show, and the SAVE button on them */
static void slot_value(char *t, int load)
{
    char n[8];
    const slot_head_t *h = slot_head((uint32_t)su_slot);
    t[0] = 0;
    text_cat(t, load ? "SLOT " : "TO ");
    int_text(su_slot + 1, n);
    text_cat(t, n);
    text_cat(t, h ? " (SAVED)" : " (EMPTY)");
}

static void slot_pick(int32_t e) { su_slot = ((su_slot + e) % (int32_t)NSLOTS + (int32_t)NSLOTS) % (int32_t)NSLOTS; }

static void slot_action(int load)
{
    char t[16], n[8];
    int r;
    t[0] = 0;
    text_cat(t, "SLOT ");
    int_text(su_slot + 1, n);
    text_cat(t, n);
    if (eng_playing()) {
        app_say(t, "STOP FIRST");
        return;
    }
    r = load ? slot_load((uint32_t)su_slot) : slot_save((uint32_t)su_slot);
    app_say(t, r == 1 ? (load ? "LOADED" : "SAVED") : r == -1 ? "EMPTY" : r == -2 ? "TOO MANY MOVES" : "FAILED");
}
