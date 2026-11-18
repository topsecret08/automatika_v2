#pragma once

#include <stdbool.h>
#include "esp_err.h"

typedef enum {
    SAFETY_CLASS_CRITICAL = 0,
    SAFETY_CLASS_QUALITY,
    SAFETY_CLASS_EXPERIMENTAL,
} safety_class_t;

// Единая точка применения выходов нагрева.
// CRITICAL применяется всегда (после проверки safety/E-STOP).
// QUALITY/EXPERIMENTAL — только после verify_mark_module_verified().
// EXPERIMENTAL дополнительно требует ненулевой shadow policy.
esp_err_t safety_gate_apply(const char *module, float value);
// Каноническая точка отправки команд управления. Сейчас алиас apply,
// сохраняется для нового кода и внешних интеграций.
esp_err_t safety_gate_submit(const char *module, float value);
esp_err_t safety_gate_apply_valve(const char *module, int idx, float value);
bool      safety_gate_can_apply(const char *module);
safety_class_t safety_gate_class(const char *module);
const char *safety_gate_class_name(safety_class_t cls);
int        safety_gate_module_count(void);
const char *safety_gate_module_name(int index);
