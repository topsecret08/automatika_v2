#include "storage.h"
#include "esp_log.h"
#include "esp_spiffs.h"

static bool lfs_ok = false;

void storage_init(void) {
    // SD-карта на контроллере отключена: контроллер работает как IO/CAN-узел.
    // Пользовательские данные (help/lessons/tests/recipes/logs) хранятся на HMI SD.
    ESP_LOGI("STORAGE", "SD: disabled (controller is IO/CAN node)");

    // SPIFFS-раздел "storage" монтируется отдельно от littlefs ("assets",
    // точка /lfs), иначе два VFS не могут делить один mount point.
    esp_vfs_spiffs_conf_t lc = {
        .base_path = "/spiffs",
        .partition_label = "storage",
        .max_files = 5,
        .format_if_mount_failed = false,
    };
    lfs_ok = (esp_vfs_spiffs_register(&lc) == ESP_OK);
    ESP_LOGI("STORAGE", "SPIFFS: %s", lfs_ok ? "OK" : "FAIL");
}

bool storage_sd_ready(void) { return false; }
bool storage_littlefs_ready(void) { return lfs_ok; }

esp_err_t storage_remount(void) {
    return ESP_ERR_NOT_SUPPORTED;
}

sdmmc_card_t* storage_get_sd_card(void) { return NULL; }

bool storage_write_file(const char *path, const void *data, size_t len) {
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    size_t wr = fwrite(data, 1, len, f);
    fclose(f);
    return wr == len;
}

size_t storage_read_file(const char *path, void *buf, size_t max) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    size_t n = fread(buf, 1, max, f);
    fclose(f);
    return n;
}
