/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* The boot splash (APP_SPLASH) and the screen saver.
 * - The splash: the SpIllt Beats mark (the owner's own brush drawing, with soft edges and four shades of its blue:
 *   logo_data.h, made by tools/logo/mklogo.py) as a two-colour screen-print flyer. Cream paper with a grain, the
 *   mark in deep blue over an orange print of itself set a few pixels off, DUBWISE and SPILLT BEATS under it.
 * - The saver: the owner's illustration of a selector at his desk (art_data.h, made by tools/art/mkart.py: 240 x
 *   240, 256 colours, 52 KB). Every 20 s his smoke billows out of the spliff into the SpIllt Beats mark, a cloud
 *   in the picture's own comic-book style (cream lobes, a grey shade, a grey-brown line round it, curls), as big as
 *   the picture, holds, and breaks up lobe by lobe; the other half of the time the picture is clear.
 * The saver comes on while stopped, after SETUP's SCREEN SAVER minutes with nothing touched. Any key, button or knob
 * wakes it, and that touch does nothing else: its press and its release are swallowed. It never comes on while
 * playing (a performer may leave the desk alone for minutes).
 * Part of the unity build: dub.c includes it. */

#include "logo_data.h"
#include "art_data.h"

#define SV_ORANGE RGB(242, 120, 72)
#define SV_PAPER RGB(236, 226, 204)              /* the flyer */
#define SV_INK RGB(28, 28, 32)
#define SV_BAND 60                               /* the screen in four bands (the canvas holds 76 rows) */

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

static void sv_mix(int32_t x, int32_t y, uint16_t c, uint32_t a)   /* c over the canvas's pixel at a/256 */
{
    uint32_t i, d, r, g, b;
    y += cv_oy;
    if ((uint32_t)x >= cv_w || y < cv_y0 || y >= cv_y1)
        return;
    i = (uint32_t)y * cv_w + (uint32_t)x;
    d = swap16(cv_px[i]);
    r = (((d >> 11) & 31u) * (256u - a) + ((c >> 11) & 31u) * a + 128u) >> 8;   /* (rounded: many faint layers */
    g = (((d >> 5) & 63u) * (256u - a) + ((c >> 5) & 63u) * a + 128u) >> 8;    /* don't drift the hue) */
    b = ((d & 31u) * (256u - a) + (c & 31u) * a + 128u) >> 8;
    cv_px[i] = swap16((r << 11) | (g << 5) | b);
}

static uint32_t sv_hash(uint32_t x, uint32_t y)
{
    uint32_t h = x * 374761393u + y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
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

/* ------------------------------------------------------------------------------------------- the picture --- */
static void sv_art(void)                         /* the illustration (art_data.h, PackBits), this band's rows */
{
    uint32_t p0 = (uint32_t)sv_oy * 240u, p1 = p0 + SV_BAND * 240u;
    uint32_t i = p0 >= ART_MID_P ? ART_MID : 0u, p = p0 >= ART_MID_P ? ART_MID_P : 0u;   /* (the band's start) */
    while (i < sizeof ART && p < p1) {
        uint32_t n = ART[i++], k;
        if (n < 128u) {                          /* n + 1 colours as they come */
            for (k = 0; k <= n; k++, p++)
                if (p >= p0 && p < p1)
                    cv_pset((int32_t)(p % 240u), (int32_t)(p / 240u) - sv_oy, ART_PAL[ART[i + k]]);
            i += n + 1u;
        } else {                                 /* one colour, 257 - n times */
            uint16_t c = ART_PAL[ART[i++]];
            for (k = 0; k < 257u - n; k++, p++)
                if (p >= p0 && p < p1)
                    cv_pset((int32_t)(p % 240u), (int32_t)(p / 240u) - sv_oy, c);
        }
    }
}

/* ---------------------------------------------------------------------------------------------- the smoke --- */
/* In the picture's own style: a comic-book cloud of round lobes, cream, a grey shadow low and right in each, one
 * ink line round the whole, and ink curls in some. Every 20 s it billows out of the spliff into the SpIllt Beats
 * mark, as big as the picture (covering it: that's the show), holds, and breaks up, lobe by lobe; for the other
 * half of the time the picture is clear. */
#define SV_TX 195                                /* the spliff's lit end in the picture */
#define SV_TY 110
#define SV_MKX 112                               /* the mark's centre */
#define SV_MKY 104
#define SV_CYCLE 20.0f
#define SV_OUTLINE RGB(112, 100, 92)             /* the cloud's own line: a grey-brown, as drawn */
#define SV_SHADE RGB(204, 194, 180)
#define SV_PUFF RGB(246, 240, 228)

static float sv_vnoise(float x)                  /* value noise along a line, smooth: 0..1 */
{
    int32_t xi = (int32_t)x;
    float f = x - (float)xi, a = (float)(sv_hash((uint32_t)xi, 7u) & 1023u), b;
    b = (float)(sv_hash((uint32_t)xi + 1u, 7u) & 1023u);
    f = f * f * (3.0f - 2.0f * f);
    return (a + (b - a) * f) * (1.0f / 1023.0f);
}

static void sv_disc(float cx, float cy, float r, uint16_t c)   /* a solid round lobe, its edge smooth */
{
    int32_t y;
    if (r < 0.5f)
        return;
    for (y = (int32_t)(cy - r); y <= (int32_t)(cy + r) + 1; y++) {
        float dy = (float)y - cy, h2 = r * r - dy * dy, h, xl, xr;
        int32_t il, ir;
        if (h2 <= 0.0f || y < sv_oy || y >= sv_oy + SV_BAND)
            continue;
        h = fm_sqrtf(h2);
        xl = cx - h;
        xr = cx + h;
        il = (int32_t)xl + 1;                    /* the pixels wholly inside: filled; the two at the ends: blended */
        ir = (int32_t)xr - 1;
        if (ir >= il)
            cv_rect(il, y - sv_oy, ir - il + 1, 1, c);
        sv_mix(il - 1, y - sv_oy, c, (uint32_t)(((float)il - xl) * 255.0f));
        sv_mix(ir + 1, y - sv_oy, c, (uint32_t)((xr - (float)(ir + 1)) * 255.0f));
    }
}

/* lobe i at time tc into the cycle: 0 when it isn't out. Out of the lit end (staggered over 2 s, 1.6 s on the
 * way, swelling), held on the mark (breathing), then broken off: drifting up and out, shrinking to nothing */
static int sv_lobe(uint32_t i, float tc, float t, float *x, float *y, float *r)
{
    float j = (float)(sv_hash(i, 11u) & 255u) * (1.0f / 255.0f), rr = 7.0f + 3.5f * j;
    float tx = (float)(SV_MKX + MARK_PT[i][0]), ty = (float)(SV_MKY + MARK_PT[i][1]);
    float s0 = 2.0f * (float)(sv_hash(i, 5u) & 1023u) * (1.0f / 1023.0f), u, v, w;
    if (tc < s0)
        return 0;
    if (tc < s0 + 1.6f) {                        /* billowing out: a curve up from the lit end */
        u = (tc - s0) * (1.0f / 1.6f);
        v = u * u * (3.0f - 2.0f * u);
        w = 1.0f - v;
        *x = w * w * (float)SV_TX + 2.0f * w * v * (220.0f - 40.0f * j) + v * v * tx;
        *y = w * w * (float)SV_TY + 2.0f * w * v * (30.0f + 30.0f * j) + v * v * ty;
        *r = rr * (0.25f + 0.75f * v);
        return 1;
    }
    if (tc < 7.5f) {                             /* the mark, breathing */
        *x = tx + 0.8f * fm_sinf(1.1f * t + (float)i);
        *y = ty + 0.6f * fm_sinf(0.9f * t + (float)i * 0.7f);
        *r = rr * (1.0f + 0.06f * fm_sinf(1.4f * t + (float)i * 1.3f));
        return 1;
    }
    u = (tc - 7.5f - 1.5f * j) * (1.0f / 1.4f);  /* breaking up: lobe by lobe */
    if (u >= 1.0f)
        return 0;
    if (u < 0.0f)
        u = 0.0f;
    *x = tx + (tx - (float)SV_MKX) * 0.3f * u + 10.0f * u * (sv_vnoise(30.0f + (float)i * 0.37f) - 0.5f);
    *y = ty - 22.0f * u * (0.5f + j) + (ty - (float)SV_MKY) * 0.2f * u;
    *r = rr * (1.0f - u * u);
    return 1;
}

static void sv_curl(float cx, float cy, float r, float ph)   /* an ink curl in a lobe: a spiral in */
{
    int32_t k;
    for (k = 0; k < 40; k++) {                   /* (a turn and a half, two pixels thick) */
        float th = (float)k * 0.24f, rho = r * 0.66f * (1.0f - th * (1.0f / 12.0f));
        float px = cx + rho * fm_cosf(th + ph), py = cy + rho * fm_sinf(th + ph);
        int32_t ix = (int32_t)px, iy = (int32_t)py;
        float fx = px - (float)ix, fy = py - (float)iy;
        if (iy < sv_oy - 1 || iy > sv_oy + SV_BAND)
            continue;
        sv_mix(ix, iy - sv_oy, SV_OUTLINE, (uint32_t)((1.0f - fx) * (1.0f - fy) * 230.0f));
        sv_mix(ix + 1, iy - sv_oy, SV_OUTLINE, (uint32_t)(fx * (1.0f - fy) * 230.0f));
        sv_mix(ix, iy + 1 - sv_oy, SV_OUTLINE, (uint32_t)((1.0f - fx) * fy * 230.0f));
        sv_mix(ix + 1, iy + 1 - sv_oy, SV_OUTLINE, (uint32_t)(fx * fy * 230.0f));
        sv_mix(ix + 1, iy - sv_oy, SV_OUTLINE, 110u);
    }
}

static void sv_smoke(float t)
{
    float tc = t - SV_CYCLE * (float)(int32_t)(t * (1.0f / SV_CYCLE)), x, y, r;
    uint32_t i, pass;
    if (tc >= 10.5f)
        return;                                  /* the picture's half */
    for (pass = 0; pass < 4; pass++)             /* the ink round all, the shade, the light, the curls */
        for (i = 0; i < MARK_PTS; i++)
            if (sv_lobe(i, tc, t, &x, &y, &r)) {
                if (pass == 0)
                    sv_disc(x, y, r + 1.8f, SV_OUTLINE);
                else if (pass == 1)
                    sv_disc(x, y, r, SV_SHADE);
                else if (pass == 2)
                    sv_disc(x - r * 0.22f, y - r * 0.26f, r * 0.74f, SV_PUFF);
                else if ((sv_hash(i, 3u) % 6u) == 0u && r > 6.0f)   /* (curls toward the cloud's edge, as drawn) */
                    sv_curl(x + (x - (float)SV_MKX) * 0.06f, y + (y - (float)SV_MKY) * 0.06f, r * 0.95f, 0.5f * t + (float)i);
            }
}

/* ------------------------------------------------------------------------------------------------- the saver --- */
static void sv_scene(float t)
{
    int32_t w;
    sv_art();
    sv_smoke(t);
    if (sv.hour) {
        w = text_w(&FONT_XS, "SOUND SYSTEM STILL RUNNING");
        cv_rect(120 - w / 2 - 4, 224 - sv_oy, w + 8, 14, C_BLACK);
        cv_text(120 - w / 2, 226 - sv_oy, &FONT_XS, "SOUND SYSTEM STILL RUNNING", C_GRAY);
    }
}

static void sv_paint(float t)
{
    for (sv_oy = 0; sv_oy < 240; sv_oy += SV_BAND) {
        cv_begin(240, SV_BAND, C_BLACK);
        sv_scene(t);
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
        uint32_t t = now - sv.t0;
        sv.frame_t = now;
        if (!sv.hour && now - sv.last >= 3600000u) {   /* an hour on its own: the dance goes on */
            sv.hour = 1;
            app_say("SOUND SYSTEM", "STILL RUNNING");
        }
        sv_paint((float)t * 0.001f);
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
