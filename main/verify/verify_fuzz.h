#pragma once

#include "esp_err.h"

typedef struct {
    int tp, fp, fn;
    int injected;
} verify_fuzz_result_t;

// Детерминированный fuzzing детектора аномалий: нормальный ряд с шумом,
// затем выбросы, дрейф и пропуски. Возвращает TP/FP/FN.
esp_err_t verify_fuzz_anomaly(verify_fuzz_result_t *out);
