#pragma once
#include <stdbool.h>

typedef struct {
    bool autodetect_heads, predecrement, auto92, pid_autotune;
    bool fridge, water_steam, stirrer, stirrer_reverse;
    bool cool_wort, spill_sensor, wifi, mqtt, alice, telegram;
    bool ntp, web_api, sd_log, ota_sd, vapor_sensor, i2c_mon;
    bool auto_restore, sound, logs_screen, manual_screen;
} flags_t;

void     flags_init(void);
flags_t* flags_get(void);
void     flags_save(void);
bool     flags_get_by_key(const char *key);
bool     flags_key_exists(const char *key);
void     flags_set_by_key(const char *key, bool v);