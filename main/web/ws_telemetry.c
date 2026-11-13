#include "ws_telemetry.h"
#include "config.h"

#include "core/events.h"
#include "core/estop.h"
#include "core/health.h"
#include "core/state.h"
#include "core/watchdog.h"
#include "control/ctrl_dose.h"
#include "control/ctrl_heater.h"
#include "control/ctrl_safety.h"
#include "hal/hal_1wire.h"
#include "level/level_sensor.h"
#include "modes/mode_base.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "WS_TEL";

// Клиент привязан к конкретному серверу: HTTP (80) и HTTPS (443) — это два
// разных httpd_handle_t, и httpd_ws_send_frame_async() не умеет отправлять
// кадр «через другой» сервер. Без этой привязки PWA по wss:// не получала бы
// телеметрию, как только к HTTP подключён хотя бы один клиент.
typedef struct {
    int             fd;
    httpd_handle_t  srv;
} ws_client_t;

static char              s_uri[32] = WS_TELEMETRY_PATH;
static SemaphoreHandle_t s_mtx = NULL;
static ws_client_t       s_clients[WS_TELEMETRY_REGISTERED_MAX];

static void registry_reset(void) {
    for (int i = 0; i < WS_TELEMETRY_REGISTERED_MAX; i++) {
        s_clients[i].fd = -1;
        s_clients[i].srv = NULL;
    }
}

esp_err_t ws_telemetry_register(httpd_handle_t srv, const char *uri) {
    if (!srv) return ESP_ERR_INVALID_ARG;

    if (!s_mtx) {
        s_mtx = xSemaphoreCreateMutex();
        if (!s_mtx) return ESP_ERR_NO_MEM;
        registry_reset();
    }
    if (uri && uri[0]) {
        strncpy(s_uri, uri, sizeof(s_uri) - 1);
        s_uri[sizeof(s_uri) - 1] = '\0';
    }

    return ESP_OK;
}

static void add_client(httpd_handle_t srv, int fd) {
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    for (int i = 0; i < WS_TELEMETRY_REGISTERED_MAX; i++) {
        if (s_clients[i].fd == fd && s_clients[i].srv == srv) {
            xSemaphoreGive(s_mtx);      /* уже зарегистрирован */
            return;
        }
    }
    for (int i = 0; i < WS_TELEMETRY_REGISTERED_MAX; i++) {
        if (s_clients[i].fd < 0) {
            s_clients[i].fd = fd;
            s_clients[i].srv = srv;
            xSemaphoreGive(s_mtx);
            ESP_LOGI(TAG, "client %d connected", fd);
            return;
        }
    }
    xSemaphoreGive(s_mtx);
    ESP_LOGW(TAG, "too many telemetry clients, rejecting %d", fd);
}

static void remove_client(httpd_handle_t srv, int fd) {
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    for (int i = 0; i < WS_TELEMETRY_REGISTERED_MAX; i++) {
        if (s_clients[i].fd == fd && s_clients[i].srv == srv) {
            s_clients[i].fd = -1;
            s_clients[i].srv = NULL;
        }
    }
    xSemaphoreGive(s_mtx);
    ESP_LOGI(TAG, "client %d disconnected", fd);
}

int ws_telemetry_client_count(void) {
    if (!s_mtx) return 0;
    int n = 0;
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    for (int i = 0; i < WS_TELEMETRY_REGISTERED_MAX; i++)
        if (s_clients[i].fd >= 0) n++;
    xSemaphoreGive(s_mtx);
    return n;
}

void ws_telemetry_push(const char *json) {
    if (!s_mtx || !json) return;

    httpd_ws_frame_t frm = {
        .type = HTTPD_WS_TYPE_TEXT,
        .payload = (uint8_t *)json,
        .len = strlen(json),
    };

    xSemaphoreTake(s_mtx, portMAX_DELAY);
    for (int i = 0; i < WS_TELEMETRY_REGISTERED_MAX; i++) {
        int fd = s_clients[i].fd;
        httpd_handle_t srv = s_clients[i].srv;
        if (fd < 0 || !srv) continue;

        esp_err_t err = httpd_ws_send_frame_async(srv, fd, &frm);
        if (err != ESP_OK) {
            /* сокет закрыт/мёртв: не даём fd переехать к новому клиенту */
            s_clients[i].fd = -1;
            s_clients[i].srv = NULL;
            httpd_sess_trigger_close(srv, fd);
            ESP_LOGW(TAG, "client %d dropped: %s", fd, esp_err_to_name(err));
        }
    }
    xSemaphoreGive(s_mtx);
}

/* Формат кадра совпадает с примером из задания, плюс safety-поля:
 * {"tc":..,"ts":..,"p":..,"m":..,"sa":..,"es":..,"lv":..,"r":..} */
void ws_telemetry_push_state(void) {
    process_state_t snap;
    state_snapshot(&snap);
    process_state_t *s = &snap;

    float tc = hal_1wire_read(0);
    float ts = hal_1wire_read(1);
    if (ts <= -50.0f) ts = tc;

    float lv = level_get_percent();

    char json[256];
    snprintf(json, sizeof(json),
             "{\"tc\":%.2f,\"ts\":%.2f,\"p\":%.1f,\"m\":%.2f,"
             "\"sa\":%d,\"es\":%d,\"lv\":%.0f,\"r\":%d,\"stg\":%d}",
             tc, ts, ctrl_heater_get_percent(), s->body_rate_ml_min,
             ctrl_safety_ok() ? 1 : 0, estop_is_pressed() ? 1 : 0,
             lv, s->is_running ? 1 : 0, (int)s->stage);

    ws_telemetry_push(json);
}

/* Задача телеметрии: ядро 1 (realtime), приоритет PRIO_TELEMETRY = 8.
 * Отправка кадра асинхронная — сетевой ввод-вывод делает задача web на
 * ядре 0, поэтому публикатор не блокирует safety/mode. */
void ws_telemetry_task(void *arg) {
    (void)arg;
    wd_register("ws_tel", 5000, false);

    while (1) {
        WD_FEED("ws_tel");
        if (ws_telemetry_client_count() > 0) ws_telemetry_push_state();
        vTaskDelay(pdMS_TO_TICKS(WS_TELEMETRY_PERIOD_MS));
    }
}

/* Универсальный ws-frame router для /ws/telemetry. */
static esp_err_t ws_telemetry_handler(httpd_req_t *req) {
    int fd = httpd_req_to_sockfd(req);

    if (req->method == HTTP_GET) {
        /* handshake завершён — регистрируем клиента у его сервера */
        add_client(req->handle, fd);
        return ESP_OK;
    }

    uint8_t buf[96] = {0};
    httpd_ws_frame_t frame = { .type = HTTPD_WS_TYPE_TEXT,
                               .payload = buf, .len = sizeof(buf) };
    esp_err_t err = httpd_ws_recv_frame(req, &frame, sizeof(buf));
    if (err != ESP_OK) {
        remove_client(req->handle, fd);
        return ESP_OK;
    }

    if (frame.type == HTTPD_WS_TYPE_CLOSE) {
        remove_client(req->handle, fd);
        return ESP_OK;
    }

    /* любой текстовый кадр клиента (например, "get") — немедленный ответ */
    ws_telemetry_push_state();
    return ESP_OK;
}

esp_err_t ws_telemetry_attach(httpd_handle_t srv) {
    if (!srv) return ESP_ERR_INVALID_ARG;

    esp_err_t err = ws_telemetry_register(srv, WS_TELEMETRY_PATH);
    if (err != ESP_OK) return err;

    httpd_uri_t u = {
        .uri = s_uri,
        .method = HTTP_GET,
        .handler = ws_telemetry_handler,
        .is_websocket = true,
    };
    err = httpd_register_uri_handler(srv, &u);
    if (err == ESP_OK)
        ESP_LOGI(TAG, "telemetry endpoint %s registered", s_uri);
    return err;
}
