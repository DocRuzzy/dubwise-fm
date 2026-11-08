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
#include "engine/params.c"                  /* (GPL-3.0-only: the parameters and the main loop's side of them) */
#include "engine/control.c"

#define SET_MAGIC 0x44554234u            /* "DUB4": the settings record; another magic loads defaults */
typedef struct {
    uint32_t magic;
    int16_t tempo;                       /* P_TEMPO: BPM x 10 */
    uint8_t quant;                       /* P_QUANT */
    uint8_t rsv;
    uint8_t echo_time, echo_fb, echo_ret, spring_ret;   /* the DUB page's knobs */
} settings_t;
/* the sounds: every voice parameter (P_VOICE_BASE..P_NIDS), in their own store object (OBJ_SOUND) */
#define SOUND_MAGIC 0x534E4431u          /* "SND1" */
typedef struct {
    uint32_t magic;
    int16_t v[P_NIDS - P_VOICE_BASE];
} sound_t;
static sound_t snd;
/* the patterns and their lengths (OBJ_PATTERN) */
#define PATTERN_MAGIC 0x50415431u        /* "PAT1" */
typedef struct {
    uint32_t magic;
    uint8_t len[NCH];
    step_t steps[NCH][NSTEPS];
} pattern_store_t;
static pattern_store_t pst;
static settings_t set;
static uint8_t set_dirty;                /* changed since the last save: SAVE is lit */

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
static void midi_in(void)                /* audio interrupt: the packets that came in */
{
    uint32_t pkt;
    int port;
    while (plat_midi_in(&pkt, &port)) {
        uint32_t st = (pkt >> 8) & 0xFFu, d1 = (pkt >> 16) & 0x7Fu, d2 = pkt >> 24, mch = st & 0x0Fu;
        int on = (st & 0xF0u) == 0x90u && d2, off = (st & 0xF0u) == 0x80u || ((st & 0xF0u) == 0x90u && !d2);
        if (st != 0xF8u && st != 0xFEu)
            midi_count++;
        if (mch == 9u) {                 /* drums ring out: note offs ignored */
            int32_t ch = gm_drum(d1);
            if (on && ch >= 0)
                eng_trig((uint32_t)ch, (int32_t)d1, d2);
        } else if (on)
            eng_trig(CH_BASS, (int32_t)d1, d2);
        else if (off)
            eng_release(CH_BASS, (int32_t)d1);
    }
}

void app_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    midi_in();
    eng_render(lr, n, master_q12);       /* the commands queued, the channels, the mix (engine/engine.c) */
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
}

static void settings_save(void)
{
    set.tempo = (int16_t)ctl_get(P_TEMPO);
    set.quant = (uint8_t)ctl_get(P_QUANT);
    set.echo_time = (uint8_t)ctl_get(P_ECHO_TIME);
    set.echo_fb = (uint8_t)ctl_get(P_ECHO_FB);
    set.echo_ret = (uint8_t)ctl_get(P_ECHO_RET);
    set.spring_ret = (uint8_t)ctl_get(P_SPRING_RET);
    snd.magic = SOUND_MAGIC;
    {
        uint32_t i;
        for (i = 0; i < P_NIDS - P_VOICE_BASE; i++)
            snd.v[i] = (int16_t)ctl_get(P_VOICE_BASE + i);
    }
    pst.magic = PATTERN_MAGIC;
    {
        uint32_t ch, i;
        for (ch = 0; ch < NCH; ch++) {
            pst.len[ch] = (uint8_t)ctl_get(P_CH(ch, CHF_LEN));
            for (i = 0; i < NSTEPS; i++)
                pst.steps[ch][i] = ctl_get_step(ch, i);
        }
    }
    if (plat_store_save(OBJ_SETTINGS, &set, sizeof set) == 0 && plat_store_save(OBJ_SOUND, &snd, sizeof snd) == 0
        && plat_store_save(OBJ_PATTERN, &pst, sizeof pst) == 0) {
        set_dirty = 0;
        app_say("SAVED", "");
    } else {
        app_say("SAVE", "FAILED");
    }
}

void app_init(void)
{
    eng_init();                          /* every parameter at its default, on both sides of the queue */
    ctl_init();
    if (plat_store_load(OBJ_SETTINGS, &set, sizeof set) != (int)sizeof set || set.magic != SET_MAGIC
        || set.tempo != param_clamp(P_TEMPO, set.tempo) || set.quant >= QUANT_N)
        settings_defaults();
    if (plat_store_load(OBJ_SOUND, &snd, sizeof snd) == (int)sizeof snd && snd.magic == SOUND_MAGIC) {
        uint32_t i;
        for (i = 0; i < P_NIDS - P_VOICE_BASE; i++)
            if (param_desc(P_VOICE_BASE + i))
                ctl_set(P_VOICE_BASE + i, snd.v[i]);   /* (clamped) */
    }
    if (plat_store_load(OBJ_PATTERN, &pst, sizeof pst) == (int)sizeof pst && pst.magic == PATTERN_MAGIC) {
        uint32_t ch, i;                  /* (more than the queue holds: the rest goes on later frames) */
        for (ch = 0; ch < NCH; ch++) {
            ctl_set(P_CH(ch, CHF_LEN), pst.len[ch]);
            for (i = 0; i < NSTEPS; i++)
                if (pst.steps[ch][i].lvl < LVL_N)
                    ctl_step(ch, i, pst.steps[ch][i]);
        }
    }
    ctl_set(P_TEMPO, set.tempo);
    ctl_set(P_QUANT, set.quant);
    ctl_set(P_ECHO_TIME, set.echo_time);  /* (clamped to their ranges) */
    ctl_set(P_ECHO_FB, set.echo_fb);
    ctl_set(P_ECHO_RET, set.echo_ret);
    ctl_set(P_SPRING_RET, set.spring_ret);
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

void app_frame(void)
{
    dub_input();
    ctl_flush();                         /* parameters the full queue refused: their latest values */
    dub_speak();
    ann_tick(plat_ms());
    dub_draw();
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
    if (strcmp(cmd, "param") && strcmp(cmd, "flood") && strcmp(cmd, "cue"))
        return 0;
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

int app_expect(const char *what, int *got)
{
    if (!strcmp(what, "note"))
        *got = eng_note();
    else if (!strcmp(what, "dirty"))
        *got = set_dirty;
    else if (!strcmp(what, "midi_in"))
        *got = (int)midi_count;
    else if (!strncmp(what, "eng.", 4) || !strncmp(what, "ui.", 3)) {   /* a parameter, as applied / as asked */
        int id = param_find(what + (what[0] == 'e' ? 4 : 3));
        if (id < 0)
            return 0;
        *got = what[0] == 'e' ? eng_param((uint32_t)id) : ctl_get((uint32_t)id);
    } else if (!strncmp(what, "ch", 2) && what[2] >= '1' && what[2] <= '8' && !strcmp(what + 3, "_db"))
        *got = peak_db(eng_ch_peak((uint32_t)(what[2] - '1')));   /* post-fader, dBFS (-120: silent) */
    else if (!strcmp(what, "echo_db"))
        *got = peak_db(eng_echo_peak());
    else if (!strcmp(what, "echo_ret_db"))   /* the echo's return, after its level */
        *got = peak_db(eng_echo_ret_peak());
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
