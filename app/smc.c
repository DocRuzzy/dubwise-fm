/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors
 *
 * After hortator-bt's smc.c (the M-VAVE SMC-Mixer in its DAW / Mackie Control mode, over Bluetooth there;
 * Copyright (C) 2026 DEADACTIVE, GPL-3.0-only, https://github.com/DEADACTIVE): its mapping of the mixer's messages,
 * the faders' soft takeover and the LEDs fed back. Rewritten for Dubwise's desk and its USB host. */
/* The SMC-Mixer as the desk (roadmap 6.5). SETUP USB PORT KEYBOARD, the mixer plugged in (it needs its own power: the
 * FM-1 gives the port none) in its DAW mode: a device whose name has "SMC" in it is taken as the mixer, not as a
 * keyboard. Its eight strips are Dubwise's:
 *   faders            the strips' levels (soft takeover: a fader acts once it's within 2 of the level, or across it)
 *   MUTE              a mute, on the next quantise line (as a tap of the mute key)
 *   SOLO              a solo, on the line
 *   REC               a throw while held (to the strip's destination, in full)
 *   SELECT            the focus (ALGORITHM's)
 *   knobs 1..8        BIG KNOB, ECHO TIME, ECHO FB, ECHO RET, SPRING RET, the focused strip's echo and spring sends,
 *                     TEMPO (each click a step)
 *   PLAY / STOP / REC the transport, and the song's capture
 * Out: the LEDs (MUTE, SOLO, REC as the throws, SELECT the focus, PLAY, STOP, REC) as notes 127 / 0, only what changed;
 * each fader's level when it changed on the FM-1 (the mixer blinks a fader not picked up yet).
 * In: on the audio interrupt (midi_in), the mixer's packets into a ring; here, in the main loop, they're played as the
 * hands' moves (undo, motion capture, the guard: all as the panel's). Part of the unity build: dub.c includes it. */

#define SMC_Q 64u
static volatile uint32_t smc_q[SMC_Q];
static volatile uint32_t smc_qw, smc_qr;
static volatile uint8_t smc_active;              /* 1: the USB device is an SMC-Mixer (midi_in hands its packets here) */
static struct {
    uint32_t lit[2], known[2];                   /* the LEDs as sent ([0]: notes 0x00..0x1F; [1]: 0x5D..0x5F); known 0:
                                                  * to send */
    uint8_t lv[NCH], last[NCH];                  /* each strip's level as the fader set it, or as last sent; each
                                                  * fader's last report */
    uint8_t picked, seen, pend;                  /* bits a fader: picked up; last[] valid; level to send */
    uint8_t on;
    uint32_t presses;
} smc;

static void smc_push(uint32_t pkt)               /* (midi_in, the audio interrupt) */
{
    uint32_t w = smc_qw;
    if (w - smc_qr < SMC_Q) {
        smc_q[w % SMC_Q] = pkt;
        smc_qw = w + 1u;
    }
}

static int smc_name(const char *n)               /* "SMC" anywhere in the device's name */
{
    for (; n[0] && n[1] && n[2]; n++)
        if ((n[0] | 32) == 's' && (n[1] | 32) == 'm' && (n[2] | 32) == 'c')
            return 1;
    return 0;
}

static void smc_fresh(void)                      /* a new connection: every LED to send, every fader unpicked */
{
    uint32_t i;
    smc.known[0] = smc.known[1] = 0;
    smc.picked = smc.seen = 0;
    smc.pend = 0xFFu;
    for (i = 0; i < NCH; i++)
        smc.lv[i] = (uint8_t)ctl_get(P_CH(i, CHF_LEVEL));
    smc.on = 1;
}

static void smc_notice(uint32_t n)               /* the level moved on the FM-1: the fader unpicked, its place sent */
{
    uint32_t v = (uint32_t)ctl_get(P_CH(n, CHF_LEVEL));
    if (v != smc.lv[n]) {
        smc.lv[n] = (uint8_t)v;
        smc.picked &= (uint8_t)~(1u << n);
        smc.pend |= (uint8_t)(1u << n);
    }
}

static void smc_fader(uint32_t n, uint32_t v)    /* fader n at v (0..127): soft takeover (hortator's) */
{
    uint32_t b = 1u << n, s, crossed;
    smc_notice(n);
    s = smc.lv[n];
    crossed = (smc.seen & b) && ((smc.last[n] < s) != (v < s));
    smc.last[n] = (uint8_t)v;
    smc.seen |= (uint8_t)b;
    if (!(smc.picked & b)) {
        if (!crossed && (v + 2u < s || v > s + 2u))
            return;
        smc.picked |= (uint8_t)b;
    }
    if (v == s)
        return;
    smc.lv[n] = (uint8_t)v;
    smc.pend |= (uint8_t)b;
    undo_mark(0x300u + n, plat_ms());            /* (a run of one fader: one undo step; under 0x1000: runs join) */
    ctl_set(P_CH(n, CHF_LEVEL), (int32_t)v);
    dub.dirty_strips = 1;
}

static void smc_step(uint32_t id, int32_t d, const char *name)   /* a knob's clicks on parameter id, said */
{
    char t[12];
    undo_mark(0x308u + (id & 7u), plat_ms());     /* (a knob's run: one step) */
    ctl_set(id, ctl_get(id) + d);
    int_text(ctl_get(id), t);
    app_say(name, t);
    dub.dirty_hdr = dub.dirty_strips = dub.dirty_foot = 1;
    set_dirty = 1;
}

static void smc_knob(uint32_t k, int32_t c)      /* knob k (0..7), c clicks */
{
    uint32_t f = (uint32_t)dub.focus;
    switch (k) {
    case 0:                                      /* the Big Knob, a detent a click (as SELECT) */
        encoder(EN_SELECT, c);
        break;
    case 1:
        smc_step(P_ECHO_TIME, c, "ECHO TIME");
        break;
    case 2:
        smc_step(P_ECHO_FB, 2 * c, "ECHO FB");
        break;
    case 3:
        smc_step(P_ECHO_RET, 2 * c, "ECHO RETURN");
        break;
    case 4:
        smc_step(P_SPRING_RET, 2 * c, "SPRING RETURN");
        break;
    case 5:
        smc_step(P_CH(f, CHF_ECHO_SEND), 2 * c, "ECHO SEND");
        break;
    case 6:
        smc_step(P_CH(f, CHF_SPRING_SEND), 2 * c, "SPRING SEND");
        break;
    default: {                                   /* TEMPO, a tenth of a BPM a click */
        char t[12];
        undo_mark(0x30Fu, plat_ms());
        ctl_set(P_TEMPO, ctl_get(P_TEMPO) + c);
        tempo_text(t);
        app_say("TEMPO", t);
        set_dirty = 1;
        dub.dirty_hdr = 1;
        break;
    }
    }
}

static void smc_button(uint32_t n, int down)     /* a button (note n): pressed (down) or let go */
{
    uint32_t k = n & 7u, now = plat_ms();
    if (down)
        undo_mark(0x2200u + ++smc.presses, now); /* (a press: a new undo step, as the panel's) */
    if (n < 0x08u) {                             /* REC: a throw, held */
        uint32_t id = P_CH(k, CHF_THROW);
        if (down) {
            if (ctl_get(P_CH(k, CHF_THROW_AMT)) != 127)
                ctl_set(P_CH(k, CHF_THROW_AMT), 127);
            ctl_set(id, 1);
            app_say("THROW", CH_NAME[k]);
        } else
            ctl_set(id, 0);
        dub.dirty_strips = 1;
        return;
    }
    if (!down)
        return;
    if (n < 0x10u)                               /* SOLO */
        solo_toggle(k, act_beats());
    else if (n < 0x18u) {                        /* MUTE: as a tap of the mute key, on the line */
        uint32_t id = P_CH(k, CHF_MUTE);
        int m = !ctl_get(id);
        ctl_cue(id, m, act_beats());
        say_mute(k, m);
    } else if (n < 0x20u) {                      /* SELECT: the focus */
        dub.focus = (int32_t)k;
        if (dub.sound)
            dub.sound = voice_fields(dub.focus) ? 1u : 0u;
        app_say("FOCUS", CH_NAME[k]);
    } else if (n == 0x5Eu && !ctl_get(P_PLAY)) { /* PLAY */
        ctl_set(P_PLAY, 1);
        app_say("PLAY", "");
    } else if (n == 0x5Du && ctl_get(P_PLAY)) {  /* STOP */
        ctl_set(P_PLAY, 0);
        app_say("STOP", "");
    } else if (n == 0x5Fu)                       /* REC: the song's capture */
        song_rec_toggle();
    dub.dirty_hdr = dub.dirty_strips = 1;
}

static void smc_in(uint32_t pkt)                 /* one packet from the mixer (CIN | status << 8 | d1 << 16 | d2 << 24) */
{
    uint32_t st = (pkt >> 8) & 0xFFu, ch = st & 15u, d1 = (pkt >> 16) & 0x7Fu, d2 = (pkt >> 24) & 0x7Fu;
    if (!smc.on)
        smc_fresh();
    switch (st >> 4) {
    case 0xEu:                                   /* a fader: pitch bend on channels 1..8 (9, the master: not used) */
        if (ch < NCH)
            smc_fader(ch, (d2 << 7 | d1) >> 7);
        break;
    case 0xBu:                                   /* the knobs: CC 16..23 on channel 1, sign-magnitude (bit 6: back) */
        if (!ch && d1 >= 16u && d1 <= 23u && (d2 & 0x3Fu))
            smc_knob(d1 - 16u, d2 & 0x40u ? -(int32_t)(d2 & 0x3Fu) : (int32_t)(d2 & 0x3Fu));
        break;
    case 0x9u:                                   /* buttons: notes on channel 1, velocity 0 a release */
        if (!ch)
            smc_button(d1, d2 != 0u);
        break;
    case 0x8u:
        if (!ch)
            smc_button(d1, 0);
        break;
    default:
        break;
    }
}

static int smc_send(uint32_t a, uint32_t b, uint32_t c)   /* 1: queued; 0: no room (the next frame) */
{
    if (!plat_midi_out_room(TO_USB))
        return 0;
    plat_midi_send(((a >> 4) & 15u) | a << 8 | b << 16 | c << 24, TO_USB);
    return 1;
}

static void smc_tick(void)                       /* every frame (dub_input, as the hands): in, then the LEDs out */
{
    uint32_t i, j, b, w[2], m = 0, s = 0, t = 0;
    int act = plat_usb_host_state() == USBH_CONNECTED && smc_name(plat_usb_host_name());
    if (act != smc_active) {
        smc_active = (uint8_t)act;
        smc.on = 0;
        if (act)
            app_say("SMC-MIXER", "THE DESK");
    }
    if (!act) {
        smc_qr = smc_qw;
        return;
    }
    while (smc_qr != smc_qw) {
        smc_in(smc_q[smc_qr % SMC_Q]);
        smc_qr++;
    }
    if (!smc.on)
        smc_fresh();
    for (i = 0; i < NCH; i++) {
        smc_notice(i);
        m |= (uint32_t)(ctl_get(P_CH(i, CHF_MUTE)) != 0) << i;
        s |= (uint32_t)(ctl_get(P_CH(i, CHF_SOLO)) != 0) << i;
        t |= (uint32_t)(ctl_get(P_CH(i, CHF_THROW)) != 0) << i;
    }
    w[0] = t | s << 8 | m << 16 | 1u << (24u + (uint32_t)dub.focus);
    w[1] = (ctl_get(P_PLAY) ? 2u : 1u) | (ctl_get(P_SONG_REC) ? 4u : 0u);
    for (j = 0; j < 2u; j++) {
        uint32_t dirty = ((w[j] ^ smc.lit[j]) | ~smc.known[j]) & (j ? 7u : 0xFFFFFFFFu);
        for (i = 0; dirty && i < 32u; i++) {
            b = 1u << i;
            if (!(dirty & b))
                continue;
            if (!smc_send(0x90u, j ? 0x5Du + i : i, (w[j] & b) ? 127u : 0u))
                return;
            smc.lit[j] = (smc.lit[j] & ~b) | (w[j] & b);
            smc.known[j] |= b;
            dirty &= ~b;
        }
    }
    for (i = 0; smc.pend && i < NCH; i++)
        if ((smc.pend >> i) & 1u) {
            if (!smc_send(0xE0u + i, smc.lv[i], smc.lv[i]))
                return;
            smc.pend &= (uint8_t)~(1u << i);
        }
}
