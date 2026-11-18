// SPDX-License-Identifier: Apache-2.0
/* Snow-ground animals: one visitor at a time, its sprites, run styles,
 * drawing and timing. Scene helpers come from tree_internal.h.
 *
 * Every animal leaves the same way whether tapped or left alone for
 * ANIMAL_IDLE_S: runners dash across to the far side, the groundhog ducks
 * back into its snow hole, which then closes. */
#include "tree_internal.h"
#include <math.h>
#include <string.h>

#define ANIMAL_IDLE_S 12.f
#define GROUNDHOG_RISE_S 0.7f
#define GROUNDHOG_DIVE_S 0.4f
#define GROUNDHOG_HOLE_S 0.8f
enum { ANIMAL_DEPTH = 405, ANIMAL_ROWS_MAX = 5, GROUNDHOG_ROWS = 4, SQUIRREL_TAIL_COLS = 4 };

/* Per-kind run style: ground speed, hop height, hop rhythm, posture lift.
 * The groundhog never runs. */
static const struct {
    float speed, hop, freq, posture;
} ANIMAL_RUN[ANIMAL_KIND_COUNT] = {
    {240, 18, 8, 0}, {210, 32, 4.5f, 0}, {220, 26, 6.5f, 0}, {200, 3, 12, 0},
    {290, 2, 14, 4}, {230, 5, 11, 0}, {190, 4, 10, -2}, {0, 0, 0, 0},
};
/* Front-facing ASCII faces. Each row is filled with the body colour from
 * its first to its last character, so the animal is solid and hides the
 * backdrop; the characters are drawn over that fill. */
static const char *const ANIMAL_SPRITES[ANIMAL_KIND_COUNT][ANIMAL_ROWS_MAX] = {
    [ANIMAL_SQUIRREL] = {" ,-, ^ ^ ", "(   (o.o)", " \\  ( v )", "  '-(\")(\")"},
    [ANIMAL_REINDEER] = {"\\|/ \\|/", " \\(_)/ ", " (o o) ", "  (o)  ", "  | |  "},
    [ANIMAL_RABBIT] = {" (\\(\\", " ( -.-)", "o_(\")(\")"},
    [ANIMAL_CAT_COFFEE] = {" /\\_/\\ ", "( o.o )", " > ^ < ", "  \"_\"~"},
    [ANIMAL_CAT_BLACK] = {" /\\_/\\ ", "( o.o )", " > ^ < ", "  \"_\"~"},
    [ANIMAL_CAT_BLACK_WHITE] = {" /\\_/\\ ", "( o.o )", " > ^ < ", "  \"_\"~"},
    [ANIMAL_CAT_GRAY] = {" /\\_/\\ ", "( o.o )", " > ^ < ", "  \"_\"~"},
    [ANIMAL_GROUNDHOG] = {" .---. ", "( o o )", " (_v_) ", "(     )"},
};
/* fill: body colour, ink: line colour, accent: eyes/nose. */
static const struct {
    unsigned fill, ink, accent;
} ANIMAL_COLORS[ANIMAL_KIND_COUNT] = {
    [ANIMAL_SQUIRREL] = {0xc8702e, 0x5a2a08, 0x5a2a08},
    [ANIMAL_REINDEER] = {0xa06a3a, 0x3e2410, 0xe23b4e},
    [ANIMAL_RABBIT] = {0xf4f6f9, 0x7c8794, 0xe58aa6},
    [ANIMAL_CAT_COFFEE] = {0xb07a45, 0x4a2a10, 0xffe2bd},
    [ANIMAL_CAT_BLACK] = {0x2a2a33, 0x8a8a99, 0xffd94d},
    [ANIMAL_CAT_BLACK_WHITE] = {0x2a2a33, 0x9a9aa8, 0x7fd0ff},
    [ANIMAL_CAT_GRAY] = {0x8a8f98, 0x3c4048, 0xf4f6f8},
    [ANIMAL_GROUNDHOG] = {0x9a6a3c, 0x3a2410, 0x1a1008},
};
#define HOLE_FILL 0x3d5468
#define HOLE_INK 0xd8e8f4

static char mirror_char(char c)
{
    switch (c) {
    case '/': return '\\';
    case '\\': return '/';
    case '(': return ')';
    case ')': return '(';
    case '<': return '>';
    case '>': return '<';
    default: return c;
    }
}
static int sprite_size(const char *const *rows, int *width)
{
    int count = 0;
    *width = 0;
    while (count < ANIMAL_ROWS_MAX && rows[count]) {
        int len = (int)strlen(rows[count]);
        *width = len > *width ? len : *width;
        count++;
    }
    return count;
}
/* One sprite row: fill from its first to last character, ink on top. Rows
 * before skip_cols (the squirrel's tail) are drawn as plain lines. */
static void draw_row(tree_scene_t *s, const animal_t *a, int r, int row_y, int left, int width,
                     bool blink, int target)
{
    const char *line = ANIMAL_SPRITES[a->kind][r];
    int len = (int)strlen(line), first = 0, last = len - 1;
    while (first < len && line[first] == ' ') {
        first++;
    }
    while (last >= 0 && line[last] == ' ') {
        last--;
    }
    bool flip = a->dir < 0;
    for (int c = first; c <= last; ++c) {
        char ch = line[c];
        unsigned fill = ANIMAL_COLORS[a->kind].fill, ink = ANIMAL_COLORS[a->kind].ink;
        uint8_t mode = CELL_FILL;
        if ((a->kind == ANIMAL_REINDEER && r == 0)) {
            mode = CELL_PLAIN; /* Antlers stay open lines. */
            ink = 0xe8cfa0;
        } else if (a->kind == ANIMAL_SQUIRREL && c < SQUIRREL_TAIL_COLS) {
            if (ch == ' ') {
                continue; /* A slim line tail, not a solid block. */
            }
            mode = CELL_PLAIN;
            ink = fill;
        }
        if (a->kind == ANIMAL_CAT_BLACK_WHITE && r >= 2) {
            fill = 0xf4f6f9; /* White chest and paws. */
            ink = 0x2a2a33;
        }
        bool face_row = r == 1 || (a->kind == ANIMAL_REINDEER && r == 2);
        if (ch == 'o' && face_row && blink) {
            ch = '-';
        } else if ((ch == 'o' || ch == '.') && face_row && a->kind != ANIMAL_REINDEER) {
            ink = ANIMAL_COLORS[a->kind].accent;
        } else if (a->kind == ANIMAL_REINDEER && r == 3 && ch == 'o') {
            ink = ANIMAL_COLORS[a->kind].accent; /* Warm nose. */
        } else if (a->kind == ANIMAL_RABBIT && r == 2 && ch == 'o') {
            ink = ANIMAL_COLORS[a->kind].accent; /* Cotton tail. */
        }
        int col = flip ? left + width - 1 - c : left + c;
        tree_place_ex(s, col * CELL_W + CELL_W / 2, row_y, ANIMAL_DEPTH,
                      tree_glyph_of(flip ? mirror_char(ch) : ch), tree_shade(ink, 1), target, mode,
                      tree_shade(fill, 1));
    }
}
/* The groundhog's head rises out of a snow hole, peeks, and ducks back in;
 * the empty hole lingers briefly. Rows below the hole stay hidden. */
static void groundhog_draw(tree_scene_t *s, const animal_t *a, bool blink, int target)
{
    int hole_row = (int)(a->ground_y / CELL_H);
    int centre = (int)(a->x / CELL_W);
    float rise = a->fleeing ? 1 - a->phase / GROUNDHOG_DIVE_S : a->idle_time / GROUNDHOG_RISE_S;
    int shown = (int)lroundf(fminf(1, fmaxf(0, rise)) * GROUNDHOG_ROWS);
    for (int c = -2; c <= 2; ++c) {
        tree_place_ex(s, (centre + c) * CELL_W + CELL_W / 2, hole_row * CELL_H + CELL_H / 2,
                      ANIMAL_DEPTH, c == -2 ? G_LPAR : c == 2 ? G_RPAR : G_UNDER,
                      tree_shade(HOLE_INK, 1), target, CELL_FILL, tree_shade(HOLE_FILL, 1));
    }
    int width = 0;
    (void)sprite_size(ANIMAL_SPRITES[a->kind], &width);
    for (int r = 0; r < shown; ++r) {
        int row = hole_row - shown + r;
        draw_row(s, a, r, row * CELL_H + CELL_H / 2, centre - width / 2, width, blink, target);
    }
}
void tree_animal_draw(tree_scene_t *s)
{
    const animal_t *a = &s->animal;
    if (!a->active) {
        return;
    }
    const bool run = a->fleeing;
    int target = a->fleeing ? 0 : ANIMAL_TARGET;
    /* Idle animals blink now and then: eyes 'o' close to '-'. */
    bool blink = !run && fmodf(s->time + a->kind * 0.7f, 3.3f) < 0.16f;
    if (a->kind == ANIMAL_GROUNDHOG) {
        groundhog_draw(s, a, blink, target);
        return;
    }
    const float hop = run ? ANIMAL_RUN[a->kind].hop : 0;
    const float freq = run ? ANIMAL_RUN[a->kind].freq : 0;
    /* Squirrel, reindeer and rabbit bound in arcs with ground contact
     * between hops; cats run low with a quick trot bob. */
    bool hopper = a->kind <= ANIMAL_RABBIT;
    float bob = !run ? sinf(s->time * 2 + a->kind) * 1.5f
                : hopper ? -fmaxf(0, sinf(a->phase * freq)) * hop
                : -fabsf(sinf(a->phase * freq)) * hop;
    int width = 0;
    int count = sprite_size(ANIMAL_SPRITES[a->kind], &width);
    int left = (int)(a->x / CELL_W) - width / 2;
    int top = (int)((a->ground_y + bob + ANIMAL_RUN[a->kind].posture * (run ? 1 : 0)) / CELL_H)
              - count;
    for (int r = 0; r < count; ++r) {
        draw_row(s, a, r, (top + r) * CELL_H + CELL_H / 2, left, width, blink, target);
    }
}
bool tree_animal_tap(tree_scene_t *s)
{
    animal_t *a = &s->animal;
    if (!a->active || a->fleeing) {
        return false;
    }
    a->fleeing = true;
    a->phase = 0;
    a->dir = a->x < 240 ? 1 : -1; /* Runners dash across to the far side. */
    return true;
}
static void animal_done(tree_scene_t *s)
{
    unsigned n = tree_hash((unsigned)(s->time * 1000) + 31);
    s->animal.active = false;
    s->animal_timer = 20 + n % 26; /* Next visit in about 20 to 45 s. */
}
void tree_animal_update(tree_scene_t *s, float dt)
{
    animal_t *a = &s->animal;
    if (!a->active) {
        if (!s->pressed && (s->animal_timer -= dt) <= 0) {
            unsigned n = tree_hash((unsigned)(s->time * 1000) + 77);
            *a = (animal_t) {
                /* Spawn on the open snow to either side of the trunk. */
                (n & 64) ? 300 + (n >> 5) % 110 : 65 + (n >> 5) % 110,
                414 + (n >> 11) % 16, 0, 0,
                (uint8_t)(n % ANIMAL_KIND_COUNT), 1, true, false
            };
        }
        return;
    }
    a->phase += dt;
    if (!a->fleeing) {
        if ((a->idle_time += dt) > ANIMAL_IDLE_S) {
            (void)tree_animal_tap(s); /* Left alone too long: same exit. */
        }
        return;
    }
    if (a->kind == ANIMAL_GROUNDHOG) {
        if (a->phase > GROUNDHOG_DIVE_S + GROUNDHOG_HOLE_S) {
            animal_done(s);
        }
        return;
    }
    a->x += a->dir * ANIMAL_RUN[a->kind].speed * dt;
    if (a->x < -48 || a->x > 528) {
        animal_done(s);
    }
}
