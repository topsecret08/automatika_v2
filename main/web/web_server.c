#include "web_server.h"
#include "web_api.h"
#include "web_alice.h"
#include "actuator_test.h"
#include "rate_limit.h"
#include "core/events.h"
#include "core/flags.h"
#include "core/flash_fs.h"
#include "core/watchdog.h"
#include "config.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

static const char *TAG = "WEB";
static httpd_handle_t s_srv = NULL;

// Встроенные ассеты (EMBED_FILES) — быстрый путь без файловой системы.
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

static esp_err_t send_embedded(httpd_req_t *r, const char *type,
                               const uint8_t *start, const uint8_t *end) {
    httpd_resp_set_type(r, type);
    return httpd_resp_send(r, (const char *)start, end - start);
}

static esp_err_t h_index(httpd_req_t *r) {
    return send_embedded(r, "text/html", index_html_start, index_html_end);
}
static esp_err_t h_appjs(httpd_req_t *r) {
    return send_embedded(r, "application/javascript", app_js_start, app_js_end);
}
static esp_err_t h_css(httpd_req_t *r) {
    return send_embedded(r, "text/css", style_css_start, style_css_end);
}
static esp_err_t h_charts(httpd_req_t *r) {
    return send_embedded(r, "application/javascript", charts_js_start, charts_js_end);
}
static esp_err_t h_apijs(httpd_req_t *r) {
    return send_embedded(r, "application/javascript", api_js_start, api_js_end);
}
static esp_err_t h_wsjs(httpd_req_t *r) {
    return send_embedded(r, "application/javascript", ws_js_start, ws_js_end);
}
static esp_err_t h_recipes(httpd_req_t *r) {
    return send_embedded(r, "application/javascript", recipes_js_start, recipes_js_end);
}
static esp_err_t h_settings(httpd_req_t *r) {
    return send_embedded(r, "application/javascript", settings_js_start, settings_js_end);
}
static esp_err_t h_manifest(httpd_req_t *r) {
    return send_embedded(r, "application/json", manifest_start, manifest_end);
}

// ------------------------------------------------------------------
// LittleFS: пользовательские файлы из /lfs/www, затем /lfs.
// Регистрируется последним как "/*" и не перекрывает встроенные ассеты.
// ------------------------------------------------------------------
static const char *mime_for(const char *path) {
    const char *dot = strrchr(path, '.');
    if (!dot) return "application/octet-stream";
    if (!strcasecmp(dot, ".html") || !strcasecmp(dot, ".htm")) return "text/html";
    if (!strcasecmp(dot, ".js"))   return "application/javascript";
    if (!strcasecmp(dot, ".css"))  return "text/css";
    if (!strcasecmp(dot, ".json")) return "application/json";
    if (!strcasecmp(dot, ".png"))  return "image/png";
    if (!strcasecmp(dot, ".jpg") || !strcasecmp(dot, ".jpeg")) return "image/jpeg";
    if (!strcasecmp(dot, ".svg"))  return "image/svg+xml";
    if (!strcasecmp(dot, ".ico"))  return "image/x-icon";
    if (!strcasecmp(dot, ".txt"))  return "text/plain";
    return "application/octet-stream";
}

static bool uri_safe(const char *uri) {
    if (!uri || uri[0] != '/') return false;
    if (strstr(uri, "..") || strchr(uri, '\\') || strchr(uri, '\r') ||
        strchr(uri, '\n')) return false;
    return strlen(uri) < 96;
}

static esp_err_t h_static(httpd_req_t *r) {
    char uri[128];
    size_t n = strlen(r->uri);
    if (n >= sizeof(uri)) {
        httpd_resp_send_err(r, HTTPD_404_NOT_FOUND, "Not found");
        return ESP_FAIL;
    }
    memcpy(uri, r->uri, n + 1);

    // Отрезаем query string.
    char *q = strchr(uri, '?');
    if (q) *q = '\0';

    if (!uri_safe(uri) || !flash_fs_ready()) {
        httpd_resp_send_err(r, HTTPD_404_NOT_FOUND, "Not found");
        return ESP_FAIL;
    }

    // Служебные файлы LittleFS (профили, экспорт конфигурации) не отдаём
    // через статический обработчик — только через API с ключом.
    if (strncmp(uri, "/profiles", 9) == 0 ||
        strcmp(uri, "/backup.json") == 0 ||
        strncmp(uri, "/scripts", 8) == 0) {
        httpd_resp_send_err(r, HTTPD_404_NOT_FOUND, "Not found");
        return ESP_FAIL;
    }

    if (uri[0] == '/' && uri[1] == '\0')
        strcpy(uri, "/index.html");

    char path[160];
    snprintf(path, sizeof(path), LFS_MOUNT_POINT "/www%s", uri);

    struct stat st;
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) {
        snprintf(path, sizeof(path), LFS_MOUNT_POINT "%s", uri);
        if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) {
            httpd_resp_send_err(r, HTTPD_404_NOT_FOUND, "Not found");
            return ESP_FAIL;
        }
    }

    FILE *f = fopen(path, "r");
    if (!f) {
        httpd_resp_send_err(r, HTTPD_404_NOT_FOUND, "Not found");
        return ESP_FAIL;
    }

    httpd_resp_set_type(r, mime_for(path));
    char buf[1024];
    size_t rd;
    while ((rd = fread(buf, 1, sizeof(buf), f)) > 0) {
        if (httpd_resp_send_chunk(r, buf, rd) != ESP_OK) {
            fclose(f);
            httpd_resp_send_chunk(r, NULL, 0);
            return ESP_FAIL;
        }
    }
    fclose(f);
    httpd_resp_send_chunk(r, NULL, 0);
    return ESP_OK;
}

// ------------------------------------------------------------------
void web_start(void *arg) {
    (void)arg;

    if (!flags_get_by_key("wifi")) {
        vTaskDelete(NULL);
        return;
    }

    wd_register("web", 10000, false);

    // Wi-Fi уже поднят wifi_prov_init() в app_main.
    rate_limit_init();

    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.max_uri_handlers = 72;
    cfg.max_open_sockets = 7;
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

    // Все обработчики /api/* проверяют X-API-Key и rate limit внутри себя;
    // actuator_test_register() дополнительно требует сервисный режим.
    web_api_register(s_srv);
    web_alice_register(s_srv);
    actuator_test_register(s_srv);

    // Catch-all для LittleFS — строго последним.
    httpd_uri_t fallback = { .uri="/*", .method=HTTP_GET, .handler=h_static };
    httpd_register_uri_handler(s_srv, &fallback);

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
