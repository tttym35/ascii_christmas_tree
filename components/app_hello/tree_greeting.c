// SPDX-License-Identifier: Apache-2.0
#include "tree_greeting.h"
#include <math.h>
#include <stddef.h>

enum { SCALE = 2, GLYPH_W = 6, GLYPH_H = 10, ADVANCE = GLYPH_W * SCALE + 3 };
enum { GLINT_WIDTH = 7, GLINT_SPEED = 170, GLINT_GAP = 420 };

static const char TEXT[] = "MERRY CHRISTMAS MOSAICO";
#define TEXT_LEN ((int)(sizeof(TEXT) - 1))
#define TEXT_W (TEXT_LEN * ADVANCE - 3)
#define TEXT_X ((480 - TEXT_W) / 2)

/* Bold capitals with 2-pixel strokes; bit 5 is the left column. */
static const struct {
    char ch;
    uint8_t rows[GLYPH_H];
} LETTERS[] = {
    {'M', {0x21, 0x33, 0x3f, 0x3f, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33}},
    {'E', {0x3f, 0x3f, 0x30, 0x30, 0x3e, 0x3e, 0x30, 0x30, 0x3f, 0x3f}},
    {'R', {0x3e, 0x3f, 0x33, 0x33, 0x3f, 0x3e, 0x3c, 0x36, 0x33, 0x33}},
    {'Y', {0x33, 0x33, 0x33, 0x3f, 0x1e, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c}},
    {'C', {0x1f, 0x3f, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x3f, 0x1f}},
    {'H', {0x33, 0x33, 0x33, 0x33, 0x3f, 0x3f, 0x33, 0x33, 0x33, 0x33}},
    {'I', {0x3f, 0x3f, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x3f, 0x3f}},
    {'S', {0x1f, 0x3f, 0x30, 0x3e, 0x1f, 0x03, 0x03, 0x03, 0x3f, 0x3e}},
    {'T', {0x3f, 0x3f, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c}},
    {'A', {0x1e, 0x3f, 0x33, 0x33, 0x3f, 0x3f, 0x33, 0x33, 0x33, 0x33}},
    {'O', {0x1e, 0x3f, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x3f, 0x1e}},
};
/* Solid foil gold, ringed by a 1 px fir-green outline so it reads on snow. */
#define GOLD_RGB 0xf2c14e
#define OUTLINE_RGB 0x143a26
#define GLINT_RGB 0xfffbea

static uint16_t rgb565(uint32_t rgb)
{
    return (uint16_t)(((rgb >> 19) & 0x1f) << 11 | ((rgb >> 10) & 0x3f) << 5 | ((rgb >> 3) & 0x1f));
}
static const uint8_t *letter(char ch)
{
    for (unsigned i = 0; i < sizeof(LETTERS) / sizeof(LETTERS[0]); ++i) {
        if (LETTERS[i].ch == ch) {
            return LETTERS[i].rows;
        }
    }
    return NULL;
}
/* True if text-local (lx, ly) is a stroke pixel. */
static bool stroke_at(int lx, int ly)
{
    if (lx < 0 || ly < 0 || lx >= TEXT_W || ly >= GLYPH_H * SCALE) {
        return false;
    }
    int index = lx / ADVANCE, cx = lx % ADVANCE;
    if (cx >= GLYPH_W * SCALE) {
        return false;
    }
    const uint8_t *rows = letter(TEXT[index]);
    int gx = cx / SCALE, gy = ly / SCALE;
    return rows && (rows[gy] >> (GLYPH_W - 1 - gx) & 1);
}
/* The text never changes, so its pixel map is built once: per pixel a
 * stroke, its 4-neighbour outline, or nothing. Only the render task calls this. */
enum { MAP_NONE, MAP_STROKE, MAP_OUTLINE };
enum { MAP_W = TEXT_W + 2, MAP_H = GLYPH_H * SCALE + 2 };
static uint8_t map[MAP_H][MAP_W];
static bool map_ready;
static void build_map(void)
{
    for (int my = 0; my < MAP_H; ++my) {
        for (int mx = 0; mx < MAP_W; ++mx) {
            int lx = mx - 1, ly = my - 1;
            if (stroke_at(lx, ly)) {
                map[my][mx] = MAP_STROKE;
            } else if (stroke_at(lx - 1, ly) || stroke_at(lx + 1, ly)
                       || stroke_at(lx, ly - 1) || stroke_at(lx, ly + 1)) {
                map[my][mx] = MAP_OUTLINE;
            } else {
                map[my][mx] = MAP_NONE;
            }
        }
    }
    map_ready = true;
}
bool tree_greeting_pixel(int x, int y, float time, uint16_t *out)
{
    int mx = x - TEXT_X + 1, my = y - GREETING_Y + 1;
    if (mx < 0 || mx >= MAP_W || my < 0 || my >= MAP_H) {
        return false;
    }
    if (!map_ready) {
        build_map();
    }
    int kind = map[my][mx];
    if (kind == MAP_NONE) {
        return false;
    }
    if (kind == MAP_OUTLINE) {
        *out = rgb565(OUTLINE_RGB);
        return true;
    }
    /* A slanted glint sweeps left to right, then rests off-text. */
    float sweep = fmodf(time * GLINT_SPEED, (float)(TEXT_W + GLINT_GAP)) - 20;
    float d = fabsf((mx - 1) + (my - 1) * 0.6f - sweep);
    *out = rgb565(d < GLINT_WIDTH ? GLINT_RGB : GOLD_RGB);
    return true;
}
