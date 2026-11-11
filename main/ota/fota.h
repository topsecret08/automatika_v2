#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    FOTA_SRC_SD = 0, FOTA_SRC_USB, FOTA_SRC_HTTP, FOTA_SRC_HTTPS, FOTA_SRC_URL_QR,
} fota_source_t;

typedef struct {
    fota_source_t source;
    const char *path;
    const char *version;
} fota_config_t;

typedef struct {
    uint32_t total_bytes, written_bytes;
    uint8_t  percent;
    bool     in_progress, failed;
    char     error[64];
} fota_progress_t;

esp_err_t fota_init(void);
esp_err_t fota_check_version(const char *path_or_url, char *out, size_t max);
esp_err_t fota_flash(const fota_config_t *cfg);
void      fota_task(void *arg);
fota_progress_t* fota_get_progress(void);
const char*      fota_current_version(void);
const char*      fota_running_version(void);
const char*      fota_partition_status(void);
esp_err_t fota_rollback(void);
esp_err_t fota_mark_valid(void);
bool      fota_is_pending_verify(void);
esp_err_t fota_flash_from_sd(const char *path);
esp_err_t fota_list_sd_images(char names[][64], int max);
esp_err_t fota_flash_from_url(const char *url);