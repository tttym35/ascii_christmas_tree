// SPDX-License-Identifier: Apache-2.0
#include "tree_scene.h"
#include "tree_greeting.h"
#include "tree_internal.h"
#ifdef ESP_PLATFORM
#include "esp_timer.h"
#include "esp_log.h"
#endif
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define TAU 6.2831853f
#define REST_SPEED 0.31f
/* Backdrop: flat night sky over a flat moonlit snow drift (no gradient). */
#define SKY_RGB 0x0b1d33
#define SNOW_GROUND_RGB 0x9db7cf
enum { HORIZON_BASE = 412, HORIZON_MOUND = 40 };
/* Treetop star sprite placement (see star_draw). */
enum { STAR_COL0 = 24, STAR_ROW0 = 1, STAR_DEPTH = 395, STAR_SPRITE_CENTRE = 6 };
#define STAR_EDGE_ON 0.2f
/* Star centre on screen, for effects that aim at the treetop. */
enum { STAR_X = (STAR_COL0 + STAR_SPRITE_CENTRE) * CELL_W + CELL_W / 2, STAR_Y = 54 };
enum { STAR = 1, RED_BOX, BLUE_BOX };
static const uint8_t GLYPHS[G_COUNT][10] = {
    [G_BLANK] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    [G_V] = {0, 0, 0b100001, 0b100001, 0b100001, 0b010010, 0b010010, 0b001100, 0b001100, 0},
    [G_Y] = {0b100001, 0b100001, 0b100001, 0b010010, 0b001100, 0b001100, 0b001100, 0b001100, 0b001100, 0},
    [G_PLUS] = {0, 0b001100, 0b001100, 0b001100, 0b011110, 0b011110, 0b001100, 0b001100, 0b001100, 0},
    [G_DOT] = {0, 0, 0, 0, 0, 0, 0, 0b001100, 0b001100, 0},
    [G_COLON] = {0, 0, 0b001100, 0b001100, 0, 0, 0b001100, 0b001100, 0, 0},
    [G_AST] = {0, 0b001100, 0b001100, 0b101101, 0b011110, 0b011110, 0b101101, 0b001100, 0b001100, 0},
    [G_SLASH] = {0, 0b000011, 0b000110, 0b001100, 0b001100, 0b011000, 0b011000, 0b110000, 0b110000, 0},
    [G_BACK] = {0, 0b110000, 0b011000, 0b001100, 0b001100, 0b000110, 0b000110, 0b000011, 0b000011, 0},
    [G_BAR] = {0b001100, 0b001100, 0b001100, 0b001100, 0b001100, 0b001100, 0b001100, 0b001100, 0b001100, 0},
    [G_HASH] = {0b010010, 0b010010, 0b010010, 0b111111, 0b010010, 0b010010, 0b111111, 0b010010, 0b010010, 0},
    [G_EQ] = {0, 0, 0b011110, 0b011110, 0, 0, 0b011110, 0b011110, 0, 0},
    [G_O] = {0b011110, 0b100001, 0b100001, 0b100001, 0b100001, 0b100001, 0b100001, 0b100001, 0b011110, 0},
    [G_L] = {0b110000, 0b110000, 0b110000, 0b110000, 0b110000, 0b110000, 0b110000, 0b110000, 0b111111, 0},
    [G_E] = {0b111111, 0b110000, 0b110000, 0b111110, 0b111110, 0b110000, 0b110000, 0b111111, 0b111111, 0},
    [G_T] = {0b111111, 0b111111, 0b001100, 0b001100, 0b001100, 0b001100, 0b001100, 0b001100, 0b001100, 0},
    [G_I] = {0b111111, 0b111111, 0b001100, 0b001100, 0b001100, 0b001100, 0b001100, 0b111111, 0b111111, 0},
    [G_N] = {0b110011, 0b110011, 0b111011, 0b111011, 0b110111, 0b110111, 0b110011, 0b110011, 0b110011, 0},
    [G_S] = {0b011111, 0b110000, 0b110000, 0b011110, 0b000011, 0b000011, 0b100001, 0b100001, 0b111110, 0},
    [G_W] = {0b100001, 0b100001, 0b100001, 0b100001, 0b100001, 0b101101, 0b101101, 0b110011, 0b010010, 0},
    /* ASCII punctuation and lower case for the snow-ground animals. */
    [G_LPAR] = {0b000110, 0b001100, 0b011000, 0b011000, 0b011000, 0b011000, 0b011000, 0b001100, 0b000110, 0},
    [G_RPAR] = {0b011000, 0b001100, 0b000110, 0b000110, 0b000110, 0b000110, 0b000110, 0b001100, 0b011000, 0},
    [G_UNDER] = {0, 0, 0, 0, 0, 0, 0, 0, 0b111111, 0b111111},
    [G_CARET] = {0, 0b001100, 0b011110, 0b110011, 0b100001, 0, 0, 0, 0, 0},
    [G_GT] = {0, 0b110000, 0b011000, 0b001100, 0b000110, 0b001100, 0b011000, 0b110000, 0, 0},
    [G_LT] = {0, 0b000011, 0b000110, 0b001100, 0b011000, 0b001100, 0b000110, 0b000011, 0, 0},
    [G_DQUOTE] = {0b110110, 0b110110, 0b100100, 0, 0, 0, 0, 0, 0, 0},
    [G_QUOTE] = {0b001100, 0b001100, 0b001000, 0, 0, 0, 0, 0, 0, 0},
    [G_COMMA] = {0, 0, 0, 0, 0, 0, 0, 0b001100, 0b001100, 0b011000},
    [G_DASH] = {0, 0, 0, 0, 0b111111, 0b111111, 0, 0, 0, 0},
    [G_TILDE] = {0, 0, 0, 0b011001, 0b110111, 0b100110, 0, 0, 0, 0},
    [G_LO] = {0, 0, 0, 0b011110, 0b110011, 0b110011, 0b110011, 0b011110, 0, 0},
    [G_LV] = {0, 0, 0, 0b110011, 0b110011, 0b011110, 0b001100, 0, 0, 0},
    [G_P] = {0b111110, 0b111111, 0b110011, 0b110011, 0b111111, 0b111110, 0b110000, 0b110000, 0b110000, 0},
    [G_EXCL] = {0b001100, 0b001100, 0b001100, 0b001100, 0b001100, 0b001100, 0, 0b001100, 0b001100, 0},
};
/* Map the ASCII characters used by sprite strings to font glyphs. */
static int glyph_of(char c)
{
    switch (c) {
    case '.': return G_DOT;
    case ':': return G_COLON;
    case '*': return G_AST;
    case '+': return G_PLUS;
    case '/': return G_SLASH;
    case '\\': return G_BACK;
    case '|': return G_BAR;
    case '#': return G_HASH;
    case '=': return G_EQ;
    case 'O': return G_O;
    case '(': return G_LPAR;
    case ')': return G_RPAR;
    case '_': return G_UNDER;
    case '^': return G_CARET;
    case '>': return G_GT;
    case '<': return G_LT;
    case '"': return G_DQUOTE;
    case '\'': return G_QUOTE;
    case ',': return G_COMMA;
    case '-': return G_DASH;
    case '~': return G_TILDE;
    case 'o': return G_LO;
    case 'v': return G_LV;
    default: return G_BLANK;
    }
}
static unsigned hash(unsigned n)
{
    n ^= n >> 16;
    n *= 0x7feb352du;
    n ^= n >> 15;
    return n;
}
static uint16_t shade(unsigned rgb, float light)
{
    int r = fminf(255, (rgb >> 16) * light), g = fminf(255, ((rgb >> 8) & 255) * light);
    int b = fminf(255, (rgb & 255) * light);
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}
static unsigned scale_rgb(unsigned rgb, float light)
{
    unsigned r = fminf(255, (rgb >> 16) * light), g = fminf(255, ((rgb >> 8) & 255) * light);
    unsigned b = fminf(255, (rgb & 255) * light);
    return (r << 16) | (g << 8) | b;
}
/* Mix an RGB colour toward white by k (0 keeps it, 1 is white). */
static unsigned tint(unsigned rgb, float k)
{
    unsigned r = rgb >> 16, g = (rgb >> 8) & 255, b = rgb & 255;
    r += (unsigned)((255 - r) * k);
    g += (unsigned)((255 - g) * k);
    b += (unsigned)((255 - b) * k);
    return (r << 16) | (g << 8) | b;
}
static void place_ex(tree_scene_t *s, int x, int y, float z, int glyph, uint16_t color,
                     int target, uint8_t mode, uint16_t extra)
{
    if (x < 0 || x >= 480 || y < 0 || y >= 480) {
        return;
    }
    tree_cell_t *cell = &s->cells[y / CELL_H][x / CELL_W];
    if (z < cell->depth) {
        return;
    }
    *cell = (tree_cell_t) {
        (int16_t)z, color, (uint16_t)glyph, extra, (int8_t)target, mode
    };
}
static void place(tree_scene_t *s, int x, int y, float z, int glyph, uint16_t color, int target)
{
    place_ex(s, x, y, z, glyph, color, target, CELL_PLAIN, 0);
}
/* Shared with the other scene modules through tree_internal.h. */
unsigned tree_hash(unsigned n)
{
    return hash(n);
}
uint16_t tree_shade(unsigned rgb, float light)
{
    return shade(rgb, light);
}
void tree_place_ex(tree_scene_t *s, int x, int y, float z, int glyph, uint16_t color, int target,
                   uint8_t mode, uint16_t extra)
{
    place_ex(s, x, y, z, glyph, color, target, mode, extra);
}
int tree_glyph_of(char c)
{
    return glyph_of(c);
}
typedef struct {
    tree_scene_t *s;
    float co, si;
} view_t;
/* Project a model point and place it; mode/extra as in place_ex, with
 * `extra` given as RGB and lit like the glyph. */
static void point_ex(view_t v, float x, float y, float z, int glyph, unsigned rgb, float light,
                     int target, uint8_t mode, unsigned extra)
{
    float depth = z * v.co + x * v.si;
    float lit = light * (0.86f + depth * 0.0015f);
    place_ex(v.s, 240 + x * v.co - z * v.si, y + depth * 0.14f, depth + 200, glyph,
             shade(rgb, lit), target, mode, shade(extra, lit));
}
static void point(view_t v, float x, float y, float z, int glyph, unsigned rgb, float light,
                  int target)
{
    point_ex(v, x, y, z, glyph, rgb, light, target, CELL_PLAIN, 0);
}
static void material_spawn(tree_scene_t *s, int idx, unsigned seed);
static void snow_burst(tree_scene_t *s);
static float radius(int layer, float h, float theta)
{
    return (42 + layer * 31) * (0.08f + 0.92f * h) * (1 + 0.085f * cosf(7 * theta + layer));
}
static void add_point(tree_point_t *array, unsigned *count, unsigned capacity, float x, float y,
                      float z, int glyph, unsigned rgb)
{
    if (*count >= capacity) {
        return;
    }
    array[(*count)++] = (tree_point_t) {
        (int16_t)lroundf(x), (int16_t)lroundf(y),
        (int16_t)lroundf(z), (uint16_t)glyph, rgb
    };
}
static void build_box(tree_scene_t *s, float cx, float cz, int half, int height, unsigned color)
{
    for (int x = -half; x <= half; x += 7)
        for (int z = -half; z <= half; z += 7) {
            bool ribbon = abs(x) < 4 || abs(z) < 4;
            add_point(s->box_lid, &s->box_lid_count, BOX_LID_CAPACITY, cx + x, 376 - height,
                      cz + z, ribbon ? G_PLUS : G_EQ, ribbon ? 0xffdd77 : color);
        }
    for (int side = -1; side <= 1; side += 2)
        for (int i = -half; i <= half; i += 7)
            for (int y = 0; y < height; y += 7) {
                bool ribbon = abs(i) < 4;
                unsigned col = ribbon ? 0xffd57d : color;
                add_point(s->box_body, &s->box_body_count, BOX_BODY_CAPACITY, cx + i, 375 - y,
                          cz + side * half, ribbon ? G_BAR : G_HASH, scale_rgb(col, 0.9f));
                add_point(s->box_body, &s->box_body_count, BOX_BODY_CAPACITY, cx + side * half,
                          375 - y, cz + i, ribbon ? G_BAR : G_EQ, scale_rgb(col, 0.65f));
            }
    for (int i = -9; i <= 9; i += 5)
        add_point(s->box_lid, &s->box_lid_count, BOX_LID_CAPACITY, cx + i,
                  369 - height - 3 * cosf(i * 0.6f), cz, G_O, 0xffdc77);
}
/* All fixed model coordinates are built once here. Each frame only rotates,
 * depth-tests and rasterises these points, plus falling snow. */
void tree_init_geometry(tree_scene_t *s)
{
    static const unsigned greens[] = {0x268763, 0x59cc8f, 0x2ca187, 0x86d692};
    static const unsigned jewels[] = {0xff5866, 0x6aaaff, 0xfb63b3, 0xcc97ff};
    s->branch_count = s->trunk_count = s->root_count = 0;
    s->box_body_count = s->box_lid_count = 0;
    s->globe_time = -1;
    s->lamp_sweep = -1;
    s->star_flash = 0;
    s->animal_timer = 10;
    s->rotation_total = 0;
    s->next_refill_rotation = 0;
    for (int i = 0; i < SNOW_CAPACITY; ++i) {
        unsigned n = hash(i + 827);
        /* Mostly '.' and '*'; only the first two start as large flakes. */
        s->snow[i] = (snow_flake_t) {
            (n >> 10) % 480, n % 470, 0, 9, (uint8_t)(i < SNOW_BIG_MAX ? 2 : i % 2)
        };
    }
    for (int layer = 0; layer < 4; ++layer) {
        int rows = 4 + layer;
        for (int row = 0; row < rows; ++row) {
            float h = (row + 0.8f) / rows;
            int count = 3 + (int)((5 + 3 * layer) * h);
            for (int j = 0; j < count; ++j) {
                unsigned seed = hash(layer * 10000 + row * 271 + j);
                float a = TAU * (j + 0.38f * (row % 3)) / count;
                float r = radius(layer, h, a);
                float y = 87 + layer * 49 + h * (72 + layer * 8) + 9 * h * sinf(7 * a + layer);
                bool jewel = seed % 19 == 0;
                add_point(s->branches, &s->branch_count, BRANCH_CAPACITY, r * cosf(a), y,
                          r * sinf(a), jewel ? G_AST : (seed & 1 ? G_Y : G_V),
                          jewel ? jewels[(seed >> 8) % 4] : greens[(seed >> 8) % 4]);
            }
        }
    }
    for (int i = 0; i < LAMP_COUNT; ++i) {
        float h = (float)i / (LAMP_COUNT - 1), a = h * 3 * TAU + 0.8f;
        float y = 97 + 232 * h, r = 0;
        for (int layer = 0; layer < 4; ++layer) {
            float f = (y - 87 - layer * 49) / (72 + layer * 8);
            if (f >= 0 && f <= 1) {
                r = fmaxf(r, radius(layer, f, a));
            }
        }
        r += 5;
        s->lamps[i] = (tree_point_t) {
            (int16_t)lroundf(r * cosf(a)), (int16_t)lroundf(y),
            (int16_t)lroundf(r * sinf(a)),
            (uint16_t)(i % 2 == 0 ? G_AST : G_COLON),
            i % 2 == 0 ? 0xffe598 : 0xe5a94e
        };
    }
    for (int i = 0; i < TREE_MATERIAL_COUNT; ++i) {
        material_spawn(s, i, hash(i * 977 + 41));
    }
    for (int i = 0; i < GIFT_COUNT; ++i) {
        s->gifts[i] = (gift_t) {
            0, 0, -1, MAT_IDLE, true
        };
    }
    for (int y = 312; y < 369; y += 9)
        for (int j = 0; j < 8; j++) {
            float a = TAU * j / 8;
            add_point(s->trunk, &s->trunk_count, TRUNK_CAPACITY, 12 * cosf(a), y, 12 * sinf(a),
                      G_BAR, scale_rgb(0xb57942, 0.72f + 0.25f * cosf(a)));
        }
    for (int j = 0; j < 5; j++)
        for (int i = 0; i < 35; i += 5) {
            float a = TAU * j / 5 + 0.1f;
            add_point(s->roots, &s->root_count, ROOT_CAPACITY, (i + 4) * cosf(a),
                      356 + sqrtf(i) * 3, (i + 4) * sinf(a), G_SLASH,
                      scale_rgb(0xd79854, 0.9f));
        }
    for (int x = 0; x < 480; ++x) {
        /* Moonlit drift: highest under the tree, settling toward the edges. */
        float t = (x - 240) / 240.f;
        s->horizon[x] = (uint16_t)lroundf(HORIZON_BASE - HORIZON_MOUND * expf(-t * t * 3));
    }
    build_box(s, -82, 41, 22, 33, 0xdb4569);
    build_box(s, 77, -10, 16, 26, 0x4f96cb);
}
typedef struct {
    int8_t dx, dy;
    uint8_t glyph;
    uint32_t rgb;
} sprite_cell_t;
enum { MAT_TARGET_BASE = 10, CANE_ROWS = 6 };
#define DIAMOND_FILL 0x2b8fb8
/* Six tree materials redrawn at roughly twice the old sprite size, so each
 * one reads at desktop distance. The stocking keeps a white cuff over a red
 * body; colours are part of the sprite, not a per-instance tint. */
static const sprite_cell_t MATERIAL_SPRITES[6][24] = {
    /* Royal-blue bow, so it no longer reads like the pink bauble. */
    [MAT_BOW] = {{-2, 0, G_BACK, 0x3d7bff}, {-1, 0, G_O, 0x9cc0ff}, {0, 0, G_O, 0x2a5fd6},
        {1, 0, G_O, 0x9cc0ff}, {2, 0, G_SLASH, 0x3d7bff}, {-2, 1, G_SLASH, 0x3d7bff},
        {-1, 1, G_BACK, 0x2a5fd6}, {1, 1, G_SLASH, 0x2a5fd6}, {2, 1, G_BACK, 0x3d7bff},
        {-1, 2, G_SLASH, 0x2a5fd6}, {1, 2, G_BACK, 0x2a5fd6}
    },
    /* Temple bell: narrow knob and shoulders widen row by row to the rim,
     * the widest line, with the clapper showing below it. */
    [MAT_BELL] = {{0, 0, G_DOT, 0xffe9a8}, {-1, 1, G_SLASH, 0xd99a26}, {0, 1, G_EQ, 0xffe9a8},
        {1, 1, G_BACK, 0xd99a26}, {-1, 2, G_HASH, 0xffc93c}, {0, 2, G_HASH, 0xe8aa2e},
        {1, 2, G_HASH, 0xffc93c}, {-2, 3, G_SLASH, 0xd99a26}, {-1, 3, G_HASH, 0xffc93c},
        {0, 3, G_HASH, 0xffe9a8}, {1, 3, G_HASH, 0xffc93c}, {2, 3, G_BACK, 0xd99a26},
        {-2, 4, G_EQ, 0xffe9a8}, {-1, 4, G_EQ, 0xffc93c}, {0, 4, G_EQ, 0xffe9a8},
        {1, 4, G_EQ, 0xffc93c}, {2, 4, G_EQ, 0xffe9a8}, {0, 5, G_DOT, 0xb56a1e}
    },
    /* Candy cane: the shaft cell under the hook start and the filled hook
     * underside keep the bend continuous, with red and white alternating. */
    [MAT_CANE] = {{0, 0, G_EQ, 0xffffff}, {1, 0, G_EQ, 0xff4d5e}, {2, 0, G_EQ, 0xffffff},
        {0, 1, G_BAR, 0xff4d5e}, {1, 1, G_EQ, 0xffffff}, {2, 1, G_BAR, 0xff4d5e},
        {0, 2, G_BAR, 0xffffff}, {0, 3, G_BAR, 0xff4d5e}, {0, 4, G_BAR, 0xffffff},
        {0, 5, G_BAR, 0xff4d5e}
    },
    [MAT_BAUBLE] = {{0, 0, G_PLUS, 0xd9d9d9}, {-1, 1, G_SLASH, 0xfb63b3}, {0, 1, G_O, 0xff8ac2},
        {1, 1, G_BACK, 0xfb63b3}, {-1, 2, G_BACK, 0xfb63b3}, {0, 2, G_O, 0xfb63b3},
        {1, 2, G_SLASH, 0xff8ac2}, {0, 3, G_DOT, 0xffffff}
    },
    /* Diamond (was the gold star): wide flat table, facet cuts, one point. */
    [MAT_STAR] = {{-2, 0, G_EQ, 0x9be8ff}, {-1, 0, G_EQ, 0xffffff}, {0, 0, G_EQ, 0x67d7f2},
        {1, 0, G_EQ, 0xffffff}, {2, 0, G_EQ, 0x9be8ff}, {-2, 1, G_SLASH, 0x67d7f2},
        {-1, 1, G_AST, 0xffffff}, {0, 1, G_BAR, 0xbdf3ff}, {1, 1, G_BACK, 0x67d7f2},
        {2, 1, G_SLASH, 0x9be8ff}, {-1, 2, G_SLASH, 0x9be8ff}, {0, 2, G_AST, 0xffffff},
        {1, 2, G_BACK, 0x9be8ff}, {0, 3, G_DOT, 0x67d7f2}
    },
    /* Stocking, one size up: double white cuff, deep red body, full foot. */
    [MAT_STOCKING] = {{-1, 0, G_EQ, 0xffffff}, {0, 0, G_EQ, 0xffffff}, {1, 0, G_EQ, 0xffffff},
        {-1, 1, G_EQ, 0xffffff}, {0, 1, G_EQ, 0xf2f2f2}, {1, 1, G_EQ, 0xffffff},
        {-1, 2, G_HASH, 0xc21630}, {0, 2, G_BAR, 0xe23b4e}, {-1, 3, G_BAR, 0xe23b4e},
        {0, 3, G_HASH, 0xc21630}, {-1, 4, G_HASH, 0xc21630}, {0, 4, G_BAR, 0xe23b4e},
        {-1, 5, G_HASH, 0xc21630}, {0, 5, G_EQ, 0xe23b4e}, {1, 5, G_EQ, 0xff5a6a},
        {2, 5, G_EQ, 0xc21630}, {0, 6, G_EQ, 0xc21630}, {1, 6, G_EQ, 0xe23b4e},
        {2, 6, G_DOT, 0xff5a6a}
    },
};
static const uint8_t MATERIAL_SPRITE_LEN[6] = {11, 18, 10, 8, 14, 19};
static const float GIFT_BASE_X[GIFT_COUNT] = {-82, 77};
static const float GIFT_BASE_Z[GIFT_COUNT] = {41, -10};
static bool anchor_is_front(tree_scene_t *s, float x, float z)
{
    return z * cosf(s->angle) + x * sinf(s->angle) > 0;
}
static void gift_center(const tree_scene_t *s, int slot, float *cx, float *cz)
{
    *cx = GIFT_BASE_X[slot] + s->gifts[slot].dx;
    *cz = GIFT_BASE_Z[slot] + s->gifts[slot].dz;
}
static void material_spawn(tree_scene_t *s, int idx, unsigned seed)
{
    material_t *m = &s->materials[idx];
    int x = m->x, y = m->y, z = m->z;
    for (int attempt = 0; attempt < 32; ++attempt) {
        unsigned n = hash(seed + attempt * 7919u);
        const tree_point_t *b = &s->branches[n % s->branch_count];
        x = (int)lroundf(b->x * 1.06f);
        y = b->y + 5;
        z = (int)lroundf(b->z * 1.06f);
        bool ok = true;
        for (int j = 0; j < TREE_MATERIAL_COUNT && ok; ++j) {
            if (j == idx) {
                continue;
            }
            const material_t *o = &s->materials[j];
            float da = fabsf(remainderf(atan2f(z, x) - atan2f(o->z, o->x), TAU));
            float dx = x - o->x, dy = y - o->y, dz = z - o->z;
            /* Keep materials apart in angle and height, and never stacked. */
            if ((da < 0.6f && fabsf(dy) < 55) || dx * dx + dy * dy + dz * dz < 1600) {
                ok = false;
            }
        }
        if (ok) {
            break;
        }
    }
    unsigned n = hash(seed * 31u + 7u);
    *m = (material_t) {
        (int16_t)x, (int16_t)y, (int16_t)z, (uint8_t)(n % 6), MAT_IDLE,
        anchor_is_front(s, x, z)
    };
}
static void gift_spawn(tree_scene_t *s, int slot, unsigned seed)
{
    float cx = GIFT_BASE_X[slot], cz = GIFT_BASE_Z[slot];
    for (int attempt = 0; attempt < 24; ++attempt) {
        unsigned n = hash(seed + attempt * 104729u);
        float a = (n % 628) / 100.f, r = 80 + (n >> 8) % 26;
        cx = r * cosf(a);
        cz = r * sinf(a);
        float ox, oz;
        gift_center(s, 1 - slot, &ox, &oz);
        float dx = cx - ox, dz = cz - oz;
        if (dx * dx + dz * dz > 75 * 75) {
            break;
        }
    }
    gift_t *g = &s->gifts[slot];
    g->dx = cx - GIFT_BASE_X[slot];
    g->dz = cz - GIFT_BASE_Z[slot];
    g->exit_time = -1;
    g->state = MAT_IDLE;
    g->was_front = anchor_is_front(s, cx, cz);
}
static effect_t *effect_for(tree_scene_t *s, int kind, int index)
{
    for (int i = 0; i < EFFECT_CAPACITY; ++i)
        if (s->effects[i].active && s->effects[i].kind == kind && s->effects[i].index == index) {
            return &s->effects[i];
        }
    return NULL;
}
static void particle_spawn(tree_scene_t *s, float x, float y, float vx, float vy, float life,
                           float gravity, int glyph, unsigned rgb, int bounces)
{
    for (int i = 0; i < PARTICLE_CAPACITY; ++i) {
        particle_t *p = &s->particles[i];
        if (p->life > 0) {
            continue;
        }
        *p = (particle_t) {
            x, y, vx, vy, life, life, gravity, rgb, (uint16_t)glyph, (uint8_t)bounces
        };
        return;
    }
}
#define CANE_TIME 1.9f
#define BAUBLE_TIME 0.4f
#define BALLOON_TIME 3.6f
/* Red gift rocket climb; sfx_red_gift.wav and the motor bang match it. */
#define ROCKET_TIME 1.8f
#define CANE_GRAVITY 900.f
/* Candy-cane drop: falls from its branch to the snow, bounces twice with
 * halving height, then rests. Returns the downward offset at time t. */
static float cane_drop(float h0, float t)
{
    if (h0 <= 0) {
        return 0;
    }
    float t1 = sqrtf(2 * h0 / CANE_GRAVITY);
    if (t < t1) {
        return 0.5f * CANE_GRAVITY * t * t;
    }
    t -= t1;
    float v = sqrtf(2 * CANE_GRAVITY * h0) * 0.5f;
    for (int k = 0; k < 2; ++k, v *= 0.5f) {
        float span = 2 * v / CANE_GRAVITY;
        if (t < span) {
            return h0 - (v * t - 0.5f * CANE_GRAVITY * t * t);
        }
        t -= span;
    }
    return h0;
}
static void particles_burst(tree_scene_t *s, int kind, float ox, float oy, unsigned seed)
{
    static const unsigned bauble_colors[] = {0xfb63b3, 0x6aaaff, 0xffd94d, 0x86d692, 0xcc97ff};
    if (kind == TREE_EFFECT_BOW) {
        /* Confetti: a big burst of coloured paper scraps that flutter down. */
        static const int paper[] = {G_PLUS, G_AST, G_EQ, G_SLASH, G_BACK, G_DOT};
        for (int i = 0; i < 44; ++i) {
            unsigned n = hash(seed + i * 3571u);
            float a = (n % 628) / 100.f, sp = 55 + (n >> 8) % 125;
            particle_spawn(s, ox, oy, cosf(a) * sp, sinf(a) * sp - 45, 1.7f, 95,
                           paper[n % 6], bauble_colors[(n >> 5) % 5], 0);
        }
        return;
    }
    if (kind == TREE_EFFECT_STAR) {
        /* Diamond starburst at the treetop: eight ASCII light rays shoot out
         * in three waves, then prismatic twinkles float down. */
        static const int rays[8] = {G_DASH, G_BACK, G_BAR, G_SLASH, G_DASH, G_BACK, G_BAR, G_SLASH};
        static const unsigned prism[] = {0xffffff, 0x9be8ff, 0xff8ac2, 0xffd94d, 0xcc97ff};
        for (int d = 0; d < 8; ++d) {
            float a = d * TAU / 8;
            for (int w = 0; w < 5; ++w) {
                /* Five speeds per ray keep a solid streak as it flies out. */
                float sp = 90 + w * 60;
                particle_spawn(s, ox, oy, cosf(a) * sp, sinf(a) * sp, 0.85f, 0, rays[d],
                               w % 2 ? 0x9be8ff : 0xffffff, 0);
            }
        }
        for (int k = 0; k < 16; ++k) {
            /* An expanding ring of '*' between the rays. */
            float a = (k + 0.5f) * TAU / 16;
            particle_spawn(s, ox, oy, cosf(a) * 120, sinf(a) * 120, 0.7f, 0, G_AST,
                           prism[k % 5], 0);
        }
        for (int i = 0; i < 28; ++i) {
            unsigned n = hash(seed + i * 3571u);
            float a = (n % 628) / 100.f, sp = 30 + (n >> 8) % 90;
            particle_spawn(s, ox, oy, cosf(a) * sp, sinf(a) * sp - 20, 1.5f, 28,
                           i & 1 ? G_AST : G_PLUS, prism[(n >> 5) % 5], 0);
        }
        return;
    }
    if (kind == TREE_EFFECT_STAR_DING) {
        /* Gold dust scatters from the treetop star and drifts down. */
        for (int i = 0; i < 28; ++i) {
            unsigned n = hash(seed + i * 3571u);
            float a = (n % 628) / 100.f, sp = 18 + (n >> 8) % 55;
            particle_spawn(s, ox, oy, cosf(a) * sp, sinf(a) * sp * 0.5f + 12, 1.3f, 55,
                           i & 1 ? G_AST : G_DOT, i & 1 ? 0xffd94d : 0xffe9a8, 0);
        }
        return;
    }
    if (kind == TREE_EFFECT_RED_GIFT) {
        /* Firework burst high above the treetop: coloured sparks rain down. */
        for (int i = 0; i < 42; ++i) {
            unsigned n = hash(seed + i * 3571u);
            float a = (n % 628) / 100.f, sp = 35 + (n >> 8) % 105;
            particle_spawn(s, ox, oy, cosf(a) * sp, sinf(a) * sp, 1.5f, 105,
                           i & 1 ? G_AST : G_DOT, bauble_colors[(n >> 5) % 5], 0);
        }
        return;
    }
    for (int i = 0; i < 18; ++i) {
        unsigned n = hash(seed + i * 3571u);
        float a = (n % 628) / 100.f, sp = 30 + (n >> 8) % 90;
        if (kind == TREE_EFFECT_CANE) {
            /* The landed cane snaps into red and white candy chunks. */
            particle_spawn(s, ox, oy, cosf(a) * sp * 1.2f, -60 - (n >> 12) % 90, 1.3f, 420,
                           i % 3 == 0 ? G_EQ : i & 1 ? G_PLUS : G_BAR,
                           i & 1 ? 0xffffff : 0xff4d5e, 2);
        }
    }
    if (kind == TREE_EFFECT_BAUBLE) {
        /* POP!: a glitter ring bursts out, five mini baubles bounce on the
         * snow, and the word POP! floats up where the bauble was. */
        static const int pop[4] = {G_P, G_O, G_P, G_EXCL};
        for (int i = 0; i < 24; ++i) {
            unsigned n = hash(seed + i * 7121u);
            float a = i * TAU / 24, sp = 150 + (n % 70);
            particle_spawn(s, ox, oy, cosf(a) * sp, sinf(a) * sp, 0.7f, 160,
                           i & 1 ? G_AST : G_PLUS, bauble_colors[i % 5], 0);
        }
        for (int i = 0; i < 5; ++i) {
            unsigned n = hash(seed + i * 911u);
            particle_spawn(s, ox, oy, (i - 2) * 38 + (int)(n % 15) - 7, -140 - (int)(n % 60),
                           2.2f, 420, G_LO, bauble_colors[i], 3);
        }
        for (int i = 0; i < 4; ++i) {
            particle_spawn(s, ox - 12 + i * CELL_W, oy - 10, 0, -38, 1.1f, 0, pop[i],
                           i == 3 ? 0xffd94d : 0xffffff, 0);
        }
    }
}
/* Start one interaction: queue its effect id for the owner (sound and motor
 * live outside the scene), spawn its immediate particles and mark the
 * material so it cannot be tapped again until it respawns behind the tree. */
static void effect_start(tree_scene_t *s, int kind, int index, float ox, float oy)
{
    static const float durations[] = {
        0, 12, ROCKET_TIME + 0.6f, 1.2f, 1.4f, CANE_TIME, BAUBLE_TIME, 1.0f, BALLOON_TIME, 0.9f, 0.1f
    };
    effect_t *e = NULL;
    for (int i = 0; i < EFFECT_CAPACITY; ++i)
        if (!s->effects[i].active) {
            e = &s->effects[i];
            break;
        }
    if (!e) {
        return;
    }
    *e = (effect_t) {
        0, durations[kind], ox, oy, (int8_t)kind, (int8_t)index, true, false
    };
    if (s->pending_count < EFFECT_CAPACITY) {
        s->pending_effects[(s->pending_head + s->pending_count) % EFFECT_CAPACITY] = kind;
        s->pending_count++;
    }
    unsigned seed = hash((unsigned)(s->time * 997) + kind * 131 + index * 17 + 5);
    if (kind == TREE_EFFECT_STAR_DING) {
        particles_burst(s, kind, ox, oy, seed);
    } else if (kind == TREE_EFFECT_BLUE_GIFT) {
        s->gifts[index].state = MAT_ANIMATING;
        s->gift = BLUE_BOX;
        s->gift_time = 0;
        s->globe_time = 0;
        snow_burst(s);
    } else if (kind == TREE_EFFECT_RED_GIFT) {
        s->gifts[index].state = MAT_ANIMATING;
        s->gift = RED_BOX;
        s->gift_time = 0;
    } else if (kind == TREE_EFFECT_BELL || kind == TREE_EFFECT_STAR || kind == TREE_EFFECT_CANE
               || kind == TREE_EFFECT_BAUBLE || kind == TREE_EFFECT_STOCKING) {
        s->materials[index].state = MAT_ANIMATING;
    } else {
        s->materials[index].state = MAT_GONE;
        particles_burst(s, kind, ox, oy, seed);
    }
}
int tree_take_effect(tree_scene_t *s)
{
    if (!s || s->pending_count == 0) {
        return TREE_EFFECT_NONE;
    }
    int kind = s->pending_effects[s->pending_head];
    s->pending_head = (s->pending_head + 1) % EFFECT_CAPACITY;
    s->pending_count--;
    return kind;
}
static void materials_draw(view_t v)
{
    for (int idx = 0; idx < TREE_MATERIAL_COUNT; ++idx) {
        const material_t *o = &v.s->materials[idx];
        if (o->state == MAT_GONE) {
            continue;
        }
        float depth = o->z * v.co + o->x * v.si;
        if (depth <= 0) {
            continue; /* Whole material hidden while it is round the back. */
        }
        float sx = 240 + o->x * v.co - o->z * v.si;
        float sy = o->y + depth * 0.14f;
        effect_t *e = o->state == MAT_ANIMATING
                      ? effect_for(v.s, o->type + TREE_EFFECT_BOW, idx) : NULL;
        if (e && o->type == MAT_STAR) {
            /* Spiral up along the lamp helix to the treetop star. */
            float f = fminf(1, e->time / e->duration);
            float swirl = (1 - f) * 30;
            sx += (STAR_X - sx) * f + cosf(f * TAU * 2) * swirl;
            sy += (STAR_Y - sy) * f + sinf(f * TAU * 2) * swirl * 0.4f;
            depth = 300;
        } else if (e && o->type == MAT_CANE) {
            int gx = sx < 0 ? 0 : sx > 479 ? 479 : (int)sx;
            sy += cane_drop(v.s->horizon[gx] - CANE_ROWS * CELL_H - sy, e->time);
            depth = 300; /* Falls in front of the foliage. */
        } else if (e && (o->type == MAT_BAUBLE || o->type == MAT_STOCKING)) {
            if (o->type == MAT_STOCKING && e->fired) {
                continue; /* The balloon has left; the stocking is gone. */
            }
            sx += sinf(e->time * 70) * 3; /* Wobble before it pops or lets go. */
            depth = 300;
        } else if (e && o->type == MAT_BELL) {
            if (e->time < 0.9f) {
                sx += sinf(e->time * 28) * 5 * (1 - e->time / 0.9f);
            } else {
                float f = (e->time - 0.9f) / 0.5f;
                sy += 110 * f * f; /* Drop, then it is gone at the ground. */
            }
        }
        int ax = (int)(sx / CELL_W) * CELL_W + CELL_W / 2;
        int ay = (int)(sy / CELL_H) * CELL_H + CELL_H / 2;
        const sprite_cell_t *cells = MATERIAL_SPRITES[o->type];
        int target = o->state == MAT_IDLE ? MAT_TARGET_BASE + idx : 0;
        /* Lit edge: each stroke is brightened and ringed with a paler tint,
         * so a material stands out from the foliage around it. */
        float light = 0.95f + depth * 0.001f;
        /* The diamond is a solid gem: teal fill with bright facet glyphs. */
        bool gem = o->type == MAT_STAR;
        for (int c = 0; c < MATERIAL_SPRITE_LEN[o->type]; ++c) {
            place_ex(v.s, ax + cells[c].dx * CELL_W, ay + cells[c].dy * CELL_H, depth + 206,
                     cells[c].glyph, shade(tint(cells[c].rgb, gem ? 0.5f : 0.18f), light), target,
                     gem ? CELL_FILL : CELL_RIM,
                     shade(gem ? DIAMOND_FILL : tint(cells[c].rgb, 0.55f), light));
        }
    }
}
/* Stocking surprise: a solid ASCII balloon on a string floats up out of
 * the cuff, swaying, until it leaves the top of the screen. */
static const char *const BALLOON[] = {" .-. ", "('  )", " '-' ", "  )  ", "  (  "};
static void balloon_draw(tree_scene_t *s, const effect_t *e)
{
    static const unsigned colors[] = {0xff4d6d, 0x3d7bff, 0xb36bff, 0xffc93c};
    unsigned fill = colors[(unsigned)e->index % 4];
    float t = e->time;
    float x = e->origin_x + sinf(t * 2.4f) * 12 * fminf(1, t);
    float y = e->origin_y - 20 - 40 * t - 12 * t * t; /* Drifts up, then speeds. */
    int left = (int)(x / CELL_W) - 2, top = (int)(y / CELL_H) - 2;
    for (int r = 0; r < 5; ++r) {
        const char *line = BALLOON[r];
        int first = 0, last = 4;
        while (line[first] == ' ') {
            first++;
        }
        while (line[last] == ' ') {
            last--;
        }
        for (int c = first; c <= last; ++c) {
            bool body = r < 3;
            char ch = line[c];
            place_ex(s, (left + c) * CELL_W + CELL_W / 2, (top + r) * CELL_H + CELL_H / 2, 409,
                     glyph_of(ch), shade(body ? tint(fill, 0.6f) : 0xe8f0f8, 1), 0,
                     body ? CELL_FILL : CELL_PLAIN, shade(fill, 1));
        }
    }
}
static void effects_draw(tree_scene_t *s)
{
    for (int i = 0; i < EFFECT_CAPACITY; ++i) {
        const effect_t *e = &s->effects[i];
        if (e->active && e->kind == TREE_EFFECT_STOCKING && e->time > 0.15f) {
            balloon_draw(s, e);
        }
    }
    for (int i = 0; i < EFFECT_CAPACITY; ++i) {
        const effect_t *e = &s->effects[i];
        if (!e->active || e->kind != TREE_EFFECT_RED_GIFT || e->fired) {
            continue;
        }
        /* The firework rocket climbs from the gift to above the treetop. */
        float f = fminf(1, e->time / ROCKET_TIME);
        float ry = e->origin_y + (46 - e->origin_y) * f;
        place(s, e->origin_x, ry, 408, G_AST, shade(0xffd94d, 1), 0);
        place(s, e->origin_x, ry + 12, 408, G_BAR, shade(0xff8a3c, 0.9f), 0);
        place(s, e->origin_x, ry + 24, 408, G_DOT, shade(0xffe9a8, 0.7f), 0);
    }
    for (int i = 0; i < EFFECT_CAPACITY; ++i) {
        const effect_t *e = &s->effects[i];
        if (!e->active || e->kind != TREE_EFFECT_BELL || e->time >= 0.9f) {
            continue;
        }
        /* Two sound rings expand from the shaken bell. */
        for (int ring = 0; ring < 2; ++ring) {
            float r = fmodf(e->time * 90 + ring * 34, 68.f);
            if (r < 6) {
                continue;
            }
            for (int k = 0; k < 10; ++k) {
                float a = TAU * k / 10;
                place(s, e->origin_x + cosf(a) * r, e->origin_y + sinf(a) * r * 0.8f, 405,
                      G_DOT, shade(0xffe9a8, 1 - r / 80), 0);
            }
        }
    }
    for (int i = 0; i < PARTICLE_CAPACITY; ++i) {
        const particle_t *p = &s->particles[i];
        if (p->life <= 0) {
            continue;
        }
        float f = p->life / p->max_life;
        place(s, p->x, p->y, 410, p->glyph, shade(p->rgb, 0.25f + 0.75f * f), 0);
    }
}
static void foliage(view_t v)
{
    /* Foliage strokes get a same-hue darker rim: bolder, no fill. */
    for (unsigned i = 0; i < v.s->branch_count; ++i) {
        const tree_point_t *p = &v.s->branches[i];
        point_ex(v, p->x, p->y, p->z, p->glyph, p->rgb, 1, 0, CELL_RIM, scale_rgb(p->rgb, 0.62f));
    }
    /* The gold helix reads as one dash-dot garland spiralling down: three
     * dashes that follow its on-screen slope, a gap, a bright '*' bead, a
     * gap. Dashes carry a dark amber rim so they stand off the foliage. */
    float head = v.s->pulse * (LAMP_COUNT - 1) / 2;
    for (int i = 0; i < LAMP_COUNT; ++i) {
        int phase = i % 6;
        if (phase == 3 || phase == 5) {
            continue;
        }
        const tree_point_t *p = &v.s->lamps[i];
        bool hot = v.s->pulse >= 0 && fabsf(i - head) < 6;
        if (v.s->lamp_sweep >= 0) {
            /* Diamond sweep lights the yellow helix from the top down. */
            float h = (float)i / (LAMP_COUNT - 1);
            hot = hot || h >= 1 - v.s->lamp_sweep / 1.2f;
        }
        /* A light wave runs down the garland, so it reads as a lit string. */
        float wave = 0.9f + 0.4f * fmaxf(0, sinf(v.s->time * 5 - i * 0.3f));
        if (phase == 4) {
            /* Beads twinkle on their own phases; at the peak they flash white
             * as '+' and shed a tiny spark. */
            unsigned n = hash(i * 977 + 13);
            float tw = sinf(v.s->time * 7 + (n % 628) / 100.f);
            bool flash = hot || tw > 0.8f;
            point(v, p->x, p->y, p->z, flash && tw > 0.93f ? G_PLUS : G_AST,
                  flash ? 0xffffff : 0xfff3b8, flash ? 1.4f : wave, 0);
            float depth = p->z * v.co + p->x * v.si;
            if (tw > 0.995f && depth > 0) {
                particle_spawn(v.s, 240 + p->x * v.co - p->z * v.si, p->y + depth * 0.14f,
                               (int)(n % 31) - 15, 18, 0.5f, 40, G_DOT, 0xfff3b8, 0);
            }
            continue;
        }
        const tree_point_t *a = &v.s->lamps[i > 0 ? i - 1 : i];
        const tree_point_t *b = &v.s->lamps[i + 1 < LAMP_COUNT ? i + 1 : i];
        float dx = (b->x - a->x) * v.co - (b->z - a->z) * v.si;
        float dy = (b->y - a->y) + ((b->z - a->z) * v.co + (b->x - a->x) * v.si) * 0.14f;
        int glyph = fabsf(dy) < 0.45f * fabsf(dx) ? G_DASH
                    : fabsf(dx) < 0.45f * fabsf(dy) ? G_BAR
                    : (dx > 0) == (dy > 0) ? G_BACK : G_SLASH;
        unsigned gold = (i / 6) % 2 ? 0xffe08a : 0xffcc33;
        point_ex(v, p->x, p->y, p->z, glyph, hot ? 0xffffd8 : gold, 1.1f * wave, 0, CELL_RIM,
                 0x8a5a0c);
    }
}
static void snow_burst(tree_scene_t *s)
{
    /* Fountain out of the opened blue gift, then the flakes disperse. */
    float cx, cz;
    gift_center(s, 1, &cx, &cz);
    float height = 26;
    float co = cosf(s->angle), si = sinf(s->angle);
    float depth = cz * co + cx * si;
    float sx = 240 + cx * co - cz * si, sy = 368 - height + depth * 0.14f;
    for (int i = 0; i < SNOW_CAPACITY; ++i) {
        unsigned n = hash((unsigned)(s->time * 997) + i * 7919 + 13);
        snow_flake_t *p = &s->snow[i];
        p->x = sx + (int)(n % 61) - 30;
        p->y = sy + (int)((n >> 6) % 17) - 8;
        p->vx = (int)((n >> 10) % 101) - 50;
        p->vy = -(40 + (int)((n >> 16) % 80));
        /* The globe throws out plenty of large flakes; they thin back to
         * SNOW_BIG_MAX as flakes re-roll after landing. */
        p->size = i % 4 == 0 ? 2 : (uint8_t)(i & 1);
    }
}
static void snow_update(tree_scene_t *s, float dt)
{
    float target = SNOW_AMBIENT;
    if (s->globe_time >= 0) {
        float f = fmaxf(0, fminf(1, (s->globe_time - 5) / 7));
        f = f * f * (3 - 2 * f);
        target = SNOW_CAPACITY - (SNOW_CAPACITY - SNOW_AMBIENT) * f;
    }
    for (int i = 0; i < SNOW_CAPACITY; ++i) {
        snow_flake_t *p = &s->snow[i];
        bool active = i < (int)(target + 0.5f);
        /* Large flakes fall fastest, small ones drift down slowly. */
        float terminal = p->size == 2 ? 17 : p->size == 1 ? 12 : 8;
        p->vy += (terminal - p->vy) * fminf(1, dt * 0.45f);
        p->vx -= p->vx * dt * 1.1f;
        /* Snow is pushed only by the finger (see tree_pointer); here it
         * just settles back to its fall speed once the push decays. */
        p->x += p->vx * dt + sinf(s->time * 0.9f + i) * 2 * dt;
        p->y += p->vy * dt;
        if (p->x < -8) {
            p->x += 496;
        } else if (p->x > 488) {
            p->x -= 496;
        }
        if (p->y > 486) {
            if (active) {
                unsigned n = hash(i * 331 + (unsigned)(s->time * 61));
                p->x = n % 480;
                p->y = -6;
                p->vx = 0;
                p->vy = terminal * 0.5f;
                /* Size is re-rolled on reset: '.' or '*', and only now and
                 * then a large flake while fewer than SNOW_BIG_MAX are out. */
                int big = 0;
                for (int k = 0; k < SNOW_CAPACITY; ++k) {
                    big += k != i && s->snow[k].size == 2 && s->snow[k].y >= -12;
                }
                p->size = big < SNOW_BIG_MAX && (n >> 7) % 9 == 0 ? 2 : (uint8_t)((n >> 4) & 1);
            } else {
                p->y = -20;
                p->vy = 0;
            }
        }
    }
}
enum { FLAKE_HIT = 24 };
/* A tapped large flake bursts into small '.' and '*' flakes that drift
 * outward and settle down; the flake itself carries on as a small '*'. */
static void snow_shatter(tree_scene_t *s, int i)
{
    snow_flake_t *p = &s->snow[i];
    unsigned seed = hash((unsigned)(s->time * 1231) + i * 97);
    for (int k = 0; k < 12; ++k) {
        unsigned n = hash(seed + k * 2654435761u);
        float a = k * TAU / 12 + (n % 40) / 100.f, sp = 35 + (n >> 8) % 55;
        particle_spawn(s, p->x, p->y, cosf(a) * sp, sinf(a) * sp - 15, 2.2f, 16,
                       k & 1 ? G_DOT : G_AST, k & 1 ? 0xe8f6ff : 0xffffff, 0);
    }
    p->size = 1;
}
/* Finger drags scatter the snow: flakes near the pointer path are pushed
 * away from it. Always on at low strength, stronger during the globe. */
static void snow_push(tree_scene_t *s, int x, int y, int dx)
{
    float strength = s->globe_time >= 0 ? 150 : 55;
    for (int i = 0; i < SNOW_CAPACITY; ++i) {
        snow_flake_t *p = &s->snow[i];
        float fx = p->x - x, fy = p->y - y;
        float d = sqrtf(fx * fx + fy * fy);
        if (d < 1 || d > 90) {
            continue;
        }
        float f = (1 - d / 90) * strength;
        /* Small flakes take the strongest push, large ones the least. */
        float push = p->size == 2 ? 0.55f : p->size == 1 ? 0.85f : 1.25f;
        p->vx += (fx / d * f + dx * 1.6f) * push;
        p->vy += fy / d * f * 0.7f;
    }
}
static void particles_update(tree_scene_t *s, float dt)
{
    for (int i = 0; i < PARTICLE_CAPACITY; ++i) {
        particle_t *p = &s->particles[i];
        if (p->life <= 0) {
            continue;
        }
        p->life -= dt;
        p->vy += p->gravity * dt;
        p->x += p->vx * dt;
        p->y += p->vy * dt;
        int gx = p->x < 0 ? 0 : p->x > 479 ? 479 : (int)p->x;
        float floor_y = s->horizon[gx] + 4; /* Land just inside the drift. */
        if (p->y > floor_y && p->vy > 0) {
            if (p->bounces > 0) {
                p->bounces--;
                p->y = floor_y;
                p->vy = -p->vy * 0.5f;
                p->vx *= 0.8f;
            } else {
                p->vy = 0;
                p->vx *= 0.5f;
                p->y = floor_y;
            }
        }
    }
}
/* An opened gift breaks into its own ASCII pieces: they hop out, scatter
 * across the snow, bounce and fade, and the box is gone at once. */
static void gift_scatter(tree_scene_t *s, int slot)
{
    gift_t *g = &s->gifts[slot];
    float co = cosf(s->angle), si = sinf(s->angle);
    unsigned seed = hash((unsigned)(s->time * 977) + slot * 131);
    unsigned k = 0;
    for (unsigned i = 0; i < s->box_body_count; i += 2) {
        const tree_point_t *p = &s->box_body[i];
        if ((p->x < 0 ? 0 : 1) != slot) {
            continue;
        }
        float x = p->x + g->dx, z = p->z + g->dz, depth = z * co + x * si;
        unsigned n = hash(seed + k++ * 7919u);
        float sx = 240 + x * co - z * si, sy = p->y + depth * 0.14f;
        float vx = (sx - (240 + (GIFT_BASE_X[slot] + g->dx) * co)) * 2.2f + (int)(n % 61) - 30;
        particle_spawn(s, sx, sy, vx, -(70 + (int)((n >> 8) % 90)), 1.8f, 320, p->glyph,
                       tint(p->rgb, 0.3f), 2);
    }
    g->exit_time = -1;
    g->state = MAT_GONE;
}
static void effects_update(tree_scene_t *s, float dt)
{
    for (int i = 0; i < EFFECT_CAPACITY; ++i) {
        effect_t *e = &s->effects[i];
        if (!e->active) {
            continue;
        }
        e->time += dt;
        if (e->kind == TREE_EFFECT_RED_GIFT) {
            /* The rocket bursts above the treetop into falling sparks. */
            if (!e->fired && e->time >= ROCKET_TIME) {
                e->fired = true;
                unsigned seed = hash((unsigned)(s->time * 997) + i * 131 + 5);
                particles_burst(s, TREE_EFFECT_RED_GIFT, e->origin_x, 46, seed);
            }
        } else if (e->kind == TREE_EFFECT_BLUE_GIFT && !e->fired && e->time >= 0.5f) {
            e->fired = true;
            gift_scatter(s, e->index); /* Lid off, then the box breaks apart. */
        } else if (e->kind == TREE_EFFECT_STOCKING && !e->fired && e->time >= 0.45f) {
            e->fired = true; /* Balloon is out of the cuff; stocking vanishes. */
        } else if (e->kind == TREE_EFFECT_STAR) {
            /* Prismatic sparkle trail along the spiral flight to the star. */
            static const unsigned prism[] = {0xffffff, 0x9be8ff, 0xff8ac2, 0xffd94d, 0xcc97ff};
            float f = fminf(1, e->time / e->duration);
            float swirl = (1 - f) * 30;
            float tx = e->origin_x + (STAR_X - e->origin_x) * f + cosf(f * TAU * 2) * swirl;
            float ty = e->origin_y + (STAR_Y - e->origin_y) * f + sinf(f * TAU * 2) * swirl * 0.4f;
            for (int k = 0; k < 2; ++k) {
                unsigned n = hash((unsigned)(s->time * 1543) + k * 389 + i * 17);
                particle_spawn(s, tx, ty, (int)(n % 41) - 20, (int)((n >> 6) % 41) - 20, 0.6f, 0,
                               (n >> 12) & 1 ? G_AST : G_PLUS, prism[(n >> 13) % 5], 0);
            }
            if (!e->fired && f >= 0.95f) {
                e->fired = true;
                s->star_flash = 1.6f;
                s->lamp_sweep = 0; /* Yellow helix lights from the top down. */
                particles_burst(s, TREE_EFFECT_STAR, STAR_X, STAR_Y,
                                hash((unsigned)(s->time * 997) + 71));
            }
        }
        if (e->time < e->duration) {
            continue;
        }
        e->active = false;
        if (e->kind == TREE_EFFECT_STAR) {
            s->materials[e->index].state = MAT_GONE;
        } else if (e->kind == TREE_EFFECT_BELL || e->kind == TREE_EFFECT_STOCKING) {
            s->materials[e->index].state = MAT_GONE;
        } else if (e->kind == TREE_EFFECT_CANE || e->kind == TREE_EFFECT_BAUBLE) {
            /* Cane snaps where it landed; bauble pops where it hung. */
            int gx = e->origin_x < 0 ? 0 : e->origin_x > 479 ? 479 : (int)e->origin_x;
            float y = e->kind == TREE_EFFECT_CANE ? s->horizon[gx] - 6 : e->origin_y;
            s->materials[e->index].state = MAT_GONE;
            particles_burst(s, e->kind, e->origin_x, y, hash((unsigned)(s->time * 997) + i));
        } else if (e->kind == TREE_EFFECT_RED_GIFT) {
            gift_scatter(s, e->index); /* Breaks apart; respawns from behind. */
        }
    }
    /* Gifts finish sinking, then stay gone until their anchor goes behind. */
    for (int i = 0; i < GIFT_COUNT; ++i) {
        gift_t *g = &s->gifts[i];
        if (g->exit_time >= 0 && (g->exit_time += dt) > 0.8f) {
            g->exit_time = -1;
            g->state = MAT_GONE;
        }
    }
}
/* The backdrop is glyphs too: a few dim '.' points in the night sky, a
 * sparse white glint line along the drift top and scattered glints in the
 * snow. The drift itself is a flat colour band shaped by s->horizon. */
static void background_draw(tree_scene_t *s)
{
    for (int col = 0; col < TREE_COLUMNS; ++col) {
        int x0 = col * CELL_W, x1 = x0 + CELL_W - 1;
        int top = s->horizon[x0] > s->horizon[x1] ? s->horizon[x0] : s->horizon[x1];
        int bottom = s->horizon[x0] < s->horizon[x1] ? s->horizon[x0] : s->horizon[x1];
        int lip = (top + CELL_H - 1) / CELL_H; /* First row wholly on the snow. */
        for (int row = 0; row < TREE_ROWS; ++row) {
            unsigned n = hash(row * 131 + col * 17 + 3);
            int glyph = G_BLANK;
            unsigned rgb = 0xffffff;
            if ((row + 1) * CELL_H <= bottom) {
                if (n % 97 == 0) {
                    glyph = G_DOT;
                    rgb = 0x50708f;
                }
            } else if (row == lip) {
                glyph = n % 7 == 0 ? G_DOT : G_BLANK;
            } else if (row > lip && n % 29 == 0) {
                glyph = n % 2 ? G_AST : G_DOT;
            }
            if (glyph != G_BLANK) {
                place(s, x0 + CELL_W / 2, row * CELL_H + CELL_H / 2, 0, glyph, shade(rgb, 1), 0);
            }
        }
    }
}
/* Treetop star: a hand-drawn front-facing ASCII sprite that never turns
 * with the tree. 'O' marks the bright core, drawn as '*'. */
static const char *const STAR_SPRITE[] = {
    "......*......",
    "...../#\\.....",
    "====/#O#\\====",
    ".\\##OOOOO##/.",
    "...\\##O##/...",
    ".../##.##\\...",
    "../#/...\\#\\..",
    ".*/.......\\*.",
};
/* The flat star turns with the tree: its columns squeeze toward the centre
 * by |cos(angle)|, so a quarter turn shows it edge-on as one vertical line. */
static void star_draw(tree_scene_t *s)
{
    float squeeze = fabsf(cosf(s->angle));
    float light = (1 + 0.15f * sinf(s->time * 3) + s->star_flash) * (0.75f + 0.25f * squeeze);
    const int centre = STAR_SPRITE_CENTRE;
    const int cx = (STAR_COL0 + centre) * CELL_W + CELL_W / 2;
    for (unsigned r = 0; r < sizeof(STAR_SPRITE) / sizeof(STAR_SPRITE[0]); ++r) {
        int y = (STAR_ROW0 + (int)r) * CELL_H + CELL_H / 2;
        if (squeeze < STAR_EDGE_ON) {
            /* Edge-on: two gold columns either side of the axis give the
             * star a little thickness; tips on top, lit side brighter. */
            for (int side = 0; side < 2; ++side) {
                unsigned rgb = r == 0 ? 0xfff6c8 : side ? 0xffd94d : 0xc98a24;
                place(s, cx - CELL_W + side * CELL_W, y, STAR_DEPTH, r == 0 ? G_AST : G_BAR,
                      shade(rgb, light), STAR);
            }
            continue;
        }
        for (int c = 0; STAR_SPRITE[r][c]; ++c) {
            char ch = STAR_SPRITE[r][c];
            if (ch == '.') {
                continue;
            }
            unsigned rgb = ch == '*' || ch == 'O' ? 0xfff6c8 : ch == '#' ? 0xffd94d : 0xf2b630;
            /* A diamond flash turns the star white-hot for a moment. */
            rgb = tint(rgb, fminf(1, s->star_flash * 0.6f));
            int x = cx + (int)lroundf((c - centre) * CELL_W * squeeze);
            /* Outer columns win where squeezed columns overlap, keeping the
             * silhouette symmetric. */
            place(s, x, y, STAR_DEPTH + abs(c - centre), ch == 'O' ? G_AST : glyph_of(ch),
                  shade(rgb, light), STAR);
        }
    }
}
void tree_prepare(tree_scene_t *s)
{
    memset(s->cells, 0, sizeof(s->cells));
    view_t v = {s, cosf(s->angle), sinf(s->angle)};
    background_draw(s);
    for (int i = 0; i < SNOW_CAPACITY; ++i) {
        const snow_flake_t *p = &s->snow[i];
        if (p->y < 0) {
            continue;
        }
        if (p->size == 2) {
            /* Rare large flake, 3 x 3 ASCII: \|/  -*-  /|\ */
            static const char BIG[3][4] = {"\\|/", "-*-", "/|\\"};
            for (int r = 0; r < 3; ++r)
                for (int c = 0; c < 3; ++c) {
                    place(s, p->x + (c - 1) * CELL_W, p->y + (r - 1) * CELL_H, 390,
                          glyph_of(BIG[r][c]), shade(r == 1 && c == 1 ? 0xffffff : 0xdcecf8, 1),
                          0);
                }
        } else if (p->size == 1) {
            place(s, p->x, p->y, 390, G_AST, shade(0xf4fbff, 1), 0);
        } else {
            /* Small dots fall behind the tree, as distant snow. */
            place(s, p->x, p->y, 1, G_DOT, shade(0xe8f6ff, 1), 0);
        }
    }
    for (unsigned i = 0; i < s->trunk_count; ++i) {
        const tree_point_t *p = &s->trunk[i];
        point_ex(v, p->x, p->y, p->z, p->glyph, p->rgb, 1, 0, CELL_RIM, scale_rgb(p->rgb, 0.6f));
    }
    float root_light = s->pulse > 1.8f ? 1.78f : 1;
    for (unsigned i = 0; i < s->root_count; ++i) {
        const tree_point_t *p = &s->roots[i];
        float facing = p->x * v.co + p->z * v.si;
        point_ex(v, p->x, p->y, p->z, facing > 0 ? G_BACK : G_SLASH, p->rgb, root_light, 0,
                 CELL_RIM, scale_rgb(p->rgb, 0.6f));
    }
    foliage(v);
    materials_draw(v);
    star_draw(s);
    /* Gifts are materials too: the whole box hides round the back, and a
     * finished box sinks before it respawns from behind the tree. */
    for (int pass = 0; pass < 2; ++pass) {
        unsigned count = pass == 0 ? s->box_body_count : s->box_lid_count;
        for (unsigned i = 0; i < count; ++i) {
            const tree_point_t *p = pass == 0 ? &s->box_body[i] : &s->box_lid[i];
            int slot = p->x < 0 ? 0 : 1;
            const gift_t *g = &s->gifts[slot];
            if (g->state == MAT_GONE) {
                continue;
            }
            float cx, cz;
            gift_center(s, slot, &cx, &cz);
            if (!anchor_is_front(s, cx, cz)) {
                continue;
            }
            int id = slot == 0 ? RED_BOX : BLUE_BOX;
            int target = g->state == MAT_IDLE ? id : 0;
            float sink = g->exit_time >= 0 ? g->exit_time / 0.8f * 34 : 0;
            float light = g->exit_time >= 0 ? 1 - g->exit_time / 0.8f * 0.6f : 1;
            /* Bold boxes: every body/lid cell is filled with a deep shade of
             * its colour and carries a brighter glyph, so the box reads as a
             * solid block; the bow loops get a dark rim instead. */
            /* Boxes keep open ASCII strokes with a dark bold outline. */
            unsigned ink = tint(p->rgb, 0.15f), edge = scale_rgb(p->rgb, 0.4f);
            if (pass == 0) {
                point_ex(v, p->x + g->dx, p->y + sink, p->z + g->dz, p->glyph, ink, light, target,
                         CELL_RIM, edge);
            } else {
                float t = s->gift == id ? s->gift_time : -1;
                float lift = t < 0 ? 0 : fminf(1, fminf(t * 4, (4 - t) * 4)) * 26;
                point_ex(v, p->x + g->dx, p->y - lift + sink, p->z + g->dz, p->glyph, ink,
                         1.14f * light, target, CELL_RIM, edge);
            }
        }
    }
    tree_animal_draw(s);
    effects_draw(s);
}
void tree_tick(tree_scene_t *s)
{
    const float dt = TREE_TICK_MS / 1000.f;
    s->time += dt;
    if (!s->pressed) {
        s->speed += (REST_SPEED - s->speed) * 0.075f;
        s->angle = remainderf(s->angle + s->speed * dt, TAU);
    }
    if (s->pulse >= 0 && (s->pulse += dt) > 2.35f) {
        s->pulse = -1;
    }
    if (s->gift_time >= 0 && (s->gift_time += dt) > 4) {
        s->gift_time = -1;
    }
    if (s->globe_time >= 0 && (s->globe_time += dt) > 12) {
        s->globe_time = -1;
    }
    if (s->star_flash > 0) {
        s->star_flash = fmaxf(0, s->star_flash - dt * 1.5f);
    }
    if (s->lamp_sweep >= 0 && (s->lamp_sweep += dt) > 1.6f) {
        s->lamp_sweep = -1;
    }
    effects_update(s, dt);
    particles_update(s, dt);
    tree_animal_update(s, dt);
    /* Existing materials never change: type and anchor stay until tapped.
     * Only a tapped-out slot refills, only during auto-rotation (not while
     * pressed, and not until inertia has settled back to rest speed), and
     * only when its anchor turns behind the tree. One refill per turn of
     * the tree at most, across tree materials and gifts together. */
    if (!s->pressed) {
        s->rotation_total += fabsf(s->speed * dt);
    }
    bool auto_rotating = !s->pressed && fabsf(s->speed - REST_SPEED) < 0.03f;
    bool refilled = false;
    for (int i = 0; i < TREE_MATERIAL_COUNT; ++i) {
        material_t *m = &s->materials[i];
        bool front = anchor_is_front(s, m->x, m->z);
        if (auto_rotating && !refilled && m->was_front && !front
                && m->state == MAT_GONE
                && s->rotation_total >= s->next_refill_rotation) {
            material_spawn(s, i, hash((unsigned)(s->time * 1000) + i * 131 + 7));
            s->next_refill_rotation = s->rotation_total + TAU * 0.95f;
            refilled = true;
        }
        m->was_front = anchor_is_front(s, m->x, m->z);
    }
    for (int i = 0; i < GIFT_COUNT; ++i) {
        gift_t *g = &s->gifts[i];
        float cx, cz;
        gift_center(s, i, &cx, &cz);
        bool front = anchor_is_front(s, cx, cz);
        if (auto_rotating && !refilled && g->was_front && !front
                && g->state == MAT_GONE && g->exit_time < 0
                && s->rotation_total >= s->next_refill_rotation) {
            gift_spawn(s, i, hash((unsigned)(s->time * 1000) + i * 977 + 3));
            s->next_refill_rotation = s->rotation_total + TAU * 0.95f;
            refilled = true;
        }
        g->was_front = anchor_is_front(s, cx, cz);
    }
    snow_update(s, dt);
    tree_prepare(s);
}
void tree_pointer(tree_scene_t *s, int x, int y, bool down)
{
    if (down && !s->pressed) {
        s->pressed = true;
        s->dragged = false;
        s->start_x = s->last_x = x;
        s->start_y = s->last_y = y;
        s->speed = 0;
    } else if (down) {
        if (abs(x - s->start_x) > 6 || abs(y - s->start_y) > 6) {
            s->dragged = true;
        }
        if (s->dragged) {
            /* Grab the front face: it follows the finger, so the angle
             * moves opposite to the raw pointer delta in screen space. */
            float delta = (x - s->last_x) * 0.012f;
            s->angle -= delta;
            if (delta != 0) {
                s->speed = fmaxf(-3, fminf(3, -delta * 16));
            }
            snow_push(s, x, y, x - s->last_x);
            s->last_x = x;
            s->last_y = y;
        }
    } else if (s->pressed) {
        s->pressed = false;
        /* A finger tap usually drifts a little; only a real drag that
         * travelled far suppresses the tap. */
        if (s->dragged && (abs(x - s->start_x) > 28 || abs(y - s->start_y) > 28)) {
            return;
        }
        int nearest = 900, target = 0;
        int gift_nearest = 900, gift_target = 0;
        for (int row = 0; row < TREE_ROWS; row++)
            for (int col = 0; col < TREE_COLUMNS; col++) {
                int id = s->cells[row][col].target;
                if (!id) {
                    continue;
                }
                int dx = col * CELL_W + CELL_W / 2 - x, dy = row * CELL_H + CELL_H / 2 - y, d = dx * dx + dy * dy;
                if (d < nearest) {
                    nearest = d;
                    target = id;
                }
                if (id >= RED_BOX && id <= BLUE_BOX && d < gift_nearest) {
                    gift_nearest = d;
                    gift_target = id;
                }
            }
        /* A large snowflake closer than any target shatters on tap. */
        int flake = -1, flake_d = FLAKE_HIT * FLAKE_HIT + 1;
        for (int i = 0; i < SNOW_CAPACITY; ++i) {
            const snow_flake_t *p = &s->snow[i];
            int dx = (int)p->x - x, dy = (int)p->y - y, d = dx * dx + dy * dy;
            if (p->size == 2 && p->y >= 0 && d < flake_d) {
                flake = i;
                flake_d = d;
            }
        }
        if (flake >= 0 && (!target || flake_d < nearest)) {
            snow_shatter(s, flake);
            return;
        }
        /* A gift in range wins over an animal under the same tap. */
        if (target == ANIMAL_TARGET && gift_target) {
            target = gift_target;
        }
        if (target == STAR) {
            effect_start(s, TREE_EFFECT_STAR_DING, -1, STAR_X, STAR_Y);
        } else if (target == ANIMAL_TARGET) {
            if (tree_animal_tap(s)) {
                if (s->pending_count < EFFECT_CAPACITY) {
                    s->pending_effects[(s->pending_head + s->pending_count) % EFFECT_CAPACITY]
                        = TREE_EFFECT_ANIMAL;
                    s->pending_count++;
                }
            } else {
                target = 0;
            }
        } else if (target >= MAT_TARGET_BASE) {
            int idx = target - MAT_TARGET_BASE;
            if (idx < TREE_MATERIAL_COUNT && s->materials[idx].state == MAT_IDLE) {
                effect_start(s, s->materials[idx].type + TREE_EFFECT_BOW, idx, x, y);
            } else {
                target = 0;
            }
        } else if (target >= RED_BOX) {
            int slot = target == RED_BOX ? 0 : 1;
            if (s->gifts[slot].state == MAT_IDLE) {
                effect_start(s, slot == 0 ? TREE_EFFECT_RED_GIFT : TREE_EFFECT_BLUE_GIFT,
                             slot, x, y);
            } else {
                target = 0;
            }
        }
        if (target) {
            s->taps++;
#ifdef ESP_PLATFORM
            ESP_LOGI("tree_pointer", "tap target=%d taps=%u", target, s->taps);
#endif
        }
    }
}
static bool solid_at(const tree_scene_t *s, int row, int col)
{
    return row >= 0 && row < TREE_ROWS && col >= 0 && col < TREE_COLUMNS
           && s->cells[row][col].mode == CELL_FILL;
}
static unsigned cell_bits(unsigned glyph, int glyph_row)
{
    if (glyph_row < 0 || glyph_row >= 10 || glyph >= G_COUNT) {
        return 0;
    }
    return (unsigned)GLYPHS[glyph][glyph_row] << 1;
}
void tree_draw(const esp_gsp_canvas_surface_t *surf, void *ctx)
{
    tree_scene_t *s = ctx;
#ifdef ESP_PLATFORM
    /* Count the top band once per canvas redraw, even for tiled output. */
    if (surf->y == 0) {
        int64_t now = esp_timer_get_time();
        if (s->last_draw_us) {
            int64_t gap = now - s->last_draw_us;
            s->total_interval_us += gap;
            if (gap > s->max_interval_us) {
                s->max_interval_us = gap;
            }
            if (++s->measured_frames == 64) {
                ESP_LOGI("tree_frames", "64 intervals mean_us=%lld max_us=%lld branches=%u",
                         (long long)(s->total_interval_us / 64), (long long)s->max_interval_us,
                         s->branch_count);
                s->total_interval_us = s->max_interval_us = s->measured_frames = 0;
            }
        }
        s->last_draw_us = now;
    }
#endif
    const uint16_t sky = shade(SKY_RGB, 1), ground = shade(SNOW_GROUND_RGB, 1);
    for (int j = 0; j < surf->height; j++) {
        int y = surf->y + j;
        uint16_t *line = (uint16_t *)((char *)surf->pixels + j * surf->stride_bytes);
        int left = surf->x, right = left + surf->width;
        int glyph_row = (y % CELL_H) - 1;
        bool in_greeting = y >= GREETING_Y - 1 && y <= GREETING_Y + GREETING_H;
        for (int col = left / CELL_W; col <= (right - 1) / CELL_W; ++col) {
            const tree_cell_t *cell = &s->cells[y / CELL_H][col];
            /* Glyph rows as 8-pixel masks: bit 7 is the cell's left pixel. */
            unsigned mid = cell_bits(cell->glyph, glyph_row);
            unsigned rim = 0;
            if (cell->mode == CELL_RIM) {
                unsigned up = cell_bits(cell->glyph, glyph_row - 1);
                unsigned down = cell_bits(cell->glyph, glyph_row + 1);
                rim = ((mid << 1) | (mid >> 1) | up | down) & ~mid & 0xffu;
            }
            int base = col * CELL_W;
            int start = left > base ? left - base : 0;
            int end = right - base < CELL_W ? right - base : CELL_W;
            /* Solid blocks (animals, gifts) get a dark 1 px outline wherever
             * the neighbouring cell is not solid too. */
            unsigned edge = 0;
            uint16_t outline = 0;
            if (cell->mode == CELL_FILL) {
                int row = y / CELL_H, sub = y % CELL_H;
                outline = (uint16_t)((cell->extra >> 1) & 0x7befu);
                if ((sub == 0 && !solid_at(s, row - 1, col))
                        || (sub == CELL_H - 1 && !solid_at(s, row + 1, col))) {
                    edge = 0xffu;
                } else {
                    edge = (solid_at(s, row, col - 1) ? 0 : 0x80u)
                           | (solid_at(s, row, col + 1) ? 0 : 0x01u);
                }
            }
            for (int offset = start; offset < end; ++offset) {
                unsigned bit = 0x80u >> offset;
                uint16_t bg = cell->mode == CELL_FILL ? (edge & bit ? outline : cell->extra)
                              : y >= s->horizon[base + offset] ? ground : sky;
                uint16_t px = mid & bit ? cell->color : rim & bit ? cell->extra : bg;
                if (in_greeting) {
                    (void)tree_greeting_pixel(base + offset, y, s->time, &px);
                }
                *line++ = px;
            }
        }
    }
}
