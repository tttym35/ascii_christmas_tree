// SPDX-License-Identifier: Apache-2.0
#include "tree_music.h"
#include "bsp/esp_mosaico.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

enum { AUDIO_CHUNK_BYTES = 2048, WAV_HEADER_BYTES = 44, DEFAULT_VOLUME = 60 };
/* The codec stays at full volume; loudness comes from per-stream gains. */
enum { CODEC_VOLUME = 100, GAIN_ONE = 32768 };

/* Fixed build assets: PCM WAV, 24 kHz mono, 16-bit, canonical 44-byte header. */
extern const uint8_t wav_start[] asm("_binary_snow_music_box_wav_start");
extern const uint8_t wav_end[] asm("_binary_snow_music_box_wav_end");
#define SFX_ASSET(name) \
    extern const uint8_t name##_start[] asm("_binary_" #name "_wav_start"); \
    extern const uint8_t name##_end[] asm("_binary_" #name "_wav_end")
SFX_ASSET(sfx_blue_gift);
SFX_ASSET(sfx_red_gift);
SFX_ASSET(sfx_bow);
SFX_ASSET(sfx_bell);
SFX_ASSET(sfx_cane);
SFX_ASSET(sfx_bauble);
SFX_ASSET(sfx_star);
SFX_ASSET(sfx_stocking);
SFX_ASSET(sfx_star_ding);
SFX_ASSET(sfx_snow_step);
static const struct {
    const uint8_t *start, *end;
} SFX[TREE_SFX_COUNT] = {
    {sfx_blue_gift_start, sfx_blue_gift_end}, {sfx_red_gift_start, sfx_red_gift_end},
    {sfx_bow_start, sfx_bow_end}, {sfx_bell_start, sfx_bell_end},
    {sfx_cane_start, sfx_cane_end}, {sfx_bauble_start, sfx_bauble_end},
    {sfx_star_start, sfx_star_end}, {sfx_stocking_start, sfx_stocking_end},
    {sfx_star_ding_start, sfx_star_ding_end}, {sfx_snow_step_start, sfx_snow_step_end},
};
static QueueHandle_t sfx_queue;
static volatile int requested_music = DEFAULT_VOLUME, requested_sfx = DEFAULT_VOLUME;

void tree_music_play_sfx(tree_sfx_t sfx)
{
    if (sfx_queue && sfx < TREE_SFX_COUNT) {
        (void)xQueueSend(sfx_queue, &sfx, 0);
    }
}
static int clamp_volume(int volume)
{
    return volume < 0 ? 0 : volume > 100 ? 100 : volume;
}
void tree_music_set_volumes(int music_volume, int sfx_volume)
{
    requested_music = clamp_volume(music_volume);
    requested_sfx = clamp_volume(sfx_volume);
}
/* Same mapping as esp_codec_dev's default curve: 0 mutes, 1..100 maps
 * linearly from -50 dB to 0 dB. Returned as a Q15 amplitude gain. */
static int volume_gain(int volume)
{
    return volume <= 0 ? 0 : (int)lroundf(GAIN_ONE * powf(10.f, (-50.f + 0.5f * volume) / 20.f));
}
static void playback(void *buffer)
{
    esp_codec_dev_handle_t speaker = bsp_audio_codec_speaker_init();
    esp_codec_dev_sample_info_t format = {
        .sample_rate = 24000, .channel = 1, .bits_per_sample = 16
    };
    if (speaker && esp_codec_dev_open(speaker, &format) == ESP_CODEC_DEV_OK) {
        int applied_music = -1, applied_sfx = -1, music_gain = 0, sfx_gain = 0;
        if (esp_codec_dev_set_out_vol(speaker, CODEC_VOLUME) != ESP_CODEC_DEV_OK) {
            ESP_LOGW("tree_music", "Codec volume not set; using its default");
        }
        const uint8_t *p = wav_start + WAV_HEADER_BYTES;
        int active_sfx = -1;
        size_t sfx_sample = 0;
        ESP_LOGI("tree_music", "Playing %u-byte music-box loop, effects mixed in",
                 (unsigned)(wav_end - wav_start - WAV_HEADER_BYTES));
        for (;;) {
            if (requested_music != applied_music || requested_sfx != applied_sfx) {
                applied_music = requested_music;
                applied_sfx = requested_sfx;
                music_gain = volume_gain(applied_music);
                sfx_gain = volume_gain(applied_sfx);
            }
            if (active_sfx < 0) {
                tree_sfx_t next;
                if (xQueueReceive(sfx_queue, &next, 0) == pdTRUE) {
                    active_sfx = next;
                    sfx_sample = 0;
                }
            }
            size_t remaining = wav_end - p;
            size_t chunk = remaining < AUDIO_CHUNK_BYTES ? remaining : AUDIO_CHUNK_BYTES;
            memcpy(buffer, p, chunk);
            p += chunk;
            if (p == wav_end) {
                p = wav_start + WAV_HEADER_BYTES;
            }
            int16_t *mix = buffer;
            if (active_sfx >= 0) {
                /* Duck the music to about half and add the effect. */
                const int16_t *sfx = (const int16_t *)(SFX[active_sfx].start + WAV_HEADER_BYTES);
                size_t sfx_total = (SFX[active_sfx].end - SFX[active_sfx].start
                                    - WAV_HEADER_BYTES) / 2;
                for (size_t i = 0; i < chunk / 2; ++i) {
                    int effect = sfx_sample < sfx_total ? sfx[sfx_sample++] : 0;
                    int v = (mix[i] * music_gain / 2 + effect * sfx_gain) / GAIN_ONE;
                    mix[i] = v > 32767 ? 32767 : v < -32768 ? -32768 : v;
                }
                if (sfx_sample >= sfx_total) {
                    active_sfx = -1;
                }
            } else {
                for (size_t i = 0; i < chunk / 2; ++i) {
                    mix[i] = (int16_t)(mix[i] * music_gain / GAIN_ONE);
                }
            }
            if (esp_codec_dev_write(speaker, buffer, chunk) != ESP_CODEC_DEV_OK) {
                break;
            }
        }
        esp_codec_dev_close(speaker);
    }
    ESP_LOGW("tree_music", "Speaker playback unavailable");
    free(buffer);
    vTaskDelete(NULL);
}
esp_err_t tree_music_start(void)
{
    if (wav_end - wav_start <= WAV_HEADER_BYTES || memcmp(wav_start + 36, "data", 4)) {
        return ESP_ERR_INVALID_SIZE;
    }
    for (int i = 0; i < TREE_SFX_COUNT; ++i)
        if (SFX[i].end - SFX[i].start <= WAV_HEADER_BYTES
                || memcmp(SFX[i].start + 36, "data", 4)) {
            return ESP_ERR_INVALID_SIZE;
        }
    sfx_queue = xQueueCreate(4, sizeof(tree_sfx_t));
    void *buffer = malloc(AUDIO_CHUNK_BYTES);
    if (!sfx_queue || !buffer) {
        free(buffer);
        return ESP_ERR_NO_MEM;
    }
    if (xTaskCreate(playback, "tree_music", 4096, buffer, 3, NULL) != pdPASS) {
        free(buffer);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
