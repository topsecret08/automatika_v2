#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"

/*
 * Минимальный набор телеметрии, который приходит по RS485 от контроллера.
 * В реальной прошивке заполняется из payload кадра TELEMETRY (0x81).
 */
typedef struct {
    float tc;        // текущая температура
    float ts;        // уставка
    float p;         // мощность, %
    float m;         // another measured channel
    float baro;      // барометр
    float flow;      // расход

    uint8_t sa;      // safety ok
    uint8_t es;      // E-STOP
    uint8_t lv;      // level
    uint8_t r;       // regulator state
    uint8_t stg;     // stage
    uint8_t run;
    uint8_t pause;

    uint32_t heads;
    uint32_t body;
    uint32_t tails;
    uint32_t elapsed;
    uint32_t db;

    const char *frac;
    bool link;
    uint32_t crc_err;
    const char *version;
} hmi_telemetry_t;

/** Создать главный экран гибридной раскладки и сделать его активным. */
lv_obj_t *ui_screen_home_create(void);

/** Обновить значения на экране из последнего кадра TELEMETRY. */
void ui_screen_home_update(const hmi_telemetry_t *t);
