#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "core/state.h"

// Карта оборудования пользователя: какие датчики и исполнительные
// устройства реально установлены. Режимы обязаны проверять её, чтобы
// отсутствующая периферия не влияла на процесс.

typedef enum {
    HW_TEMP_CUBE = 0,
    HW_TEMP_COLUMN,
    HW_TEMP_JACKET,
    HW_TEMP_TANK,
    HW_TEMP_TSA,        // трубка связи с атмосферой (TSA)
    HW_TEMP_ROLE_COUNT
} hw_temp_role_t;

typedef enum {
    HW_DEV_PRESSURE = 0,
    HW_DEV_VACUUM,
    HW_DEV_LEVEL,
    HW_DEV_SCALE,
    HW_DEV_REFLUX,
    HW_DEV_COOLING_PUMP,
    HW_DEV_WATER_VALVE,
    HW_DEV_VACUUM_PUMP,
    HW_DEV_STIRRER,
    HW_DEV_DOSER,
    HW_DEV_VALVES,
    HW_DEV_TSA,          // TSA safety enabled (atmospheric tube temp monitor)
    HW_DEV_COUNT
} hw_device_t;

#define HW_TEMP_REDUNDANCY_MAX_DELTA_C  3.0f
#define HW_TEMP_REDUNDANCY_BIAS_ALPHA   0.05f

typedef struct {
    int8_t   temp[HW_TEMP_ROLE_COUNT];         // index 1-Wire or -1
    int8_t   temp_backup[HW_TEMP_ROLE_COUNT];  // backup 1-Wire index or -1
    uint32_t devices;                          // bitmask HW_DEV_*
} hw_profile_t;

typedef struct {
    bool ready;
    char reason[96];
} hw_mode_status_t;

void hw_profile_init(void);
const hw_profile_t *hw_profile_get(void);

esp_err_t hw_profile_set(const hw_profile_t *in);

bool hw_has_device(hw_device_t dev);
int  hw_temp_index(hw_temp_role_t role);
int  hw_temp_backup_index(hw_temp_role_t role);
bool hw_temp_available(hw_temp_role_t role);
bool hw_temp_is_redundant(hw_temp_role_t role);
float hw_temp_read(hw_temp_role_t role);              // NAN, если роль не подключена; с fallback+коррекцией
float hw_temp_read_fast(hw_temp_role_t role);         // быстрое чтение с fallback+коррекцией
float hw_temp_read_compensated(hw_temp_role_t role, float pressure_hpa); // с компенсацией давления
int   hw_temp_active_index(hw_temp_role_t role);
bool  hw_temp_rising(hw_temp_role_t role);
float hw_temp_slope(hw_temp_role_t role);
int64_t hw_temp_last_update_us(hw_temp_role_t role);

// Проверка минимального набора для режима. Чистая функция: count задаётся
// вызывающим, чтобы её можно было тестировать без железа.
void hw_profile_check(const hw_profile_t *p, uint8_t onewire_count,
                      process_type_t type, hw_mode_status_t *out);

// Проверка по текущей карте и реальному числу датчиков.
void hw_profile_check_mode(process_type_t type, hw_mode_status_t *out);

// JSON для /api/hardware.
void hw_profile_json(char *out, size_t cap);

const char *hw_device_name(hw_device_t dev);
const char *hw_device_key(hw_device_t dev);
