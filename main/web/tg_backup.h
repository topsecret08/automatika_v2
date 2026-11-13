#pragma once
#include "esp_err.h"
#include <stdbool.h>

// Резервная копия конфигурации в Telegram.
//
// tg_backup_request() ставит задачу в очередь (не блокирует HTTP-обработчик).
// Задача:
//   1) экспортирует конфиг в /lfs/backup.json (без секретов);
//   2) отправляет файл через Bot API sendDocument (multipart/form-data)
//      на chat_id из NVS ("telegram"/"chat").
//
// Входящий /backup в боте (приём файла и config_backup_import) выполняется
// через POST /api/config/import: бот не является файловым шлюзом, поэтому
// документ принимается штатным API с X-API-Key.

esp_err_t tg_backup_init(void);
esp_err_t tg_backup_request(void);
bool      tg_backup_busy(void);
const char *tg_backup_last_result(void);
