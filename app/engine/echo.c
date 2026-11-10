/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* echo.h. Compiled on its own at -O2 (fm1.toml o2). Floats only. */
#include "echo.h"
#include "fastmath.h"
#include "plat.h"                                /* PLAT_POOL */
#include "dsp.h"

static PLAT_POOL int16_t line[ECHO_LINE];        /* the tape: big, so in the pool */
#define LINE_FS 16384.0f                         /* full scale on the tape: 2x headroom for the saturation */

/* the in-loop all-passes (Schroeder): short and gentle. A Schroeder all-pass delays the lows by D (1+g)/(1-g), so
 * DUB-1's 37/89/151/276/441 at g 0.5 held a bass note's repeat back ~270 ms a pass, and a tempo-synced echo drifted
 * late (measured: 657 ms for 600). These smear each repeat by a few ms; the loop reads the tape AP_TOTAL earlier, so
 * a repeat lands on T on average (the lows ~8 ms a pass late, like tape wow) */
#define NAP 5
static const uint16_t AP_LEN[NAP] = {7, 13, 23, 37, 53};
#define AP_TOTAL (7 + 13 + 23 + 37 + 53)
static PLAT_POOL float ap_buf[AP_TOTAL];
static uint16_t ap_pos[NAP];
#define AP_G 0.3f

static echo_cfg_t cfg;
static uint32_t wpos;                            /* the write head */
static float t_now;                              /* the slewed time */
static float hp_x1, hp_y1, lp_y1;                /* the loop's filters */
static float dec_s1, dec_s2;                     /* the decimator's low-pass (two one-poles at ~4 kHz, 44.1 kHz) */
static float up_prev, up_cur, up_lp;             /* the interpolator's two line-rate outputs, and its smoothing */
static float upr_prev, upr_cur, upr_lp;          /* the right side's */
static hp4_t side_hp;                            /* the right's difference, high-passed at 150 Hz, 24 dB/oct */
#define SIDE_A 0.9182f                           /* exp(-2 pi 150 / 11025) */
static float out_r;                              /* line_step's right side */
static uint32_t phase;                           /* 0..3: where in the line sample this 44.1 kHz sample is */
static float lim_env = 0.0f, lim_g = 1.0f;
static uint32_t quiet;                           /* line samples in a row: silent in, both heads read zero */
/* 6.3: wow and flutter (after demake's media_block, GPL-3.0-only: a slow wow whose rate wanders, a faint flutter; and
 * DUB-1's depth, a fraction of T): the read heads move by T x WOW x 0.3 % at most, so each repeat bends a few cents,
 * more on every pass, as tape does. The wow's rate wanders 0.4 .. 1.2 Hz with a slow sine (demake: a random walk), so
 * where it is depends only on the time since PLAY, idle or not: a replay is the same, bit for bit (5.1) */
#define WOW_P1 13781u                            /* 0.8 Hz, in line samples (11,025 Hz) */
#define WOW_P2 157500u                           /* its rate's wander, 0.07 Hz */
#define WOW_P3 1575u                             /* the flutter, 7 Hz */
static uint32_t wow_c1, wow_c2, wow_c3;          /* (counters of line samples since PLAY, each in its period: idle or
                                                  * not, and on the line's own grid, so a replay reads the same) */

static void echo_settle(void);
void echo_wow_start(void)                       /* (and the line's samples from PLAY: a replay's fall where the pass's did) */
{
    wow_c1 = wow_c2 = wow_c3 = 0;
    if (lim_env < 1e-6f)                         /* (nothing heard from it, -120 dB: whatever it held settles to
                                                  * exactly 0, the engine doesn't run it while stopped and silent; a
                                                  * tail ringing is left alone) */
        echo_settle();
    if (quiet >= ECHO_LINE || lim_env < 1e-3f)   /* (the line's grid from here, so a replay's line samples fall where
                                                  * the pass's did; no click: nothing heard) */
        phase = 0;
}
static inline void wow_count(uint32_t n)
{
    wow_c1 = (wow_c1 + n) % WOW_P1;
    wow_c2 = (wow_c2 + n) % WOW_P2;
    wow_c3 = (wow_c3 + n) % WOW_P3;
}
static int last_heads_zero;
#define IDLE_AFTER ECHO_LINE                     /* the whole tape read as zero: it is empty */

static void echo_settle(void)                    /* (the tape is empty: what's left in the loop is 1e-20s) */
{
    uint32_t i;
    for (i = 0; i < AP_TOTAL; i++)
        ap_buf[i] = 0.0f;
    hp_x1 = hp_y1 = lp_y1 = dec_s1 = dec_s2 = 0.0f;
    up_prev = up_cur = up_lp = upr_prev = upr_cur = upr_lp = out_r = 0.0f;
    lim_env = 0.0f;
    lim_g = 1.0f;
    hp4_clear(&side_hp);
}

void echo_clear(void)                            /* the tape and the loop's state emptied; its time and settings kept
                                                  * (roadmap 5.2: tails off) */
{
    uint32_t i;
    for (i = 0; i < ECHO_LINE; i++)
        line[i] = 0;
    for (i = 0; i < AP_TOTAL; i++)
        ap_buf[i] = 0.0f;
    hp_x1 = hp_y1 = lp_y1 = dec_s1 = dec_s2 = up_prev = up_cur = up_lp = 0.0f;
    upr_prev = upr_cur = upr_lp = out_r = 0.0f;
    lim_env = 0.0f;
    lim_g = 1.0f;
    hp4_clear(&side_hp);
}

void echo_init(void)
{
    uint32_t i;
    for (i = 0; i < ECHO_LINE; i++)
        line[i] = 0;
    for (i = 0; i < AP_TOTAL; i++)
        ap_buf[i] = 0.0f;
    for (i = 0; i < NAP; i++)
        ap_pos[i] = 0;
    wpos = 0;
    t_now = 6615.0f;
    hp_x1 = hp_y1 = lp_y1 = dec_s1 = dec_s2 = up_prev = up_cur = up_lp = 0.0f;
    phase = 0;
    lim_env = 0.0f;
    lim_g = 1.0f;
    cfg.t = 6615.0f;
    cfg.g = 0.0f;
    cfg.tone = 0.5f;
    cfg.head2 = 0.0f;
    cfg.head3 = 0.0f;
    cfg.freeze = 0;
    cfg.slew = 0.0005f;
    cfg.width = 0.0f;
    cfg.diffuse = 1;
    cfg.drive = 1.0f;
    upr_prev = upr_cur = upr_lp = out_r = 0.0f;
    hp4_clear(&side_hp);
    quiet = IDLE_AFTER;
}

void echo_config(const echo_cfg_t *c)
{
    cfg = *c;
    if (cfg.t > ECHO_T_MAX)
        cfg.t = ECHO_T_MAX;
    if (cfg.t < 64.0f)
        cfg.t = 64.0f;
}

static inline float tap(float d)                 /* the line d samples behind the write head, interpolated */
{
    /* d's whole and fractional parts first, then the head's place: the fraction doesn't depend on where the head is
     * on the tape (wpos - d in floats near 18,000 keeps only ~0.002 of it: a replay, whose head sits elsewhere, read
     * the wow's in-between differently; 6.3) */
    int32_t di = (int32_t)d, i;
    float fd = d - (float)di;
    uint32_t a, b;
    i = (int32_t)wpos - di;                      /* the sample d's whole part back .. */
    if (i < 0)
        i += (int32_t)ECHO_LINE;
    a = (uint32_t)i;
    b = a == 0u ? ECHO_LINE - 1u : a - 1u;       /* .. and the one before it */
    return ((float)line[a] + fd * (float)(line[b] - line[a])) * (1.0f / LINE_FS);
}

static float line_step(float x)                  /* one line-rate sample: the input in, the return out */
{
    float h1, h2, h3 = 0.0f, hl, y, l;
    uint32_t k;
    int32_t w;
    float tw;
    t_now += (cfg.t - t_now) * cfg.slew;         /* ~180 ms (a dive: its own): the tape's speed changes, not jumps */
    tw = t_now;
    if (cfg.wow > 0.0f) {                        /* 6.3: the wow (0.8 Hz +- 0.4, wandering) and a faint flutter (7 Hz) */
        float c2 = (float)wow_c2 * (1.0f / (float)WOW_P2), ph;   /* (0.9095: the wander's 0.4 Hz / (2 pi 0.07 Hz)) */
        ph = (float)wow_c1 * (1.0f / (float)WOW_P1) - 0.9095f * fm_sin_turns(c2 + 0.25f);   /* (the rate's integral) */
        ph -= (float)(int32_t)ph;
        if (ph < 0.0f)
            ph += 1.0f;
        tw = t_now * (1.0f + cfg.wow * 0.003f * (0.95f * fm_sin_turns(ph)
                                                  + 0.05f * fm_sin_turns((float)wow_c3 * (1.0f / (float)WOW_P3))));
    }
    h1 = tap(tw);
    h2 = tap(2.0f * tw);
    if (cfg.head3 > 0.0f)                        /* 7.1: 3T, or as far as the tape goes (T over 0.55 s) */
        h3 = tap(fm_minf(3.0f * tw, (float)(ECHO_LINE - 4u)));
    last_heads_zero = h1 == 0.0f && h2 == 0.0f && h3 == 0.0f;
    hl = tap((cfg.pingpong ? 2.0f : 1.0f) * tw - (float)AP_TOTAL);   /* the loop's own read: the all-passes' delay
                                                                         * taken off (ping-pong: from 2T) */
    if (cfg.freeze) {                            /* held: the tape loops as it is (no filters, no new input) */
        y = cfg.pingpong ? h2 : h1;
        goto write;
    }
    /* the loop: high-pass ~80 Hz, tone low-pass, five all-passes, gain, saturation */
    y = undenorm(0.955f * (hp_y1 + hl - hp_x1));
    hp_x1 = hl;
    hp_y1 = y;
    lp_y1 = undenorm(lp_y1 + cfg.tone * (y - lp_y1));
    y = lp_y1;
    if (cfg.diffuse) {
        float *bp = ap_buf;
        for (k = 0; k < NAP; k++) {
            float d = bp[ap_pos[k]], v = undenorm(y + AP_G * d);
            bp[ap_pos[k]] = v;
            y = d - AP_G * v;
            if (++ap_pos[k] == AP_LEN[k])
                ap_pos[k] = 0;
            bp += AP_LEN[k];
        }
    }
    {
        float g = cfg.pingpong ? cfg.g * cfg.g : cfg.g;   /* (a lap of 2T: two repeats' worth) */
        y = cfg.drive > 1.0f ? fm_tanhf(cfg.drive * (g * y + x)) / cfg.drive : fm_tanhf(g * y + x);
    }
write:
    w = (int32_t)(y * LINE_FS);
    if (w >= -1 && w <= 1)                       /* (a tape step either side of 0 written as 0, -78 dBFS: with the wow's
                                                  * moving read, the loop can't hold a +-1 limit cycle for ever, so it
                                                  * empties and goes idle as it did; 6.3) */
        w = 0;
    line[wpos] = (int16_t)(w > 32767 ? 32767 : w < -32768 ? -32768 : w);
    if (++wpos == ECHO_LINE)
        wpos = 0;
    /* the return: the raw heads, then the peak limiter (-6 dBFS, instant attack, ~150 ms release), on both sides */
    if (cfg.pingpong) {                          /* odd repeats (T) left, even (2T, one g down) right: their mid on
                                                  * both, their difference above ~150 Hz apart (the lows centred) */
        float r = (cfg.g > 1.0f ? 1.0f : cfg.g) * h2, d = hp4_run(&side_hp, SIDE_A, 0.75f * (h1 - r));
        y = 0.75f * (h1 + r);                    /* (the highs all on their side: 1.5 x; the lows 0.75 x on both) */
        out_r = y - d;
        y += d;
    } else if ((y = h1 + cfg.head2 * h2 + cfg.head3 * h3), cfg.width > 0.0f) {   /* the right reads later; only its difference above ~120 Hz */
        float d = tap(tw + cfg.width) + cfg.head2 * h2 + cfg.head3 * h3 - y;
        out_r = y + hp4_run(&side_hp, SIDE_A, d);
    } else {
        out_r = y;
    }
    l = fm_maxf(fm_fabsf(y), fm_fabsf(out_r));
    lim_env = l > lim_env ? l : undenorm(lim_env * 0.9994f);
    lim_g = lim_env > 0.5f ? 0.5f / lim_env : 1.0f;
    out_r *= lim_g;
    return y * lim_g;
}

void echo_run(const float *in, float *out, float *outr, uint32_t n)
{
    uint32_t i;
    if (quiet >= IDLE_AFTER && !cfg.freeze) {    /* idle: the tape empty, nothing coming in: costs nothing */
        for (i = 0; i < n && fm_fabsf(in[i]) < 1e-15f; i++)   /* (a send's last 1e-25s are silence too) */
            ;
        if (i == n) {
            phase += n;                          /* (the grid runs on through the silence) */
            wow_count(phase / 4u);
            phase %= 4u;
            for (i = 0; i < n; i++)
                out[i] = outr[i] = 0.0f;
            t_now = cfg.t;                       /* (an empty tape has nothing to bend: the time is there at once) */
            return;
        }
        quiet = 0;
    }
    for (i = 0; i < n; i++) {
        float x = fm_fabsf(in[i]) < 1e-15f ? 0.0f : in[i];   /* (-300 dB and under: nothing; a replay then the same) */
        dec_s1 = undenorm(dec_s1 + 0.44f * (x - dec_s1));   /* ~4 kHz, twice: images below 5.5 kHz kept out */
        dec_s2 = undenorm(dec_s2 + 0.44f * (dec_s1 - dec_s2));
        if (++phase == 4u) {
            phase = 0;
            wow_count(1u);
            up_prev = up_cur;
            upr_prev = upr_cur;
            up_cur = line_step(dec_s2);
            upr_cur = out_r;
            if (x == 0.0f && fm_fabsf(dec_s2) < 1e-12f && up_cur == 0.0f && last_heads_zero)   /* (the decimator's
                                                  * last 1e-25s count as silence: it never quite reaches 0) */
                if (++quiet == IDLE_AFTER)       /* idle from here: every part of it back to exactly 0 (a start from
                                                  * silence then the same each time, bit for bit: a replay, 5.1) */
                    echo_settle();
            else
                quiet = 0;
        }
        /* back up: linear between the last two line samples (a line sample late), then smoothed */
        up_lp = undenorm(up_lp + 0.5f * ((up_prev + (up_cur - up_prev) * (float)phase * 0.25f) - up_lp));
        out[i] = up_lp;
        upr_lp = undenorm(upr_lp + 0.5f * ((upr_prev + (upr_cur - upr_prev) * (float)phase * 0.25f) - upr_lp));
        outr[i] = upr_lp;
    }
}

float echo_gr(void) { return lim_g; }
float echo_t_now(void) { return t_now; }
