#include "i18n.h"
#include "config.h"

#include "events.h"
#include "esp_log.h"
#include "nvs.h"

static const char *TAG = "I18N";

static lang_t s_lang = (lang_t)I18N_DEFAULT_LANG;
static nvs_handle_t s_nvs;
static bool s_nvs_ok = false;

void i18n_init(void) {
    s_nvs_ok = (nvs_open("ui", NVS_READWRITE, &s_nvs) == ESP_OK);
    if (!s_nvs_ok) {
        ESP_LOGW(TAG, "NVS unavailable, language = %s",
                 i18n_lang_name(s_lang));
        return;
    }

    uint8_t v = (uint8_t)s_lang;
    if (nvs_get_u8(s_nvs, "lang", &v) == ESP_OK && v <= (uint8_t)LANG_EN) {
        s_lang = (lang_t)v;
    }
    ESP_LOGI(TAG, "language: %s", i18n_lang_name(s_lang));
}

void i18n_set_lang(lang_t lang) {
    if (lang != LANG_RU && lang != LANG_EN) return;
    if (s_lang == lang) return;

    s_lang = lang;
    if (s_nvs_ok) {
        nvs_set_u8(s_nvs, "lang", (uint8_t)lang);
        nvs_commit(s_nvs);
    }
    events_log(EVENT_USER_ACTION, "Language: %s", i18n_lang_name(lang));
}

lang_t i18n_get_lang(void) { return s_lang; }

const char *i18n_lang_name(lang_t lang) {
    return lang == LANG_EN ? "en" : "ru";
}

const char *tr(const char *ru, const char *en) {
    if (s_lang == LANG_EN) return en ? en : (ru ? ru : "");
    return ru ? ru : (en ? en : "");
}
