#include "ota_guard.h"

#include "ota_manager.h"
#include "secure_ota.h"

#include "control/ctrl_safety.h"
#include "core/estop.h"
#include "core/session_manager.h"
#include "core/state.h"

static const char *s_reason = "allowed";

esp_err_t ota_guard_check(void) {
    // Проверяем оба OTA-механизма: managed (ota_manager) и secure (secure_ota).
    if (ota_manager_update_running() || secure_ota_in_progress()) {
        s_reason = "update in progress";
        return ESP_ERR_INVALID_STATE;
    }
    if (state_get()->is_running || session_is_running()) {
        s_reason = "process is running";
        return ESP_ERR_INVALID_STATE;
    }
    if (!ctrl_safety_ok() || estop_is_pressed()) {
        s_reason = "safety/estop active";
        return ESP_ERR_INVALID_STATE;
    }
    s_reason = "allowed";
    return ESP_OK;
}

const char *ota_guard_reason(void) { return s_reason; }
