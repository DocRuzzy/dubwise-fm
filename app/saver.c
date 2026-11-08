/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* The boot splash (APP_SPLASH) and the screen saver: echo rings in the label's orange, fading as they spread, like a
 * dub's repeats; the DUBWISE word and SPILLT BEATS under it. Drawn, not stored: no image in the flash.
 * The screen saver comes on while stopped, after SETUP's SCREEN SAVER minutes with nothing touched; the rings then
 * ripple out on the tempo's beat and the words drift (a pixel never lit the same for long). Any key, button or knob
 * wakes it, and that touch does nothing else: its press and its release are swallowed. It never comes on while
 * playing (a performer may leave the desk alone for minutes).
 * Part of the unity build: dub.c includes it. */

#define SV_ORANGE RGB(242, 120, 72)
#define SV_TAN RGB(188, 138, 90)                 /* the SpIllt Beats mark's ground, its two blues */
#define SV_BLUE RGB(84, 120, 180)
#define SV_SKY RGB(120, 180, 212)

/* The SpIllt Beats mark: an S and a B sharing strokes (the S's top is the B's top, its centre line the B's back),
 * brushed. Its strokes in a 240 x 205 box (traced from the label's own drawing) */
static const int16_t LOGO[][4] = {
    {60, 12, 204, 14},                           /* the top: the S's and the B's (from the centre line on: no stub) */
    {62, 22, 120, 118},                          /* the S's centre line, the B's back */
    {8, 62, 74, 63},                             /* the S's lower turn: its top stops short of the centre line .. */
    {10, 66, 46, 118}, {46, 118, 118, 118},      /* .. its bottom stops at it (no stub) */
    {74, 74, 54, 100},
    {190, 22, 152, 50}, {152, 62, 212, 62}, {212, 62, 220, 100}, {220, 100, 200, 125},   /* the B's upper bowl */
    {200, 125, 122, 195}, {122, 135, 122, 195}, {122, 137, 160, 138},             /* its lower bowl and foot */
};
#define LOGO_N (sizeof LOGO / sizeof LOGO[0])
#define SV_BAND 120                              /* the screen in two bands (the canvas holds 144 rows) */

static struct {
    uint32_t last;                               /* the last touch, ms */
    uint8_t on;
    uint32_t t0, frame_t;
    uint32_t kswallow, bswallow;                 /* keys and buttons that woke it: ignored until let go */
    uint8_t hour;                                /* an hour untouched: said so */
} sv;
static int32_t sv_oy;                            /* the band being drawn: its first row */

static uint32_t sv_minutes(void)
{
    static const uint8_t M[SAVER_N] = {0, 1, 5, 15};
    int32_t v = ctl_get(P_SAVER);
    return v >= 0 && v < SAVER_N ? M[v] : 0u;
}

static uint16_t sv_fade(uint16_t c, uint32_t a)   /* c at a/256 of itself, over black */
{
    uint32_t r = (c >> 11) & 31u, g = (c >> 5) & 63u, b = c & 31u;
    return (uint16_t)(((r * a >> 8) << 11) | ((g * a >> 8) << 5) | (b * a >> 8));
}

static void sv_ring(int32_t cx, int32_t cy, int32_t r, uint16_t c)   /* a circle, two pixels thick */
{
    int32_t k;
    for (k = 0; k < 2; k++, r++) {
        int32_t x = r, y = 0, err = 1 - r;
        while (x >= y) {                         /* the midpoint circle, its eight octants */
            cv_pset(cx + x, cy + y - sv_oy, c);
            cv_pset(cx - x, cy + y - sv_oy, c);
            cv_pset(cx + x, cy - y - sv_oy, c);
            cv_pset(cx - x, cy - y - sv_oy, c);
            cv_pset(cx + y, cy + x - sv_oy, c);
            cv_pset(cx - y, cy + x - sv_oy, c);
            cv_pset(cx + y, cy - x - sv_oy, c);
            cv_pset(cx - y, cy - x - sv_oy, c);
            y++;
            if (err < 0)
                err += 2 * y + 1;
            else {
                x--;
                err += 2 * (y - x) + 1;
            }
        }
    }
}

/* the mark at (x0, y0), scaled by k/256: a dark brush, then a light one along its upper half */
static void sv_logo(int32_t x0, int32_t y0, int32_t k)
{
    uint32_t i;
    int32_t t = k * 7 / 256 < 2 ? 2 : k * 7 / 256, dx, dy;   /* the brush: ~14 px wide in the drawing, to scale */
    for (i = 0; i < LOGO_N; i++) {
        int32_t ax = x0 + LOGO[i][0] * k / 256, ay = y0 + LOGO[i][1] * k / 256 - sv_oy;
        int32_t bx = x0 + LOGO[i][2] * k / 256, by = y0 + LOGO[i][3] * k / 256 - sv_oy;
        for (dy = -t; dy <= t; dy++)
            for (dx = -t; dx <= t; dx++)
                cv_line(ax + dx, ay + dy, bx + dx, by + dy, SV_BLUE);
        for (dy = -t / 2; dy <= 0; dy++)         /* the light along the upper half, as the brush left it */
            cv_line(ax - 1, ay + dy - 1, bx - 1, by + dy - 1, SV_SKY);
    }
}

/* the picture: rings at phase ph (0..1 of the spacing) around (cx, cy), the words at (wx, wy) */
static void sv_scene(int32_t cx, int32_t cy, float ph, int32_t wx, int32_t wy)
{
    int32_t i, w;
    for (i = 0; i < 9; i++) {                    /* each further ring a repeat down: dimmer */
        float r = 40.0f + ((float)i + ph) * 17.0f;
        uint32_t a = (uint32_t)(255.0f * (1.0f - ((float)i + ph) / 9.0f));
        sv_ring(cx, cy, (int32_t)r, sv_fade(SV_ORANGE, a));
    }
    cv_rect(cx - 34, cy - 30 - sv_oy, 68, 60, C_BLACK);   /* the source: the label's mark */
    sv_logo(cx - 30, cy - 26, 64);
    w = text_w(&FONT_L, "DUBWISE");
    cv_rect(wx - w / 2 - 6, wy - 2 - sv_oy, w + 12, 50, C_BLACK);
    cv_text(wx - w / 2, wy - sv_oy, &FONT_L, "DUBWISE", C_WHITE);
    w = text_w(&FONT_XS, "SPILLT BEATS");
    cv_text(wx - w / 2, wy + 32 - sv_oy, &FONT_XS, "SPILLT BEATS", SV_ORANGE);
    if (sv.hour) {
        w = text_w(&FONT_XS, "SOUND SYSTEM STILL RUNNING");
        cv_rect(120 - w / 2 - 4, 4 - sv_oy, w + 8, 16, C_BLACK);
        cv_text(120 - w / 2, 6 - sv_oy, &FONT_XS, "SOUND SYSTEM STILL RUNNING", C_GRAY);
    }
}

static void sv_paint(int32_t cx, int32_t cy, float ph, int32_t wx, int32_t wy)
{
    for (sv_oy = 0; sv_oy < 240; sv_oy += SV_BAND) {
        cv_begin(240, SV_BAND, C_BLACK);
        sv_scene(cx, cy, ph, wx, wy);
        cv_blit(0, (uint32_t)sv_oy);
    }
    sv_oy = 0;
}

void app_splash(void)                            /* the mark on its ground, the name under it */
{
    int32_t w;
    for (sv_oy = 0; sv_oy < 240; sv_oy += SV_BAND) {
        cv_begin(240, SV_BAND, SV_TAN);
        sv_logo(54, 22, 146);                   /* 136 x 117 */
        w = text_w(&FONT_L, "DUBWISE");
        cv_text(120 - w / 2, 160 - sv_oy, &FONT_L, "DUBWISE", RGB(52, 36, 24));
        w = text_w(&FONT_XS, "SPILLT BEATS");
        cv_text(120 - w / 2, 196 - sv_oy, &FONT_XS, "SPILLT BEATS", SV_BLUE);
        cv_blit(0, (uint32_t)sv_oy);
    }
    sv_oy = 0;
}

/* dub_input: 1 = this frame's input was the saver's (nothing else to do). act: a key or button changed */
static int saver_input(uint32_t *keys, uint32_t *btn, uint32_t now)
{
    uint32_t k;
    int act = *keys != dub.keys || *btn != dub.btn;
    if (sv.on) {
        for (k = 0; k < NE; k++)
            if (plat_enc((int)k))
                act = 1;
        if (act) {                               /* awake: what woke it is swallowed, the page drawn again */
            sv.on = 0;
            sv.last = now;
            sv.kswallow = *keys;
            sv.bswallow = *btn;
            dub_init();
            dub.page_drawn = 0xFFu;
            app_say("SCREEN SAVER", "OFF");
        }
    } else if (act)
        sv.last = now;
    sv.kswallow &= *keys;                        /* let go: no longer swallowed */
    sv.bswallow &= *btn;
    *keys &= ~sv.kswallow;
    *btn &= ~sv.bswallow;
    return sv.on;
}

static void saver_touch(uint32_t now) { sv.last = now; }

/* dub_draw: 1 = the saver drew (the page does not) */
static int saver_draw(uint32_t now)
{
    uint32_t m = sv_minutes();
    if (!sv.on) {
        if (!m || eng_playing() || now - sv.last < m * 60000u)
            return 0;
        sv.on = 1;
        sv.t0 = now;
        sv.frame_t = 0;
        sv.hour = 0;
    }
    if (eng_playing()) {                         /* (MIDI started it) */
        sv.on = 0;
        sv.last = now;
        dub_init();
        dub.page_drawn = 0xFFu;
        return 0;
    }
    if (now - sv.frame_t >= 100u) {              /* 10 frames a second */
        uint32_t t = now - sv.t0, beat = 1200000u / (uint32_t)(ctl_get(P_TEMPO) > 0 ? ctl_get(P_TEMPO) : 750);   /* two beats a ring */
        float ph = (float)(t % beat) / (float)beat;
        uint32_t slot = (t / 20000u) % 4u;       /* the words move every 20 s */
        static const int8_t DX[4] = {0, 22, 0, -22}, DY[4] = {0, -14, 12, -6};
        sv.frame_t = now;
        if (!sv.hour && now - sv.last >= 3600000u) {   /* an hour on its own: the dance goes on */
            sv.hour = 1;
            app_say("SOUND SYSTEM", "STILL RUNNING");
        }
        sv_paint(120, 120, ph, 120 + DX[slot], 170 + DY[slot]);
    }
    return 1;
}

#ifdef PLAT_HOST
static int saver_expect(const char *what, int *got)
{
    if (!strcmp(what, "saver"))
        *got = sv.on;
    else
        return 0;
    return 1;
}
#endif
