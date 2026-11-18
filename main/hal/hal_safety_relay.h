#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

// Чистая проверка: NC-реле закрыто только при свежем heartbeat.
static inline bool safety_relay_should_close(int64_t now_us,
                                             int64_t last_hb_us,
                                             uint32_t timeout_ms,
                                             bool forced_open) {
    if (forced_open || timeout_ms == 0) return false;
    if (last_hb_us <= 0) return false;
    return (now_us - last_hb_us) < (int64_t)timeout_ms * 1000;
}

esp_err_t hal_safety_relay_init(int gpio_pin, uint32_t timeout_ms);
void      hal_safety_relay_heartbeat(void);
bool      hal_safety_relay_is_closed(void);
bool      hal_safety_relay_is_forced(void);
bool      hal_safety_relay_is_enabled(void);
// Снять защёлку force_open. Вызывается только из явного сброса аварии
// оператором; до первого heartbeat реле остаётся разомкнутым.
esp_err_t hal_safety_relay_rearm(void);
void      hal_safety_relay_force_open(void);
