#include "http_handlers.h"
#include "esp_log.h"

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[]   asm("_binary_index_html_end");
extern const uint8_t app_js_start[]     asm("_binary_app_js_start");
extern const uint8_t app_js_end[]       asm("_binary_app_js_end");
extern const uint8_t style_css_start[]  asm("_binary_style_css_start");
extern const uint8_t style_css_end[]    asm("_binary_style_css_end");

esp_err_t http_index(httpd_req_t *r) {
    httpd_resp_set_type(r, "text/html");
    httpd_resp_send(r, (const char*)index_html_start,
                    index_html_end - index_html_start);
    return ESP_OK;
}

esp_err_t http_appjs(httpd_req_t *r) {
    httpd_resp_set_type(r, "application/javascript");
    httpd_resp_send(r, (const char*)app_js_start, app_js_end - app_js_start);
    return ESP_OK;
}

esp_err_t http_css(httpd_req_t *r) {
    httpd_resp_set_type(r, "text/css");
    httpd_resp_send(r, (const char*)style_css_start,
                    style_css_end - style_css_start);
    return ESP_OK;
}