#include "flags.h"
#include "nvs.h"
#include <string.h>
#include <stddef.h>

static flags_t f;
static nvs_handle_t nvs;

static const struct { const char *k; size_t off; bool def; } MAP[] = {
    {"autodetect_heads", offsetof(flags_t, autodetect_heads), true},
    {"predecrement",     offsetof(flags_t, predecrement),     true},
    {"auto92",           offsetof(flags_t, auto92),           false},
    {"pid_autotune",     offsetof(flags_t, pid_autotune),     true},
    {"fridge",           offsetof(flags_t, fridge),           true},
    {"water_steam",      offsetof(flags_t, water_steam),      false},
    {"stirrer",          offsetof(flags_t, stirrer),          true},
    {"stirrer_reverse",  offsetof(flags_t, stirrer_reverse),  false},
    {"cool_wort",        offsetof(flags_t, cool_wort),        true},
    {"spill_sensor",     offsetof(flags_t, spill_sensor),     true},
    {"wifi",             offsetof(flags_t, wifi),             true},
    {"mqtt",             offsetof(flags_t, mqtt),             true},
    {"alice",            offsetof(flags_t, alice),            false},
    {"telegram",         offsetof(flags_t, telegram),         false},
    {"ntp",              offsetof(flags_t, ntp),              true},
    {"web_api",          offsetof(flags_t, web_api),          true},
    {"sd_log",           offsetof(flags_t, sd_log),           true},
    {"ota_sd",           offsetof(flags_t, ota_sd),           true},
    {"vapor_sensor",     offsetof(flags_t, vapor_sensor),     true},
    {"i2c_mon",          offsetof(flags_t, i2c_mon),          true},
    {"auto_restore",     offsetof(flags_t, auto_restore),     true},
    {"sound",            offsetof(flags_t, sound),            true},
    {"logs_screen",      offsetof(flags_t, logs_screen),      true},
    {"manual_screen",    offsetof(flags_t, manual_screen),    true},
};
#define MAP_N (sizeof(MAP)/sizeof(MAP[0]))

void flags_init(void) {
    nvs_open("flags", NVS_READWRITE, &nvs);
    for (size_t i = 0; i < MAP_N; i++) {
        uint8_t v = MAP[i].def;
        nvs_get_u8(nvs, MAP[i].k, &v);
        *(bool*)((char*)&f + MAP[i].off) = v;
    }
}

flags_t* flags_get(void) { return &f; }

void flags_save(void) {
    for (size_t i = 0; i < MAP_N; i++) {
        uint8_t v = *(bool*)((char*)&f + MAP[i].off);
        nvs_set_u8(nvs, MAP[i].k, v);
    }
    nvs_commit(nvs);
}

bool flags_get_by_key(const char *key) {
    for (size_t i = 0; i < MAP_N; i++)
        if (strcmp(MAP[i].k, key) == 0)
            return *(bool*)((char*)&f + MAP[i].off);
    return false;
}

void flags_set_by_key(const char *key, bool v) {
    for (size_t i = 0; i < MAP_N; i++)
        if (strcmp(MAP[i].k, key) == 0) {
            *(bool*)((char*)&f + MAP[i].off) = v;
            flags_save();
            return;
        }
}