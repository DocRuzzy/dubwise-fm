/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* Sound patches (roadmap 5.4, spec 4.9; the plan: docs/plan-5.4-patches.md). A patch is one channel's whole voice
 * stride (every raw field and the four macros, int8) with a name, 48 bytes. The library: 16 user patches a channel
 * on the device (not a project's: there in every project), in two store objects (channels 1-4, 5-8), read into the
 * pool at boot so browsing never reads the flash. Factory patches come first: INIT on every channel, and the sounds
 * already tuned by ear (5.5 brings the rest).
 * On the SOUND view (EDIT): PRESET steps through the focused channel's patches, loading each at once (a run of turns:
 * one undo step); SAVE held + PRESET stores the sound in the next free slot (KICK 01 ...), or, on a user patch browsed
 * to, overwrites it when asked twice. Never while playing: a flash write would cut the sound (STOP TO SAVE).
 * A field appended to a voice must sound neutral at 0 (params.h), so a patch from before it loads as it was.
 * SysEx (4.3's framing, F0 7D 44 57): 31 ch slot (dump) -> 32 ch slot + 48 bytes 7-bit packed, or 13 ch slot
 * (empty); 33 ch slot + patch (put: 16 kept, 15 refused); 34 ch (the channel's 16, then 12 ch); 36 ch (the current
 * sound as a patch: 32 ch 127 ...); 37 ch + patch (into the current sound). Part of the unity build: dub.c includes it
 * after slots.c (pack7, unpack7). */

#define PATCH_VER 1u
#define PATCH_N 16u                              /* user patches a channel */
#define PATCH_MAGIC 0x31544150u                  /* "PAT1" */
#define OBJ_PATCH(o) ((o) ? OBJ_PATCH1 : OBJ_PATCH0)
typedef struct {
    uint8_t ver, ch, model, flags;               /* flags bit 0: used */
    char name[8];
    int8_t f[VOICE_STRIDE];                      /* P_VOICE(ch, 0..31) */
    uint16_t sum;                                /* CRC-16 of the 44 bytes before it */
    uint16_t rsv;
} patch_t;
typedef char patch_is_48[sizeof(patch_t) == 48u ? 1 : -1];
typedef struct {
    uint32_t magic;
    patch_t p[NCH / 2u][PATCH_N];
} patlib_t;
typedef char patlib_fits[sizeof(patlib_t) <= PLAT_STORE_MAX ? 1 : -1];
static PLAT_POOL patlib_t plib[2];
static uint8_t plib_foreign[2];                  /* 1: an object we can't read (a newer firmware's): kept until asked */

typedef struct {
    uint8_t ch;
    char name[9];
    uint8_t n;
    struct {
        uint8_t f;
        int8_t v;
    } s[4];
} factory_t;
static const factory_t FACTORY[] = {             /* (INIT, every channel's defaults, comes first: not listed) */
    {CH_BASS, "STEPPERS", 3, {{VB_SUB, 100}, {VB_CUTOFF, 20}, {VB_DECAY, 50}}},   /* (demo.c's UK STEPPERS) */
    {CH_SKANK, "STEPPERS", 2, {{VS_DECAY, 30}, {VS_TONE, 70}}},
    {CH_LEAD, "BRASS", 1, {{VL_INST, INST_BRASS}}},
};
#define NFACTORY (sizeof FACTORY / sizeof FACTORY[0])
static int8_t pl_pos[NCH] = {-1, -1, -1, -1, -1, -1, -1, -1};   /* the patch shown: 0..nf-1 factory, nf+s user s */

static uint16_t pl_crc(const uint8_t *p, uint32_t n)   /* CRC-16/CCITT-FALSE */
{
    uint32_t c = 0xFFFFu, i, k;
    for (i = 0; i < n; i++) {
        c ^= (uint32_t)p[i] << 8;
        for (k = 0; k < 8u; k++)
            c = c & 0x8000u ? (c << 1) ^ 0x1021u : c << 1;
    }
    return (uint16_t)(c & 0xFFFFu);
}

static patch_t *pl_user(uint32_t ch, uint32_t s) { return &plib[ch / 4u].p[ch % 4u][s]; }
static int pl_used(const patch_t *p) { return (p->flags & 1u) && p->sum == pl_crc((const uint8_t *)p, 44u); }

static uint32_t pl_nfact(uint32_t ch)            /* factory patches of channel ch: INIT, FACTORY's, the kits' (5.5) */
{
    uint32_t i, n = 1;
    for (i = 0; i < NFACTORY; i++)
        n += FACTORY[i].ch == ch;
    for (i = 0; i < NKITS; i++)
        n += (uint32_t)kit_touches(i, ch);
    return n;
}

static void pl_factory(uint32_t ch, uint32_t k, patch_t *p)   /* factory patch k of channel ch, made up */
{
    uint32_t i, f, j = 0;
    for (i = 0; i < sizeof *p; i++)
        ((uint8_t *)p)[i] = 0;
    p->ver = PATCH_VER;
    p->ch = (uint8_t)ch;
    p->flags = 1;
    for (f = 0; f < VOICE_STRIDE; f++)
        p->f[f] = param_desc(P_VOICE(ch, f)) ? (int8_t)param_def(P_VOICE(ch, f)) : 0;
    if (k == 0u) {
        text_cat(p->name, "INIT");               /* (name: zeroed above, 8 wide, not always ended) */
    } else {
        for (i = 0; i < NFACTORY; i++)
            if (FACTORY[i].ch == ch && ++j == k) {
                for (f = 0; f < FACTORY[i].n; f++)
                    p->f[FACTORY[i].s[f].f] = FACTORY[i].s[f].v;
                for (f = 0; f < 8u && FACTORY[i].name[f]; f++)
                    p->name[f] = FACTORY[i].name[f];
            }
        for (i = 0; i < NKITS; i++)              /* a kit's voice for this channel (5.5) */
            if (kit_touches(i, ch) && ++j == k) {
                const kset_t *s;
                for (s = KITS[i].s; s->id; s++)
                    if (kit_is_voice(s->id) && (s->id - P_VOICE_BASE) / VOICE_STRIDE == ch)
                        p->f[(s->id - P_VOICE_BASE) % VOICE_STRIDE] = (int8_t)s->v;
                for (f = 0; f < 8u && KITS[i].patch[f]; f++)
                    p->name[f] = KITS[i].patch[f];
            }
    }
    p->sum = pl_crc((const uint8_t *)p, 44u);
}

static int pl_get(uint32_t ch, int32_t pos, patch_t *p)   /* the patch at browse position pos: 0 none */
{
    uint32_t nf = pl_nfact(ch);
    if (pos < 0)
        return 0;
    if ((uint32_t)pos < nf) {
        pl_factory(ch, (uint32_t)pos, p);
        return 1;
    }
    if ((uint32_t)pos - nf >= PATCH_N || !pl_used(pl_user(ch, (uint32_t)pos - nf)))
        return 0;
    *p = *pl_user(ch, (uint32_t)pos - nf);
    return 1;
}

static void pl_name(const patch_t *p, char *out)   /* its name as text */
{
    uint32_t i;
    for (i = 0; i < 8u && p->name[i]; i++)
        out[i] = p->name[i];
    out[i] = 0;
}

static void pl_take(uint32_t ch, patch_t *p)     /* the current sound into p */
{
    uint32_t f, i;
    for (i = 0; i < sizeof *p; i++)
        ((uint8_t *)p)[i] = 0;
    p->ver = PATCH_VER;
    p->ch = (uint8_t)ch;
    p->flags = 1;
    for (f = 0; f < VOICE_STRIDE; f++)
        p->f[f] = param_desc(P_VOICE(ch, f)) ? (int8_t)ctl_get(P_VOICE(ch, f)) : 0;
}

static void pl_apply(uint32_t ch, const patch_t *p)   /* a patch into the sound (clamped to today's ranges) */
{
    uint32_t f;
    for (f = 0; f < VOICE_STRIDE; f++)
        if (param_desc(P_VOICE(ch, f)))
            ctl_set(P_VOICE(ch, f), p->f[f]);
    dub.dirty_strips = dub.dirty_foot = dub.dirty_centre = 1;
}

static int pl_edited(uint32_t ch)                /* 1: the sound isn't the patch shown any more */
{
    patch_t p;
    uint32_t f;
    if (!pl_get(ch, pl_pos[ch], &p))
        return 1;
    for (f = 0; f < VOICE_STRIDE; f++)
        if (param_desc(P_VOICE(ch, f)) && ctl_get(P_VOICE(ch, f)) != param_clamp(P_VOICE(ch, f), p.f[f]))
            return 1;
    return 0;
}

static void pl_load(void)                        /* (load_saved) the library from the store */
{
    uint32_t o;
    for (o = 0; o < NCH; o++)
        pl_pos[o] = -1;                          /* (no patch shown yet) */
    for (o = 0; o < 2u; o++) {
        int n = plat_store_load(OBJ_PATCH(o), &plib[o], sizeof plib[o]);
        plib_foreign[o] = 0;
        if (n < 0 || (n == (int)sizeof plib[o] && plib[o].magic == PATCH_MAGIC))
            ;
        else
            plib_foreign[o] = 1;                 /* (a layout we don't know: not ours to write over unasked) */
        if (n < 0 || plib_foreign[o]) {
            uint32_t i;
            for (i = 0; i < sizeof plib[o]; i++)
                ((uint8_t *)&plib[o])[i] = 0;
            plib[o].magic = PATCH_MAGIC;
        }
    }
}

static int pl_store(uint32_t o)                  /* object o to the flash (stopped only): 1 done */
{
    plib[o].magic = PATCH_MAGIC;
    if (plat_store_save(OBJ_PATCH(o), &plib[o], sizeof plib[o]) != 0)
        return 0;
    plib_foreign[o] = 0;
    return 1;
}

static void pl_say(uint32_t ch, const patch_t *p, int edited)
{
    char n[12];
    pl_name(p, n);
    if (edited)
        text_cat(n, "*");
    app_say(CH_NAME[ch], n);
}

static void pl_browse(int32_t e)                 /* PRESET on the SOUND view: the next patch, loaded */
{
    uint32_t ch = (uint32_t)dub.focus, nf = pl_nfact(ch), n = nf + PATCH_N;
    int32_t pos = pl_pos[ch], k;
    patch_t p;
    if (pos < 0)
        pos = e > 0 ? -1 : (int32_t)n;
    for (k = 0; k < (int32_t)n; k++) {           /* (the next one there, the empty slots passed over; at an end: stays) */
        int32_t q = pos + (e > 0 ? 1 : -1);
        if (q < 0 || q >= (int32_t)n)
            break;
        pos = q;
        if (pl_get(ch, pos, &p)) {
            pl_pos[ch] = (int8_t)pos;
            pl_apply(ch, &p);
            pl_say(ch, &p, 0);
            return;
        }
    }
    if (pl_get(ch, pl_pos[ch], &p))
        pl_say(ch, &p, pl_edited(ch));
}

static void pl_save_key(void)                    /* SAVE held + PRESET on the SOUND view */
{
    uint32_t ch = (uint32_t)dub.focus, nf = pl_nfact(ch), o = ch / 4u, s, i;
    int32_t pos = pl_pos[ch];
    patch_t *u;
    char t[24];
    if (eng_playing()) {
        app_say("PATCH", "STOP TO SAVE");
        return;
    }
    if (plib_foreign[o] && !confirm(CF_PATCH_FOREIGN, "REPLACE NEWER PATCHES"))
        return;
    if (pos >= (int32_t)nf && pl_used(pl_user(ch, (uint32_t)pos - nf))) {   /* a user patch shown (edited or not):
                                                                          * overwrite it, asked twice */
        char n[12];
        pl_name(pl_user(ch, (uint32_t)pos - nf), n);
        t[0] = 0;
        text_cat(t, "OVERWRITE ");
        text_cat(t, n);
        if (!confirm(CF_PATCH_OVER + (uint32_t)pos, t))
            return;
        s = (uint32_t)pos - nf;
    } else {
        for (s = 0; s < PATCH_N && pl_used(pl_user(ch, s)); s++)
            ;
        if (s == PATCH_N) {
            app_say("16 ALREADY", "BROWSE TO OVERWRITE");
            return;
        }
    }
    u = pl_user(ch, s);
    pl_take(ch, u);
    for (i = 0; i < 5u && CH_NAME[ch][i]; i++)   /* KICK 01, SNARE 07 ... */
        u->name[i] = CH_NAME[ch][i];
    u->name[i] = ' ';
    u->name[i + 1u] = (char)('0' + (s + 1u) / 10u);
    u->name[i + 2u] = (char)('0' + (s + 1u) % 10u);
    u->sum = pl_crc((const uint8_t *)u, 44u);
    if (!pl_store(o)) {
        u->flags = 0;
        app_say("PATCH", "SAVE FAILED");
        return;
    }
    pl_pos[ch] = (int8_t)(nf + s);
    pl_name(u, t);
    app_say("SAVED", t);
    dub.dirty_foot = 1;
}

/* ---------------------------------------------------------------------------------------------- SysEx --- */
static void pl_reply(uint32_t cmd, uint32_t ch, uint32_t slot, const patch_t *p)
{
    uint8_t f[10 + (48u * 8u + 6u) / 7u];
    uint32_t n = 0;
    f[n++] = 0xF0, f[n++] = 0x7D, f[n++] = 0x44, f[n++] = 0x57, f[n++] = (uint8_t)cmd;
    f[n++] = (uint8_t)ch, f[n++] = (uint8_t)slot;
    if (p)
        n += pack7((const uint8_t *)p, sizeof *p, f + n);
    f[n++] = 0xF7;
    plat_sysex_send(f, n);
}

static struct {
    uint8_t on, ch, next;
} pld;                                           /* a channel's library going out, a patch a frame as room allows */

static void pl_sx_tick(void)
{
    while (pld.on && plat_sysex_room() >= 80u) {
        if (pld.next >= PATCH_N) {
            pl_reply(0x12, pld.ch, 0, 0);
            pld.on = 0;
            break;
        }
        if (pl_used(pl_user(pld.ch, pld.next)))
            pl_reply(0x32, pld.ch, pld.next, pl_user(pld.ch, pld.next));
        else
            pl_reply(0x13, pld.ch, pld.next, 0);
        pld.next++;
    }
}

static int pl_check(const uint8_t *raw, uint32_t got, uint32_t ch, patch_t *p)   /* a patch that came in: 1 good */
{
    uint32_t i;
    if (got != sizeof *p)
        return 0;
    for (i = 0; i < sizeof *p; i++)
        ((uint8_t *)p)[i] = raw[i];
    return p->ver >= 1u && p->ver <= PATCH_VER && p->ch == ch && pl_used(p);
}

static int pl_sysex(const uint8_t *p, uint32_t n)   /* (slot_sysex) commands 31..37: 1 ours */
{
    uint32_t cmd = p[3], ch = p[4], s = n > 5u ? p[5] : 0u, got;
    uint8_t raw[56];
    patch_t pt;
    if (ch >= NCH)
        return 1;
    if (cmd == 0x31u && n >= 6u && s < PATCH_N) {   /* dump one */
        if (pl_used(pl_user(ch, s)))
            pl_reply(0x32, ch, s, pl_user(ch, s));
        else
            pl_reply(0x13, ch, s, 0);
    } else if (cmd == 0x33u && n >= 6u && s < PATCH_N) {   /* put one */
        got = unpack7(p + 6, n - 6u, raw, sizeof raw);
        if (eng_playing() || plib_foreign[ch / 4u] || !pl_check(raw, got, ch, &pt)) {
            pl_reply(0x15, ch, s, 0);
            return 1;
        }
        *pl_user(ch, s) = pt;
        if (!pl_store(ch / 4u)) {
            pl_user(ch, s)->flags = 0;
            pl_reply(0x15, ch, s, 0);
            return 1;
        }
        pl_reply(0x16, ch, s, 0);
    } else if (cmd == 0x34u) {                   /* a channel's 16 */
        pld.on = 1, pld.ch = (uint8_t)ch, pld.next = 0;
    } else if (cmd == 0x36u) {                   /* the current sound */
        pl_take(ch, &pt);
        text_cat(pt.name, "CURRENT");
        pt.sum = pl_crc((const uint8_t *)&pt, 44u);
        pl_reply(0x32, ch, 0x7Fu, &pt);
    } else if (cmd == 0x37u) {                   /* into the current sound */
        got = unpack7(p + 5, n - 5u, raw, sizeof raw);
        if (!pl_check(raw, got, ch, &pt)) {
            pl_reply(0x15, ch, 0x7Fu, 0);
            return 1;
        }
        pl_apply(ch, &pt);
        pl_pos[ch] = -1;
        pl_reply(0x16, ch, 0x7Fu, 0);
    }
    return 1;
}

/* ----------------------------------------------------------------------------------------- a kit loaded --- */
static int32_t kit_pos(uint32_t k, uint32_t ch)  /* kit k's voice as channel ch's factory patch: its position, -1 */
{
    uint32_t i, j = 1;
    for (i = 0; i < NFACTORY; i++)
        j += FACTORY[i].ch == ch;
    for (i = 0; i < NKITS; i++)
        if (kit_touches(i, ch)) {
            if (i == k)
                return (int32_t)j;
            j++;
        }
    return -1;
}

static void kit_load(uint32_t k, int bus)        /* (SETUP's LOAD KIT, SAVE) the voices it sets, whole; the bus too */
{
    uint32_t ch;
    const kset_t *s;
    patch_t p;
    for (ch = 0; ch < NCH; ch++) {
        int32_t pos = kit_pos(k, ch);
        if (pos >= 0 && pl_get(ch, pos, &p)) {
            pl_apply(ch, &p);
            pl_pos[ch] = (int8_t)pos;
        }
    }
    if (bus)
        for (s = KITS[k].s; s->id; s++)
            if (!kit_is_voice(s->id))
                ctl_set(s->id, s->v);
    set_dirty = 1;
    dub.dirty_hdr = dub.dirty_strips = dub.dirty_foot = dub.dirty_centre = 1;
    app_say(KITS[k].name, bus ? "VOICES AND BUS LOADED" : "VOICES LOADED");
}

#ifdef PLAT_HOST
static void pl_oldsound(void)                    /* a sound saved before 5.3 (no new fields, no macros), no library */
{
    static const uint16_t NEW53[] = {P_VOICE(CH_KICK, VK_DRIVE), P_VOICE(CH_KICK, VK_CTONE), P_VOICE(CH_SNARE, VR_DROP),
                                     P_VOICE(CH_SNARE, VR_HPF), P_VOICE(CH_HATS, VH_HPF), P_VOICE(CH_BASS, VB_FEEDBACK),
                                     P_VOICE(CH_BASS, VB_DRIVE), P_VOICE(CH_BASS, VB_GLIDE), P_VOICE(CH_SKANK, VS_ATTACK),
                                     P_VOICE(CH_ORGAN, VO_DETUNE), P_VOICE(CH_LEAD, VL_DETUNE)};
    uint32_t i, k;
    snd.magic = SOUND_MAGIC;
    for (i = 0; i < P_NIDS - P_VOICE_BASE; i++) {
        uint32_t id = P_VOICE_BASE + i, f = i % VOICE_STRIDE;
        snd.v[i] = param_desc(id) ? (int16_t)param_def(id) : 0;
        if (f >= VM_BASE)
            snd.v[i] = 0;                        /* (no macros then) */
        for (k = 0; k < sizeof NEW53 / sizeof NEW53[0]; k++)
            if (id == NEW53[k])
                snd.v[i] = 0;                    /* (nor these: what an old save holds there) */
    }
    snd.v[P_VOICE(CH_KICK, VK_TUNE) - P_VOICE_BASE] = 50;   /* (a sound of its own, to see it come back) */
    snd.v[P_VOICE(CH_BASS, VB_CUTOFF) - P_VOICE_BASE] = 60;
    plat_store_save(OBJ_SOUND, &snd, sizeof snd);
    plat_store_save(OBJ_PATCH0, &snd, 0);        /* no library then (an empty object reads as none) */
    plat_store_save(OBJ_PATCH1, &snd, 0);
}

static void pl_foreign_obj(void)                 /* a newer firmware's library object (a downgrade) */
{
    static uint8_t b[64];
    uint32_t i;
    for (i = 0; i < sizeof b; i++)
        b[i] = (uint8_t)i;
    b[0] = 'P', b[1] = 'A', b[2] = 'T', b[3] = '2';
    plat_store_save(OBJ_PATCH0, b, sizeof b);
}

static int pl_expect(const char *what, int *got)
{
    if (!strcmp(what, "pl_pos"))                 /* the focused channel's patch shown, -1 none */
        *got = pl_pos[dub.focus];
    else if (!strncmp(what, "pl_used", 7)) {     /* pl_usedC: user patches of channel C */
        uint32_t ch = (uint32_t)(atoi(what + 7) - 1) % NCH, s, n = 0;
        for (s = 0; s < PATCH_N; s++)
            n += (uint32_t)pl_used(pl_user(ch, s));
        *got = (int)n;
    } else if (!strcmp(what, "pl_edited"))
        *got = pl_edited((uint32_t)dub.focus);
    else if (!strcmp(what, "voice_int8")) {      /* 1: every voice field's range fits a patch's int8 */
        uint32_t id;
        *got = 1;
        for (id = P_VOICE_BASE; id < P_NIDS; id++)
            if (param_desc(id) && (param_desc(id)->min < -128 || param_desc(id)->max > 127))
                *got = 0;
    } else if (!strcmp(what, "pl_foreign"))
        *got = plib_foreign[0] | plib_foreign[1] << 1;
    else
        return 0;
    return 1;
}
#endif
