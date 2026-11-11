#include "web_alice.h"
#include "core/state.h"
#include "modes/mode_base.h"
#include "cJSON.h"
#include <string.h>

static esp_err_t h_alice(httpd_req_t *r) {
    char body[1024];
    int n = httpd_req_recv(r, body, sizeof(body) - 1);
    if (n <= 0) return ESP_FAIL;
    body[n] = 0;

    cJSON *root = cJSON_Parse(body);
    cJSON *req = cJSON_GetObjectItem(root, "request");
    cJSON *cmd = cJSON_GetObjectItem(req, "command");

    const char *resp_text = "Не понял команду";
    char buf[128];
    if (cJSON_IsString(cmd)) {
        const char *c = cmd->valuestring;
        if (strstr(c, "старт")) {
            state_get()->is_running = true;
            resp_text = "Запускаю процесс";
        } else if (strstr(c, "стоп")) {
            state_get()->is_running = false;
            if (mode_get() && mode_get()->stop) mode_get()->stop();
            resp_text = "Останавливаю";
        } else if (strstr(c, "температура")) {
            snprintf(buf, sizeof(buf), "Температура куба %.1f градусов",
                     state_get()->cube_temp);
            resp_text = buf;
        }
    }
    char out[512];
    snprintf(out, sizeof(out),
        "{\"version\":\"1.0\",\"response\":{\"text\":\"%s\",\"end_session\":false}}",
        resp_text);
    cJSON_Delete(root);
    httpd_resp_set_type(r, "application/json");
    httpd_resp_sendstr(r, out);
    return ESP_OK;
}

void web_alice_register(httpd_handle_t srv) {
    httpd_uri_t u = { .uri="/alice", .method=HTTP_POST, .handler=h_alice };
    httpd_register_uri_handler(srv, &u);
}