#pragma once

#include "esp_err.h"

// Единая проверка перед любым запуском OTA.
// Возвращает ESP_OK, если обновление разрешено, иначе ESP_ERR_INVALID_STATE.
// Причина доступна через ota_guard_reason().
esp_err_t   ota_guard_check(void);
const char *ota_guard_reason(void);
