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
        "${PROD_DIR}/hal/temp_filter.c"
        "${PROD_DIR}/hal/sensor_pipeline.c"
        "${PROD_DIR}/control/select_algorithm.c"
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
    PRIV_REQUIRES
        unity
        driver
        esp_timer
        sdmmc
    WHOLE_ARCHIVE
)
