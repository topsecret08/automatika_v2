idf_component_register(
    SRCS
        "main.c"
        "test_temp_filter.c"
        "test_level_logic.c"
        "test_event_log.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES unity main
)
