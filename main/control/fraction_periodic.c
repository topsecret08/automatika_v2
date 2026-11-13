#include "fraction_periodic.h"
#include "alcohol_balance.h"
#include "config.h"

#include "core/estop.h"
#include "core/events.h"
#include "core/state.h"
#include "core/watchdog.h"
#include "core/control_config.h"
#include "control/ctrl_dose.h"
#include "control/ctrl_safety.h"
#include "control/ctrl_valve_control.h"
#include "control/ctrl_heater.h"
#include "control/reflux.h"
#include "hal/baro.h"
#include "hal/flow_sensor.h"
#include "hal/hal_1wire.h"
#include "hal/hal_relay.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"

#include <math.h>
#include <string.h>

static const char *TAG = "FSEL";

#define FSEL_NVS_NAMESPACE "fsel"
#define FSEL_NVS_KEY       "cfg"
#define FSEL_CFG_MAGIC     0x4653454Cu   // "FSEL"
#define FSEL_CFG_VERSION   3

static fsel_config_t s_cfg;
static fsel_state_t  s_st;
static int64_t       s_last_decrement_us = 0;
static float         s_last_t_stage = 0.0f;
static bool          s_body_exhausted = false;
static portMUX_TYPE  s_mux = portMUX_INITIALIZER_UNLOCKED;

static int64_t now_us(void) { return esp_timer_get_time(); }

static void sync_output_device(const fsel_config_t *cfg);

static float clampf(float v, float lo, float hi) {
    if (!isfinite(v)) return lo;
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static bool config_valid(const fsel_config_t *cfg) {
    if (!cfg) return false;
    if (cfg->magic != FSEL_CFG_MAGIC || cfg->version != FSEL_CFG_VERSION) return false;
    if (cfg->out_device > FSEL_OUT_PUMP) return false;
    if (cfg->transition_mode > FSEL_TRANSITION_MANUAL) return false;
    if (cfg->select_mode > FSEL_SELECT_CONTINUOUS) return false;
    if (cfg->sensor > FSEL_SENSOR_COLUMN) return false;
    if (cfg->algo >= ALGO_COUNT) return false;
    if (cfg->trend_pause_c_per_min <= 0.0f ||
        cfg->trend_decrement_c_per_min < 0.0f) return false;
    if (cfg->body_min_open_time_sec <= 0.0f) return false;
    if (cfg->body_decrement_percent < 0.0f || cfg->body_decrement_percent > 100.0f)
        return false;
    if (cfg->heads_open_time_sec < 0.0f || cfg->body_open_time_sec < 0.0f ||
        cfg->tails_open_time_sec < 0.0f) return false;
    if (cfg->heads_close_time_sec < 0.0f || cfg->body_close_time_sec < 0.0f ||
        cfg->tails_close_time_sec < 0.0f) return false;
    if (cfg->select_mode == FSEL_SELECT_PERIODIC) {
        if (cfg->heads_open_time_sec + cfg->heads_close_time_sec <= 0.0f)
            return false;
        if (cfg->body_open_time_sec + cfg->body_close_time_sec <= 0.0f)
            return false;
        if (cfg->tails_open_time_sec + cfg->tails_close_time_sec <= 0.0f)
            return false;
    }
    return true;
}

static float stage_cfg_open(const fsel_config_t *cfg, fsel_stage_t stage) {
    switch (stage) {
    case FSEL_STAGE_HEADS: return cfg->heads_open_time_sec;
    case FSEL_STAGE_BODY:  return cfg->body_open_time_sec;
    case FSEL_STAGE_TAILS: return cfg->tails_open_time_sec;
    default:               return cfg->body_open_time_sec;
    }
}

static float stage_cfg_close(const fsel_config_t *cfg, fsel_stage_t stage) {
    switch (stage) {
    case FSEL_STAGE_HEADS: return cfg->heads_close_time_sec;
    case FSEL_STAGE_BODY:  return cfg->body_close_time_sec;
    case FSEL_STAGE_TAILS: return cfg->tails_close_time_sec;
    default:               return cfg->body_close_time_sec;
    }
}

static void stage_cfg_set(fsel_config_t *cfg, fsel_stage_t stage,
                          float open, float close) {
    switch (stage) {
    case FSEL_STAGE_HEADS:
        cfg->heads_open_time_sec = open;
        cfg->heads_close_time_sec = close;
        break;
    case FSEL_STAGE_TAILS:
        cfg->tails_open_time_sec = open;
        cfg->tails_close_time_sec = close;
        break;
    case FSEL_STAGE_BODY:
    default:
        cfg->body_open_time_sec = open;
        cfg->body_close_time_sec = close;
        break;
    }
}

void fsel_defaults(fsel_config_t *cfg) {
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));
    cfg->magic = FSEL_CFG_MAGIC;
    cfg->version = FSEL_CFG_VERSION;
    cfg->enabled = FSEL_DEFAULT_ENABLED != 0;
    cfg->out_device = FSEL_OUT_VALVE;
    cfg->transition_mode = FSEL_TRANSITION_AUTO;
    cfg->select_mode = FSEL_SELECT_PERIODIC;
    cfg->sensor = FSEL_SENSOR_STEAM;
    cfg->auto_mode_defaults = true;
    cfg->algo = ALGO_CLASSIC;
    cfg->trend_pause_c_per_min = TREND_PAUSE_C_PER_MIN;
    cfg->trend_decrement_c_per_min = TREND_DECREMENT_C_PER_MIN;
    cfg->trend_window_sec = TREND_WINDOW_SEC;

    cfg->heads_timer_sec = FSEL_HEADS_TIMER_SEC;
    cfg->heads_open_time_sec = FSEL_HEADS_OPEN_SEC;
    cfg->heads_close_time_sec = FSEL_HEADS_CLOSE_SEC;

    cfg->body_open_time_sec = FSEL_BODY_OPEN_SEC;
    cfg->body_close_time_sec = FSEL_BODY_CLOSE_SEC;
    cfg->body_hysteresis_c = FSEL_BODY_HYSTERESIS_C;
    cfg->body_decrement_percent = FSEL_BODY_DECREMENT_PERCENT;
    cfg->body_min_open_time_sec = FSEL_BODY_MIN_OPEN_SEC;
    cfg->body_temp_fix_delay_sec = FSEL_BODY_FIX_DELAY_SEC;

    cfg->tails_temperature_end = FSEL_TAILS_END_T;
    cfg->tails_open_time_sec = FSEL_TAILS_OPEN_SEC;
    cfg->tails_close_time_sec = FSEL_TAILS_CLOSE_SEC;
}

esp_err_t fsel_config_load(fsel_config_t *cfg) {
    if (!cfg) return ESP_ERR_INVALID_ARG;

    nvs_handle_t h;
    esp_err_t err = nvs_open(FSEL_NVS_NAMESPACE, NVS_READONLY, &h);
    if (err != ESP_OK) {
        fsel_defaults(cfg);
        return ESP_ERR_NOT_FOUND;
    }

    size_t len = sizeof(*cfg);
    err = nvs_get_blob(h, FSEL_NVS_KEY, cfg, &len);
    nvs_close(h);

    if (err != ESP_OK || len != sizeof(*cfg) || !config_valid(cfg)) {
        // Старая схема (v1) имела только period; сбрасываем её в дефолты,
        // чтобы не тащить несовместимые тайминги.
        fsel_defaults(cfg);
        return (err == ESP_OK) ? ESP_ERR_INVALID_STATE : err;
    }
    return ESP_OK;
}

esp_err_t fsel_config_save(const fsel_config_t *cfg) {
    if (!cfg || !config_valid(cfg)) return ESP_ERR_INVALID_ARG;

    nvs_handle_t h;
    esp_err_t err = nvs_open(FSEL_NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;

    err = nvs_set_blob(h, FSEL_NVS_KEY, cfg, sizeof(*cfg));
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

void fsel_init(void) {
    fsel_config_load(&s_cfg);
    memset(&s_st, 0, sizeof(s_st));
    s_st.stage = FSEL_STAGE_DONE;
    s_st.active = false;
    s_st.valve_open = false;
    if (s_cfg.enabled) sync_output_device(&s_cfg);
    ESP_LOGI(TAG, "init: enabled=%d sensor=%s defaults=%d", s_cfg.enabled,
             fsel_sensor_name(s_cfg.sensor), s_cfg.auto_mode_defaults);
}

static void apply_output(bool open, const fsel_config_t *cfg) {
    if (!cfg) return;

    // Safety/E-STOP всегда важнее FSEL.
    if (open && (estop_is_pressed() || !ctrl_safety_ok())) open = false;

    float rate = control_config_get_rate_ml_min();
    if (rate <= 0.0f) rate = 60.0f;

    // Фактическое устройство берём из глобальной конфигурации: FSEL лишь
    // синхронизирует её со своим выбором.
    if (control_config_get_output_device() == OUTPUT_DEVICE_VALVE) {
        valve_set_pct(VALVE_BODY, open ? 100.0f : 0.0f);
    } else {
        ctrl_dose_process(open ? rate : 0.0f, 0.1f, VALVE_BODY);
    }
}

static void sync_output_device(const fsel_config_t *cfg) {
    if (!cfg) return;
    output_device_t dev = (cfg->out_device == FSEL_OUT_PUMP)
                          ? OUTPUT_DEVICE_PUMP : OUTPUT_DEVICE_VALVE;
    if (control_config_get_output_device() != dev) {
        control_config_set_output_device(dev);
    }
}

static void runtime_times(float *open, float *close, float *period) {
    float o = s_st.open_time_sec;
    float c = s_st.close_time_sec;
    if (o < 0.0f) o = 0.0f;
    if (c < 0.0f) c = 0.0f;
    if (open) *open = o;
    if (close) *close = c;
    if (period) *period = o + c;
}

static void enter_body(const fsel_config_t *cfg, float t_stage) {
    s_st.stage = FSEL_STAGE_BODY;
    s_st.open_time_sec = cfg->body_open_time_sec;
    s_st.close_time_sec = cfg->body_close_time_sec;
    s_st.period_sec = s_st.open_time_sec + s_st.close_time_sec;
    s_st.stage_started_at_us = now_us();
    s_st.last_cycle_start_us = s_st.stage_started_at_us;
    s_st.valve_open = false;
    s_st.countdown_sec = 0.0f;
    s_st.t_fix = 0.0f;
    // Если T ещё неизвестна (ручной переход до первого замера), фиксируем
    // её на первом же валидном обновлении, а не 0 °C.
    s_st.t_fix_ready = (cfg->body_temp_fix_delay_sec == 0 && t_stage > 0.0f);
    if (s_st.t_fix_ready) s_st.t_fix = t_stage;
    s_body_exhausted = false;
    s_last_decrement_us = 0;
    apply_output(false, cfg);
    events_log(EVENT_STAGE_CHANGE, "Отбор: BODY (T_fix=%.2f)", s_st.t_fix);
}

static void enter_tails(const fsel_config_t *cfg) {
    s_st.stage = FSEL_STAGE_TAILS;
    s_st.open_time_sec = cfg->tails_open_time_sec;
    s_st.close_time_sec = cfg->tails_close_time_sec;
    s_st.period_sec = s_st.open_time_sec + s_st.close_time_sec;
    s_st.stage_started_at_us = now_us();
    s_st.last_cycle_start_us = s_st.stage_started_at_us;
    s_st.valve_open = false;
    s_st.countdown_sec = 0.0f;
    apply_output(false, cfg);
    events_log(EVENT_STAGE_CHANGE, "Отбор: TAILS");
}

static void enter_done(const fsel_config_t *cfg) {
    s_st.stage = FSEL_STAGE_DONE;
    s_st.valve_open = false;
    s_st.countdown_sec = 0.0f;
    apply_output(false, cfg);
    events_log(EVENT_STAGE_CHANGE, "Отбор: DONE");
}

void fsel_start(void) {
    taskENTER_CRITICAL(&s_mux);
    fsel_config_t cfg = s_cfg;
    taskEXIT_CRITICAL(&s_mux);

    if (!cfg.enabled) return;

    s_st.stage = FSEL_STAGE_HEADS;
    s_st.open_time_sec = cfg.heads_open_time_sec;
    s_st.close_time_sec = cfg.heads_close_time_sec;
    s_st.period_sec = s_st.open_time_sec + s_st.close_time_sec;
    s_st.stage_started_at_us = now_us();
    s_st.last_cycle_start_us = s_st.stage_started_at_us;
    s_st.valve_open = false;
    s_st.countdown_sec = 0.0f;
    s_st.t_fix = 0.0f;
    s_st.t_fix_ready = false;
    s_st.active = true;
    s_body_exhausted = false;
    s_last_decrement_us = 0;
    s_last_t_stage = 0.0f;
    apply_output(false, &cfg);
    events_log(EVENT_STAGE_CHANGE, "Отбор: HEADS");
}

void fsel_stop(void) {
    taskENTER_CRITICAL(&s_mux);
    fsel_config_t cfg = s_cfg;
    taskEXIT_CRITICAL(&s_mux);

    s_st.stage = FSEL_STAGE_DONE;
    s_st.active = false;
    s_st.valve_open = false;
    s_st.countdown_sec = 0.0f;
    apply_output(false, &cfg);
}

void fsel_update(float t_stage, float t_cube, float pressure_hpa) {
    (void)pressure_hpa;   // остаток спирта считает safety-задача (abv_update)

    taskENTER_CRITICAL(&s_mux);
    fsel_config_t cfg = s_cfg;
    if (!cfg.enabled || !s_st.active) {
        taskEXIT_CRITICAL(&s_mux);
        return;
    }
    taskEXIT_CRITICAL(&s_mux);

    float base_rate = control_config_get_rate_ml_min();
    if (base_rate <= 0.0f) base_rate = 60.0f;
    float open_s, close_s, period_s;
    runtime_times(&open_s, &close_s, &period_s);
    float duty = 1.0f;
    if (cfg.select_mode == FSEL_SELECT_PERIODIC && period_s > 0.001f) {
        duty = open_s / period_s;
        duty = clampf(duty, 0.0f, 1.0f);
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
        s_st.countdown_sec = 0.0f;
        return;
    }

    int64_t now = now_us();

    switch (s_st.stage) {
    case FSEL_STAGE_HEADS:
        if (cfg.transition_mode == FSEL_TRANSITION_AUTO &&
            cfg.heads_timer_sec == 0) {
            enter_body(&cfg, t_stage);
            return;
        }
        if (cfg.transition_mode == FSEL_TRANSITION_AUTO &&
            cfg.heads_timer_sec > 0 &&
            (now - s_st.stage_started_at_us) >= (int64_t)cfg.heads_timer_sec * 1000000LL) {
            enter_body(&cfg, t_stage);
            return;
        }
        break;

    case FSEL_STAGE_BODY:
        if (!s_st.t_fix_ready) {
            if ((now - s_st.stage_started_at_us) >=
                (int64_t)cfg.body_temp_fix_delay_sec * 1000000LL &&
                t_stage > 0.0f) {
                s_st.t_fix = t_stage;
                s_st.t_fix_ready = true;
            }
        }

        {
            int ch = (cfg.sensor == FSEL_SENSOR_CUBE) ? 0 : 1;
            float trend = hal_1wire_trend(ch);
            s_st.trend_c_per_min = trend;

            algo_config_t ac = {
                .algo = cfg.algo,
                .trend_pause_c_per_min = cfg.trend_pause_c_per_min,
                .trend_decrement_c_per_min = cfg.trend_decrement_c_per_min,
            };
            algo_result_t ar = algo_evaluate(&ac, trend, t_stage,
                                             s_st.t_fix,
                                             cfg.body_hysteresis_c);

            bool classic = (cfg.algo == ALGO_CLASSIC ||
                            cfg.algo == ALGO_ADAPTIVE);
            bool overshoot = classic && s_st.t_fix_ready &&
                             t_stage > (s_st.t_fix + cfg.body_hysteresis_c);

            if (ar == ALGO_RESULT_PAUSE || overshoot) {
                if ((overshoot || ar == ALGO_RESULT_DECREMENT) &&
                    (s_last_decrement_us == 0 ||
                     (now - s_last_decrement_us) >= 1000000LL)) {
                    s_last_decrement_us = now;
                    float pct = cfg.body_decrement_percent *
                                algo_decrement_multiplier(&ac, ar);
                    fsel_apply_decrement(pct);

                    if (s_st.open_time_sec < cfg.body_min_open_time_sec) {
                        if (cfg.transition_mode == FSEL_TRANSITION_AUTO) {
                            enter_tails(&cfg);
                        } else {
                            s_body_exhausted = true;
                            s_st.valve_open = false;
                            s_st.countdown_sec = 0.0f;
                            apply_output(false, &cfg);
                        }
                        return;
                    }
                }
                s_st.valve_open = false;
                s_st.countdown_sec = 0.0f;
                apply_output(false, &cfg);
                return;
            }

            if (ar == ALGO_RESULT_DECREMENT &&
                (s_last_decrement_us == 0 ||
                 (now - s_last_decrement_us) >= 1000000LL)) {
                s_last_decrement_us = now;
                fsel_apply_decrement(cfg.body_decrement_percent *
                                     algo_decrement_multiplier(&ac, ar));
                if (s_st.open_time_sec < cfg.body_min_open_time_sec) {
                    if (cfg.transition_mode == FSEL_TRANSITION_AUTO) {
                        enter_tails(&cfg);
                    } else {
                        s_body_exhausted = true;
                        s_st.valve_open = false;
                        s_st.countdown_sec = 0.0f;
                        apply_output(false, &cfg);
                    }
                    return;
                }
            }
        }
        break;

    case FSEL_STAGE_TAILS:
        if (t_cube >= cfg.tails_temperature_end) {
            enter_done(&cfg);
            return;
        }
        break;

    case FSEL_STAGE_DONE:
    default:
        if (s_st.valve_open) {
            s_st.valve_open = false;
            apply_output(false, &cfg);
        }
        s_st.countdown_sec = 0.0f;
        return;
    }

    // BODY в ручном режиме после исчерпания open_time не должен
    // автоматически открываться: ждём /api/selection/next.
    if (s_st.stage == FSEL_STAGE_BODY && s_body_exhausted) {
        if (s_st.valve_open) {
            s_st.valve_open = false;
            apply_output(false, &cfg);
        }
        s_st.countdown_sec = 0.0f;
        return;
    }

    bool should_open = false;

    if (cfg.select_mode == FSEL_SELECT_CONTINUOUS) {
        // Для клапана «непрерывный» = постоянно открыт; для насоса —
        // постоянная подача. ШИМ клапана не эмулируем: соленоид дребезжит.
        should_open = true;
        s_st.countdown_sec = 0.0f;
    } else {
        float period = s_st.period_sec;
        float open = s_st.open_time_sec;
        if (period <= 0.001f) period = 0.001f;
        if (open < 0.0f) open = 0.0f;
        if (open > period) open = period;

        float elapsed = (float)(now - s_st.last_cycle_start_us) / 1e6f;
        if (elapsed < 0.0f) elapsed = 0.0f;
        float phase = fmodf(elapsed, period);
        if (phase < 0.0f) phase = 0.0f;

        if (phase < open) {
            should_open = true;
            s_st.countdown_sec = open - phase;
        } else {
            should_open = false;
            s_st.countdown_sec = period - phase;
        }
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

void fsel_force_next_stage(void) {
    taskENTER_CRITICAL(&s_mux);
    fsel_config_t cfg = s_cfg;
    taskEXIT_CRITICAL(&s_mux);

    switch (s_st.stage) {
    case FSEL_STAGE_HEADS:
        enter_body(&cfg, s_last_t_stage);
        break;
    case FSEL_STAGE_BODY:
        enter_tails(&cfg);
        break;
    case FSEL_STAGE_TAILS:
        enter_done(&cfg);
        break;
    default:
        break;
    }
}

fsel_stage_t fsel_get_stage(void) { return s_st.stage; }
float fsel_get_open_time(void) { return s_st.open_time_sec; }
float fsel_get_close_time(void) { return s_st.close_time_sec; }
float fsel_get_countdown(void) { return s_st.countdown_sec; }
float fsel_get_t_fix(void) { return s_st.t_fix; }
float fsel_get_trend(void) { return s_st.trend_c_per_min; }
select_algo_t fsel_get_algo(void) { return s_cfg.algo; }

void fsel_apply_decrement(float percent) {
    if (!isfinite(percent) || percent <= 0.0f) return;
    if (percent > 100.0f) percent = 100.0f;

    s_st.open_time_sec *= (1.0f - percent / 100.0f);
    if (s_st.open_time_sec < 0.0f) s_st.open_time_sec = 0.0f;
    s_st.close_time_sec = s_st.period_sec - s_st.open_time_sec;
    if (s_st.close_time_sec < 0.0f) s_st.close_time_sec = 0.0f;
}

esp_err_t fsel_set_algo(select_algo_t algo) {
    if (algo >= ALGO_COUNT) return ESP_ERR_INVALID_ARG;

    fsel_config_t cfg;
    taskENTER_CRITICAL(&s_mux);
    s_cfg.algo = algo;
    s_cfg.auto_mode_defaults = false;
    cfg = s_cfg;
    taskEXIT_CRITICAL(&s_mux);
    fsel_config_save(&cfg);

    // Смена алгоритма на ходу не должна тащить старый декремент.
    if (s_st.active && s_st.stage == FSEL_STAGE_BODY) {
        s_st.open_time_sec = FSEL_BODY_OPEN_SEC;
        s_st.close_time_sec = FSEL_BODY_CLOSE_SEC;
        s_st.period_sec = s_st.open_time_sec + s_st.close_time_sec;
        s_st.last_cycle_start_us = now_us();
    }
    return ESP_OK;
}
float fsel_get_hysteresis(void) { return s_cfg.body_hysteresis_c; }
float fsel_get_decrement(void) { return s_cfg.body_decrement_percent; }
float fsel_get_min_open_time(void) { return s_cfg.body_min_open_time_sec; }
float fsel_get_heads_timer_sec(void) { return (float)s_cfg.heads_timer_sec; }
float fsel_get_tails_end_temp(void) { return s_cfg.tails_temperature_end; }
bool fsel_is_valve_open(void) { return s_st.valve_open; }
bool fsel_is_active(void) { return s_st.active; }

bool fsel_is_enabled(void) {
    taskENTER_CRITICAL(&s_mux);
    bool enabled = s_cfg.enabled;
    taskEXIT_CRITICAL(&s_mux);
    return enabled;
}

void fsel_get_state(fsel_state_t *out) {
    if (!out) return;
    taskENTER_CRITICAL(&s_mux);
    *out = s_st;
    taskEXIT_CRITICAL(&s_mux);
    out->ethanol_remaining_ml = abv_get_remaining_ml();
    out->ethanol_percent = abv_get_remaining_percent();
}

void fsel_get_config(fsel_config_t *out) {
    if (!out) return;
    taskENTER_CRITICAL(&s_mux);
    *out = s_cfg;
    taskEXIT_CRITICAL(&s_mux);
}

esp_err_t fsel_set_config(const fsel_config_t *cfg) {
    if (!cfg || !config_valid(cfg)) return ESP_ERR_INVALID_ARG;

    fsel_config_t copy = *cfg;
    copy.magic = FSEL_CFG_MAGIC;
    copy.version = FSEL_CFG_VERSION;

    select_algo_t old_algo;
    taskENTER_CRITICAL(&s_mux);
    old_algo = s_cfg.algo;
    s_cfg = copy;
    taskEXIT_CRITICAL(&s_mux);
    sync_output_device(&s_cfg);

    if (copy.algo != old_algo && s_st.active &&
        s_st.stage == FSEL_STAGE_BODY) {
        s_st.open_time_sec = FSEL_BODY_OPEN_SEC;
        s_st.close_time_sec = FSEL_BODY_CLOSE_SEC;
        s_st.period_sec = s_st.open_time_sec + s_st.close_time_sec;
        s_st.last_cycle_start_us = now_us();
    }
    return ESP_OK;
}

esp_err_t fsel_set_param(const char *name, float value) {
    if (!name || !isfinite(value)) return ESP_ERR_INVALID_ARG;

    taskENTER_CRITICAL(&s_mux);
    fsel_config_t cfg = s_cfg;
    taskEXIT_CRITICAL(&s_mux);

    fsel_stage_t stage = s_st.active ? s_st.stage : FSEL_STAGE_BODY;
    if (stage == FSEL_STAGE_DONE) stage = FSEL_STAGE_BODY;

    bool timing_changed = false;
    bool runtime_only = false;
    bool use_runtime = s_st.active && s_st.stage == stage;

    if (strcmp(name, "open_time_sec") == 0) {
        float open = clampf(value, 0.0f, 3600.0f);
        float period = use_runtime ? s_st.period_sec
                                   : (stage_cfg_open(&cfg, stage) +
                                      stage_cfg_close(&cfg, stage));
        if (period < open) period = open + 0.1f;
        float close = algo_close_time(period, open);
        stage_cfg_set(&cfg, stage, open, close);
        if (use_runtime) {
            s_st.open_time_sec = open;
            s_st.close_time_sec = close;
            s_st.period_sec = period;
        }
        timing_changed = true;
    } else if (strcmp(name, "close_time_sec") == 0) {
        float close = clampf(value, 0.0f, 3600.0f);
        float open = use_runtime ? s_st.open_time_sec
                                 : stage_cfg_open(&cfg, stage);
        stage_cfg_set(&cfg, stage, open, close);
        if (use_runtime) {
            s_st.open_time_sec = open;
            s_st.close_time_sec = close;
            s_st.period_sec = open + close;
        }
        timing_changed = true;
    } else if (strcmp(name, "period_sec") == 0) {
        float period = clampf(value, 0.1f, 7200.0f);
        float open = use_runtime ? s_st.open_time_sec
                                 : stage_cfg_open(&cfg, stage);
        if (open > period) open = period;
        float close = period - open;
        stage_cfg_set(&cfg, stage, open, close);
        if (use_runtime) {
            s_st.open_time_sec = open;
            s_st.close_time_sec = close;
            s_st.period_sec = period;
        }
        timing_changed = true;
    } else if (strcmp(name, "t_fix") == 0) {
        s_st.t_fix = clampf(value, 0.0f, 150.0f);
        s_st.t_fix_ready = true;
        runtime_only = true;
    } else if (strcmp(name, "body_hysteresis_c") == 0) {
        cfg.body_hysteresis_c = clampf(value, 0.0f, 20.0f);
    } else if (strcmp(name, "body_decrement_percent") == 0) {
        cfg.body_decrement_percent = clampf(value, 0.0f, 100.0f);
    } else if (strcmp(name, "body_min_open_time_sec") == 0) {
        cfg.body_min_open_time_sec = clampf(value, 0.01f, 60.0f);
    } else if (strcmp(name, "heads_timer_sec") == 0) {
        cfg.heads_timer_sec = (uint32_t)clampf(value, 0.0f, 86400.0f);
    } else if (strcmp(name, "tails_temperature_end") == 0) {
        cfg.tails_temperature_end = clampf(value, 0.0f, 150.0f);
    } else {
        return ESP_ERR_INVALID_ARG;
    }

    if (!runtime_only) cfg.auto_mode_defaults = false;

    taskENTER_CRITICAL(&s_mux);
    s_cfg = cfg;
    taskEXIT_CRITICAL(&s_mux);

    if (timing_changed) {
        s_st.last_cycle_start_us = now_us();
    }

    esp_err_t err = fsel_config_save(&cfg);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "param %s: NVS save failed (%s)", name,
                 esp_err_to_name(err));
    }
    return err;
}

void fsel_apply_mode_defaults(process_type_t type) {
    taskENTER_CRITICAL(&s_mux);
    fsel_config_t cfg = s_cfg;
    taskEXIT_CRITICAL(&s_mux);

    if (!cfg.enabled || !cfg.auto_mode_defaults) return;

    float open = 0.0f, close = 0.0f;
    fsel_select_mode_t select = FSEL_SELECT_CONTINUOUS;
    fsel_out_device_t device = FSEL_OUT_VALVE;

    switch (type) {
    case PROCESS_DISTILL:
        open = FSEL_MODE_DISTILL_OPEN_SEC;
        close = FSEL_MODE_DISTILL_CLOSE_SEC;
        select = FSEL_SELECT_PERIODIC;
        device = FSEL_OUT_PUMP;
        break;
    case PROCESS_RECTIFY:
        open = FSEL_MODE_RECTIFY_OPEN_SEC;
        close = FSEL_MODE_RECTIFY_CLOSE_SEC;
        select = FSEL_SELECT_PERIODIC;
        device = FSEL_OUT_VALVE;
        break;
    case PROCESS_BEER:
        open = 0.0f;
        close = 0.0f;
        select = FSEL_SELECT_CONTINUOUS;
        device = FSEL_OUT_PUMP;
        break;
    case PROCESS_IDLE:
        return;
    default:
        open = 0.0f;
        close = 0.0f;
        select = FSEL_SELECT_CONTINUOUS;
        device = FSEL_OUT_VALVE;
        break;
    }

    cfg.heads_open_time_sec = open;
    cfg.heads_close_time_sec = close;
    cfg.body_open_time_sec = open;
    cfg.body_close_time_sec = close;
    cfg.tails_open_time_sec = open;
    cfg.tails_close_time_sec = close;
    cfg.select_mode = select;
    cfg.out_device = device;

    fsel_config_t old;
    taskENTER_CRITICAL(&s_mux);
    old = s_cfg;
    s_cfg = cfg;
    taskEXIT_CRITICAL(&s_mux);
    sync_output_device(&cfg);

    if (memcmp(&old, &cfg, sizeof(cfg)) != 0) {
        fsel_config_save(&cfg);
        ESP_LOGI(TAG, "mode defaults: type=%d open=%.2f close=%.2f continuous=%d",
                 (int)type, open, close, select == FSEL_SELECT_CONTINUOUS);
    }
}

bool fsel_mode_tick(process_state_t *st, float power_heads,
                    float power_body, float power_tails) {
    if (!fsel_is_enabled()) return false;
    if (!st) return false;

    fsel_stage_t ss = fsel_get_stage();
    if (!fsel_is_active()) ss = FSEL_STAGE_HEADS;

    // Конденсатор должен работать на всех этапах: штатный PRECHECK
    // пропускается, поэтому включаем охлаждение здесь.
    cooling_valve_set(true);
    hal_relay_set(RELAY_COOLING_PUMP, true);

    if (!ctrl_safety_ok()) {
        ctrl_heater_force_off();
        if (fsel_is_active()) fsel_stop();
        st->stage = ST_ERROR;
        return true;
    }

    switch (ss) {
    case FSEL_STAGE_HEADS:
        ctrl_heater_set_percent(power_heads);
        reflux_enable(false);
        st->stage = ST_HEADS;
        break;
    case FSEL_STAGE_BODY:
        ctrl_heater_set_percent(power_body);
        reflux_enable(true);
        st->stage = ST_BODY;
        break;
    case FSEL_STAGE_TAILS:
        ctrl_heater_set_percent(power_tails);
        reflux_enable(true);
        st->stage = ST_TAILS;
        break;
    case FSEL_STAGE_DONE:
    default:
        ctrl_heater_force_off();
        ctrl_dose_stop();
        reflux_enable(false);
        st->stage = ST_DONE;
        st->is_running = false;
        break;
    }
    return true;
}

const char *fsel_stage_name(fsel_stage_t stage) {
    switch (stage) {
    case FSEL_STAGE_HEADS: return "heads";
    case FSEL_STAGE_BODY:  return "body";
    case FSEL_STAGE_TAILS: return "tails";
    case FSEL_STAGE_DONE:  return "done";
    default:               return "?";
    }
}

const char *fsel_sensor_name(fsel_sensor_t sensor) {
    switch (sensor) {
    case FSEL_SENSOR_CUBE:   return "cube";
    case FSEL_SENSOR_STEAM:  return "steam";
    case FSEL_SENSOR_COLUMN: return "column";
    default:                 return "?";
    }
}

void fsel_task(void *arg) {
    (void)arg;
    wd_register("fsel", 2000, false);

    while (1) {
        WD_FEED("fsel");

        bool enabled = fsel_is_enabled();
        bool running = state_get()->is_running;

        if (!enabled || !running) {
            if (fsel_is_active()) fsel_stop();
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        if (!fsel_is_active()) fsel_start();
        if (!fsel_is_active()) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        fsel_config_t cfg;
        fsel_get_config(&cfg);

        float pressure = baro_get_pressure_hpa();
        float cube = hal_1wire_read_compensated(0, pressure);
        float column = hal_1wire_read_compensated(1, pressure);
        float t_stage = cube;

        if (cfg.sensor == FSEL_SENSOR_STEAM) {
            // Компенсация давления уже выполнена конвейером.
            t_stage = column;
        } else if (cfg.sensor == FSEL_SENSOR_COLUMN) {
            t_stage = column;
        }

        fsel_update(t_stage, cube, pressure);

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
