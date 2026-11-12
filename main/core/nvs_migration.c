#include "nvs_migration.h"
#include "control_config.h"
#include "events.h"

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

#include <string.h>

static const char *TAG = "NVS_MIG";

static uint8_t s_prev = 0;

static esp_err_t erase_and_reinit(void) {
    ESP_LOGW(TAG, "NVS incompatible, erasing (ALL settings are lost)");
    events_log(EVENT_WARNING, "NVS schema incompatible — storage erased");
    esp_err_t err = nvs_flash_erase();
    if (err != ESP_OK) return err;
    return nvs_flash_init();
}

/* v1 → v2: ключи переименованы в стиле control_config. */
static void migrate_v2(nvs_handle_t h) {
    uint8_t u8 = 0;
    if (nvs_get_u8(h, "mode", &u8) == ESP_OK) {
        uint8_t cur = 0;
        if (nvs_get_u8(h, "ctrl_mode", &cur) != ESP_OK)
            nvs_set_u8(h, "ctrl_mode", u8);
        nvs_erase_key(h, "mode");
        ESP_LOGI(TAG, "v2: mode → ctrl_mode");
    }
    uint32_t u32 = 0;
    if (nvs_get_u32(h, "steps_ml", &u32) == ESP_OK) {
        uint32_t cur = 0;
        if (nvs_get_u32(h, "steps_per_ml", &cur) != ESP_OK)
            nvs_set_u32(h, "steps_per_ml", u32);
        nvs_erase_key(h, "steps_ml");
        ESP_LOGI(TAG, "v2: steps_ml → steps_per_ml");
    }
}

/* v2 → v3: ключи API-аутентификации.
 * В примере из задания здесь подставлялся фиксированный
 * "AutomatikaSecret2026" — это делает ключ известным всем и не защищает
 * устройство. Мы вместо этого:
 *   - переносим старый токен навыка (если был) в skill_token;
 *   - api_key НЕ подставляем: если его нет, API закрыт до провижининга. */
static void migrate_v3(nvs_handle_t h) {
    char tmp[64];
    size_t sz = sizeof(tmp);
    if (nvs_get_str(h, "skill_token", tmp, &sz) != ESP_OK) {
        sz = sizeof(tmp);
        if (nvs_get_str(h, "alice_token", tmp, &sz) == ESP_OK && tmp[0]) {
            nvs_set_str(h, "skill_token", tmp);
            nvs_erase_key(h, "alice_token");
            ESP_LOGI(TAG, "v3: alice_token → skill_token");
        }
    }

    uint8_t svc = 0;
    if (nvs_get_u8(h, "svc", &svc) != ESP_OK)
        nvs_set_u8(h, "svc", 0);

    sz = sizeof(tmp);
    if (nvs_get_str(h, "api_key", tmp, &sz) != ESP_OK || tmp[0] == '\0') {
        ESP_LOGW(TAG, "v3: api_key not provisioned — REST API stays locked "
                      "until POST /api/config/generate-key");
    }
}

esp_err_t nvs_migration_run(void) {
    nvs_handle_t h;
    esp_err_t err = nvs_open(API_KEY_NVS_NS, NVS_READWRITE, &h);

    if (err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        err = erase_and_reinit();
        if (err != ESP_OK) return err;
        err = nvs_open(API_KEY_NVS_NS, NVS_READWRITE, &h);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open(%s): %s", API_KEY_NVS_NS, esp_err_to_name(err));
        return err;
    }

    uint8_t ver = 0;
    if (nvs_get_u8(h, "schema_ver", &ver) != ESP_OK) ver = 1;
    s_prev = ver;

    if (ver > NVS_SCHEMA_VERSION) {
        ESP_LOGW(TAG, "schema v%u is newer than firmware v%u — refusing to "
                 "write (downgrade protection)", ver, NVS_SCHEMA_VERSION);
        nvs_close(h);
        return ESP_ERR_INVALID_VERSION;
    }

    if (ver < 2) migrate_v2(h);
    if (ver < 3) migrate_v3(h);

    if (ver != NVS_SCHEMA_VERSION) {
        nvs_set_u8(h, "schema_ver", NVS_SCHEMA_VERSION);
        err = nvs_commit(h);
        ESP_LOGI(TAG, "schema v%u → v%u: %s", ver, NVS_SCHEMA_VERSION,
                 err == ESP_OK ? "ok" : esp_err_to_name(err));
        if (err == ESP_OK)
            events_log(EVENT_INFO, "NVS schema migrated v%u → v%u", ver,
                       NVS_SCHEMA_VERSION);
    } else {
        ESP_LOGI(TAG, "schema v%u (up to date)", ver);
    }

    nvs_close(h);
    return ESP_OK;
}

uint8_t nvs_migration_schema_version(void) { return NVS_SCHEMA_VERSION; }
uint8_t nvs_migration_previous_version(void) { return s_prev; }
