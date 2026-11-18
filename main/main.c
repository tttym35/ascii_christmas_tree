// SPDX-License-Identifier: Apache-2.0
#include "app_display.h"
#include "bsp/esp_mosaico.h"
#include "tree_feedback.h"
#include "tree_music.h"
#include "app_hello.h"
#include "esp_iris.h"
#include "esp_log.h"
#include "iris_ota_support.h"
#include "iris_screen_mirror.h"
#include "nvs_flash.h"
#include "ui_bundle.h"

static const char *TAG = "ascii_christmas_tree";

/* The composition root retains owned handles, never controller internals. */
static struct {
    esp_gsp_handle_t ui;
    app_hello_handle_t controller;
} application;

static void on_volume(int music_volume, int sfx_volume, void *ctx)
{
    (void)ctx;
    tree_music_set_volumes(music_volume, sfx_volume);
}
static void on_ai_key(void *button, void *ctx)
{
    (void)button;
    (void)ctx;
    (void)app_hello_toggle_volume_panel(application.controller);
}
static void init_ai_key(void)
{
    static button_handle_t buttons[BSP_BUTTON_NUM];
    int count = 0;
    if (bsp_iot_button_create(buttons, &count, BSP_BUTTON_NUM) != ESP_OK
            || iot_button_register_cb(buttons[BSP_BUTTON_AI], BUTTON_SINGLE_CLICK, NULL,
                                      on_ai_key, NULL) != ESP_OK) {
        ESP_LOGW(TAG, "AI key unavailable; volume panel stays hidden");
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_iris_boot_probe());
    ESP_ERROR_CHECK(nvs_flash_init());
    /* Recovery must remain reachable even when the UI bundle cannot be loaded. */
    iris_ota_support_start();

    esp_gsp_config_t config;
    const esp_err_t err = ui_bundle_open(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "GSP UI unavailable (0x%x); Recovery RPC remains active", err);
        return;
    }
    esp_gsp_esp_lcd_config_t display;
    const esp_err_t display_err = app_display_init(&display);
    if (display_err != ESP_OK) {
        ESP_LOGE(TAG, "Display unavailable (0x%x); Recovery RPC remains active", display_err);
        return;
    }
    ESP_ERROR_CHECK(iris_screen_mirror_init());
    ESP_ERROR_CHECK(esp_gsp_esp_lcd_start(&config, &display, &application.ui));
    ESP_ERROR_CHECK(iris_screen_mirror_attach(application.ui));
    ESP_ERROR_CHECK(app_hello_create(application.ui, &application.controller));
    /* GSP retains the callback context. Device UI/controller live until reboot;
     * teardown requires quiescing GSP before app_hello_destroy(), then closing UI. */
    ESP_ERROR_CHECK_WITHOUT_ABORT(tree_music_start());
    ESP_ERROR_CHECK_WITHOUT_ABORT(tree_feedback_start(application.controller));
    (void)app_hello_set_volume_handler(application.controller, on_volume, NULL);
    init_ai_key();
    ESP_LOGI(TAG, "ASCII Christmas tree ready");
}
