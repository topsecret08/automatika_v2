#include "flash_fs.h"
#include "config.h"

#include "esp_littlefs.h"
#include "esp_log.h"

#include <sys/stat.h>

static const char *TAG = "LFS";
static bool s_ready = false;

esp_err_t flash_fs_init(void) {
    if (s_ready) return ESP_OK;

    esp_vfs_littlefs_conf_t conf = {
        .base_path = LFS_MOUNT_POINT,
        .partition_label = LFS_PARTITION_LABEL,
        .format_if_mount_failed = true,
        .dont_mount = false,
    };

    esp_err_t err = esp_vfs_littlefs_register(&conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "littlefs mount failed: %s", esp_err_to_name(err));
        return err;
    }

    s_ready = true;
    mkdir(PATH_SCRIPTS_FLASH, 0755);

    size_t total = 0, used = 0;
    esp_littlefs_info(LFS_PARTITION_LABEL, &total, &used);
    ESP_LOGI(TAG, "mounted %s: %u/%u KB used", LFS_MOUNT_POINT,
             (unsigned)(used / 1024), (unsigned)(total / 1024));
    return ESP_OK;
}

bool flash_fs_ready(void) { return s_ready; }
