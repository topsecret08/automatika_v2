#include "actuator_test.h"
#include "api_auth.h"

#include "control/ctrl_dose.h"
#include "control/ctrl_heater.h"
#include "control/ctrl_safety.h"
#include "control/ctrl_valve_control.h"
#include "core/control_config.h"
#include "core/estop.h"
#include "core/events.h"
#include "config.h"
#include "hal/hal_relay.h"

#include "cJSON.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "ACTUATOR";

// ------------------------------------------------------------------
// Автовыключение тестового нагрева через окно в секундах (1..60).
// Отдельная задача — POST возвращается сразу.
// ------------------------------------------------------------------
static void heater_off_delay(void *arg) {
    uint32_t sec = (uint32_t)(uintptr_t)arg;
    vTaskDelay(pdMS_TO_TICKS(sec * 1000));
    ctrl_heater_service_set(0.0f);
    events_log(EVENT_INFO, "Service: heater test off after %us", (unsigned)sec);
    vTaskDelete(NULL);
}

static const char *json_str(const cJSON *root, const char *key) {
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(root, key);
    return cJSON_IsString(v) ? v->valuestring : NULL;
}

static esp_err_t h_diag_actuators(httpd_req_t *req) {
    if (api_check_auth(req) != ESP_OK) return api_reject_unauthorized(req);
    if (!control_config_service_enabled()) {
        httpd_resp_send_err(req, HTTPD_403_FORBIDDEN,
                            "{\"error\":\"service mode disabled\"}");
        return ESP_FAIL;
    }

    char buf[192];
    int n = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (n <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "{\"error\":\"empty\"}");
        return ESP_FAIL;
    }
    buf[n] = 0;

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "{\"error\":\"bad json\"}");
        return ESP_FAIL;
    }

    const char *actuator = json_str(root, "actuator");
    const char *action = json_str(root, "action");

    if (!actuator || !action) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                            "{\"error\":\"actuator/action required\"}");
        return ESP_FAIL;
    }

    if (!ctrl_safety_ok() || estop_is_pressed()) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                            "{\"error\":\"safety/estop active\"}");
        return ESP_FAIL;
    }

    bool ok = false;
    bool prehandled = false;

    if (strcmp(actuator, "valve") == 0) {
        if (strcmp(action, "open") == 0) {
            valve_set_pct(VALVE_BODY, 100.0f);
            ok = true;
        } else if (strcmp(action, "close") == 0) {
            valve_set_pct(VALVE_BODY, 0.0f);
            ok = true;
        }
    } else if (strcmp(actuator, "pump") == 0) {
        if (strcmp(action, "dose") == 0) {
            double ml = 10.0, rate = 60.0;
            const cJSON *j = cJSON_GetObjectItemCaseSensitive(root, "ml");
            if (cJSON_IsNumber(j)) ml = j->valuedouble;
            j = cJSON_GetObjectItemCaseSensitive(root, "rate");
            if (cJSON_IsNumber(j)) rate = j->valuedouble;
            ok = ctrl_dose_volume((float)ml, (float)rate);
            if (!ok) {
                prehandled = true;
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                                    "{\"error\":\"dose rejected\"}");
            }
        }
    } else if (strcmp(actuator, "heater") == 0) {
        if (strcmp(action, "on") == 0) {
            double pct = 10.0, secs = 5.0;
            const cJSON *j = cJSON_GetObjectItemCaseSensitive(root, "pct");
            if (cJSON_IsNumber(j)) pct = j->valuedouble;
            j = cJSON_GetObjectItemCaseSensitive(root, "seconds");
            if (cJSON_IsNumber(j)) secs = j->valuedouble;
            if (secs < 1.0) secs = 1.0;
            if (secs > 60.0) secs = 60.0;
            ok = ctrl_heater_service_set((float)pct);
            if (ok) {
                xTaskCreate(heater_off_delay, "svc_hoff", 2048,
                            (void *)(uintptr_t)secs, 3, NULL);
            }
        } else if (strcmp(action, "off") == 0) {
            ctrl_heater_service_set(0.0f);
            ok = true;
        }
    } else if (strcmp(actuator, "relay") == 0) {
        const cJSON *ch = cJSON_GetObjectItemCaseSensitive(root, "ch");
        const cJSON *st = cJSON_GetObjectItemCaseSensitive(root, "state");
        if (cJSON_IsNumber(ch) && cJSON_IsNumber(st)) {
            hal_relay_set(ch->valueint, st->valueint != 0);
            ok = true;
        }
    }

    cJSON_Delete(root);
    if (prehandled) return ESP_FAIL;

    if (!ok) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "{\"error\":\"unknown\"}");
        return ESP_FAIL;
    }

    ESP_LOGW(TAG, "service action %s/%s", actuator, action);
    events_log(EVENT_USER_ACTION, "Service actuator: %s/%s", actuator, action);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}

static esp_err_t h_diag_actuator_status(httpd_req_t *req) {
    if (api_check_auth(req) != ESP_OK) return api_reject_unauthorized(req);

    char json[256];
    snprintf(json, sizeof(json),
             "{\"service\":%d,\"safety\":%d,\"estop\":%d,"
             "\"heater\":%.1f,\"dose_busy\":%d,\"steps_per_ml\":%u}",
             control_config_service_enabled() ? 1 : 0,
             ctrl_safety_ok() ? 1 : 0, estop_is_pressed() ? 1 : 0,
             ctrl_heater_get_percent(), ctrl_dose_busy() ? 1 : 0,
             (unsigned)ctrl_dose_steps_per_ml());
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json);
    return ESP_OK;
}

static esp_err_t h_diag_service(httpd_req_t *req) {
    if (api_check_auth(req) != ESP_OK) return api_reject_unauthorized(req);

    char buf[96];
    int n = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (n <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "{\"error\":\"empty\"}");
        return ESP_FAIL;
    }
    buf[n] = 0;

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "{\"error\":\"bad json\"}");
        return ESP_FAIL;
    }
    const cJSON *en = cJSON_GetObjectItemCaseSensitive(root, "enabled");
    bool on = cJSON_IsTrue(en);
    if (cJSON_IsNumber(en)) on = en->valueint != 0;
    cJSON_Delete(root);

    if (control_config_set_service(on) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "{\"error\":\"nvs\"}");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, on ? "{\"service\":true}" : "{\"service\":false}");
    return ESP_OK;
}

void actuator_test_register(httpd_handle_t srv) {
    httpd_uri_t u1 = { .uri = "/api/diag/actuators", .method = HTTP_POST,
                       .handler = h_diag_actuators };
    httpd_uri_t u2 = { .uri = "/api/diag/service", .method = HTTP_POST,
                       .handler = h_diag_service };
    httpd_uri_t u3 = { .uri = "/api/diag/status", .method = HTTP_GET,
                       .handler = h_diag_actuator_status };
    httpd_register_uri_handler(srv, &u1);
    httpd_register_uri_handler(srv, &u2);
    httpd_register_uri_handler(srv, &u3);
}
