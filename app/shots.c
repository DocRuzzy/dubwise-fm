/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* Your own one-shots (roadmap 5.14; docs/plan-5.14-oneshots.md): BIM, HORN and LASER (black keys 8-10) replaced by
 * sounds made on a computer (tools/make_oneshots.py from WAV files; tools/dubwise_slots.py shots put sends them).
 * 1.5 s in all (33,075 bytes of 8-bit mu-law at 22,050 Hz, the built-in shots' format), split any way between the
 * three. They live in the flash's PLAT_XDATA1 window (the first 9 sectors, 36 KiB), outside the store, so they stay
 * over a reboot, an update and NEW PROJECT. At boot they're copied into the pool (the render never reads the flash).
 *
 * The image (logical bytes; on the flash each sector's bytes 0xF00..0xF07 are left erased, the data stepping round
 * them, so nothing here can read as the update loader's record: plat.h):
 *   0   "SHT1"
 *   4   the data's length (<= 33,075)
 *   8   FNV-1a over the data (a write cut short, or damage: no user shots, the factory ones play)
 *   12  three entries of 12: offset (2, from the data's start), length (2), a name (8, for the screen)
 *   48  the data
 * The header goes in last, as a song slot's does.
 *
 * SysEx (4.3's framing, F0 7D 44 57 ..):
 *   51              dump: the FM-1 answers 61 IDX(2) DATA (256-byte pieces of the image) .. then 62 LEN(3), or 63
 *                   (no shots of yours: the factory ones)
 *   52 IDX(2) DATA  a piece of an image (IDX from 0, in order; IDX 0 erases the area: the factory shots play)
 *   53 LEN(3)       whole: checked (its header, length, entries, sum), the header written, the shots used at once;
 *                   the FM-1 answers 66 (kept) or 65 (refused: playing, out of order, damaged)
 *   54              the factory shots back (the header erased): 66, or 65 while playing
 * Main loop only. Part of the unity build: dub.c includes it, after slots.c (its 7-bit packing and FNV sum). */

#define SH_MAGIC 0x31544853u                     /* "SHT1" */
#define SH_AT PLAT_XDATA1_LO
#define SH_SECTORS 9u
#define SH_USE (4096u - 8u)                      /* a sector's bytes for us (0xF00..0xF07 left erased) */
typedef struct {
    uint32_t magic, len, sum;
    struct {
        uint16_t off, len;
        char name[8];
    } e[FXV_NSHOTS];
} sh_head_t;
typedef char sh_head_is_48[sizeof(sh_head_t) == 48u ? 1 : -1];
typedef char sh_fits[sizeof(sh_head_t) + FXV_SHOT_MAX <= SH_SECTORS * SH_USE ? 1 : -1];
typedef char sh_in_xdata1[SH_AT + SH_SECTORS * 4096u <= PLAT_XDATA1_HI ? 1 : -1];

static uint8_t sh_user;                          /* 1: the owner's shots are playing (SETUP shows it) */
static struct {
    int8_t on;
    uint32_t off, len;
} shd;                                           /* a dump going out */
static struct {
    int8_t on;
    uint8_t head[sizeof(sh_head_t)];
    uint32_t got;
} shp;                                           /* a put coming in */

static uint32_t sh_phys(uint32_t l)              /* a logical byte's place in the flash */
{
    uint32_t s = l / SH_USE, r = l % SH_USE;
    return SH_AT + s * 4096u + (r < 0xF00u ? r : r + 8u);
}
static uint32_t sh_run(uint32_t l)               /* the logical bytes from l before a hole or a sector's end */
{
    uint32_t r = l % SH_USE;
    return r < 0xF00u ? 0xF00u - r : SH_USE - r;
}
static void sh_write(uint32_t l, const uint8_t *src, uint32_t n)
{
    while (n) {
        uint32_t k = sh_run(l) < n ? sh_run(l) : n;
        plat_data_write(sh_phys(l), src, k);
        l += k, src += k, n -= k;
    }
}
static void sh_read(uint32_t l, uint8_t *dst, uint32_t n)
{
    while (n) {
        uint32_t k = sh_run(l) < n ? sh_run(l) : n, i;
        const uint8_t *p = plat_data(sh_phys(l));
        for (i = 0; i < k; i++)
            dst[i] = p ? p[i] : 0xFFu;
        l += k, dst += k, n -= k;
    }
}
static uint32_t sh_sum(uint32_t l, uint32_t n)   /* FNV-1a over logical bytes, in the flash */
{
    uint32_t h = 2166136261u;
    while (n) {
        uint32_t k = sh_run(l) < n ? sh_run(l) : n;
        const uint8_t *p = plat_data(sh_phys(l));
        if (!p)
            return 0;
        h = slot_sum(h, p, k);
        l += k, n -= k;
    }
    return h;
}

static int sh_head_ok(const sh_head_t *h)        /* the header alone: the magic, the length, the entries inside */
{
    uint32_t k;
    if (h->magic != SH_MAGIC || h->len > FXV_SHOT_MAX)
        return 0;
    for (k = 0; k < FXV_NSHOTS; k++)
        if ((uint32_t)h->e[k].off + h->e[k].len > h->len)
            return 0;
    return 1;
}
static const sh_head_t *sh_head(void)            /* the flash's header, if whole and its data's sum right; 0 none */
{
    static sh_head_t h;
    sh_read(0, (uint8_t *)&h, sizeof h);
    return sh_head_ok(&h) && sh_sum(sizeof h, h.len) == h.sum ? &h : 0;
}

static void sh_apply(void)                       /* (main loop) the flash's shots into the pool, or the factory's */
{
    const sh_head_t *h = sh_head();
    uint32_t k, i, off[FXV_NSHOTS], len[FXV_NSHOTS];
    static char nm[FXV_NSHOTS][9];
    const char *np[FXV_NSHOTS];
    if (!h) {
        if (sh_user)
            fxv_shots_factory();
        sh_user = 0;
        return;
    }
    fxv_shots_hold();
    sh_read(sizeof *h, fxv_shots_ram(), h->len);
    for (k = 0; k < FXV_NSHOTS; k++) {
        off[k] = h->e[k].off, len[k] = h->e[k].len;
        for (i = 0; i < 8u; i++) {               /* (the screen's characters only) */
            char c = h->e[k].name[i];
            nm[k][i] = c >= 'a' && c <= 'z' ? (char)(c - 32) : c >= ' ' && c <= '~' ? c : ' ';
        }
        nm[k][8] = 0;
        for (i = 8u; i > 0u && nm[k][i - 1u] == ' '; i--)
            nm[k][i - 1u] = 0;
        if (!nm[k][0]) {
            nm[k][0] = 'S', nm[k][1] = 'H', nm[k][2] = 'O', nm[k][3] = 'T', nm[k][4] = ' ';
            nm[k][5] = (char)('1' + k), nm[k][6] = 0;
        }
        np[k] = nm[k];
    }
    fxv_shots_use(off, len, np);
    sh_user = 1;
}
static void sh_load(void) { sh_apply(); }        /* (load_saved, at boot) */

static int sh_factory(void)                      /* SETUP or SysEx 54: the built-in shots back; 0 refused (playing) */
{
    if (eng_playing())
        return 0;
    plat_data_erase(SH_AT);                      /* (the header: the rest is left, unread) */
    fxv_shots_factory();
    sh_user = 0;
    return 1;
}

static void sh_reply(uint32_t cmd, uint32_t len3, int with_len)
{
    uint8_t f[10];
    uint32_t n = 0;
    f[n++] = 0xF0, f[n++] = 0x7D, f[n++] = 0x44, f[n++] = 0x57, f[n++] = (uint8_t)cmd;
    if (with_len)
        f[n++] = (uint8_t)(len3 & 0x7Fu), f[n++] = (uint8_t)(len3 >> 7 & 0x7Fu), f[n++] = (uint8_t)(len3 >> 14 & 0x7Fu);
    f[n++] = 0xF7;
    plat_sysex_send(f, n);
}

static void sh_sx_tick(void)                     /* every frame: a dump's next pieces, as the ring takes them */
{
    while (shd.on && plat_sysex_room() >= sizeof sx_out) {
        uint32_t n = shd.len - shd.off < SX_PIECE ? shd.len - shd.off : SX_PIECE, k = 0, idx = shd.off / SX_PIECE;
        if (!n) {
            sh_reply(0x62, shd.len, 1);
            shd.on = 0;
            break;
        }
        sh_read(shd.off, sx_raw, n);
        sx_out[k++] = 0xF0, sx_out[k++] = 0x7D, sx_out[k++] = 0x44, sx_out[k++] = 0x57, sx_out[k++] = 0x61;
        sx_out[k++] = (uint8_t)(idx & 0x7Fu), sx_out[k++] = (uint8_t)(idx >> 7 & 0x7Fu);
        k += pack7(sx_raw, n, sx_out + k);
        sx_out[k++] = 0xF7;
        if (plat_sysex_send(sx_out, k) != 0) {
            shd.on = 0;
            break;
        }
        shd.off += n;
    }
}

static int sh_sysex(const uint8_t *p, uint32_t n)   /* (slot_sysex: commands 51..54) */
{
    uint32_t cmd = p[3], i, idx, got, off;
    if (cmd == 0x51u) {                          /* dump */
        const sh_head_t *h = sh_head();
        if (!h) {
            sh_reply(0x63, 0, 0);
            return 1;
        }
        shd.on = 1, shd.off = 0, shd.len = (uint32_t)sizeof *h + h->len;
        return 1;
    }
    if (cmd == 0x52u && n >= 6u) {               /* a piece */
        idx = p[4] | (uint32_t)p[5] << 7;
        if (idx == 0u) {
            if (eng_playing()) {
                shp.on = 0;
                sh_reply(0x65, 0, 0);
                return 1;
            }
            for (i = 0; i < SH_SECTORS; i++)
                plat_data_erase(SH_AT + i * 4096u);
            fxv_shots_factory();                 /* (the flash has none of ours now) */
            sh_user = 0;
            shp.on = 1, shp.got = 0;
        }
        if (!shp.on || idx * SX_PIECE != shp.got) {
            shp.on = 0;
            sh_reply(0x65, 0, 0);
            return 1;
        }
        got = unpack7(p + 6, n - 6u, sx_raw, SX_PIECE);
        if (shp.got + got > sizeof(sh_head_t) + FXV_SHOT_MAX) {
            shp.on = 0;
            sh_reply(0x65, 0, 0);
            return 1;
        }
        for (i = 0, off = shp.got; i < got; i++, off++)   /* the header kept for the end; the data to the flash */
            if (off < sizeof(sh_head_t))
                shp.head[off] = sx_raw[i];
        if (shp.got + got > sizeof(sh_head_t)) {
            uint32_t skip = shp.got < sizeof(sh_head_t) ? (uint32_t)sizeof(sh_head_t) - shp.got : 0u;
            sh_write(shp.got + skip, sx_raw + skip, got - skip);
        }
        shp.got += got;
        return 1;
    }
    if (cmd == 0x53u && n >= 7u) {               /* whole: checked, then its header, then used */
        sh_head_t h;
        uint32_t len = p[4] | (uint32_t)p[5] << 7 | (uint32_t)p[6] << 14;
        for (i = 0; i < sizeof h; i++)
            ((uint8_t *)&h)[i] = shp.head[i];
        if (!shp.on || len != shp.got || len < sizeof h || h.len + sizeof h != len || !sh_head_ok(&h)
            || sh_sum(sizeof h, h.len) != h.sum) {
            shp.on = 0;
            sh_reply(0x65, 0, 0);
            return 1;
        }
        sh_write(0, (const uint8_t *)&h, sizeof h);
        shp.on = 0;
        sh_apply();
        sh_reply(0x66, 0, 0);
        dub.dirty_hdr = 1;
        return 1;
    }
    if (cmd == 0x54u) {                          /* the factory's back */
        sh_reply(sh_factory() ? 0x66 : 0x65, 0, 0);
        return 1;
    }
    return 1;
}

#ifdef PLAT_HOST
static int sh_expect(const char *what, int *got)
{
    if (!strcmp(what, "shots_user"))             /* 1: the owner's shots playing */
        *got = sh_user;
    else if (!strncmp(what, "shot_len", 8) && what[8] >= '1' && what[8] <= '3')   /* shot_lenK: its bytes */
        *got = (int)fxv_shot_len((uint32_t)(what[8] - '1'));
    else if (!strcmp(what, "shots_holes"))       /* 1: bytes 0xF00..0xF07 of all 9 sectors read 0xFF (plat.h) */
    {
        uint32_t s, i;
        *got = 1;
        for (s = 0; s < SH_SECTORS; s++)
            for (i = 0; i < 8u; i++) {
                const uint8_t *q = plat_data(SH_AT + s * 4096u + 0xF00u + i);
                if (!q || *q != 0xFFu)
                    *got = 0;
            }
    } else
        return 0;
    return 1;
}
#endif
