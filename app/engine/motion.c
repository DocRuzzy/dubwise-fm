/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* motion.h. Compiled on its own (fm1.toml o2). */
#include "motion.h"
#include "plat.h"

/* the moves: a pool of chunks of MOT_CHUNK, each bar a chain of them (most bars have none); a bar's moves in order,
 * move i in its chain's chunk i / MOT_CHUNK. Bounded: a bar holds MOT_BAR_MAX at most (MOT_BAR_MAX / MOT_CHUNK hops) */
static PLAT_POOL mot_ev_t pool[MOT_CHUNKS][MOT_CHUNK];
static PLAT_POOL uint16_t nxt[MOT_CHUNKS];       /* the chunk after (the chain, or the free list); MOT_NONE: none */
static PLAT_POOL uint16_t head[SONG_BARS];
static uint16_t free_head;
uint8_t mot_n[SONG_BARS];
uint32_t mot_drops;
volatile uint32_t mot_dirty;
static volatile int suspended;                   /* the main loop has the table (CMD_MOTION: suspend .. resume) */

static int32_t bar = -1;                         /* the bar whose motion plays (-1: none) */
static int rec;
static uint32_t next;                            /* the next move to play in it */
#define NTOUCH 16u
static uint16_t touched[NTOUCH];                 /* hands on these in this bar: their motion not played */
static uint32_t ntouched;

static mot_ev_t *at(uint32_t b, uint32_t i)      /* bar b's move i (i < its chunks x MOT_CHUNK) */
{
    uint32_t c = head[b], k;
    for (k = i / MOT_CHUNK; k; k--)
        c = nxt[c];
    return &pool[c][i % MOT_CHUNK];
}

static int grow(uint32_t b)                      /* room for one more move in bar b: 0 none (the pool is full) */
{
    uint32_t n = mot_n[b], c, last;
    if (n >= MOT_BAR_MAX)
        return 0;
    if (n % MOT_CHUNK)
        return 1;                                /* (room in its last chunk) */
    if (free_head == MOT_NONE)
        return 0;
    c = free_head;
    free_head = nxt[c];
    nxt[c] = MOT_NONE;
    if (!n)
        head[b] = (uint16_t)c;
    else {
        for (last = head[b]; nxt[last] != MOT_NONE; last = nxt[last])
            ;
        nxt[last] = (uint16_t)c;
    }
    return 1;
}

static void shrink(uint32_t b)                   /* the chunks past what bar b's moves need: back to the pool */
{
    uint32_t need = (mot_n[b] + MOT_CHUNK - 1u) / MOT_CHUNK, k, c;
    uint16_t *link = &head[b];
    for (k = 0; k < need; k++)
        link = &nxt[*link];
    c = *link;
    *link = MOT_NONE;
    while (c != MOT_NONE) {
        uint32_t n2 = nxt[c];
        nxt[c] = free_head;
        free_head = (uint16_t)c;
        c = n2;
    }
}

static void reset_all(void)
{
    uint32_t b, c;
    for (b = 0; b < SONG_BARS; b++) {
        mot_n[b] = 0;
        head[b] = MOT_NONE;
    }
    for (c = 0; c < MOT_CHUNKS; c++)
        nxt[c] = (uint16_t)(c + 1u < MOT_CHUNKS ? c + 1u : MOT_NONE);
    free_head = 0;
}

int mot_recordable(uint32_t id)                  /* (not the spin-back: a song that rewinds itself never ends) */
{
    return id >= P_CH_BASE && id < P_NIDS && id != P_SPIN && param_desc(id) != 0;
}

int mot_momentary(uint32_t id)
{
    uint32_t ch;
    for (ch = 0; ch < NCH; ch++)
        if (id == P_CH(ch, CHF_THROW))
            return (int)ch;
    return id == P_ECHO_FREEZE ? (int)NCH : id == P_ECHO_PUSH ? (int)NCH + 1 : id == P_ECHO_DIVE ? (int)NCH + 2
           : id == P_SPRING_KICK ? (int)NCH + 3 : id == P_WET ? (int)NCH + 4 : id == P_DNB ? (int)NCH + 5 : -1;
}

uint32_t mot_mom_id(uint32_t k)
{
    static const uint16_t X[6] = {P_ECHO_FREEZE, P_ECHO_PUSH, P_ECHO_DIVE, P_SPRING_KICK, P_WET, P_DNB};
    return k < NCH ? P_CH(k, CHF_THROW) : X[(k - NCH) % 6u];
}

static int keeps_all(uint32_t id)                /* every press and release kept (no thinning) */
{
    uint32_t ch;
    if (mot_momentary(id) >= 0)
        return 1;
    for (ch = 0; ch < NCH; ch++)
        if (id == P_CH(ch, CHF_MUTE) || id == P_CH(ch, CHF_SOLO))
            return 1;
    return 0;
}

void mot_init(void)
{
    reset_all();
    mot_drops = 0;
    mot_dirty = 0;
    suspended = 0;
    bar = -1;
    ntouched = 0;
}

void mot_clear(void)
{
    reset_all();
    next = 0;
    mot_dirty = 1;
}

void mot_suspend(int on)                         /* engine (CMD_MOTION): the table handed to the main loop, or back */
{
    suspended = on;
    bar = -1;
    ntouched = 0;
}

int mot_suspended(void) { return suspended; }

static int is_touched(uint32_t id)
{
    uint32_t i;
    for (i = 0; i < ntouched; i++)
        if (touched[i] == id)
            return 1;
    return 0;
}

static void punch(uint32_t b, uint32_t id, uint32_t tick)   /* id's motion in bar b from tick on: gone */
{
    uint32_t i, k = 0, n = mot_n[b];
    for (i = 0; i < n; i++) {
        mot_ev_t *m = at(b, i);
        if (m->id == id && m->tick >= tick) {
            if ((int32_t)b == bar && i < next)
                next--;
            continue;
        }
        if (k != i)
            *at(b, k) = *m;
        k++;
    }
    mot_n[b] = (uint8_t)k;
    shrink(b);
}

static void touch(uint32_t id, uint32_t tick)
{
    if (is_touched(id))
        return;
    if (ntouched < NTOUCH)
        touched[ntouched++] = (uint16_t)id;
    if (rec && bar >= 0)
        punch((uint32_t)bar, id, tick);
}

void mot_line(int32_t b, int r, const int16_t *ev)
{
    uint32_t i, k = 0;
    int carry = r && rec && bar >= 0 && b >= 0;
    if (suspended)
        return;
    for (i = 0; i < ntouched; i++)               /* a throw still held over the line: it goes on replacing */
        if (carry && mot_momentary(touched[i]) >= 0 && ev[touched[i]])
            touched[k++] = touched[i];
    ntouched = k;
    bar = b >= 0 && b < (int32_t)SONG_BARS ? b : -1;
    rec = r;
    next = 0;
    if (bar >= 0 && rec)
        for (i = 0; i < ntouched; i++)
            punch((uint32_t)bar, touched[i], 0);
}

static int insert(uint32_t b, uint32_t tick, uint32_t id, int32_t v)   /* in order, after every move at or before tick */
{
    uint32_t n = mot_n[b], i;
    mot_ev_t *m;
    if (!grow(b)) {
        mot_drops++;
        return -1;
    }
    for (i = n; i > 0 && at(b, i - 1u)->tick > tick; i--)
        *at(b, i) = *at(b, i - 1u);
    m = at(b, i);
    m->tick = (uint16_t)tick;
    m->id = (uint16_t)id;
    m->v = (int16_t)v;
    mot_n[b] = (uint8_t)(n + 1u);
    mot_dirty = 1;
    return (int)i;
}

void mot_user(uint32_t id, int32_t v, uint32_t tick, int rec_now)
{
    uint32_t n, i;
    int k;
    if (suspended || bar < 0 || !mot_recordable(id))
        return;
    if (!rec_now)
        rec = 0;
    touch(id, tick);
    if (!rec)
        return;
    n = mot_n[bar];
    if (!keeps_all(id))                          /* a knob: one value a sixteenth (the latest) */
        for (i = n; i-- > 0;) {
            mot_ev_t *m = at((uint32_t)bar, i);
            if (m->id == id) {
                if (m->tick / (MOT_TPB / 4u) == tick / (MOT_TPB / 4u)) {
                    m->v = (int16_t)v;
                    mot_dirty = 1;
                    return;
                }
                break;
            }
        }
    k = insert((uint32_t)bar, tick, id, v);
    if (k >= 0 && (uint32_t)k < next)
        next++;
}

int mot_put(uint32_t b, uint32_t tick, uint32_t id, int32_t v)   /* main loop, suspended (or before the engine runs) */
{
    if (b >= SONG_BARS || tick >= 4u * MOT_TPB || !mot_recordable(id))
        return 0;
    return insert(b, tick, id, v) >= 0;
}

int mot_due(uint32_t tick, mot_ev_t *e)
{
    if (bar < 0)
        return 0;
    while (next < mot_n[bar] && at((uint32_t)bar, next)->tick <= tick) {
        const mot_ev_t *m = at((uint32_t)bar, next++);
        if (is_touched(m->id))
            continue;
        *e = *m;
        return 1;
    }
    return 0;
}

int32_t mot_next_tick(void)
{
    uint32_t i;
    if (bar < 0)
        return -1;
    for (i = next; i < mot_n[bar]; i++) {
        const mot_ev_t *m = at((uint32_t)bar, i);
        if (!is_touched(m->id))
            return m->tick;
    }
    return -1;
}

/* engine -> main loop */
#define FB_N 64u
static struct {
    uint16_t id;
    int16_t v;
} fb[FB_N];
static volatile uint32_t fb_w, fb_r;

void mot_fb_push(uint32_t id, int32_t v)
{
    uint32_t w = fb_w;
    if (w - fb_r >= FB_N)
        return;                                  /* full: the screen misses one (the sound does not) */
    fb[w % FB_N].id = (uint16_t)id;
    fb[w % FB_N].v = (int16_t)v;
    __asm__ volatile("" ::: "memory");
    fb_w = w + 1u;
}

int mot_fb_pop(uint32_t *id, int32_t *v)
{
    uint32_t r = fb_r;
    if (r == fb_w)
        return 0;
    __asm__ volatile("" ::: "memory");
    *id = fb[r % FB_N].id;
    *v = fb[r % FB_N].v;
    __asm__ volatile("" ::: "memory");
    fb_r = r + 1u;
    return 1;
}

/* the flash copy: a header (magic, length, sum) written last, then the counts and each bar's moves */
#define MOT_MAGIC 0x32544F4Du                    /* "MOT2" (512 bars) */
typedef struct {
    uint32_t magic, len, sum;
} mot_head_t;

static uint32_t sum_bytes(uint32_t h, const uint8_t *p, uint32_t n)
{
    while (n--)
        h = (h ^ *p++) * 16777619u;
    return h;
}

uint32_t mot_total(void)
{
    uint32_t b, t = 0;
    for (b = 0; b < SONG_BARS; b++)
        t += mot_n[b];
    return t;
}

int mot_save(void)
{
    mot_head_t h;
    uint32_t b, off, sc, sectors, i;
    if (!mot_dirty)
        return 1;
    h.magic = MOT_MAGIC;
    h.len = SONG_BARS + mot_total() * (uint32_t)sizeof(mot_ev_t);
    sectors = ((uint32_t)sizeof h + h.len + 4095u) / 4096u;
    if (sectors > MOT_FLASH_SECTORS)
        return 0;
    h.sum = sum_bytes(2166136261u, mot_n, SONG_BARS);
    for (b = 0; b < SONG_BARS; b++)
        for (i = 0; i < mot_n[b]; i++)
            h.sum = sum_bytes(h.sum, (const uint8_t *)at(b, i), (uint32_t)sizeof(mot_ev_t));
    for (sc = 0; sc < sectors; sc++)
        if (plat_data_erase(MOT_FLASH + sc * 4096u) != 0)
            return 0;
    off = MOT_FLASH + (uint32_t)sizeof h;
    if (plat_data_write(off, mot_n, SONG_BARS) != 0)
        return 0;
    off += SONG_BARS;
    for (b = 0; b < SONG_BARS; b++)              /* a chunk's run at a time */
        for (i = 0; i < mot_n[b]; i += MOT_CHUNK) {
            uint32_t k = mot_n[b] - i < MOT_CHUNK ? mot_n[b] - i : MOT_CHUNK, n = k * (uint32_t)sizeof(mot_ev_t);
            if (plat_data_write(off, at(b, i), n) != 0)
                return 0;
            off += n;
        }
    if (plat_data_write(MOT_FLASH, &h, sizeof h) != 0)   /* last: a save cut short reads as none */
        return 0;
    mot_dirty = 0;
    return 1;
}

void mot_load(void)                              /* (at boot, before the engine runs) */
{
    const uint8_t *p = plat_data(MOT_FLASH);
    mot_head_t h;
    uint32_t b, t = 0, i;
    if (!p)
        return;
    for (i = 0; i < sizeof h; i++)
        ((uint8_t *)&h)[i] = p[i];
    if (h.magic != MOT_MAGIC || h.len < SONG_BARS || (uint32_t)sizeof h + h.len > MOT_FLASH_SECTORS * 4096u
        || sum_bytes(2166136261u, p + sizeof h, h.len) != h.sum)
        return;
    p += sizeof h;
    for (b = 0; b < SONG_BARS; b++)
        t += p[b] <= MOT_BAR_MAX ? p[b] : 100000u;
    if (t > MOT_CHUNKS * MOT_CHUNK || SONG_BARS + t * (uint32_t)sizeof(mot_ev_t) != h.len)
        return;
    reset_all();
    {
        const uint8_t *e = p + SONG_BARS;
        for (b = 0; b < SONG_BARS; b++)
            for (i = 0; i < p[b]; i++, e += sizeof(mot_ev_t)) {
                mot_ev_t m;
                uint32_t k;
                for (k = 0; k < sizeof m; k++)
                    ((uint8_t *)&m)[k] = e[k];
                if (insert(b, m.tick, m.id, m.v) < 0)
                    break;
            }
    }
    mot_dirty = 0;
}
