#pragma once
#include "esp_err.h"
#include "config.h"
#include <stdbool.h>
#include <stddef.h>

// Профили/рецепты в LittleFS: /lfs/profiles/<name>.json.
//
// Имя файла — только [A-Za-z0-9_-], до 31 символа (защита от path traversal).
// JSON сохраняется как есть (cJSON на стороне API), максимальный размер
// ограничен PROFILES_MAX_BYTES.

#define PROFILES_DIR       LFS_MOUNT_POINT "/profiles"
#define PROFILES_MAX_BYTES (8 * 1024)
#define PROFILES_MAX_NAME  32
#define PROFILES_MAX_COUNT 32

esp_err_t profiles_init(void);

// Список имён без расширения. Возвращает количество (0..max).
int profiles_list(char names[][PROFILES_MAX_NAME], int max);

esp_err_t profiles_save(const char *name, const char *json);
// Читает файл в buf (NUL-терминирован). Возвращает ESP_ERR_NOT_FOUND и т.п.
esp_err_t profiles_load(const char *name, char *buf, size_t max);
esp_err_t profiles_delete(const char *name);

bool profile_name_valid(const char *name);
