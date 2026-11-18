idf_component_register(SRCS "app_hello.c" "tree_scene.c" "tree_greeting.c" "tree_animals.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_timer log
    REQUIRES esp-gsp)
include("${MOSAICO_UTILS_ROOT}/mosaico-tools/cmake/gsp_bundle.cmake")
mosaico_gsp_add_ui_bundle(${COMPONENT_LIB} "ui/main.json")
