#include "web_api.h"
#include "api_auth.h"
#include "actuator_test.h"
#include "rate_limit.h"
#include "ws_telemetry.h"

#include "core/config_backup.h"
#include "core/control_config.h"
#include "core/estop.h"
#include "core/event_log.h"
#include "core/health.h"
#include "core/i18n.h"
#include "core/nvs_migration.h"
#include "core/profiles.h"
#include "core/roles.h"
#include "core/process_control.h"
#include "core/menu_engine.h"
#include "core/recipes_db.h"
#include "db/db_manager.h"
#include "ai/anomaly.h"
#include "ai/action_predictor.h"
#include "ai/cache_manager.h"
#include "ai/page_predictor.h"
#include "ai/tinyml_predict.h"
#include "control/ctrl_autolearn.h"
#include "hal/hal_usb.h"
#include "control/ctrl_sensor_calib.h"
#include "control/valve_calibration.h"
#include "core/valve_config.h"
#include "core/session_recovery.h"
#include "core/trust_dashboard.h"
#include "core/session_manager.h"
#include "core/state.h"
#include "core/events.h"
#include "core/flags.h"
#include "core/loop_verify.h"
#include "core/hw_profile.h"
#include "core/watchdog.h"
#include "hal/hal_safety_relay.h"
#include "esp_timer.h"
#include "hal/hal_1wire.h"
#include "hal/baro.h"
#include "hal/flow_sensor.h"
#include "hal/pressure_sensor.h"
#include "modes/mode_base.h"
#include "modes/beer_presets.h"
#include "control/ctrl_heater.h"
#include "control/ctrl_safety.h"
#include "control/ctrl_dose.h"
#include "control/pump_doser.h"
#include "control/ctrl_pid.h"
#include "control/fraction_logic.h"
#include "control/fraction_periodic.h"
#include "control/alcohol_balance.h"
#include "control/param_registry.h"
#include "control/regulator_driver.h"
#include "control/safety_gate.h"
#include "control/ctrl_stepper.h"
#include "ota/ota_guard.h"
#include "control/adaptive_pid.h"
#include "verify/verify_config.h"
#include "verify/verify_db.h"
#include "verify/shadow.h"
#include "verify/verify_fuzz.h"
#include "control/pid_autotune.h"
#include "control/reflux.h"
#include "level/level_sensor.h"
#include "control/ctrl_scale.h"
#include "core/hw_profile.h"
#include "ota/ota_manager.h"
#include "ota/secure_ota.h"
#include "script/lua_engine.h"
#include "web/mqtt_app.h"
#include "web/telegram_bot.h"
#include "web/telegram_notify.h"
#include "web/brewfather.h"
#include "web/calendar.h"
#include "web/remote.h"
#include "web/tg_backup.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_log.h"
#include "cJSON.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

// Читалка тела запроса доступна остальным модулям (ws_telemetry/actuator).
int  web_api_recv_body(httpd_req_t *r, char *buf, size_t max);

static const char *TAG = "WEBAPI";

// ============================================================
// helpers
// ============================================================
//
// Все POST-обработчики обязаны начинать с api_check_auth(req).
// Единственное документированное исключение — webhook Алисы и
// Telegram (они работают по протоколу сервиса, см. web_alice.c и
// api_check_telegram_secret()), для них защита — секрет/строгий парсинг.

static int recv_body(httpd_req_t *r, char *buf, size_t max) {
    if (max == 0) return -1;
    int total = 0;

    if (r->content_len > 0) {
        if ((size_t)r->content_len >= max) return -1;
        while (total < (int)r->content_len) {
            int n = httpd_req_recv(r, buf + total, r->content_len - total);
            if (n <= 0) {
                if (n == HTTPD_SOCK_ERR_TIMEOUT) continue;
                return -1;
            }
            total += n;
        }
    }
    buf[total] = '\0';
    return total;
}

static esp_err_t send_json(httpd_req_t *r, const char *json) {
    httpd_resp_set_type(r, "application/json");
    return httpd_resp_sendstr(r, json);
}

static esp_err_t send_err(httpd_req_t *r, const char *status,
                          const char *json) {
    httpd_resp_set_status(r, status);
    return send_json(r, json);
}

static bool json_bool(const cJSON *v) {
    if (!v) return false;
    if (cJSON_IsBool(v)) return cJSON_IsTrue(v);
    if (cJSON_IsNumber(v)) return v->valuedouble != 0;
    if (cJSON_IsString(v)) {
        const char *s = v->valuestring;
        return strcmp(s, "1") == 0 || strcmp(s, "true") == 0 ||
               strcmp(s, "on") == 0 || strcmp(s, "yes") == 0;
    }
    return false;
}

static const char* json_string(const cJSON *root, const char *key) {
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(root, key);
    return cJSON_IsString(v) ? v->valuestring : NULL;
}

// ============================================================
// state / control
// ============================================================
static esp_err_t h_state(httpd_req_t *r) {
    if (api_check_read_auth(r) != ESP_OK) return api_reject_unauthorized(r);
    process_state_t snap;
    state_snapshot(&snap);
    process_state_t *s = &snap;

    float flow = hw_has_device(HW_DEV_FLOW) ? flow_get_lpm() : -1.0f;
    float level = (hw_has_device(HW_DEV_LEVEL) && level_is_available())
                      ? level_get_percent() : -1.0f;
    float scale_ml = (hw_has_device(HW_DEV_SCALE) && scale_is_ready())
                         ? scale_get_total_ml() : -1.0f;
    float jacket = hw_temp_available(HW_TEMP_JACKET)
                       ? hw_temp_read(HW_TEMP_JACKET) : -1.0f;
    float tank = hw_temp_available(HW_TEMP_TANK)
                     ? hw_temp_read(HW_TEMP_TANK) : -1.0f;
    if (!isfinite(flow)) flow = -1.0f;
    if (!isfinite(level)) level = -1.0f;
    if (!isfinite(scale_ml)) scale_ml = -1.0f;
    if (!isfinite(jacket)) jacket = -1.0f;
    if (!isfinite(tank)) tank = -1.0f;
    int cube_idx = hw_temp_index(HW_TEMP_CUBE);
    float slope = cube_idx >= 0 ? hal_1wire_slope(cube_idx) : 0.0f;
    int rising = cube_idx >= 0 ? (hal_1wire_rising(cube_idx) ? 1 : 0) : 0;

    char json[1792];
    snprintf(json, sizeof(json),
        "{\"type\":%d,\"stage\":%d,\"cube\":%.2f,\"col\":%.2f,"
        "\"power\":%.1f,\"power_auto\":%.1f,\"power_manual\":%d,"
        "\"heads\":%.0f,\"body\":%.0f,\"tails\":%.0f,"
        "\"pressure\":%.2f,\"vacuum\":%.0f,\"running\":%d,"
        "\"flow\":%.2f,\"level\":%.0f,\"scale_ml\":%.0f,"
        "\"jacket\":%.1f,\"tank\":%.1f,"
        "\"paused\":%d,\"elapsed\":%u,\"safety\":%d,"
        "\"estop\":%d,\"estop_healthy\":%d,\"service\":%d,"
        "\"level_required_fault\":%d,"
        "\"slope0\":%.2f,\"rising0\":%d,"
        "\"mode\":\"%s\",\"ctrl_mode\":%d,\"out_dev\":%d,"
        "\"regulator\":%d,\"regulator_name\":\"%s\",\"regulator_ready\":%d,"
        "\"regulator_gpio\":%d,\"pwm_freq_hz\":%d,"
        "\"pwm_resolution\":%d,\"pwm_inverted\":%d,"
        "\"analog_range\":%d,\"analog_min_pct\":%.2f,"
        "\"analog_max_pct\":%.2f,\"ssvr_power_sqrt\":%d,"
        "\"regulator_error\":\"%s\"}",
        (int)s->type, (int)s->stage, s->cube_temp, s->column_temp,
        ctrl_heater_get_percent(), ctrl_heater_get_auto_percent(),
        ctrl_heater_manual_active() ? 1 : 0,
        s->heads_collected_ml, s->body_collected_ml,
        s->tails_collected_ml, s->pressure_bar, s->vacuum_mbar,
        s->is_running ? 1 : 0,
        (double)flow, (double)level, (double)scale_ml,
        (double)jacket, (double)tank,
        s->is_paused ? 1 : 0, (unsigned)s->elapsed_sec,
        ctrl_safety_ok() ? 1 : 0, estop_is_pressed() ? 1 : 0,
        estop_is_healthy() ? 1 : 0,
        control_config_service_enabled() ? 1 : 0,
        level_is_required_fault() ? 1 : 0,
        (double)slope, rising,
        mode_get() ? mode_get()->id : "none",
        (int)control_config_get_mode(),
        (int)control_config_get_output_device(),
        (int)control_config_get_regulator(),
        control_config_regulator_name(control_config_get_regulator()),
        regulator_is_ready() ? 1 : 0,
        regulator_get_gpio(), regulator_get_pwm_freq(),
        regulator_get_pwm_resolution(), regulator_get_pwm_inverted() ? 1 : 0,
        regulator_get_analog_range(),
        (double)regulator_get_analog_min_pct(),
        (double)regulator_get_analog_max_pct(),
        regulator_get_ssvr_power_sqrt() ? 1 : 0,
        regulator_last_error());
    return send_json(r, json);
}

typedef struct { const char *id; process_type_t type; } mode_map_t;

static const mode_map_t MODES[] = {
    {"distill", PROCESS_DISTILL}, {"rectify", PROCESS_RECTIFY},
    {"beer",    PROCESS_BEER},    {"pvk",     PROCESS_PVK},
    {"nbk",     PROCESS_NBK},     {"vacuum",  PROCESS_VACUUM},
    {"double",  PROCESS_DOUBLE},  {"gabriel", PROCESS_GABRIEL},
    {"ferment", PROCESS_FERMENT},
};

static const mode_map_t* find_mode(const char *id) {
    if (!id) return NULL;
    for (size_t i = 0; i < sizeof(MODES) / sizeof(*MODES); i++)
        if (strcmp(MODES[i].id, id) == 0) return &MODES[i];
    return NULL;
}

static esp_err_t h_start(httpd_req_t *r) {
    if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);
    if (roles_is_enabled() && (int)roles_get_current() < (int)ROLE_OPERATOR)
        return send_err(r, "403 Forbidden", "{\"error\":\"role\"}");

    char body[192];
    int n = recv_body(r, body, sizeof(body));
    if (n < 0) return send_err(r, "400 Bad Request", "{\"error\":\"body too large\"}");

    process_type_t type = state_get()->type;
    char mode_id[32] = "";

    if (n > 0) {
        cJSON *root = cJSON_Parse(body);
        if (root) {
            const char *mid = json_string(root, "mode");
            if (mid) {
                strncpy(mode_id, mid, sizeof(mode_id) - 1);
                mode_id[sizeof(mode_id) - 1] = '\0';
            }
            const cJSON *tv = cJSON_GetObjectItemCaseSensitive(root, "type");
            if (cJSON_IsNumber(tv)) type = (process_type_t)tv->valueint;
            cJSON_Delete(root);
        }
    }

    const process_mode_t *m = NULL;
    if (mode_id[0]) {
        const mode_map_t *mm = find_mode(mode_id);
        if (!mm) return send_err(r, "400 Bad Request", "{\"error\":\"unknown mode\"}");
        type = mm->type;
        m = mode_from_type(type);
    } else {
        m = mode_get();
        if (!m) m = mode_from_type(PROCESS_DISTILL);
    }

    if (!m) return send_err(r, "400 Bad Request", "{\"error\":\"mode not available\"}");
    char why[96], err[160];
    if (!process_control_hw_ready(type, why, sizeof(why))) {
        snprintf(err, sizeof(err), "{\"error\":\"%s\"}", why);
        return send_err(r, "409 Conflict", err);
    }
    if (!ctrl_safety_ok())
        return send_err(r, "409 Conflict", "{\"error\":\"safety lock active\"}");
    if (estop_is_pressed())
        return send_err(r, "409 Conflict", "{\"error\":\"estop active\"}");

    // Всегда перезапускаем режим: повторный старт после аварии не должен
    // продолжать старый этап (например, ST_BODY при остывшем кубе).
    mode_set(m);

    process_state_t *s = state_get();
    s->type = type;
    s->is_running = true;
    s->is_paused = false;

    events_log(EVENT_INFO, "Web: start %s", m->id);
    return send_json(r, "{\"status\":\"ok\"}");
}

static esp_err_t h_stop(httpd_req_t *r) {
    if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);
    ctrl_heater_force_off();
    ctrl_dose_stop();
    if (mode_get() && mode_get()->stop) mode_get()->stop();
    state_get()->is_running = false;
    state_get()->is_paused = false;
    events_log(EVENT_INFO, "Web: stop");
    return send_json(r, "{\"status\":\"ok\"}");
}

static esp_err_t h_pause(httpd_req_t *r) {
    if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);
    if (!state_get()->is_running) {
        httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "{\"error\":\"not running\"}");
        return ESP_FAIL;
    }
    state_get()->is_paused = true;
    events_log(EVENT_INFO, "Web: pause");
    return send_json(r, "{\"status\":\"ok\"}");
}

static esp_err_t h_power(httpd_req_t *r) {
    if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);

    char body[64];
    int n = recv_body(r, body, sizeof(body));
    if (n < 0) return send_err(r, "400 Bad Request", "{\"error\":\"bad body\"}");

    bool auto_mode = false;
    float pct = 0.0f;
    if (n > 0) {
        cJSON *root = cJSON_Parse(body);
        if (root) {
            const cJSON *mv = cJSON_GetObjectItemCaseSensitive(root, "mode");
            if (cJSON_IsString(mv) && strcmp(mv->valuestring, "auto") == 0)
                auto_mode = true;
            const cJSON *pv = cJSON_GetObjectItemCaseSensitive(root, "power");
            if (cJSON_IsNumber(pv)) pct = (float)pv->valuedouble;
            else {
                const cJSON *vv = cJSON_GetObjectItemCaseSensitive(root, "value");
                if (cJSON_IsNumber(vv)) pct = (float)vv->valuedouble;
            }
            cJSON_Delete(root);
        } else {
            pct = (float)atof(body);
        }
    } else {
        return send_err(r, "400 Bad Request", "{\"error\":\"power required\"}");
    }

    if (auto_mode) {
        ctrl_heater_set_auto();
        events_log(EVENT_USER_ACTION, "Web: power auto");
    } else {
        if (pct < 0) pct = 0;
        if (pct > 100) pct = 100;

        esp_err_t err = safety_gate_submit("web_power", pct);
        if (err == ESP_ERR_INVALID_STATE)
            return send_err(r, "409 Conflict",
                            "{\"error\":\"regulator not ready or safety active\"}");
        if (err != ESP_OK)
            return send_err(r, "400 Bad Request", "{\"error\":\"invalid power\"}");
        events_log(EVENT_USER_ACTION, "Web: manual power %.0f%%", pct);
    }

    char out[448];
    snprintf(out, sizeof(out),
             "{\"power\":%.1f,\"power_auto\":%.1f,"
             "\"power_manual\":%d,\"running\":%d,\"service\":%d,"
             "\"regulator_ready\":%d,"
             "\"regulator_gpio\":%d,\"pwm_freq_hz\":%d,"
             "\"pwm_resolution\":%d,\"pwm_inverted\":%d,"
             "\"analog_range\":%d,\"analog_min_pct\":%.2f,"
             "\"analog_max_pct\":%.2f,\"ssvr_power_sqrt\":%d,"
             "\"regulator_error\":\"%s\"}",
             ctrl_heater_get_percent(), ctrl_heater_get_auto_percent(),
             ctrl_heater_manual_active() ? 1 : 0,
             state_get()->is_running ? 1 : 0,
             control_config_service_enabled() ? 1 : 0,
             regulator_is_ready() ? 1 : 0,
             regulator_get_gpio(), regulator_get_pwm_freq(),
             regulator_get_pwm_resolution(),
             regulator_get_pwm_inverted() ? 1 : 0,
             regulator_get_analog_range(),
             (double)regulator_get_analog_min_pct(),
             (double)regulator_get_analog_max_pct(),
             regulator_get_ssvr_power_sqrt() ? 1 : 0,
             regulator_last_error());
    return send_json(r, out);
}

static esp_err_t h_safety(httpd_req_t *r) {
    if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);

    if (r->method == HTTP_POST && !ctrl_safety_ok()) {
        esp_err_t err = ctrl_safety_reset();
        if (err != ESP_OK) {
            char out[256];
            snprintf(out, sizeof(out),
                     "{\"error\":\"reset refused\",\"detail\":\"%s\","
                     "\"safety\":0,\"estop\":%d,"
                     "\"relay_enabled\":%d,\"relay_closed\":0,"
                     "\"relay_forced\":%d}",
                     ctrl_safety_reset_error(), estop_is_pressed() ? 1 : 0,
                     hal_safety_relay_is_enabled() ? 1 : 0,
                     hal_safety_relay_is_forced() ? 1 : 0);
            return send_err(r, "409 Conflict", out);
        }
    }

    lv_result_t lr = {0};
    bool have_loop = loop_verify_last_result(&lr);

    char json[320];
    snprintf(json, sizeof(json),
             "{\"safety\":%d,\"estop\":%d,"
             "\"relay_enabled\":%d,\"relay_closed\":%d,"
             "\"relay_forced\":%d,"
             "\"loop_valid\":%d,\"loop_aggregate\":%lu,"
             "\"loop_flags\":%lu,\"loop_latency_us\":%lu}",
             ctrl_safety_ok() ? 1 : 0, estop_is_pressed() ? 1 : 0,
             hal_safety_relay_is_enabled() ? 1 : 0,
             hal_safety_relay_is_closed() ? 1 : 0,
             hal_safety_relay_is_forced() ? 1 : 0,
             have_loop ? 1 : 0,
             (unsigned long)lr.aggregate,
             (unsigned long)(lr.sensor_flags | lr.program_flags |
                             lr.actuator_flags),
             (unsigned long)lr.latency_e2e_us);
    return send_json(r, json);
}

// --- установки управления (control_config) ----------------------------
static esp_err_t h_control(httpd_req_t *r) {
    if (r->method == HTTP_GET) {
        if (api_check_read_auth(r) != ESP_OK) return api_reject_unauthorized(r);
        regulator_settings_t rs;
        control_config_get_regulator_settings(&rs);
        char json[896];
        snprintf(json, sizeof(json),
                 "{\"ctrl_mode\":%d,\"out_dev\":%d,\"hys\":%.2f,"
                 "\"rate_mlmin\":%.2f,\"flow_mlmin\":%.2f,\"steps_per_ml\":%u,"
                 "\"regulator\":%d,\"regulator_name\":\"%s\",\"reg_baud\":%u,"
                 "\"reg_gpio\":%d,\"reg_freq\":%lu,\"reg_resolution\":%u,"
                 "\"reg_inverted\":%d,\"analog_range\":%d,"
                 "\"analog_min_pct\":%.2f,\"analog_max_pct\":%.2f,"
                 "\"ssvr_power_sqrt\":%d,"
                 "\"regulator_ready\":%d,\"regulator_gpio\":%d,"
                 "\"pwm_freq_hz\":%lu,\"pwm_resolution\":%u,"
                 "\"pwm_inverted\":%d,\"regulator_error\":\"%s\","
                 "\"adaptive_pid\":%d}",
                 (int)control_config_get_mode(),
                 (int)control_config_get_output_device(),
                 control_config_get_hys(),
                 control_config_get_rate_ml_min(),
                 control_config_get_flow_ml_min(),
                 (unsigned)control_config_get_steps_per_ml(),
                 (int)rs.type,
                 control_config_regulator_name(rs.type),
                 (unsigned)rs.uart_baud,
                 rs.pwm_gpio,
                 (unsigned long)rs.pwm_freq_hz,
                 (unsigned)rs.pwm_resolution,
                 rs.pwm_inverted ? 1 : 0,
                 (int)rs.analog_range,
                 (double)rs.analog_min_pct, (double)rs.analog_max_pct,
                 rs.ssvr_power_sqrt ? 1 : 0,
                 regulator_is_ready() ? 1 : 0,
                 regulator_get_gpio(),
                 (unsigned long)rs.pwm_freq_hz,
                 (unsigned)rs.pwm_resolution,
                 rs.pwm_inverted ? 1 : 0,
                 regulator_last_error(),
                 control_config_get_adaptive_pid() ? 1 : 0);
        return send_json(r, json);
    }

    if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);

    char body[512];
    if (recv_body(r, body, sizeof(body)) <= 0)
        return send_err(r, "400 Bad Request", "{\"error\":\"empty body\"}");

    cJSON *root = cJSON_Parse(body);
    if (!root) return send_err(r, "400 Bad Request", "{\"error\":\"bad json\"}");

    esp_err_t err = ESP_OK;
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(root, "ctrl_mode");
    if (cJSON_IsNumber(v))
        err = control_config_set_mode((control_mode_t)v->valueint);
    v = cJSON_GetObjectItemCaseSensitive(root, "out_dev");
    if (err == ESP_OK && cJSON_IsNumber(v))
        err = control_config_set_output_device((output_device_t)v->valueint);
    v = cJSON_GetObjectItemCaseSensitive(root, "hys");
    if (err == ESP_OK && cJSON_IsNumber(v))
        err = control_config_set_hys((float)v->valuedouble);
    v = cJSON_GetObjectItemCaseSensitive(root, "rate_mlmin");
    if (err == ESP_OK && cJSON_IsNumber(v))
        err = control_config_set_rate_ml_min((float)v->valuedouble);
    v = cJSON_GetObjectItemCaseSensitive(root, "flow_mlmin");
    if (err == ESP_OK && cJSON_IsNumber(v))
        err = control_config_set_flow_ml_min((float)v->valuedouble);
    v = cJSON_GetObjectItemCaseSensitive(root, "steps_per_ml");
    if (err == ESP_OK && cJSON_IsNumber(v))
        err = control_config_set_steps_per_ml((uint32_t)v->valueint);
    v = cJSON_GetObjectItemCaseSensitive(root, "algo");
    if (err == ESP_OK && cJSON_IsNumber(v))
        err = fsel_set_algo((select_algo_t)v->valueint);
    v = cJSON_GetObjectItemCaseSensitive(root, "adaptive_pid");
    if (err == ESP_OK && cJSON_IsBool(v))
        err = control_config_set_adaptive_pid(cJSON_IsTrue(v));

    // Настройки регулятора применяются одной транзакцией: сначала новый
    // выход инициализируется, затем сохраняется в NVS. При ошибке старый
    // конфиг восстанавливается.
    regulator_settings_t rs;
    bool reg_change = false;
    bool reg_error = false;
    if (err == ESP_OK &&
        control_config_get_regulator_settings(&rs) == ESP_OK) {
        v = cJSON_GetObjectItemCaseSensitive(root, "regulator");
        if (cJSON_IsNumber(v)) {
            rs.type = (regulator_type_t)v->valueint;
            reg_change = true;
        }
        v = cJSON_GetObjectItemCaseSensitive(root, "reg_gpio");
        if (cJSON_IsNumber(v)) {
            rs.pwm_gpio = (int16_t)v->valueint;
            reg_change = true;
        }
        v = cJSON_GetObjectItemCaseSensitive(root, "reg_freq");
        if (cJSON_IsNumber(v)) {
            rs.pwm_freq_hz = (uint32_t)v->valueint;
            reg_change = true;
        }
        v = cJSON_GetObjectItemCaseSensitive(root, "reg_resolution");
        if (cJSON_IsNumber(v)) {
            rs.pwm_resolution = (uint8_t)v->valueint;
            reg_change = true;
        }
        v = cJSON_GetObjectItemCaseSensitive(root, "reg_inverted");
        if (cJSON_IsBool(v)) {
            rs.pwm_inverted = cJSON_IsTrue(v);
            reg_change = true;
        }
        v = cJSON_GetObjectItemCaseSensitive(root, "reg_baud");
        if (cJSON_IsNumber(v)) {
            rs.uart_baud = (uint32_t)v->valueint;
            reg_change = true;
        }
        v = cJSON_GetObjectItemCaseSensitive(root, "analog_range");
        if (cJSON_IsNumber(v)) {
            rs.analog_range = (analog_range_t)v->valueint;
            reg_change = true;
        }
        v = cJSON_GetObjectItemCaseSensitive(root, "analog_min_pct");
        if (cJSON_IsNumber(v)) {
            rs.analog_min_pct = (float)v->valuedouble;
            reg_change = true;
        }
        v = cJSON_GetObjectItemCaseSensitive(root, "analog_max_pct");
        if (cJSON_IsNumber(v)) {
            rs.analog_max_pct = (float)v->valuedouble;
            reg_change = true;
        }
        v = cJSON_GetObjectItemCaseSensitive(root, "ssvr_power_sqrt");
        if (cJSON_IsBool(v)) {
            rs.ssvr_power_sqrt = cJSON_IsTrue(v);
            reg_change = true;
        }
        if (reg_change) {
            err = regulator_apply_settings(&rs);
            if (err != ESP_OK) reg_error = true;
        }
    }

    cJSON_Delete(root);

    if (!reg_error && err != ESP_OK)
        return send_err(r, "400 Bad Request",
                        "{\"error\":\"invalid setting\"}");

    if (err == ESP_ERR_INVALID_ARG || err == ESP_ERR_INVALID_STATE) {
        char out[192];
        snprintf(out, sizeof(out),
                 "{\"error\":\"invalid regulator settings\","
                 "\"detail\":\"%s\"}", regulator_last_error());
        return send_err(r, "400 Bad Request", out);
    }
    if (err != ESP_OK) {
        char out[224];
        snprintf(out, sizeof(out),
                 "{\"error\":\"regulator init failed\",\"detail\":\"%s\","
                 "\"regulator_ready\":%d}", regulator_last_error(),
                 regulator_is_ready() ? 1 : 0);
        return send_err(r, "409 Conflict", out);
    }
    return send_json(r, "{\"status\":\"ok\"}");
}

// --- отбор -----------------------------------------------------------
static esp_err_t h_dose(httpd_req_t *r) {
    if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);

    char body[128];
    if (recv_body(r, body, sizeof(body)) <= 0)
        return send_err(r, "400 Bad Request", "{\"error\":\"empty body\"}");

    cJSON *root = cJSON_Parse(body);
    if (!root) return send_err(r, "400 Bad Request", "{\"error\":\"bad json\"}");

    const cJSON *vol = cJSON_GetObjectItemCaseSensitive(root, "volume_ml");
    const cJSON *rate = cJSON_GetObjectItemCaseSensitive(root, "rate_mlmin");
    float ml = cJSON_IsNumber(vol) ? (float)vol->valuedouble : 0.0f;
    float rm = cJSON_IsNumber(rate) ? (float)rate->valuedouble
                                    : control_config_get_rate_ml_min();
    cJSON_Delete(root);

    if (ml <= 0.0f) return send_err(r, "400 Bad Request", "{\"error\":\"volume_ml\"}");
    esp_err_t derr = pump_dose_volume(ml, rm);
    if (derr == ESP_ERR_INVALID_SIZE)
        return send_err(r, "400 Bad Request",
                        "{\"error\":\"volume exceeds DOSER_MAX_DOSE_ML\"}");
    if (derr == ESP_ERR_INVALID_ARG)
        return send_err(r, "400 Bad Request", "{\"error\":\"invalid volume/rate\"}");
    if (derr != ESP_OK)
        return send_err(r, "409 Conflict", "{\"error\":\"dose rejected\"}");

    return send_json(r, "{\"status\":\"ok\"}");
}

// --- диагностика E-STOP -----------------------------------------------
static esp_err_t h_stepper(httpd_req_t *r) {
    if (r->method == HTTP_GET) {
        if (api_check_read_auth(r) != ESP_OK)
            return api_reject_unauthorized(r);
        char json[768];
        ctrl_stepper_status_json(json, sizeof(json));
        return send_json(r, json);
    }

    if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);

    char body[512];
    if (recv_body(r, body, sizeof(body)) <= 0)
        return send_err(r, "400 Bad Request", "{\"error\":\"empty body\"}");
    cJSON *root = cJSON_Parse(body);
    if (!root) return send_err(r, "400 Bad Request", "{\"error\":\"bad json\"}");

    const char *action = json_string(root, "action");
    esp_err_t err = ESP_ERR_INVALID_ARG;

    if (action && strcmp(action, "stop") == 0) {
        ctrl_stepper_stop_all();
        err = ESP_OK;
    } else if (action && strcmp(action, "stirrer") == 0) {
        const cJSON *on = cJSON_GetObjectItemCaseSensitive(root, "on");
        const cJSON *rev = cJSON_GetObjectItemCaseSensitive(root, "reverse");
        const cJSON *rpm = cJSON_GetObjectItemCaseSensitive(root, "rpm");
        err = ctrl_stepper_stirrer(cJSON_IsTrue(on),
                                   cJSON_IsTrue(rev),
                                   cJSON_IsNumber(rpm)
                                       ? (float)rpm->valuedouble : 60.0f);
    } else if (action && strcmp(action, "dose") == 0) {
        const cJSON *ml = cJSON_GetObjectItemCaseSensitive(root, "ml");
        const cJSON *rate = cJSON_GetObjectItemCaseSensitive(root, "rate_ml_min");
        if (!cJSON_IsNumber(ml)) {
            cJSON_Delete(root);
            return send_err(r, "400 Bad Request", "{\"error\":\"ml required\"}");
        }
        bool queued = ctrl_dose_volume((float)ml->valuedouble,
                                       cJSON_IsNumber(rate)
                                           ? (float)rate->valuedouble : 60.0f);
        cJSON_Delete(root);
        if (!queued)
            return send_err(r, "409 Conflict",
                            "{\"error\":\"dose rejected (safety/queue)\"}");
        httpd_resp_set_status(r, "202 Accepted");
        return send_json(r, "{\"status\":\"queued\"}");
    } else if (action && (strcmp(action, "move") == 0 ||
                          strcmp(action, "home") == 0)) {
        if (!control_config_service_enabled()) {
            cJSON_Delete(root);
            return send_err(r, "409 Conflict",
                            "{\"error\":\"service mode required\"}");
        }
        const cJSON *axis = cJSON_GetObjectItemCaseSensitive(root, "axis");
        int ax = cJSON_IsNumber(axis) ? axis->valueint : 0;
        if (ax < 0 || ax >= STEPPER_AXIS_COUNT) {
            cJSON_Delete(root);
            return send_err(r, "400 Bad Request", "{\"error\":\"axis\"}");
        }
        if (strcmp(action, "home") == 0) {
            err = ctrl_stepper_home((stepper_axis_t)ax);
        } else {
            const cJSON *steps = cJSON_GetObjectItemCaseSensitive(root, "steps");
            const cJSON *speed = cJSON_GetObjectItemCaseSensitive(root, "speed_sps");
            if (!cJSON_IsNumber(steps)) {
                cJSON_Delete(root);
                return send_err(r, "400 Bad Request", "{\"error\":\"steps\"}");
            }
            err = ctrl_stepper_move_service(
                (stepper_axis_t)ax, steps->valueint,
                cJSON_IsNumber(speed) ? (uint16_t)speed->valueint : 0);
        }
    } else if (action && strcmp(action, "config") == 0) {
        stepper_settings_t cfg = *ctrl_stepper_settings();
        const cJSON *v = cJSON_GetObjectItemCaseSensitive(root, "enabled");
        if (cJSON_IsBool(v)) cfg.enabled = cJSON_IsTrue(v);
        v = cJSON_GetObjectItemCaseSensitive(root, "address");
        if (cJSON_IsNumber(v)) cfg.address = (uint8_t)v->valueint;
        v = cJSON_GetObjectItemCaseSensitive(root, "max_speed_sps");
        if (cJSON_IsNumber(v)) cfg.max_speed_sps = (uint16_t)v->valueint;
        v = cJSON_GetObjectItemCaseSensitive(root, "accel_sps2");
        if (cJSON_IsNumber(v)) cfg.accel_sps2 = (uint16_t)v->valueint;
        v = cJSON_GetObjectItemCaseSensitive(root, "steps_per_ml");
        if (cJSON_IsNumber(v)) cfg.steps_per_ml = (float)v->valuedouble;
        v = cJSON_GetObjectItemCaseSensitive(root, "stirrer_steps_per_rev");
        if (cJSON_IsNumber(v)) cfg.stirrer_steps_per_rev = (uint16_t)v->valueint;
        err = ctrl_stepper_apply_settings(&cfg);
    }

    cJSON_Delete(root);
    if (err != ESP_OK) {
        char out[128];
        snprintf(out, sizeof(out), "{\"error\":\"%s\"}", esp_err_to_name(err));
        return send_err(r, "409 Conflict", out);
    }

    char json[768];
    ctrl_stepper_status_json(json, sizeof(json));
    return send_json(r, json);
}

static esp_err_t h_hardware(httpd_req_t *r) {
    if (r->method == HTTP_GET) {
        if (api_check_read_auth(r) != ESP_OK)
            return api_reject_unauthorized(r);
        char json[1536];
        hw_profile_json(json, sizeof(json));
        return send_json(r, json);
    }

    if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);

    char body[1024];
    if (recv_body(r, body, sizeof(body)) <= 0)
        return send_err(r, "400 Bad Request", "{\"error\":\"empty body\"}");
    cJSON *root = cJSON_Parse(body);
    if (!root) return send_err(r, "400 Bad Request", "{\"error\":\"bad json\"}");

    hw_profile_t cfg = *hw_profile_get();
    static const char *const temp_keys[HW_TEMP_ROLE_COUNT] = {
        "cube", "column", "jacket", "tank"
    };
    const cJSON *temp = cJSON_GetObjectItemCaseSensitive(root, "temp");
    if (cJSON_IsObject(temp)) {
        for (int i = 0; i < HW_TEMP_ROLE_COUNT; i++) {
            const cJSON *v = cJSON_GetObjectItemCaseSensitive(
                temp, temp_keys[i]);
            if (cJSON_IsNumber(v)) cfg.temp[i] = (int8_t)v->valueint;
        }
    }

    const cJSON *devices = cJSON_GetObjectItemCaseSensitive(root, "devices");
    if (cJSON_IsObject(devices)) {
        for (int i = 0; i < HW_DEV_COUNT; i++) {
            const cJSON *v = cJSON_GetObjectItemCaseSensitive(
                devices, hw_device_key((hw_device_t)i));
            if (cJSON_IsBool(v)) {
                if (cJSON_IsTrue(v)) cfg.devices |= 1u << i;
                else cfg.devices &= ~(1u << i);
            }
        }
    }

    esp_err_t err = hw_profile_set(&cfg);
    cJSON_Delete(root);
    if (err != ESP_OK)
        return send_err(r, "400 Bad Request",
                        "{\"error\":\"invalid sensor index or mask\"}");

    char json[1536];
    hw_profile_json(json, sizeof(json));
    return send_json(r, json);
}

static esp_err_t h_beer(httpd_req_t *r) {
    if (r->method == HTTP_GET) {
        if (api_check_read_auth(r) != ESP_OK)
            return api_reject_unauthorized(r);
    } else if (api_check_auth(r) != ESP_OK) {
        return api_reject_unauthorized(r);
    }

    int count = beer_preset_count();
    if (r->method == HTTP_POST) {
        char body[128];
        if (recv_body(r, body, sizeof(body)) <= 0)
            return send_err(r, "400 Bad Request", "{\"error\":\"empty\"}");
        cJSON *root = cJSON_Parse(body);
        const cJSON *v = root ? cJSON_GetObjectItemCaseSensitive(root, "preset")
                              : NULL;
        if (cJSON_IsNumber(v)) beer_preset_select(v->valueint);
        if (root) cJSON_Delete(root);
    }

    int steps = 0;
    const beer_step_t *recipe = beer_preset_get(beer_preset_active(), &steps);
    char json[1024];
    int off = snprintf(json, sizeof(json),
                       "{\"active\":%d,\"count\":%d,\"steps\":[",
                       beer_preset_active(), count);
    for (int i = 0; i < steps && off < (int)sizeof(json); i++) {
        off += snprintf(json + off, sizeof(json) - off,
                        "%s{\"type\":%u,\"name\":\"%s\",\"temp\":%.1f,"
                        "\"duration_sec\":%u}",
                        i ? "," : "", recipe[i].type, recipe[i].name,
                        (double)recipe[i].temp,
                        (unsigned)recipe[i].duration_sec);
    }
    snprintf(json + off, sizeof(json) - off, "]}");
    return send_json(r, json);
}

static esp_err_t h_estop(httpd_req_t *r) {
    if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);

    char body[64];
    int n = recv_body(r, body, sizeof(body));
    (void)n;

    bool ack = n > 0 && strstr(body, "ack") != NULL;
    bool cleared = ack ? estop_acknowledge() : false;

    char json[160];
    snprintf(json, sizeof(json),
             "{\"estop\":%d,\"button\":%d,\"acked\":%d,\"source\":\"%s\"}",
             estop_is_pressed() ? 1 : 0, estop_button_down() ? 1 : 0,
             cleared ? 1 : 0, estop_source_name());
    return send_json(r, json);
}

// --- Lua --------------------------------------------------------------
static esp_err_t h_script_run(httpd_req_t *r) {
    if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);

    char body[256];
    if (recv_body(r, body, sizeof(body)) <= 0)
        return send_err(r, "400 Bad Request", "{\"error\":\"empty body\"}");

    cJSON *root = cJSON_Parse(body);
    if (!root) return send_err(r, "400 Bad Request", "{\"error\":\"bad json\"}");

    const char *path = json_string(root, "path");
    char path_copy[LUA_SCRIPT_PATH_MAX] = "";
    if (path) {
        strncpy(path_copy, path, sizeof(path_copy) - 1);
    }
    cJSON_Delete(root);

    if (!lua_engine_ready())
        return send_err(r, "503 Service Unavailable", "{\"error\":\"engine off\"}");

    lua_run_result_t res = lua_engine_request(path_copy);
    char json[256];
    snprintf(json, sizeof(json),
             "{\"status\":\"%s\",\"error\":\"%s\",\"enabled\":%d}",
             lua_run_result_name(res),
             lua_engine_last_error(),
             lua_engine_enabled() ? 1 : 0);

    if (res == LUA_RUN_OK) return send_json(r, json);
    if (res == LUA_RUN_BUSY) return send_err(r, "429 Too Many Requests", json);
    return send_err(r, "409 Conflict", json);
}

static esp_err_t h_script_state(httpd_req_t *r) {
    if (api_check_read_auth(r) != ESP_OK) return api_reject_unauthorized(r);
    char json[320];
    snprintf(json, sizeof(json),
             "{\"ready\":%d,\"enabled\":%d,\"status\":\"%s\",\"error\":\"%s\"}",
             lua_engine_ready() ? 1 : 0, lua_engine_enabled() ? 1 : 0,
             lua_engine_status(), lua_engine_last_error());
    return send_json(r, json);
}

// ============================================================
// settings / integrations
// ============================================================
static esp_err_t h_settings(httpd_req_t *r) {
    if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);

    char body[384];
    if (recv_body(r, body, sizeof(body)) <= 0)
        return send_err(r, "400 Bad Request", "{\"error\":\"empty body\"}");

    cJSON *root = cJSON_Parse(body);
    if (!root) return send_err(r, "400 Bad Request", "{\"error\":\"bad json\"}");

    const char *key = json_string(root, "key");
    if (!key || !key[0]) {
        cJSON_Delete(root);
        return send_err(r, "400 Bad Request", "{\"error\":\"key required\"}");
    }

    char key_copy[48];
    strncpy(key_copy, key, sizeof(key_copy) - 1);
    key_copy[sizeof(key_copy) - 1] = '\0';

    const cJSON *val = cJSON_GetObjectItemCaseSensitive(root, "value");
    bool ok = true;

    if (flags_key_exists(key_copy)) {
        flags_set_by_key(key_copy, json_bool(val));
    } else {
        nvs_handle_t h;
        if (nvs_open("settings", NVS_READWRITE, &h) == ESP_OK) {
            if (cJSON_IsString(val))       nvs_set_str(h, key_copy, val->valuestring);
            else if (cJSON_IsNumber(val))  nvs_set_i32(h, key_copy, val->valueint);
            else if (cJSON_IsBool(val))    nvs_set_u8(h, key_copy, cJSON_IsTrue(val) ? 1 : 0);
            else ok = false;
            if (ok) nvs_commit(h);
            nvs_close(h);
        } else {
            ok = false;
        }
    }
    cJSON_Delete(root);

    if (!ok) return send_err(r, "400 Bad Request", "{\"error\":\"unsupported value\"}");
    events_log(EVENT_INFO, "Web: setting %s updated", key_copy);
    return send_json(r, "{\"status\":\"ok\"}");
}

static esp_err_t h_wifi(httpd_req_t *r) {
    if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);

    char body[192];
    if (recv_body(r, body, sizeof(body)) <= 0)
        return send_err(r, "400 Bad Request", "{\"error\":\"empty body\"}");

    cJSON *root = cJSON_Parse(body);
    if (!root) return send_err(r, "400 Bad Request", "{\"error\":\"bad json\"}");

    const char *ssid = json_string(root, "ssid");
    const char *pass = json_string(root, "pass");
    if (!ssid || strlen(ssid) == 0 || strlen(ssid) > 31) {
        cJSON_Delete(root);
        return send_err(r, "400 Bad Request", "{\"error\":\"ssid required\"}");
    }
    if (pass && strlen(pass) > 63) {
        cJSON_Delete(root);
        return send_err(r, "400 Bad Request", "{\"error\":\"password too long\"}");
    }
    if (pass && pass[0] && strlen(pass) < 8) {
        cJSON_Delete(root);
        return send_err(r, "400 Bad Request", "{\"error\":\"WPA2 needs >=8 chars\"}");
    }

    nvs_handle_t h;
    if (nvs_open("wifi", NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_str(h, "ssid", ssid);
        nvs_set_str(h, "pass", pass ? pass : "");
        nvs_commit(h);
        nvs_close(h);
    }
    cJSON_Delete(root);
    events_log(EVENT_INFO, "Web: Wi-Fi credentials saved (reboot required)");
    return send_json(r, "{\"status\":\"ok\",\"reboot\":true}");
}

static esp_err_t h_mqtt(httpd_req_t *r) {
    if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);

    char body[384];
    if (recv_body(r, body, sizeof(body)) <= 0)
        return send_err(r, "400 Bad Request", "{\"error\":\"empty body\"}");

    cJSON *root = cJSON_Parse(body);
    if (!root) return send_err(r, "400 Bad Request", "{\"error\":\"bad json\"}");

    const char *uri  = json_string(root, "uri");
    const char *user = json_string(root, "user");
    const char *pass = json_string(root, "pass");
    mqtt_set_config(uri, user, pass);
    cJSON_Delete(root);
    return send_json(r, "{\"status\":\"ok\"}");
}

static esp_err_t h_telegram(httpd_req_t *r) {
    if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);

    char body[384];
    if (recv_body(r, body, sizeof(body)) <= 0)
        return send_err(r, "400 Bad Request", "{\"error\":\"empty body\"}");

    cJSON *root = cJSON_Parse(body);
    if (!root) return send_err(r, "400 Bad Request", "{\"error\":\"bad json\"}");

    const char *token = json_string(root, "token");
    const char *chat  = json_string(root, "chat_id");
    const char *secret = json_string(root, "webhook_secret");
    telegram_set_credentials(token, chat);
    telegram_set_webhook_secret(secret);
    cJSON_Delete(root);
    return send_json(r, "{\"status\":\"ok\"}");
}

static esp_err_t h_brewfather(httpd_req_t *r) {
    if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);

    char body[384];
    if (recv_body(r, body, sizeof(body)) <= 0)
        return send_err(r, "400 Bad Request", "{\"error\":\"empty body\"}");

    cJSON *root = cJSON_Parse(body);
    if (!root) return send_err(r, "400 Bad Request", "{\"error\":\"bad json\"}");

    const char *uid = json_string(root, "user_id");
    const char *key = json_string(root, "api_key");
    brewfather_set_credentials(uid, key);
    cJSON_Delete(root);
    return send_json(r, "{\"status\":\"ok\"}");
}

static esp_err_t h_calendar(httpd_req_t *r) {
    if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);

    char body[768];
    if (recv_body(r, body, sizeof(body)) <= 0)
        return send_err(r, "400 Bad Request", "{\"error\":\"empty body\"}");

    cJSON *root = cJSON_Parse(body);
    if (!root) return send_err(r, "400 Bad Request", "{\"error\":\"bad json\"}");

    const char *token = json_string(root, "token");
    if (token) calendar_set_token(token);

    const cJSON *en = cJSON_GetObjectItemCaseSensitive(root, "enabled");
    if (en) calendar_set_enabled(json_bool(en));

    cJSON_Delete(root);
    return send_json(r, "{\"status\":\"ok\"}");
}

// ============================================================
// Remote / Blynk + MQTT bridge
// ============================================================
static void json_copy_str(char *dst, size_t max, const cJSON *v) {
    if (!cJSON_IsString(v) || !v->valuestring) return;
    strncpy(dst, v->valuestring, max - 1);
    dst[max - 1] = '\0';
}

static void remote_status_json(char *json, size_t max) {
    remote_status_t st;
    remote_get_status(&st);
    snprintf(json, max,
        "{\"enabled\":%d,\"token_set\":%d,\"blynk\":%d,"
        "\"mqtt_configured\":%d,\"mqtt\":%d,"
        "\"host\":\"%s\",\"port\":%u,\"vref_mv\":%u}",
        st.enabled ? 1 : 0, st.token_set ? 1 : 0,
        st.blynk_connected ? 1 : 0,
        st.mqtt_configured ? 1 : 0, st.mqtt_connected ? 1 : 0,
        st.host, (unsigned)st.port, (unsigned)st.vref_mv);
}

static esp_err_t h_remote(httpd_req_t *r) {
    if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);

    if (r->method == HTTP_GET) {
        char json[512];
        remote_status_json(json, sizeof(json));
        return send_json(r, json);
    }

    char body[768];
    if (recv_body(r, body, sizeof(body)) <= 0)
        return send_err(r, "400 Bad Request", "{\"error\":\"empty body\"}");

    cJSON *root = cJSON_Parse(body);
    if (!root)
        return send_err(r, "400 Bad Request", "{\"error\":\"bad json\"}");

    remote_cfg_t cfg;
    remote_get_config(&cfg);

    json_copy_str(cfg.host, sizeof(cfg.host),
                  cJSON_GetObjectItemCaseSensitive(root, "host"));
    json_copy_str(cfg.token, sizeof(cfg.token),
                  cJSON_GetObjectItemCaseSensitive(root, "token"));
    json_copy_str(cfg.mqtt_uri, sizeof(cfg.mqtt_uri),
                  cJSON_GetObjectItemCaseSensitive(root, "mqtt_uri"));
    json_copy_str(cfg.mqtt_user, sizeof(cfg.mqtt_user),
                  cJSON_GetObjectItemCaseSensitive(root, "mqtt_user"));
    json_copy_str(cfg.mqtt_pass, sizeof(cfg.mqtt_pass),
                  cJSON_GetObjectItemCaseSensitive(root, "mqtt_pass"));
    json_copy_str(cfg.mqtt_token, sizeof(cfg.mqtt_token),
                  cJSON_GetObjectItemCaseSensitive(root, "mqtt_token"));

    const cJSON *v = cJSON_GetObjectItemCaseSensitive(root, "enabled");
    if (cJSON_IsBool(v)) cfg.enabled = cJSON_IsTrue(v);
    else if (cJSON_IsNumber(v)) cfg.enabled = (v->valueint != 0);

    v = cJSON_GetObjectItemCaseSensitive(root, "port");
    if (cJSON_IsNumber(v) && v->valueint > 0 && v->valueint <= 65535)
        cfg.port = (uint16_t)v->valueint;

    v = cJSON_GetObjectItemCaseSensitive(root, "vref_mv");
    if (cJSON_IsNumber(v) && v->valueint > 0)
        cfg.vref_mv = (uint16_t)v->valueint;

    esp_err_t err = remote_configure(&cfg);
    cJSON_Delete(root);

    if (err != ESP_OK)
        return send_err(r, "400 Bad Request",
                        "{\"error\":\"invalid config\"}");

    char json[512];
    remote_status_json(json, sizeof(json));
    return send_json(r, json);
}

// ============================================================
// OTA
// ============================================================
static esp_err_t h_ota_post(httpd_req_t *r) {
    if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);
    if (!ota_guard_try_reserve()) {
        char out[128];
        snprintf(out, sizeof(out), "{\"error\":\"%s\"}", ota_guard_reason());
        return send_err(r, "409 Conflict", out);
    }
    ota_guard_set_web_running(true);
    ota_guard_release();

    const esp_partition_t *p = esp_ota_get_next_update_partition(NULL);
    if (!p) {
        ota_guard_set_web_running(false);
        return send_err(r, "500 Internal Server Error", "{\"error\":\"no ota partition\"}");
    }
    if (r->content_len <= 0) {
        ota_guard_set_web_running(false);
        return send_err(r, "400 Bad Request", "{\"error\":\"empty image\"}");
    }

    esp_ota_handle_t h = 0;
    esp_err_t err = esp_ota_begin(p, OTA_SIZE_UNKNOWN, &h);
    if (err != ESP_OK) {
        ota_guard_set_web_running(false);
        events_log(EVENT_ERROR, "Web OTA begin failed: %s", esp_err_to_name(err));
        return send_err(r, "500 Internal Server Error", "{\"error\":\"ota begin\"}");
    }

    char buf[1024];
    int n;
    int remaining = r->content_len;
    int64_t deadline = esp_timer_get_time() +
                       (int64_t)OTA_WEB_TIMEOUT_MS * 1000LL;
    while (remaining > 0) {
        if (esp_timer_get_time() > deadline) {
            esp_ota_abort(h);
            ota_guard_set_web_running(false);
            events_log(EVENT_ERROR, "Web OTA timeout");
            return send_err(r, "408 Request Timeout", "{\"error\":\"ota timeout\"}");
        }
        n = httpd_req_recv(r, buf, sizeof(buf));
        if (n <= 0) {
            if (n == HTTPD_SOCK_ERR_TIMEOUT) {
                WD_FEED("web");
                continue;
            }
            esp_ota_abort(h);
            ota_guard_set_web_running(false);
            events_log(EVENT_ERROR, "Web OTA receive failed");
            return send_err(r, "400 Bad Request", "{\"error\":\"receive\"}");
        }
        err = esp_ota_write(h, buf, n);
        if (err != ESP_OK) {
            esp_ota_abort(h);
            ota_guard_set_web_running(false);
            events_log(EVENT_ERROR, "Web OTA write failed: %s", esp_err_to_name(err));
            return send_err(r, "500 Internal Server Error", "{\"error\":\"write\"}");
        }
        remaining -= n;
        WD_FEED("web");
    }

    err = esp_ota_end(h);
    if (err != ESP_OK) {
        ota_guard_set_web_running(false);
        events_log(EVENT_ERROR, "Web OTA end failed: %s", esp_err_to_name(err));
        return send_err(r, "500 Internal Server Error", "{\"error\":\"validate\"}");
    }

    err = esp_ota_set_boot_partition(p);
    if (err != ESP_OK) {
        ota_guard_set_web_running(false);
        events_log(EVENT_ERROR, "Web OTA boot partition failed: %s", esp_err_to_name(err));
        return send_err(r, "500 Internal Server Error", "{\"error\":\"boot partition\"}");
    }

    events_log(EVENT_INFO, "Web OTA complete, rebooting");
    httpd_resp_sendstr(r, "{\"status\":\"ok\",\"reboot\":true}");
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
    return ESP_OK;
}

// ============================================================
// health / events / OTA-managed / backup / config
// ============================================================

// GET /api/trust — состояние доверия к датчикам.
static esp_err_t h_trust(httpd_req_t *r) {
    if (api_check_read_auth(r) != ESP_OK) return api_reject_unauthorized(r);

    trust_entry_t ents[TRUST_MAX_SENSORS];
    size_t n = trust_dashboard_snapshot(ents, TRUST_MAX_SENSORS);

    char json[1024];
    int off = snprintf(json, sizeof(json), "{\"sensors\":[");
    for (size_t i = 0; i < n && off > 0 && off < (int)sizeof(json); i++) {
        off += snprintf(json + off, sizeof(json) - (size_t)off,
                        "%s{\"id\":%u,\"trust\":%.3f,\"trend\":%.3f,"
                        "\"status\":%u,\"last_good_us\":%lld}",
                        i ? "," : "", (unsigned)ents[i].sensor_id,
                        (double)ents[i].trust, (double)ents[i].trend,
                        (unsigned)ents[i].status,
                        (long long)ents[i].last_good_us);
    }
    if (off > 0 && off < (int)sizeof(json) - 4)
        snprintf(json + off, sizeof(json) - (size_t)off, "]}");
    return send_json(r, json);
}

// GET /api/health — БЕЗ ключа: мониторингу нужно проверять устройство
// до провижининга ключа. Секретов в ответе нет.
static esp_err_t h_health(httpd_req_t *r) {
    return send_json(r, health_json());
}

// GET /api/events[?limit=N] — журнал событий в JSON.
static esp_err_t h_events(httpd_req_t *r) {
    if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);

    int limit = 64;
    size_t qlen = httpd_req_get_url_query_len(r);
    if (qlen > 0 && qlen < 64) {
        char q[64];
        if (httpd_req_get_url_query_str(r, q, sizeof(q)) == ESP_OK) {
            char val[16];
            if (httpd_query_key_value(q, "limit", val, sizeof(val)) == ESP_OK) {
                int v = atoi(val);
                if (v > 0 && v <= EVENTS_RING_MAX) limit = v;
            }
        }
    }
    return send_json(r, event_log_dump_json(limit));
}

// GET /api/ota — состояние (managed OTA), POST /api/ota — см. ниже.
static esp_err_t h_ota_get(httpd_req_t *r) {
    if (api_check_read_auth(r) != ESP_OK) return api_reject_unauthorized(r);
    return send_json(r, ota_manager_status_json());
}

// POST /api/config/export {"path":"/sdcard/cfg.json","secrets":false}
static esp_err_t h_config_export(httpd_req_t *r) {
    if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);

    char body[192];
    int n = recv_body(r, body, sizeof(body));
    (void)n;

    char path[96] = "/sdcard/cfg.json";
    bool secrets = false;

    if (n > 0) {
        cJSON *root = cJSON_Parse(body);
        if (root) {
            const char *p = json_string(root, "path");
            if (p && p[0]) {
                strncpy(path, p, sizeof(path) - 1);
                path[sizeof(path) - 1] = '\0';
            }
            secrets = json_bool(cJSON_GetObjectItemCaseSensitive(root, "secrets"));
            cJSON_Delete(root);
        }
    }

    esp_err_t err = config_backup_export(path, secrets);
    if (err != ESP_OK) return send_err(r, "400 Bad Request", "{\"error\":\"export\"}");

    char json[256];
    snprintf(json, sizeof(json), "{\"status\":\"ok\",\"info\":\"%s\"}",
             config_backup_info());
    return send_json(r, json);
}

// POST /api/config/import {"path":"/sdcard/cfg.json"}
static esp_err_t h_config_import(httpd_req_t *r) {
    if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);

    char body[192];
    int n = recv_body(r, body, sizeof(body));
    (void)n;

    char path[96] = "/sdcard/cfg.json";
    if (n > 0) {
        cJSON *root = cJSON_Parse(body);
        if (root) {
            const char *p = json_string(root, "path");
            if (p && p[0]) {
                strncpy(path, p, sizeof(path) - 1);
                path[sizeof(path) - 1] = '\0';
            }
            cJSON_Delete(root);
        }
    }

    esp_err_t err = config_backup_import(path);
    if (err != ESP_OK) return send_err(r, "400 Bad Request", "{\"error\":\"import\"}");

    char json[256];
    snprintf(json, sizeof(json), "{\"status\":\"ok\",\"info\":\"%s\"}",
             config_backup_info());
    return send_json(r, json);
}

// POST /api/config {"steps_per_ml":N,"ctrl_mode":...,"out_dev":...,"api_key":"..."}
// Единая точка изменения конфигурации (аналог SETUP.md-команд из задания).
static esp_err_t h_config(httpd_req_t *r) {
    if (r->method == HTTP_GET) {
        if (api_check_read_auth(r) != ESP_OK) return api_reject_unauthorized(r);
        regulator_settings_t rs;
        control_config_get_regulator_settings(&rs);
        char json[896];
        snprintf(json, sizeof(json),
                 "{\"schema\":%u,\"ctrl_mode\":%d,\"out_dev\":%d,\"hys\":%.2f,"
                 "\"rate_mlmin\":%.2f,\"flow_mlmin\":%.2f,\"steps_per_ml\":%u,"
                 "\"service\":%d,\"api_key_set\":%d,\"lua\":%d,"
                 "\"algo\":%d,\"regulator\":%d,\"reg_baud\":%u,"
                 "\"reg_gpio\":%d,\"reg_freq\":%lu,\"reg_resolution\":%u,"
                 "\"reg_inverted\":%d,\"analog_range\":%d,"
                 "\"analog_min_pct\":%.2f,\"analog_max_pct\":%.2f,"
                 "\"ssvr_power_sqrt\":%d,\"regulator_ready\":%d,"
                 "\"regulator_error\":\"%s\"}",
                 (unsigned)nvs_migration_schema_version(),
                 (int)control_config_get_mode(),
                 (int)control_config_get_output_device(),
                 control_config_get_hys(),
                 control_config_get_rate_ml_min(),
                 control_config_get_flow_ml_min(),
                 (unsigned)control_config_get_steps_per_ml(),
                 control_config_service_enabled() ? 1 : 0,
                 control_config_api_has_key() ? 1 : 0,
                 lua_engine_enabled() ? 1 : 0,
                 (int)fsel_get_algo(),
                 (int)rs.type,
                 (unsigned)rs.uart_baud,
                 rs.pwm_gpio,
                 (unsigned long)rs.pwm_freq_hz,
                 (unsigned)rs.pwm_resolution,
                 rs.pwm_inverted ? 1 : 0,
                 (int)rs.analog_range,
                 (double)rs.analog_min_pct, (double)rs.analog_max_pct,
                 rs.ssvr_power_sqrt ? 1 : 0,
                 regulator_is_ready() ? 1 : 0,
                 regulator_last_error());
        return send_json(r, json);
    }

    if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);
    return h_control(r);
}

// POST /api/config/generate-key — сгенерировать новый X-API-Key (требует
// текущий ключ или сервисный режим). Возвращает ключ ОДИН раз.
static esp_err_t h_config_generate_key(httpd_req_t *r) {
    bool service = control_config_service_enabled();
    bool has_key = control_config_api_has_key();

    if (has_key && !service) {
        // Ключ уже есть: сменить его можно только зная текущий.
        if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);
    } else if (!has_key) {
        // Bootstrap первого ключа. Без этого свежепрошитое устройство
        // недоступно для записи: ключа нет, а сервисный режим включается
        // только запросом с ключом. Разрешаем ровно один раз — пока ключ
        // не создан. Ключ возвращается в ответе, поэтому генерировать его
        // нужно сразу после первого включения, до выхода в общую сеть.
        events_log(EVENT_WARNING, "API key bootstrap: generated without auth");
        ESP_LOGW(TAG, "generating first API key (no key provisioned)");
    }

    char key[API_KEY_LEN];
    esp_err_t err = control_config_api_generate(key, sizeof(key));
    if (err != ESP_OK) return send_err(r, "500 Internal Server Error", "{\"error\":\"nvs\"}");

    char json[96];
    snprintf(json, sizeof(json), "{\"api_key\":\"%s\"}", key);
    return send_json(r, json);
}

// POST /api/config/clear-key — закрыть API полностью (ключ удаляется).
static esp_err_t h_config_clear_key(httpd_req_t *r) {
    if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);
    esp_err_t err = control_config_api_clear();
    if (err != ESP_OK) return send_err(r, "500 Internal Server Error", "{\"error\":\"nvs\"}");
    return send_json(r, "{\"status\":\"ok\",\"api_key_set\":0}");
}

// POST /api/telegram/test — проверить уведомления.
static esp_err_t h_telegram_test(httpd_req_t *r) {
    if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);
    bool queued = telegram_notify_alert("ИНФО", "Проверка уведомлений Automatika");
    char json[128];
    snprintf(json, sizeof(json),
             "{\"queued\":%d,\"configured\":%d,\"sent\":%u,\"failed\":%u}",
             queued ? 1 : 0, telegram_is_configured() ? 1 : 0,
             (unsigned)telegram_notify_sent(), (unsigned)telegram_notify_failed());
    return send_json(r, json);
}

// POST /api/script/enable {"enabled":true}
static esp_err_t h_script_enable(httpd_req_t *r) {
    if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);

    char body[64];
    int n = recv_body(r, body, sizeof(body));
    bool on = (n > 0) && (strstr(body, "true") || strstr(body, "1"));

    if (on && !control_config_service_enabled())
        return send_err(r, "409 Conflict",
                        "{\"error\":\"enable service mode first\"}");

    lua_engine_set_enabled(on);
    char json[64];
    snprintf(json, sizeof(json), "{\"lua\":%d}", on ? 1 : 0);
    return send_json(r, json);
}

// POST /api/ota/url {"url":"https://..."} — managed OTA с автоподтверждением.
static esp_err_t h_ota_url(httpd_req_t *r) {
    if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);

    char body[224];
    if (recv_body(r, body, sizeof(body)) <= 0)
        return send_err(r, "400 Bad Request", "{\"error\":\"empty body\"}");

    cJSON *root = cJSON_Parse(body);
    if (!root) return send_err(r, "400 Bad Request", "{\"error\":\"bad json\"}");
    const char *url = json_string(root, "url");
    char url_copy[192] = "";
    if (url && strlen(url) >= sizeof(url_copy)) {
        cJSON_Delete(root);
        return send_err(r, "400 Bad Request", "{\"error\":\"url too long\"}");
    }
    if (url) {
        strncpy(url_copy, url, sizeof(url_copy) - 1);
    }
    cJSON_Delete(root);

    if (!url_copy[0]) return send_err(r, "400 Bad Request", "{\"error\":\"url required\"}");
    if (ota_guard_check() != ESP_OK) {
        char out[128];
        snprintf(out, sizeof(out), "{\"error\":\"%s\"}", ota_guard_reason());
        return send_err(r, "409 Conflict", out);
    }

    esp_err_t err = ota_manager_start_update(url_copy);
    if (err == ESP_ERR_INVALID_STATE)
        return send_err(r, "409 Conflict", "{\"error\":\"update in progress\"}");
    if (err != ESP_OK)
        return send_err(r, "400 Bad Request", "{\"error\":\"start failed\"}");

    return send_json(r, "{\"status\":\"started\"}");
}

// POST /api/ota/confirm — подтвердить текущий образ вручную.
// POST /api/ota/secure {"url":"https://..."} — OTA только по HTTPS с
// проверкой сертификата (esp_crt_bundle). Загрузка идёт в отдельной задаче.
static esp_err_t h_ota_secure(httpd_req_t *r) {
    if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);

    char body[224];
    if (recv_body(r, body, sizeof(body)) <= 0)
        return send_err(r, "400 Bad Request", "{\"error\":\"empty body\"}");

    cJSON *root = cJSON_Parse(body);
    if (!root) return send_err(r, "400 Bad Request", "{\"error\":\"bad json\"}");
    const char *url = json_string(root, "url");
    char url_copy[192] = "";
    if (url && strlen(url) >= sizeof(url_copy)) {
        cJSON_Delete(root);
        return send_err(r, "400 Bad Request", "{\"error\":\"url too long\"}");
    }
    if (url) strncpy(url_copy, url, sizeof(url_copy) - 1);
    cJSON_Delete(root);

    if (!url_copy[0]) return send_err(r, "400 Bad Request", "{\"error\":\"url required\"}");
    if (ota_guard_check() != ESP_OK) {
        char out[128];
        snprintf(out, sizeof(out), "{\"error\":\"%s\"}", ota_guard_reason());
        return send_err(r, "409 Conflict", out);
    }

    esp_err_t err = secure_ota_start(url_copy);
    if (err == ESP_ERR_INVALID_STATE)
        return send_err(r, "409 Conflict", "{\"error\":\"update in progress\"}");
    if (err == ESP_ERR_INVALID_ARG)
        return send_err(r, "400 Bad Request", "{\"error\":\"https:// required\"}");
    if (err != ESP_OK)
        return send_err(r, "500 Internal Server Error", "{\"error\":\"start failed\"}");

    httpd_resp_set_status(r, "202 Accepted");
    return send_json(r, "{\"status\":\"started\",\"mode\":\"https\"}");
}

static esp_err_t h_ota_confirm(httpd_req_t *r) {
    if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);
    esp_err_t err = ota_manager_confirm_running_app();
    if (err != ESP_OK) return send_err(r, "409 Conflict", "{\"error\":\"confirm\"}");
    return send_json(r, ota_manager_status_json());
}

// ============================================================
// WebSocket
// ============================================================
static esp_err_t h_ws_token(httpd_req_t *r) {
    if (api_check_read_auth(r) != ESP_OK) return api_reject_unauthorized(r);

    char token[33];
    esp_err_t err = api_auth_ws_token_issue(token, sizeof(token));
    if (err != ESP_OK)
        return send_err(r, "500 Internal Server Error", "{\"error\":\"token\"}");

    char out[96];
    snprintf(out, sizeof(out), "{\"token\":\"%s\",\"ttl_sec\":60}", token);
    return send_json(r, out);
}

static esp_err_t h_ws(httpd_req_t *r) {
    if (r->method == HTTP_GET) {
        if (!api_auth_ws_handshake_ok(r))
            return send_err(r, "401 Unauthorized",
                            "{\"error\":\"ws unauthorized\"}");
        return ESP_OK;
    }

    httpd_ws_frame_t in = { .type = HTTPD_WS_TYPE_TEXT };
    uint8_t inbuf[32];
    in.payload = inbuf;
    in.len = sizeof(inbuf);
    if (httpd_ws_recv_frame(r, &in, sizeof(inbuf)) != ESP_OK)
        return ESP_OK;   /* кадр не нужен: отвечаем состоянием */

    char buf[224];
    process_state_t *s = state_get();
    snprintf(buf, sizeof(buf),
        "{\"cube\":%.2f,\"col\":%.2f,\"pwr\":%.1f,\"stg\":%d,"
        "\"run\":%d,\"safe\":%d,\"estop\":%d,\"slope\":%.2f}",
        s->cube_temp, s->column_temp, s->power_percent, (int)s->stage,
        s->is_running ? 1 : 0, ctrl_safety_ok() ? 1 : 0,
        estop_is_pressed() ? 1 : 0, hal_1wire_slope(0));

    httpd_ws_frame_t f = { .type = HTTPD_WS_TYPE_TEXT };
    f.payload = (uint8_t*)buf;
    f.len = strlen(buf);
    return httpd_ws_send_frame(r, &f);
}

// ============================================================
// profiles / autotune / session / i18n / backup / environment
// ============================================================
static esp_err_t h_profiles(httpd_req_t *r) {
    if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);

    if (r->method == HTTP_GET) {
        char qs[96];
        char name[PROFILES_MAX_NAME];
        if (httpd_req_get_url_query_str(r, qs, sizeof(qs)) == ESP_OK &&
            httpd_query_key_value(qs, "name", name, sizeof(name)) == ESP_OK) {
            char buf[PROFILES_MAX_BYTES + 1];
            esp_err_t err = profiles_load(name, buf, sizeof(buf));
            if (err != ESP_OK)
                return send_err(r, "404 Not Found", "{\"error\":\"not found\"}");
            return send_json(r, buf);
        }

        char names[PROFILES_MAX_COUNT][PROFILES_MAX_NAME];
        int n = profiles_list(names, PROFILES_MAX_COUNT);

        cJSON *arr = cJSON_CreateArray();
        if (!arr) return send_err(r, "500 Internal Server Error", "{\"error\":\"oom\"}");
        for (int i = 0; i < n; i++)
            cJSON_AddItemToArray(arr, cJSON_CreateString(names[i]));

        char *js = cJSON_PrintUnformatted(arr);
        cJSON_Delete(arr);
        if (!js) return send_err(r, "500 Internal Server Error", "{\"error\":\"oom\"}");
        esp_err_t err = send_json(r, js);
        cJSON_free(js);
        return err;
    }

    char body[PROFILES_MAX_BYTES + 256];
    int n = recv_body(r, body, sizeof(body));
    if (n <= 0)
        return send_err(r, "400 Bad Request", "{\"error\":\"empty body\"}");

    cJSON *root = cJSON_Parse(body);
    if (!root)
        return send_err(r, "400 Bad Request", "{\"error\":\"bad json\"}");

    const char *name = json_string(root, "name");
    esp_err_t err = ESP_OK;
    const char *msg = "{\"ok\":true}";

    if (r->method == HTTP_DELETE) {
        err = profiles_delete(name);
    } else {
        const cJSON *profile = cJSON_GetObjectItemCaseSensitive(root, "profile");
        if (!cJSON_IsObject(profile)) {
            cJSON_Delete(root);
            return send_err(r, "400 Bad Request", "{\"error\":\"profile object required\"}");
        }
        char *js = cJSON_PrintUnformatted(profile);
        if (!js) {
            cJSON_Delete(root);
            return send_err(r, "500 Internal Server Error", "{\"error\":\"oom\"}");
        }
        err = profiles_save(name, js);
        cJSON_free(js);
    }
    cJSON_Delete(root);

    if (err == ESP_ERR_INVALID_ARG || err == ESP_ERR_INVALID_SIZE)
        return send_err(r, "400 Bad Request", "{\"error\":\"invalid name or size\"}");
    if (err == ESP_ERR_NOT_FOUND)
        return send_err(r, "404 Not Found", "{\"error\":\"not found\"}");
    if (err != ESP_OK)
        return send_err(r, "500 Internal Server Error", "{\"error\":\"storage\"}");

    return send_json(r, msg);
}

static esp_err_t h_autotune(httpd_req_t *r) {
    if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);

    if (r->method == HTTP_GET) {
        process_state_t *st = state_get();
        char json[192];
        snprintf(json, sizeof(json),
                 "{\"running\":%d,\"done\":%d,\"kp\":%.3f,\"ki\":%.4f,\"kd\":%.3f}",
                 pid_autotune_is_running() ? 1 : 0,
                 pid_autotune_done() ? 1 : 0,
                 st->pid_kp, st->pid_ki, st->pid_kd);
        return send_json(r, json);
    }

    char body[192];
    int n = recv_body(r, body, sizeof(body));
    cJSON *root = (n > 0) ? cJSON_Parse(body) : NULL;
    const char *action = root ? json_string(root, "action") : NULL;

    esp_err_t err;
    if (action && strcmp(action, "stop") == 0) {
        pid_autotune_stop();
        err = ESP_OK;
    } else {
        const cJSON *t = root ? cJSON_GetObjectItemCaseSensitive(root, "target") : NULL;
        float target = cJSON_IsNumber(t) ? (float)t->valuedouble
                                         : state_get()->target_cube_t;
        err = pid_autotune_start(target);
    }
    if (root) cJSON_Delete(root);

    if (err == ESP_ERR_INVALID_STATE)
        return send_err(r, "409 Conflict",
                        "{\"error\":\"unavailable (safety/PID mode/busy)\"}");
    if (err != ESP_OK)
        return send_err(r, "400 Bad Request", "{\"error\":\"invalid target\"}");

    return send_json(r, "{\"ok\":true}");
}

static esp_err_t h_session(httpd_req_t *r) {
    if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);

    if (r->method == HTTP_GET) {
        session_snapshot_t s;
        if (!session_load(&s)) {
            return send_json(r, "{\"valid\":false}");
        }
        char json[256];
        snprintf(json, sizeof(json),
                 "{\"valid\":true,\"setpoint\":%.1f,\"volume_ml\":%.0f,"
                 "\"heads_ml\":%.0f,\"body_ml\":%.0f,\"tails_ml\":%.0f,"
                 "\"mode\":%d,\"stage\":%d,\"saved_at\":%lld}",
                 s.setpoint_cube, s.volume_done_ml, s.heads_ml, s.body_ml,
                 s.tails_ml, (int)s.mode, (int)s.stage,
                 (long long)s.saved_at);
        return send_json(r, json);
    }

    char body[96];
    int n = recv_body(r, body, sizeof(body));
    cJSON *root = (n > 0) ? cJSON_Parse(body) : NULL;
    const char *action = root ? json_string(root, "action") : NULL;
    esp_err_t err = ESP_ERR_INVALID_ARG;

    if (action && strcmp(action, "clear") == 0) {
        session_clear();
        state_clear();
        err = ESP_OK;
    } else if (action && strcmp(action, "restore") == 0) {
        session_snapshot_t s;
        if (!session_load(&s)) {
            if (root) cJSON_Delete(root);
            return send_err(r, "404 Not Found", "{\"error\":\"no snapshot\"}");
        }
        process_state_t *st = state_get();
        st->target_cube_t = s.setpoint_cube;
        st->target_column_t = s.setpoint_column;
        st->heads_collected_ml = s.heads_ml;
        st->body_collected_ml = s.body_ml;
        st->tails_collected_ml = s.tails_ml;
        st->type = (process_type_t)s.mode;
        st->stage = (proc_stage_t)s.stage;
        /* Нагрев НЕ включается: сначала проверки безопасности и /api/start. */
        st->is_running = false;
        st->is_paused = true;
        err = ESP_OK;
    }

    if (root) cJSON_Delete(root);

    if (err != ESP_OK)
        return send_err(r, "400 Bad Request", "{\"error\":\"action clear|restore\"}");
    return send_json(r, "{\"ok\":true}");
}

static esp_err_t h_i18n(httpd_req_t *r) {
    if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);

    if (r->method == HTTP_GET) {
        char json[48];
        snprintf(json, sizeof(json), "{\"lang\":\"%s\"}",
                 i18n_lang_name(i18n_get_lang()));
        return send_json(r, json);
    }

    char body[64];
    int n = recv_body(r, body, sizeof(body));
    if (n <= 0) return send_err(r, "400 Bad Request", "{\"error\":\"empty\"}");

    cJSON *root = cJSON_Parse(body);
    const char *lang = root ? json_string(root, "lang") : NULL;
    esp_err_t err = ESP_ERR_INVALID_ARG;
    if (lang) {
        if (strcmp(lang, "en") == 0) { i18n_set_lang(LANG_EN); err = ESP_OK; }
        else if (strcmp(lang, "ru") == 0) { i18n_set_lang(LANG_RU); err = ESP_OK; }
    }
    if (root) cJSON_Delete(root);

    if (err != ESP_OK)
        return send_err(r, "400 Bad Request", "{\"error\":\"lang ru|en\"}");
    return send_json(r, "{\"ok\":true}");
}

static esp_err_t h_backup_telegram(httpd_req_t *r) {
    if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);
    if (!telegram_is_configured())
        return send_err(r, "409 Conflict", "{\"error\":\"telegram not configured\"}");

    esp_err_t err = tg_backup_request();
    if (err != ESP_OK)
        return send_err(r, "409 Conflict", "{\"error\":\"backup busy\"}");

    httpd_resp_set_status(r, "202 Accepted");
    return send_json(r, "{\"status\":\"queued\"}");
}

static void fsel_state_json(char *out, size_t out_len) {
    fsel_state_t st;
    fsel_get_state(&st);
    snprintf(out, out_len,
             "{\"stage\":%d,\"stage_name\":\"%s\",\"t_fix\":%.2f,"
             "\"open_time\":%.2f,\"close_time\":%.2f,\"period\":%.2f,"
             "\"countdown\":%.2f,\"valve_open\":%d,\"trend\":%.2f,"
             "\"ethanol_ml\":%.1f,\"ethanol_percent\":%.1f,"
             "\"active\":%d,\"enabled\":%d}",
             (int)st.stage, fsel_stage_name(st.stage), st.t_fix,
             st.open_time_sec, st.close_time_sec, st.period_sec,
             st.countdown_sec, st.valve_open ? 1 : 0, st.trend_c_per_min,
             st.ethanol_remaining_ml, st.ethanol_percent,
             st.active ? 1 : 0, fsel_is_enabled() ? 1 : 0);
}

static esp_err_t h_fsel_state(httpd_req_t *r) {
    if (api_check_read_auth(r) != ESP_OK) return api_reject_unauthorized(r);
    char json[512];
    fsel_state_json(json, sizeof(json));
    return send_json(r, json);
}

static void fsel_config_json(char *out, size_t out_len) {
    fsel_config_t c;
    fsel_get_config(&c);
    snprintf(out, out_len,
             "{\"enabled\":%d,\"out_device\":%d,\"transition_mode\":%d,"
             "\"select_mode\":%d,\"sensor\":%d,\"auto_mode_defaults\":%d,"
             "\"algo\":%d,\"trend_pause_c_per_min\":%.2f,"
             "\"trend_decrement_c_per_min\":%.2f,\"trend_window_sec\":%.0f,"
             "\"heads_timer_sec\":%u,\"heads_open_time_sec\":%.2f,"
             "\"heads_close_time_sec\":%.2f,\"heads_period_sec\":%.2f,"
             "\"body_open_time_sec\":%.2f,\"body_close_time_sec\":%.2f,"
             "\"body_period_sec\":%.2f,"
             "\"body_hysteresis_c\":%.2f,\"body_decrement_percent\":%.2f,"
             "\"body_min_open_time_sec\":%.2f,\"body_temp_fix_delay_sec\":%u,"
             "\"tails_temperature_end\":%.2f,\"tails_open_time_sec\":%.2f,"
             "\"tails_close_time_sec\":%.2f,\"tails_period_sec\":%.2f}",
             c.enabled ? 1 : 0, (int)c.out_device, (int)c.transition_mode,
             (int)c.select_mode, (int)c.sensor,
             c.auto_mode_defaults ? 1 : 0,
             (int)c.algo, c.trend_pause_c_per_min,
             c.trend_decrement_c_per_min, c.trend_window_sec,
             (unsigned)c.heads_timer_sec, c.heads_open_time_sec,
             c.heads_close_time_sec,
             c.heads_open_time_sec + c.heads_close_time_sec,
             c.body_open_time_sec, c.body_close_time_sec,
             c.body_open_time_sec + c.body_close_time_sec,
             c.body_hysteresis_c, c.body_decrement_percent,
             c.body_min_open_time_sec, (unsigned)c.body_temp_fix_delay_sec,
             c.tails_temperature_end, c.tails_open_time_sec,
             c.tails_close_time_sec,
             c.tails_open_time_sec + c.tails_close_time_sec);
}

static esp_err_t h_fsel_config(httpd_req_t *r) {
    if (r->method == HTTP_GET) {
        char json[1024];
        fsel_config_json(json, sizeof(json));
        return send_json(r, json);
    }

    if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);

    char body[768];
    int n = recv_body(r, body, sizeof(body));
    if (n <= 0) return send_err(r, "400 Bad Request", "{\"error\":\"empty body\"}");

    cJSON *root = cJSON_Parse(body);
    if (!root) return send_err(r, "400 Bad Request", "{\"error\":\"bad json\"}");

    fsel_config_t cfg;
    fsel_get_config(&cfg);

    const cJSON *v;
    if ((v = cJSON_GetObjectItemCaseSensitive(root, "enabled")) != NULL) {
        if (cJSON_IsBool(v)) cfg.enabled = cJSON_IsTrue(v);
        else if (cJSON_IsNumber(v)) cfg.enabled = v->valueint != 0;
    }
    if (cJSON_IsNumber((v = cJSON_GetObjectItemCaseSensitive(root, "out_device"))))
        cfg.out_device = (fsel_out_device_t)v->valueint;
    if (cJSON_IsNumber((v = cJSON_GetObjectItemCaseSensitive(root, "transition_mode"))))
        cfg.transition_mode = (fsel_transition_t)v->valueint;
    if (cJSON_IsNumber((v = cJSON_GetObjectItemCaseSensitive(root, "select_mode"))))
        cfg.select_mode = (fsel_select_mode_t)v->valueint;
    if (cJSON_IsNumber((v = cJSON_GetObjectItemCaseSensitive(root, "sensor"))))
        cfg.sensor = (fsel_sensor_t)v->valueint;
    if (cJSON_IsNumber((v = cJSON_GetObjectItemCaseSensitive(root, "algo"))))
        cfg.algo = (select_algo_t)v->valueint;
    if (cJSON_IsNumber((v = cJSON_GetObjectItemCaseSensitive(root, "trend_pause_c_per_min"))))
        cfg.trend_pause_c_per_min = (float)v->valuedouble;
    if (cJSON_IsNumber((v = cJSON_GetObjectItemCaseSensitive(root, "trend_decrement_c_per_min"))))
        cfg.trend_decrement_c_per_min = (float)v->valuedouble;
    if (cJSON_IsNumber((v = cJSON_GetObjectItemCaseSensitive(root, "trend_window_sec"))))
        cfg.trend_window_sec = (float)v->valuedouble;

    if (cJSON_IsNumber((v = cJSON_GetObjectItemCaseSensitive(root, "heads_timer_sec"))))
        cfg.heads_timer_sec = (uint32_t)v->valuedouble;
    if (cJSON_IsNumber((v = cJSON_GetObjectItemCaseSensitive(root, "heads_open_time_sec"))))
        cfg.heads_open_time_sec = (float)v->valuedouble;
    if (cJSON_IsNumber((v = cJSON_GetObjectItemCaseSensitive(root, "heads_period_sec")))) {
        float period = (float)v->valuedouble;
        cfg.heads_close_time_sec = period - cfg.heads_open_time_sec;
        if (cfg.heads_close_time_sec < 0.0f) cfg.heads_close_time_sec = 0.0f;
    }
    if (cJSON_IsNumber((v = cJSON_GetObjectItemCaseSensitive(root, "heads_close_time_sec"))))
        cfg.heads_close_time_sec = (float)v->valuedouble;

    if (cJSON_IsNumber((v = cJSON_GetObjectItemCaseSensitive(root, "body_open_time_sec"))))
        cfg.body_open_time_sec = (float)v->valuedouble;
    if (cJSON_IsNumber((v = cJSON_GetObjectItemCaseSensitive(root, "body_period_sec")))) {
        float period = (float)v->valuedouble;
        cfg.body_close_time_sec = period - cfg.body_open_time_sec;
        if (cfg.body_close_time_sec < 0.0f) cfg.body_close_time_sec = 0.0f;
    }
    if (cJSON_IsNumber((v = cJSON_GetObjectItemCaseSensitive(root, "body_close_time_sec"))))
        cfg.body_close_time_sec = (float)v->valuedouble;

    if (cJSON_IsNumber((v = cJSON_GetObjectItemCaseSensitive(root, "body_hysteresis_c"))))
        cfg.body_hysteresis_c = (float)v->valuedouble;
    if (cJSON_IsNumber((v = cJSON_GetObjectItemCaseSensitive(root, "body_decrement_percent"))))
        cfg.body_decrement_percent = (float)v->valuedouble;
    if (cJSON_IsNumber((v = cJSON_GetObjectItemCaseSensitive(root, "body_min_open_time_sec"))))
        cfg.body_min_open_time_sec = (float)v->valuedouble;
    if (cJSON_IsNumber((v = cJSON_GetObjectItemCaseSensitive(root, "body_temp_fix_delay_sec"))))
        cfg.body_temp_fix_delay_sec = (uint32_t)v->valuedouble;
    if (cJSON_IsNumber((v = cJSON_GetObjectItemCaseSensitive(root, "tails_temperature_end"))))
        cfg.tails_temperature_end = (float)v->valuedouble;
    if (cJSON_IsNumber((v = cJSON_GetObjectItemCaseSensitive(root, "tails_open_time_sec"))))
        cfg.tails_open_time_sec = (float)v->valuedouble;
    if (cJSON_IsNumber((v = cJSON_GetObjectItemCaseSensitive(root, "tails_period_sec")))) {
        float period = (float)v->valuedouble;
        cfg.tails_close_time_sec = period - cfg.tails_open_time_sec;
        if (cfg.tails_close_time_sec < 0.0f) cfg.tails_close_time_sec = 0.0f;
    }
    if (cJSON_IsNumber((v = cJSON_GetObjectItemCaseSensitive(root, "tails_close_time_sec"))))
        cfg.tails_close_time_sec = (float)v->valuedouble;

    if ((v = cJSON_GetObjectItemCaseSensitive(root, "auto_mode_defaults")) != NULL) {
        if (cJSON_IsBool(v)) cfg.auto_mode_defaults = cJSON_IsTrue(v);
        else if (cJSON_IsNumber(v)) cfg.auto_mode_defaults = v->valueint != 0;
    }

    cJSON_Delete(root);

    // Мягкая валидация диапазонов: лучше 400, чем конфиг, который
    // блокирует отбор навсегда.
    bool periodic = cfg.select_mode == FSEL_SELECT_PERIODIC;
    if ((periodic &&
         (cfg.heads_open_time_sec + cfg.heads_close_time_sec <= 0.0f ||
          cfg.body_open_time_sec + cfg.body_close_time_sec <= 0.0f ||
          cfg.tails_open_time_sec + cfg.tails_close_time_sec <= 0.0f)) ||
        cfg.heads_open_time_sec < 0.0f || cfg.body_open_time_sec < 0.0f ||
        cfg.tails_open_time_sec < 0.0f ||
        cfg.heads_close_time_sec < 0.0f || cfg.body_close_time_sec < 0.0f ||
        cfg.tails_close_time_sec < 0.0f ||
        cfg.body_min_open_time_sec <= 0.0f ||
        cfg.body_decrement_percent < 0.0f || cfg.body_decrement_percent > 100.0f) {
        return send_err(r, "400 Bad Request", "{\"error\":\"invalid range\"}");
    }

    esp_err_t err = fsel_set_config(&cfg);
    if (err == ESP_OK) err = fsel_config_save(&cfg);
    if (err != ESP_OK) return send_err(r, "500 Internal Server Error", "{\"error\":\"nvs\"}");

    return send_json(r, "{\"status\":\"ok\"}");
}

static esp_err_t h_fsel_options(httpd_req_t *r) {
    if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);

    char body[256];
    int n = recv_body(r, body, sizeof(body));
    if (n <= 0) return send_err(r, "400 Bad Request", "{\"error\":\"empty body\"}");

    cJSON *root = cJSON_Parse(body);
    if (!root) return send_err(r, "400 Bad Request", "{\"error\":\"bad json\"}");

    fsel_config_t cfg;
    fsel_get_config(&cfg);

    const cJSON *v;
    if (cJSON_IsNumber((v = cJSON_GetObjectItemCaseSensitive(root, "out_device"))))
        cfg.out_device = (fsel_out_device_t)v->valueint;
    if (cJSON_IsNumber((v = cJSON_GetObjectItemCaseSensitive(root, "transition_mode"))))
        cfg.transition_mode = (fsel_transition_t)v->valueint;
    if (cJSON_IsNumber((v = cJSON_GetObjectItemCaseSensitive(root, "select_mode"))))
        cfg.select_mode = (fsel_select_mode_t)v->valueint;
    if (cJSON_IsNumber((v = cJSON_GetObjectItemCaseSensitive(root, "sensor"))))
        cfg.sensor = (fsel_sensor_t)v->valueint;
    if (cJSON_IsNumber((v = cJSON_GetObjectItemCaseSensitive(root, "algo"))))
        cfg.algo = (select_algo_t)v->valueint;
    if (cJSON_IsNumber((v = cJSON_GetObjectItemCaseSensitive(root, "trend_pause_c_per_min"))))
        cfg.trend_pause_c_per_min = (float)v->valuedouble;
    if (cJSON_IsNumber((v = cJSON_GetObjectItemCaseSensitive(root, "trend_decrement_c_per_min"))))
        cfg.trend_decrement_c_per_min = (float)v->valuedouble;
    if (cJSON_IsNumber((v = cJSON_GetObjectItemCaseSensitive(root, "trend_window_sec"))))
        cfg.trend_window_sec = (float)v->valuedouble;
    if (cJSON_IsBool((v = cJSON_GetObjectItemCaseSensitive(root, "enabled"))))
        cfg.enabled = cJSON_IsTrue(v);
    if ((v = cJSON_GetObjectItemCaseSensitive(root, "auto_mode_defaults")) != NULL) {
        if (cJSON_IsBool(v)) cfg.auto_mode_defaults = cJSON_IsTrue(v);
        else if (cJSON_IsNumber(v)) cfg.auto_mode_defaults = v->valueint != 0;
    }
    cJSON_Delete(root);

    esp_err_t err = fsel_set_config(&cfg);
    if (err == ESP_OK) err = fsel_config_save(&cfg);
    if (err != ESP_OK) return send_err(r, "500 Internal Server Error", "{\"error\":\"nvs\"}");

    char json[320];
    fsel_state_json(json, sizeof(json));
    return send_json(r, json);
}

static esp_err_t h_fsel_next(httpd_req_t *r) {
    if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);
    fsel_force_next_stage();
    char json[512];
    fsel_state_json(json, sizeof(json));
    return send_json(r, json);
}

// Точечное редактирование параметра из PWA/LCD/Telegram:
// {"name":"open_time_sec","value":1.5}
static esp_err_t h_fsel_param(httpd_req_t *r) {
    if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);

    char body[192];
    int n = recv_body(r, body, sizeof(body));
    if (n <= 0) return send_err(r, "400 Bad Request", "{\"error\":\"empty body\"}");

    cJSON *root = cJSON_Parse(body);
    if (!root) return send_err(r, "400 Bad Request", "{\"error\":\"bad json\"}");

    const cJSON *name = cJSON_GetObjectItemCaseSensitive(root, "name");
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(root, "value");
    if (!cJSON_IsString(name) || !name->valuestring ||
        !cJSON_IsNumber(value)) {
        cJSON_Delete(root);
        return send_err(r, "400 Bad Request",
                        "{\"error\":\"name and value required\"}");
    }

    esp_err_t err = fsel_set_param(name->valuestring, (float)value->valuedouble);
    cJSON_Delete(root);

    if (err == ESP_ERR_INVALID_ARG)
        return send_err(r, "400 Bad Request", "{\"error\":\"unknown parameter\"}");
    if (err != ESP_OK)
        return send_err(r, "500 Internal Server Error", "{\"error\":\"nvs\"}");

    char json[512];
    fsel_state_json(json, sizeof(json));
    return send_json(r, json);
}

static void abv_state_json(char *out, size_t out_len) {
    snprintf(out, out_len,
             "{\"volume_start_ml\":%.0f,\"abv_start_percent\":%.1f,"
             "\"remaining_ml\":%.1f,\"remaining_percent\":%.1f,"
             "\"current_abv_percent\":%.1f,\"boiling_c\":%.1f,"
             "\"ethanol_g\":%.1f}",
             abv_get_start_volume_ml(), abv_get_start_abv_percent(),
             abv_get_remaining_ml(), abv_get_remaining_percent(),
             abv_get_current_abv_percent(), abv_get_boiling_point_c(),
             abv_get_ethanol_g());
}

static esp_err_t h_alcohol(httpd_req_t *r) {
    if (r->method == HTTP_GET) {
        char json[320];
        abv_state_json(json, sizeof(json));
        return send_json(r, json);
    }

    if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);

    char body[192];
    int n = recv_body(r, body, sizeof(body));
    if (n <= 0) return send_err(r, "400 Bad Request", "{\"error\":\"empty body\"}");

    cJSON *root = cJSON_Parse(body);
    if (!root) return send_err(r, "400 Bad Request", "{\"error\":\"bad json\"}");

    alcohol_init_t init = {
        .volume_start_ml = abv_get_start_volume_ml(),
        .abv_start_percent = abv_get_start_abv_percent(),
        .ethanol_density = ABV_ETHANOL_DENSITY,
    };

    const cJSON *v;
    if (cJSON_IsNumber((v = cJSON_GetObjectItemCaseSensitive(root, "volume_ml"))))
        init.volume_start_ml = (float)v->valuedouble;
    if (cJSON_IsNumber((v = cJSON_GetObjectItemCaseSensitive(root, "abv_percent"))))
        init.abv_start_percent = (float)v->valuedouble;
    cJSON_Delete(root);

    if (init.volume_start_ml <= 0.0f || init.volume_start_ml > 200000.0f ||
        init.abv_start_percent < 0.0f || init.abv_start_percent > 100.0f) {
        return send_err(r, "400 Bad Request", "{\"error\":\"invalid range\"}");
    }

    esp_err_t err = abv_config_save(&init);
    if (err != ESP_OK)
        return send_err(r, "500 Internal Server Error", "{\"error\":\"nvs\"}");

    abv_init(&init);

    char json[320];
    abv_state_json(json, sizeof(json));
    return send_json(r, json);
}

static void session_state_json(char *out, size_t out_len) {
    session_state_t state = session_get_state();
    snprintf(out, out_len,
             "{\"state\":%d,\"state_name\":\"%s\","
             "\"running\":%d,\"visible\":%d,\"paused\":%d}",
             (int)state, session_state_name(state),
             session_is_running() ? 1 : 0,
             session_is_visible() ? 1 : 0,
             session_is_paused() ? 1 : 0);
}

static esp_err_t h_session_state(httpd_req_t *r) {
    if (api_check_read_auth(r) != ESP_OK) return api_reject_unauthorized(r);
    char json[192];
    session_state_json(json, sizeof(json));
    return send_json(r, json);
}

static esp_err_t h_session_minimize(httpd_req_t *r) {
    if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);
    session_minimize();
    char json[192];
    session_state_json(json, sizeof(json));
    return send_json(r, json);
}

static esp_err_t h_session_restore(httpd_req_t *r) {
    if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);
    session_restore();
    char json[192];
    session_state_json(json, sizeof(json));
    return send_json(r, json);
}

static esp_err_t h_session_stop(httpd_req_t *r) {
    if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);

    char body[96];
    int n = recv_body(r, body, sizeof(body));
    bool confirmed = false;
    if (n > 0) {
        cJSON *root = cJSON_Parse(body);
        if (root) {
            const cJSON *c = cJSON_GetObjectItemCaseSensitive(root, "confirm");
            confirmed = cJSON_IsTrue(c);
            cJSON_Delete(root);
        }
    }
    if (!confirmed) {
        return send_err(r, "400 Bad Request",
                        "{\"ok\":false,\"error\":\"confirmation_required\"}");
    }

    session_stop();
    char json[192];
    session_state_json(json, sizeof(json));
    return send_json(r, json);
}

static esp_err_t h_history_processes(httpd_req_t *r) {
    if (api_check_read_auth(r) != ESP_OK) return api_reject_unauthorized(r);

    int limit = 20, offset = 0;
    size_t qlen = httpd_req_get_url_query_len(r);
    if (qlen > 0 && qlen < 64) {
        char q[64];
        if (httpd_req_get_url_query_str(r, q, sizeof(q)) == ESP_OK) {
            char val[16];
            if (httpd_query_key_value(q, "limit", val, sizeof(val)) == ESP_OK) {
                int v = atoi(val);
                if (v > 0 && v <= 50) limit = v;
            }
            if (httpd_query_key_value(q, "offset", val, sizeof(val)) == ESP_OK) {
                int v = atoi(val);
                if (v >= 0) offset = v;
            }
        }
    }

    cJSON *arr = cJSON_CreateArray();
    if (!arr) return send_err(r, "500 Internal Server Error", "{\"error\":\"memory\"}");

    if (db_is_ready()) {
        db_process_t *rows = calloc((size_t)limit, sizeof(*rows));
        if (!rows) {
            cJSON_Delete(arr);
            return send_err(r, "500 Internal Server Error", "{\"error\":\"memory\"}");
        }
        int n = db_process_list(rows, limit, offset);
        for (int i = 0; i < n; i++) {
            const db_process_t *p = &rows[i];
            cJSON *o = cJSON_CreateObject();
            cJSON_AddNumberToObject(o, "id", (double)p->id);
            cJSON_AddNumberToObject(o, "type", p->type);
            cJSON_AddNumberToObject(o, "started_ms", (double)p->started_ms);
            cJSON_AddNumberToObject(o, "ended_ms", (double)p->ended_ms);
            cJSON_AddNumberToObject(o, "duration_sec", p->duration_sec);
            cJSON_AddNumberToObject(o, "heads_ml", p->heads_ml);
            cJSON_AddNumberToObject(o, "body_ml", p->body_ml);
            cJSON_AddNumberToObject(o, "tails_ml", p->tails_ml);
            cJSON_AddNumberToObject(o, "rating", p->rating);
            cJSON_AddStringToObject(o, "recipe_id", p->recipe_id);
            cJSON_AddItemToArray(arr, o);
        }
        free(rows);
    }

    char *json = cJSON_PrintUnformatted(arr);
    cJSON_Delete(arr);
    if (!json) return send_err(r, "500 Internal Server Error", "{\"error\":\"memory\"}");
    esp_err_t err = send_json(r, json);
    free(json);
    return err;
}

static esp_err_t h_ai_status(httpd_req_t *r) {
    if (api_check_read_auth(r) != ESP_OK) return api_reject_unauthorized(r);

    cJSON *root = cJSON_CreateObject();
    if (!root) return send_err(r, "500 Internal Server Error", "{\"error\":\"memory\"}");
    cJSON_AddBoolToObject(root, "db_ready", db_is_ready());

    cJSON *an = cJSON_AddObjectToObject(root, "anomaly");
    const anomaly_state_t *a = anomaly_get();
    if (a) {
        cJSON_AddNumberToObject(an, "error", a->error);
        cJSON_AddNumberToObject(an, "threshold", a->threshold);
        cJSON_AddBoolToObject(an, "is_anomaly", a->is_anomaly);
        cJSON_AddNumberToObject(an, "since_ms", (double)a->since_ms);
        cJSON_AddNumberToObject(an, "severity", a->severity);
    }
    cJSON_AddNumberToObject(an, "samples", (double)anomaly_samples());

    cJSON_AddNumberToObject(root, "action_accuracy", action_predictor_accuracy());
    cJSON_AddNumberToObject(root, "cache_hit_rate", cache_hit_rate());

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json) return send_err(r, "500 Internal Server Error", "{\"error\":\"memory\"}");
    esp_err_t err = send_json(r, json);
    free(json);
    return err;
}

static esp_err_t h_maint_reset(httpd_req_t *r) {
    if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);
    if (!roles_check(ROLE_ENGINEER)) return send_err(r, "403 Forbidden", "{\"error\":\"role\"}");

    char body[96];
    int n = recv_body(r, body, sizeof(body));
    if (n <= 0) return send_err(r, "400 Bad Request", "{\"error\":\"empty body\"}");

    cJSON *root = cJSON_Parse(body);
    if (!root) return send_err(r, "400 Bad Request", "{\"error\":\"bad json\"}");
    const cJSON *t = cJSON_GetObjectItemCaseSensitive(root, "target");
    const char *target = cJSON_IsString(t) ? t->valuestring : NULL;
    if (!target) {
        cJSON_Delete(root);
        return send_err(r, "400 Bad Request", "{\"error\":\"target required\"}");
    }

    if (strcmp(target, "ai") == 0) {
        predictor_reset();
        action_predictor_reset();
        anomaly_reset_model();
    } else if (strcmp(target, "autolearn") == 0) {
        autolearn_reset();
    } else if (strcmp(target, "tinyml") == 0) {
        tiny_ml_reset();
    } else {
        cJSON_Delete(root);
        return send_err(r, "400 Bad Request", "{\"error\":\"unknown target\"}");
    }
    events_log(EVENT_USER_ACTION, "Web: reset %s", target);
    cJSON_Delete(root);
    return send_json(r, "{\"ok\":true}");
}

static esp_err_t h_maint_db(httpd_req_t *r) {
    if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);
    if (!roles_check(ROLE_ENGINEER)) return send_err(r, "403 Forbidden", "{\"error\":\"role\"}");

    char body[96];
    int n = recv_body(r, body, sizeof(body));
    if (n <= 0) return send_err(r, "400 Bad Request", "{\"error\":\"empty body\"}");

    cJSON *root = cJSON_Parse(body);
    if (!root) return send_err(r, "400 Bad Request", "{\"error\":\"bad json\"}");
    const cJSON *o = cJSON_GetObjectItemCaseSensitive(root, "op");
    const char *op = cJSON_IsString(o) ? o->valuestring : NULL;
    if (!op) {
        cJSON_Delete(root);
        return send_err(r, "400 Bad Request", "{\"error\":\"op required\"}");
    }
    if (!db_is_ready()) {
        cJSON_Delete(root);
        return send_err(r, "409 Conflict", "{\"error\":\"db not ready\"}");
    }

    if (strcmp(op, "backup") == 0 || strcmp(op, "vacuum") == 0) {
        bool ok = strcmp(op, "backup") == 0 ? db_backup("/sdcard/backups/db_auto.db") : db_vacuum();
        events_log(EVENT_USER_ACTION, "Web: db %s %s", op, ok ? "ok" : "failed");
        cJSON_Delete(root);
        return ok ? send_json(r, "{\"ok\":true}")
                  : send_err(r, "500 Internal Server Error", "{\"error\":\"db op failed\"}");
    }
    if (strcmp(op, "export") == 0) {
        bool ok = db_export_all_csv("/sdcard");
        events_log(EVENT_USER_ACTION, "Web: db export %s", ok ? "ok" : "failed");
        cJSON_Delete(root);
        return ok ? send_json(r, "{\"ok\":true,\"file\":\"/sdcard/processes.csv\"}")
                  : send_err(r, "500 Internal Server Error", "{\"error\":\"export failed\"}");
    }
    if (strcmp(op, "stats") == 0) {
        char text[256];
        db_stats(text, sizeof(text));
        cJSON_Delete(root);
        cJSON *out = cJSON_CreateObject();
        if (!out) return send_err(r, "500 Internal Server Error", "{\"error\":\"memory\"}");
        cJSON_AddStringToObject(out, "text", text);
        char *json = cJSON_PrintUnformatted(out);
        cJSON_Delete(out);
        if (!json) return send_err(r, "500 Internal Server Error", "{\"error\":\"memory\"}");
        esp_err_t err = send_json(r, json);
        free(json);
        return err;
    }

    cJSON_Delete(root);
    return send_err(r, "400 Bad Request", "{\"error\":\"unknown op\"}");
}

static esp_err_t h_calib_state(httpd_req_t *r) {
    if (api_check_read_auth(r) != ESP_OK) return api_reject_unauthorized(r);

    cJSON *root = cJSON_CreateObject();
    if (!root) return send_err(r, "500 Internal Server Error", "{\"error\":\"memory\"}");

    cJSON *sens = cJSON_AddObjectToObject(root, "sensors");
    const sensor_calib_t *sc = sensor_calib_get();
    if (sens && sc) {
        cJSON_AddNumberToObject(sens, "state", (int)sc->state);
        cJSON_AddBoolToObject(sens, "saved", sc->saved);
        cJSON_AddNumberToObject(sens, "reference", sc->reference_temp);
        cJSON_AddStringToObject(sens, "error", sc->error);
        cJSON *offs = cJSON_AddArrayToObject(sens, "offsets");
        int n = hal_1wire_count();
        if (n > MAX_TEMP_SENSORS) n = MAX_TEMP_SENSORS;
        for (int i = 0; offs && i < n; i++) {
            cJSON_AddItemToArray(offs, cJSON_CreateNumber(sc->offset[i]));
        }
    }

    cJSON *sg = cJSON_AddObjectToObject(root, "scale");
    const scale_t *scale = scale_get();
    if (sg && scale) {
        cJSON_AddNumberToObject(sg, "state", (int)scale->state);
        cJSON_AddBoolToObject(sg, "ready", scale_is_ready());
        cJSON_AddBoolToObject(sg, "stable", scale->stable);
        cJSON_AddNumberToObject(sg, "grams", scale->grams_filtered);
        cJSON_AddNumberToObject(sg, "total_ml", scale->total_ml);
        cJSON_AddNumberToObject(sg, "flow_ml_min", scale->flow_ml_per_min);
        cJSON_AddNumberToObject(sg, "density", scale->density);
        cJSON_AddBoolToObject(sg, "auto_tare", scale->auto_tare);
    }

    cJSON *valves = cJSON_AddArrayToObject(root, "valves");
    for (int i = 0; valves && i < VALVE_COUNT; i++) {
        const valve_calib_session_t *v = valve_calib_get(i);
        if (!v) continue;
        cJSON *o = cJSON_CreateObject();
        if (!o) continue;
        cJSON_AddNumberToObject(o, "idx", i);
        cJSON_AddBoolToObject(o, "active", v->active);
        cJSON_AddNumberToObject(o, "open_count", (double)v->open_count);
        cJSON_AddNumberToObject(o, "total_open_ms", (double)v->total_open_ms);
        cJSON_AddNumberToObject(o, "measured_ml", v->measured_ml);
        cJSON_AddNumberToObject(o, "ml_per_hour", v->ml_per_hour);
        cJSON_AddNumberToObject(o, "ml_per_open", v->ml_per_open);
        cJSON_AddItemToArray(valves, o);
    }

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json) return send_err(r, "500 Internal Server Error", "{\"error\":\"memory\"}");
    esp_err_t err = send_json(r, json);
    free(json);
    return err;
}

static esp_err_t h_usb_status(httpd_req_t *r) {
    if (api_check_read_auth(r) != ESP_OK) return api_reject_unauthorized(r);
    char json[48];
    snprintf(json, sizeof(json), "{\"mounted\":%d}", hal_usb_is_mounted() ? 1 : 0);
    return send_json(r, json);
}

static esp_err_t h_calib_scale_tare(httpd_req_t *r) {
    if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);
    if (!roles_check(ROLE_ENGINEER)) return send_err(r, "403 Forbidden", "{\"error\":\"role\"}");
    /* Тара сбрасывает накопленный объём отбора: во время процесса запрещена. */
    if (state_get()->is_running) return send_err(r, "409 Conflict", "{\"error\":\"process running\"}");
    scale_tare_now();
    return send_json(r, "{\"ok\":true}");
}

static esp_err_t h_calib_sensors_abort(httpd_req_t *r) {
    if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);
    if (!roles_check(ROLE_ENGINEER)) return send_err(r, "403 Forbidden", "{\"error\":\"role\"}");
    sensor_calib_abort();
    return send_json(r, "{\"ok\":true}");
}

static esp_err_t h_recipes_list(httpd_req_t *r) {
    if (api_check_read_auth(r) != ESP_OK) return api_reject_unauthorized(r);
    cJSON *arr = cJSON_CreateArray();
    if (!arr) return send_err(r, "500 Internal Server Error", "{\"error\":\"memory\"}");
    for (size_t i = 0; i < RECIPES_COUNT; i++) {
        const recipe_t *rc = &RECIPES[i];
        cJSON *o = cJSON_CreateObject();
        if (!o) continue;
        cJSON_AddStringToObject(o, "id", rc->id);
        cJSON_AddStringToObject(o, "name", rc->name);
        cJSON_AddStringToObject(o, "category", rc->category);
        cJSON_AddStringToObject(o, "description", rc->description);
        cJSON_AddItemToArray(arr, o);
    }
    char *json = cJSON_PrintUnformatted(arr);
    cJSON_Delete(arr);
    if (!json) return send_err(r, "500 Internal Server Error", "{\"error\":\"memory\"}");
    esp_err_t err = send_json(r, json);
    free(json);
    return err;
}

static esp_err_t h_recipe_get(httpd_req_t *r) {
    if (api_check_read_auth(r) != ESP_OK) return api_reject_unauthorized(r);
    char id[48] = "";
    size_t qlen = httpd_req_get_url_query_len(r);
    if (qlen == 0 || qlen >= 64) return send_err(r, "400 Bad Request", "{\"error\":\"id required\"}");
    char q[64];
    if (httpd_req_get_url_query_str(r, q, sizeof(q)) != ESP_OK ||
        httpd_query_key_value(q, "id", id, sizeof(id)) != ESP_OK) {
        return send_err(r, "400 Bad Request", "{\"error\":\"id required\"}");
    }
    const recipe_t *rc = recipes_find(id);
    if (!rc) return send_err(r, "404 Not Found", "{\"error\":\"unknown recipe\"}");

    cJSON *o = cJSON_CreateObject();
    if (!o) return send_err(r, "500 Internal Server Error", "{\"error\":\"memory\"}");
    cJSON_AddStringToObject(o, "id", rc->id);
    cJSON_AddStringToObject(o, "name", rc->name);
    cJSON_AddStringToObject(o, "category", rc->category);
    cJSON_AddStringToObject(o, "description", rc->description);
    cJSON_AddStringToObject(o, "ingredients", rc->ingredients);
    cJSON_AddStringToObject(o, "steps_md", rc->steps_md);
    char *json = cJSON_PrintUnformatted(o);
    cJSON_Delete(o);
    if (!json) return send_err(r, "500 Internal Server Error", "{\"error\":\"memory\"}");
    esp_err_t err = send_json(r, json);
    free(json);
    return err;
}

static esp_err_t h_menu_toggles(httpd_req_t *r) {
    if (api_check_read_auth(r) != ESP_OK) return api_reject_unauthorized(r);

    const menu_item_t *items[64];
    int n = menu_engine_list_toggles(items, 64);
    cJSON *arr = cJSON_CreateArray();
    if (!arr) return send_err(r, "500 Internal Server Error", "{\"error\":\"memory\"}");
    for (int i = 0; i < n; i++) {
        const menu_item_t *it = items[i];
        uint8_t v = 0;
        menu_engine_toggle_get(it->nvs_key, &v);
        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "key", it->nvs_key);
        cJSON_AddStringToObject(o, "title", it->title_ru ? it->title_ru : "");
        cJSON_AddNumberToObject(o, "value", v);
        cJSON_AddNumberToObject(o, "role", it->required_role);
        cJSON_AddBoolToObject(o, "editable", roles_check(it->required_role));
        cJSON_AddItemToArray(arr, o);
    }
    char *json = cJSON_PrintUnformatted(arr);
    cJSON_Delete(arr);
    if (!json) return send_err(r, "500 Internal Server Error", "{\"error\":\"memory\"}");
    esp_err_t err = send_json(r, json);
    free(json);
    return err;
}

static esp_err_t h_menu_toggle(httpd_req_t *r) {
    if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);

    char body[160];
    int n = recv_body(r, body, sizeof(body));
    if (n <= 0) return send_err(r, "400 Bad Request", "{\"error\":\"empty body\"}");

    cJSON *root = cJSON_Parse(body);
    if (!root) return send_err(r, "400 Bad Request", "{\"error\":\"bad json\"}");

    const cJSON *key = cJSON_GetObjectItemCaseSensitive(root, "key");
    const cJSON *val = cJSON_GetObjectItemCaseSensitive(root, "value");
    if (!cJSON_IsString(key) || !key->valuestring || !cJSON_IsNumber(val) ||
        (val->valuedouble != 0.0 && val->valuedouble != 1.0)) {
        cJSON_Delete(root);
        return send_err(r, "400 Bad Request", "{\"error\":\"key and value 0|1 required\"}");
    }

    const menu_item_t *it = menu_engine_find_toggle(key->valuestring);
    if (!it) {
        cJSON_Delete(root);
        return send_err(r, "400 Bad Request", "{\"error\":\"unknown toggle\"}");
    }
    if (!roles_check(it->required_role)) {
        cJSON_Delete(root);
        return send_err(r, "403 Forbidden", "{\"error\":\"role\"}");
    }

    bool ok = menu_engine_toggle_set(it->nvs_key, val->valuedouble != 0.0 ? 1 : 0);
    cJSON_Delete(root);
    if (!ok) return send_err(r, "500 Internal Server Error", "{\"error\":\"nvs write\"}");
    return send_json(r, "{\"ok\":true}");
}

static esp_err_t h_param_get(httpd_req_t *r) {
    if (api_check_read_auth(r) != ESP_OK) return api_reject_unauthorized(r);
    char json[2048];
    int off = snprintf(json, sizeof(json), "{\"running\":%d,\"params\":[",
                       session_is_running() ? 1 : 0);

    for (int i = 0; i < param_count() && off > 0 &&
                    off < (int)sizeof(json); i++) {
        const param_def_t *p = param_get_at(i);
        if (!p) continue;
        bool locked = param_is_locked((param_id_t)i);
        off += snprintf(json + off, sizeof(json) - (size_t)off,
                        "%s{\"id\":\"%s\",\"label\":\"%s\",\"unit\":\"%s\","
                        "\"value\":%.2f,\"min\":%.2f,\"max\":%.2f,\"step\":%.2f,"
                        "\"editable\":%d,\"editable_running\":%d}",
                        i ? "," : "", p->id, p->label, p->unit,
                        (double)param_read((param_id_t)i),
                        (double)p->min, (double)p->max, (double)p->step,
                        locked ? 0 : 1, p->editable_running ? 1 : 0);
    }

    if (off > 0 && off < (int)sizeof(json) - 3) {
        snprintf(json + off, sizeof(json) - (size_t)off, "]}");
        return send_json(r, json);
    }
    return send_err(r, "500 Internal Server Error", "{\"error\":\"json\"}");
}

static esp_err_t h_param_post(httpd_req_t *r) {
    if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);

    char body[192];
    int n = recv_body(r, body, sizeof(body));
    if (n <= 0)
        return send_err(r, "400 Bad Request", "{\"error\":\"empty body\"}");

    cJSON *root = cJSON_Parse(body);
    if (!root)
        return send_err(r, "400 Bad Request", "{\"error\":\"bad json\"}");

    const cJSON *name = cJSON_GetObjectItemCaseSensitive(root, "name");
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(root, "value");
    if (!cJSON_IsString(name) || !name->valuestring || !cJSON_IsNumber(value)) {
        cJSON_Delete(root);
        return send_err(r, "400 Bad Request",
                        "{\"ok\":false,\"error\":\"name and value required\"}");
    }

    const param_def_t *def = param_get_by_name(name->valuestring);
    if (!def) {
        cJSON_Delete(root);
        return send_err(r, "400 Bad Request",
                        "{\"ok\":false,\"error\":\"unknown_parameter\"}");
    }

    param_id_t id = (param_id_t)(def - param_get_at(0));
    float v = (float)value->valuedouble;
    bool ok = param_write(id, v);
    cJSON_Delete(root);

    if (!ok) {
        if (strcmp(def->id, "power") == 0)
            return send_err(r, "409 Conflict",
                            "{\"ok\":false,\"error\":\"regulator_not_ready\"}");
        return send_err(r, "403 Forbidden",
                        "{\"ok\":false,\"error\":\"locked_while_running\"}");
    }

    char json[160];
    snprintf(json, sizeof(json), "{\"ok\":true,\"name\":\"%s\",\"value\":%.2f}",
             def->id, (double)param_read(id));
    return send_json(r, json);
}

static esp_err_t h_fsel_algo(httpd_req_t *r) {
    if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);

    char body[96];
    int n = recv_body(r, body, sizeof(body));
    if (n <= 0) return send_err(r, "400 Bad Request", "{\"error\":\"empty body\"}");

    cJSON *root = cJSON_Parse(body);
    if (!root) return send_err(r, "400 Bad Request", "{\"error\":\"bad json\"}");
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(root, "algo");
    int algo = cJSON_IsNumber(v) ? v->valueint : -1;
    cJSON_Delete(root);

    if (algo < 0 || algo >= ALGO_COUNT)
        return send_err(r, "400 Bad Request", "{\"error\":\"algo\"}");

    esp_err_t err = fsel_set_algo((select_algo_t)algo);
    if (err != ESP_OK)
        return send_err(r, "500 Internal Server Error", "{\"error\":\"nvs\"}");

    char json[128];
    snprintf(json, sizeof(json), "{\"algo\":%d,\"name\":\"%s\"}",
             algo, algo_name((select_algo_t)algo));
    return send_json(r, json);
}

static esp_err_t h_verify_state(httpd_req_t *r) {
    if (api_check_read_auth(r) != ESP_OK) return api_reject_unauthorized(r);
    char json[1536];
    int off = snprintf(json, sizeof(json),
        "{\"enabled\":%d,\"stage\":%d,\"stage_name\":\"%s\","
        "\"shadow\":%d,\"shadow_name\":\"%s\",\"clean_runs\":%u,"
        "\"run_id\":%lld,\"db\":%d,\"modules\":[",
        verify_is_enabled() ? 1 : 0, (int)verify_get_stage(),
        verify_stage_name(verify_get_stage()),
        (int)verify_get_shadow_policy(),
        verify_shadow_name(verify_get_shadow_policy()),
        (unsigned)verify_get_clean_runs(verify_get_stage()),
        (long long)shadow_current_run(),
        verify_db_is_ready() ? 1 : 0);

    for (int i = 0; i < safety_gate_module_count() &&
                    off > 0 && off < (int)sizeof(json); i++) {
        const char *name = safety_gate_module_name(i);
        safety_class_t cls = safety_gate_class(name);
        off += snprintf(json + off, sizeof(json) - (size_t)off,
            "%s{\"name\":\"%s\",\"class\":\"%s\",\"verified\":%d}",
            i ? "," : "", name, safety_gate_class_name(cls),
            verify_is_module_verified(name) ? 1 : 0);
    }
    if (off > 0 && off < (int)sizeof(json) - 3) {
        snprintf(json + off, sizeof(json) - (size_t)off, "]}");
        return send_json(r, json);
    }
    return send_err(r, "500 Internal Server Error", "{\"error\":\"json\"}");
}

static esp_err_t h_verify_shadow(httpd_req_t *r) {
    if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);

    char body[64];
    int n = recv_body(r, body, sizeof(body));
    if (n <= 0) return send_err(r, "400 Bad Request", "{\"error\":\"empty\"}");
    cJSON *root = cJSON_Parse(body);
    if (!root) return send_err(r, "400 Bad Request", "{\"error\":\"json\"}");

    const cJSON *v = cJSON_GetObjectItemCaseSensitive(root, "policy");
    int policy = cJSON_IsNumber(v) ? v->valueint : -1;
    cJSON_Delete(root);
    if (policy < 0 || policy > SHADOW_STEP)
        return send_err(r, "400 Bad Request", "{\"error\":\"policy\"}");

    esp_err_t err = verify_set_shadow_policy((shadow_policy_t)policy);
    if (err != ESP_OK)
        return send_err(r, "500 Internal Server Error", "{\"error\":\"nvs\"}");

    if (policy == SHADOW_OFF) shadow_finish_run(NULL);
    else if (shadow_current_run() == 0) shadow_start_run("api");
    return send_json(r, "{\"status\":\"ok\"}");
}

static esp_err_t h_verify_stage(httpd_req_t *r) {
    if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);

    char body[64];
    int n = recv_body(r, body, sizeof(body));
    if (n <= 0) return send_err(r, "400 Bad Request", "{\"error\":\"empty\"}");
    cJSON *root = cJSON_Parse(body);
    if (!root) return send_err(r, "400 Bad Request", "{\"error\":\"json\"}");
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(root, "stage");
    int stage = cJSON_IsNumber(v) ? v->valueint : -1;
    cJSON_Delete(root);

    esp_err_t err = verify_set_stage((verify_stage_t)stage);
    if (err == ESP_ERR_INVALID_STATE)
        return send_err(r, "409 Conflict",
                        "{\"error\":\"need_clean_runs\"}");
    if (err != ESP_OK)
        return send_err(r, "400 Bad Request", "{\"error\":\"stage\"}");
    return send_json(r, "{\"status\":\"ok\"}");
}

static esp_err_t h_verify_action(httpd_req_t *r) {
    if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);

    char body[96];
    int n = recv_body(r, body, sizeof(body));
    if (n <= 0) return send_err(r, "400 Bad Request", "{\"error\":\"empty\"}");
    cJSON *root = cJSON_Parse(body);
    if (!root) return send_err(r, "400 Bad Request", "{\"error\":\"json\"}");
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(root, "action");
    const char *action = cJSON_IsString(v) ? v->valuestring : "";
    cJSON_Delete(root);

    if (strcmp(action, "run") == 0) {
        shadow_request_run();
    } else if (strcmp(action, "step") == 0) {
        shadow_request_step();
    } else if (strcmp(action, "cancel") == 0) {
        shadow_cancel();
    } else {
        // "clean" намеренно убран: счётчик чистых прогонов увеличивается
        // только реальным завершением adaptive PID (verify_report_clean_run).
        return send_err(r, "400 Bad Request",
                        "{\"error\":\"action\",\"hint\":\"run|step|cancel\"}");
    }
    return send_json(r, "{\"status\":\"ok\"}");
}

static esp_err_t h_verify_module(httpd_req_t *r) {
    if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);

    char body[128];
    int n = recv_body(r, body, sizeof(body));
    if (n <= 0) return send_err(r, "400 Bad Request", "{\"error\":\"empty\"}");
    cJSON *root = cJSON_Parse(body);
    if (!root) return send_err(r, "400 Bad Request", "{\"error\":\"json\"}");

    const cJSON *mv = cJSON_GetObjectItemCaseSensitive(root, "module");
    const cJSON *vv = cJSON_GetObjectItemCaseSensitive(root, "verified");
    const char *module = cJSON_IsString(mv) ? mv->valuestring : NULL;
    bool has_verified = cJSON_IsBool(vv);
    bool verified = has_verified && cJSON_IsTrue(vv);
    cJSON_Delete(root);

    if (!module || !module[0])
        return send_err(r, "400 Bad Request", "{\"error\":\"module\"}");
    if (!has_verified)
        return send_err(r, "400 Bad Request",
                        "{\"error\":\"verified must be boolean\"}");

    esp_err_t err = verified ? verify_mark_module_verified(module)
                             : verify_clear_module_verified(module);
    if (err != ESP_OK)
        return send_err(r, "400 Bad Request", "{\"error\":\"nvs\"}");
    return send_json(r, "{\"status\":\"ok\"}");
}

static esp_err_t h_verify_replay(httpd_req_t *r) {
    if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);

    char body[96];
    int n = recv_body(r, body, sizeof(body));
    int64_t run_id = shadow_current_run();
    if (n > 0) {
        cJSON *root = cJSON_Parse(body);
        if (root) {
            const cJSON *v = cJSON_GetObjectItemCaseSensitive(root, "run_id");
            if (cJSON_IsNumber(v)) run_id = (int64_t)v->valuedouble;
            cJSON_Delete(root);
        }
    }
    if (run_id <= 0)
        return send_err(r, "400 Bad Request", "{\"error\":\"run_id\"}");

    float max_err = 0.0f;
    int rows = 0;
    esp_err_t err = verify_db_replay(run_id, shadow_pid_preview, &max_err, &rows);

    char json[160];
    snprintf(json, sizeof(json),
             "{\"ok\":%d,\"run_id\":%lld,\"rows\":%d,\"max_err\":%.6f}",
             err == ESP_OK ? 1 : 0, (long long)run_id, rows,
             (double)max_err);
    return send_json(r, json);
}

static esp_err_t h_verify_freeze(httpd_req_t *r) {
    if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);
    adaptive_pid_t *p = adaptive_pid_global();
    if (!p) return send_err(r, "400 Bad Request", "{\"error\":\"adaptive_pid\"}");
    adaptive_pid_freeze_current(p);
    return send_json(r, "{\"status\":\"ok\"}");
}

static esp_err_t h_verify_fuzz(httpd_req_t *r) {
    if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);

    verify_fuzz_result_t res;
    esp_err_t err = verify_fuzz_anomaly(&res);
    char json[128];
    snprintf(json, sizeof(json),
             "{\"ok\":%d,\"tp\":%d,\"fp\":%d,\"fn\":%d,\"injected\":%d}",
             err == ESP_OK ? 1 : 0, res.tp, res.fp, res.fn, res.injected);
    return send_json(r, json);
}

static esp_err_t h_env(httpd_req_t *r) {
    if (api_check_read_auth(r) != ESP_OK) return api_reject_unauthorized(r);
    char json[384];
    snprintf(json, sizeof(json),
             "{\"baro\":{\"present\":%d,\"hpa\":%.2f,\"boiling_c\":%.2f},"
             "\"column\":{\"kpa\":%.2f,\"source\":\"%s\"},"
             "\"flow\":{\"present\":%d,\"lpm\":%.2f,\"ok\":%d},"
             "\"fraction\":\"%s\",\"reflux_ratio\":%.2f,\"reflux\":%d}",
             baro_available() ? 1 : 0, baro_get_pressure_hpa(),
             baro_boiling_point_c(baro_get_pressure_hpa()),
             column_pressure_kpa(), pressure_sensor_source(),
             flow_is_present() ? 1 : 0, flow_get_lpm(), flow_is_ok() ? 1 : 0,
             fraction_name(fraction_get()), reflux_get_ratio(),
             reflux_enabled() ? 1 : 0);
    return send_json(r, json);
}

// ============================================================
// registration
// ============================================================
void web_api_register(httpd_handle_t srv) {
    httpd_uri_t uris[] = {
        { .uri="/api/state",    .method=HTTP_GET,  .handler=h_state },
        { .uri="/api/start",    .method=HTTP_POST, .handler=h_start },
        { .uri="/api/stop",     .method=HTTP_POST, .handler=h_stop },
        { .uri="/api/pause",    .method=HTTP_POST, .handler=h_pause },
        { .uri="/api/power",    .method=HTTP_POST, .handler=h_power },
        { .uri="/api/safety",   .method=HTTP_POST, .handler=h_safety },
{ .uri="/api/safety",   .method=HTTP_GET,  .handler=h_safety },
{ .uri="/api/stepper",  .method=HTTP_GET,  .handler=h_stepper },
{ .uri="/api/stepper",  .method=HTTP_POST, .handler=h_stepper },
{ .uri="/api/hardware", .method=HTTP_GET,  .handler=h_hardware },
{ .uri="/api/hardware", .method=HTTP_POST, .handler=h_hardware },
{ .uri="/api/beer",     .method=HTTP_GET,  .handler=h_beer },
{ .uri="/api/beer",     .method=HTTP_POST, .handler=h_beer },
        { .uri="/api/estop",    .method=HTTP_POST, .handler=h_estop },
        { .uri="/api/control",  .method=HTTP_GET,  .handler=h_control },
        { .uri="/api/control",  .method=HTTP_POST, .handler=h_control },
        { .uri="/api/dose",     .method=HTTP_POST, .handler=h_dose },
        { .uri="/api/script/run",   .method=HTTP_POST, .handler=h_script_run },
        { .uri="/api/script/state", .method=HTTP_GET,  .handler=h_script_state },
        { .uri="/api/settings", .method=HTTP_POST, .handler=h_settings },
        { .uri="/api/wifi",     .method=HTTP_POST, .handler=h_wifi },
        { .uri="/api/mqtt",     .method=HTTP_POST, .handler=h_mqtt },
        { .uri="/api/telegram", .method=HTTP_POST, .handler=h_telegram },
        { .uri="/api/brewfather", .method=HTTP_POST, .handler=h_brewfather },
        { .uri="/api/calendar", .method=HTTP_POST, .handler=h_calendar },
        { .uri="/api/remote",  .method=HTTP_GET,  .handler=h_remote },
        { .uri="/api/remote",  .method=HTTP_POST, .handler=h_remote },
        { .uri="/api/ota",      .method=HTTP_POST, .handler=h_ota_post },
        { .uri="/ws",           .method=HTTP_GET,  .handler=h_ws, .is_websocket=true },
    };
    for (size_t i = 0; i < sizeof(uris)/sizeof(*uris); i++) {
        esp_err_t err = rate_limit_register(srv, &uris[i]);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "register %s failed: %s", uris[i].uri, esp_err_to_name(err));
        }
    }

    actuator_test_register(srv);

    // Новые эндпоинты V0.9 beta: health, журнал, managed OTA, бэкап, ключи,
    // телеметрия (WS), конфигурация.
    httpd_uri_t extra[] = {
        { .uri="/api/health",              .method=HTTP_GET,  .handler=h_health },
        { .uri="/api/trust",               .method=HTTP_GET,  .handler=h_trust },
        { .uri="/api/ws-token",            .method=HTTP_GET,  .handler=h_ws_token },
        { .uri="/api/events",              .method=HTTP_GET,  .handler=h_events },
        { .uri="/api/ota",                 .method=HTTP_GET,  .handler=h_ota_get },
        { .uri="/api/ota/url",             .method=HTTP_POST, .handler=h_ota_url },
        { .uri="/api/ota/confirm",         .method=HTTP_POST, .handler=h_ota_confirm },
        { .uri="/api/ota/secure",          .method=HTTP_POST, .handler=h_ota_secure },
        { .uri="/api/config",              .method=HTTP_GET,  .handler=h_config },
        { .uri="/api/config",              .method=HTTP_POST, .handler=h_config },
        { .uri="/api/config/export",       .method=HTTP_POST, .handler=h_config_export },
        { .uri="/api/config/import",       .method=HTTP_POST, .handler=h_config_import },
        { .uri="/api/config/generate-key", .method=HTTP_POST, .handler=h_config_generate_key },
        { .uri="/api/config/clear-key",    .method=HTTP_POST, .handler=h_config_clear_key },
        { .uri="/api/telegram/test",       .method=HTTP_POST, .handler=h_telegram_test },
        { .uri="/api/script/enable",       .method=HTTP_POST, .handler=h_script_enable },
        { .uri="/api/profiles",            .method=HTTP_GET,  .handler=h_profiles },
        { .uri="/api/profiles",            .method=HTTP_POST, .handler=h_profiles },
        { .uri="/api/profiles",            .method=HTTP_DELETE, .handler=h_profiles },
        { .uri="/api/autotune",            .method=HTTP_GET,  .handler=h_autotune },
        { .uri="/api/autotune",            .method=HTTP_POST, .handler=h_autotune },
        { .uri="/api/session",             .method=HTTP_GET,  .handler=h_session },
        { .uri="/api/session",             .method=HTTP_POST, .handler=h_session },
        { .uri="/api/i18n",                .method=HTTP_GET,  .handler=h_i18n },
        { .uri="/api/i18n",                .method=HTTP_POST, .handler=h_i18n },
        { .uri="/api/backup/telegram",     .method=HTTP_POST, .handler=h_backup_telegram },
        { .uri="/api/selection/state",          .method=HTTP_GET,  .handler=h_fsel_state },
        { .uri="/api/selection/config",         .method=HTTP_GET,  .handler=h_fsel_config },
        { .uri="/api/selection/config",         .method=HTTP_POST, .handler=h_fsel_config },
        { .uri="/api/selection/options",        .method=HTTP_POST, .handler=h_fsel_options },
        { .uri="/api/selection/next",           .method=HTTP_POST, .handler=h_fsel_next },
        { .uri="/api/selection/next_stage",     .method=HTTP_POST, .handler=h_fsel_next },
        { .uri="/api/selection/param",          .method=HTTP_POST, .handler=h_fsel_param },
        { .uri="/api/selection/algo",           .method=HTTP_POST, .handler=h_fsel_algo },
        { .uri="/api/alcohol",             .method=HTTP_GET,  .handler=h_alcohol },
        { .uri="/api/alcohol",             .method=HTTP_POST, .handler=h_alcohol },
        { .uri="/api/session/state",       .method=HTTP_GET,  .handler=h_session_state },
        { .uri="/api/session/minimize",    .method=HTTP_POST, .handler=h_session_minimize },
        { .uri="/api/session/restore",     .method=HTTP_POST, .handler=h_session_restore },
        { .uri="/api/session/stop",        .method=HTTP_POST, .handler=h_session_stop },
        { .uri="/api/param",               .method=HTTP_GET,  .handler=h_param_get },
        { .uri="/api/param",               .method=HTTP_POST, .handler=h_param_post },
{ .uri="/api/menu/toggles",        .method=HTTP_GET,  .handler=h_menu_toggles },
{ .uri="/api/history/processes",   .method=HTTP_GET,  .handler=h_history_processes },
{ .uri="/api/ai/status",           .method=HTTP_GET,  .handler=h_ai_status },
{ .uri="/api/maint/reset",         .method=HTTP_POST, .handler=h_maint_reset },
{ .uri="/api/maint/db",            .method=HTTP_POST, .handler=h_maint_db },
{ .uri="/api/usb/status",          .method=HTTP_GET,  .handler=h_usb_status },
{ .uri="/api/calib/state",         .method=HTTP_GET,  .handler=h_calib_state },
{ .uri="/api/menu/toggle",         .method=HTTP_POST, .handler=h_menu_toggle },
{ .uri="/api/recipes",             .method=HTTP_GET,  .handler=h_recipes_list },
{ .uri="/api/recipe",              .method=HTTP_GET,  .handler=h_recipe_get },
{ .uri="/api/calib/scale/tare",    .method=HTTP_POST, .handler=h_calib_scale_tare },
{ .uri="/api/calib/sensors/abort", .method=HTTP_POST, .handler=h_calib_sensors_abort },
        { .uri="/api/verify/state",        .method=HTTP_GET,  .handler=h_verify_state },
        { .uri="/api/verify/shadow",       .method=HTTP_POST, .handler=h_verify_shadow },
        { .uri="/api/verify/stage",        .method=HTTP_POST, .handler=h_verify_stage },
        { .uri="/api/verify/action",       .method=HTTP_POST, .handler=h_verify_action },
        { .uri="/api/verify/module",       .method=HTTP_POST, .handler=h_verify_module },
        { .uri="/api/verify/replay",       .method=HTTP_POST, .handler=h_verify_replay },
        { .uri="/api/verify/fuzz",         .method=HTTP_POST, .handler=h_verify_fuzz },
        { .uri="/api/verify/freeze",       .method=HTTP_POST, .handler=h_verify_freeze },
        { .uri="/api/env",                 .method=HTTP_GET,  .handler=h_env },
    };
    for (size_t i = 0; i < sizeof(extra)/sizeof(*extra); i++) {
        esp_err_t err = rate_limit_register(srv, &extra[i]);
        if (err != ESP_OK)
            ESP_LOGE(TAG, "register %s failed: %s", extra[i].uri, esp_err_to_name(err));
    }

    // WebSocket-телеметрия /ws/telemetry
    esp_err_t werr = ws_telemetry_attach(srv);
    if (werr != ESP_OK)
        ESP_LOGE(TAG, "ws telemetry attach failed: %s", esp_err_to_name(werr));
}
