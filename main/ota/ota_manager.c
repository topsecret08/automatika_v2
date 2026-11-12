#include "ota_manager.h"
#include "config.h"

#include "core/events.h"
#include "core/estop.h"
#include "core/health.h"
#include "core/storage.h"
#include "core/watchdog.h"
#include "control/ctrl_safety.h"
#include "db/db_manager.h"
#include "web/telegram_notify.h"

#include "esp_app_format.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Доступ к fota_app_version() без включения заголовков ota/fota.c.
const char *fota_running_version(void);

static const char *TAG = "OTA_MGR";

static volatile ota_state_t s_state = OTA_STATE_IDLE;
static volatile bool        s_update_running = false;
static volatile int64_t     s_window_start_us = 0;
static volatile int64_t     s_stable_since_us = 0;
static volatile bool        s_confirm_done = false;
static char                 s_error[96] = "";

static void set_error(const char *msg) {
    snprintf(s_error, sizeof(s_error), "%s", msg ? msg : "");
}

static bool running_pending_verify(void) {
    const esp_partition_t *run = esp_ota_get_running_partition();
    esp_ota_img_states_t st;
    if (esp_ota_get_state_partition(run, &st) != ESP_OK) return false;
    return st == ESP_OTA_IMG_PENDING_VERIFY;
}

static bool running_partition_has_app(void) {
    const esp_partition_t *run = esp_ota_get_running_partition();
    if (!run) return false;
    esp_app_desc_t desc;
    if (esp_ota_get_partition_description(run, &desc) != ESP_OK) return false;
    return (strncmp(desc.version, "v", 1) == 0 || desc.version[0] != '\0');
}

esp_err_t ota_manager_init(void) {
    const esp_partition_t *run = esp_ota_get_running_partition();
    esp_ota_img_states_t st;
    const char *state_txt = "?";

    if (esp_ota_get_state_partition(run, &st) == ESP_OK) {
        switch (st) {
        case ESP_OTA_IMG_NEW:            state_txt = "NEW"; break;
        case ESP_OTA_IMG_PENDING_VERIFY: state_txt = "PENDING_VERIFY"; break;
        case ESP_OTA_IMG_VALID:          state_txt = "VALID"; break;
        case ESP_OTA_IMG_INVALID:        state_txt = "INVALID"; break;
        case ESP_OTA_IMG_ABORTED:        state_txt = "ABORTED"; break;
        default:                         state_txt = "UNDEFINED"; break;
        }
    }

    ESP_LOGI(TAG, "running %s (%s), version %s", run ? run->label : "?",
             state_txt, fota_running_version());

    if (running_pending_verify()) {
        s_state = OTA_STATE_PENDING;
        s_window_start_us = esp_timer_get_time();
        s_stable_since_us = 0;
        s_confirm_done = false;
        events_log(EVENT_WARNING, "OTA: new image pending verify — health window %us",
                   (unsigned)(OTA_CONFIRM_WINDOW_MS / 1000));
        telegram_notify("Прошивка обновлена, идёт самопроверка");
    } else {
        s_state = OTA_STATE_CONFIRMED;
        s_confirm_done = true;
    }
    return ESP_OK;
}

esp_err_t ota_manager_confirm_running_app(void) {
    if (s_confirm_done && s_state == OTA_STATE_CONFIRMED) return ESP_OK;

    esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
    if (err == ESP_OK) {
        s_state = OTA_STATE_CONFIRMED;
        s_confirm_done = true;
        ESP_LOGI(TAG, "image confirmed valid");
        events_log(EVENT_INFO, "OTA: image confirmed valid");
        telegram_notify("Прошивка подтверждена и успешно работает");
        return ESP_OK;
    }

    /* ESP_ERR_OTA_ROLLBACK_INVALID_STATE — образ уже VALID, это не ошибка */
    if (err == ESP_ERR_OTA_ROLLBACK_INVALID_STATE) {
        s_state = OTA_STATE_CONFIRMED;
        s_confirm_done = true;
        return ESP_OK;
    }

    set_error("confirm failed");
    ESP_LOGE(TAG, "confirm failed: %s", esp_err_to_name(err));
    events_log(EVENT_ERROR, "OTA: confirm failed %s", esp_err_to_name(err));
    return err;
}

/* Единственная точка решения: образ подтверждается ПО РЕЗУЛЬТАТУ
 * проверки здоровья, а не по таймауту. */
static bool health_gate_ok(void) {
    if (estop_is_pressed()) return false;
    if (!ctrl_safety_ok()) return false;
    if (!health_check_healthy()) return false;
    if (storage_sd_ready() && !db_is_ready()) return false;
    return true;
}

static void confirm_from_window(void) {
    int64_t now = esp_timer_get_time();

    if (health_gate_ok()) {
        if (s_stable_since_us == 0) {
            s_stable_since_us = now;
            events_log(EVENT_INFO, "OTA: health ok, confirm in %us",
                       (unsigned)(OTA_CONFIRM_STABLE_MS / 1000));
        } else if (now - s_stable_since_us >= (int64_t)OTA_CONFIRM_STABLE_MS * 1000LL) {
            ota_manager_confirm_running_app();
            return;
        }
    } else {
        s_stable_since_us = 0;
    }

    if (now - s_window_start_us > (int64_t)OTA_CONFIRM_WINDOW_MS * 1000LL) {
        if (s_stable_since_us != 0) return;   /* досчитываем стабильное окно */

        s_state = OTA_STATE_ROLLBACK;
        events_log(EVENT_EMERGENCY, "OTA: image not healthy → rollback");
        telegram_notify("Проверка прошивки не пройдена — откат на предыдущую версию");
        ESP_LOGE(TAG, "health window expired, rolling back");
        vTaskDelay(pdMS_TO_TICKS(300));
        esp_ota_mark_app_invalid_rollback_and_reboot();
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
    }
}

/* ------------------------------------------------------------------ */
/* Обновление по URL                                                   */
/* ------------------------------------------------------------------ */

static esp_err_t flash_from_url(const char *url) {
    if (!url || strncmp(url, "http", 4) != 0) return ESP_ERR_INVALID_ARG;
    if (strncmp(url, "http://", 7) == 0 && !OTA_ALLOW_PLAIN_HTTP) {
        ESP_LOGE(TAG, "plain HTTP OTA disabled; use https://");
        return ESP_ERR_INVALID_ARG;
    }

    const esp_partition_t *update = esp_ota_get_next_update_partition(NULL);
    if (!update) return ESP_ERR_NOT_FOUND;

    esp_http_client_config_t http = {
        .url = url,
        .timeout_ms = 20000,
        .keep_alive_enable = true,
        .buffer_size = 4096,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t c = esp_http_client_init(&http);
    if (!c) return ESP_ERR_NO_MEM;

    esp_err_t err = esp_http_client_open(c, 0);
    if (err != ESP_OK) {
        events_log(EVENT_ERROR, "OTA: connect failed %s", esp_err_to_name(err));
        esp_http_client_cleanup(c);
        return err;
    }

    int len = esp_http_client_fetch_headers(c);
    int status = esp_http_client_get_status_code(c);
    if (status != 200 || len <= 0) {
        events_log(EVENT_ERROR, "OTA: HTTP %d, len %d", status, len);
        esp_http_client_close(c);
        esp_http_client_cleanup(c);
        return ESP_FAIL;
    }
    if ((size_t)len > update->size) {
        events_log(EVENT_ERROR, "OTA: image %d > partition %u", len,
                   (unsigned)update->size);
        esp_http_client_close(c);
        esp_http_client_cleanup(c);
        return ESP_ERR_INVALID_SIZE;
    }

    esp_ota_handle_t ota = 0;
    err = esp_ota_begin(update, (size_t)len, &ota);
    if (err != ESP_OK) {
        esp_http_client_close(c);
        esp_http_client_cleanup(c);
        return err;
    }

    size_t buf_cap = 4096;
    char *buf = heap_caps_malloc(buf_cap, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf) buf = malloc(buf_cap);
    if (!buf) {
        esp_ota_abort(ota);
        esp_http_client_close(c);
        esp_http_client_cleanup(c);
        return ESP_ERR_NO_MEM;
    }

    int written = 0;
    uint8_t last_pct = 0;
    while (written < len) {
        int r = esp_http_client_read(c, buf, (int)buf_cap);
        if (r < 0) { err = ESP_FAIL; break; }
        if (r == 0) { err = ESP_ERR_INVALID_SIZE; break; }

        err = esp_ota_write(ota, buf, (size_t)r);
        if (err != ESP_OK) break;
        written += r;
        WD_FEED("ota");

        uint8_t pct = (uint8_t)((int64_t)written * 100 / len);
        if (pct >= last_pct + 20) {
            last_pct = pct;
            ESP_LOGI(TAG, "OTA %u %%", pct);
        }
    }

    free(buf);
    esp_http_client_close(c);
    esp_http_client_cleanup(c);

    if (err != ESP_OK || written != len) {
        esp_ota_abort(ota);
        set_error("download/write failed");
        events_log(EVENT_ERROR, "OTA: write failed after %d/%d bytes", written, len);
        telegram_notify("Обновление по URL не удалось — прошивка не изменена");
        return err != ESP_OK ? err : ESP_FAIL;
    }

    err = esp_ota_end(ota);
    if (err != ESP_OK) {
        set_error("image validation failed");
        events_log(EVENT_ERROR, "OTA: image invalid %s", esp_err_to_name(err));
        telegram_notify("Загруженный образ не прошёл проверку — прошивка не изменена");
        return err;
    }

    err = esp_ota_set_boot_partition(update);
    if (err != ESP_OK) {
        set_error("set boot partition failed");
        return err;
    }

    events_log(EVENT_INFO, "OTA: image staged (%d bytes), rebooting", len);
    telegram_notify("Обновление загружено, устройство перезагружается");
    return ESP_OK;
}

typedef struct {
    char url[192];
} ota_job_t;

static void ota_update_task(void *arg) {
    ota_job_t *job = (ota_job_t *)arg;
    wd_register("ota", 30000, true);

    esp_err_t err = flash_from_url(job->url);
    s_update_running = false;

    if (err == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(500));
        esp_restart();
    }
    free(job);
    vTaskDelete(NULL);
}

esp_err_t ota_manager_start_update(const char *firmware_url) {
    if (!firmware_url || strlen(firmware_url) >= 192) return ESP_ERR_INVALID_ARG;
    if (s_update_running) return ESP_ERR_INVALID_STATE;
    if (!ctrl_safety_ok() || estop_is_pressed()) return ESP_ERR_INVALID_STATE;

    ota_job_t *job = calloc(1, sizeof(*job));
    if (!job) return ESP_ERR_NO_MEM;
    strncpy(job->url, firmware_url, sizeof(job->url) - 1);

    s_update_running = true;
    set_error("");
    events_log(EVENT_INFO, "OTA: download started");

    if (xTaskCreatePinnedToCore(ota_update_task, "ota", 8192, job, 4,
                                NULL, 0) != pdPASS) {
        free(job);
        s_update_running = false;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

/* ------------------------------------------------------------------ */

ota_state_t ota_manager_state(void) { return s_state; }

const char *ota_manager_state_name(void) {
    switch (s_state) {
    case OTA_STATE_PENDING:   return "pending";
    case OTA_STATE_CONFIRMED: return "confirmed";
    case OTA_STATE_ROLLBACK:  return "rollback";
    default:                  return "idle";
    }
}

bool ota_manager_update_running(void) { return s_update_running; }
const char *ota_manager_last_error(void) { return s_error; }

const char *ota_manager_status_json(void) {
    static char out[320];
    const esp_partition_t *run = esp_ota_get_running_partition();
    const esp_partition_t *next = esp_ota_get_next_update_partition(NULL);

    snprintf(out, sizeof(out),
             "{\"state\":\"%s\",\"running\":\"%s\",\"next\":\"%s\","
             "\"pending\":%d,\"update\":%d,\"error\":\"%s\"}",
             ota_manager_state_name(),
             run ? run->label : "?",
             next ? next->label : "-",
             running_pending_verify() ? 1 : 0,
             s_update_running ? 1 : 0,
             s_error);
    return out;
}

void ota_manager_task(void *arg) {
    (void)arg;
    wd_register("ota", 5000, false);

    while (1) {
        WD_FEED("ota");
        if (s_state == OTA_STATE_PENDING) confirm_from_window();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
