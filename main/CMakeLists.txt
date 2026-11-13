# Корень PWA-оболочки. Файлы лежат в ../littlefs, потому что оттуда же
# собирается образ раздела `assets` (littlefs, 1 МБ). Одни и те же исходники
# используются дважды:
#   1) EMBED_FILES — встроенная копия, работает без /lfs и всегда
#      соответствует прошивке (авторитетная для HTTP-обработчиков);
#   2) automatika_littlefs_image — образ build/assets.bin: канонический
#      источник файлов и артефакт прошивки раздела.
set(WEB_DIR "${CMAKE_CURRENT_LIST_DIR}/../littlefs")

idf_component_register(
    SRCS
        "main.c" "selftest.c"
        "core/control_config.c" "core/estop.c" "core/skill_auth.c"
        "core/flash_fs.c" "core/nvs_migration.c" "core/config_backup.c"
        "core/health.c" "core/event_log.c"
        "core/i18n.c" "core/profiles.c" "core/sd_resilience.c"
        "core/session_recovery.c"
        "level/level_sensor.c"
        "core/state.c" "core/events.c" "core/roles.c" "core/flags.c"
        "core/session_manager.c"
        "core/storage.c" "core/watchdog.c" "core/valve_config.c"
        "core/process_control.c"
        "core/menu_engine.c" "core/menu_tree.c"
        "core/menu_callbacks.c" "core/menu_validator.c"
        "core/help_db.c" "core/learning.c" "core/recipes_db.c"
        "core/sd_content.c"
        "net/wifi_prov.c" "net/time_sync.c" "net/mdns_service.c"
        "hal/hal_i2c.c" "hal/hal_1wire.c" "hal/temp_filter.c" "hal/sensor_pipeline.c"
        "hal/hal_adc.c" "hal/hal_dac.c"
        "hal/hal_relay.c" "hal/hal_display.c" "hal/hal_touch.c"
        "hal/hal_sd.c" "hal/hal_hx711.c" "hal/hal_usb.c"
        "hal/baro.c" "hal/flow_sensor.c" "hal/pressure_sensor.c"
        "control/ctrl_safety.c" "control/ctrl_pid.c" "control/autopid_glue.cpp"
        "control/ctrl_heater.c" "control/ctrl_dose.c" "control/pump_doser.c"
        "control/ctrl_valves.c" "control/ctrl_valve_control.c"
        "control/ctrl_forecast.c" "control/valve_calibration.c"
        "control/ctrl_scale.c" "control/ctrl_autolearn.c"
        "control/ctrl_alcohol_balance.c" "control/ctrl_time_predict.c"
        "control/ctrl_sensor_calib.c"
        "control/fraction_logic.c" "control/fraction_periodic.c" "control/alcohol_balance.c"
        "control/param_registry.c"
        "control/select_algorithm.c"
        "control/regulator_driver.c"
        "control/safety_gate.c"
        "control/adaptive_pid.c"
        "verify/verify_config.c"
        "verify/verify_db.c"
        "verify/shadow.c"
        "verify/pid_metrics.c"
        "verify/verify_fuzz.c"
        "control/reflux.c"
        "control/pid_autotune.c"
        "modes/mode_base.c" "modes/mode_registry.c"
        "modes/mode_distill.c" "modes/mode_rectify.c" "modes/mode_beer.c"
        "modes/mode_pvk.c" "modes/mode_nbk.c" "modes/mode_vacuum.c"
        "modes/mode_double.c" "modes/mode_ferment.c"
        "modes/beer_recipe.c" "modes/beer_callbacks.c"
        "modes/beer_presets.c" "modes/distill_presets.c"
        "modes/rectify_presets.c" "modes/pvk_presets.c" "modes/nbk_presets.c"
        "script/lua_engine.c"
        "ai/screen_registry.c" "ai/page_predictor.c"
        "ai/action_predictor.c" "ai/anomaly.c" "ai/tinyml_predict.c"
        "ai/cache_manager.c" "ai/ui_preloader.c"
        "ai/nn_pie.c" "ai/screen_serialize.c"
        "ui/ui.c" "ui/ui_theme.c" "ui/ui_glass_theme.c" "ui/ui_screen_home.c"
        "ui/ui_menu.c" "ui/ui_gestures.c" "ui/haptic.c"
        "ui/ui_session_indicator.c"
        "ui/ui_screen_advanced.c"
        "ui/ui_screen_selection.c"
        "ui/fonts/lv_font_cyr_16.c" "ui/fonts/lv_font_cyr_24.c"
        "ui/ui_builder.c"
        "ui/ui_screens.c" "ui/ui_screens_full.c" "ui/ui_welcome.c"
        "ui/ui_graph_sqlite.c"
        "ui/ui_beer_editor.c" "ui/ui_beer_editor_callbacks.c"
        "ui/ui_anim.c" "ui/ui_fps.c" "ui/ui_wd.c"
        "ui/ui_ai.c" "ui/ui_ai_warnings.c" "ui/ui_scale.c" "ui/ui_scale_calib.c"
        "ui/ui_usb.c" "ui/ui_fota.c"
        "ui/ui_valve_calib.c" "ui/ui_alcohol_balance.c"
        "ui/ui_sensor_calib.c" "ui/ui_sensor_calib_callbacks.c"
        "ui/ui_roles.c" "ui/ui_learning.c" "ui/ui_recipes.c"
        "ui/ui_calc_loader.c" "ui/ui_scheme.c" "ui/ui_scheme_menu.c"
        "ui/ui_scheme_canvas.c" "ui/ui_markdown.c" "ui/ui_help.c"
        "ui/ui_icons.c" "ui/ui_physical.c"
        "db/db_manager.c" "db/db_logger.c" "db/db_psram_alloc.c"
        "db/db_migrations.c" "db/db_export.c" "db/db_import.c" "db/db_queries.c"
        "web/web_server.c" "web/web_api.c" "web/web_alice.c" "web/api_auth.c"
        "web/actuator_test.c" "web/rate_limit.c" "web/tg_backup.c"
        "web/qr_wifi.c" "web/brewfather.c" "web/calendar.c"
        "web/mqtt_client.c" "web/telegram_bot.c" "web/telegram_commands.c"
        "web/telegram_notify.c" "web/ws_telemetry.c"
        "web/samovar.c" "web/samovar_blynk.c" "web/samovar_mqtt.c"
        "web/ws_handler.c" "web/http_handlers.c"
        "web/https_server.c"
        "ota/fota.c" "ota/ota_manager.c" "ota/ota_sd.c" "ota/ota_url.c"
        "ota/ota_qr.c" "ota/ota_verify.c" "ota/secure_ota.c"
        "patches/critical_fixes.c"
        "third_party/qrcodegen.c"
    INCLUDE_DIRS "." "third_party"
    # Тестовый компонент main_test (Unity) живёт в test/ и включается
    # отдельным проектом test_app (см. test/README.md).
    # PWA-оболочка: индекс, стили, скрипты, manifest, service worker,
    # офлайн-страница и иконки. Символы _binary_*_start/end не меняются,
    # потому что IDF берёт только basename файла.
    EMBED_FILES
        "${WEB_DIR}/index.html" "${WEB_DIR}/js/app.js" "${WEB_DIR}/js/menu.js"
        "${WEB_DIR}/js/gestures.js" "${WEB_DIR}/js/tab-swipe.js"
        "${WEB_DIR}/js/swipe-delete.js" "${WEB_DIR}/js/pull-refresh.js"
        "${WEB_DIR}/css/tokens.css" "${WEB_DIR}/css/components.css"
        "${WEB_DIR}/vendor/uPlot.iife.min.js" "${WEB_DIR}/vendor/uPlot.min.css"
        "${WEB_DIR}/charts.js" "${WEB_DIR}/api.js" "${WEB_DIR}/ws.js"
        "${WEB_DIR}/recipes.js" "${WEB_DIR}/settings.js"
        "${WEB_DIR}/manifest.json" "${WEB_DIR}/sw.js" "${WEB_DIR}/offline.html"
        "${WEB_DIR}/icons/icon-192.png" "${WEB_DIR}/icons/icon-512.png"
        "${WEB_DIR}/icons/icon-maskable-512.png"
        "${WEB_DIR}/icons/apple-touch-icon.png"
    REQUIRES
        nvs_flash esp_wifi esp_http_server esp_https_server esp_timer
        esp_netif json driver fatfs sdmmc spi_flash
        app_update mqtt esp_http_client esp_lcd spiffs vfs esp_event esp_https_ota
        mbedtls lwip wifi_provisioning
        lvgl__lvgl espressif__esp_lcd_touch espressif__esp_lcd_st7796
        esp_sqlite3 espressif__esp_tinyusb
        espressif__esp_lcd_touch_ft5x06
        espressif__onewire_bus espressif__ds18b20
        espressif__lua hayschan__autopid_for_esp_idf joltwallet__littlefs
)

# Образ littlefs для раздела `assets` (1 МБ, см. partitions.csv).
# Содержимое — PWA-оболочка из ../littlefs: index/app/style, manifest.json,
# sw.js, offline.html, icons/. Образ собирается при каждом `idf.py build`
# в build/assets.bin и прошивается вместе с приложением.
#
# ВНИМАНИЕ: `idf.py flash` перезаписывает раздел assets целиком. Если на
# устройстве уже лежат пользовательские скрипты/профили в /lfs, сделайте
# резервную копию (POST /api/config/export) до прошивки.
#
# FLASH_IN_PROJECT — включать образ в `idf.py flash`. Если нужен только
# бинарник без авто-прошивки раздела, уберите этот флаг.

# littlefs_create_partition_image() из joltwallet__littlefs создаёт venv и
# ставит littlefs-python==0.11.0 из sdist. На Python 3.14 sdist собирается
# только MSVC, которого на машине сборки может не быть. Поэтому заранее
# готовим venv с 0.19.0: для неё публикуется готовый wheel cp314-win_amd64,
# а CLI (create/extract/list) совместим с 0.11.0.
set(LITTLEFS_PY_VENV "${CMAKE_BINARY_DIR}/littlefs_py_venv")
if(CMAKE_HOST_WIN32)
    set(LITTLEFS_PY "${LITTLEFS_PY_VENV}/Scripts/littlefs-python.exe")
else()
    set(LITTLEFS_PY "${LITTLEFS_PY_VENV}/bin/littlefs-python")
endif()
if(NOT EXISTS "${LITTLEFS_PY}")
    message(STATUS "Preparing littlefs-python venv (prebuilt wheel)")
    execute_process(COMMAND ${PYTHON} -m venv "${LITTLEFS_PY_VENV}"
                    RESULT_VARIABLE _lfs_venv_rc)
    if(_lfs_venv_rc EQUAL 0)
        execute_process(
            COMMAND "${LITTLEFS_PY_VENV}/Scripts/pip.exe" install --only-binary=:all:
                    "littlefs-python==0.19.0"
            RESULT_VARIABLE _lfs_pip_rc)
        if(NOT _lfs_pip_rc EQUAL 0)
            message(WARNING "littlefs-python install failed; image build may fail")
        endif()
    else()
        message(WARNING "cannot create littlefs venv; image build may fail")
    endif()
endif()

# Своя цель вместо компонентной: компонентная привязывает OUTPUT к каталогу
# venv. Если pip внутри неё падает (нет MSVC), ninja не пишет запись в
# .ninja_log и запускает команду заново на каждой сборке. Здесь образ
# собирается напрямую уже готовым littlefs-python, а интеграция с
# `idf.py flash` повторяет логику компонента.
function(automatika_littlefs_image partition base_dir)
    set(options FLASH_IN_PROJECT)
    cmake_parse_arguments(arg "${options}" "" "" ${ARGN})

    get_filename_component(base_dir_full_path "${base_dir}" ABSOLUTE)
    partition_table_get_partition_info(size "--partition-name ${partition}" "size")
    partition_table_get_partition_info(offset "--partition-name ${partition}" "offset")
    if(NOT size OR NOT offset)
        message(FATAL_ERROR "partition '${partition}' not found in partition table")
    endif()

    set(image_file "${CMAKE_BINARY_DIR}/${partition}.bin")

    add_custom_target(littlefs_${partition}_bin ALL
        COMMAND "${LITTLEFS_PY}" create "${base_dir_full_path}" "${image_file}"
                -v --fs-size=${size} --name-max=${CONFIG_LITTLEFS_OBJ_NAME_LEN}
                --block-size=4096
        COMMENT "Building littlefs image for '${partition}'"
        VERBATIM)

    set_property(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" APPEND PROPERTY
                 ADDITIONAL_CLEAN_FILES "${image_file}")

    idf_component_get_property(main_args esptool_py FLASH_ARGS)
    idf_component_get_property(sub_args esptool_py FLASH_SUB_ARGS)
    esptool_py_flash_target(${partition}-flash "${main_args}" "${sub_args}")
    esptool_py_flash_target_image(${partition}-flash "${partition}"
                                  "${offset}" "${image_file}")
    add_dependencies(${partition}-flash littlefs_${partition}_bin)

    if(arg_FLASH_IN_PROJECT)
        esptool_py_flash_target_image(flash "${partition}" "${offset}" "${image_file}")
        add_dependencies(flash littlefs_${partition}_bin)
    endif()
endfunction()

automatika_littlefs_image(assets "${WEB_DIR}" FLASH_IN_PROJECT)
