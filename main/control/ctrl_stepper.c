#include "ctrl_stepper.h"

#include "config.h"
#include "control/ctrl_safety.h"
#include "core/control_config.h"
#include "core/estop.h"
#include "core/events.h"
#include "core/watchdog.h"
#include "hal/hal_relay.h"
#include "hal/i2c_stepper.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "STEPPER";

#define STEPPER_TIMEOUT_MS      50
#define STEPPER_STOP_TIMEOUT_MS 20
#define STEPPER_RETRY_MS        5000

#if defined(CONFIG_I2C_STEPPER_ENABLE) && CONFIG_I2C_STEPPER_ENABLE
#define STEPPER_DEFAULT_ENABLED true
#else
#define STEPPER_DEFAULT_ENABLED false
#endif

static stepper_settings_t s_cfg;
static stepper_axis_state_t s_axis[STEPPER_AXIS_COUNT];
static SemaphoreHandle_t s_state_mtx;
static portMUX_TYPE s_seq_mux = portMUX_INITIALIZER_UNLOCKED;
static uint8_t s_seq;
static nvs_handle_t s_nvs;
static bool s_nvs_ok;
static bool s_stopped_for_safety;

static bool guard_ok(void) {
    return ctrl_safety_ok() && !estop_is_pressed();
}

static uint8_t next_seq(void) {
    taskENTER_CRITICAL(&s_seq_mux);
    uint8_t value = ++s_seq;
    taskEXIT_CRITICAL(&s_seq_mux);
    return value;
}

static void settings_defaults(void) {
    s_cfg.enabled = STEPPER_DEFAULT_ENABLED;
    s_cfg.address = STEPPER_I2C_ADDR;
    s_cfg.max_speed_sps = STEPPER_MAX_SPEED_SPS;
    s_cfg.accel_sps2 = STEPPER_ACCEL_SPS2;
    s_cfg.steps_per_ml = (float)control_config_get_steps_per_ml();
    s_cfg.stirrer_steps_per_rev = STEPPER_STIRRER_STEPS_PER_REV;
}

static void settings_load(void) {
    if (!s_nvs_ok) return;
    uint8_t u8;
    uint16_t u16;
    uint32_t u32;
    if (nvs_get_u8(s_nvs, "en", &u8) == ESP_OK) s_cfg.enabled = u8 != 0;
    if (nvs_get_u8(s_nvs, "addr", &u8) == ESP_OK) s_cfg.address = u8;
    if (nvs_get_u16(s_nvs, "max", &u16) == ESP_OK) s_cfg.max_speed_sps = u16;
    if (nvs_get_u16(s_nvs, "accel", &u16) == ESP_OK) s_cfg.accel_sps2 = u16;
    if (nvs_get_u32(s_nvs, "spm", &u32) == ESP_OK && u32 > 0)
        s_cfg.steps_per_ml = (float)u32;
    if (nvs_get_u16(s_nvs, "spr", &u16) == ESP_OK && u16 > 0)
        s_cfg.stirrer_steps_per_rev = u16;
}

static void settings_save(void) {
    if (!s_nvs_ok) return;
    nvs_set_u8(s_nvs, "en", s_cfg.enabled ? 1 : 0);
    nvs_set_u8(s_nvs, "addr", s_cfg.address);
    nvs_set_u16(s_nvs, "max", s_cfg.max_speed_sps);
    nvs_set_u16(s_nvs, "accel", s_cfg.accel_sps2);
    nvs_set_u32(s_nvs, "spm", (uint32_t)(s_cfg.steps_per_ml + 0.5f));
    nvs_set_u16(s_nvs, "spr", s_cfg.stirrer_steps_per_rev);
    nvs_commit(s_nvs);
}

static void axis_reset(void) {
    if (!s_state_mtx) return;
    xSemaphoreTake(s_state_mtx, portMAX_DELAY);
    memset(s_axis, 0, sizeof(s_axis));
    xSemaphoreGive(s_state_mtx);
}

static void axis_store(stepper_axis_t axis, const stepper_status_t *st) {
    if (!s_state_mtx || axis >= STEPPER_AXIS_COUNT) return;
    xSemaphoreTake(s_state_mtx, portMAX_DELAY);
    s_axis[axis].online = true;
    s_axis[axis].status = *st;
    xSemaphoreGive(s_state_mtx);
}

bool ctrl_stepper_get_axis(stepper_axis_t axis, stepper_axis_state_t *out) {
    if (!out || !s_state_mtx || axis >= STEPPER_AXIS_COUNT) return false;
    xSemaphoreTake(s_state_mtx, portMAX_DELAY);
    *out = s_axis[axis];
    xSemaphoreGive(s_state_mtx);
    return out->online;
}

static bool transact(stepper_request_t *req, stepper_status_t *st,
                     uint32_t timeout_ms) {
    req->seq = next_seq();
    esp_err_t err = i2c_stepper_transact(req, st, timeout_ms);
    if (err != ESP_OK) return false;
    axis_store((stepper_axis_t)req->axis, st);
    return true;
}

static bool status_read(stepper_axis_t axis, stepper_status_t *st,
                        uint32_t timeout_ms) {
    stepper_request_t req = { .cmd = STEPPER_CMD_STATUS,
                              .axis = (uint8_t)axis, .len = 0 };
    return transact(&req, st, timeout_ms);
}

bool ctrl_stepper_available(void) {
    return s_cfg.enabled && i2c_stepper_online();
}

const stepper_settings_t *ctrl_stepper_settings(void) { return &s_cfg; }

void ctrl_stepper_init(void) {
    settings_defaults();

    s_nvs_ok = (nvs_open("stepper", NVS_READWRITE, &s_nvs) == ESP_OK);
    if (s_nvs_ok) settings_load();

    if (!s_state_mtx) s_state_mtx = xSemaphoreCreateMutex();
    axis_reset();

    if (s_cfg.enabled) {
        esp_err_t err = i2c_stepper_init(s_cfg.address, STEPPER_TIMEOUT_MS);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "enabled but offline: %s", esp_err_to_name(err));
            events_log(EVENT_WARNING, "Stepper offline at 0x%02X",
                       s_cfg.address);
        }
    }

    ESP_LOGI(TAG, "enabled=%d addr=0x%02X max=%u sps=%.0f",
             s_cfg.enabled ? 1 : 0, s_cfg.address,
             (unsigned)s_cfg.max_speed_sps, (double)s_cfg.steps_per_ml);
}

esp_err_t ctrl_stepper_apply_settings(const stepper_settings_t *in) {
    if (!in) return ESP_ERR_INVALID_ARG;
    if (!isfinite(in->steps_per_ml) || in->steps_per_ml < 1.0f ||
        in->steps_per_ml > 100000.0f)
        return ESP_ERR_INVALID_ARG;
    if (in->address < 0x08 || in->address > 0x77)
        return ESP_ERR_INVALID_ARG;
    if (in->max_speed_sps < 1 || in->max_speed_sps > 20000)
        return ESP_ERR_INVALID_ARG;
    if (in->accel_sps2 > 50000)
        return ESP_ERR_INVALID_ARG;
    if (in->stirrer_steps_per_rev < 1)
        return ESP_ERR_INVALID_ARG;

    bool reinit = (in->enabled != s_cfg.enabled) ||
                  (in->address != s_cfg.address);

    if (reinit) ctrl_stepper_stop_all();
    s_cfg = *in;
    settings_save();

    if (reinit) {
        (void)i2c_stepper_deinit();
        axis_reset();
        if (s_cfg.enabled) {
            esp_err_t err = i2c_stepper_init(s_cfg.address,
                                             STEPPER_TIMEOUT_MS);
            if (err != ESP_OK) {
                events_log(EVENT_WARNING, "Stepper init failed: %s",
                           esp_err_to_name(err));
                return err;
            }
        }
    }
    return ESP_OK;
}

esp_err_t ctrl_stepper_dose_ml(float volume_ml, float rate_ml_min,
                               float *done_ml) {
    if (done_ml) *done_ml = 0.0f;
    if (!ctrl_stepper_available()) return ESP_ERR_NOT_SUPPORTED;
    if (!guard_ok()) return ESP_ERR_INVALID_STATE;

    uint32_t steps = stepper_ml_to_steps(volume_ml, s_cfg.steps_per_ml);
    if (steps == 0) return ESP_ERR_INVALID_ARG;

    stepper_status_t st;
    if (!status_read(STEPPER_AXIS_DOSER, &st, STEPPER_TIMEOUT_MS))
        return ESP_FAIL;

    int32_t start_pos = st.position;
    uint16_t speed = stepper_rate_to_sps(rate_ml_min, s_cfg.steps_per_ml,
                                         s_cfg.max_speed_sps);
    stepper_request_t req;
    stepper_build_move(&req, 0, STEPPER_AXIS_DOSER,
                       (int32_t)steps, speed, s_cfg.accel_sps2);
    if (!transact(&req, &st, STEPPER_TIMEOUT_MS)) return ESP_FAIL;

    int32_t target = start_pos + (int32_t)steps;
    uint32_t timeout = stepper_move_timeout_ms(steps, speed,
                                               s_cfg.accel_sps2, 2000);
    int64_t deadline = esp_timer_get_time() + (int64_t)timeout * 1000;

    int comm_errors = 0;
    while (esp_timer_get_time() < deadline) {
        if (!guard_ok()) {
            ctrl_stepper_stop_all();
            return ESP_ERR_INVALID_STATE;
        }
        WD_FEED("doser");
        vTaskDelay(pdMS_TO_TICKS(100));

        if (!status_read(STEPPER_AXIS_DOSER, &st, STEPPER_TIMEOUT_MS)) {
            if (++comm_errors >= 5) {
                ctrl_stepper_stop_all();
                return ESP_FAIL;
            }
            continue;
        }
        comm_errors = 0;

        if (st.fault != STEPPER_FAULT_NONE) {
            events_log(EVENT_WARNING, "Stepper doser fault %u", st.fault);
            ctrl_stepper_stop_all();
            return ESP_ERR_INVALID_STATE;
        }
        if (st.state == STEPPER_STATE_IDLE && st.position == target) {
            if (done_ml) *done_ml = volume_ml;
            return ESP_OK;
        }
    }

    ctrl_stepper_stop_all();
    if (done_ml && isfinite((float)(st.position - start_pos)))
        *done_ml = (float)(st.position - start_pos) / s_cfg.steps_per_ml;
    events_log(EVENT_WARNING, "Stepper dose timeout");
    return ESP_ERR_TIMEOUT;
}

static uint16_t rpm_to_sps(float rpm) {
    if (!isfinite(rpm) || rpm <= 0.0f) return 1;
    float sps = (rpm / 60.0f) * (float)s_cfg.stirrer_steps_per_rev;
    if (sps < 1.0f) sps = 1.0f;
    if (sps > (float)s_cfg.max_speed_sps) sps = (float)s_cfg.max_speed_sps;
    return (uint16_t)(sps + 0.5f);
}

esp_err_t ctrl_stepper_stirrer(bool on, bool reverse, float rpm) {
    if (!s_cfg.enabled) {
        // Явный откат: реле мешалки, если I2C-модуль не сконфигурирован.
        hal_relay_set(RELAY_STIRRER, on);
        return ESP_OK;
    }
    if (!guard_ok()) return ESP_ERR_INVALID_STATE;

    stepper_status_t st;
    stepper_request_t req = { .axis = STEPPER_AXIS_STIRRER };
    if (!on) {
        req.cmd = STEPPER_CMD_STOP;
        req.len = 0;
        return transact(&req, &st, STEPPER_TIMEOUT_MS) ? ESP_OK : ESP_FAIL;
    }

    uint16_t speed = rpm_to_sps(rpm);
    stepper_build_run(&req, 0, STEPPER_AXIS_STIRRER, reverse, speed);
    return transact(&req, &st, STEPPER_TIMEOUT_MS) ? ESP_OK : ESP_FAIL;
}

void ctrl_stepper_stop_all(void) {
    if (!i2c_stepper_online()) return;
    stepper_status_t st;
    for (uint8_t axis = 0; axis < STEPPER_AXIS_COUNT; axis++) {
        stepper_request_t req = { .cmd = STEPPER_CMD_STOP, .axis = axis,
                                  .len = 0 };
        (void)transact(&req, &st, STEPPER_STOP_TIMEOUT_MS);
    }
    s_stopped_for_safety = true;
}

esp_err_t ctrl_stepper_move_service(stepper_axis_t axis, int32_t steps,
                                    uint16_t speed_sps) {
    if (!ctrl_stepper_available()) return ESP_ERR_NOT_SUPPORTED;
    if (!guard_ok()) return ESP_ERR_INVALID_STATE;
    if (axis >= STEPPER_AXIS_COUNT || steps == 0) return ESP_ERR_INVALID_ARG;
    if (speed_sps == 0 || speed_sps > s_cfg.max_speed_sps)
        speed_sps = s_cfg.max_speed_sps;

    stepper_status_t st;
    stepper_request_t req;
    stepper_build_move(&req, 0, axis, steps, speed_sps, s_cfg.accel_sps2);
    return transact(&req, &st, STEPPER_TIMEOUT_MS) ? ESP_OK : ESP_FAIL;
}

esp_err_t ctrl_stepper_home(stepper_axis_t axis) {
    if (!ctrl_stepper_available()) return ESP_ERR_NOT_SUPPORTED;
    if (!guard_ok()) return ESP_ERR_INVALID_STATE;
    if (axis >= STEPPER_AXIS_COUNT) return ESP_ERR_INVALID_ARG;

    stepper_status_t st;
    stepper_request_t req = { .cmd = STEPPER_CMD_HOME,
                              .axis = (uint8_t)axis, .len = 2 };
    req.payload[0] = (uint8_t)(s_cfg.max_speed_sps & 0xFFu);
    req.payload[1] = (uint8_t)(s_cfg.max_speed_sps >> 8);
    return transact(&req, &st, STEPPER_TIMEOUT_MS) ? ESP_OK : ESP_FAIL;
}

void ctrl_stepper_task(void *arg) {
    (void)arg;
    wd_register("stepper", 5000, false);

    uint32_t retry_ms = 0;
    uint8_t last_fault[STEPPER_AXIS_COUNT] = {0};

    while (1) {
        WD_FEED("stepper");

        if (s_cfg.enabled && !i2c_stepper_online()) {
            if (retry_ms == 0) {
                if (i2c_stepper_init(s_cfg.address, STEPPER_TIMEOUT_MS) ==
                    ESP_OK) {
                    events_log(EVENT_INFO, "Stepper reconnected");
                } else {
                    retry_ms = STEPPER_RETRY_MS;
                }
            }
        }

        if (ctrl_stepper_available()) {
            if (!guard_ok()) {
                if (!s_stopped_for_safety) ctrl_stepper_stop_all();
            } else {
                s_stopped_for_safety = false;
            }
        } else {
            s_stopped_for_safety = false;
        }

        if (ctrl_stepper_available() && guard_ok()) {
            for (uint8_t axis = 0; axis < STEPPER_AXIS_COUNT; axis++) {
                stepper_status_t st;
                if (!status_read((stepper_axis_t)axis, &st,
                                 STEPPER_TIMEOUT_MS))
                    continue;
                if (st.fault != STEPPER_FAULT_NONE &&
                    st.fault != last_fault[axis]) {
                    events_log(EVENT_WARNING,
                               "Stepper axis %u fault %u", axis, st.fault);
                }
                last_fault[axis] = st.fault;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(STEPPER_POLL_MS));
        if (retry_ms > STEPPER_POLL_MS) retry_ms -= STEPPER_POLL_MS;
        else retry_ms = 0;
    }
}

void ctrl_stepper_status_json(char *out, size_t cap) {
    if (!out || cap == 0) return;

    stepper_axis_state_t axes[STEPPER_AXIS_COUNT];
    for (int i = 0; i < STEPPER_AXIS_COUNT; i++) {
        memset(&axes[i], 0, sizeof(axes[i]));
        (void)ctrl_stepper_get_axis((stepper_axis_t)i, &axes[i]);
    }

    snprintf(out, cap,
             "{\"enabled\":%d,\"online\":%d,\"address\":%u,"
             "\"max_speed_sps\":%u,\"accel_sps2\":%u,"
             "\"steps_per_ml\":%.1f,\"stirrer_steps_per_rev\":%u,"
             "\"axes\":["
             "{\"axis\":0,\"online\":%d,\"state\":%u,\"fault\":%u,"
             "\"position\":%ld,\"speed_sps\":%u,\"vin_mv\":%u,\"limit\":%u},"
             "{\"axis\":1,\"online\":%d,\"state\":%u,\"fault\":%u,"
             "\"position\":%ld,\"speed_sps\":%u,\"vin_mv\":%u,\"limit\":%u}"
             "]}",
             s_cfg.enabled ? 1 : 0, ctrl_stepper_available() ? 1 : 0,
             s_cfg.address, (unsigned)s_cfg.max_speed_sps,
             (unsigned)s_cfg.accel_sps2, (double)s_cfg.steps_per_ml,
             (unsigned)s_cfg.stirrer_steps_per_rev,
             axes[0].online ? 1 : 0, axes[0].status.state,
             axes[0].status.fault, (long)axes[0].status.position,
             axes[0].status.speed_sps, axes[0].status.vin_mv,
             axes[0].status.limit,
             axes[1].online ? 1 : 0, axes[1].status.state,
             axes[1].status.fault, (long)axes[1].status.position,
             axes[1].status.speed_sps, axes[1].status.vin_mv,
             axes[1].status.limit);
}
