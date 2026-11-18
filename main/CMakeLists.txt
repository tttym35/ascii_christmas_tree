idf_component_register(SRCS "main.c" "tree_music.c" "tree_feedback.c"
    EMBED_FILES "audio/snow_music_box.wav" "audio/sfx_blue_gift.wav" "audio/sfx_red_gift.wav"
        "audio/sfx_bow.wav" "audio/sfx_bell.wav" "audio/sfx_cane.wav" "audio/sfx_bauble.wav"
        "audio/sfx_star.wav" "audio/sfx_stocking.wav" "audio/sfx_star_ding.wav"
        "audio/sfx_snow_step.wav"
    PRIV_REQUIRES esp-mosaico-bsp app_display app_hello esp_iris esp_mosaico_app_recovery
        esp_mosaico_gsp_bundle esp_mosaico_gsp_iris nvs_flash log)
