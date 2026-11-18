// SPDX-License-Identifier: Apache-2.0
#pragma once
/* Private to app_hello: shared by the scene modules (tree_scene.c and
 * tree_animals.c); not part of the component's public interface. */
#include "tree_scene.h"

/* 6 x 10 glyphs in an 8 x 12 cell: one blank column on each side and one
 * blank row above and below, so strokes and neighbours stay separated.
 * Each row is 6 bits, leftmost pixel in bit 5. */
enum {
    G_BLANK, G_V, G_Y, G_PLUS, G_DOT, G_COLON, G_AST, G_SLASH, G_BACK,
    G_BAR, G_HASH, G_EQ, G_O, G_L, G_E, G_T, G_I, G_N, G_S, G_W,
    G_LPAR, G_RPAR, G_UNDER, G_CARET, G_GT, G_LT, G_DQUOTE, G_QUOTE, G_COMMA,
    G_DASH, G_TILDE, G_LO, G_LV, G_P, G_EXCL, G_COUNT
};

/* Touch target id of the visiting animal (materials use 10.., gifts 2..3). */
enum { ANIMAL_TARGET = 20 };

unsigned tree_hash(unsigned n);
/** RGB888 scaled by light, as RGB565. */
uint16_t tree_shade(unsigned rgb, float light);
/** Depth-tested glyph placement into the cell grid; mode/extra per CELL_*. */
void tree_place_ex(tree_scene_t *s, int x, int y, float z, int glyph, uint16_t color, int target,
                   uint8_t mode, uint16_t extra);
/** Font glyph for an ASCII character used in sprite strings (G_BLANK if none). */
int tree_glyph_of(char c);

/** Draw the visiting animal, if any, into the cell grid. */
void tree_animal_draw(tree_scene_t *s);
/** Start the visiting animal's exit (run across, or dive back into its
 * hole); returns false if there is no animal or it is already exiting. */
bool tree_animal_tap(tree_scene_t *s);
/** Spawn, idle, exit timing for the visiting animal. */
void tree_animal_update(tree_scene_t *s, float dt);
