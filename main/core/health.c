#include "core/health.h"
#include "config.h"

#include "control/ctrl_dose.h"
#include "control/ctrl_safety.h"
#include "core/control_config.h"
#include "core/estop.h"
#include "core/events.h"
#include "core/flags.h"
#include "core/state.h"
#include "core/storage.h"
#include "core/watchdog.h"
#include "db/db_manager.h"
#include "hal/hal_1wire.h"
#include "script/lua_engine.h"

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static char s_last_reason[96] = "";

void health_init(void) { health_json(); }

bool health_check_healthy(void) {
    /* Критерии «здорового» образа (для подтверждения OTA):
     *   - конфигурация в NVS доступна;
     *   - watchdog работает и нет «зависаний»critical-задач;
     *   - авария/аварийная кнопка не активны;
     *   - внутренняя RAM не на грани;
     *   - датчики: либо есть, либо их отсутствие не блокирует работу. */

    if (!control_config_ready()) {
        snprintf(s_last_reason, sizeof(s_last_reason), "config NVS not ready");
        return false;
    }
    if (!ctrl_safety_ok()) {
        snprintf(s_last_reason, sizeof(s_last_reason), "safety fault");
        return false;
    }
    if (estop_is_pressed()) {
        snprintf(s_last_reason, sizeof(s_last_reason), "estop active");
        return false;
    }

    uint32_t free_int = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    if (free_int < HEALTH_MIN_FREE_INTERNAL) {
        snprintf(s_last_reason, sizeof(s_last_reason), "low internal RAM: %u",
                 (unsigned)free_int);
        return false;
    }

    /* Любая задача, «зависавшая» дольше своего таймаута — признак
     * нестабильного образа (чаще всего — новая прошивка после OTA). */
    if (wd_count() > 0) {
        uint32_t worst = 0;
        for (int i = 0; i < wd_count(); i++) {
            wd_task_t *t = wd_get(i);
            if (!t || !t->enabled) continue;
            if (t->max_stale_ms > t->timeout_ms && t->max_stale_ms > worst)
                worst = t->max_stale_ms;
        }
        if (worst > 0) {
            snprintf(s_last_reason, sizeof(s_last_reason),
                     "task stale %ums", (unsigned)worst);
            return false;
        }
    }

    s_last_reason[0] = '\0';
    return true;
}

health_level_t health_level(void) {
    if (!health_check_healthy()) return HEALTH_FAIL;
    if (!storage_sd_ready()) return HEALTH_DEGRADED;
    if (hal_1wire_count() == 0) return HEALTH_DEGRADED;
    return HEALTH_OK;
}

const char *health_level_name(void) {
    switch (health_level()) {
    case HEALTH_OK:       return "ok";
    case HEALTH_DEGRADED: return "degraded";
    default:              return "fail";
    }
}

const char *health_last_reason(void) { return s_last_reason; }

const char *health_json(void) {
    static char out[768];

    process_state_t *st = state_get();
    wifi_ap_record_t ap;
    int rssi = 0;
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) rssi = ap.rssi;

    snprintf(out, sizeof(out),
        "{\"level\":\"%s\",\"reason\":\"%s\","
        "\"free_heap\":%u,\"min_free_heap\":%u,\"free_psram\":%u,"
        "\"uptime_s\":%lld,\"reset\":%d,"
        "\"wifi_rssi\":%d,\"sd\":%d,\"db\":%d,\"sensors\":%d,"
        "\"safety\":%d,\"estop\":%d,\"running\":%d,\"stage\":%d,"
        "\"version\":\"%s\",\"tasks\":%d}",
        health_level_name(), s_last_reason,
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
        (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
        (long long)(esp_timer_get_time() / 1000000),
        (int)esp_reset_reason(),
        rssi,
        storage_sd_ready() ? 1 : 0,
        db_is_ready() ? 1 : 0,
        hal_1wire_count(),
        ctrl_safety_ok() ? 1 : 0,
        estop_is_pressed() ? 1 : 0,
        st->is_running ? 1 : 0,
        (int)st->stage,
        FW_VERSION,
        wd_count());

    return out;
}
