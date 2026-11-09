/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* The boot splash (APP_SPLASH) and the screen saver, with the SpIllt Beats mark: the owner's own brush drawing, kept
 * as three blues (logo_data.h, made by tools/logo/mklogo.py: 2.6 KB for its two sizes).
 * - The splash: a two-colour screen-print flyer. Cream paper with a grain, the mark in deep blue over an orange
 *   print of itself set a few pixels off (misregistered), DUBWISE and SPILLT BEATS under it.
 * - The saver: a dubplate. The mark on the record's tan label, orange rings running out across the black on the
 *   tempo's beat and fading as they spread (a dub's repeats), a spliff resting on the plate, its smoke curling up.
 * The saver comes on while stopped, after SETUP's SCREEN SAVER minutes with nothing touched. Any key, button or knob
 * wakes it, and that touch does nothing else: its press and its release are swallowed. It never comes on while
 * playing (a performer may leave the desk alone for minutes).
 * Part of the unity build: dub.c includes it. */

#include "logo_data.h"

#define SV_ORANGE RGB(242, 120, 72)
#define SV_TAN RGB(188, 138, 90)                 /* the record's label */
#define SV_PAPER RGB(236, 226, 204)              /* the flyer */
#define SV_INK RGB(28, 28, 32)
#define SV_BAND 120                              /* the screen in two bands (the canvas holds 144 rows) */
#define SV_CX 120                                /* the record's centre */
#define SV_CY 112

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

static void sv_mix(int32_t x, int32_t y, uint16_t c, uint32_t a)   /* c over the canvas's pixel at a/256 */
{
    uint32_t i, d, r, g, b;
    y += cv_oy;
    if ((uint32_t)x >= cv_w || y < cv_y0 || y >= cv_y1)
        return;
    i = (uint32_t)y * cv_w + (uint32_t)x;
    d = swap16(cv_px[i]);
    r = (((d >> 11) & 31u) * (256u - a) + ((c >> 11) & 31u) * a) >> 8;
    g = (((d >> 5) & 63u) * (256u - a) + ((c >> 5) & 63u) * a) >> 8;
    b = ((d & 31u) * (256u - a) + (c & 31u) * a) >> 8;
    cv_px[i] = swap16((r << 11) | (g << 5) | b);
}

static uint32_t sv_hash(uint32_t x, uint32_t y)
{
    uint32_t h = x * 374761393u + y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

static void sv_ring(int32_t cx, int32_t cy, int32_t r, uint16_t c)   /* a circle, one pixel */
{
    int32_t x = r, y = 0, err = 1 - r;
    while (x >= y) {                             /* the midpoint circle, its eight octants */
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

static void sv_disc(int32_t cx, int32_t cy, int32_t r, uint16_t c)   /* a filled circle */
{
    int32_t y, x;
    for (y = -r; y <= r; y++) {
        for (x = r; x * x + y * y > r * r; x--)
            ;
        cv_rect(cx - x, cy + y - sv_oy, 2 * x + 1, 1, c);
    }
}

/* the mark (a run-length list, logo_data.h) at (x0, y0), its tones 1..3 in pal (0: not drawn) */
static void sv_mark(const uint8_t *d, uint32_t n, int32_t w, int32_t x0, int32_t y0, const uint16_t *pal)
{
    uint32_t i;
    int32_t p = 0;
    for (i = 0; i < n; i++) {
        uint32_t tone = d[i] >> 6;
        int32_t len = (int32_t)(d[i] & 63u) + 1;
        if (tone && pal[tone])
            for (; len > 0; len--, p++)
                cv_pset(x0 + p % w, y0 + p / w - sv_oy, pal[tone]);
        else
            p += len;
    }
}

/* ------------------------------------------------------------------------------------------------ the smoke --- */
#define SV_EX 184                                /* the ember: the lit end of the spliff resting on the plate */
#define SV_EY 204
#define SV_RISE 200                              /* how high the smoke goes before it's gone */

static float sv_vnoise(float x)                  /* value noise along a line, smooth: 0..1 */
{
    int32_t xi = (int32_t)x;
    float f = x - (float)xi, a = (float)(sv_hash((uint32_t)xi, 7u) & 1023u), b;
    b = (float)(sv_hash((uint32_t)xi + 1u, 7u) & 1023u);
    f = f * f * (3.0f - 2.0f * f);
    return (a + (b - a) * f) * (1.0f / 1023.0f);
}

/* two strands curling up from the ember: thin and close at first, wider, looser and fainter as they rise, leaning
 * with the room's air, breaking into wisps (noise rising with them) */
static void sv_smoke(float t)
{
    int32_t h, s, x;
    for (h = 2; h < SV_RISE; h++) {
        int32_t y = SV_EY - h;
        float hf = (float)h, up = 1.0f - hf / (float)SV_RISE;
        if (y - sv_oy < 0 || y - sv_oy >= SV_BAND)
            continue;
        for (s = 0; s < 2; s++) {
            float amp = 0.6f + hf * 0.13f, w = 0.8f + hf * 0.065f, cx, a0, br;
            cx = (float)SV_EX - hf * 0.22f + 5.0f * fm_sinf(0.23f * t + (float)s) * hf * (1.0f / (float)SV_RISE) +
                 amp * fm_sinf(0.047f * hf - 1.5f * t + (float)s * 2.1f) +
                 0.45f * amp * fm_sinf(0.12f * hf - 2.6f * t + (float)s * 0.7f);
            br = sv_vnoise(40.0f + hf * 0.035f - t * 0.55f + (float)s * 13.0f);   /* where the strand breaks */
            br = (br - 0.2f) * 2.0f;
            br = br < 0.0f ? 0.0f : br > 1.0f ? 1.0f : br;
            a0 = (h < 10 ? 170.0f : 230.0f) * up * br * (s ? 0.75f : 1.0f);
            if (a0 < 4.0f)
                continue;
            for (x = (int32_t)(cx - 2.5f * w); x <= (int32_t)(cx + 2.5f * w) + 1; x++) {
                float d = ((float)x - cx) / w, k = 1.0f / (1.0f + d * d);
                uint32_t a = (uint32_t)(a0 * k * k);
                if (a > 3u)
                    sv_mix(x, y - sv_oy, RGB(206, 210, 220), a > 200u ? 200u : a);
            }
        }
    }
}

static void sv_spliff(uint32_t frame)            /* resting on the plate, its lit end to the right, glowing */
{
    static const uint16_t EMBER[4] = {RGB(255, 112, 40), RGB(255, 150, 60), RGB(255, 96, 32), RGB(255, 186, 96)};
    int32_t i, k, x0 = SV_EX - 34, y0 = SV_EY + 9;
    for (i = 0; i <= 30; i++) {                  /* the paper: a cone, thin at the roach, wider at the lit end */
        int32_t x = x0 + i, y = y0 - (i * 9) / 34, half = 1 + i / 12;
        for (k = -half; k <= half; k++)
            cv_pset(x, y + k - sv_oy, k == half ? RGB(176, 168, 152) : RGB(234, 228, 214));
        cv_pset(x, y + half + 1 - sv_oy, RGB(8, 8, 8));   /* its shadow on the plate */
    }
    cv_rect(x0 - 3, y0 - 1 - sv_oy, 4, 3, RGB(196, 160, 112));   /* the roach */
    cv_rect(SV_EX - 4, SV_EY - 2 - sv_oy, 3, 5, RGB(120, 116, 112));   /* the ash */
    for (k = -5; k <= 5; k++)                    /* the glow around the ember */
        for (i = -5; i <= 5; i++)
            if (i * i + k * k <= 25)
                sv_mix(SV_EX + i, SV_EY + k - sv_oy, RGB(255, 110, 40), (uint32_t)(70 - (i * i + k * k) * 2));
    cv_rect(SV_EX - 1, SV_EY - 2 - sv_oy, 3, 5, EMBER[sv_hash(frame, 3u) & 3u]);   /* it flickers */
    cv_pset(SV_EX, SV_EY - sv_oy, EMBER[3]);
}

/* ------------------------------------------------------------------------------------------------- the saver --- */
static void sv_scene(float ph, float t)          /* the dubplate: rings at phase ph (0..1 of the spacing), t in s */
{
    static const uint16_t PAL[4] = {0, RGB(52, 92, 160), RGB(84, 130, 196), RGB(150, 200, 228)};
    int32_t i, w;
    for (i = 0; i < 9; i++) {                    /* each further ring a repeat down: dimmer */
        float r = 62.0f + ((float)i + ph) * 8.0f;
        uint32_t a = (uint32_t)(230.0f * (1.0f - ((float)i + ph) / 9.0f));
        sv_ring(SV_CX, SV_CY, (int32_t)r, sv_fade(SV_ORANGE, a));
        sv_ring(SV_CX, SV_CY, (int32_t)r + 4, RGB(26, 24, 24));   /* the groove between */
    }
    sv_disc(SV_CX, SV_CY, 54, SV_TAN);
    sv_ring(SV_CX, SV_CY, 54, RGB(120, 84, 52));
    sv_mark(LOGO_SAVER, sizeof LOGO_SAVER, LOGO_SAVER_W, SV_CX - LOGO_SAVER_W / 2, SV_CY - LOGO_SAVER_H / 2, PAL);
    sv_spliff((uint32_t)(t * 10.0f));
    sv_smoke(t);
    if (sv.hour) {
        w = text_w(&FONT_XS, "SOUND SYSTEM STILL RUNNING");
        cv_text(120 - w / 2, 220 - sv_oy, &FONT_XS, "SOUND SYSTEM STILL RUNNING", C_GRAY);
    }
}

static void sv_paint(float ph, float t)
{
    for (sv_oy = 0; sv_oy < 240; sv_oy += SV_BAND) {
        cv_begin(240, SV_BAND, C_BLACK);
        sv_scene(ph, t);
        cv_blit(0, (uint32_t)sv_oy);
    }
    sv_oy = 0;
}

/* ------------------------------------------------------------------------------------------------ the splash --- */
void app_splash(void)                            /* the flyer: the mark over its orange print, the name under it */
{
    static const uint16_t UNDER[4] = {0, SV_ORANGE, SV_ORANGE, SV_ORANGE};
    static const uint16_t TOP[4] = {0, RGB(28, 58, 120), RGB(40, 80, 150), RGB(70, 112, 178)};
    int32_t w, x, y, mx = 120 - LOGO_SPLASH_W / 2, my = 12;
    for (sv_oy = 0; sv_oy < 240; sv_oy += SV_BAND) {
        cv_begin(240, SV_BAND, SV_PAPER);
        for (y = 0; y < SV_BAND; y++)            /* the paper's grain */
            for (x = 0; x < 240; x++) {
                uint32_t h = sv_hash((uint32_t)x, (uint32_t)(y + sv_oy)) & 255u;
                if (h < 6u)
                    cv_pset(x, y, h < 2u ? RGB(196, 184, 160) : RGB(218, 206, 182));
            }
        sv_mark(LOGO_SPLASH, sizeof LOGO_SPLASH, LOGO_SPLASH_W, mx + 4, my + 4, UNDER);   /* off register */
        sv_mark(LOGO_SPLASH, sizeof LOGO_SPLASH, LOGO_SPLASH_W, mx, my, TOP);
        w = text_w(&FONT_L, "DUBWISE");
        cv_text(120 - w / 2, 160 - sv_oy, &FONT_L, "DUBWISE", SV_INK);
        w = text_w(&FONT_XS, "SPILLT BEATS");
        cv_text(120 - w / 2, 196 - sv_oy, &FONT_XS, "SPILLT BEATS", RGB(232, 90, 50));
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
        sv.frame_t = now;
        if (!sv.hour && now - sv.last >= 3600000u) {   /* an hour on its own: the dance goes on */
            sv.hour = 1;
            app_say("SOUND SYSTEM", "STILL RUNNING");
        }
        sv_paint(ph, (float)t * 0.001f);
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
