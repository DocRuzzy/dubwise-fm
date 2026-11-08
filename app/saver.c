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
#define SV_BAND 120                              /* the screen in two bands (the canvas holds 144 rows) */

static struct {
    uint32_t last;                               /* the last touch, ms */
    uint8_t on;
    uint32_t t0, frame_t;
    uint32_t kswallow, bswallow;                 /* keys and buttons that woke it: ignored until let go */
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

/* the picture: rings at phase ph (0..1 of the spacing) around (cx, cy), the words at (wx, wy) */
static void sv_scene(int32_t cx, int32_t cy, float ph, int32_t wx, int32_t wy)
{
    int32_t i, w;
    for (i = 0; i < 9; i++) {                    /* each further ring a repeat down: dimmer */
        float r = 14.0f + ((float)i + ph) * 17.0f;
        uint32_t a = (uint32_t)(255.0f * (1.0f - ((float)i + ph) / 9.0f));
        sv_ring(cx, cy, (int32_t)r, sv_fade(SV_ORANGE, a));
    }
    cv_rect(cx - 5, cy - 5 - sv_oy, 10, 10, SV_ORANGE);   /* the source: a drop */
    w = text_w(&FONT_L, "DUBWISE");
    cv_rect(wx - w / 2 - 6, wy - 2 - sv_oy, w + 12, 50, C_BLACK);
    cv_text(wx - w / 2, wy - sv_oy, &FONT_L, "DUBWISE", C_WHITE);
    w = text_w(&FONT_XS, "SPILLT BEATS");
    cv_text(wx - w / 2, wy + 32 - sv_oy, &FONT_XS, "SPILLT BEATS", SV_ORANGE);
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

void app_splash(void) { sv_paint(120, 92, 0.0f, 120, 170); }

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
