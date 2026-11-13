#include "web_server.h"
#include "web_api.h"
#include "web_alice.h"
#include "actuator_test.h"
#include "rate_limit.h"
#include "https_server.h"
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
#include <sys/types.h>

static const char *TAG = "WEB";
static httpd_handle_t s_srv = NULL;      // HTTP  :80
static httpd_handle_t s_https = NULL;    // HTTPS :443 (PWA)

// ------------------------------------------------------------------
// Встроенные ассеты (EMBED_FILES) — быстрый путь без файловой системы.
// Файлы лежат в ../littlefs (см. main/CMakeLists.txt). IDF берёт basename,
// поэтому имена символов остались прежними.
// ------------------------------------------------------------------
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
extern const uint8_t sw_js_start[]      asm("_binary_sw_js_start");
extern const uint8_t sw_js_end[]        asm("_binary_sw_js_end");
extern const uint8_t offline_html_start[] asm("_binary_offline_html_start");
extern const uint8_t offline_html_end[]   asm("_binary_offline_html_end");
extern const uint8_t icon_192_png_start[] asm("_binary_icon_192_png_start");
extern const uint8_t icon_192_png_end[]   asm("_binary_icon_192_png_end");
extern const uint8_t icon_512_png_start[] asm("_binary_icon_512_png_start");
extern const uint8_t icon_512_png_end[]   asm("_binary_icon_512_png_end");
extern const uint8_t icon_maskable_512_png_start[] asm("_binary_icon_maskable_512_png_start");
extern const uint8_t icon_maskable_512_png_end[]   asm("_binary_icon_maskable_512_png_end");
extern const uint8_t apple_touch_icon_png_start[] asm("_binary_apple_touch_icon_png_start");
extern const uint8_t apple_touch_icon_png_end[]   asm("_binary_apple_touch_icon_png_end");

// ------------------------------------------------------------------
// Общие заголовки
// ------------------------------------------------------------------
static void set_security_headers(httpd_req_t *r) {
    httpd_resp_set_hdr(r, "X-Content-Type-Options", "nosniff");
    httpd_resp_set_hdr(r, "X-Frame-Options", "DENY");
    httpd_resp_set_hdr(r, "Referrer-Policy", "no-referrer");
    // connect-src нужен для WebSocket: 'self' не покрывает ws:/wss:.
    httpd_resp_set_hdr(r, "Content-Security-Policy",
        "default-src 'self'; "
        "script-src 'self'; "
        "style-src 'self'; "
        "img-src 'self' data:; "
        "connect-src 'self' ws: wss:; "
        "object-src 'none'; base-uri 'none'; frame-ancestors 'none'");
}

// Кэширование: HTML и service worker — всегда свежие, статика — надолго.
static const char *cache_for(const char *path) {
    if (strstr(path, ".html") || strcmp(path, "/") == 0) return "no-cache";
    if (strstr(path, "sw.js")) return "no-cache";
    if (strstr(path, ".css") || strstr(path, ".js")) return "public, max-age=86400";
    if (strstr(path, ".json")) return "public, max-age=3600";
    if (strstr(path, ".png") || strstr(path, ".svg") || strstr(path, ".ico"))
        return "public, max-age=604800";
    return "no-cache";
}

static esp_err_t send_embedded(httpd_req_t *r, const char *path, const char *type,
                               const uint8_t *start, const uint8_t *end) {
    set_security_headers(r);
    httpd_resp_set_type(r, type);
    httpd_resp_set_hdr(r, "Cache-Control", cache_for(path));
    return httpd_resp_send(r, (const char *)start, (ssize_t)(end - start));
}

// ------------------------------------------------------------------
// Обработчики оболочки
// ------------------------------------------------------------------
static esp_err_t h_index(httpd_req_t *r) {
    return send_embedded(r, "/index.html", "text/html; charset=utf-8",
                         index_html_start, index_html_end);
}
static esp_err_t h_appjs(httpd_req_t *r) {
    return send_embedded(r, "/app.js", "application/javascript; charset=utf-8",
                         app_js_start, app_js_end);
}
static esp_err_t h_css(httpd_req_t *r) {
    return send_embedded(r, "/style.css", "text/css; charset=utf-8",
                         style_css_start, style_css_end);
}
static esp_err_t h_charts(httpd_req_t *r) {
    return send_embedded(r, "/charts.js", "application/javascript; charset=utf-8",
                         charts_js_start, charts_js_end);
}
static esp_err_t h_apijs(httpd_req_t *r) {
    return send_embedded(r, "/api.js", "application/javascript; charset=utf-8",
                         api_js_start, api_js_end);
}
static esp_err_t h_wsjs(httpd_req_t *r) {
    return send_embedded(r, "/ws.js", "application/javascript; charset=utf-8",
                         ws_js_start, ws_js_end);
}
static esp_err_t h_recipes(httpd_req_t *r) {
    return send_embedded(r, "/recipes.js", "application/javascript; charset=utf-8",
                         recipes_js_start, recipes_js_end);
}
static esp_err_t h_settings(httpd_req_t *r) {
    return send_embedded(r, "/settings.js", "application/javascript; charset=utf-8",
                         settings_js_start, settings_js_end);
}
static esp_err_t h_manifest(httpd_req_t *r) {
    return send_embedded(r, "/manifest.json", "application/manifest+json; charset=utf-8",
                         manifest_start, manifest_end);
}

// Service Worker обязан отдаваться с корня scope и без агрессивного кэша,
// иначе браузер не увидит новую версию после OTA.
static esp_err_t h_sw(httpd_req_t *r) {
    set_security_headers(r);
    httpd_resp_set_type(r, "application/javascript; charset=utf-8");
    httpd_resp_set_hdr(r, "Cache-Control", "no-cache");
    httpd_resp_set_hdr(r, "Service-Worker-Allowed", "/");
    return httpd_resp_send(r, (const char *)sw_js_start,
                           (ssize_t)(sw_js_end - sw_js_start));
}

static esp_err_t h_offline(httpd_req_t *r) {
    return send_embedded(r, "/offline.html", "text/html; charset=utf-8",
                         offline_html_start, offline_html_end);
}
static esp_err_t h_icon192(httpd_req_t *r) {
    return send_embedded(r, "/icons/icon-192.png", "image/png",
                         icon_192_png_start, icon_192_png_end);
}
static esp_err_t h_icon512(httpd_req_t *r) {
    return send_embedded(r, "/icons/icon-512.png", "image/png",
                         icon_512_png_start, icon_512_png_end);
}
static esp_err_t h_icon_maskable(httpd_req_t *r) {
    return send_embedded(r, "/icons/icon-maskable-512.png", "image/png",
                         icon_maskable_512_png_start, icon_maskable_512_png_end);
}
static esp_err_t h_apple_icon(httpd_req_t *r) {
    return send_embedded(r, "/icons/apple-touch-icon.png", "image/png",
                         apple_touch_icon_png_start, apple_touch_icon_png_end);
}

// Публичный сертификат: нужен, чтобы установить его в доверенные на
// телефоне. Приватный ключ (key.pem) не отдаётся ни при каких условиях.
static esp_err_t h_cert(httpd_req_t *r) {
    const char *pem = https_server_cert_pem();
    if (!pem) {
        httpd_resp_send_err(r, HTTPD_404_NOT_FOUND, "HTTPS is not running");
        return ESP_FAIL;
    }
    set_security_headers(r);
    httpd_resp_set_type(r, "application/x-pem-file");
    httpd_resp_set_hdr(r, "Cache-Control", "no-store");
    httpd_resp_set_hdr(r, "Content-Disposition",
                       "attachment; filename=\"automatika-cert.pem\"");
    return httpd_resp_sendstr(r, pem);
}

// ------------------------------------------------------------------
// LittleFS: пользовательские файлы из /lfs/www, затем /lfs.
// Регистрируется последним как "/*" и не перекрывает встроенные ассеты.
// ------------------------------------------------------------------
static const char *mime_for(const char *path) {
    const char *dot = strrchr(path, '.');
    if (!dot) return "application/octet-stream";
    if (!strcasecmp(dot, ".html") || !strcasecmp(dot, ".htm")) return "text/html; charset=utf-8";
    if (!strcasecmp(dot, ".js"))   return "application/javascript; charset=utf-8";
    if (!strcasecmp(dot, ".css"))  return "text/css; charset=utf-8";
    if (!strcasecmp(dot, ".json")) return "application/json; charset=utf-8";
    if (!strcasecmp(dot, ".png"))  return "image/png";
    if (!strcasecmp(dot, ".jpg") || !strcasecmp(dot, ".jpeg")) return "image/jpeg";
    if (!strcasecmp(dot, ".svg"))  return "image/svg+xml";
    if (!strcasecmp(dot, ".ico"))  return "image/x-icon";
    if (!strcasecmp(dot, ".txt"))  return "text/plain; charset=utf-8";
    return "application/octet-stream";
}

static bool uri_safe(const char *uri) {
    if (!uri || uri[0] != '/') return false;
    if (strstr(uri, "..") || strchr(uri, '\\') || strchr(uri, '\r') ||
        strchr(uri, '\n')) return false;
    return strlen(uri) < 96;
}

static bool uri_private(const char *uri) {
    // Служебные файлы littlefs не отдаём через статический обработчик:
    // только через API с ключом. Особенно key.pem и backup.json.
    return strncmp(uri, "/profiles", 9) == 0 ||
           strncmp(uri, "/scripts", 8) == 0 ||
           strcmp(uri, "/backup.json") == 0 ||
           strstr(uri, ".pem") != NULL ||
           strcmp(uri, "/key.pem") == 0 ||
           strcmp(uri, "/cert.pem") == 0;
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

    if (!uri_safe(uri) || !flash_fs_ready() || uri_private(uri)) {
        httpd_resp_send_err(r, HTTPD_404_NOT_FOUND, "Not found");
        return ESP_FAIL;
    }

    if (uri[0] == '/' && uri[1] == '\0')
        strcpy(uri, "/index.html");

    char path[160];
    snprintf(path, sizeof(path), LFS_MOUNT_POINT "/www%s", uri);

    // Отдаём только содержимое /lfs/www. Файлы в корне /lfs (профили,
    // скрипты, резервные копии, сертификаты) не должны быть доступны
    // статикой: для них есть API с X-API-Key.
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) {
        httpd_resp_send_err(r, HTTPD_404_NOT_FOUND, "Not found");
        return ESP_FAIL;
    }

    FILE *f = fopen(path, "r");
    if (!f) {
        httpd_resp_send_err(r, HTTPD_404_NOT_FOUND, "Not found");
        return ESP_FAIL;
    }

    set_security_headers(r);
    httpd_resp_set_type(r, mime_for(path));
    httpd_resp_set_hdr(r, "Cache-Control", cache_for(path));

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
// HTTP -> HTTPS (только при WEB_HTTPS_REDIRECT=1).
// Обработчик регистрируется как fallback: конкретные /api/* хендлеры
// имеют приоритет, поэтому API и Telegram-webhook не ломаются.
// ------------------------------------------------------------------
static esp_err_t h_https_redirect(httpd_req_t *r) {
    char host[80];
    if (httpd_req_get_hdr_value_str(r, "Host", host, sizeof(host)) != ESP_OK) {
        strncpy(host, WEB_CERT_CN, sizeof(host) - 1);
        host[sizeof(host) - 1] = '\0';
    }

    // Отрезаем :80, если браузер прислал его явно.
    char *colon = strchr(host, ':');
    if (colon && strchr(colon + 1, ':') == NULL) *colon = '\0';

    char url[192];
    snprintf(url, sizeof(url), "https://%s%s", host, r->uri);
    httpd_resp_set_status(r, "301 Moved Permanently");
    httpd_resp_set_hdr(r, "Location", url);
    httpd_resp_set_hdr(r, "Cache-Control", "no-store");
    return httpd_resp_send(r, NULL, 0);
}

// ------------------------------------------------------------------
// Регистрация обработчиков. Статика и API разделены: при включённом
// WEB_HTTPS_REDIRECT статику на HTTP не регистрируем вообще, иначе
// catch-all редирект никогда не сработает — конкретные обработчики имеют
// приоритет над "/*".
// ------------------------------------------------------------------
static void register_static(httpd_handle_t srv) {
    httpd_uri_t uris[] = {
        { .uri="/",              .method=HTTP_GET, .handler=h_index },
        { .uri="/index.html",    .method=HTTP_GET, .handler=h_index },
        { .uri="/app.js",        .method=HTTP_GET, .handler=h_appjs },
        { .uri="/style.css",     .method=HTTP_GET, .handler=h_css },
        { .uri="/charts.js",     .method=HTTP_GET, .handler=h_charts },
        { .uri="/api.js",        .method=HTTP_GET, .handler=h_apijs },
        { .uri="/ws.js",         .method=HTTP_GET, .handler=h_wsjs },
        { .uri="/recipes.js",    .method=HTTP_GET, .handler=h_recipes },
        { .uri="/settings.js",   .method=HTTP_GET, .handler=h_settings },
        { .uri="/manifest.json", .method=HTTP_GET, .handler=h_manifest },
        { .uri="/sw.js",         .method=HTTP_GET, .handler=h_sw },
        { .uri="/offline.html",  .method=HTTP_GET, .handler=h_offline },
        { .uri="/icons/icon-192.png",          .method=HTTP_GET, .handler=h_icon192 },
        { .uri="/icons/icon-512.png",          .method=HTTP_GET, .handler=h_icon512 },
        { .uri="/icons/icon-maskable-512.png", .method=HTTP_GET, .handler=h_icon_maskable },
        { .uri="/icons/apple-touch-icon.png",  .method=HTTP_GET, .handler=h_apple_icon },
        { .uri="/apple-touch-icon.png",        .method=HTTP_GET, .handler=h_apple_icon },
        { .uri="/apple-touch-icon-precomposed.png", .method=HTTP_GET, .handler=h_apple_icon },
    };
    for (size_t i = 0; i < sizeof(uris)/sizeof(*uris); i++) {
        esp_err_t err = httpd_register_uri_handler(srv, &uris[i]);
        if (err != ESP_OK)
            ESP_LOGW(TAG, "register %s failed: %s", uris[i].uri, esp_err_to_name(err));
    }
}

// Сертификат нужен и по HTTP: его скачивают до того, как браузер начнёт
// доверять HTTPS. Поэтому регистрируется независимо от редиректа.
static void register_cert(httpd_handle_t srv) {
    httpd_uri_t u = { .uri="/cert.pem", .method=HTTP_GET, .handler=h_cert };
    esp_err_t err = httpd_register_uri_handler(srv, &u);
    if (err != ESP_OK)
        ESP_LOGW(TAG, "register /cert.pem failed: %s", esp_err_to_name(err));
}

static void register_api(httpd_handle_t srv) {
    // Все обработчики /api/* проверяют X-API-Key и rate limit внутри себя;
    // actuator_test_register() дополнительно требует сервисный режим.
    web_api_register(srv);
    web_alice_register(srv);
    actuator_test_register(srv);
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
    // Обработчиков стало больше (PWA + HTTPS-дубль), поэтому запас по слотам.
    cfg.max_uri_handlers = 96;
    cfg.max_open_sockets = 5;
    cfg.stack_size = 8192;
    cfg.lru_purge_enable = true;

    if (httpd_start(&s_srv, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "httpd start failed");
        vTaskDelete(NULL);
        return;
    }

    // PWA требует HTTPS. Если сертификат/сервер не поднялись — работаем
    // только по HTTP: веб-интерфейс и API остаются доступны.
#if WEB_HTTPS_ENABLE
    if (https_server_start(&s_https) != ESP_OK) {
        s_https = NULL;
        ESP_LOGW(TAG, "HTTPS unavailable — PWA install disabled, HTTP only");
    }
#endif

    // Режим редиректа имеет смысл только при живом HTTPS.
    bool redirect = (WEB_HTTPS_REDIRECT != 0) && (s_https != NULL);

    // API и сертификат доступны по обоим протоколам всегда: иначе при
    // включённом редиректе сломались бы API-клиенты и установка сертификата.
    register_api(s_srv);
    register_cert(s_srv);

    if (!redirect) {
        register_static(s_srv);
    }

    if (s_https) {
        register_static(s_https);
        register_api(s_https);
        register_cert(s_https);

        httpd_uri_t fallback_tls = { .uri="/*", .method=HTTP_GET, .handler=h_static };
        httpd_register_uri_handler(s_https, &fallback_tls);
    }

    // Catch-all для HTTP — строго последним. При включённом редиректе
    // статика уходит на HTTPS, API остаётся на своих обработчиках.
    httpd_uri_t fallback = { .uri="/*", .method=HTTP_GET,
                             .handler = redirect ? h_https_redirect : h_static };
    httpd_register_uri_handler(s_srv, &fallback);

    ESP_LOGI(TAG, "Web server started (HTTP :80%s)",
             s_https ? ", HTTPS :443" : "");
    events_log(EVENT_INFO, "Web started%s", s_https ? " (HTTPS)" : "");

    while (1) {
        WD_FEED("web");
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void web_stop(void) {
#if WEB_HTTPS_ENABLE
    https_server_stop();
    s_https = NULL;
#endif
    if (s_srv) { httpd_stop(s_srv); s_srv = NULL; }
}

httpd_handle_t web_get_handle(void) { return s_srv; }
