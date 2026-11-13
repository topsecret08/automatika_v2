#pragma once
#include "esp_err.h"
#include "esp_ota_ops.h"
#include "config.h"
#include <stdbool.h>

// Отказоустойчивое обновление (anti-brick).
//
// Схема:
//   1. New image загружается в ota_0/ota_1; загрузчик помечает её
//      ESP_OTA_IMG_PENDING_VERIFY (CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE).
//   2. ota_manager_init() в app_main фиксирует состояние и запускает окно
//      проверки: OTA_CONFIRM_WINDOW_MS.
//   3. Задача ota_manager_task() в течение окна проверяет ЗДОРОВЬЕ системы
//      (health_check_healthy(), ctrl_safety_ok(), E-STOP, БД, watchdog).
//      Если всё чисто OTA_CONFIRM_STABLE_MS подряд — образ фиксируется
//      (esp_ota_mark_app_valid_cancel_rollback()).
//   4. Если здоровье не подтвердилось — esp_ota_mark_app_invalid_rollback_and_reboot():
//      загрузчик вернётся на предыдущий рабочий раздел.
//
// Отличие от примера в задании: образ НЕ подтверждается через 3–30 с
// безусловно — только по результату реальной проверки здоровья.

#define OTA_CONFIRM_WINDOW_MS 60000u   /* время на проверку после старта */
#define OTA_CONFIRM_STABLE_MS OTA_AUTO_CONFIRM_MS /* непрерывное «здоровое» окно */

typedef enum {
    OTA_STATE_IDLE = 0,
    OTA_STATE_PENDING,       /* новая прошивка ждёт проверки */
    OTA_STATE_CONFIRMED,     /* образ зафиксирован */
    OTA_STATE_ROLLBACK,      /* будет откат */
} ota_state_t;

esp_err_t    ota_manager_init(void);
void         ota_manager_task(void *arg);

// Явное подтверждение (например, из меню «Прошивка → Подтвердить»).
esp_err_t    ota_manager_confirm_running_app(void);

// Обновление по URL (HTTP/HTTPS), асинхронно через задачу «ota».
// Возвращает ESP_OK, если задача запущена.
esp_err_t    ota_manager_start_update(const char *firmware_url);

ota_state_t  ota_manager_state(void);
const char  *ota_manager_state_name(void);
bool         ota_manager_update_running(void);
const char  *ota_manager_last_error(void);
const char  *ota_manager_status_json(void);
