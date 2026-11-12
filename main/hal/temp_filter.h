#pragma once
#include <stdbool.h>
#include <stdint.h>

// Интеллектуальная обработка сигнала (термопары/датчика)
//
// Окно, среднее, линейный тренд и упреждающий флаг роста.
// Вся математика — float, без аллокаций.
//
// Смысл флагов:
//   rising  — текущий рост: тренд (в °C/мин) поднялся выше порога или
//             накопил предупреждение и не сброшен спадом на TREND_DROP;
//   slope   — линейный тренд в °C/мин по последним samples (по диапазону).
// Индекс s_idx — индекс источника (куб/колонна/рубашка).

#define TEMP_FILTER_MAX_WINDOW 10
#define TEMP_FILTER_MAX_CHANNELS 8

typedef struct {
    float   buffer[TEMP_FILTER_MAX_WINDOW];
    bool    ok[TEMP_FILTER_MAX_WINDOW];   // сэмпл валиден
    uint8_t index;
    uint8_t count;
    uint8_t window_size;
    uint8_t bad_count;

    float   trend_threshold;   // °C/мин — порог накопления тренда
    float   rise_delta;        // °C — минимальный рост для флага rising
    float   drop_delta;        // °C — спад, сбрасывающий флаг

    float   value;             // среднее
    float   slope;             // °C/мин
    bool    rising;

    float   trend_start;       // значение в момент начала роста
    bool    trend_active;
} TempFilter;

void  temp_filter_init(TempFilter *f, uint8_t window_size,
                       float trend_threshold, float rise_delta,
                       float drop_delta);

// Добавляет сэмпл. Значения <= -50 °C или > 200 °C считаются отказом
// датчика: не входят в среднее (после 3 подряд — сброс окна).
// Возвращает сглаженное значение; температура отказа — тоже результат.
float temp_filter_update(TempFilter *f, float new_sample);

// Упреждающий анализ: true — температура растёт быстрее порога.
bool  temp_filter_is_rising(TempFilter *f, float delta_threshold);

float temp_filter_value(const TempFilter *f);
float temp_filter_slope(const TempFilter *f);
bool  temp_filter_rising(const TempFilter *f);
int   temp_filter_valid_count(const TempFilter *f);
void  temp_filter_reset(TempFilter *f);

// ------------------------------------------------------------------
// Публичный API проекта: по одному фильтру на канал DS18B20.
// Индексы совпадают с hal_1wire_* (0 — куб, 1 — колонна/пар, 3 — рубашка).
// ------------------------------------------------------------------

void  temp_filter_module_init(uint8_t channels, uint8_t window);
// Кормит канал сырым значением (вызывается фоновой задачей 1-Wire).
float temp_filter_feed(int ch, float raw);
float temp_filter_read(int ch);          // сглаженное
bool  temp_filter_ch_rising(int ch);     // флаг роста
float temp_filter_ch_slope(int ch);      // °C/мин
bool  temp_filter_ch_valid(int ch);
