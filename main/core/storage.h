#pragma once
#include <stdbool.h>
#include <stdio.h>
#include "sdmmc_cmd.h"
#include "esp_err.h"

void   storage_init(void);
bool   storage_sd_ready(void);
bool   storage_littlefs_ready(void);
// Перемонтировать SD после сбоя (используется sd_resilience).
esp_err_t storage_remount(void);
bool   storage_write_file(const char *path, const void *data, size_t len);
size_t storage_read_file(const char *path, void *buf, size_t max);
sdmmc_card_t* storage_get_sd_card(void);