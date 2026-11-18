// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_gsp.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Opaque UI controller; ownership and threading contracts: ../README.md. */
typedef struct app_hello *app_hello_handle_t;

/** Value copy for diagnostics; no pointers into private controller state. */
typedef struct {
    uint32_t count;             /**< Recognized star and gift taps. */
    bool celebrating;           /**< True while a gift celebration is active. */
    esp_gsp_err_t last_error;   /**< Most recent callback/setter error, or ESP_GSP_OK. */
} app_hello_snapshot_t;

/** Interaction effects, one per material kind. The controller only reports
 * them; sound and vibration stay with the composition root in main. */
typedef enum {
    APP_HELLO_EFFECT_NONE, APP_HELLO_EFFECT_BLUE_GIFT, APP_HELLO_EFFECT_RED_GIFT,
    APP_HELLO_EFFECT_BOW, APP_HELLO_EFFECT_BELL, APP_HELLO_EFFECT_CANE,
    APP_HELLO_EFFECT_BAUBLE, APP_HELLO_EFFECT_STAR, APP_HELLO_EFFECT_STOCKING,
    APP_HELLO_EFFECT_STAR_DING, APP_HELLO_EFFECT_ANIMAL
} app_hello_effect_t;
typedef void (*app_hello_effect_cb_t)(app_hello_effect_t effect, void *ctx);
/** Volume sink: background music and sound effects, each 0..100. */
typedef void (*app_hello_volume_cb_t)(int music_volume, int sfx_volume, void *ctx);

/** Create the tree controller for ui; the caller owns *out_handle.
 * ui/out_handle must be non-NULL, and ui must outlive the controller.
 * Returns ESP_GSP_OK or a GSP error; clears *out_handle on failure. */
esp_gsp_err_t app_hello_create(esp_gsp_handle_t ui, app_hello_handle_t *out_handle);

/** Register the interaction-effect sink (NULL clears it). The callback runs
 * on the UI loop; it must only queue work, never block or drive hardware. */
esp_gsp_err_t app_hello_set_effect_handler(app_hello_handle_t handle,
                                           app_hello_effect_cb_t cb, void *ctx);

/** Register the volume sink for the two on-screen sliders (NULL clears it).
 * Called on the UI loop whenever either slider value changes. */
esp_gsp_err_t app_hello_set_volume_handler(app_hello_handle_t handle,
                                           app_hello_volume_cb_t cb, void *ctx);

/** Request to open or close the volume panel (the board's AI key calls this).
 * Safe from any task: it only sets a flag that the next UI tick applies.
 * While the panel is open, a tap outside it closes it without reaching the
 * tree. */
esp_gsp_err_t app_hello_toggle_volume_panel(app_hello_handle_t handle);

/** Copy diagnostics to out_snapshot; both arguments must be non-NULL.
 * Call on the UI loop or after dispatch stops; this is not a cross-thread read.
 * Returns ESP_GSP_OK or ESP_GSP_ERR_INVALID_ARG; output unchanged on error. */
esp_gsp_err_t app_hello_read(app_hello_handle_t handle, app_hello_snapshot_t *out_snapshot);

/** Detach callbacks, release the controller and clear *handle.
 * Quiesce event/timer dispatch first and keep the borrowed ui alive.
 * Returns ESP_GSP_OK or a GSP error; retains the handle if cleanup fails. */
esp_gsp_err_t app_hello_destroy(app_hello_handle_t *handle);

#ifdef __cplusplus
}
#endif
