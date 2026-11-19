#include "fota.h"
#include "core/events.h"
#include "core/storage.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_https_ota.h"
#include "esp_app_format.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <dirent.h>

static const char *TAG = "FOTA";
static fota_progress_t s_prog = {0};

extern esp_err_t fota_flash_from_sd(const char *path);
extern esp_err_t fota_flash_from_url(const char *url);

esp_err_t fota_init(void) {
    const esp_partition_t *run = esp_ota_get_running_partition();
    esp_ota_img_states_t st;
    if (esp_ota_get_state_partition(run, &st) == ESP_OK) {
        if (st == ESP_OTA_IMG_PENDING_VERIFY) {
            // Подтверждение делает только ota_manager_task по результату
            // проверки здоровья (не по таймауту).
            ESP_LOGW(TAG, "Running image PENDING_VERIFY");
        }
    }
    ESP_LOGI(TAG, "FOTA init: %s", fota_running_version());
    return ESP_OK;
}

fota_progress_t* fota_get_progress(void) { return &s_prog; }
const char* fota_current_version(void) { return fota_running_version(); }

const char* fota_running_version(void) {
    const esp_app_desc_t *d = esp_app_get_description();
    return d->version;
}

const char* fota_partition_status(void) {
    static char buf[128];
    const esp_partition_t *run = esp_ota_get_running_partition();
    const esp_partition_t *next = esp_ota_get_next_update_partition(NULL);
    esp_ota_img_states_t st;
    const char *state_s = "?";
    if (esp_ota_get_state_partition(run, &st) == ESP_OK) {
        switch (st) {
        case ESP_OTA_IMG_NEW: state_s = "NEW"; break;
        case ESP_OTA_IMG_PENDING_VERIFY: state_s = "PENDING"; break;
        case ESP_OTA_IMG_VALID: state_s = "VALID"; break;
        case ESP_OTA_IMG_INVALID: state_s = "INVALID"; break;
        case ESP_OTA_IMG_ABORTED: state_s = "ABORTED"; break;
        default: state_s = "UNDEFINED";
        }
    }
    snprintf(buf, sizeof(buf), "Running: %s (%s)\nNext: %s\nSize: %u KB",
             run->label, state_s, next ? next->label : "—",
             (unsigned)(run->size / 1024));
    return buf;
}

esp_err_t fota_check_version(const char *path_or_url, char *out, size_t max) {
    uint8_t buf[0x200];
    if (strstr(path_or_url, "http")) {
        esp_http_client_config_t cfg = {
            .url = path_or_url,
            .timeout_ms = 10000,
            .crt_bundle_attach = esp_crt_bundle_attach,
        };
        esp_http_client_handle_t c = esp_http_client_init(&cfg);
        esp_http_client_open(c, 0);
        int n = esp_http_client_read(c, (char*)buf, sizeof(buf));
        esp_http_client_close(c);
        esp_http_client_cleanup(c);
        if (n < 0x120) return ESP_FAIL;
    } else {
        FILE *f = fopen(path_or_url, "rb");
        if (!f) return ESP_FAIL;
        fread(buf, 1, sizeof(buf), f);
        fclose(f);
    }
    esp_app_desc_t d;
    memcpy(&d, buf + 0x20, sizeof(d));
    if (d.magic_word != ESP_APP_DESC_MAGIC_WORD) return ESP_FAIL;
    snprintf(out, max, "%.*s", (int)sizeof(d.version), d.version);
    return ESP_OK;
}

esp_err_t fota_rollback(void) {
    return esp_ota_mark_app_invalid_rollback_and_reboot();
}

bool fota_is_pending_verify(void) {
    const esp_partition_t *run = esp_ota_get_running_partition();
    esp_ota_img_states_t st;
    if (esp_ota_get_state_partition(run, &st) == ESP_OK)
        return st == ESP_OTA_IMG_PENDING_VERIFY;
    return false;
}

esp_err_t fota_flash(const fota_config_t *cfg) {
    if (!cfg) return ESP_FAIL;
    if (cfg->source == FOTA_SRC_SD || cfg->source == FOTA_SRC_USB)
        return fota_flash_from_sd(cfg->path);
    return fota_flash_from_url(cfg->path);
}

