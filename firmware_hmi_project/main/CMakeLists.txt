idf_component_register(
    SRCS
        "main.c"
        "hmi_can.c"
        "hmi_telemetry.c"
        "ai/screen_registry.c"
        "ai/page_predictor.c"
        "ui/ui_futuristic.c"
        "ui/ui_screen_home.c"
        "ui/ui_screen_menu.c"
        "ui/ui_screen_rectification.c"
        "ui/ui_screens_modes.c"
        "ui/ui_generated.c"
        "ui/fonts/cyr_12.c"
        "ui/fonts/cyr_14.c"
        "ui/fonts/cyr_16.c"
        "ui/fonts/cyr_18.c"
        "ui/fonts/cyr_20.c"
        "ui/fonts/cyr_24.c"
        "ui/fonts/cyr_28.c"
        "ui/fonts/cyr_32.c"
        "../components/waveshare_rgb_lcd_port.c"
    INCLUDE_DIRS
        "."
        "ui"
        "ui/fonts"
        "ai"
        "../components"
    REQUIRES
        driver
        can_hmi
        esp_lcd
        esp_timer
        esp_pm
        json
        nvs_flash
        lvgl
        esp_lcd_touch
        esp_lcd_touch_gt911
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE LV_LVGL_H_INCLUDE_SIMPLE)

idf_build_get_property(build_components BUILD_COMPONENTS)
if("lvgl" IN_LIST build_components)
    idf_component_get_property(lvgl_lib lvgl COMPONENT_LIB)
elseif("lvgl__lvgl" IN_LIST build_components)
    idf_component_get_property(lvgl_lib lvgl__lvgl COMPONENT_LIB)
endif()

if(lvgl_lib)
    target_compile_options(${lvgl_lib} PRIVATE -Wno-format)
endif()
