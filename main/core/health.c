#include "core/health.h"
#include "config.h"

#include "control/ctrl_dose.h"
#include "control/ctrl_safety.h"
#include "control/ctrl_scale.h"
#include "core/control_config.h"
#include "core/estop.h"
#include "core/events.h"
#include "core/flags.h"
#include "core/hw_profile.h"
#include "core/state.h"
#include "core/storage.h"
#include "core/watchdog.h"
#include "db/db_manager.h"
#include "hal/hal_1wire.h"
#include "hal/zmpt_rms.h"
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
    /* SD on controller is intentionally disabled (data stored on HMI TF card). */
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
    static char out[1280];

    process_state_t *st = state_get();
    wifi_ap_record_t ap;
    int rssi = 0;
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) rssi = ap.rssi;

    bool scale_ok = !hw_has_device(HW_DEV_SCALE) || scale_is_ready();
    bool zmpt_ok = !zmpt_rms_fault();
    bool pres_ok = !hw_has_device(HW_DEV_PRESSURE) || isfinite(st->pressure_bar);

    snprintf(out, sizeof(out),
        "{\"level\":\"%s\",\"reason\":\"%s\","
        "\"free_heap\":%u,\"min_free_heap\":%u,\"free_psram\":%u,"
        "\"uptime_s\":%lld,\"reset\":%d,"
        "\"wifi_rssi\":%d,\"sd\":%d,\"db\":%d,\"sensors\":%d,"
        "\"safety\":%d,\"estop\":%d,\"running\":%d,\"stage\":%d,"
        "\"version\":\"%s\",\"tasks\":%d,"
        "\"contours\":{\"hx711\":%d,\"zmpt\":%d,\"pressure\":%d,"
        "\"flood\":%d,\"eq_lock\":%d,\"eq_phase\":%u,\"vol_guard\":%d,"
        "\"vol_no_flow\":%d,\"vol_overflow\":%d,\"mpc\":%d,\"fusion\":%d,"
        "\"guard_reason\":\"%s\"}}",
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
        wd_count(),
        scale_ok ? 1 : 0, zmpt_ok ? 1 : 0, pres_ok ? 1 : 0,
        st->flood_active ? 1 : 0, st->eq_lock_active ? 1 : 0, (unsigned)st->eq_lock_phase,
        st->volume_guard_active ? 1 : 0, st->volume_no_flow ? 1 : 0, st->volume_overflow ? 1 : 0,
        st->predictive_heat_active ? 1 : 0,
        (st->fusion_cube_conf > 0.0f || st->fusion_column_conf > 0.0f) ? 1 : 0,
        st->actions_block_reason[0] ? st->actions_block_reason : "");

    return out;
}
