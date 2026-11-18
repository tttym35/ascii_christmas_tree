// SPDX-License-Identifier: Apache-2.0
#include "tree_feedback.h"
#include "tree_music.h"
#include "bsp/esp_mosaico.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static const char *TAG = "tree_feedback";
static QueueHandle_t effect_queue;

static void on_effect(app_hello_effect_t effect, void *ctx)
{
    (void)ctx;
    if (effect_queue) {
        (void)xQueueSend(effect_queue, &effect, 0);
    }
}
static void pulse(uint8_t strength, int ms)
{
    (void)bsp_motor_set_strength(strength);
    vTaskDelay(pdMS_TO_TICKS(ms));
    (void)bsp_motor_set_strength(0);
}
/* One rhythm per interaction, weakest for the blue gift, hardest for the
 * bauble. The effect id also selects the sound, in the same order. */
static void run_pattern(app_hello_effect_t effect)
{
    tree_music_play_sfx((tree_sfx_t)(effect - 1));
    switch (effect) {
    case APP_HELLO_EFFECT_BLUE_GIFT:
        pulse(20, 600);
        break;
    case APP_HELLO_EFFECT_RED_GIFT:
        pulse(40, 300); /* Launch, then the hard bang when the rocket bursts. */
        vTaskDelay(pdMS_TO_TICKS(1500));
        pulse(70, 120);
        break;
    case APP_HELLO_EFFECT_BOW:
        pulse(30, 80);
        vTaskDelay(pdMS_TO_TICKS(70));
        pulse(30, 80);
        break;
    case APP_HELLO_EFFECT_BELL:
        for (int i = 0; i < 3; ++i) {
            pulse(50, 70);
            vTaskDelay(pdMS_TO_TICKS(60));
        }
        break;
    case APP_HELLO_EFFECT_CANE:
        pulse(70, 90);
        break;
    case APP_HELLO_EFFECT_BAUBLE:
        pulse(80, 140);
        break;
    case APP_HELLO_EFFECT_STAR:
        pulse(25, 200);
        break;
    case APP_HELLO_EFFECT_STOCKING:
        for (int i = 0; i < 3; ++i) {
            pulse(45, 70);
            vTaskDelay(pdMS_TO_TICKS(60));
        }
        break;
    case APP_HELLO_EFFECT_STAR_DING:
    case APP_HELLO_EFFECT_ANIMAL:
        break; /* Sound only: the star ding and snow steps never vibrate. */
    default:
        break;
    }
}
static void feedback_task(void *arg)
{
    (void)arg;
    if (bsp_motor_init() != ESP_OK) {
        ESP_LOGW(TAG, "Motor unavailable; effects stay audio-only");
    }
    app_hello_effect_t effect;
    for (;;) {
        if (xQueueReceive(effect_queue, &effect, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "effect=%d", (int)effect);
            run_pattern(effect);
        }
    }
}
esp_err_t tree_feedback_start(app_hello_handle_t controller)
{
    if (!controller) {
        return ESP_ERR_INVALID_ARG;
    }
    effect_queue = xQueueCreate(4, sizeof(app_hello_effect_t));
    if (!effect_queue) {
        return ESP_ERR_NO_MEM;
    }
    esp_gsp_err_t err = app_hello_set_effect_handler(controller, on_effect, NULL);
    if (err != ESP_GSP_OK) {
        return ESP_FAIL;
    }
    if (xTaskCreate(feedback_task, "tree_feedback", 4096, NULL, 3, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
