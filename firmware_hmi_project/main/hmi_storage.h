#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HMI_SD_MOUNT_POINT "/sdcard"

/* Waveshare ESP32-S3-Touch-LCD-7 TF slot (SPI, shared with CH422G EXIO4 CS). */
#define HMI_SD_PIN_MOSI    11
#define HMI_SD_PIN_CLK     12
#define HMI_SD_PIN_MISO    13

esp_err_t hmi_storage_init(void);
bool      hmi_storage_sd_ready(void);

/* Ensure base directories exist on SD (help, lessons, tests, recipes, logs). */
esp_err_t hmi_storage_ensure_dirs(void);

/* Append a line to a file on SD, creating parent dirs as needed. */
bool hmi_storage_append_line(const char *path, const char *line);

/* Read whole file into buf (NUL-terminated). Returns bytes read, 0 on error. */
size_t hmi_storage_read_file(const char *path, char *buf, size_t max_len);

/* Write whole buffer to file (overwrite). */
bool hmi_storage_write_file(const char *path, const void *data, size_t len);

#ifdef __cplusplus
}
#endif
