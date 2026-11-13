#include "web_alice.h"
#include "rate_limit.h"
#include "core/state.h"
#include "core/events.h"
#include "core/skill_auth.h"
#include "control/ctrl_heater.h"
#include "control/ctrl_safety.h"
#include "control/ctrl_dose.h"
#include "modes/mode_base.h"
#include "cJSON.h"
#include <stdio.h>
#include <string.h>

// Webhook Алисы не может нести X-API-Key (запрос формирует сервис
// Яндекс), поэтому:
//   * включается только при заданном «skill token» (X-Skill-Token,
//     проверяется постоянным по времени сравнением);
//   * команды ограничены: старт/стоп процесса и запрос температуры;
//     произвольное управление мощностью через голос не принимается —
//     это защита от случайных/посторонних фраз.

static void reply(httpd_req_t *r, const char *text, bool end) {
    char out[512];
    snprintf(out, sizeof(out),
        "{\"version\":\"1.0\",\"response\":{\"text\":\"%s\",\"end_session\":%s}}",
        text, end ? "true" : "false");
    httpd_resp_set_type(r, "application/json");
    httpd_resp_sendstr(r, out);
}

static esp_err_t h_alice(httpd_req_t *r) {
    if (!skill_token_check_header(r)) {
        httpd_resp_send_err(r, HTTPD_401_UNAUTHORIZED, "{\"error\":\"skill token\"}");
        return ESP_FAIL;
    }

    char body[1024];
    int n = httpd_req_recv(r, body, sizeof(body) - 1);
    if (n <= 0) return ESP_FAIL;
    body[n] = 0;

    cJSON *root = cJSON_Parse(body);
    if (!root) { reply(r, "Ошибка запроса", true); return ESP_OK; }

    cJSON *req = cJSON_GetObjectItem(root, "request");
    cJSON *cmd = cJSON_GetObjectItem(req, "command");

    char text[160];

    if (cJSON_IsString(cmd)) {
        const char *c = cmd->valuestring;

        if (strstr(c, "старт") || strstr(c, "запусти")) {
            if (!ctrl_safety_ok()) {
                snprintf(text, sizeof(text), "Авария активна, запуск запрещён");
            } else {
                const process_mode_t *m = mode_get();
                if (!m) m = mode_from_type(PROCESS_DISTILL);
                if (m) mode_set(m);
                state_get()->is_running = true;
                state_get()->is_paused = false;
                events_log(EVENT_USER_ACTION, "Alice: start");
                snprintf(text, sizeof(text), "Запускаю процесс");
            }
        } else if (strstr(c, "стоп") || strstr(c, "останови")) {
            ctrl_heater_set_percent(0);
            ctrl_dose_stop();
            if (mode_get() && mode_get()->stop) mode_get()->stop();
            state_get()->is_running = false;
            events_log(EVENT_USER_ACTION, "Alice: stop");
            snprintf(text, sizeof(text), "Останавливаю");
        } else if (strstr(c, "температура")) {
            snprintf(text, sizeof(text), "Температура куба %.1f градусов",
                     state_get()->cube_temp);
        } else {
            snprintf(text, sizeof(text), "Не понял команду");
        }
    } else {
        snprintf(text, sizeof(text), "Не понял команду");
    }

    reply(r, text, false);
    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t web_alice_register(httpd_handle_t srv) {
    httpd_uri_t u = { .uri="/alice", .method=HTTP_POST, .handler=h_alice };
    return rate_limit_register(srv, &u);
}
