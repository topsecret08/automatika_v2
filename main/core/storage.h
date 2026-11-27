#pragma once
#include <stdbool.h>
#include <stdio.h>
#include "sdmmc_cmd.h"
#include "esp_err.h"

void   storage_init(void);
bool   storage_sd_ready(void);
bool   storage_littlefs_ready(void);
// Перемонтировать SD. На контроллере SD отключена: возвращает ESP_ERR_NOT_SUPPORTED.
esp_err_t storage_remount(void);
// Записать файл в файловую систему хранения. Для /sdcard/... на контроллере с отключённой SD недоступно.
bool   storage_write_file(const char *path, const void *data, size_t len);
// Прочитать файл из файловой системы хранения. Для /sdcard/... на контроллере с отключённой SD недоступно.
size_t storage_read_file(const char *path, void *buf, size_t max);
// Получить дескриптор SD-карты. На контроллере возвращает NULL.
sdmmc_card_t* storage_get_sd_card(void);