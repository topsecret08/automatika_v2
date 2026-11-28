#include "hmi_storage.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "driver/gpio.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

#include "waveshare_rgb_lcd_port.h"

static const char *TAG = "hmi_storage";

static bool s_sd_ready = false;
static sdmmc_card_t *s_card = NULL;
static bool s_spi_bus_init = false;

static esp_err_t ensure_dir(const char *path)
{
    struct stat st;
    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode) ? ESP_OK : ESP_ERR_INVALID_STATE;
    }
    if (mkdir(path, 0775) == 0) {
        return ESP_OK;
    }
    if (errno == EEXIST) {
        return ESP_OK;
    }
    ESP_LOGW(TAG, "mkdir failed: %s errno=%d", path, errno);
    return ESP_FAIL;
}

esp_err_t hmi_storage_ensure_dirs(void)
{
    if (!s_sd_ready) return ESP_ERR_INVALID_STATE;
    const char *dirs[] = {
        HMI_SD_MOUNT_POINT "/help",
        HMI_SD_MOUNT_POINT "/lessons",
        HMI_SD_MOUNT_POINT "/tests",
        HMI_SD_MOUNT_POINT "/recipes",
        HMI_SD_MOUNT_POINT "/logs",
        HMI_SD_MOUNT_POINT "/logs/events",
        HMI_SD_MOUNT_POINT "/logs/process",
    };
    for (size_t i = 0; i < sizeof(dirs) / sizeof(dirs[0]); ++i) {
        esp_err_t err = ensure_dir(dirs[i]);
        if (err != ESP_OK) return err;
    }
    return ESP_OK;
}

esp_err_t hmi_storage_init(void)
{
    if (s_sd_ready) return ESP_OK;

    /* TF card CS is CH422G EXIO4 (active low). Hold selected before SDSPI mount.
     * SDSPI driver will not toggle EXIO4; using GPIO_NUM_NC means "no GPIO CS". */
    esp_err_t exio_err = waveshare_rgb_lcd_exio_set_bits(WAVESHARE_EXIO_TF_CS, false);
    if (exio_err != ESP_OK) {
        ESP_LOGW(TAG, "CH422G EXIO4 CS assert failed: %s", esp_err_to_name(exio_err));
        /* Continue: card may still be accessible if CS is already low. */
    }

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI2_HOST;
    host.max_freq_khz = SDMMC_FREQ_DEFAULT;

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = HMI_SD_PIN_MOSI,
        .miso_io_num = HMI_SD_PIN_MISO,
        .sclk_io_num = HMI_SD_PIN_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };

    if (!s_spi_bus_init) {
        esp_err_t ret = spi_bus_initialize((spi_host_device_t)host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
        if (ret == ESP_OK || ret == ESP_ERR_INVALID_STATE) {
            s_spi_bus_init = true;
        } else {
            ESP_LOGW(TAG, "SPI bus init failed: %s", esp_err_to_name(ret));
            waveshare_rgb_lcd_exio_set_bits(WAVESHARE_EXIO_TF_CS, true);
            return ret;
        }
    }

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = GPIO_NUM_NC;
    slot_config.host_id = (spi_host_device_t)host.slot;

    esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 8,
        .allocation_unit_size = 16 * 1024,
    };

    esp_err_t ret = esp_vfs_fat_sdspi_mount(HMI_SD_MOUNT_POINT, &host, &slot_config, &mount_config, &s_card);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "SD mount failed: %s", esp_err_to_name(ret));
        s_card = NULL;
        waveshare_rgb_lcd_exio_set_bits(WAVESHARE_EXIO_TF_CS, true);
        return ret;
    }

    s_sd_ready = true;
    sdmmc_card_print_info(stdout, s_card);
    ESP_LOGI(TAG, "SD mounted at %s", HMI_SD_MOUNT_POINT);
    hmi_storage_ensure_dirs();
    return ESP_OK;
}

bool hmi_storage_sd_ready(void)
{
    return s_sd_ready;
}

esp_err_t hmi_storage_rotate_file(const char *path, size_t max_bytes, int keep)
{
    if (!s_sd_ready || !path || keep < 1) return ESP_ERR_INVALID_ARG;
    struct stat st;
    if (stat(path, &st) != 0) return ESP_OK;
    if ((size_t)st.st_size < max_bytes) return ESP_OK;

    char oldp[192];
    char newp[192];
    for (int i = keep - 1; i >= 1; --i) {
        snprintf(oldp, sizeof(oldp), "%s.%d", path, i);
        snprintf(newp, sizeof(newp), "%s.%d", path, i + 1);
        rename(oldp, newp);
    }
    snprintf(newp, sizeof(newp), "%s.1", path);
    if (rename(path, newp) != 0) {
        ESP_LOGW(TAG, "rotate failed: %s errno=%d", path, errno);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "rotated %s", path);
    return ESP_OK;
}

bool hmi_storage_append_line(const char *path, const char *line)
{
    if (!s_sd_ready || !path || !line) return false;
    FILE *f = fopen(path, "a");
    if (!f) {
        ESP_LOGW(TAG, "append open failed: %s errno=%d", path, errno);
        return false;
    }
    fputs(line, f);
    if (line[0] && line[strlen(line) - 1] != '\n') {
        fputc('\n', f);
    }
    fclose(f);
    return true;
}

size_t hmi_storage_read_file(const char *path, char *buf, size_t max_len)
{
    if (!s_sd_ready || !path || !buf || max_len == 0) return 0;
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    size_t n = fread(buf, 1, max_len - 1, f);
    buf[n] = '\0';
    fclose(f);
    return n;
}

bool hmi_storage_write_file(const char *path, const void *data, size_t len)
{
    if (!s_sd_ready || !path || (!data && len > 0)) return false;
    FILE *f = fopen(path, "w");
    if (!f) {
        ESP_LOGW(TAG, "write open failed: %s errno=%d", path, errno);
        return false;
    }
    size_t n = fwrite(data, 1, len, f);
    fclose(f);
    return n == len;
}
