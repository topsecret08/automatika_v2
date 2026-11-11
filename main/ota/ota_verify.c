#include "esp_ota_ops.h"
#include "esp_app_format.h"
#include <string.h>

// Проверка целостности образа
bool ota_verify_image(const uint8_t *buf, size_t len) {
    if (len < sizeof(esp_image_header_t) + sizeof(esp_app_desc_t)) return false;
    esp_app_desc_t d;
    memcpy(&d, buf + sizeof(esp_image_header_t), sizeof(d));
    return d.magic_word == ESP_APP_DESC_MAGIC_WORD;
}