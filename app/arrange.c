/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dubwise FM contributors */
/* The ARRANGE page (HOME on the DUB page; spec 4.2), first version, P1.7: scenes.
 *   White 1-16          recall that scene: on the line, as a mute would (PLAY held: now)
 *   SAVE held + white   store the desk (and the patterns, by the scope) into that scene
 *   KNOB 4              the scope a store takes: DESK, or DESK + PATTERNS ("P")
 * The song (black keys: sections; KNOB 1-3) comes with P1.8. Part of the unity build: dub.c includes it. */

static struct {
    int32_t cur;                         /* the scene last recalled, -1 none */
    uint8_t scope;                       /* the next store's */
    uint32_t sig;
} arr = {-1, SCOPE_DESK, 0};

static void arr_mark(void) { arr.sig = 0; }

static void arr_scene_name(uint32_t s, char *t)
{
    char n[8];
    t[0] = 0;
    text_cat(t, "SCENE ");
    int_text((int32_t)s + 1, n);
    text_cat(t, n);
}

static void arr_store(uint32_t s)
{
    char t[16];
    ctl_scene_store(s, arr.scope);
    scenes_dirty |= (uint16_t)(1u << s);
    set_dirty = 1;
    arr_scene_name(s, t);
    app_say(t, arr.scope == SCOPE_PATTERNS ? "STORED WITH PATTERNS" : "STORED");
}

static void arr_recall(uint32_t s, uint32_t beats)
{
    char t[16];
    arr_scene_name(s, t);
    if (!scenes[s].used) {
        app_say(t, "EMPTY");
        return;
    }
    ctl_scene_recall(s, beats);
    arr.cur = (int32_t)s;
    if (st.sig)
        st.sig = 0;                      /* (the STEPS grid shows the patterns) */
    app_say(t, scenes[s].scope == SCOPE_PATTERNS ? "WITH PATTERNS" : "");
}

static void arr_step(int32_t d)          /* PRESET on the DUB page: the next stored scene up or down, cued */
{
    int32_t k, s = arr.cur;
    for (k = 0; k < NSCENES; k++) {
        s = (s + (d > 0 ? 1 : -1) + NSCENES) % NSCENES;
        if (scenes[s].used) {
            arr_recall((uint32_t)s, act_beats());
            return;
        }
    }
    app_say("SCENES", "NONE STORED");
}

static void arr_keys(uint32_t keys, uint32_t prev)
{
    uint32_t k;
    for (k = 0; k < 16u; k++)
        if ((keys >> WK[k] & 1u) && !(prev >> WK[k] & 1u)) {
            if (dub.btn >> B_SAVE & 1u) {
                dub.save_used = 1;
                arr_store(k);
            } else
                arr_recall(k, act_beats());
        }
}

static void arr_knob(uint32_t i, int32_t e)
{
    if (i == 3u) {
        arr.scope = (uint8_t)((arr.scope + (e > 0 ? 1 : SCOPE_N - 1)) % SCOPE_N);
        app_say("SCOPE", arr.scope == SCOPE_PATTERNS ? "DESK AND PATTERNS" : "DESK");
    } else
        app_say("SONG", "TO COME");
}

static uint32_t arr_lights(uint32_t *dim)
{
    uint32_t k, bright = 0;
    for (k = 0; k < 16u; k++) {
        uint32_t bit = 1u << WK[k];
        if ((int32_t)k == arr.cur && scenes[k].used)
            bright |= cued_mask() && blink_on() ? 0u : bit;
        else if (scenes[k].used)
            *dim |= bit;
    }
    return bright;
}

static uint32_t arr_sigf(void)
{
    uint32_t h = (uint32_t)(arr.cur + 1) * 7u + arr.scope * 131u + (cued_mask() ? 1031u * (1u + blink_on()) : 0u), k;
    for (k = 0; k < NSCENES; k++)
        h = h * 3u + scenes[k].used + scenes[k].scope * 2u;
    return h;
}

static void arr_draw(int all)
{
    uint32_t k, sig = arr_sigf();
    char t[8];
    if (!all && sig == arr.sig)
        return;
    arr.sig = sig;
    cv_begin(240, PAGE_H, C_BLACK);
    cv_text(6, 2, &FONT_XS, "SCENES", C_GRAY);
    if (cued_mask())
        cv_text(70, 2, &FONT_XS, "CUED", C_WHITE);
    for (k = 0; k < NSCENES; k++) {             /* 4 x 4: number, P for patterns, the last recalled lit */
        int32_t x = 6 + (int32_t)(k % 8u) * 29, y = 22 + (int32_t)(k / 8u) * 56;
        int used = scenes[k].used, cur = (int32_t)k == arr.cur;
        cv_rect(x, y, 26, 48, cur && used ? C_HI : used ? C_LINE : C_BLACK);
        if (!used) {
            cv_rect(x, y, 26, 1, C_LINE);
            cv_rect(x, y + 47, 26, 1, C_LINE);
        }
        int_text((int32_t)k + 1, t);
        cv_text(x + (26 - text_w(&FONT_B, t)) / 2, y + 6, &FONT_B, t, cur && used ? C_BLACK : used ? C_WHITE : C_DIM);
        if (used && scenes[k].scope == SCOPE_PATTERNS)
            cv_text(x + (26 - text_w(&FONT_XS, "P")) / 2, y + 28, &FONT_XS, "P", cur ? C_BLACK : C_AMB);
    }
    cv_blit(0, STRIP_Y);
    lcd_fill(0, STRIP_Y + PAGE_H, 240, FOOT_Y - STRIP_Y - PAGE_H, C_BLACK);
    cv_begin(240, FOOT_H, C_BLACK);
    cv_rect(0, 0, 240, 1, C_LINE);
    cv_text(6, 4, &FONT_XS, "SAVE + KEY: STORE", C_GRAY);
    cv_text(6, 18, &FONT_XS, "KEY: RECALL", C_GRAY);
    cv_text(150, 4, &FONT_XS, "K4 SCOPE", C_GRAY);
    cv_text(150, 18, &FONT_XS, arr.scope == SCOPE_PATTERNS ? "DESK+PAT" : "DESK", C_HI);
    cv_blit(0, FOOT_Y);
}

#ifdef PLAT_HOST
static int arr_expect(const char *what, int *got)
{
    if (!strcmp(what, "scene"))          /* the scene last recalled (1-16), 0 none */
        *got = arr.cur + 1;
    else if (!strncmp(what, "scene_used", 10))
        *got = scenes[(atoi(what + 10) - 1) % NSCENES].used;
    else
        return 0;
    return 1;
}
#endif
