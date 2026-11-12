#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

// Доступ к контенту на SD-карте (/sdcard/<раздел>/).
// Карта создаётся скриптом generate_sdcard.py.

#define SD_CONTENT_MAX_NAME 64

typedef enum {
    CT_HELP = 0,
    CT_RECIPES,
    CT_LESSONS,
    CT_SCHEMES,
    CT_CALC,
    CT_LANG,
    CT_IMAGES,
    CT_COUNT
} content_type_t;

esp_err_t   sd_content_init(void);
bool        sd_content_ready(void);

// Каталог раздела, например "/sdcard/recipes". Никогда не NULL.
const char* sd_content_path(content_type_t type);

// Список файлов раздела (без подкаталогов, скрытых и служебных).
// Возвращает количество имён, записанных в names.
int         sd_content_list(content_type_t type,
                            char names[][SD_CONTENT_MAX_NAME],
                            int max);

// Чтение файла раздела целиком в buf (с завершающим нулём).
bool        sd_content_read(content_type_t type, const char *name,
                            char *buf, size_t size);

size_t      sd_content_size(content_type_t type, const char *name);
