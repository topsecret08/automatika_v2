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
        "core/storage.c" "core/watchdog.c" "core/valve_config.c"
        "core/menu_engine.c" "core/menu_tree.c"
        "core/menu_callbacks.c" "core/menu_validator.c"
        "core/help_db.c" "core/learning.c" "core/recipes_db.c"
        "core/sd_content.c"
        "net/wifi_prov.c" "net/time_sync.c" "net/mdns_service.c"
        "hal/hal_i2c.c" "hal/hal_1wire.c" "hal/temp_filter.c" "hal/hal_adc.c" "hal/hal_dac.c"
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
        "control/fraction_logic.c" "control/reflux.c" "control/pid_autotune.c"
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
        "ui/ui.c" "ui/ui_theme.c" "ui/ui_builder.c"
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
        "web/mqtt_client.c" "web/telegram_bot.c"
        "web/telegram_notify.c" "web/ws_telemetry.c"
        "web/samovar.c" "web/samovar_blynk.c" "web/samovar_mqtt.c"
        "web/ws_handler.c" "web/http_handlers.c"
        "ota/fota.c" "ota/ota_manager.c" "ota/ota_sd.c" "ota/ota_url.c"
        "ota/ota_qr.c" "ota/ota_verify.c" "ota/secure_ota.c"
        "patches/critical_fixes.c"
        "third_party/qrcodegen.c"
    INCLUDE_DIRS "." "third_party"
    # Тестовый компонент main_test (Unity) живёт в test/ и включается
    # отдельным проектом test_app (см. test/README.md).
    EMBED_FILES
        "web/www/index.html" "web/www/app.js" "web/www/style.css"
        "web/www/charts.js" "web/www/api.js" "web/www/ws.js"
        "web/www/recipes.js" "web/www/settings.js" "web/www/manifest.json"
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
