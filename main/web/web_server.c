#include "web_server.h"
#include "web_api.h"
#include "web_alice.h"
#include "actuator_test.h"
#include "core/events.h"
#include "core/flags.h"
#include "core/watchdog.h"
#include "esp_http_server.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_random.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
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

// Пароль точки доступа генерируется один раз и хранится в NVS "wifi".
// Пароль по умолчанию (automatika123) больше не используется: AP с
// известным паролем — прямой доступ к управлению нагревом.
static void ap_password(char *out, size_t out_len) {
    nvs_handle_t h;
    if (nvs_open("wifi", NVS_READWRITE, &h) != ESP_OK) {
        snprintf(out, out_len, "%s", "automatika");
        return;
    }

    size_t sz = out_len;
    if (nvs_get_str(h, "ap_pass", out, &sz) == ESP_OK && strlen(out) >= 8) {
        nvs_close(h);
        return;
    }

    static const char alnum[] =
        "abcdefghijkmnopqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    uint8_t rnd[12];
    esp_fill_random(rnd, sizeof(rnd));
    for (size_t i = 0; i < 10 && i + 1 < out_len; i++)
        out[i] = alnum[rnd[i] % (sizeof(alnum) - 1)];
    out[10] = '\0';

    nvs_set_str(h, "ap_pass", out);
    nvs_commit(h);
    nvs_close(h);
    events_log(EVENT_WARNING, "Wi-Fi AP password generated (see menu «QR → AP»)");
}

static void wifi_start(void) {
    esp_netif_init();
    esp_err_t ev = esp_event_loop_create_default();
    if (ev != ESP_OK && ev != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "event loop init failed: %s", esp_err_to_name(ev));
    }

    wifi_init_config_t wc = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wc);

    // Учётные данные из NVS (настраиваются через /api/wifi).
    char ssid[33] = "";
    char pass[65] = "";
    nvs_handle_t h;
    if (nvs_open("wifi", NVS_READONLY, &h) == ESP_OK) {
        size_t sz = sizeof(ssid);
        nvs_get_str(h, "ssid", ssid, &sz);
        sz = sizeof(pass);
        nvs_get_str(h, "pass", pass, &sz);
        nvs_close(h);
    }

    if (ssid[0]) {
        esp_netif_create_default_wifi_sta();

        wifi_config_t cfg = {0};
        strncpy((char*)cfg.sta.ssid, ssid, sizeof(cfg.sta.ssid));
        strncpy((char*)cfg.sta.password, pass, sizeof(cfg.sta.password));

        esp_wifi_set_mode(WIFI_MODE_STA);
        esp_wifi_set_config(WIFI_IF_STA, &cfg);
        esp_wifi_start();
        esp_wifi_connect();
        ESP_LOGI(TAG, "Wi-Fi STA: %s", ssid);
    } else {
        // Нет сохранённой сети — поднимаем точку доступа для настройки.
        esp_netif_create_default_wifi_ap();

        char appass[16];
        ap_password(appass, sizeof(appass));

        wifi_config_t cfg = {0};
        strncpy((char*)cfg.ap.ssid, "Automatika_Setup", sizeof(cfg.ap.ssid));
        cfg.ap.ssid_len = strlen("Automatika_Setup");
        strncpy((char*)cfg.ap.password, appass, sizeof(cfg.ap.password));
        cfg.ap.max_connection = 4;
        cfg.ap.authmode = WIFI_AUTH_WPA2_PSK;

        esp_wifi_set_mode(WIFI_MODE_AP);
        esp_wifi_set_config(WIFI_IF_AP, &cfg);
        esp_wifi_start();
        ESP_LOGW(TAG, "Wi-Fi: no credentials, AP '%s' started (password from NVS)",
                 "Automatika_Setup");
        events_log(EVENT_WARNING, "Wi-Fi AP mode: Automatika_Setup");
    }
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
    cfg.max_uri_handlers = 40;
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

    // Все обработчики /api/* проверяют X-API-Key внутри себя;
    // actuator_test_register() дополнительно требует сервисный режим.
    web_api_register(s_srv);
    web_alice_register(s_srv);
    actuator_test_register(s_srv);

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

httpd_handle_t web_get_handle(void) { return s_srv; }
