#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "core/state.h"
#include "core/events.h"
#include "core/watchdog.h"
#include "core/storage.h"
#include "core/menu_validator.h"
#include "hal/hal_i2c.h"
#include "hal/hal_1wire.h"
#include "db/db_manager.h"
#include "db/db_psram_alloc.h"
#include "ai/page_predictor.h"
#include "ai/anomaly.h"
#include "ai/tinyml_predict.h"
#include "ai/cache_manager.h"
#include "ota/fota.h"
#include <string.h>

static const char *TAG = "SELFTEST";

void selftest_run_all(void) {
    ESP_LOGI(TAG, "========== SELF TEST ==========");

    ESP_LOGI(TAG, "Heap:     %6u KB", (unsigned)(esp_get_free_heap_size() / 1024));
    ESP_LOGI(TAG, "PSRAM:    %6u KB free",
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
    ESP_LOGI(TAG, "Internal: %6u KB free",
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024));
    ESP_LOGI(TAG, "SD card:  %s", storage_sd_ready() ? "OK" : "NOT MOUNTED");

    int found = 0;
    char devlist[128] = "";
    for (uint8_t a = 1; a < 127; a++) {
        if (hal_i2c_probe(a)) {
            char buf[16]; snprintf(buf, sizeof(buf), "%02X ", a);
            strcat(devlist, buf); found++;
        }
    }
    ESP_LOGI(TAG, "I2C:      %d devices [%s]", found, devlist);

    int ds = hal_1wire_count();
    ESP_LOGI(TAG, "DS18B20:  %d sensors", ds);
    for (int i = 0; i < ds; i++) {
        ESP_LOGI(TAG, "  #%d %s: %.4f°C (noise %.3f°C)",
                 i, hal_1wire_addr_str(i),
                 hal_1wire_read(i), hal_1wire_noise_rms(i));
    }

    if (db_is_ready()) {
        ESP_LOGI(TAG, "SQLite:   %d processes", db_process_count());
        ESP_LOGI(TAG, "SQLite PSRAM: %u KB",
                 (unsigned)(db_psram_used() / 1024));
    }

    menu_validate_all();
    wd_stats_dump();

    ESP_LOGI(TAG, "AI predictor: %u samples, acc %.0f%%",
             predictor_samples(), predictor_accuracy() * 100);
    ESP_LOGI(TAG, "AI anomaly:   %u samples, err %.3f",
             anomaly_samples(), anomaly_get()->error);
    ESP_LOGI(TAG, "AI tinyml:    %u samples, MAE %.0f min",
             tiny_ml_samples(), tiny_ml_mae() / 60.0f);
    cache_dump();

    ESP_LOGI(TAG, "FOTA: %s", fota_partition_status());
    ESP_LOGI(TAG, "========== END ==========");
}