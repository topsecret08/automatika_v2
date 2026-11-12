#include "web_api.h"
#include "core/state.h"
#include "core/events.h"
#include "core/flags.h"
#include "modes/mode_base.h"
#include "control/ctrl_heater.h"
#include "esp_ota_ops.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

static esp_err_t h_state(httpd_req_t *r) {
    process_state_t *s = state_get();
    char json[512];
    snprintf(json, sizeof(json),
        "{\"type\":%d,\"stage\":%d,\"cube\":%.1f,\"col\":%.1f,"
        "\"power\":%.0f,\"heads\":%.0f,\"body\":%.0f,\"tails\":%.0f,"
        "\"pressure\":%.2f,\"vacuum\":%.0f,\"running\":%d,"
        "\"paused\":%d,\"elapsed\":%u}",
        s->type, s->stage, s->cube_temp, s->column_temp,
        s->power_percent, s->heads_collected_ml, s->body_collected_ml,
        s->tails_collected_ml, s->pressure_bar, s->vacuum_mbar,
        s->is_running, s->is_paused, (unsigned)s->elapsed_sec);
    httpd_resp_set_type(r, "application/json");
    httpd_resp_sendstr(r, json);
    return ESP_OK;
}

static esp_err_t h_start(httpd_req_t *r) {
    state_get()->is_running = true;
    state_get()->is_paused = false;
    httpd_resp_sendstr(r, "{\"status\":\"ok\"}");
    return ESP_OK;
}

static esp_err_t h_stop(httpd_req_t *r) {
    if (mode_get() && mode_get()->stop) mode_get()->stop();
    state_get()->is_running = false;
    httpd_resp_sendstr(r, "{\"status\":\"ok\"}");
    return ESP_OK;
}

static esp_err_t h_pause(httpd_req_t *r) {
    state_get()->is_paused = true;
    httpd_resp_sendstr(r, "{\"status\":\"ok\"}");
    return ESP_OK;
}

static esp_err_t h_power(httpd_req_t *r) {
    char buf[16] = {0};
    int len = httpd_req_recv(r, buf, sizeof(buf) - 1);
    if (len > 0) {
        float pct = atof(buf);
        ctrl_heater_set_percent(pct);
    }
    httpd_resp_sendstr(r, "{\"status\":\"ok\"}");
    return ESP_OK;
}

static esp_err_t h_ota_post(httpd_req_t *r) {
    esp_ota_handle_t h;
    const esp_partition_t *p = esp_ota_get_next_update_partition(NULL);
    esp_ota_begin(p, OTA_SIZE_UNKNOWN, &h);
    char buf[1024]; int n;
    while ((n = httpd_req_recv(r, buf, sizeof(buf))) > 0)
        esp_ota_write(h, buf, n);
    esp_ota_end(h);
    esp_ota_set_boot_partition(p);
    httpd_resp_sendstr(r, "OTA OK, rebooting...");
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
    return ESP_OK;
}

static esp_err_t h_ws(httpd_req_t *r) {
    if (r->method == HTTP_GET) return ESP_OK;
    httpd_ws_frame_t f = { .type = HTTPD_WS_TYPE_TEXT };
    process_state_t *s = state_get();
    static char buf[256];
    snprintf(buf, sizeof(buf),
        "{\"cube\":%.1f,\"col\":%.1f,\"pwr\":%.0f,\"stg\":%d}",
        s->cube_temp, s->column_temp, s->power_percent, s->stage);
    f.payload = (uint8_t*)buf;
    f.len = strlen(buf);
    return httpd_ws_send_frame(r, &f);
}

void web_api_register(httpd_handle_t srv) {
    httpd_uri_t uris[] = {
        { .uri="/api/state", .method=HTTP_GET,  .handler=h_state },
        { .uri="/api/start", .method=HTTP_POST, .handler=h_start },
        { .uri="/api/stop",  .method=HTTP_POST, .handler=h_stop },
        { .uri="/api/pause", .method=HTTP_POST, .handler=h_pause },
        { .uri="/api/power", .method=HTTP_POST, .handler=h_power },
        { .uri="/api/ota",   .method=HTTP_POST, .handler=h_ota_post },
        { .uri="/ws",        .method=HTTP_GET,  .handler=h_ws, .is_websocket=true },
    };
    for (size_t i = 0; i < sizeof(uris)/sizeof(*uris); i++)
        httpd_register_uri_handler(srv, &uris[i]);
}