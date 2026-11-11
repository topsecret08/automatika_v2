#pragma once
#include <stdbool.h>
#include "sdmmc_cmd.h"

// Примечание: SD-монтирование выполняется в storage.c (SPI3_HOST)
// Этот файл содержит только дополнительные операции с SD.

bool hal_sd_mounted(void);
sdmmc_card_t* hal_sd_get_card(void);
bool hal_sd_file_exists(const char *path);
size_t hal_sd_file_size(const char *path);