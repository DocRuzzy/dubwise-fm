/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* The live envelope view (roadmap 5.10, spec 4.9): with the SOUND view open, the centre draws the focused voice's
 * envelopes from its settings as they are, so a knob turned reshapes them at once: its loudness (filled, a white
 * line) and a second curve (amber): the kick's pitch falling, the rim's index (or the SYNDRUM's drop), the open hat
 * over the closed one, the bass's pluck, the skank's bite, the lead's vibrato, the siren's sweep. Each hit (the
 * engine's count, eng_trig_count) starts a playhead across them, redrawn at ~20 Hz in the centre's rectangle only.
 * Reduced motion: no playhead. The shapes follow voices.c, chordv.c and fxv.c's formulas (not their sound): a guide
 * for the hand, not a scope. Part of the unity build: dub.c includes it. UI only. */

typedef struct {
    float att, dec, sus, hold, rel;              /* ms; sustain 0..1; how long it's drawn held (a tonal voice) */
    float t2;                                    /* the second curve: its 60 dB fall, ms (0: none) */
    float lfo;                                   /* or an LFO's rate, Hz (the lead's vibrato after 300 ms, a siren) */
    float span;                                  /* the drawing's width, ms */
    const char *what;                            /* the second curve's name */
} env_t;

static float ev_lin(uint32_t id, float lo, float hi) { return lo + (hi - lo) * (float)ctl_get(id) * (1.0f / 127.0f); }

static env_t env_of(uint32_t ch)                 /* the focused voice's shapes, from its settings */
{
    env_t e = {1.0f, 100.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 300.0f, ""};
    switch (ch) {
    case CH_KICK:
        e.dec = ctl_get(P_VOICE(ch, VK_MODEL)) == KICK_BOOM ? ev_lin(P_VOICE(ch, VK_DECAY), 300.0f, 1600.0f)
                                                             : ev_lin(P_VOICE(ch, VK_DECAY), 100.0f, 1000.0f);
        e.t2 = ev_lin(P_VOICE(ch, VK_PTIME), 10.0f, 120.0f);
        e.what = "PITCH";
        break;
    case CH_SNARE:
        e.dec = ctl_get(P_VOICE(ch, VR_MODEL)) == RIM_SYNDRUM ? ev_lin(P_VOICE(ch, VR_DECAY), 80.0f, 600.0f)
                                                               : ev_lin(P_VOICE(ch, VR_DECAY), 20.0f, 200.0f);
        e.t2 = ctl_get(P_VOICE(ch, VR_MODEL)) == RIM_SYNDRUM ? 150.0f : 12.0f;
        e.what = ctl_get(P_VOICE(ch, VR_MODEL)) == RIM_SYNDRUM ? "DROP" : "INDEX";
        break;
    case CH_HATS:
        e.dec = ev_lin(P_VOICE(ch, VH_CLOSED), 20.0f, 150.0f);
        e.t2 = ev_lin(P_VOICE(ch, VH_OPEN), 150.0f, 800.0f);
        e.what = "OPEN";
        break;
    case CH_BASS:
        e.att = 2.0f;
        e.dec = ev_lin(P_VOICE(ch, VB_DECAY), 80.0f, 1500.0f);
        e.sus = (float)ctl_get(P_VOICE(ch, VB_SUSTAIN)) * (1.0f / 127.0f);
        e.hold = 400.0f;
        e.rel = 60.0f;
        e.t2 = ev_lin(P_VOICE(ch, VB_IDECAY), 10.0f, 150.0f);
        e.what = "PLUCK";
        break;
    case CH_SKANK:
        e.att = ev_lin(P_VOICE(ch, VS_ATTACK), 0.0f, 30.0f) + 1.0f;
        e.dec = ev_lin(P_VOICE(ch, VS_DECAY), 60.0f, 200.0f);
        e.t2 = 30.0f;
        e.what = "BITE";
        break;
    case CH_ORGAN:
        e.att = 2.0f;
        e.dec = ev_lin(P_VOICE(ch, VO_DECAY), 40.0f, 300.0f);
        e.t2 = 5.0f;
        e.what = "CLICK";
        break;
    case CH_LEAD:
        e.att = ev_lin(P_VOICE(ch, VL_ATTACK), 10.0f, 150.0f);
        e.dec = 1.0e6f;
        e.sus = 1.0f;
        e.hold = 700.0f;
        e.rel = ev_lin(P_VOICE(ch, VL_RELEASE), 30.0f, 600.0f);
        e.lfo = ctl_get(P_VOICE(ch, VL_VIBRATO)) ? 4.5f : 0.0f;
        e.what = "VIBRATO";
        break;
    default:                                     /* the vox: a siren held, its sweep */
        e.att = 5.0f;
        e.dec = 1.0e6f;
        e.sus = 1.0f;
        e.hold = 900.0f;
        e.rel = 200.0f;
        e.lfo = ev_lin(P_VOICE(ch, VX_RATE), 0.5f, 12.0f);
        e.what = "SWEEP";
        break;
    }
    e.span = e.att + (e.hold > 0.0f ? e.hold + e.rel : e.dec) * 1.05f;   /* (a drum: to its 60 dB) */
    if (e.t2 > e.span && e.t2 < 2000.0f)
        e.span = e.t2 * 1.05f;
    if (e.span < 60.0f)
        e.span = 60.0f;
    return e;
}

static float env_amp(const env_t *e, float t)    /* the loudness at t ms: 0..1 */
{
    float k;
    if (t < e->att)
        return t / e->att;
    t -= e->att;
    if (e->hold > 0.0f && t > e->hold) {         /* let go: from where it was, released */
        float at = e->sus + (1.0f - e->sus) * fm_exp2f(-9.97f * e->hold / e->dec);
        return at * fm_exp2f(-9.97f * (t - e->hold) / e->rel);
    }
    k = fm_exp2f(-9.97f * t / e->dec);           /* (60 dB: 2^-9.97) */
    return e->sus + (1.0f - e->sus) * k;
}

static float env_two(const env_t *e, float t)    /* the second curve at t ms: 0..1 */
{
    if (e->lfo > 0.0f) {                         /* an LFO: the lead's after 300 ms, a siren's at once */
        float on = e->what[0] == 'V' ? (t < 300.0f ? 0.0f : (t - 300.0f) < 200.0f ? (t - 300.0f) / 200.0f : 1.0f) : 1.0f;
        return 0.5f + 0.45f * on * fm_sinf(6.2831853f * e->lfo * t * 0.001f);
    }
    if (e->t2 <= 0.0f)
        return -1.0f;
    return fm_exp2f(-9.97f * t / e->t2);
}

static struct {
    uint32_t seen, hit_ms, drawn_ms;             /* the focused channel's hit count seen, the last hit, the last frame */
    int32_t ch;
} envv = {0, 0, 0, -1};

static int env_view(void) { return dub.sound && dub.page == PAGE_DUB && voice_fields(dub.focus) && !dice_bank(); }

static void env_tick(uint32_t now)               /* every frame: a hit starts the playhead; it moves at ~20 Hz */
{
    uint32_t n;
    if (!env_view())
        return;
    n = eng_trig_count((uint32_t)dub.focus);
    if (envv.ch != dub.focus) {                    /* (a new focus: its hits from now on) */
        envv.ch = dub.focus;
        envv.seen = n;
        envv.hit_ms = 0;
        dub.dirty_centre = 1;
    }
    if (n != envv.seen) {
        envv.seen = n;
        envv.hit_ms = now ? now : 1u;
        dub.dirty_centre = 1;
    }
    if (envv.hit_ms) {
        env_t e = env_of((uint32_t)dub.focus);
        if (ctl_get(P_REDUCED) || (float)(now - envv.hit_ms) > e.span + 50.0f) {
            envv.hit_ms = 0;                     /* (past the end, or reduced motion: still, until the next hit) */
            dub.dirty_centre = 1;
        } else if (now - envv.drawn_ms >= 50u)   /* (~20 Hz, whatever else redraws the centre) */
            dub.dirty_centre = 1;
    }
}

static void env_draw(void)                       /* (draw_centre) the shapes, the playhead */
{
    env_t e = env_of((uint32_t)dub.focus);
    int32_t x, x0 = 8, w = 224, y0 = 18, h = 50, lx, ly = -1;
    char t[16], n[8];
    uint32_t now = plat_ms();
    envv.drawn_ms = now;
    t[0] = 0;
    text_cat(t, CH_NAME[dub.focus]);
    cv_text(8, 4, &FONT_XS, t, C_GRAY);
    lx = cv_text(80, 4, &FONT_XS, "LEVEL", C_WHITE);
    cv_text(lx + 10, 4, &FONT_XS, e.what, C_AMB);
    int_text((int32_t)(e.span + 0.5f), n);
    t[0] = 0;
    text_cat(t, n);
    text_cat(t, " MS");
    cv_text(232 - text_w(&FONT_XS, t), 4, &FONT_XS, t, C_DIM);
    cv_rect(x0, y0 + h, w, 1, C_LINE);
    for (x = 0; x < w; x++) {                    /* the loudness, filled; the second curve, a line */
        float tm = e.span * (float)x / (float)w, a = env_amp(&e, tm), b = env_two(&e, tm);
        int32_t ah = (int32_t)(a * (float)h + 0.5f);
        if (ah > 0)
            cv_rect(x0 + x, y0 + h - ah, 1, ah, C_DIM);
        cv_rect(x0 + x, y0 + h - ah, 1, 1, C_WHITE);
        if (b >= 0.0f) {
            int32_t by = y0 + h - (int32_t)(b * (float)h + 0.5f);
            if (ly >= 0 && (by - ly > 1 || ly - by > 1))   /* (joined: no gaps on a steep fall) */
                cv_rect(x0 + x, by < ly ? by : ly, 1, by < ly ? ly - by : by - ly, C_AMB);
            else
                cv_rect(x0 + x, by, 1, 1, C_AMB);
            ly = by;
        }
    }
    if (e.hold > 0.0f) {                         /* a tonal voice: where the key is let go */
        int32_t xr = x0 + (int32_t)((e.att + e.hold) / e.span * (float)w);
        cv_rect(xr, y0, 1, h, C_LINE);
    }
    if (envv.hit_ms && !ctl_get(P_REDUCED)) {      /* the playhead, from the last hit */
        float tm = (float)(now - envv.hit_ms);
        if (tm <= e.span) {
            int32_t xp = x0 + (int32_t)(tm / e.span * (float)w);
            cv_rect(xp, y0 - 2, 2, h + 3, C_WHITE);
        }
    }
}

#ifdef PLAT_HOST
static int env_expect(const char *what, int *got)
{
    if (!strcmp(what, "env_span"))               /* the drawing's width, ms */
        *got = (int)(env_of((uint32_t)dub.focus).span + 0.5f);
    else if (!strcmp(what, "env_playhead"))      /* 1: a playhead running */
        *got = envv.hit_ms != 0u;
    else if (!strcmp(what, "env_view"))
        *got = env_view();
    else
        return 0;
    return 1;
}
#endif
