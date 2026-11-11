#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    float offset;      // °C — прибавляется после фильтрации
    float scale;       // обычно 1.0
    bool  calibrated;
} ds18b20_calib_t;

// Инициализация шины + поиск датчиков
esp_err_t hal_1wire_init(void);
int       hal_1wire_count(void);
const char* hal_1wire_addr_str(int idx);
const uint8_t* hal_1wire_rom(int idx);

// Чтение
float hal_1wire_read_raw(int idx);       // 0.0625°C шаг
float hal_1wire_read_filtered(int idx);  // медиана+усреднение → ~0.015°C
float hal_1wire_read(int idx);           // + калибровка
int   hal_1wire_read_all(float *out, int max);

// Калибровка
const ds18b20_calib_t* hal_1wire_get_calib(int idx);
void  hal_1wire_set_calib(int idx, float offset, float scale);
void  hal_1wire_save_calib(void);
void  hal_1wire_reset_calib(int idx);

// Диагностика
float hal_1wire_noise_rms(int idx);      // СКО шума, °C