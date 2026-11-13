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

// Чтение. Фоновая задача опрашивает шину ~1 раз в секунду;
// все функции ниже возвращают значение из кэша без блокировки.
float hal_1wire_read_raw(int idx);       // 0.0625°C шаг (блокирующее чтение)
float hal_1wire_read_filtered(int idx);  // фильтр TempFilter (окно FILTER_WINDOW)
float hal_1wire_read_fast(int idx);      // последнее сырое значение + калибровка
float hal_1wire_read(int idx);           // медиана + Калман + калибровка
float hal_1wire_read_compensated(int idx, float pressure_hpa);
float hal_1wire_trend(int idx);          // °C/мин по конвейеру
int   hal_1wire_read_all(float *out, int max);

// Упреждающий анализ тренда (temp_filter_is_rising по каналу).
bool  hal_1wire_rising(int idx);
float hal_1wire_slope(int idx);          // °C/мин, линейный тренд

// Калибровка
const ds18b20_calib_t* hal_1wire_get_calib(int idx);
void  hal_1wire_set_calib(int idx, float offset, float scale);
void  hal_1wire_save_calib(void);
void  hal_1wire_reset_calib(int idx);

// Диагностика
float hal_1wire_noise_rms(int idx);      // СКО шума, °C