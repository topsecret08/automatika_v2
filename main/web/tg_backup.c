#include "tg_backup.h"
#include "telegram_bot.h"

#include "core/config_backup.h"
#include "core/core_pinning.h"
#include "core/events.h"
#include "core/watchdog.h"

#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static const char *TAG = "TG_BACKUP";

#define BACKUP_PATH "/lfs/backup.json"
#define BOUNDARY    "----AutomatikaBackup7f3a"
#define RESP_MAX    256

static QueueHandle_t s_q = NULL;
static volatile bool s_busy = false;
static char s_result[128] = "idle";

const char *tg_backup_last_result(void) { return s_result; }
bool tg_backup_busy(void) { return s_busy; }

static esp_err_t read_file(const char *path, char **out, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) return ESP_ERR_NOT_FOUND;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return ESP_FAIL; }
    long sz = ftell(f);
    if (sz <= 0 || sz > CONFIG_BACKUP_MAX_BYTES) { fclose(f); return ESP_ERR_INVALID_SIZE; }
    rewind(f);

    char *buf = malloc((size_t)sz);
    if (!buf) { fclose(f); return ESP_ERR_NO_MEM; }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    if (rd != (size_t)sz) { free(buf); return ESP_FAIL; }

    *out = buf;
    *out_len = rd;
    return ESP_OK;
}

static bool send_document(void) {
    char token[128] = "";
    char chat[32] = "";
    telegram_get_credentials(token, sizeof(token), chat, sizeof(chat));
    if (!token[0] || !chat[0]) {
        snprintf(s_result, sizeof(s_result), "telegram not configured");
        return false;
    }

    // 1. Свежий экспорт без секретов.
    esp_err_t err = config_backup_export(BACKUP_PATH, false);
    if (err != ESP_OK) {
        snprintf(s_result, sizeof(s_result), "export failed: %s", esp_err_to_name(err));
        return false;
    }

    char *file = NULL;
    size_t file_len = 0;
    err = read_file(BACKUP_PATH, &file, &file_len);
    if (err != ESP_OK) {
        snprintf(s_result, sizeof(s_result), "read failed: %s", esp_err_to_name(err));
        return false;
    }

    char url[192];
    snprintf(url, sizeof(url), "https://api.telegram.org/bot%s/sendDocument", token);

    char pre_chat[192];
    snprintf(pre_chat, sizeof(pre_chat),
             "--" BOUNDARY "\r\n"
             "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n"
             "%s\r\n", chat);

    char pre_file[256];
    snprintf(pre_file, sizeof(pre_file),
             "--" BOUNDARY "\r\n"
             "Content-Disposition: form-data; name=\"document\"; "
             "filename=\"automatika_backup.json\"\r\n"
             "Content-Type: application/json\r\n\r\n");

    char post_file[64];
    snprintf(post_file, sizeof(post_file), "\r\n--" BOUNDARY "--\r\n");

    size_t content_len = strlen(pre_chat) + strlen(pre_file) + file_len +
                         strlen(post_file);

    esp_http_client_config_t cfg = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 20000,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .buffer_size = 2048,
    };
    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    if (!c) {
        free(file);
        snprintf(s_result, sizeof(s_result), "http client alloc failed");
        return false;
    }

    char ctype[96];
    snprintf(ctype, sizeof(ctype),
             "multipart/form-data; boundary=" BOUNDARY);
    esp_http_client_set_header(c, "Content-Type", ctype);

    bool ok = false;
    if (esp_http_client_open(c, (int)content_len) == ESP_OK) {
        bool wr_ok = true;
        wr_ok &= esp_http_client_write(c, pre_chat, strlen(pre_chat)) > 0;
        wr_ok &= esp_http_client_write(c, pre_file, strlen(pre_file)) > 0;
        wr_ok &= esp_http_client_write(c, file, file_len) > 0;
        wr_ok &= esp_http_client_write(c, post_file, strlen(post_file)) > 0;

        if (wr_ok) {
            esp_http_client_fetch_headers(c);
            int status = esp_http_client_get_status_code(c);
            ok = (status == 200);

            if (ok) {
                snprintf(s_result, sizeof(s_result), "sent %u bytes",
                         (unsigned)file_len);
            } else {
                char resp[RESP_MAX] = "";
                int rd = esp_http_client_read(c, resp, sizeof(resp) - 1);
                if (rd > 0) {
                    resp[rd] = '\0';
                    snprintf(s_result, sizeof(s_result), "HTTP %d: %.80s",
                             status, resp);
                } else {
                    snprintf(s_result, sizeof(s_result), "HTTP %d", status);
                }
            }
        } else {
            snprintf(s_result, sizeof(s_result), "write failed");
        }
        esp_http_client_close(c);
    } else {
        snprintf(s_result, sizeof(s_result), "connect failed");
    }

    esp_http_client_cleanup(c);
    free(file);

    if (ok)
        events_log(EVENT_USER_ACTION, "Config backup sent to Telegram");
    else
        events_log(EVENT_WARNING, "Telegram backup failed: %s", s_result);

    return ok;
}

static void tg_backup_task(void *arg) {
    (void)arg;
    wd_register("tg_backup", 60000, false);

    while (1) {
        uint8_t req = 0;
        if (xQueueReceive(s_q, &req, portMAX_DELAY) == pdTRUE) {
            s_busy = true;
            WD_FEED("tg_backup");
            ESP_LOGI(TAG, "sending config backup");
            send_document();
            s_busy = false;
        }
        WD_FEED("tg_backup");
    }
}

esp_err_t tg_backup_init(void) {
    if (s_q) return ESP_OK;
    s_q = xQueueCreate(1, sizeof(uint8_t));
    if (!s_q) return ESP_ERR_NO_MEM;

    BaseType_t ok = xTaskCreatePinnedToCore(
        tg_backup_task, "tg_backup", 6144, NULL, 3, NULL, CORE_NETWORK_AI);
    if (ok != pdPASS) {
        vQueueDelete(s_q);
        s_q = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t tg_backup_request(void) {
    if (!s_q) return ESP_ERR_INVALID_STATE;
    if (s_busy) return ESP_ERR_INVALID_STATE;

    uint8_t v = 1;
    if (xQueueSend(s_q, &v, 0) != pdTRUE) {
        snprintf(s_result, sizeof(s_result), "backup already queued");
        return ESP_ERR_INVALID_STATE;
    }
    snprintf(s_result, sizeof(s_result), "queued");
    return ESP_OK;
}
