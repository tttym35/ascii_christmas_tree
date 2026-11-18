cmake_minimum_required(VERSION 3.20)
project(hello_backend LANGUAGES C)

if(NOT ESP_GSP_COMPONENT_DIR AND DEFINED ENV{ESP_GSP_COMPONENT_DIR})
    set(ESP_GSP_COMPONENT_DIR "$ENV{ESP_GSP_COMPONENT_DIR}")
endif()
if(NOT ESP_GSP_COMPONENT_DIR)
    foreach(candidate IN ITEMS
            "${CMAKE_CURRENT_LIST_DIR}/../../../managed_components/espressif__esp-gsp")
        get_filename_component(candidate "${candidate}" ABSOLUTE)
        if(EXISTS "${candidate}/tools/sim_bridge/CMakeLists.txt")
            set(ESP_GSP_COMPONENT_DIR "${candidate}")
            break()
        endif()
    endforeach()
endif()
if(NOT EXISTS "${ESP_GSP_COMPONENT_DIR}/tools/sim_bridge/CMakeLists.txt")
    message(FATAL_ERROR
        "Install espressif/esp-gsp first, or set ESP_GSP_COMPONENT_DIR")
endif()

add_subdirectory("${ESP_GSP_COMPONENT_DIR}/tools/sim_bridge" sim_bridge)
gsp_add_backend(hello_backend
    SOURCES ../tree_scene.c ../tree_greeting.c ../tree_animals.c ../app_hello.c platform_pc.c
    INCLUDE_DIRECTORIES ../include
    SCENES ../ui/main.json)

if(APPLE)
    enable_language(OBJC)
    target_sources(hello_backend PRIVATE music.m)
    set_source_files_properties(music.m PROPERTIES COMPILE_FLAGS "-fobjc-arc")
    target_compile_definitions(hello_backend PRIVATE TREE_MAC_AUDIO
        TREE_MUSIC_PATH="${CMAKE_CURRENT_LIST_DIR}/../../../main/audio/snow_music_box.wav")
    target_link_libraries(hello_backend PRIVATE "-framework AVFoundation" "-framework Foundation")
endif()
