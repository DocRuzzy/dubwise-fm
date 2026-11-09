/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* MIDI learn and the USB MIDI host (the 2026-11 QoL review 6).
 *
 * MIDI LEARN (SETUP): KNOB 2 up arms it for 20 s. Move a DUBWISE control (a knob, the Big Knob, a mute or throw key, a
 * black key's move ...), then the controller's knob, fader or pad: that control change (its channel and number) now
 * moves the parameter, over its whole range (a switch: on from 64). KNOB 2 down forgets them all (asked twice). 32 at
 * most; a CC learned again moves its new parameter. Kept with SAVE (the store's OBJ_SCNPAT0 + 3, free since the
 * session). A controller's moves are the player's: the song captures them as motion like the knobs', but undo doesn't
 * keep them (it keeps what the panel's hands do).
 * The CCs come in on the audio interrupt (midi_in) into a small ring; the main loop (ml_tick) learns or applies them.
 *
 * USB PORT (SETUP): COMPUTER (the default: audio, MIDI, the mirror, backup) or KEYBOARD (fm1.toml usb_host: a
 * USB-MIDI keyboard or controller plugged into the FM-1, through a powered hub or adapter: the FM-1 gives the port no
 * power). Its notes play as MIDI does (channel 10 the drums, 2 the lead, the rest the bass), its CCs as learned.
 * While it is a keyboard, no computer: no USB audio, mirror or backup. Part of the unity build: dub.c includes it. */

#define ML_N 32u
#define ML_MAGIC 0x3150414Du                     /* "MAP1" */
#define OBJ_MAP (OBJ_SCNPAT0 + 3)
typedef struct {
    uint8_t ch, cc;                              /* 0xFF: none */
    uint16_t id;
} ml_map_t;
static struct {
    uint32_t magic;
    ml_map_t m[ML_N];
} ml;
typedef char ml_fits[sizeof ml <= PLAT_STORE_MAX ? 1 : -1];

#define CCQ 64u                                  /* the audio interrupt's CCs for the main loop */
static volatile uint32_t ccq[CCQ], ccq_w, ccq_r;
static int32_t ml_target = -1;                   /* learning: the parameter last touched, -1 none yet */
static uint32_t ml_until, ml_dirty;              /* learning until (0: not); the map changed since a save */

static void ml_cc_in(uint32_t ch, uint32_t cc, uint32_t v)   /* audio interrupt (midi_in): a CC, for the main loop */
{
    uint32_t w = ccq_w;
    if (w - ccq_r >= CCQ)
        return;                                  /* (full: dropped; a knob sends its next value soon) */
    ccq[w % CCQ] = ch | cc << 8 | v << 16;
    CMDQ_BARRIER();
    ccq_w = w + 1u;
}

static void ml_clear(void)
{
    uint32_t i;
    for (i = 0; i < ML_N; i++)
        ml.m[i].ch = ml.m[i].cc = 0xFFu;
}

static uint32_t ml_count(void)
{
    uint32_t i, n = 0;
    for (i = 0; i < ML_N; i++)
        n += ml.m[i].ch != 0xFFu;
    return n;
}

static void ml_load(void)
{
    if (plat_store_load(OBJ_MAP, &ml, sizeof ml) != (int)sizeof ml || ml.magic != ML_MAGIC)
        ml_clear();
}

static int ml_save(void)                         /* (save_project) the map, if it changed */
{
    if (!ml_dirty)
        return 1;
    ml.magic = ML_MAGIC;
    if (plat_store_save(OBJ_MAP, &ml, sizeof ml) != 0)
        return 0;
    ml_dirty = 0;
    return 1;
}

static void ml_touched(uint32_t id)              /* (undo.c's watch) a parameter the hands changed: learning's target */
{
    char l[24];
    if (!ml_until || plat_ms() >= ml_until || ml_target == (int32_t)id || id == P_SIREN || id == P_SHOT)
        return;                                  /* (the siren and the shots: values a knob can't sweep) */
    ml_target = (int32_t)id;
    ml_until = plat_ms() + 20000u;
    upper(param_desc(id)->name, l, sizeof l);
    app_say(l, "NOW THE CONTROLLER");
}

static void ml_forget_all(void)
{
    ml_clear();
    ml_dirty = 1;
    set_dirty = 1;
    ml_until = 0;
    app_say("MIDI LEARN", "ALL FORGOTTEN");
}

static void ml_learn_start(void)
{
    ml_target = -1;
    ml_until = plat_ms() + 20000u;
    app_say("MIDI LEARN", "MOVE A CONTROL");
}

static void ml_bind(uint32_t ch, uint32_t cc)
{
    uint32_t i, free = ML_N;
    char t[24], n[8], l[24];
    for (i = 0; i < ML_N; i++) {
        if (ml.m[i].ch == ch && ml.m[i].cc == cc)
            break;                               /* learned again: its new parameter */
        if (ml.m[i].ch == 0xFFu && free == ML_N)
            free = i;
    }
    if (i == ML_N)
        i = free;
    if (i == ML_N) {
        app_say("MIDI LEARN", "32 ALREADY: FORGET SOME");
        ml_until = 0;
        return;
    }
    ml.m[i].ch = (uint8_t)ch;
    ml.m[i].cc = (uint8_t)cc;
    ml.m[i].id = (uint16_t)ml_target;
    ml_dirty = 1;
    set_dirty = 1;
    ml_until = 0;
    t[0] = 0;
    text_cat(t, "CC ");
    int_text((int32_t)cc, n);
    text_cat(t, n);
    text_cat(t, " TO ");
    upper(param_desc((uint32_t)ml_target)->name, l, sizeof l);
    text_cat(t, l);
    app_say("LEARNED", t);
}

static void ml_tick(void)                        /* every frame: the CCs that came in, learned or applied */
{
    uint32_t r = ccq_r;
    if (ml_until && plat_ms() >= ml_until) {
        ml_until = 0;
        app_say("MIDI LEARN", "OFF");
    }
    while (r != ccq_w) {
        uint32_t e, ch, cc, v, i;
        CMDQ_BARRIER();
        e = ccq[r % CCQ];
        r++;
        ch = e & 0xFFu;
        cc = (e >> 8) & 0xFFu;
        v = (e >> 16) & 0x7Fu;
        if (ml_until && ml_target >= 0) {
            ml_bind(ch, cc);
            continue;
        }
        for (i = 0; i < ML_N; i++)
            if (ml.m[i].ch == ch && ml.m[i].cc == cc) {
                const pdesc_t *d = param_desc(ml.m[i].id);
                int32_t lo, hi, x;
                if (!d)
                    continue;
                lo = d->min;
                hi = d->max;
                x = hi - lo == 1 ? (v >= 64u ? hi : lo) : lo + ((int32_t)v * (hi - lo) + 63) / 127;
                if (ctl_get(ml.m[i].id) != x) {
                    ctl_set(ml.m[i].id, x);
                    if (mot_momentary(ml.m[i].id) < 0)
                        set_dirty = 1;
                    dub.dirty_strips = dub.dirty_foot = dub.dirty_centre = 1;
                }
            }
    }
    ccq_r = r;
}

/* ------------------------------------------------------------------------------------- the USB MIDI host --- */
static void usbh_tick(void)                      /* every frame: the port as asked, and what happened, said */
{
    static int asked = -1;
    int want = ctl_get(P_USB_PORT) == USBPORT_KEYBOARD, m;
    if (want != asked) {
        asked = want;
        plat_usb_host(want);
    }
    if (plat_usb_lost()) {                       /* a keyboard gone: what its notes held, let go */
        uint32_t ch;
        for (ch = CH_BASS; ch <= CH_LEAD; ch += CH_LEAD - CH_BASS)
            if (eng_voice_note(ch) >= 0)
                ctl_note(ch, eng_voice_note(ch), 0);
    }
    m = plat_usb_host_msg();
    if (m == USBH_MSG_ON)
        app_say("USB PORT", "KEYBOARD: PLUG ONE IN");
    else if (m == USBH_MSG_OFF)
        app_say("USB PORT", "COMPUTER");
    else if (m == USBH_MSG_MIDI)
        app_say("KEYBOARD", plat_usb_host_name()[0] ? plat_usb_host_name() : "CONNECTED");
    else if (m == USBH_MSG_NOMIDI)
        app_say("USB DEVICE", "NOT A MIDI ONE");
    else if (m == USBH_MSG_LOW)
        app_say("USB DEVICE", "NEEDS A POWERED HUB");
    else if (m == USBH_MSG_LOST)
        app_say("KEYBOARD", "UNPLUGGED");
}

#ifdef PLAT_HOST
static int ml_expect(const char *what, int *got)
{
    if (!strcmp(what, "ml_count"))               /* the CCs learned */
        *got = (int)ml_count();
    else if (!strcmp(what, "ml_learning"))       /* 0 off, 1 armed (no control yet), 2 a control chosen */
        *got = ml_until ? (ml_target >= 0 ? 2 : 1) : 0;
    else
        return 0;
    return 1;
}
#endif
