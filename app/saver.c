/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* The boot splash (APP_SPLASH) and the screen saver, with the SpIllt Beats mark: the owner's own brush drawing, kept
 * with soft edges and four shades of its blue (logo_data.h, made by tools/logo/mklogo.py: 4.6 KB for its
 * two sizes). Everything is drawn smooth: the rings, the label's edge, the face's edge, the smoke.
 * - The splash: a two-colour screen-print flyer. Cream paper with a grain, the mark in deep blue over an orange
 *   print of itself set a few pixels off (misregistered), DUBWISE and SPILLT BEATS under it.
 * - The saver: a dubplate. The mark on the record's tan label, orange rings running out across the black on the
 *   tempo's beat and fading as they spread (a dub's repeats). A face in profile, pure black at the left edge, cuts
 *   into the record (the vase-and-faces figure) and blows smoke across it, a breath every 7 s.
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
#define SV_CX 142                                /* the record's centre (right of the face) */
#define SV_CY 116

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

static void sv_ring(int32_t cx, int32_t cy, float r, uint16_t c, uint32_t a)   /* a smooth circle, c at a/256 */
{
    int32_t x, xm = (int32_t)(r * 0.7072f) + 1;
    for (x = 0; x <= xm; x++) {                  /* each octant's column: the two pixels the curve passes between */
        float yf = fm_sqrtf(r * r - (float)(x * x));
        int32_t y = (int32_t)yf, k;
        uint32_t a1 = (uint32_t)((yf - (float)y) * (float)a), a0 = a - a1;
        for (k = 0; k < 2; k++, y++) {
            uint32_t w = k ? a1 : a0;
            if (y < x)
                continue;
            sv_mix(cx + x, cy + y - sv_oy, c, w);
            sv_mix(cx - x, cy + y - sv_oy, c, w);
            sv_mix(cx + x, cy - y - sv_oy, c, w);
            sv_mix(cx - x, cy - y - sv_oy, c, w);
            if (x != y) {
                sv_mix(cx + y, cy + x - sv_oy, c, w);
                sv_mix(cx - y, cy + x - sv_oy, c, w);
                sv_mix(cx + y, cy - x - sv_oy, c, w);
                sv_mix(cx - y, cy - x - sv_oy, c, w);
            }
        }
    }
}

static void sv_disc(int32_t cx, int32_t cy, float r, uint16_t c)   /* a filled circle, its edge smooth */
{
    int32_t y, y0 = sv_oy - cy, y1 = sv_oy + SV_BAND - cy;
    for (y = (int32_t)-r; y <= (int32_t)r; y++) {
        float xf;
        int32_t x;
        uint32_t e;
        if (y < y0 || y >= y1)
            continue;
        xf = fm_sqrtf(r * r - (float)(y * y));
        x = (int32_t)xf;
        e = (uint32_t)((xf - (float)x) * 255.0f);
        cv_rect(cx - x + 1, cy + y - sv_oy, 2 * x - 1, 1, c);
        sv_mix(cx - x, cy + y - sv_oy, c, 128u + e / 2u);   /* the edge pixels, as much as the circle covers */
        sv_mix(cx + x, cy + y - sv_oy, c, 128u + e / 2u);
        sv_mix(cx - x - 1, cy + y - sv_oy, c, e / 2u);
        sv_mix(cx + x + 1, cy + y - sv_oy, c, e / 2u);
    }
}

/* the mark (a run-length list, logo_data.h) at (x0, y0): each pixel its shade (0..3) from pal, laid over the ground
 * as much as it covers (its edges soft) */
static void sv_mark(const uint8_t *d, uint32_t n, int32_t w, int32_t x0, int32_t y0, const uint16_t *pal)
{
    static const uint8_t COV[4] = {0, 96, 176, 255};
    uint32_t i;
    int32_t p = 0;
    for (i = 0; i < n; i++) {
        uint32_t cov = (d[i] >> 4) & 3u, shade = d[i] >> 6;
        int32_t len = (int32_t)(d[i] & 15u) + 1;
        if (cov)
            for (; len > 0; len--, p++)
                sv_mix(x0 + p % w, y0 + p / w - sv_oy, pal[shade], COV[cov]);
        else
            p += len;
    }
}

/* ------------------------------------------------------------------------- the face and its exhaled smoke --- */
/* a face in profile, pure black, mostly off the screen to the left, facing right: the record shows only where the
 * face isn't (the vase-and-faces figure). Its edge, (y, x) points down the screen */
static const uint8_t PROFILE[][2] = {
    {0, 38}, {24, 44}, {48, 52}, {64, 59}, {76, 55}, {88, 62}, {104, 78}, {113, 90}, {119, 94}, {124, 88}, {130, 76},
    {138, 80}, {144, 80}, {148, 73}, {153, 79}, {158, 80}, {166, 71}, {184, 80}, {196, 74}, {204, 62}, {216, 52},
    {240, 54},
};
#define PROFILE_N (sizeof PROFILE / sizeof PROFILE[0])
#define SV_MX 74                                 /* the lips, where the smoke leaves */
#define SV_MY 148
#define SV_REACH 186                             /* how far the smoke goes before it's gone */
#define SV_BREATH 7.0f                           /* an exhale every 7 s */

static float sv_face_x(int32_t y)                /* the face's edge at row y: a smooth curve through PROFILE */
{
    uint32_t i;
    for (i = 1; i < PROFILE_N; i++)
        if (y <= PROFILE[i][0]) {
            float p0 = PROFILE[i > 1 ? i - 2 : 0][1], p1 = PROFILE[i - 1][1], p2 = PROFILE[i][1];
            float p3 = PROFILE[i + 1 < PROFILE_N ? i + 1 : i][1];
            float u = (float)(y - PROFILE[i - 1][0]) / (float)(PROFILE[i][0] - PROFILE[i - 1][0]);
            return 0.5f * (2.0f * p1 + (p2 - p0) * u + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * u * u +
                           (3.0f * p1 - p0 - 3.0f * p2 + p3) * u * u * u);   /* (Catmull-Rom) */
        }
    return PROFILE[PROFILE_N - 1][1];
}

static void sv_face(void)                        /* black over the record left of its edge, the edge smooth */
{
    int32_t y;
    for (y = sv_oy; y < sv_oy + SV_BAND && y < 240; y++) {
        float xf = sv_face_x(y);
        int32_t x = (int32_t)xf;
        cv_rect(0, y - sv_oy, x, 1, C_BLACK);
        sv_mix(x, y - sv_oy, C_BLACK, 255u - (uint32_t)((xf - (float)x) * 255.0f) / 2u - 64u);
    }
}

static float sv_vnoise(float x)                  /* value noise along a line, smooth: 0..1 */
{
    int32_t xi = (int32_t)x;
    float f = x - (float)xi, a = (float)(sv_hash((uint32_t)xi, 7u) & 1023u), b;
    b = (float)(sv_hash((uint32_t)xi + 1u, 7u) & 1023u);
    f = f * f * (3.0f - 2.0f * f);
    return (a + (b - a) * f) * (1.0f / 1023.0f);
}

static float sv_breath(float tau)                /* how hard the breath was going out at time tau: 0..1 */
{
    float p;
    if (tau < 0.0f)
        return 0.0f;
    p = tau - SV_BREATH * (float)(int32_t)(tau * (1.0f / SV_BREATH));   /* 0..7 s into this breath */
    if (p > 2.4f)
        return 0.0f;                             /* (breathing in) */
    p *= 1.0f / 2.4f;
    return p < 0.15f ? p * (1.0f / 0.15f) : 1.0f - (p - 0.15f) * (1.0f / 0.85f) * (p - 0.15f) * (1.0f / 0.85f);
}

/* the exhale, blown out to the right: each column d px out from the lips is smoke that left them a while ago (it
 * slows as it goes), so the puffs travel; two strands, curling, widening and rising as they go, breaking into
 * wisps, fading */
static void sv_smoke(float t)
{
    int32_t d, s, y;
    for (d = 1; d < SV_REACH; d++) {
        float df = (float)d, out = 1.0f - df / (float)SV_REACH;
        float age = df * (1.0f / 70.0f) + df * df * (1.0f / 9000.0f);   /* 70 px/s at the lips, slowing */
        float puff = sv_breath(t - age);
        int32_t x = SV_MX + d;
        if (puff < 0.02f)
            continue;
        for (s = 0; s < 2; s++) {
            float amp = 0.3f + df * 0.11f, w = 0.9f + df * 0.075f, cy, a0, br;
            cy = (float)SV_MY - df * 0.16f - df * df * 0.0006f +
                 amp * fm_sinf(0.05f * df - 1.3f * t + (float)s * 2.3f) +
                 0.5f * amp * fm_sinf(0.13f * df - 2.2f * t + (float)s * 0.9f);
            br = sv_vnoise(40.0f + df * 0.04f - t * 0.5f + (float)s * 13.0f);
            br = (br - 0.2f) * 2.0f;
            br = br < 0.0f ? 0.0f : br > 1.0f ? 1.0f : br;
            a0 = 220.0f * out * br * puff * (s ? 0.7f : 1.0f);
            if (a0 < 4.0f)
                continue;
            for (y = (int32_t)(cy - 2.5f * w); y <= (int32_t)(cy + 2.5f * w) + 1; y++) {
                float e = ((float)y - cy) / w, k = 1.0f / (1.0f + e * e);
                uint32_t a = (uint32_t)(a0 * k * k);
                if (a > 3u && y >= sv_oy && y < sv_oy + SV_BAND)
                    sv_mix(x, y - sv_oy, RGB(206, 210, 220), a > 200u ? 200u : a);
            }
        }
    }
}

/* ------------------------------------------------------------------------------------------------- the saver --- */
static void sv_scene(float ph, float t)          /* the dubplate: rings at phase ph (0..1 of the spacing), t in s */
{
    static const uint16_t PAL[4] = {RGB(40, 76, 146), RGB(60, 104, 176), RGB(104, 152, 208), RGB(164, 208, 234)};
    int32_t i, w;
    sv_disc(SV_CX, SV_CY, 150.0f, RGB(20, 19, 22));   /* the vinyl */
    for (i = 0; i < 31; i++)                     /* its grooves */
        sv_ring(SV_CX, SV_CY, 60.0f + (float)i * 3.0f, RGB(44, 42, 46), 160u);
    for (i = 0; i < 9; i++) {                    /* the echo's rings, each further one a repeat down: dimmer */
        float r = 60.0f + ((float)i + ph) * 10.0f;
        sv_ring(SV_CX, SV_CY, r, SV_ORANGE, (uint32_t)(240.0f * (1.0f - ((float)i + ph) / 9.0f)));
    }
    sv_disc(SV_CX, SV_CY, 54.0f, SV_TAN);
    sv_ring(SV_CX, SV_CY, 54.0f, RGB(120, 84, 52), 200u);
    sv_mark(LOGO_SAVER, sizeof LOGO_SAVER, LOGO_SAVER_W, SV_CX - LOGO_SAVER_W / 2, SV_CY - LOGO_SAVER_H / 2, PAL);
    sv_face();
    sv_smoke(t);
    if (sv.hour) {
        w = text_w(&FONT_XS, "SOUND SYSTEM STILL RUNNING");
        cv_text(SV_CX - w / 2, 226 - sv_oy, &FONT_XS, "SOUND SYSTEM STILL RUNNING", C_GRAY);
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
    static const uint16_t UNDER[4] = {SV_ORANGE, SV_ORANGE, SV_ORANGE, SV_ORANGE};
    static const uint16_t TOP[4] = {RGB(26, 54, 116), RGB(36, 74, 144), RGB(60, 100, 168), RGB(104, 144, 200)};
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
