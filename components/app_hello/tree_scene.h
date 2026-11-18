// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "esp_gsp.h"

enum { CELL_W = 8, CELL_H = 12, TREE_COLUMNS = 480 / CELL_W, TREE_ROWS = 480 / CELL_H, TREE_TICK_MS = 33 };
/* A cell's glyph is drawn over the backdrop; CELL_FILL paints the whole
 * cell in `extra` first (solid animals), CELL_RIM rings each glyph stroke
 * with `extra` (lit material edges). */
enum { CELL_PLAIN, CELL_FILL, CELL_RIM };
typedef struct {
    int16_t depth;
    uint16_t color, glyph, extra;
    int8_t target;
    uint8_t mode;
} tree_cell_t;
typedef struct {
    int16_t x, y, z;
    uint16_t glyph;
    uint32_t rgb;
} tree_point_t;
enum {
    BRANCH_CAPACITY = 4 * 7 * 32,
    LAMP_COUNT = 144,
    TRUNK_CAPACITY = 64,
    ROOT_CAPACITY = 40,
    BOX_BODY_CAPACITY = 320,
    BOX_LID_CAPACITY = 128
};
/* Interactive material kinds. Tree materials are the first six; the two
 * gift kinds follow. Effect ids in tree_effect_t map from these. */
enum {
    MAT_BOW, MAT_BELL, MAT_CANE, MAT_BAUBLE, MAT_STAR, MAT_STOCKING,
    MAT_GIFT_RED, MAT_GIFT_BLUE, MAT_KIND_COUNT
};
enum {
    TREE_EFFECT_NONE, TREE_EFFECT_BLUE_GIFT, TREE_EFFECT_RED_GIFT,
    TREE_EFFECT_BOW, TREE_EFFECT_BELL, TREE_EFFECT_CANE, TREE_EFFECT_BAUBLE,
    TREE_EFFECT_STAR, TREE_EFFECT_STOCKING, TREE_EFFECT_STAR_DING,
    TREE_EFFECT_ANIMAL
};
enum { SNOW_CAPACITY = 56, SNOW_AMBIENT = 18, SNOW_BIG_MAX = 2 };
enum { TREE_MATERIAL_COUNT = 4, GIFT_COUNT = 2, PARTICLE_CAPACITY = 160, EFFECT_CAPACITY = 4 };
enum material_state { MAT_IDLE, MAT_ANIMATING, MAT_GONE };
/* Snow-ground animal kinds. The four cats share one shape with palettes. */
enum {
    ANIMAL_SQUIRREL, ANIMAL_REINDEER, ANIMAL_RABBIT, ANIMAL_CAT_COFFEE,
    ANIMAL_CAT_BLACK, ANIMAL_CAT_BLACK_WHITE, ANIMAL_CAT_GRAY, ANIMAL_GROUNDHOG, ANIMAL_KIND_COUNT
};
typedef struct {
    float x, y, vx, vy;
    uint8_t size; /* 0 small '.', 1 '*', 2 rare 3x3 ASCII flake */
} snow_flake_t;
typedef struct {
    float x, ground_y, phase, idle_time;
    uint8_t kind;
    int8_t dir;
    bool active, fleeing; /* fleeing: tapped or timed out, now exiting */
} animal_t;
/* One interactive material instance. The anchor keeps rotating with the
 * tree even while the material is gone, so a replacement can only appear
 * after the anchor has passed behind the tree. */
typedef struct {
    int16_t x, y, z;
    uint8_t type;
    uint8_t state;
    bool was_front;
} material_t;
/* A ground gift box: geometry points stay relative to the slot's initial
 * centre, dx/dz move the whole box to its current ground anchor. */
typedef struct {
    float dx, dz;
    float exit_time; /* >= 0 while sinking away after its interaction */
    uint8_t state;
    bool was_front;
} gift_t;
typedef struct {
    float x, y, vx, vy, life, max_life, gravity;
    uint32_t rgb;
    uint16_t glyph;
    uint8_t bounces;
} particle_t;
typedef struct {
    float time, duration;
    float origin_x, origin_y;
    int8_t kind;
    int8_t index; /* tree material index, or gift slot */
    bool active, fired; /* fired: mid-effect trigger (star/lamps) done */
} effect_t;
typedef struct {
    snow_flake_t snow[SNOW_CAPACITY];
    material_t materials[TREE_MATERIAL_COUNT];
    gift_t gifts[GIFT_COUNT];
    particle_t particles[PARTICLE_CAPACITY];
    effect_t effects[EFFECT_CAPACITY];
    animal_t animal;
    float animal_timer, rotation_total, next_refill_rotation;
    float globe_time, lamp_sweep, star_flash;
    tree_point_t branches[BRANCH_CAPACITY], lamps[LAMP_COUNT];
    tree_point_t trunk[TRUNK_CAPACITY], roots[ROOT_CAPACITY];
    tree_point_t box_body[BOX_BODY_CAPACITY], box_lid[BOX_LID_CAPACITY];
    unsigned branch_count, trunk_count, root_count, box_body_count, box_lid_count;
    uint16_t horizon[480]; /* Snow-drift top edge per screen column. */
    tree_cell_t cells[TREE_ROWS][TREE_COLUMNS];
    float angle, speed, time, pulse, gift_time;
    int gift, start_x, start_y, last_x, last_y;
    bool pressed, dragged;
    unsigned taps;
    uint8_t pending_effects[EFFECT_CAPACITY];
    unsigned pending_head, pending_count;
#ifdef ESP_PLATFORM
    int64_t last_draw_us, total_interval_us, max_interval_us;
    unsigned measured_frames;
#endif
} tree_scene_t;

void tree_init_geometry(tree_scene_t *s);
void tree_prepare(tree_scene_t *s);
void tree_tick(tree_scene_t *s);
void tree_pointer(tree_scene_t *s, int x, int y, bool down);
/** Pop one queued interaction effect id (TREE_EFFECT_*), or TREE_EFFECT_NONE. */
int tree_take_effect(tree_scene_t *s);
void tree_draw(const esp_gsp_canvas_surface_t *surface, void *ctx);
