#pragma once
#include <stdbool.h>
#include "esp_err.h"

// Внутренняя файловая система (littlefs, раздел "assets") для скриптов
// Lua и служебных файлов, которые должны переживать извлечение SD.
// Монтируется в LFS_MOUNT_POINT; при ошибке система работает без неё.

esp_err_t flash_fs_init(void);
bool      flash_fs_ready(void);
