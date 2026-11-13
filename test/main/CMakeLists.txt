# Компонент тестов: только production-исходники, не зависящие от железа,
# плюс stubs.c для пары внешних символов.
#
# Почему не REQUIRES main: продуктивный компонент `main` содержит app_main()
# и тянет весь проект (LVGL, SQLite, USB). В тестовом приложении app_main()
# должен быть свой, поэтому нужные .c подключаются напрямую.
set(PROD_DIR "${CMAKE_CURRENT_LIST_DIR}/../../main")

idf_component_register(
    SRCS
        "main.c"
        "stubs.c"
        "test_temp_filter.c"
        "test_level_logic.c"
        "test_event_log.c"
        "test_pump_doser.c"
        "test_sensor_pipeline.c"
        "test_selection_logic.c"
        "test_interface_conflicts.c"
        "test_pid_metrics.c"
        "test_verify_fuzz.c"
        "test_alcohol_balance.c"
        "${PROD_DIR}/hal/temp_filter.c"
        "${PROD_DIR}/hal/sensor_pipeline.c"
        "${PROD_DIR}/control/select_algorithm.c"
        "${PROD_DIR}/control/alcohol_balance.c"
        "${PROD_DIR}/verify/pid_metrics.c"
        "${PROD_DIR}/verify/verify_fuzz.c"
        "${PROD_DIR}/level/level_sensor.c"
        "${PROD_DIR}/core/event_log.c"
        "${PROD_DIR}/core/events.c"
        "${PROD_DIR}/control/pump_doser.c"
    INCLUDE_DIRS
        "."
        "${PROD_DIR}"
        "${PROD_DIR}/core"
        "${PROD_DIR}/hal"
        "${PROD_DIR}/level"
        "${PROD_DIR}/control"
        "${PROD_DIR}/verify"
    PRIV_REQUIRES
        unity
        driver
        esp_timer
        nvs_flash
        sdmmc
    WHOLE_ARCHIVE
)
