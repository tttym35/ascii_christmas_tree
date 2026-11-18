// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "esp_err.h"

/** Sound effects in interaction-effect order (app_hello_effect_t minus NONE). */
typedef enum {
    TREE_SFX_BLUE_GIFT, TREE_SFX_RED_GIFT, TREE_SFX_BOW, TREE_SFX_BELL,
    TREE_SFX_CANE, TREE_SFX_BAUBLE, TREE_SFX_STAR, TREE_SFX_STOCKING,
    TREE_SFX_STAR_DING, TREE_SFX_SNOW_STEP, TREE_SFX_COUNT
} tree_sfx_t;

/** Start the board's music loop once after UI startup. The playback task
 * owns the speaker; effects are mixed into its stream on request. */
esp_err_t tree_music_start(void);
/** Queue one effect to mix over the music (music ducks to about half). */
void tree_music_play_sfx(tree_sfx_t sfx);
/** Set background-music and effect volumes, each 0..100, applied by the
 * playback task as separate software gains on the -50..0 dB curve that the
 * codec volume used before (0 mutes). */
void tree_music_set_volumes(int music_volume, int sfx_volume);
