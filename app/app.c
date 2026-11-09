/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal */
/* Dubwise FM's side of the contract with the platform (app.h), started from fm1-base's template: the boot, the
 * settings, MIDI in, the audio render (the engine's, engine/), the messages and the screen reader. The pages are
 * their own files (GPL-3.0-only): dub.c, the DUB page. Part of the unity build (device/unity.c, host/plat_host.c),
 * after the platform: gfx.c and announce.c are in scope. */
#include "fastmath.h"
#include "engine/engine.h"
#include "engine/control.h"
#include "engine/echo.h"
#include "engine/scenes.h"
#include "engine/song.h"
#include "engine/fxv.h"
#include "engine/motion.h"
#include "engine/params.c"                  /* (GPL-3.0-only: the parameters and the main loop's side of them) */
#include "engine/control.c"

#define SET_MAGIC 0x44554240u            /* "DUB@": the settings record; another magic loads defaults */
typedef struct {
    uint32_t magic;
    int16_t tempo;                       /* P_TEMPO: BPM x 10 */
    uint8_t quant;                       /* P_QUANT */
    uint8_t rsv;
    uint8_t echo_time, echo_fb, echo_ret, spring_ret;   /* the DUB page's knobs */
    uint8_t setup[P_NGLOBAL];            /* the globals from P_LEAD on: lead, cycle, runway .. (SETUP) */
    uint8_t chords[P_CHORD_END - P_CHORD0];   /* the chord lane */
    uint8_t hpf_on;                      /* where the Big Knob sits */
    uint8_t fx[32];                      /* the FX page's (SET_FX; room to grow) */
} settings_t;
/* the sounds: every voice parameter (P_VOICE_BASE..P_NIDS), in their own store object (OBJ_SOUND) */
#define SOUND_MAGIC 0x534E4431u          /* "SND1" */
typedef struct {
    uint32_t magic;
    int16_t v[P_NIDS - P_VOICE_BASE];
} sound_t;
static sound_t snd;
static const uint16_t SET_FX[] = {P_ECHO_TONE, P_ECHO_HEAD2, P_SPRING_DECAY, P_SPRING_TONE, P_XS_E2S, P_XS_S2E,
                                    P_DIVE_DEPTH, P_DIVE_TIME, P_ECHO_WIDTH, P_PH_ON, P_PH_RATE, P_PH_DEPTH, P_PH_FB,
                                    P_PH_SHAPE, P_DRIVE_ON, P_DRIVE_AMT, P_DRIVE_MIX, P_SGATE_ON, P_SGATE_HOLD,
                                    P_SGATE_REL, P_ECHO_PP, P_PH_MODE, P_PH_RATE2};
#define NSET_FX (sizeof SET_FX / sizeof SET_FX[0])
typedef char set_fx_fits[NSET_FX <= 32u ? 1 : -1];
static uint16_t scenes_dirty;            /* (arrange.c sets the bits; saved and cleared by SAVE) */
static uint8_t set_dirty;                /* changed since the last save: SAVE is lit */
#include "patstore.c"                    /* the patterns, scenes and song in the store (GPL-3.0-only) */

static settings_t set;
static uint8_t save_hold;                  /* a backup streaming over USB: no saves meanwhile */

/* ------------------------------------------------------------------- MIDI --- */
/* MIDI in plays the voices: channel 10 the drums by General MIDI's notes, any other the bass */
static volatile uint32_t midi_count;     /* messages in (clock and active sensing left out) */
static int32_t gm_drum(uint32_t note)    /* GM drum note -> our channel, -1 none */
{
    switch (note) {
    case 35: case 36: return CH_KICK;
    case 37: case 38: case 39: case 40: return CH_SNARE;
    case 42: case 44: case 46: return CH_HATS;
    default: return -1;
    }
}
static void ml_cc_in(uint32_t ch, uint32_t cc, uint32_t v);
static void midi_in(void)                /* audio interrupt: the packets that came in */
{
    uint32_t pkt;
    int port;
    while (plat_midi_in(&pkt, &port)) {
        uint32_t st = (pkt >> 8) & 0xFFu, d1 = (pkt >> 16) & 0x7Fu, d2 = pkt >> 24, mch = st & 0x0Fu;
        int on = (st & 0xF0u) == 0x90u && d2, off = (st & 0xF0u) == 0x80u || ((st & 0xF0u) == 0x90u && !d2);
        if (st != 0xF8u && st != 0xFEu)
            midi_count++;
        if (st >= 0xF8u) {                   /* real time: the clock, Start, Continue, Stop (SETUP: MIDI CLOCK IN) */
#ifdef PLAT_HOST
            eng_clock_in(st, eng_clock_now());   /* (the host: the script's time, not the computer's) */
#else
            eng_clock_in(st, plat_midi_in_t24());
#endif
            continue;
        }
        if ((st & 0xF0u) == 0xB0u && d1 < 120u) {   /* a control change: learned (midilearn.c), on the main loop */
            ml_cc_in(mch, d1, d2 & 0x7Fu);
            continue;
        }
        if (mch == 9u) {                 /* drums ring out: note offs ignored */
            int32_t ch = gm_drum(d1);
            if (on && ch >= 0)
                eng_trig((uint32_t)ch, (int32_t)d1, d2);
        } else if (on && eng_midi_allowed((int32_t)d1))   /* the key lock, if it covers MIDI in */
            eng_trig(mch == 1u ? CH_LEAD : CH_BASS, (int32_t)d1, d2);   /* channel 2 the lead, the rest the bass */
        else if (off)
            eng_release(mch == 1u ? CH_LEAD : CH_BASS, (int32_t)d1);
    }
}

/* The live input (P2.8): the computer's audio over USB, on channel 8. It needs fm1-base's proposed
 * usb_audio_out_app option (docs/upstream/usb-audio-to-app.md): the computer's audio handed to the render, resampled to
 * its clock, and not mixed in after it. Without it, the platform mixes the computer's audio in after the render (not
 * mutable, not throwable) and channel 8 has no live input. The host simulator: a test tone (`livein HZ DB`). */
#ifdef PLAT_HOST
static float host_live_hz, host_live_amp, host_live_ph;
static int32_t host_live[2u * ENG_BLOCK];
#endif

static void live_in(uint32_t n)
{
#if defined(PLAT_HOST)
    uint32_t i;
    if (host_live_hz <= 0.0f) {
        eng_live_in(0, 0, 0.0f);
        return;
    }
    for (i = 0; i < n && i < ENG_BLOCK; i++) {
        int32_t v = (int32_t)(host_live_amp * 8388607.0f * fm_sin_turns(host_live_ph));
        host_live[2u * i] = host_live[2u * i + 1u] = v;
        host_live_ph += host_live_hz * (1.0f / 44100.0f);
        if (host_live_ph >= 1.0f)
            host_live_ph -= 1.0f;
    }
    eng_live_in(host_live, i, 8388608.0f);
#elif FM1B_UAC_OUT_APP
    uint32_t frames = 0;
    const int32_t *p = plat_usb_play_in(&frames);   /* proposed upstream: full scale 1 << 23 */
    eng_live_in(p, frames < n ? frames : n, 8388608.0f);
#else
    (void)n;
    eng_live_in(0, 0, 0.0f);
#endif
}

void app_tick(void)                     /* 1 kHz: the MIDI clock's messages, each at its moment */
{
    uint32_t st;
    while (eng_clock_pop(eng_clock_now(), &st))
        plat_midi_send(0x0Fu | st << 8, TO_USB);
}

static float stem_pk[8];                 /* (tests: the stems' peaks, 16-bit full scale) */
void app_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    uint32_t run = 0;
    int16_t *u = PLAT_USB_CH >= 8 ? plat_usb_ch(0, &run) : 0;   /* (a computer recording the channels 3-10) */
    const float *st;
    midi_in();
    live_in(n);
    st = eng_stems(u != 0);
    eng_render(lr, n, master_q12);       /* the commands queued, the channels, the mix (engine/engine.c) */
    if (st) {                            /* the stems into the USB recording (QoL 4), 16-bit, where the ring has them */
        uint32_t f, k;
        for (f = 0; f < n; f++) {
            if (!run) {
                u = plat_usb_ch(f, &run);
                if (!u)
                    break;
            }
            for (k = 0; k < 8u; k++) {
                float x = st[f * 8u + k] * 32767.0f;
                int32_t v = (int32_t)(x > 32767.0f ? 32767.0f : x < -32767.0f ? -32767.0f : x);
                if (fm_fabsf(x) > stem_pk[k])
                    stem_pk[k] = fm_fabsf(x);
                u[k] = (int16_t)(u[k] + v);
            }
            u += PLAT_USB_CH_STRIDE;
            run--;
        }
    }
}

void app_panic(void) { eng_panic(); }

/* --------------------------------------------------------------- settings --- */
static void settings_defaults(void)
{
    set.magic = SET_MAGIC;
    set.tempo = (int16_t)param_def(P_TEMPO);
    set.quant = (uint8_t)param_def(P_QUANT);
    set.echo_time = (uint8_t)param_def(P_ECHO_TIME);
    set.echo_fb = (uint8_t)param_def(P_ECHO_FB);
    set.echo_ret = (uint8_t)param_def(P_ECHO_RET);
    set.spring_ret = (uint8_t)param_def(P_SPRING_RET);
    set.hpf_on = (uint8_t)param_def(P_HPF_ON);
    {
        uint32_t i;
        for (i = 0; i < NSET_FX; i++)
            set.fx[i] = (uint8_t)param_def(SET_FX[i]);
    }
    {
        uint32_t i;
        for (i = P_LEAD; i < P_NGLOBAL; i++)
            set.setup[i] = (uint8_t)param_def(i);
        for (i = P_CHORD0; i < P_CHORD_END; i++)
            set.chords[i - P_CHORD0] = (uint8_t)param_def(i);
    }
}

static int save_project(int quiet);
static void settings_save(void) { save_project(0); }
static int ml_save(void);
static void ml_load(void);

static int save_project(int quiet)       /* SAVE (quiet: the autosave, which says nothing): 1 saved */
{
    if (save_hold) {                       /* (a backup streaming: what it reads must not move) */
        if (!quiet)
            app_say("SAVE", "BACKUP RUNNING");
        return 0;
    }
    set.tempo = (int16_t)ctl_get(P_TEMPO);
    set.quant = (uint8_t)ctl_get(P_QUANT);
    set.echo_time = (uint8_t)ctl_get(P_ECHO_TIME);
    set.echo_fb = (uint8_t)ctl_get(P_ECHO_FB);
    set.echo_ret = (uint8_t)ctl_get(P_ECHO_RET);
    set.spring_ret = (uint8_t)ctl_get(P_SPRING_RET);
    set.hpf_on = (uint8_t)ctl_get(P_HPF_ON);
    {
        uint32_t i;
        for (i = 0; i < NSET_FX; i++)
            set.fx[i] = (uint8_t)ctl_get(SET_FX[i]);
    }
    {
        uint32_t i;
        for (i = P_LEAD; i < P_NGLOBAL; i++)
            set.setup[i] = (uint8_t)ctl_get(i);
        for (i = P_CHORD0; i < P_CHORD_END; i++)
            set.chords[i - P_CHORD0] = (uint8_t)ctl_get(i);
    }
    snd.magic = SOUND_MAGIC;
    {
        uint32_t i;
        for (i = 0; i < P_NIDS - P_VOICE_BASE; i++)
            snd.v[i] = (int16_t)ctl_get(P_VOICE_BASE + i);
    }
    if (plat_store_save(OBJ_SETTINGS, &set, sizeof set) == 0 && plat_store_save(OBJ_SOUND, &snd, sizeof snd) == 0
        && proj_save() && ml_save()
        && ((eng_playing() && ctl_get(P_SONG_REC)) || mot_save())) {   /* (motion: not while it is captured) */
        set_dirty = 0;
        if (!quiet)
            app_say("SAVED", "");
        return 1;
    }
    if (!quiet)
        app_say("SAVE", "FAILED");
    return 0;
}

static void load_saved(void);

static int slot_sysex(const uint8_t *p, uint32_t n);
int app_sysex(const uint8_t *p, uint32_t n) { return slot_sysex(p, n); }   /* (slots.c) */

void app_init(void)
{
    eng_init();                          /* every parameter at its default, on both sides of the queue */
    ctl_init();
    mot_load();                          /* the song's motion (P2.7): the flash's data area, not a store object */
    load_saved();
}

/* Backup and restore over USB (fm1.toml backup = true; tools/fm1_backup.py): the saved project and settings (the store
 * objects in APP_BACKUP_OBJS). The motion and the song slots live in the flash's data area, outside the store: a slot
 * goes to and from the computer with tools/dubwise_slots.py */
int app_backup_playing(void) { return eng_playing(); }
void app_backup_flush(void)
{
    if (set_dirty || scenes_dirty)
        settings_save();
}
void app_backup_hold(int on) { save_hold = (uint8_t)(on != 0); }
static void undo_reset(void);
void app_backup_restored(void)           /* the restored objects, read as at boot */
{
    load_saved();
    undo_reset();
    set_dirty = 0;
}

static void load_saved(void)             /* the store's objects into both sides of the queue (boot, a restore) */
{
    if (plat_store_load(OBJ_SETTINGS, &set, sizeof set) != (int)sizeof set || set.magic != SET_MAGIC
        || set.tempo != param_clamp(P_TEMPO, set.tempo) || set.quant >= QUANT_N)
        settings_defaults();
    if (plat_store_load(OBJ_SOUND, &snd, sizeof snd) == (int)sizeof snd && snd.magic == SOUND_MAGIC) {
        uint32_t i;
        for (i = 0; i < P_NIDS - P_VOICE_BASE; i++)
            if (param_desc(P_VOICE_BASE + i))
                ctl_set(P_VOICE_BASE + i, snd.v[i]);   /* (clamped) */
    }
    proj_load();
    ml_load();
    ctl_set(P_TEMPO, set.tempo);
    ctl_set(P_QUANT, set.quant);
    ctl_set(P_ECHO_TIME, set.echo_time);  /* (clamped to their ranges) */
    ctl_set(P_ECHO_FB, set.echo_fb);
    ctl_set(P_ECHO_RET, set.echo_ret);
    ctl_set(P_SPRING_RET, set.spring_ret);
    ctl_set(P_HPF_ON, set.hpf_on);
    {
        uint32_t i;
        for (i = 0; i < NSET_FX; i++)
            ctl_set(SET_FX[i], set.fx[i]);
    }
    {
        uint32_t i;
        for (i = P_LEAD; i < P_NGLOBAL; i++)
            ctl_set(i, set.setup[i]);
        for (i = P_CHORD0; i < P_CHORD_END; i++)
            ctl_set(i, set.chords[i - P_CHORD0]);
    }
}

/* ---------------------------------------------------------------- helpers --- */
static void text_cat(char *d, const char *s)
{
    while (*d)
        d++;
    while (*s)
        *d++ = *s++;
    *d = 0;
}

static void int_text(int32_t v, char *b)     /* signed, no padding */
{
    char t[12];
    int n = 0;
    uint32_t u = (uint32_t)(v < 0 ? -v : v);
    do
        t[n++] = (char)('0' + u % 10u);
    while ((u /= 10u) && n < 11);
    if (v < 0)
        *b++ = '-';
    while (n)
        *b++ = t[--n];
    *b = 0;
}

/* ---------------------------------------------------------------- message --- */
static struct {
    char text[2][24];                    /* a short message over the header */
    uint32_t until;
    uint8_t changed;
} msg;

void app_say(const char *a, const char *b)
{
    uint32_t i;
    char t[48];
    for (i = 0; i < 23u && a && a[i]; i++)
        msg.text[0][i] = a[i];
    msg.text[0][i] = 0;
    for (i = 0; i < 23u && b && b[i]; i++)
        msg.text[1][i] = b[i];
    msg.text[1][i] = 0;
    msg.until = plat_ms() + 1300u;
    msg.changed = 1;
    if (ann_on()) {                      /* and to a screen reader (announce.c, fm1-reader) */
        t[0] = 0;
        text_cat(t, msg.text[0]);
        if (msg.text[1][0]) {
            text_cat(t, " ");
            text_cat(t, msg.text[1]);
        }
        ann_say(t, 0, ANN_NOW);
    }
}

#include "dub.c"                         /* the DUB page (GPL-3.0-only) */

void app_ui_init(void)
{
    touch_on(1);                         /* velocity and pressure: the keys' readings on */
    lcd_fill(0, 0, 240, 240, C_BLACK);
    dub_init();
}

void app_input_only(void) { dub_input(); }

void app_redraw(void)                    /* (APP_REDRAW: the screen mirror lost a part) the whole screen again */
{
    dub_init();
    dub.page_drawn = 0xFFu;
    help.drawn = 0;
}

/* AUTOSAVE (SETUP, on by default; the 2026-11 QoL review): what SAVE keeps, saved by itself once nothing has been
 * touched for 4 s, stopped, the sound died away (a flash write stops the sound for a moment) and nothing being
 * loaded. Quiet: SAVE's light going out says it. A failed one waits 30 s before the next try */
static uint32_t as_fail_t;
static void autosave_tick(void)
{
    uint32_t now = plat_ms();
    if (!ctl_get(P_AUTOSAVE) || save_hold || eng_playing() || eng_spinning() || demo_moves_due || slot_moves)
        return;
    if (!(set_dirty || scenes_dirty || mot_dirty) || dub.btn || dub.keys || now - sv.last < 4000u
        || eng_out_peak() > 0.001f)
        return;
    if (as_fail_t && now - as_fail_t < 30000u)
        return;
    as_fail_t = save_project(1) ? 0u : (now ? now : 1u);
}

void app_frame(void)
{
    dub_input();
    ml_tick();                           /* a controller's CCs, learned or applied (midilearn.c) */
    usbh_tick();                         /* the USB port: a computer or a keyboard */
    ctl_flush();                         /* parameters the full queue refused: their latest values */
    ctl_follow();                        /* (and the song's motion played back) */
    dub_speak();
    ann_tick(plat_ms());
    dub_draw();
    autosave_tick();
}

#ifdef PLAT_HOST
static int host_fails;
static int peak_db(float pk) { return pk > 1e-6f ? (int)floorf(20.0f * log10f(pk) + 0.5f) : -120; }

/* the simulator's commands (app.h APP_HOST_CMD), through the same queue as the UI:
 *   param NAME VALUE      set a parameter (ch1.mute, v4.wave, tempo, dev.tones ...: engine/params.c)
 *   flood NAME FROM TO    set it to FROM, FROM+1 .. TO, all at once (more than the queue holds: overflow)
 *   cue NAME VALUE BEATS  set it at the next multiple of BEATS beats (launch quantise)
 *   perf [reset]          print the engine's per-part profile (P0.7): host us a block, and the device's share of a
 *                         block by the x80 rule; "reset" restarts the maxima */
static int pmeta_name(const char *a, int *id);
int app_host_cmd(const char *cmd, const char *a, const char *b, const char *c, const char *d)
{
    int id;
    (void)d;
    if (!strcmp(cmd, "perf")) {
        uint32_t i, tot = 0, totmax = 0;
        if (!strcmp(a, "reset")) {
            eng_prof_reset();
            return 1;
        }
        printf("perf: part      avg us   max us   device %% (x80, avg)\n");
        for (i = 0; i < NPARTS; i++) {
            eng_prof_t p = eng_prof(i);
            tot += p.avg;
            totmax += p.max;
            printf("perf: %-8s %7.2f  %7.2f   %5.2f\n", eng_part_name(i), p.avg / 384.0, p.max / 24.0,
                   p.avg / 16.0 * 80.0 * 100.0 / ENG_BLOCK_TICKS);
        }
        printf("perf: total    %7.2f  %7.2f   %5.2f\n", tot / 384.0, totmax / 24.0,
               tot / 16.0 * 80.0 * 100.0 / ENG_BLOCK_TICKS);
        return 1;
    }
    if (!strcmp(cmd, "idle")) {          /* idle MS: as if nothing had been touched for MS longer (the saver's hour) */
        sv.last -= (uint32_t)atoi(a);
        return 1;
    }
    if (!strcmp(cmd, "splash")) {        /* the boot splash, drawn as the device draws it (the host shows none) */
        app_splash();
        return 1;
    }
    if (!strcmp(cmd, "livein")) {        /* livein HZ DB: the live input's stand-in, a sine (HZ 0: none) */
        host_live_hz = (float)atof(a);
        host_live_amp = fm_exp2f((float)atof(b) * (1.0f / 6.0206f));
        return 1;
    }
    if (!strcmp(cmd, "step")) {          /* step CH PAT I LVL [NOTE [OFF [GATE]]]: CH 1-8, PAT A-D, I from 1 */
        char pc[8] = {0};
        int ch = 1, i = 1, v[4] = {0, -1, 0, 0}, n;
        step_t st;
        n = sscanf(plat_host_line, "%*s %d %7s %d %d %d %d %d", &ch, pc, &i, &v[0], &v[1], &v[2], &v[3]);
        if (n < 4)
            return 1;
        ch = (ch - 1) & 7;
        st = ctl_get_step_at((uint32_t)ch, (uint32_t)(pc[0] - 'A') % NPAT, (uint32_t)(i - 1));
        st.lvl = (uint8_t)v[0];
        if (n >= 5)
            st.note = (int8_t)v[1];
        st.off = (int8_t)(n >= 6 ? v[2] : 0);
        st.gate = (uint8_t)(n >= 7 ? v[3] : 0);
        ctl_step_at((uint32_t)ch, (uint32_t)(pc[0] - 'A') % NPAT, (uint32_t)(i - 1), st);
        return 1;
    }
    if (!strcmp(cmd, "oldsave")) {       /* a save from before the session (patstore.c), for the next reboot */
        pst_oldsave();
        return 1;
    }
    if (!strcmp(cmd, "clearpat")) {      /* clearpat CH PAT: every step off */
        uint32_t ch = (uint32_t)(atoi(a) - 1) % NCH, pt = (uint32_t)(b[0] - 'A') % NPAT, i;
        for (i = 0; i < NSTEPS; i++) {
            step_t st = {LVL_OFF, default_note(ch), 0, 0};
            ctl_step_at(ch, pt, i, st);
        }
        return 1;
    }
    if (strcmp(cmd, "param") && strcmp(cmd, "flood") && strcmp(cmd, "cue"))
        return 0;
    if (!strcmp(cmd, "param") && pmeta_name(a, &id) >= 0) {   /* chN.len / grid / swing: the pattern playing's */
        uint32_t ch = (uint32_t)id / 3u;
        pmeta_t m = ctl_meta_now(ch);
        if (id % 3 == 0)
            m.len = (uint8_t)atoi(b);
        else if (id % 3 == 1)
            m.grid = (uint8_t)atoi(b);
        else
            m.swing = (uint8_t)atoi(b);
        ctl_meta_set(ch, (uint32_t)ctl_get(P_CH(ch, CHF_PAT)), m);
        return 1;
    }
    if ((id = param_find(a)) < 0) {
        printf("FAIL: no parameter %s\n", a);
        host_fails++;
        return 1;
    }
    if (!strcmp(cmd, "param"))
        ctl_set((uint32_t)id, atoi(b));
    else if (!strcmp(cmd, "cue"))
        ctl_cue((uint32_t)id, atoi(b), (uint32_t)atoi(c));
    else {
        int v, from = atoi(b), to = atoi(c);
        for (v = from; v <= to; v++)
            ctl_set((uint32_t)id, v);
    }
    return 1;
}
int app_host_fails(void) { return host_fails; }

static int pmeta_name(const char *a, int *id)    /* "chN.len" / ".grid" / ".swing": (N-1) x 3 + field, else -1 */
{
    int f;
    if (strncmp(a, "ch", 2) || a[2] < '1' || a[2] > '8' || a[3] != '.')
        return -1;
    f = !strcmp(a + 4, "len") ? 0 : !strcmp(a + 4, "grid") ? 1 : !strcmp(a + 4, "swing") ? 2 : -1;
    if (f < 0)
        return -1;
    *id = (a[2] - '1') * 3 + f;
    return *id;
}

int app_expect(const char *what, int *got)
{
    if (pst_expect(what, got))
        return 1;
    if (!strcmp(what, "note"))
        *got = eng_note();
    else if (!strcmp(what, "dirty"))
        *got = set_dirty;
    else if (!strcmp(what, "midi_in"))
        *got = (int)midi_count;
    else if ((!strncmp(what, "eng.", 4) || !strncmp(what, "ui.", 3))
             && pmeta_name(what + (what[0] == 'e' ? 4 : 3), got) >= 0) {   /* a pattern's setting, playing / asked */
        uint32_t ch = (uint32_t)*got / 3u, f = (uint32_t)*got % 3u;
        pmeta_t m = what[0] == 'e' ? eng_pmeta(ch, eng_pat(ch)) : ctl_meta_now(ch);
        *got = f == 0u ? m.len : f == 1u ? m.grid : m.swing;
    } else if (!strncmp(what, "eng.", 4) || !strncmp(what, "ui.", 3)) {   /* a parameter, as applied / as asked */
        int id = param_find(what + (what[0] == 'e' ? 4 : 3));
        if (id < 0)
            return 0;
        *got = what[0] == 'e' ? eng_param((uint32_t)id) : ctl_get((uint32_t)id);
    } else if (!strncmp(what, "ch", 2) && what[2] >= '1' && what[2] <= '8' && !strcmp(what + 3, "_db"))
        *got = peak_db(eng_ch_peak((uint32_t)(what[2] - '1')));   /* post-fader, dBFS (-120: silent) */
    else if (!strncmp(what, "stem", 4) && what[4] >= '1' && what[4] <= '8' && !strcmp(what + 5, "_db")) {
        float pk = stem_pk[what[4] - '1'] / 32767.0f;   /* stemN_db: USB channel N + 2's peak since the last look */
        stem_pk[what[4] - '1'] = 0.0f;
        *got = peak_db(pk);
    } else if (!strcmp(what, "out_db"))        /* the mix, before MASTER */
        *got = peak_db(eng_out_peak());
    else if (!strcmp(what, "echo_db"))
        *got = peak_db(eng_echo_peak());
    else if (!strcmp(what, "echo_ret_db"))   /* the echo's return, after its level */
        *got = peak_db(eng_echo_ret_peak());
    else if (!strcmp(what, "spring_ret_db"))   /* the spring's return, after its level */
        *got = peak_db(eng_spring_ret_peak());
    else if (!strcmp(what, "clk_out"))       /* MIDI clocks sent (SETUP: MIDI CLOCK OUT) */
        *got = (int)eng_clocks_sent();
    else if (!strcmp(what, "crashes"))
        *got = (int)eng_crashes();
    else if (!strcmp(what, "echo_t"))        /* the echo's time now, line samples (11,025 a second) */
        *got = (int)(eng_echo_t() + 0.5f);
    else if (!strcmp(what, "echo_div"))
        *got = eng_echo_div();
    else if (!strcmp(what, "echo_gr"))       /* the return limiter's gain, % */
        *got = (int)(eng_echo_gr() * 100.0f + 0.5f);
    else if (!strcmp(what, "echo_g"))        /* the loop gain, x 1000 */
        *got = (int)(eng_echo_g() * 1000.0f + 0.5f);
    else if (!strcmp(what, "spring_db"))
        *got = peak_db(eng_spring_peak());
    else if (!strcmp(what, "q_sent"))        /* the command queue (engine/control.c) */
        *got = (int)ctl_stats.sent;
    else if (!strcmp(what, "q_refused"))
        *got = (int)ctl_stats.refused;
    else if (!strcmp(what, "q_resent"))
        *got = (int)ctl_stats.resent;
    else if (!strcmp(what, "q_pending"))
        *got = (int)ctl_stats.pending;
    else if (!strcmp(what, "q_applied"))
        *got = (int)eng_cmds();
    else if (!strcmp(what, "t_fire"))        /* the transport: the sample the last cue landed on (since the start) */
        *got = (int)eng_fire_at();
    else if (!strcmp(what, "t_fires"))
        *got = (int)eng_fires();
    else if (!strcmp(what, "t_cues"))
        *got = (int)eng_cues();
    else if (!strcmp(what, "t_beat"))
        *got = (int)eng_beat();
    else if (!strcmp(what, "t_playing"))
        *got = eng_playing();
    else if (!strncmp(what, "hit", 3) && what[3] >= '1' && what[3] <= '8' && !what[4])
        *got = (int)eng_seq_hit_at((uint32_t)(what[3] - '1'));   /* hitN: channel N's last hit, samples since PLAY */
    else if (!strcmp(what, "tier"))          /* the guard tier: 0 Studio, 1 Four-track, 2 Seven-inch */
        *got = eng_tier();
    else if (!strcmp(what, "load"))          /* the load, % of the block (the host's ticks) */
        *got = (int)eng_load_pct();
    else if (!strcmp(what, "ticks"))         /* the runway's audible ticks */
        *got = (int)eng_ticks();
    else if (!strncmp(what, "chord_ch", 8))  /* chord_chN: the chord channel N last played */
        *got = (int)eng_chord_last((uint32_t)(atoi(what + 8) - 1) % NCH);
    else if (!strcmp(what, "seq_hits"))
        *got = (int)eng_seq_hits();
    else if (!strcmp(what, "perf_pct")) {   /* the engine's average cost, as a % of the device's block (x80 rule) */
        uint32_t i, tot = 0;
        for (i = 0; i < NPARTS; i++)
            tot += eng_prof(i).avg;
        *got = (int)(tot / 16.0 * 80.0 * 100.0 / ENG_BLOCK_TICKS + 0.5);
    }
    else
        return dub_expect(what, got);        /* the page's own (dub.c) */
    return 1;
}
#endif
