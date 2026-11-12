#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>

// Экспорт/импорт пользовательской конфигурации в JSON на SD-карту.
//
// Отличия от примера в задании:
//   * импорт идёт по БЕЛОМУ СПИСКУ ключей — произвольный JSON не может
//     записать в NVS что угодно (защита от подмены чужой конфигурацией);
//   * api_key НЕ входит в бэкап по умолчанию: файл на SD — слабое место,
//     ключ переносится отдельно (экспорт с секретами — если явно нужно);
//   * импорт валидирует значения через control_config_set_* (не пишет
//     «как есть» в NVS), поэтому одни и те же проверки, что и в API.

#define CONFIG_BACKUP_MAGIC      "automatika_cfg"
#define CONFIG_BACKUP_MAX_BYTES  (8 * 1024)

esp_err_t config_backup_export(const char *path, bool include_secrets);
esp_err_t config_backup_import(const char *path);

// Краткое описание содержимого последнего файла (для ответа API).
const char *config_backup_info(void);
