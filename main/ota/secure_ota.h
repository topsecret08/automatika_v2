#pragma once
#include "esp_err.h"
#include <stdbool.h>

// Безопасное OTA-обновление по URL.
//
// Отличия от fota_flash_from_url():
//   * всегда проверяет сертификат сервера через esp_crt_bundle (pinning
//     доверенных корней, а не «любой HTTPS»);
//   * запрещает http://, если OTA_ALLOW_PLAIN_HTTP == 0;
//   * пишет понятную ошибку в secure_ota_last_error();
//   * при успехе перезагружает устройство (образ проверяется bootloader'ом,
//     CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y).
//
// Подписанная OTA (secure boot v2 / CONFIG_SECURE_SIGNED_APPS_RSA_SCHEME)
// включается отдельно ключами подписи: см. README, раздел «Подписанная OTA».
// При OTA_SIGNED_ENABLED == 1 образ должен быть подписан espsecure.py,
// иначе bootloader откатится на предыдущий раздел.

esp_err_t secure_ota_start(const char *url);
bool      secure_ota_in_progress(void);
const char *secure_ota_last_error(void);
