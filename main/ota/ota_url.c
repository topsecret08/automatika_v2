#include "fota.h"
#include "ota_manager.h"
#include "core/events.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern fota_progress_t* fota_get_progress(void);

esp_err_t fota_flash_from_url(const char *url) {
    fota_progress_t *p = fota_get_progress();
    p->in_progress = true;
    p->failed = false;

    esp_err_t err = ota_manager_flash_url(url);
    if (err == ESP_OK) {
        p->percent = 100;
        p->in_progress = false;
        events_log(EVENT_INFO, "FOTA URL complete");
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
    } else {
        p->in_progress = false;
        p->failed = true;
        events_log(EVENT_ERROR, "FOTA URL failed");
    }
    return err;
}
