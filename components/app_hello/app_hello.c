// SPDX-License-Identifier: Apache-2.0
#include "app_hello.h"
#include "tree_scene.h"
#define GSP_BUNDLE_ENABLE_RAW_IDS
#include "bundle_gsp.h"
#include <stdatomic.h>
#include <stdlib.h>

/* Volume panel rectangle; must match the volume_panel container in
 * ui/main.json. Taps outside it close the panel. */
enum { PANEL_X = 40, PANEL_Y = 150, PANEL_W = 400, PANEL_H = 132, DEFAULT_VOLUME = 60 };

/* Who receives the current touch, decided by where it first pressed. */
typedef enum { TOUCH_TREE, TOUCH_PANEL, TOUCH_IGNORED } touch_owner_t;

struct app_hello {
    tree_scene_t scene;
    esp_gsp_handle_t ui;
    void *timer;
    esp_gsp_err_t error;
    app_hello_effect_cb_t effect_cb;
    void *effect_ctx;
    app_hello_volume_cb_t volume_cb;
    void *volume_ctx;
    int music_volume, sfx_volume;
    bool panel_visible;
    bool finger_down;
    uint8_t touch_owner; /* touch_owner_t, decided once when a touch starts. */
    atomic_bool panel_toggle_requested;
};

/* Hand queued interaction effects to the registered sink. Runs on the UI
 * loop (tick or pointer), so the sink only queues hardware work. */
static void drain_effects(struct app_hello *s)
{
    int kind;
    while ((kind = tree_take_effect(&s->scene)) != TREE_EFFECT_NONE) {
        if (s->effect_cb) {
            s->effect_cb((app_hello_effect_t)kind, s->effect_ctx);
        }
    }
}
/* Runs on the UI loop only. Opening the panel ends any tree drag in
 * progress, so the tree does not stay "pressed" while the panel owns input. */
static void set_panel_visible(struct app_hello *s, bool visible)
{
    s->panel_visible = visible;
    if (visible) {
        s->scene.pressed = false;
        s->scene.dragged = false;
        if (s->finger_down) {
            s->touch_owner = TOUCH_IGNORED; /* Finish that touch untouched. */
        }
    }
    esp_gsp_err_t err = esp_gsp_set_visible(s->ui, GSP_TREE_BIND_VOLUME_PANEL, visible);
    if (err == ESP_GSP_OK && visible) {
        err = esp_gsp_set_value(s->ui, GSP_TREE_BIND_MUSIC_VOLUME, s->music_volume);
    }
    if (err == ESP_GSP_OK && visible) {
        err = esp_gsp_set_value(s->ui, GSP_TREE_BIND_SFX_VOLUME, s->sfx_volume);
    }
    if (err != ESP_GSP_OK) {
        s->error = err;
    }
}
static bool inside_panel(int32_t x, int32_t y)
{
    return x >= PANEL_X && x < PANEL_X + PANEL_W && y >= PANEL_Y && y < PANEL_Y + PANEL_H;
}
/* Only where a touch first presses matters. Pressed inside the open panel:
 * the sliders own it to release, even if the finger slides out. Pressed
 * outside the open panel: close the panel and ignore the rest of that touch,
 * so it never taps or drags the tree. Returns true unless the tree owns it. */
static bool panel_owns_touch(struct app_hello *s, int32_t x, int32_t y, bool pressed)
{
    if (pressed && !s->finger_down) {
        s->finger_down = true;
        if (!s->panel_visible) {
            s->touch_owner = TOUCH_TREE;
        } else if (inside_panel(x, y)) {
            s->touch_owner = TOUCH_PANEL;
        } else {
            set_panel_visible(s, false);
            s->touch_owner = TOUCH_IGNORED;
        }
    }
    bool owned = s->touch_owner != TOUCH_TREE;
    if (!pressed) {
        s->finger_down = false;
        s->touch_owner = TOUCH_TREE;
    }
    return owned;
}
static void tick(esp_gsp_handle_t ui, void *ctx)
{
    struct app_hello *s = ctx;
    tree_tick(&s->scene);
    drain_effects(s);
    if (atomic_exchange(&s->panel_toggle_requested, false)) {
        set_panel_visible(s, !s->panel_visible);
    }
    if (s->panel_visible) {
        int32_t music = s->music_volume, sfx = s->sfx_volume;
        bool ok = esp_gsp_get_value(ui, GSP_TREE_BIND_MUSIC_VOLUME, &music) == ESP_GSP_OK
                  && esp_gsp_get_value(ui, GSP_TREE_BIND_SFX_VOLUME, &sfx) == ESP_GSP_OK;
        if (ok && (music != s->music_volume || sfx != s->sfx_volume)) {
            s->music_volume = music;
            s->sfx_volume = sfx;
            if (s->volume_cb) {
                s->volume_cb(music, sfx, s->volume_ctx);
            }
        }
    }
    esp_gsp_err_t err = esp_gsp_canvas_invalidate(ui, GSP_TREE_BIND_FOREST);
    if (err != ESP_GSP_OK) {
        s->error = err;
    }
}
static void pointer(esp_gsp_handle_t ui, int32_t x, int32_t y, bool down, void *ctx)
{
    (void)ui;
    struct app_hello *s = ctx;
    if (panel_owns_touch(s, x, y, down)) {
        return;
    }
    tree_pointer(&s->scene, x, y, down);
    drain_effects(s);
}
esp_gsp_err_t app_hello_create(esp_gsp_handle_t ui, app_hello_handle_t *out)
{
    if (!out || !ui) {
        return ESP_GSP_ERR_INVALID_ARG;
    }
    *out = NULL;
    struct app_hello *s = calloc(1, sizeof(*s));
    if (!s) {
        return ESP_GSP_ERR_NO_MEM;
    }
    s->ui = ui;
    s->music_volume = s->sfx_volume = DEFAULT_VOLUME;
    atomic_init(&s->panel_toggle_requested, false);
    s->scene.angle = 0.45f;
    s->scene.speed = 0.31f;
    s->scene.pulse = s->scene.gift_time = -1;
    tree_init_geometry(&s->scene);
    tree_prepare(&s->scene);
    esp_gsp_err_t err = esp_gsp_set_pointer_observer(ui, pointer, s);
    if (err == ESP_GSP_OK) {
        s->timer = esp_gsp_timer_create(ui, TREE_TICK_MS, tick, s);
        if (!s->timer) {
            err = ESP_GSP_ERR_NO_MEM;
        }
    }
    if (err == ESP_GSP_OK) {
        err = esp_gsp_canvas_set_draw_cb(ui, GSP_TREE_BIND_FOREST, tree_draw, &s->scene);
    }
    if (err != ESP_GSP_OK) {
        if (s->timer) {
            esp_gsp_timer_delete(ui, s->timer);
        }
        esp_gsp_set_pointer_observer(ui, NULL, NULL);
        free(s);
        return err;
    }
    *out = s;
    return ESP_GSP_OK;
}
esp_gsp_err_t app_hello_set_effect_handler(app_hello_handle_t s, app_hello_effect_cb_t cb,
                                           void *ctx)
{
    if (!s) {
        return ESP_GSP_ERR_INVALID_ARG;
    }
    s->effect_cb = cb;
    s->effect_ctx = ctx;
    return ESP_GSP_OK;
}
esp_gsp_err_t app_hello_set_volume_handler(app_hello_handle_t s, app_hello_volume_cb_t cb,
                                           void *ctx)
{
    if (!s) {
        return ESP_GSP_ERR_INVALID_ARG;
    }
    s->volume_cb = cb;
    s->volume_ctx = ctx;
    return ESP_GSP_OK;
}
esp_gsp_err_t app_hello_toggle_volume_panel(app_hello_handle_t s)
{
    if (!s) {
        return ESP_GSP_ERR_INVALID_ARG;
    }
    atomic_store(&s->panel_toggle_requested, true);
    return ESP_GSP_OK;
}
esp_gsp_err_t app_hello_read(app_hello_handle_t s, app_hello_snapshot_t *out)
{
    if (!s || !out) {
        return ESP_GSP_ERR_INVALID_ARG;
    }
    *out = (app_hello_snapshot_t) {
        s->scene.taps, s->scene.gift_time >= 0 || s->scene.globe_time >= 0, s->error
    };
    return ESP_GSP_OK;
}
esp_gsp_err_t app_hello_destroy(app_hello_handle_t *handle)
{
    if (!handle) {
        return ESP_GSP_ERR_INVALID_ARG;
    }
    struct app_hello *s = *handle;
    if (!s) {
        return ESP_GSP_OK;
    }
    esp_gsp_err_t err = esp_gsp_set_pointer_observer(s->ui, NULL, NULL);
    if (err != ESP_GSP_OK) {
        return err;
    }
    if (s->timer) {
        err = esp_gsp_timer_delete(s->ui, s->timer);
        if (err != ESP_GSP_OK) {
            return err;
        }
        s->timer = NULL;
    }
    err = esp_gsp_canvas_stop(s->ui, GSP_TREE_BIND_FOREST);
    if (err == ESP_GSP_OK) {
        err = esp_gsp_flush(s->ui, 1000);
    }
    if (err != ESP_GSP_OK) {
        return err;
    }
    free(s);
    *handle = NULL;
    return ESP_GSP_OK;
}
