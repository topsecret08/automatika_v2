#include "web_api.h"
#include "api_auth.h"
#include "actuator_test.h"

#include "core/control_config.h"
#include "core/estop.h"
#include "core/roles.h"
#include "core/state.h"
#include "core/events.h"
#include "core/flags.h"
#include "core/watchdog.h"
#include "hal/hal_1wire.h"
#include "modes/mode_base.h"
#include "control/ctrl_heater.h"
#include "control/ctrl_safety.h"
#include "control/ctrl_dose.h"
#include "control/ctrl_pid.h"
#include "script/lua_engine.h"
#include "web/mqtt_app.h"
#include "web/telegram_bot.h"
#include "web/brewfather.h"
#include "web/calendar.h"
#include "web/samovar.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_log.h"
#include "cJSON.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

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
    process_state_t *s = state_get();
    char json[768];
    snprintf(json, sizeof(json),
        "{\"type\":%d,\"stage\":%d,\"cube\":%.2f,\"col\":%.2f,"
        "\"power\":%.1f,\"heads\":%.0f,\"body\":%.0f,\"tails\":%.0f,"
        "\"pressure\":%.2f,\"vacuum\":%.0f,\"running\":%d,"
        "\"paused\":%d,\"elapsed\":%u,\"safety\":%d,"
        "\"estop\":%d,\"slope0\":%.2f,\"rising0\":%d,"
        "\"mode\":\"%s\",\"ctrl_mode\":%d,\"out_dev\":%d}",
        (int)s->type, (int)s->stage, s->cube_temp, s->column_temp,
        s->power_percent, s->heads_collected_ml, s->body_collected_ml,
        s->tails_collected_ml, s->pressure_bar, s->vacuum_mbar,
        s->is_running ? 1 : 0, s->is_paused ? 1 : 0, (unsigned)s->elapsed_sec,
        ctrl_safety_ok() ? 1 : 0, estop_is_pressed() ? 1 : 0,
        hal_1wire_slope(0), hal_1wire_rising(0) ? 1 : 0,
        mode_get() ? mode_get()->id : "none",
        (int)control_config_get_mode(),
        (int)control_config_get_output_device());
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
    ctrl_heater_set_percent(0);
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

    float pct = 0;
    if (n > 0) {
        cJSON *root = cJSON_Parse(body);
        if (root) {
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

    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;

    if (pct > 0 && !ctrl_safety_ok())
        return send_err(r, "409 Conflict", "{\"error\":\"safety lock active\"}");
    if (pct > 0 && estop_is_pressed())
        return send_err(r, "409 Conflict", "{\"error\":\"estop active\"}");

    ctrl_heater_set_percent(pct);
    events_log(EVENT_INFO, "Web: power %.0f%%", pct);
    char out[64];
    snprintf(out, sizeof(out), "{\"power\":%.1f}", ctrl_heater_get_percent());
    return send_json(r, out);
}

static esp_err_t h_safety(httpd_req_t *r) {
    if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);

    if (!ctrl_safety_ok()) ctrl_safety_reset();

    char json[128];
    snprintf(json, sizeof(json), "{\"safety\":%d,\"estop\":%d}",
             ctrl_safety_ok() ? 1 : 0, estop_is_pressed() ? 1 : 0);
    return send_json(r, json);
}

// --- установки управления (control_config) ----------------------------
static esp_err_t h_control(httpd_req_t *r) {
    if (r->method == HTTP_GET) {
        char json[256];
        snprintf(json, sizeof(json),
                 "{\"ctrl_mode\":%d,\"out_dev\":%d,\"hys\":%.2f,"
                 "\"rate_mlmin\":%.2f,\"flow_mlmin\":%.2f,\"steps_per_ml\":%u}",
                 (int)control_config_get_mode(),
                 (int)control_config_get_output_device(),
                 control_config_get_hys(),
                 control_config_get_rate_ml_min(),
                 control_config_get_flow_ml_min(),
                 (unsigned)control_config_get_steps_per_ml());
        return send_json(r, json);
    }

    if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);

    char body[256];
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

    cJSON_Delete(root);

    if (err != ESP_OK) return send_err(r, "400 Bad Request", "{\"error\":\"invalid\"}");
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
    if (!ctrl_dose_volume(ml, rm))
        return send_err(r, "409 Conflict", "{\"error\":\"dose rejected\"}");

    return send_json(r, "{\"status\":\"ok\"}");
}

// --- диагностика E-STOP -----------------------------------------------
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
// Samovar / Blynk + MQTT bridge
// ============================================================
static void json_copy_str(char *dst, size_t max, const cJSON *v) {
    if (!cJSON_IsString(v) || !v->valuestring) return;
    strncpy(dst, v->valuestring, max - 1);
    dst[max - 1] = '\0';
}

static void samovar_status_json(char *json, size_t max) {
    samovar_status_t st;
    samovar_get_status(&st);
    snprintf(json, max,
        "{\"enabled\":%d,\"token_set\":%d,\"blynk\":%d,"
        "\"mqtt_configured\":%d,\"mqtt\":%d,"
        "\"host\":\"%s\",\"port\":%u,\"vref_mv\":%u}",
        st.enabled ? 1 : 0, st.token_set ? 1 : 0,
        st.blynk_connected ? 1 : 0,
        st.mqtt_configured ? 1 : 0, st.mqtt_connected ? 1 : 0,
        st.host, (unsigned)st.port, (unsigned)st.vref_mv);
}

static esp_err_t h_samovar(httpd_req_t *r) {
    if (r->method == HTTP_GET) {
        char json[512];
        samovar_status_json(json, sizeof(json));
        return send_json(r, json);
    }

    if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);

    char body[768];
    if (recv_body(r, body, sizeof(body)) <= 0)
        return send_err(r, "400 Bad Request", "{\"error\":\"empty body\"}");

    cJSON *root = cJSON_Parse(body);
    if (!root)
        return send_err(r, "400 Bad Request", "{\"error\":\"bad json\"}");

    samovar_cfg_t cfg;
    samovar_get_config(&cfg);

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

    esp_err_t err = samovar_configure(&cfg);
    cJSON_Delete(root);

    if (err != ESP_OK)
        return send_err(r, "400 Bad Request",
                        "{\"error\":\"invalid config\"}");

    char json[512];
    samovar_status_json(json, sizeof(json));
    return send_json(r, json);
}

// ============================================================
// OTA
// ============================================================
static esp_err_t h_ota_post(httpd_req_t *r) {
    if (api_check_auth(r) != ESP_OK) return api_reject_unauthorized(r);

    const esp_partition_t *p = esp_ota_get_next_update_partition(NULL);
    if (!p) return send_err(r, "500 Internal Server Error", "{\"error\":\"no ota partition\"}");
    if (r->content_len <= 0)
        return send_err(r, "400 Bad Request", "{\"error\":\"empty image\"}");

    esp_ota_handle_t h = 0;
    esp_err_t err = esp_ota_begin(p, OTA_SIZE_UNKNOWN, &h);
    if (err != ESP_OK) {
        events_log(EVENT_ERROR, "Web OTA begin failed: %s", esp_err_to_name(err));
        return send_err(r, "500 Internal Server Error", "{\"error\":\"ota begin\"}");
    }

    char buf[1024];
    int n;
    int remaining = r->content_len;
    while (remaining > 0) {
        n = httpd_req_recv(r, buf, sizeof(buf));
        if (n <= 0) {
            if (n == HTTPD_SOCK_ERR_TIMEOUT) continue;
            esp_ota_abort(h);
            events_log(EVENT_ERROR, "Web OTA receive failed");
            return send_err(r, "400 Bad Request", "{\"error\":\"receive\"}");
        }
        err = esp_ota_write(h, buf, n);
        if (err != ESP_OK) {
            esp_ota_abort(h);
            events_log(EVENT_ERROR, "Web OTA write failed: %s", esp_err_to_name(err));
            return send_err(r, "500 Internal Server Error", "{\"error\":\"write\"}");
        }
        remaining -= n;
    }

    err = esp_ota_end(h);
    if (err != ESP_OK) {
        events_log(EVENT_ERROR, "Web OTA end failed: %s", esp_err_to_name(err));
        return send_err(r, "500 Internal Server Error", "{\"error\":\"validate\"}");
    }

    err = esp_ota_set_boot_partition(p);
    if (err != ESP_OK) {
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
// WebSocket
// ============================================================
static esp_err_t h_ws(httpd_req_t *r) {
    if (r->method == HTTP_GET) return ESP_OK;

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
        { .uri="/api/samovar",  .method=HTTP_GET,  .handler=h_samovar },
        { .uri="/api/samovar",  .method=HTTP_POST, .handler=h_samovar },
        { .uri="/api/ota",      .method=HTTP_POST, .handler=h_ota_post },
        { .uri="/ws",           .method=HTTP_GET,  .handler=h_ws, .is_websocket=true },
    };
    for (size_t i = 0; i < sizeof(uris)/sizeof(*uris); i++) {
        esp_err_t err = httpd_register_uri_handler(srv, &uris[i]);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "register %s failed: %s", uris[i].uri, esp_err_to_name(err));
        }
    }

    actuator_test_register(srv);
}
