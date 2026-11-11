#include "fota.h"
#include "core/events.h"
// Заглушка: интеграция с QR
esp_err_t ota_flash_from_qr(const char *qr_payload) {
    return fota_flash_from_url(qr_payload);
}