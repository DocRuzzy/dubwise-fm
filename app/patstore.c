/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* The project's patterns, scenes and song in the store (the session, roadmap 4.8), and what an older save becomes.
 *
 *   OBJ_PATTERN   "PAT2": each channel's pattern playing, and each pattern's length, grid and swing
 *   OBJ_PST0..2   "PST1": the steps, three channels an object (the objects the scenes' pattern copies used to fill)
 *   OBJ_SCENES    "SCN2": the scenes' desks, and the pattern each channel plays in a "P" scene
 *   OBJ_SONG      "SON2": the song's bars (7 bytes: the patterns each bar played)
 *
 * Before the session (PAT1, SCN1 + SCP1, SON1) a channel had one pattern, and a "P" scene kept a copy of all eight.
 * Loaded, the one pattern becomes A; each "P" scene's copy becomes the pattern of B..D that is the same, or the next
 * free one (D, once all four are taken), and the scene plays it. Song slots saved before (slots.c, DSL1) the same.
 * Part of the unity build: app.c includes it. */

#define OBJ_PST0 OBJ_SCNPAT0
#define PST_CH 3u                                /* channels an object */
typedef char pst_objs_fit[(NCH + PST_CH - 1u) / PST_CH <= OBJ_SCNPAT5 - OBJ_SCNPAT0 + 1u ? 1 : -1];

#define PAT2_MAGIC 0x32544150u                   /* "PAT2" */
typedef struct {
    uint32_t magic;
    uint8_t cur[NCH];
    pmeta_t meta[NCH][NPAT];
} pat2_store_t;
#define PST_MAGIC 0x32545350u                    /* "PST2" (5.8, 5.9: CHANCE and REPEAT in a step's level byte, a drum
                                                  * step's TUNE and DECAY locks in its gate) */
#define PST1_MAGIC 0x31545350u                   /* "PST1": a drum step's gate held a live-recorded length: cleared */
typedef struct {
    uint32_t magic;
    uint8_t ch0, rsv[3];
    step_t steps[PST_CH][NPAT][NSTEPS];
} pst_store_t;
#define SCN2_MAGIC 0x324E4353u                   /* "SCN2" */
typedef struct {
    uint32_t magic;
    uint8_t used[NSCENES], scope[NSCENES];
    int16_t desk[NSCENES][SCENE_DESK];
    uint8_t pat[NSCENES][NCH];
} scn2_store_t;
#define SON2_MAGIC 0x324E4F53u                   /* "SON2" */
typedef struct {
    uint32_t magic;
    song_bar_t bars[SONG_BARS];
} son2_store_t;

/* before the session */
#define PAT1_MAGIC 0x50415431u                   /* "PAT1" (its bytes as stored, as SCN1, SCP1 and SON1 below) */
#define SCN1_MAGIC 0x53434E31u
#define SCP1_MAGIC 0x53435031u
#define SON1_MAGIC 0x534F4E31u
typedef struct {
    uint8_t lvl;
    int8_t note;
} step_v1_t;
typedef struct {
    uint32_t magic;
    uint8_t len[NCH];
    step_v1_t steps[NCH][NSTEPS];
} pat1_store_t;
typedef struct {
    uint32_t magic;
    uint8_t used[NSCENES], scope[NSCENES];
    int16_t desk[NSCENES][SCENE_DESK];
} scn1_store_t;
#define SCP1_PER 3u
typedef struct {
    uint32_t magic;
    uint8_t len[SCP1_PER][NCH];
    step_v1_t pat[SCP1_PER][NCH][NSTEPS];
} scp1_store_t;
typedef struct {
    uint8_t flags, scene, mute, solo;
    int8_t hpf;
    uint8_t section;
} song_v1_t;
typedef struct {
    uint32_t magic;
    song_v1_t bars[SONG_BARS];
} son1_store_t;
typedef struct {                                 /* a scene in a DSL1 slot, as it was in memory */
    uint8_t used, scope;
    int16_t desk[SCENE_DESK];
    uint8_t len[NCH];
    step_v1_t pat[NCH][NSTEPS];
} scene_v1_t;
typedef char scene_v1_size[sizeof(scene_v1_t) == 1136u ? 1 : -1];

static union {                                   /* one object at a time */
    pat2_store_t a;
    pst_store_t s;
    scn2_store_t c;
    son2_store_t g;
    pat1_store_t a1;
    scn1_store_t c1;
    scp1_store_t p1;
    son1_store_t g1;
} pbuf;
typedef char pst_fits[(sizeof(pat2_store_t) <= PLAT_STORE_MAX && sizeof(pst_store_t) <= PLAT_STORE_MAX
                       && sizeof(scn2_store_t) <= PLAT_STORE_MAX && sizeof(son2_store_t) <= PLAT_STORE_MAX) ? 1 : -1];

/* ------------------------------------------------------------------------------------------- migration --- */
static uint8_t mig_n[NCH];                       /* patterns given out so far, a channel */

static step_t step_v1(step_v1_t o)
{
    step_t s;
    s.lvl = o.lvl < LVL_N ? o.lvl : LVL_OFF;
    s.note = o.note;
    s.off = 0;
    s.gate = 0;
    return s;
}

static void mig_start(void)                      /* every pattern empty, A playing */
{
    uint32_t ch, p, i;
    for (ch = 0; ch < NCH; ch++) {
        mig_n[ch] = 1;
        ctl_set(P_CH(ch, CHF_PAT), 0);
        for (p = 0; p < NPAT; p++) {
            pmeta_t m = pmeta_def();
            ctl_meta_set(ch, p, m);
            for (i = 0; i < NSTEPS; i++) {
                step_t s = {LVL_OFF, default_note(ch), 0, 0};
                ctl_step_at(ch, p, i, s);
            }
        }
    }
}

static void mig_put(uint32_t ch, uint32_t p, uint32_t len, const step_v1_t *st)   /* (st: maybe unaligned flash) */
{
    pmeta_t m = pmeta_def();
    uint32_t i;
    m.len = (uint8_t)len;
    ctl_meta_set(ch, p, m);
    for (i = 0; i < NSTEPS; i++) {
        step_v1_t o;
        o.lvl = ((const uint8_t *)st)[2u * i];
        o.note = (int8_t)((const uint8_t *)st)[2u * i + 1u];
        ctl_step_at(ch, p, i, step_v1(o));
    }
}

static uint32_t mig_pat(uint32_t ch, uint32_t len, const step_v1_t *st)   /* an old "P" scene's copy: its pattern */
{
    uint32_t p, i;
    for (p = 0; p < mig_n[ch]; p++) {
        if (ctl_meta_get(ch, p).len != len)
            continue;
        for (i = 0; i < NSTEPS; i++) {
            step_v1_t o;
            o.lvl = ((const uint8_t *)st)[2u * i];
            o.note = (int8_t)((const uint8_t *)st)[2u * i + 1u];
            if (step_pack(ctl_get_step_at(ch, p, i)) != step_pack(step_v1(o)))
                break;
        }
        if (i == NSTEPS)
            return p;                            /* the same as one already there */
    }
    if (mig_n[ch] == NPAT)
        return NPAT - 1u;                        /* (all four taken: the last) */
    p = mig_n[ch]++;
    mig_put(ch, p, len, st);
    return p;
}

static void mig_bar(song_bar_t *d, const song_v1_t *o)
{
    d->flags = o->flags & (BAR_SET | BAR_SECTION);
    d->section = (uint8_t)(o->section < SECTIONS ? o->section : 0u);
    d->scene = o->scene;
    d->mute = o->mute;
    d->solo = o->solo;
    d->hpf = o->hpf;
    d->pats[0] = d->pats[1] = 0;
}

/* ------------------------------------------------------------------------------------------------ save --- */
static int pats_save(void)
{
    uint32_t ch, p, i, o;
    pbuf.a.magic = PAT2_MAGIC;
    for (ch = 0; ch < NCH; ch++) {
        pbuf.a.cur[ch] = (uint8_t)ctl_get(P_CH(ch, CHF_PAT));
        for (p = 0; p < NPAT; p++)
            pbuf.a.meta[ch][p] = ctl_meta_get(ch, p);
    }
    if (plat_store_save(OBJ_PATTERN, &pbuf.a, sizeof pbuf.a) != 0)
        return 0;
    for (o = 0; o * PST_CH < NCH; o++) {
        pbuf.s.magic = PST_MAGIC;
        pbuf.s.ch0 = (uint8_t)(o * PST_CH);
        pbuf.s.rsv[0] = pbuf.s.rsv[1] = pbuf.s.rsv[2] = 0;
        for (ch = 0; ch < PST_CH; ch++)
            for (p = 0; p < NPAT; p++)
                for (i = 0; i < NSTEPS; i++) {
                    step_t z = {0, 0, 0, 0};
                    pbuf.s.steps[ch][p][i] = o * PST_CH + ch < NCH ? ctl_get_step_at(o * PST_CH + ch, p, i) : z;
                }
        if (plat_store_save(OBJ_PST0 + o, &pbuf.s, sizeof pbuf.s) != 0)
            return 0;
    }
    return 1;
}

static int scenes_save(void)                     /* the scenes, if anything changed */
{
    uint32_t s, k, ch;
    if (!scenes_dirty)
        return 1;
    pbuf.c.magic = SCN2_MAGIC;
    for (s = 0; s < NSCENES; s++) {
        pbuf.c.used[s] = scenes[s].used;
        pbuf.c.scope[s] = scenes[s].scope;
        for (k = 0; k < SCENE_DESK; k++)
            pbuf.c.desk[s][k] = scenes[s].desk[k];
        for (ch = 0; ch < NCH; ch++)
            pbuf.c.pat[s][ch] = scenes[s].pat[ch];
    }
    if (plat_store_save(OBJ_SCENES, &pbuf.c, sizeof pbuf.c) != 0)
        return 0;
    scenes_dirty = 0;
    return 1;
}

static int song_save(void)
{
    uint32_t i;
    pbuf.g.magic = SON2_MAGIC;
    for (i = 0; i < SONG_BARS; i++)
        pbuf.g.bars[i] = song[i];
    return plat_store_save(OBJ_SONG, &pbuf.g, sizeof pbuf.g) == 0;
}

static int proj_save(void) { return pats_save() && scenes_save() && song_save(); }

/* ------------------------------------------------------------------------------------------------ load --- */
static void scene_put(uint32_t s, uint8_t used, uint8_t scope, const int16_t *desk)
{
    uint32_t k, ch;
    scenes[s].used = 0;
    scenes[s].scope = scope < SCOPE_N ? scope : SCOPE_DESK;
    for (k = 0; k < SCENE_DESK; k++)
        scenes[s].desk[k] = desk[k];
    for (ch = 0; ch < NCH; ch++)
        scenes[s].pat[ch] = 0;
    scenes[s].used = used ? 1u : 0u;
}

static void proj_load(void)                      /* the store's patterns, scenes and song (an older save converted) */
{
    uint32_t s, ch, p, i, o;
    int old = 0;
    if (plat_store_load(OBJ_SCENES, &pbuf.c, sizeof pbuf.c) == (int)sizeof pbuf.c && pbuf.c.magic == SCN2_MAGIC) {
        for (s = 0; s < NSCENES; s++) {
            scene_put(s, pbuf.c.used[s], pbuf.c.scope[s], pbuf.c.desk[s]);
            for (ch = 0; ch < NCH; ch++)
                scenes[s].pat[ch] = (uint8_t)(pbuf.c.pat[s][ch] < NPAT ? pbuf.c.pat[s][ch] : 0u);
        }
    } else if (plat_store_load(OBJ_SCENES, &pbuf.c1, sizeof pbuf.c1) == (int)sizeof pbuf.c1
               && pbuf.c1.magic == SCN1_MAGIC) {
        for (s = 0; s < NSCENES; s++)
            scene_put(s, pbuf.c1.used[s], pbuf.c1.scope[s], pbuf.c1.desk[s]);
        old = 1;
    }
    if (plat_store_load(OBJ_SONG, &pbuf.g, sizeof pbuf.g) == (int)sizeof pbuf.g && pbuf.g.magic == SON2_MAGIC) {
        for (i = 0; i < SONG_BARS; i++)
            song[i] = pbuf.g.bars[i];
    } else if (plat_store_load(OBJ_SONG, &pbuf.g1, sizeof pbuf.g1) == (int)sizeof pbuf.g1
               && pbuf.g1.magic == SON1_MAGIC) {
        for (i = 0; i < SONG_BARS; i++)
            mig_bar(&song[i], &pbuf.g1.bars[i]);
    }
    if (plat_store_load(OBJ_PATTERN, &pbuf.a, sizeof pbuf.a) == (int)sizeof pbuf.a && pbuf.a.magic == PAT2_MAGIC) {
        uint8_t cur[NCH];
        for (ch = 0; ch < NCH; ch++) {
            cur[ch] = pbuf.a.cur[ch];
            for (p = 0; p < NPAT; p++)
                ctl_meta_set(ch, p, pbuf.a.meta[ch][p]);
        }
        for (ch = 0; ch < NCH; ch++)
            ctl_set(P_CH(ch, CHF_PAT), cur[ch]);
        for (o = 0; o * PST_CH < NCH; o++) {     /* (more than the queue holds: the rest goes on later frames) */
            if (plat_store_load(OBJ_PST0 + o, &pbuf.s, sizeof pbuf.s) != (int)sizeof pbuf.s
                || (pbuf.s.magic != PST_MAGIC && pbuf.s.magic != PST1_MAGIC))
                continue;
            for (ch = 0; ch < PST_CH && o * PST_CH + ch < NCH; ch++)
                for (p = 0; p < NPAT; p++)
                    for (i = 0; i < NSTEPS; i++) {
                        step_t st = pbuf.s.steps[ch][p][i];
                        if (pbuf.s.magic == PST1_MAGIC) {   /* the converter (plan 5.8/5.9): no locks, 100 %, no repeat */
                            st.lvl &= 3u;
                            if (is_drum(o * PST_CH + ch))
                                st.gate = 0;
                        }
                        if (STEP_REPEAT(st) >= REP_N)
                            STEP_REPEAT_SET(st, REP_OFF);
                        ctl_step_at(o * PST_CH + ch, p, i, st);
                    }
            if (pbuf.s.magic == PST1_MAGIC)
                set_dirty = 1;                   /* (saved as PST2 at the next SAVE) */
        }
        return;
    }
    if (plat_store_load(OBJ_PATTERN, &pbuf.a1, sizeof pbuf.a1) != (int)sizeof pbuf.a1 || pbuf.a1.magic != PAT1_MAGIC)
        return;                                  /* nothing saved: the boot's ONE DROP */
    mig_start();                                 /* a save from before the session: its pattern is A */
    for (ch = 0; ch < NCH; ch++)
        mig_put(ch, 0, pbuf.a1.len[ch], pbuf.a1.steps[ch]);
    if (old)                                     /* its "P" scenes' copies: patterns of their own */
        for (o = 0; o * SCP1_PER < NSCENES; o++) {
            if (plat_store_load(OBJ_SCNPAT0 + o, &pbuf.p1, sizeof pbuf.p1) != (int)sizeof pbuf.p1
                || pbuf.p1.magic != SCP1_MAGIC)
                continue;
            for (i = 0; i < SCP1_PER && o * SCP1_PER + i < NSCENES; i++) {
                s = o * SCP1_PER + i;
                if (!scenes[s].used || scenes[s].scope != SCOPE_PATTERNS)
                    continue;
                for (ch = 0; ch < NCH; ch++)
                    scenes[s].pat[ch] = (uint8_t)mig_pat(ch, pbuf.p1.len[i][ch], pbuf.p1.pat[i][ch]);
            }
        }
    scenes_dirty = 0xFFFFu;                      /* (saved in the new form at the next SAVE) */
    set_dirty = 1;
}

#ifdef PLAT_HOST
/* oldsave: a save as one from before the session would be (tests/patterns.txt): the kick's pattern steps 1 and 9
 * (16 long); scene 1 "P" with the kick on 1, 5, 9, 13; scene 2 "P" with the kick as the pattern itself; bar 1 of the
 * song set, with the scene 1 */
static void pst_oldsave(void)
{
    uint32_t ch, i, s;
    for (i = 0; i < sizeof pbuf; i++)
        ((uint8_t *)&pbuf)[i] = 0;
    pbuf.a1.magic = PAT1_MAGIC;
    for (ch = 0; ch < NCH; ch++) {
        pbuf.a1.len[ch] = 16;
        for (i = 0; i < NSTEPS; i++)
            pbuf.a1.steps[ch][i].note = default_note(ch);
    }
    pbuf.a1.steps[CH_KICK][0].lvl = pbuf.a1.steps[CH_KICK][8].lvl = LVL_NORMAL;
    plat_store_save(OBJ_PATTERN, &pbuf.a1, sizeof pbuf.a1);
    for (i = 0; i < sizeof pbuf; i++)
        ((uint8_t *)&pbuf)[i] = 0;
    pbuf.c1.magic = SCN1_MAGIC;
    pbuf.c1.used[0] = pbuf.c1.used[1] = 1;
    pbuf.c1.scope[0] = pbuf.c1.scope[1] = SCOPE_PATTERNS;
    for (s = 0; s < 2u; s++)
        for (i = 0; i < SCENE_DESK; i++)
            pbuf.c1.desk[s][i] = (int16_t)param_def(scene_param(i));
    plat_store_save(OBJ_SCENES, &pbuf.c1, sizeof pbuf.c1);
    for (i = 0; i < sizeof pbuf; i++)
        ((uint8_t *)&pbuf)[i] = 0;
    pbuf.p1.magic = SCP1_MAGIC;
    for (s = 0; s < SCP1_PER; s++)
        for (ch = 0; ch < NCH; ch++) {
            pbuf.p1.len[s][ch] = 16;
            for (i = 0; i < NSTEPS; i++)
                pbuf.p1.pat[s][ch][i].note = default_note(ch);
        }
    for (i = 0; i < 16u; i += 4u)
        pbuf.p1.pat[0][CH_KICK][i].lvl = LVL_NORMAL;
    pbuf.p1.pat[1][CH_KICK][0].lvl = pbuf.p1.pat[1][CH_KICK][8].lvl = LVL_NORMAL;
    plat_store_save(OBJ_SCNPAT0, &pbuf.p1, sizeof pbuf.p1);
    for (i = 0; i < sizeof pbuf; i++)
        ((uint8_t *)&pbuf)[i] = 0;
    pbuf.g1.magic = SON1_MAGIC;
    pbuf.g1.bars[0].flags = BAR_SET | BAR_SECTION;
    pbuf.g1.bars[0].scene = 1;
    pbuf.g1.bars[0].section = 3;
    plat_store_save(OBJ_SONG, &pbuf.g1, sizeof pbuf.g1);
}

static void pst_oldsteps(void)                   /* (tests) the steps saved as PST1, the kick's live-recorded gates in
                                                  * them: a save from before 5.8 */
{
    uint32_t o, ch, p, i;
    for (o = 0; o * PST_CH < NCH; o++) {
        if (plat_store_load(OBJ_PST0 + o, &pbuf.s, sizeof pbuf.s) != (int)sizeof pbuf.s)
            continue;
        pbuf.s.magic = PST1_MAGIC;
        for (ch = 0; ch < PST_CH; ch++)
            for (p = 0; p < NPAT; p++)
                for (i = 0; i < NSTEPS; i++)
                    if (o * PST_CH + ch == CH_KICK && LVL(pbuf.s.steps[ch][p][i]))
                        pbuf.s.steps[ch][p][i].gate = 37;   /* (as live recording left it) */
        plat_store_save(OBJ_PST0 + o, &pbuf.s, sizeof pbuf.s);
    }
}

static int pst_expect(const char *what, int *got)   /* scnpatS_C: scene S's pattern for channel C (both from 1) */
{
    if (strncmp(what, "scnpat", 6) || what[6] < '1' || !strchr(what, '_'))
        return 0;
    *got = scenes[(atoi(what + 6) - 1) % NSCENES].pat[(atoi(strchr(what, '_') + 1) - 1) % NCH];
    return 1;
}
#endif
