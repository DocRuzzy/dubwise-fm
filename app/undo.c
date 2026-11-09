/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* Undo and redo for what the hands do (the 2026-11 QoL review 2; interface spec 4.6: "Undo everything").
 * HOME + REC undoes, HOME + GLO redoes (HOME + PLAY is the rewind). Every change the player makes to the desk is kept
 * as its old and new value: parameters (mutes, solos, levels, sends, knobs, the Big Knob, sounds, the key ...), pattern
 * steps, a riddim template, a scene recalled. A press of a key or a button starts a new step of the history; a knob's
 * turns run together into one. Not kept: what lets go by itself (throws, freeze, push, dive, drop to wet, drum and
 * bass, the crash), the transport and song capture, the rewind; a demo, a slot or a restore loaded clears it.
 * The last song capture (REC) is still undone by HOME + REC when it is the latest thing done.
 * 1,024 changes (8 KB of the pool); a step that would not fit drops the oldest. Main loop only. Part of the unity
 * build: dub.c includes it. */

#define UNDO_N 1024u
typedef struct {
    uint16_t key;                                /* a parameter's ID, or CTL_STEP_KEY | a step */
    int16_t was, now;
    uint8_t start;                               /* 1: the first change of a step of the history */
    uint8_t rsv;
} undo_ev_t;
static PLAT_POOL undo_ev_t uh[UNDO_N];
static uint32_t uh_n, uh_cur;                    /* kept; done (the rest are redoable) */
static uint8_t undo_live, undo_busy, undo_new;   /* the hands' input now; undo itself working; next is a new step */
static uint32_t undo_tag, undo_t, undo_seq, undo_last_seq;   /* the input that started this step, when; steps so far */

static int undo_kept(uint32_t key)
{
    if (key & CTL_STEP_KEY)
        return 1;
    if (key >= P_CH_BASE && key < P_CH_BASE + NCH * CH_STRIDE && (key - P_CH_BASE) % CH_STRIDE == CHF_THROW_AMT)
        return 0;                                /* (a throw's amount, set by its key's velocity: part of the throw) */
    return key != P_PLAY && key != P_SONG_REC && key != P_SPIN && mot_momentary(key) < 0;
}

static void undo_drop_oldest(void)               /* the oldest step out (the ring full) */
{
    uint32_t k = 1, i;
    while (k < uh_n && !uh[k].start)
        k++;
    for (i = k; i < uh_n; i++)
        uh[i - k] = uh[i];
    uh_n -= k;
    uh_cur = uh_cur > k ? uh_cur - k : 0;
}

static void undo_watch(uint32_t key, int32_t old, int32_t nw)   /* control.c: each change to the mirror */
{
    undo_ev_t *e;
    if (!undo_live || undo_busy || !undo_kept(key))
        return;
    if (uh_cur < uh_n)                           /* a new change: the redo is gone */
        uh_n = uh_cur;
    if (uh_n == UNDO_N) {
        undo_drop_oldest();
        if (uh_n == UNDO_N) {                    /* (a step bigger than the whole ring: nothing kept of it) */
            uh_n = uh_cur = 0;
            return;
        }
    }
    e = &uh[uh_n++];
    e->key = (uint16_t)key;
    e->was = (int16_t)old;
    e->now = (int16_t)nw;
    e->start = undo_new || uh_n == 1u;
    if (e->start) {
        undo_seq++;
        undo_last_seq = undo_seq;
    }
    undo_new = 0;
    uh_cur = uh_n;
}

static uint32_t undo_seq_next(void) { return ++undo_seq; }   /* (a song capture's place among the steps) */
static void undo_live_set(int on)
{
    static int hooked;
    if (!hooked) {
        ctl_set_watch(undo_watch);
        hooked = 1;
    }
    undo_live = (uint8_t)on;
}

static void undo_reset(void)                     /* a load: the history no longer applies */
{
    uh_n = uh_cur = 0;
    undo_new = 1;
}

static void undo_mark(uint32_t tag, uint32_t now)   /* the hands: a new step unless the same knob, still turning */
{
    if (tag != undo_tag || now - undo_t > 700u || tag >= 0x1000u)
        undo_new = 1;
    undo_tag = tag;
    undo_t = now;
}

static void undo_apply(const undo_ev_t *e, int back)
{
    int32_t v = back ? e->was : e->now;
    if (e->key & CTL_STEP_KEY) {
        uint32_t k = e->key & ~CTL_STEP_KEY;
        ctl_step(k / NSTEPS, k % NSTEPS, ctl_step_unpack(v));
    } else {
        ctl_set(e->key, v);
    }
}

static int undo_back(void)                       /* 1: a step undone */
{
    uint32_t i;
    if (!uh_cur)
        return 0;
    undo_busy = 1;
    for (i = uh_cur; i-- > 0;) {
        undo_apply(&uh[i], 1);
        if (uh[i].start)
            break;
    }
    uh_cur = i;
    undo_busy = 0;
    undo_new = 1;
    return 1;
}

static int undo_fwd(void)                        /* 1: a step redone */
{
    uint32_t i = uh_cur;
    if (uh_cur >= uh_n)
        return 0;
    undo_busy = 1;
    do
        undo_apply(&uh[i++], 0);
    while (i < uh_n && !uh[i].start);
    uh_cur = i;
    undo_busy = 0;
    undo_new = 1;
    return 1;
}

static void undo_key(int redo)                   /* HOME + REC, HOME + GLO */
{
    int ok;
    if (!redo && arr.has_undo && arr_cap_seq > undo_last_seq) {   /* the song capture is the latest: that first */
        song_undo_last();
        return;
    }
    ok = redo ? undo_fwd() : undo_back();
    if (ok) {
        set_dirty = 1;
        dub_init();
        st.sig = su_sig = ks.sig = fx_sig = arr.sig = 0;
    }
    app_say(redo ? "REDO" : "UNDO", ok ? "" : "NOTHING MORE");
}

#ifdef PLAT_HOST
static int undo_expect(const char *what, int *got)
{
    if (!strcmp(what, "undo_n"))
        *got = (int)uh_n;
    else if (!strcmp(what, "undo_cur"))
        *got = (int)uh_cur;
    else
        return 0;
    return 1;
}
#endif
