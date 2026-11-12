#include "fota.h"
#include "core/events.h"
#include "esp_https_ota.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"

extern fota_progress_t* fota_get_progress(void);

esp_err_t fota_flash_from_url(const char *url) {
    fota_progress_t *p = fota_get_progress();
    p->in_progress = true;
    p->failed = false;

    esp_http_client_config_t http_cfg = {
        .url = url,
        .timeout_ms = 30000,
        .keep_alive_enable = true,
        .buffer_size = 4096,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_https_ota_config_t ota_cfg = { .http_config = &http_cfg };

    esp_err_t err = esp_https_ota(&ota_cfg);
    if (err == ESP_OK) {
        p->percent = 100;
        p->in_progress = false;
        events_log(EVENT_INFO, "FOTA URL complete");
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
    } else {
        p->failed = true;
        events_log(EVENT_ERROR, "FOTA URL failed");
    }
    return err;
}