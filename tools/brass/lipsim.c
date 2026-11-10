/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors
 * Roadmap 7.7: the STK Brass tick (Perry R. Cook and Gary P. Scavone, MIT-style) simulated offline, as the firmware
 * would run it; tools/brass/README.md. cc -O2 -o lipsim lipsim.c -lm */
/* offline: STK Brass tick (as the firmware would run it, floats, 44.1 kHz), its speaking and pitch measured */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#define SR 44100.0f
#define L 4096
typedef struct { int speaks; float f, amp, jitter, onset_ms; } res_t;
static res_t run(float f, float tens, float breath, float dmul, float att_ms, int N)
{
    static float line[L]; int w = 0, i; res_t r = {0, 0, 0, 0, -1};
    float y1 = 0, y2 = 0, dcx = 0, dcy = 0, rr = 0.997f, wl = 2 * 3.14159265f * f * tens / SR;
    float a1 = -2 * rr * cosf(wl), a2 = rr * rr, d = (2 * SR / f + 3) * dmul;
    static float buf[16384]; float pk = 0;
    for (i = 0; i < L; i++) line[i] = 0;
    for (i = 0; i < N; i++) {
        float env = att_ms > 0 ? fminf(1.0f, i / (att_ms * 0.001f * SR)) : 1.0f, mouth = 0.3f * breath * env;
        int di = (int)d; float fr = d - di; int a = (w - di + L) % L, b = (a - 1 + L) % L;
        float bore = 0.85f * (line[a] + fr * (line[b] - line[a]));
        float dp = 0.03f * (mouth - bore) - a1 * y1 - a2 * y2; y2 = y1; y1 = dp;
        dp *= dp; if (dp > 1) dp = 1;
        float x = dp * mouth + (1 - dp) * bore;
        dcy = x - dcx + 0.99f * dcy; dcx = x;
        line[w] = dcy; w = (w + 1) % L;
        if (r.onset_ms < 0 && fabsf(bore) > 0.02f) r.onset_ms = i * 1000.0f / SR;
        if (i >= N - 4096) { buf[i - (N - 4096)] = bore; pk = fmaxf(pk, fabsf(bore)); }
    }
    r.amp = pk;
    if (pk > 0.02f) {                             /* the pitch by autocorrelation over the last 4,096 samples */
        int lag, lo = (int)(SR / 2500), hi = (int)(SR / 40), best = 0; double e0 = 0, bv = -1, ac[1200];
        for (i = 0; i < 2048; i++) e0 += buf[i] * buf[i];
        for (lag = lo; lag < hi && lag < 1200; lag++) {
            double s = 0, e1 = 0;
            for (i = 0; i < 2048; i++) { s += buf[i] * buf[i + lag]; e1 += buf[i + lag] * buf[i + lag]; }
            ac[lag] = s / sqrt(e0 * e1 + 1e-20);
        }
        /* the first lag whose correlation is within 5 % of the best: the period, not a multiple of it */
        for (lag = lo; lag < hi && lag < 1200; lag++) if (ac[lag] > bv) bv = ac[lag];
        for (lag = lo + 1; lag < hi - 1 && lag < 1199; lag++)
            if (ac[lag] >= 0.95 * bv && ac[lag] >= ac[lag - 1] && ac[lag] >= ac[lag + 1]) { best = lag; break; }
        if (best) {
            double y0 = ac[best - 1], y1v = ac[best], y2v = ac[best + 1], dl = 0.5 * (y0 - y2v) / (y0 - 2 * y1v + y2v);
            r.f = SR / (float)(best + dl); r.jitter = 1.0f - (float)y1v; r.speaks = y1v > 0.9;
        }
    }
    return r;
}
int main(int argc, char **argv)
{
    int note; float breaths[4] = {0.3f, 0.5f, 0.7f, 0.9f};
    printf("note  target  mode tension   speak onset   cents(b.3 .5 .7 .9)  1-period\n");
    for (note = 36; note <= 88; note += 1) {
        float ft = 440.0f * powf(2.0f, (note - 69) / 12.0f), bscore = 1e9, bt = 0, bm = 0, bc[4] = {NAN, NAN, NAN, NAN}, bon = 0, bj = 0;
        int bsp = 0;
        for (int mode = 1; mode <= 2; mode++)
            for (float tens = 0.3f; tens <= 3.0f; tens += 0.01f) {
                float dmul = mode == 1 ? 0.5f * (2 * SR / ft) / (2 * SR / ft + 3) : 1.0f;   /* mode 1: the bore at the note's own period */
                res_t r = run(ft, tens, 0.6f, dmul, 30, 12000);
                if (!r.speaks || fabsf(1200 * log2f(r.f / ft)) > 30) continue;
                float c[4], mx = 0, on = 0, jit = 0; int sp = 0;
                for (int k = 0; k < 4; k++) {
                    r = run(ft, tens, breaths[k], dmul, 30, 16000);
                    if (r.speaks && r.onset_ms >= 0 && r.onset_ms < 80) { sp++; c[k] = 1200 * log2f(r.f / ft); mx = fmaxf(mx, fabsf(c[k])); on = fmaxf(on, r.onset_ms); jit = fmaxf(jit, r.jitter); }
                    else c[k] = NAN;
                }
                float score = (4 - sp) * 1000 + mx + jit * 300;
                if (score < bscore) { bscore = score; bt = tens; bm = (float)mode; bsp = sp; for (int k = 0; k < 4; k++) bc[k] = c[k]; bon = on; bj = jit; }
            }
        printf("%4d %7.1f  %3.0f  %6.2f   %d/4  %5.1f   %6.1f %6.1f %6.1f %6.1f   %.4f\n", note, ft, bm, bt, bsp, bon, bc[0], bc[1], bc[2], bc[3], bj);
        fflush(stdout);
    }
    return 0;
}
