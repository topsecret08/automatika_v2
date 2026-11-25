#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"

typedef struct {
    bool ready;
    const char *stage;
    float steam;       // ПАР, °C
    float steam_set;   // УСТ, °C
    float steam_fix;   // T ФИКС, °C
    float t_fix;       // порог фиксации
    float open_s;      // ВРЕМЯ ОТКРЫТ
    float close_s;     // ВРЕМЯ ЗАКРЫТ
    float remain_s;    // до закрытия
    bool valve_open;
    uint32_t alcohol_ml;
    uint8_t strength;
    float kub;         // КУБ для графика
    float par;         // ПАР для графика
} rect_state_t;

lv_obj_t *ui_screen_rectification_create(void);
void ui_screen_rectification_update(const rect_state_t *s);
