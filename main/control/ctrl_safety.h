#pragma once
#include <stdbool.h>
#include "esp_err.h"

void ctrl_safety_init(void);
void ctrl_safety_task(void *arg);
bool ctrl_safety_ok(void);
void ctrl_safety_trigger(const char *reason);
// ESP_OK — авария сброшена; ESP_ERR_INVALID_STATE — сброс запрещён
// (процесс/нагрев/уровень/E-STOP). Причина — ctrl_safety_reset_error().
esp_err_t ctrl_safety_reset(void);
const char *ctrl_safety_reset_error(void);

// E-STOP защёлкнут по железу (нажата кнопка) — нагрев и отбор запрещены.
bool ctrl_safety_estop_latched(void);
