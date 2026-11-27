#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"

/* Экран «Подготовка» */
typedef struct {
    const char *profile;
    float t1;
    float t2;
    uint8_t level;
    float flow;
    bool rinse_done;
    bool sealed;
    float preheat;
} prep_state_t;

lv_obj_t *ui_screen_preparation_create(void);
void ui_screen_preparation_update(const prep_state_t *s);

/* Экран «Диагностика» */
typedef struct {
    bool can_ok;
    uint32_t rx_err;
    uint32_t frames;
    uint16_t latency_ms;
    uint32_t heap_kb;
    bool valve_open;
    bool pump_on;
    uint8_t mixer;
    uint8_t heater;
} diag_state_t;

lv_obj_t *ui_screen_diagnostics_create(void);
void ui_screen_diagnostics_update(const diag_state_t *s);

/* Экран «Система» */
typedef struct {
    const char *version;
    const char *available;
    bool api_ok;
    uint16_t uptime_h;
    uint8_t cpu_temp;
    uint16_t psram_mb;
    uint8_t update_progress;
} sys_state_t;

lv_obj_t *ui_screen_system_create(void);
void ui_screen_system_update(const sys_state_t *s);
