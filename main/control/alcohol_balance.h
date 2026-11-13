#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

// Оценка остатка этанола в кубе по температуре кипения и абсолютному
// давлению.
//
// Модель: Антуан (T в K, P в бар) + коэффициенты активности Ван-Лаара +
// закон Рауля. По составу жидкости считается давление насыщения смеси;
// если оно близко к измеренному P_абс, куб кипит и пар уносит этанол.
// Количество пара берётся из текущей скорости отбора, поэтому оценка
// приблизительная, но реагирует на режим и давление, а не на «магические»
// проценты в минуту.
//
// Модуль не управляет железом. Вызывать abv_update() периодически (в
// прошивке — из safety-задачи каждые 500 мс).

typedef struct {
    float volume_start_ml;      // начальный объём заливки, мл
    float abv_start_percent;    // начальная объёмная крепость, %
    float ethanol_density;      // г/мл, 0.789
} alcohol_init_t;

#define ABV_DEFAULT_VOLUME_ML   20000.0f
#define ABV_DEFAULT_PERCENT     40.0f
#define ABV_ETHANOL_DENSITY     0.789f

void  abv_defaults(alcohol_init_t *init);

// Инициализировать/сбросить баланс. Повторный вызов начинает расчёт заново.
void  abv_init(const alcohol_init_t *init);
// Сброс к сохранённым начальным параметрам (новый процесс).
void  abv_reset(void);
bool  abv_ready(void);

// Обновить по T куба (°C) и P_абс (hPa). Возвращает остаток этанола, мл.
float abv_update(float t_cube_c, float pressure_hpa);

float abv_get_remaining_ml(void);
float abv_get_remaining_percent(void);
float abv_get_ethanol_g(void);
float abv_get_current_abv_percent(void);
float abv_get_start_volume_ml(void);
float abv_get_start_abv_percent(void);
float abv_get_boiling_point_c(void);

// Парциальное давление насыщенного пара этанола, Па (T в °C).
float abv_ethanol_vapor_pressure(float t_c);
// Парциальное давление насыщенного пара воды, Па.
float abv_water_vapor_pressure(float t_c);

esp_err_t abv_config_load(alcohol_init_t *out);
esp_err_t abv_config_save(const alcohol_init_t *init);
