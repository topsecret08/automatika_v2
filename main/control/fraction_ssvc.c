#include "fraction_ssvc.h"
#include "config.h"

#include "core/estop.h"
#include "core/events.h"
#include "core/state.h"
#include "core/watchdog.h"
#include "core/control_config.h"
#include "control/ctrl_dose.h"
#include "control/ctrl_safety.h"
#include "control/ctrl_valve_control.h"
#include "hal/baro.h"
#include "hal/hal_1wire.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"

#include <math.h>
#include <string.h>

static const char *TAG = "SSVC";

#define SSVC_NVS_NAMESPACE "ssvc"
#define SSVC_NVS_KEY       "cfg"
#define SSVC_CFG_MAGIC     0x53535643u   // "SSVC"
#define SSVC_CFG_VERSION   1

static ssvc_config_t s_cfg;
static ssvc_state_t  s_st;
static int64_t       s_last_decrement_us = 0;
static float         s_last_t_stage = 0.0f;
static bool          s_body_exhausted = false;
static portMUX_TYPE  s_mux = portMUX_INITIALIZER_UNLOCKED;

static int64_t now_us(void) { return esp_timer_get_time(); }

static void sync_output_device(const ssvc_config_t *cfg);

static bool config_valid(const ssvc_config_t *cfg) {
    if (!cfg) return false;
    if (cfg->magic != SSVC_CFG_MAGIC || cfg->version != SSVC_CFG_VERSION) return false;
    if (cfg->out_device > SSVC_OUT_PUMP) return false;
    if (cfg->transition_mode > SSVC_TRANSITION_MANUAL) return false;
    if (cfg->select_mode > SSVC_SELECT_CONTINUOUS) return false;
    if (cfg->sensor > SSVC_SENSOR_COLUMN) return false;
    if (cfg->heads_period_sec <= 0.0f || cfg->body_period_sec <= 0.0f ||
        cfg->tails_period_sec <= 0.0f) return false;
    if (cfg->body_min_open_time_sec <= 0.0f) return false;
    if (cfg->body_decrement_percent < 0.0f || cfg->body_decrement_percent > 100.0f)
        return false;
    return true;
}

void ssvc_defaults(ssvc_config_t *cfg) {
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));
    cfg->magic = SSVC_CFG_MAGIC;
    cfg->version = SSVC_CFG_VERSION;
    cfg->enabled = SSVC_DEFAULT_ENABLED != 0;
    cfg->out_device = SSVC_OUT_VALVE;
    cfg->transition_mode = SSVC_TRANSITION_AUTO;
    cfg->select_mode = SSVC_SELECT_PERIODIC;
    cfg->sensor = SSVC_SENSOR_CUBE;

    cfg->heads_timer_sec = SSVC_HEADS_TIMER_SEC;
    cfg->heads_open_time_sec = SSVC_HEADS_OPEN_SEC;
    cfg->heads_period_sec = SSVC_HEADS_PERIOD_SEC;

    cfg->body_open_time_sec = SSVC_BODY_OPEN_SEC;
    cfg->body_period_sec = SSVC_BODY_PERIOD_SEC;
    cfg->body_hysteresis_c = SSVC_BODY_HYSTERESIS_C;
    cfg->body_decrement_percent = SSVC_BODY_DECREMENT_PERCENT;
    cfg->body_min_open_time_sec = SSVC_BODY_MIN_OPEN_SEC;
    cfg->body_temp_fix_delay_sec = SSVC_BODY_FIX_DELAY_SEC;

    cfg->tails_temperature_end = SSVC_TAILS_END_T;
    cfg->tails_open_time_sec = SSVC_TAILS_OPEN_SEC;
    cfg->tails_period_sec = SSVC_TAILS_PERIOD_SEC;
}

esp_err_t ssvc_config_load(ssvc_config_t *cfg) {
    if (!cfg) return ESP_ERR_INVALID_ARG;

    nvs_handle_t h;
    esp_err_t err = nvs_open(SSVC_NVS_NAMESPACE, NVS_READONLY, &h);
    if (err != ESP_OK) {
        ssvc_defaults(cfg);
        return ESP_ERR_NOT_FOUND;
    }

    size_t len = sizeof(*cfg);
    err = nvs_get_blob(h, SSVC_NVS_KEY, cfg, &len);
    nvs_close(h);

    if (err != ESP_OK || len != sizeof(*cfg) || !config_valid(cfg)) {
        ssvc_defaults(cfg);
        return (err == ESP_OK) ? ESP_ERR_INVALID_STATE : err;
    }
    return ESP_OK;
}

esp_err_t ssvc_config_save(const ssvc_config_t *cfg) {
    if (!cfg || !config_valid(cfg)) return ESP_ERR_INVALID_ARG;

    nvs_handle_t h;
    esp_err_t err = nvs_open(SSVC_NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;

    err = nvs_set_blob(h, SSVC_NVS_KEY, cfg, sizeof(*cfg));
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

void ssvc_init(void) {
    ssvc_config_load(&s_cfg);
    s_st.stage = SSVC_STAGE_DONE;
    s_st.active = false;
    s_st.valve_open = false;
    if (s_cfg.enabled) sync_output_device(&s_cfg);
    ESP_LOGI(TAG, "init: enabled=%d sensor=%s", s_cfg.enabled,
             ssvc_sensor_name(s_cfg.sensor));
}

static void apply_output(bool open, const ssvc_config_t *cfg) {
    if (!cfg) return;

    // Safety/E-STOP всегда важнее SSVC.
    if (open && (estop_is_pressed() || !ctrl_safety_ok())) open = false;

    float rate = control_config_get_rate_ml_min();
    if (rate <= 0.0f) rate = 60.0f;

    // Фактическое устройство берём из глобальной конфигурации: SSVC лишь
    // синхронизирует её со своим выбором. Иначе ctrl_dose_process может
    // управлять клапаном, когда выбран насос, и наоборот.
    if (control_config_get_output_device() == OUTPUT_DEVICE_VALVE) {
        // Клапан — позиция 0/100 %. Для периодического режима это
        // открыт/закрыт; для непрерывного — постоянно открыт.
        valve_set_pct(VALVE_BODY, open ? 100.0f : 0.0f);
    } else {
        // Насос: ctrl_dose_process сам решает, сколько шагов подать за dt.
        ctrl_dose_process(open ? rate : 0.0f, 0.1f, VALVE_BODY);
    }
}

static void sync_output_device(const ssvc_config_t *cfg) {
    if (!cfg) return;
    output_device_t dev = (cfg->out_device == SSVC_OUT_PUMP)
                          ? OUTPUT_DEVICE_PUMP : OUTPUT_DEVICE_VALVE;
    if (control_config_get_output_device() != dev) {
        control_config_set_output_device(dev);
    }
}

static void enter_body(const ssvc_config_t *cfg, float t_stage) {
    s_st.stage = SSVC_STAGE_BODY;
    s_st.open_time_sec = cfg->body_open_time_sec;
    s_st.period_sec = cfg->body_period_sec;
    s_st.stage_started_at_us = now_us();
    s_st.last_cycle_start_us = s_st.stage_started_at_us;
    s_st.valve_open = false;
    s_st.t_fix = 0.0f;
    // Если T ещё неизвестна (ручной переход до первого замера), фиксируем
    // её на первом же валидном обновлении, а не 0 °C.
    s_st.t_fix_ready = (cfg->body_temp_fix_delay_sec == 0 && t_stage > 0.0f);
    if (s_st.t_fix_ready) s_st.t_fix = t_stage;
    s_body_exhausted = false;
    s_last_decrement_us = 0;
    apply_output(false, cfg);
    events_log(EVENT_STAGE_CHANGE, "SSVC: BODY (T_fix=%.2f)", s_st.t_fix);
}

static void enter_tails(const ssvc_config_t *cfg) {
    s_st.stage = SSVC_STAGE_TAILS;
    s_st.open_time_sec = cfg->tails_open_time_sec;
    s_st.period_sec = cfg->tails_period_sec;
    s_st.stage_started_at_us = now_us();
    s_st.last_cycle_start_us = s_st.stage_started_at_us;
    s_st.valve_open = false;
    apply_output(false, cfg);
    events_log(EVENT_STAGE_CHANGE, "SSVC: TAILS");
}

static void enter_done(const ssvc_config_t *cfg) {
    s_st.stage = SSVC_STAGE_DONE;
    s_st.valve_open = false;
    apply_output(false, cfg);
    events_log(EVENT_STAGE_CHANGE, "SSVC: DONE");
}

void ssvc_start(void) {
    taskENTER_CRITICAL(&s_mux);
    ssvc_config_t cfg = s_cfg;
    taskEXIT_CRITICAL(&s_mux);

    if (!cfg.enabled) return;

    s_st.stage = SSVC_STAGE_HEADS;
    s_st.open_time_sec = cfg.heads_open_time_sec;
    s_st.period_sec = cfg.heads_period_sec;
    s_st.stage_started_at_us = now_us();
    s_st.last_cycle_start_us = s_st.stage_started_at_us;
    s_st.valve_open = false;
    s_st.t_fix = 0.0f;
    s_st.t_fix_ready = false;
    s_st.active = true;
    s_body_exhausted = false;
    s_last_decrement_us = 0;
    s_last_t_stage = 0.0f;
    apply_output(false, &cfg);
    events_log(EVENT_STAGE_CHANGE, "SSVC: HEADS");
}

void ssvc_stop(void) {
    taskENTER_CRITICAL(&s_mux);
    ssvc_config_t cfg = s_cfg;
    taskEXIT_CRITICAL(&s_mux);

    s_st.stage = SSVC_STAGE_DONE;
    s_st.active = false;
    s_st.valve_open = false;
    apply_output(false, &cfg);
}

void ssvc_update(float t_stage, float t_cube) {
    taskENTER_CRITICAL(&s_mux);
    ssvc_config_t cfg = s_cfg;
    if (!cfg.enabled || !s_st.active) {
        taskEXIT_CRITICAL(&s_mux);
        return;
    }
    taskEXIT_CRITICAL(&s_mux);

    float base_rate = control_config_get_rate_ml_min();
    if (base_rate <= 0.0f) base_rate = 60.0f;
    float duty = 1.0f;
    if (cfg.select_mode == SSVC_SELECT_PERIODIC && s_st.period_sec > 0.0f) {
        duty = s_st.open_time_sec / s_st.period_sec;
        if (duty < 0.0f) duty = 0.0f;
        if (duty > 1.0f) duty = 1.0f;
    }
    state_get()->body_rate_ml_min = base_rate * duty;

    if (!isfinite(t_stage)) t_stage = 0.0f;
    if (!isfinite(t_cube)) t_cube = 0.0f;
    s_last_t_stage = t_stage;

    // Авария/E-STOP: закрыть выход независимо от расписания.
    if (estop_is_pressed() || !ctrl_safety_ok()) {
        if (s_st.valve_open) {
            s_st.valve_open = false;
            apply_output(false, &cfg);
        }
        return;
    }

    int64_t now = now_us();

    switch (s_st.stage) {
    case SSVC_STAGE_HEADS:
        if (cfg.transition_mode == SSVC_TRANSITION_AUTO &&
            cfg.heads_timer_sec == 0) {
            enter_body(&cfg, t_stage);
            return;
        }
        if (cfg.transition_mode == SSVC_TRANSITION_AUTO &&
            cfg.heads_timer_sec > 0 &&
            (now - s_st.stage_started_at_us) >= (int64_t)cfg.heads_timer_sec * 1000000LL) {
            enter_body(&cfg, t_stage);
            return;
        }
        break;

    case SSVC_STAGE_BODY:
        if (!s_st.t_fix_ready) {
            if ((now - s_st.stage_started_at_us) >=
                (int64_t)cfg.body_temp_fix_delay_sec * 1000000LL &&
                t_stage > 0.0f) {
                s_st.t_fix = t_stage;
                s_st.t_fix_ready = true;
            }
        }

        if (s_st.t_fix_ready &&
            t_stage > (s_st.t_fix + cfg.body_hysteresis_c)) {
            // Гистерезис: закрыть и уменьшить open_time, но не чаще раза
            // в секунду — иначе один скачок температуры съест весь диапазон.
            if (s_last_decrement_us == 0 || (now - s_last_decrement_us) >= 1000000LL) {
                s_last_decrement_us = now;
                s_st.open_time_sec *= (1.0f - cfg.body_decrement_percent / 100.0f);
                if (s_st.open_time_sec < cfg.body_min_open_time_sec) {
                    if (cfg.transition_mode == SSVC_TRANSITION_AUTO) {
                        enter_tails(&cfg);
                    } else {
                        s_body_exhausted = true;
                        s_st.valve_open = false;
                        apply_output(false, &cfg);
                    }
                    return;
                }
            }
            s_st.valve_open = false;
            apply_output(false, &cfg);
            return;
        }
        break;

    case SSVC_STAGE_TAILS:
        if (t_cube >= cfg.tails_temperature_end) {
            enter_done(&cfg);
            return;
        }
        break;

    case SSVC_STAGE_DONE:
    default:
        if (s_st.valve_open) {
            s_st.valve_open = false;
            apply_output(false, &cfg);
        }
        return;
    }

    // BODY в ручном режиме после исчерпания open_time не должен
    // автоматически открываться: ждём /api/ssvc/next.
    if (s_st.stage == SSVC_STAGE_BODY && s_body_exhausted) {
        if (s_st.valve_open) {
            s_st.valve_open = false;
            apply_output(false, &cfg);
        }
        return;
    }

    bool should_open = false;

    if (cfg.select_mode == SSVC_SELECT_CONTINUOUS) {
        // Для клапана «непрерывный» = постоянно открыт; для насоса —
        // постоянная подача с rate_mlmin. ШИМ клапана не эмулируем:
        // механический соленоид от 50 % ШИМ дребезжит.
        should_open = true;
    } else {
        int64_t period_us = (int64_t)(s_st.period_sec * 1000000.0f);
        int64_t open_us = (int64_t)(s_st.open_time_sec * 1000000.0f);
        if (period_us <= 0) period_us = 1;
        if (open_us < 0) open_us = 0;
        if (open_us > period_us) open_us = period_us;
        int64_t elapsed = now - s_st.last_cycle_start_us;
        if (elapsed < 0) elapsed = 0;
        should_open = (elapsed % period_us) < open_us;
    }

    if (should_open != s_st.valve_open) {
        s_st.valve_open = should_open;
        apply_output(should_open, &cfg);
    } else if (should_open &&
               control_config_get_output_device() == OUTPUT_DEVICE_PUMP) {
        // Насос нужно кормить каждый такт, а не только на фронте.
        apply_output(true, &cfg);
    }
}

void ssvc_force_next_stage(void) {
    taskENTER_CRITICAL(&s_mux);
    ssvc_config_t cfg = s_cfg;
    taskEXIT_CRITICAL(&s_mux);

    switch (s_st.stage) {
    case SSVC_STAGE_HEADS:
        enter_body(&cfg, s_last_t_stage);
        break;
    case SSVC_STAGE_BODY:
        enter_tails(&cfg);
        break;
    case SSVC_STAGE_TAILS:
        enter_done(&cfg);
        break;
    default:
        break;
    }
}

ssvc_stage_t ssvc_get_stage(void) { return s_st.stage; }
float ssvc_get_open_time(void) { return s_st.open_time_sec; }
float ssvc_get_t_fix(void) { return s_st.t_fix; }
bool ssvc_is_valve_open(void) { return s_st.valve_open; }
bool ssvc_is_active(void) { return s_st.active; }

bool ssvc_is_enabled(void) {
    taskENTER_CRITICAL(&s_mux);
    bool enabled = s_cfg.enabled;
    taskEXIT_CRITICAL(&s_mux);
    return enabled;
}

void ssvc_get_state(ssvc_state_t *out) {
    if (!out) return;
    taskENTER_CRITICAL(&s_mux);
    *out = s_st;
    taskEXIT_CRITICAL(&s_mux);
}

void ssvc_get_config(ssvc_config_t *out) {
    if (!out) return;
    taskENTER_CRITICAL(&s_mux);
    *out = s_cfg;
    taskEXIT_CRITICAL(&s_mux);
}

esp_err_t ssvc_set_config(const ssvc_config_t *cfg) {
    if (!cfg || !config_valid(cfg)) return ESP_ERR_INVALID_ARG;

    ssvc_config_t copy = *cfg;
    copy.magic = SSVC_CFG_MAGIC;
    copy.version = SSVC_CFG_VERSION;

    taskENTER_CRITICAL(&s_mux);
    s_cfg = copy;
    taskEXIT_CRITICAL(&s_mux);
    sync_output_device(&s_cfg);
    return ESP_OK;
}

const char *ssvc_stage_name(ssvc_stage_t stage) {
    switch (stage) {
    case SSVC_STAGE_HEADS: return "heads";
    case SSVC_STAGE_BODY:  return "body";
    case SSVC_STAGE_TAILS: return "tails";
    case SSVC_STAGE_DONE:  return "done";
    default:               return "?";
    }
}

const char *ssvc_sensor_name(ssvc_sensor_t sensor) {
    switch (sensor) {
    case SSVC_SENSOR_CUBE:   return "cube";
    case SSVC_SENSOR_STEAM:  return "steam";
    case SSVC_SENSOR_COLUMN: return "column";
    default:                 return "?";
    }
}

void ssvc_task(void *arg) {
    (void)arg;
    wd_register("ssvc", 2000, false);

    while (1) {
        WD_FEED("ssvc");

        bool enabled = ssvc_is_enabled();
        bool running = state_get()->is_running;

        if (!enabled || !running) {
            if (ssvc_is_active()) ssvc_stop();
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        if (!ssvc_is_active()) ssvc_start();
        if (!ssvc_is_active()) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        ssvc_config_t cfg;
        ssvc_get_config(&cfg);

        float cube = hal_1wire_read_fast(0);
        float column = hal_1wire_read_fast(1);
        float t_stage = cube;

        if (cfg.sensor == SSVC_SENSOR_STEAM) {
            t_stage = baro_correct_boiling_point(column, baro_get_pressure_hpa());
        } else if (cfg.sensor == SSVC_SENSOR_COLUMN) {
            t_stage = column;
        }

        ssvc_update(t_stage, cube);

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
