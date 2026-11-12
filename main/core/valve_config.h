#pragma once
#include <stdbool.h>
#include <stdint.h>

// Конфигурация одного клапана (головы / тело / хвосты).
// Хранится в NVS, правится из меню и калибровки.

typedef enum {
    VALVE_NORMALLY_CLOSED = 0,   // НЗ: обесточен = закрыт
    VALVE_NORMALLY_OPEN   = 1,   // НО: обесточен = открыт
} valve_norm_t;

typedef enum {
    VALVE_MODE_SERVO = 0,        // сервопривод на PCA9685 (угол)
    VALVE_MODE_PULSE = 1,        // импульсный (соленоид)
} valve_mode_t;

#define VALVE_COUNT 3

typedef struct {
    char          name[32];
    valve_norm_t  norm;
    valve_mode_t  mode;
    bool          invert;
    float         dead_zone_pct;     // мёртвая зона, %
    float         angle_min;         // минимальный угол сервы, °
    float         angle_max;         // максимальный угол сервы, °
    uint32_t      pulse_period_ms;   // период импульса, мс
    float         ml_per_hour;       // откалиброванный расход, мл/ч
    bool          calibrated;
    uint32_t      last_calib_ms;
} valve_config_t;

void           valve_config_init(void);
valve_config_t* valve_config_get(int idx);
void           valve_config_save(void);
void           valve_config_reset(int idx);
