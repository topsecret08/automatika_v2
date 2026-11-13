#include "storage.h"
#include "config.h"
#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "esp_log.h"
#include "esp_spiffs.h"

static bool sd_ok = false;
static bool lfs_ok = false;
static sdmmc_card_t *s_card = NULL;

// ------------------------------------------------------------------
// SD (SPI3_HOST; LCD живёт на SPI2_HOST)
// ------------------------------------------------------------------
static esp_err_t sd_mount(void) {
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI3_HOST;

    spi_bus_config_t bus = {
        .mosi_io_num = PIN_SD_MOSI,
        .miso_io_num = PIN_SD_MISO,
        .sclk_io_num = PIN_SD_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    esp_err_t bus_err = spi_bus_initialize(SPI3_HOST, &bus, SPI_DMA_CH_AUTO);
    if (bus_err != ESP_OK && bus_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE("STORAGE", "SPI3 init failed: %s", esp_err_to_name(bus_err));
        return bus_err;
    }

    sdspi_device_config_t slot = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot.gpio_cs = PIN_SD_CS;
    slot.host_id = SPI3_HOST;

    esp_vfs_fat_sdmmc_mount_config_t mnt = {
        // Не форматировать карту при сбое монтирования: иначе любая
        // неисправность/помеха стирает пользовательские данные.
        .format_if_mount_failed = false,
        .max_files = 8,
        .allocation_unit_size = 16 * 1024,
    };
    esp_err_t r = esp_vfs_fat_sdspi_mount("/sdcard", &host, &slot, &mnt, &s_card);
    sd_ok = (r == ESP_OK);
    if (!sd_ok) s_card = NULL;
    return r;
}

esp_err_t storage_remount(void) {
    if (sd_ok) {
        esp_vfs_fat_sdcard_unmount("/sdcard", s_card);
        s_card = NULL;
        sd_ok = false;
    }
    esp_err_t err = sd_mount();
    ESP_LOGI("STORAGE", "SD remount: %s", err == ESP_OK ? "OK" : "FAIL");
    return err;
}

void storage_init(void) {
    esp_err_t r = sd_mount();
    ESP_LOGI("STORAGE", "SD: %s", r == ESP_OK ? "OK" : "FAIL");

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

bool storage_sd_ready(void) { return sd_ok; }
bool storage_littlefs_ready(void) { return lfs_ok; }
sdmmc_card_t* storage_get_sd_card(void) { return s_card; }

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
