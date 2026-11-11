#include "web_server.h"
#include "web_api.h"
#include "web_alice.h"
#include "core/events.h"
#include "core/flags.h"
#include "core/watchdog.h"
#include "esp_http_server.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "WEB";
static httpd_handle_t s_srv = NULL;

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[]   asm("_binary_index_html_end");
extern const uint8_t app_js_start[]     asm("_binary_app_js_start");
extern const uint8_t app_js_end[]       asm("_binary_app_js_end");
extern const uint8_t style_css_start[]  asm("_binary_style_css_start");
extern const uint8_t style_css_end[]    asm("_binary_style_css_end");
extern const uint8_t charts_js_start[]  asm("_binary_charts_js_start");
extern const uint8_t charts_js_end[]    asm("_binary_charts_js_end");
extern const uint8_t api_js_start[]     asm("_binary_api_js_start");
extern const uint8_t api_js_end[]       asm("_binary_api_js_end");
extern const uint8_t ws_js_start[]      asm("_binary_ws_js_start");
extern const uint8_t ws_js_end[]        asm("_binary_ws_js_end");
extern const uint8_t recipes_js_start[] asm("_binary_recipes_js_start");
extern const uint8_t recipes_js_end[]   asm("_binary_recipes_js_end");
extern const uint8_t settings_js_start[]asm("_binary_settings_js_start");
extern const uint8_t settings_js_end[]  asm("_binary_settings_js_end");
extern const uint8_t manifest_start[]   asm("_binary_manifest_json_start");
extern const uint8_t manifest_end[]     asm("_binary_manifest_json_end");

static esp_err_t h_index(httpd_req_t *r) {
    httpd_resp_set_type(r, "text/html");
    httpd_resp_send(r, (const char*)index_html_start, index_html_end - index_html_start);
    return ESP_OK;
}
static esp_err_t h_appjs(httpd_req_t *r) {
    httpd_resp_set_type(r, "application/javascript");
    httpd_resp_send(r, (const char*)app_js_start, app_js_end - app_js_start);
    return ESP_OK;
}
static esp_err_t h_css(httpd_req_t *r) {
    httpd_resp_set_type(r, "text/css");
    httpd_resp_send(r, (const char*)style_css_start, style_css_end - style_css_start);
    return ESP_OK;
}
static esp_err_t h_charts(httpd_req_t *r) {
    httpd_resp_set_type(r, "application/javascript");
    httpd_resp_send(r, (const char*)charts_js_start, charts_js_end - charts_js_start);
    return ESP_OK;
}
static esp_err_t h_apijs(httpd_req_t *r) {
    httpd_resp_set_type(r, "application/javascript");
    httpd_resp_send(r, (const char*)api_js_start, api_js_end - api_js_start);
    return ESP_OK;
}
static esp_err_t h_wsjs(httpd_req_t *r) {
    httpd_resp_set_type(r, "application/javascript");
    httpd_resp_send(r, (const char*)ws_js_start, ws_js_end - ws_js_start);
    return ESP_OK;
}
static esp_err_t h_recipes(httpd_req_t *r) {
    httpd_resp_set_type(r, "application/javascript");
    httpd_resp_send(r, (const char*)recipes_js_start, recipes_js_end - recipes_js_start);
    return ESP_OK;
}
static esp_err_t h_settings(httpd_req_t *r) {
    httpd_resp_set_type(r, "application/javascript");
    httpd_resp_send(r, (const char*)settings_js_start, settings_js_end - settings_js_start);
    return ESP_OK;
}
static esp_err_t h_manifest(httpd_req_t *r) {
    httpd_resp_set_type(r, "application/json");
    httpd_resp_send(r, (const char*)manifest_start, manifest_end - manifest_start);
    return ESP_OK;
}

static void wifi_start(void) {
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t wc = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wc);

    // TODO: брать из NVS
    wifi_config_t cfg = {0};
    strncpy((char*)cfg.sta.ssid, "YOUR_SSID", 32);
    strncpy((char*)cfg.sta.password, "YOUR_PASS", 64);

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &cfg);
    esp_wifi_start();
    esp_wifi_connect();
}

void web_start(void *arg) {
    (void)arg;
    wd_register("web", 10000, false);

    if (!flags_get_by_key("wifi")) {
        vTaskDelete(NULL);
        return;
    }

    wifi_start();

    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.max_uri_handlers = 24;
    cfg.stack_size = 8192;
    cfg.lru_purge_enable = true;

    if (httpd_start(&s_srv, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "httpd start failed");
        vTaskDelete(NULL);
        return;
    }

    httpd_uri_t uris[] = {
        { .uri="/",              .method=HTTP_GET, .handler=h_index },
        { .uri="/app.js",        .method=HTTP_GET, .handler=h_appjs },
        { .uri="/style.css",     .method=HTTP_GET, .handler=h_css },
        { .uri="/charts.js",     .method=HTTP_GET, .handler=h_charts },
        { .uri="/api.js",        .method=HTTP_GET, .handler=h_apijs },
        { .uri="/ws.js",         .method=HTTP_GET, .handler=h_wsjs },
        { .uri="/recipes.js",    .method=HTTP_GET, .handler=h_recipes },
        { .uri="/settings.js",   .method=HTTP_GET, .handler=h_settings },
        { .uri="/manifest.json", .method=HTTP_GET, .handler=h_manifest },
    };
    for (size_t i = 0; i < sizeof(uris)/sizeof(*uris); i++)
        httpd_register_uri_handler(s_srv, &uris[i]);

    web_api_register(s_srv);
    web_alice_register(s_srv);

    ESP_LOGI(TAG, "Web server started");
    events_log(EVENT_INFO, "Web started");

    while (1) {
        WD_FEED("web");
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void web_stop(void) {
    if (s_srv) { httpd_stop(s_srv); s_srv = NULL; }
}