#include "screen_registry.h"
#include <string.h>

static const char *NAMES[SCR_COUNT] = {
    [SCR_NONE]          = "none",
    [SCR_MAIN_MENU]     = "main_menu",
    [SCR_PROCESS]       = "process",
    [SCR_GRAPHS]        = "graphs",
    [SCR_STATS]         = "stats",
    [SCR_LOGS]          = "logs",
    [SCR_SCHEME]        = "scheme",
    [SCR_FORECAST]      = "forecast",
    [SCR_HEATMAP]       = "heatmap",
    [SCR_MANUAL]        = "manual",
    [SCR_GABRIEL]       = "gabriel",
    [SCR_CALC_DILUTION] = "calc_dilution",
    [SCR_CALC_SUGAR]    = "calc_sugar",
    [SCR_CALC_GRAIN]    = "calc_grain",
    [SCR_CALC_YIELD]    = "calc_yield",
    [SCR_CALC_ABV]      = "calc_abv",
    [SCR_CALC_TEMP]     = "calc_temp",
    [SCR_CALC_PRESSURE] = "calc_pressure",
    [SCR_CALC_GABRIEL]  = "calc_gabriel",
    [SCR_SET_HW]        = "set_hw",
    [SCR_SET_KIT]       = "set_kit",
    [SCR_SET_FLAGS]     = "set_flags",
    [SCR_SET_CALIB]     = "set_calib",
    [SCR_SET_PID]       = "set_pid",
    [SCR_SET_MANUAL]    = "set_manual",
    [SCR_SET_GENERAL]   = "set_general",
    [SCR_SET_EXPORT]    = "set_export",
    [SCR_SET_RESTORE]   = "set_restore",
    [SCR_SET_RESET]     = "set_reset",
    [SCR_DIAG_OVER]     = "diag_over",
    [SCR_DIAG_I2C]      = "diag_i2c",
    [SCR_DIAG_DS]       = "diag_ds",
    [SCR_DIAG_TEST]     = "diag_test",
    [SCR_DIAG_MON]      = "diag_mon",
    [SCR_DIAG_EXPORT]   = "diag_export",
    [SCR_DIAG_PERF]     = "diag_perf",
    [SCR_SCALE_MAIN]    = "scale_main",
    [SCR_SCALE_CALIB]   = "scale_calib",
    [SCR_AL_SHOW]       = "al_show",
    [SCR_AL_STATS]      = "al_stats",
    [SCR_MODE_DISTILL]  = "mode_distill",
    [SCR_MODE_RECTIFY]  = "mode_rectify",
    [SCR_MODE_BEER]     = "mode_beer",
    [SCR_MODE_PVK]      = "mode_pvk",
    [SCR_MODE_NBK]      = "mode_nbk",
    [SCR_MODE_VACUUM]   = "mode_vacuum",
    [SCR_MODE_DOUBLE]   = "mode_double",
    [SCR_MODE_FERMENT]  = "mode_ferment",
    [SCR_LEARN_MODULE1] = "learn_m1",
    [SCR_LEARN_MODULE2] = "learn_m2",
    [SCR_LEARN_MODULE3] = "learn_m3",
    [SCR_LEARN_MODULE4] = "learn_m4",
    [SCR_LEARN_MODULE5] = "learn_m5",
    [SCR_LEARN_MODULE6] = "learn_m6",
    [SCR_LEARN_QUIZ]    = "learn_quiz",
    [SCR_RECIPES_LIST]  = "recipes_list",
    [SCR_RECIPE_DETAIL] = "recipe_detail",
    [SCR_ROLES_LOGIN]   = "roles_login",
    [SCR_ROLES_MATRIX]  = "roles_matrix",
    [SCR_HELP_LIST]     = "help_list",
    [SCR_HELP_ARTICLE]  = "help_article",
    [SCR_DEMO_START]    = "demo_start",
    [SCR_DEMO_STATS]    = "demo_stats",
};

const char* screen_name(screen_id_t id) {
    return (id < SCR_COUNT) ? NAMES[id] : "?";
}

screen_id_t screen_from_name(const char *name) {
    for (int i = 0; i < SCR_COUNT; i++)
        if (NAMES[i] && strcmp(NAMES[i], name) == 0) return (screen_id_t)i;
    return SCR_NONE;
}