// SPDX-License-Identifier: Apache-2.0
#include <stdio.h>
#include <stdlib.h>
#include "app_hello.h"
#include "gsp_sim_bridge.h"

#ifdef TREE_MAC_AUDIO
void tree_music_start(void);
void tree_music_stop(void);
#endif

/* The bridge ABI has no caller context. Keep only its owned opaque handle here. */
static struct {
    app_hello_handle_t controller;
} backend;

esp_gsp_err_t gsp_bridge_app_init(esp_gsp_handle_t ui)
{
    esp_gsp_err_t err = app_hello_create(ui, &backend.controller);
    /* The simulator has no AI key: TREE_SIM_VOLUME_PANEL=1 opens the volume
     * panel at start so its sliders and tap-outside close can be checked. */
    const char *panel = getenv("TREE_SIM_VOLUME_PANEL");
    if (err == ESP_GSP_OK && panel && panel[0] == '1') {
        err = app_hello_toggle_volume_panel(backend.controller);
    }
#ifdef TREE_MAC_AUDIO
    if (err == ESP_GSP_OK) {
        tree_music_start();
    }
#endif
    return err;
}

void gsp_bridge_app_deinit(esp_gsp_handle_t ui)
{
    (void)ui;
#ifdef TREE_MAC_AUDIO
    tree_music_stop();
#endif
    app_hello_snapshot_t snapshot;
    if (app_hello_read(backend.controller, &snapshot) == ESP_GSP_OK) {
        fprintf(stderr, "tree_backend: taps=%u, last_error=%d\n",
                (unsigned)snapshot.count, (int)snapshot.last_error);
    }
    /* The bridge calls deinit after poll exits; no callbacks can use the state. */
    const esp_gsp_err_t err = app_hello_destroy(&backend.controller);
    if (err != ESP_GSP_OK) {
        fprintf(stderr, "hello_backend: cleanup failed (%d)\n", err);
    }
}
