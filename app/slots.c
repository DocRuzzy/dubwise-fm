/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* Song slots (2026-11 review): four saved projects beside the one SAVE keeps. A project is everything a song needs to
 * sound as it did: the desk and the effects, the sounds, the key and the chord lane, the patterns (four a channel), the
 * 16 scenes, the song (512 bars) and its motion. The device's own preferences (the runway, the screen saver,
 * MIDI clock, the guard ...) stay as they are.
 * SETUP: SAVE TO SLOT and LOAD SLOT; KNOB 2 picks the slot (1-4), the SAVE button does it. Stopped only: a flash write
 * stops the sound for a moment. A loaded project is the current one (SAVE keeps it as that).
 * Each slot is 32 KiB of the flash's data area (two free runs: 0x97000 and 0xBD000), a header written last (a save
 * cut short reads as empty), then the parameters, the patterns' settings and steps, the scenes, the song, and the
 * motion (its counts, then its moves, 2,920 at most). A slot saved before the session (DSL1: one pattern a channel,
 * scenes with copies) loads as patstore.c converts an old save. Part of the unity build: dub.c includes it. */

#define NSLOTS 4u
#define SLOT_BYTES 0x8000u
static const uint32_t SLOT_AT[NSLOTS] = {0x97000u, 0x9F000u, 0xA7000u, 0xBD000u};
#define SLOT_MAGIC 0x324C5344u                   /* "DSL2" (the session) */
#define SLOT_MAGIC1 0x314C5344u                  /* "DSL1" (before it) */
typedef struct {
    uint32_t magic, len, sum, moves;
} slot_head_t;
#define SLOT_FIXED (P_NIDS * 2u + NCH * NPAT * (uint32_t)sizeof(pmeta_t) + NCH * NPAT * NSTEPS * (uint32_t)sizeof(step_t) \
                    + (uint32_t)sizeof scenes + (uint32_t)sizeof song + SONG_BARS)
#define SLOT_FIXED1 (P_NIDS * 2u + NCH * NSTEPS * 2u + NSCENES * (uint32_t)sizeof(scene_v1_t) \
                     + SONG_BARS * (uint32_t)sizeof(song_v1_t) + SONG_BARS)
#define SLOT_MOVES_MAX ((SLOT_BYTES - (uint32_t)sizeof(slot_head_t) - SLOT_FIXED) / (uint32_t)sizeof(mot_ev_t))
typedef char slot_fits[SLOT_FIXED + sizeof(slot_head_t) < SLOT_BYTES ? 1 : -1];

static int slot_head_ok(const slot_head_t *h)    /* a whole slot of either form */
{
    uint32_t fixed = h->magic == SLOT_MAGIC ? SLOT_FIXED : h->magic == SLOT_MAGIC1 ? SLOT_FIXED1 : 0u;
    return fixed && h->moves <= SLOT_BYTES && h->len == fixed + h->moves * (uint32_t)sizeof(mot_ev_t)
           && h->len <= SLOT_BYTES - sizeof *h;
}

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
    return id >= P_CH_BASE && id < P_NIDS && param_desc(id) && mot_momentary(id) < 0 && id != P_SPIN && id != P_SHOT && id != P_TAILS;
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
    return (h.magic == SLOT_MAGIC || h.magic == SLOT_MAGIC1) && h.len <= SLOT_BYTES - sizeof h ? &h : 0;
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
    for (ch = 0; ch < NCH; ch++)                 /* the patterns' settings */
        for (i = 0; i < NPAT; i++) {
            pmeta_t m = ctl_meta_get(ch, i);
            if (!slot_put(&off, &sum, &m, sizeof m))
                return 0;
        }
    for (ch = 0; ch < NCH; ch++)                 /* their steps, a pattern at a time */
        for (i = 0; i < NPAT; i++) {
            typedef char fits[NSTEPS * sizeof(step_t) <= sizeof slot_buf ? 1 : -1];
            (void)sizeof(fits);
            for (k = 0; k < NSTEPS; k++) {
                step_t st = ctl_get_step_at(ch, i, k);
                slot_buf[4u * k] = st.lvl;
                slot_buf[4u * k + 1u] = (uint8_t)st.note;
                slot_buf[4u * k + 2u] = (uint8_t)st.off;
                slot_buf[4u * k + 3u] = st.gate;
            }
            if (!slot_put(&off, &sum, slot_buf, NSTEPS * 4u))
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
    if (!slot_head_ok(&h) || slot_sum(2166136261u, p, h.len) != h.sum)
        return 0;
    for (i = 0; i < P_NIDS; i++)                 /* the project's parameters (the device's stay) */
        if (project_param(i))
            ctl_set(i, (int16_t)(p[2u * i] | p[2u * i + 1u] << 8));
    if (h.magic == SLOT_MAGIC1) {                /* from before the session: converted (patstore.c) */
        static scene_v1_t sv;
        const uint8_t *lens = p, *base = p + P_NIDS * 2u;
        uint32_t s2;
        p += P_NIDS * 2u + NCH * NSTEPS * 2u;
        mig_start();
        for (ch = 0; ch < NCH; ch++) {
            uint32_t id = P_CH(ch, CHF_LEN), len = lens[2u * id];
            mig_put(ch, 0, len >= 1u && len <= NSTEPS ? len : 16u, (const step_v1_t *)(const void *)(base + ch * NSTEPS * 2u));
        }
        for (s2 = 0; s2 < NSCENES; s2++, p += sizeof sv) {
            for (i = 0; i < sizeof sv; i++)
                ((uint8_t *)&sv)[i] = p[i];
            scene_put(s2, sv.used, sv.scope, sv.desk);
            if (sv.used && sv.scope == SCOPE_PATTERNS)
                for (ch = 0; ch < NCH; ch++)
                    scenes[s2].pat[ch] = (uint8_t)mig_pat(ch, sv.len[ch] >= 1u && sv.len[ch] <= NSTEPS ? sv.len[ch] : 16u,
                                                          sv.pat[ch]);
        }
        for (b = 0; b < SONG_BARS; b++, p += sizeof(song_v1_t)) {
            song_v1_t o;
            for (i = 0; i < sizeof o; i++)
                ((uint8_t *)&o)[i] = p[i];
            mig_bar(&song[b], &o);
        }
    } else {
        p += P_NIDS * 2u;
        for (ch = 0; ch < NCH; ch++)
            for (i = 0; i < NPAT; i++, p += sizeof(pmeta_t)) {
                pmeta_t m;
                m.len = p[0];
                m.grid = p[1];
                m.swing = p[2];
                m.rsv = 0;
                ctl_meta_set(ch, i, m);
            }
        for (ch = 0; ch < NCH; ch++)
            for (i = 0; i < NPAT; i++) {
                uint32_t k;
                for (k = 0; k < NSTEPS; k++, p += 4) {
                    step_t st;
                    st.lvl = p[0] < LVL_N ? p[0] : LVL_OFF;
                    st.note = (int8_t)p[1];
                    st.off = (int8_t)p[2];
                    st.gate = p[3];
                    ctl_step_at(ch, i, k, st);
                }
            }
        for (i = 0; i < sizeof scenes; i++)
            ((uint8_t *)scenes)[i] = *p++;
        for (i = 0; i < sizeof song; i++)
            ((uint8_t *)song)[i] = *p++;
    }
    slot_counts_off = 0;
    slot_moves = p;                              /* the counts, then the moves: written by slot_tick */
    slot_nmoves = h.moves;
    ctl_motion(1);
    scenes_dirty = 0xFFFFu;
    song_dirty = 1;
    set_dirty = 1;
    arr.sig = 0;
    undo_reset();
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
    if (!load && slot_head((uint32_t)su_slot) && !confirm(CF_SLOT_SAVE + (uint32_t)su_slot, "OVERWRITE"))
        return;                          /* (a saved slot: asked again) */
    if (load && (set_dirty || scenes_dirty || mot_dirty) && slot_head((uint32_t)su_slot)
        && !confirm(CF_SLOT_LOAD + (uint32_t)su_slot, "NOT SAVED: LOAD"))
        return;                          /* (what isn't saved would go: asked again) */
    r = load ? slot_load((uint32_t)su_slot) : slot_save((uint32_t)su_slot);
    app_say(t, r == 1 ? (load ? "LOADED" : "SAVED") : r == -1 ? "EMPTY" : r == -2 ? "TOO MANY MOVES" : "FAILED");
}

/* ------------------------------------------------------- slots over USB --- */
/* A slot to and from a computer (tools/dubwise_slots.py), as SysEx: F0 7D 44 57 (7D: non-commercial; "DW") CMD SLOT
 * ... F7, the data 7-bit packed (7 bytes in 8: their top bits first). The image is the slot as the flash holds it
 * (its header, then its body): what the computer keeps is a file of exactly that.
 *   01 s              dump slot s: the FM-1 answers 11 s IDX(2) DATA (a 256-byte piece at IDX x 256) .. then
 *                     12 s LEN(3) (done), or 13 s (empty)
 *   21 s IDX(2) DATA  a piece of an image for slot s (IDX from 0, in order; IDX 0 erases the slot)
 *   22 s LEN(3)       the image is whole: checked (its header, its length, its sum) and its header written last;
 *                     the FM-1 answers 16 s (kept) or 15 s (refused: playing, out of order, damaged: the slot empty)
 * Main loop only (app_sysex, and the frame's slot_sx_tick: a dump goes out as the ring has room) */
#define SX_PIECE 256u
static struct {
    int8_t on;
    uint8_t slot;
    uint32_t off, len;
} sxd;
static struct {
    int8_t on;
    uint8_t slot;
    uint8_t head[sizeof(slot_head_t)];
    uint32_t got;
} sxp;
static uint8_t sx_out[10 + (SX_PIECE * 8u + 6u) / 7u];
static uint8_t sx_raw[SX_PIECE];

static uint32_t pack7(const uint8_t *in, uint32_t n, uint8_t *out)
{
    uint32_t i, k, o = 0;
    for (i = 0; i < n; i += 7u) {
        uint32_t m = n - i < 7u ? n - i : 7u, top = o++;
        out[top] = 0;
        for (k = 0; k < m; k++) {
            out[top] |= (uint8_t)((in[i + k] >> 7) << k);
            out[o++] = in[i + k] & 0x7Fu;
        }
    }
    return o;
}

static uint32_t unpack7(const uint8_t *in, uint32_t n, uint8_t *out, uint32_t max)
{
    uint32_t i = 0, o = 0, k;
    while (i < n) {
        uint32_t top = in[i++];
        for (k = 0; k < 7u && i < n && o < max; k++)
            out[o++] = (uint8_t)(in[i++] | ((top >> k) & 1u) << 7);
    }
    return o;
}

static void sx_reply(uint32_t cmd, uint32_t slot, uint32_t len3, int with_len)
{
    uint8_t f[12];
    uint32_t n = 0;
    f[n++] = 0xF0, f[n++] = 0x7D, f[n++] = 0x44, f[n++] = 0x57, f[n++] = (uint8_t)cmd, f[n++] = (uint8_t)slot;
    if (with_len)
        f[n++] = (uint8_t)(len3 & 0x7Fu), f[n++] = (uint8_t)(len3 >> 7 & 0x7Fu), f[n++] = (uint8_t)(len3 >> 14 & 0x7Fu);
    f[n++] = 0xF7;
    plat_sysex_send(f, n);
}

static void slot_sx_tick(void)                   /* every frame: a dump's next pieces, as the ring takes them */
{
    while (sxd.on && plat_sysex_room() >= sizeof sx_out) {
        uint32_t n = sxd.len - sxd.off < SX_PIECE ? sxd.len - sxd.off : SX_PIECE, i, k = 0, idx = sxd.off / SX_PIECE;
        const uint8_t *src = plat_data(SLOT_AT[sxd.slot] + sxd.off);
        if (!n) {
            sx_reply(0x12, sxd.slot, sxd.len, 1);
            sxd.on = 0;
            break;
        }
        for (i = 0; i < n; i++)
            sx_raw[i] = src[i];                  /* (the ring takes RAM) */
        sx_out[k++] = 0xF0, sx_out[k++] = 0x7D, sx_out[k++] = 0x44, sx_out[k++] = 0x57, sx_out[k++] = 0x11;
        sx_out[k++] = sxd.slot, sx_out[k++] = (uint8_t)(idx & 0x7Fu), sx_out[k++] = (uint8_t)(idx >> 7 & 0x7Fu);
        k += pack7(sx_raw, n, sx_out + k);
        sx_out[k++] = 0xF7;
        if (plat_sysex_send(sx_out, k) != 0) {
            sxd.on = 0;                          /* (no computer: given up) */
            break;
        }
        sxd.off += n;
    }
}

static int pl_sysex(const uint8_t *p, uint32_t n);   /* (patches.c: 31..37, the patches) */
static int slot_sysex(const uint8_t *p, uint32_t n)   /* app_sysex: the bytes between F0 and F7; 1: ours */
{
    uint32_t cmd, s, idx, i, got, off;
    if (n < 5u || p[0] != 0x7Du || p[1] != 0x44u || p[2] != 0x57u)
        return 0;
    cmd = p[3];
    if (cmd >= 0x31u && cmd <= 0x37u)
        return pl_sysex(p, n);
    s = p[4];
    if (s >= NSLOTS)
        return 1;
    if (cmd == 0x01u) {                          /* dump */
        const slot_head_t *h = slot_head(s);
        if (!h) {
            sx_reply(0x13, s, 0, 0);
            return 1;
        }
        sxd.on = 1, sxd.slot = (uint8_t)s, sxd.off = 0, sxd.len = (uint32_t)sizeof(slot_head_t) + h->len;
        return 1;
    }
    if (cmd == 0x21u && n >= 7u) {               /* a piece */
        idx = p[5] | (uint32_t)p[6] << 7;
        if (idx == 0u) {
            if (eng_playing()) {
                sx_reply(0x15, s, 0, 0);
                sxp.on = 0;
                return 1;
            }
            for (i = 0; i < SLOT_BYTES / 4096u; i++)
                plat_data_erase(SLOT_AT[s] + i * 4096u);
            sxp.on = 1, sxp.slot = (uint8_t)s, sxp.got = 0;
        }
        if (!sxp.on || sxp.slot != s || idx * SX_PIECE != sxp.got) {
            sxp.on = 0;
            sx_reply(0x15, s, 0, 0);
            return 1;
        }
        got = unpack7(p + 7, n - 7u, sx_raw, SX_PIECE);
        if (sxp.got + got > SLOT_BYTES) {
            sxp.on = 0;
            sx_reply(0x15, s, 0, 0);
            return 1;
        }
        for (i = 0, off = sxp.got; i < got; i++, off++)   /* the header kept for the end; the body to the flash */
            if (off < sizeof(slot_head_t))
                sxp.head[off] = sx_raw[i];
        if (sxp.got + got > sizeof(slot_head_t)) {
            uint32_t skip = sxp.got < sizeof(slot_head_t) ? (uint32_t)sizeof(slot_head_t) - sxp.got : 0u;
            plat_data_write(SLOT_AT[s] + sxp.got + skip, sx_raw + skip, got - skip);
        }
        sxp.got += got;
        return 1;
    }
    if (cmd == 0x22u && n >= 8u) {               /* whole: checked, then its header */
        slot_head_t h;
        uint32_t len = p[5] | (uint32_t)p[6] << 7 | (uint32_t)p[7] << 14;
        const uint8_t *body = plat_data(SLOT_AT[s] + sizeof h);
        for (i = 0; i < sizeof h; i++)
            ((uint8_t *)&h)[i] = sxp.head[i];
        if (!sxp.on || sxp.slot != s || len != sxp.got || h.len + sizeof h != len || !slot_head_ok(&h)
            || slot_sum(2166136261u, body, h.len) != h.sum) {
            sxp.on = 0;
            sx_reply(0x15, s, 0, 0);
            return 1;
        }
        plat_data_write(SLOT_AT[s], &h, sizeof h);
        sxp.on = 0;
        sx_reply(0x16, s, 0, 0);
        return 1;
    }
    return 1;
}
