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
