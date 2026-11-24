#pragma once
#include "esp_err.h"
#include "esp_partition.h"
#include <stddef.h>

// Проверка подписи образа перед выставлением загрузочного раздела.
// Формат хвоста и подписи: tools/sign_firmware.py.
//   total_len  — полный размер записанного образа (включая хвост подписи).
//   image_len  — выход: размер самого образа без хвоста.
// Возвращает ESP_OK, если подпись верна; ESP_ERR_NOT_FOUND — образ без хвоста
// (не подписан); ESP_FAIL — подпись не совпала или проверка не удалась.
esp_err_t ota_signature_verify(const esp_partition_t *part, size_t total_len, size_t *image_len);
