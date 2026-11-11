#include "ws_handler.h"
#include "core/state.h"
#include <string.h>

esp_err_t ws_handler(httpd_req_t *r) {
    if (r->method == HTTP_GET) return ESP_OK;
    httpd_ws_frame_t f = { .type = HTTPD_WS_TYPE_TEXT };
    process_state_t *s = state_get();
    static char buf[256];
    snprintf(buf, sizeof(buf),
        "{\"cube\":%.1f,\"col\":%.1f,\"pwr\":%.0f}",
        s->cube_temp, s->column_temp, s->power_percent);
    f.payload = (uint8_t*)buf;
    f.len = strlen(buf);
    return httpd_ws_send_frame(r, &f);
}