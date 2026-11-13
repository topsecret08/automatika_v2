#pragma once

// Выбор алгоритма отбора.
//
// Идентификаторы нейтральные; в документации указана совместимость с
// классической схемой периодического отбора и тренд-анализом.
// Конкретные сторонние названия в коде не используются.

typedef enum {
    ALGO_CLASSIC = 0,   // таймер → T_fix → гистерезис/декремент
    ALGO_TREND,         // непрерывный тренд-анализ
    ALGO_ADAPTIVE,      // классика + тренд-предиктор
    ALGO_COUNT
} select_algo_t;

typedef enum {
    ALGO_RESULT_NORMAL = 0,
    ALGO_RESULT_PAUSE,
    ALGO_RESULT_DECREMENT,
} algo_result_t;

typedef struct {
    select_algo_t algo;
    float trend_pause_c_per_min;
    float trend_decrement_c_per_min;
} algo_config_t;

void algo_defaults(algo_config_t *cfg);
select_algo_t algo_parse(const char *name);
const char *algo_name(select_algo_t algo);

// trend — °C/мин по конвейеру датчика пара; t_stage — текущая T пара;
// t_fix — зафиксированная T тела; hysteresis — допуск.
algo_result_t algo_evaluate(const algo_config_t *cfg, float trend,
                            float t_stage, float t_fix, float hysteresis);

// Множитель декремента: тренд режет мягче, адаптивный — агрессивнее.
float algo_decrement_multiplier(const algo_config_t *cfg, algo_result_t result);

// Согласование периода и open_time: close = period - open.
float algo_close_time(float period, float open_time);
