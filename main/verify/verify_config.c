#include "verify_config.h"

#include "core/events.h"

#include "esp_log.h"
#include "nvs.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "VERIFY_CFG";

#define VERIFY_NS          "verify"
#define VERIFY_KEY_STAGE   "stage"
#define VERIFY_KEY_SHADOW  "shadow"
#define VERIFY_KEY_RUNS    "runs"

static nvs_handle_t s_nvs;
static bool s_ready;
static verify_stage_t s_stage = STAGE_SIMPLE_RELAY;
static shadow_policy_t s_shadow = SHADOW_OFF;
static uint32_t s_clean_runs[STAGE_MAX];

static uint32_t hash_name(const char *name) {
    uint32_t h = 5381;
    while (name && *name) h = ((h << 5) + h) + (uint8_t)*name++;
    return h;
}

static void module_key(const char *name, char *out, size_t max) {
    snprintf(out, max, "v_%08lx", (unsigned long)hash_name(name));
}

esp_err_t verify_config_init(void) {
    if (s_ready) return ESP_OK;

    esp_err_t err = nvs_open(VERIFY_NS, NVS_READWRITE, &s_nvs);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS verify unavailable: %s", esp_err_to_name(err));
        return err;
    }

    uint8_t u8 = 0;
    if (nvs_get_u8(s_nvs, VERIFY_KEY_SHADOW, &u8) == ESP_OK &&
        u8 <= SHADOW_STEP) {
        s_shadow = (shadow_policy_t)u8;
    }
    u8 = 0;
    if (nvs_get_u8(s_nvs, VERIFY_KEY_STAGE, &u8) == ESP_OK &&
        u8 < STAGE_MAX) {
        s_stage = (verify_stage_t)u8;
    }

    size_t len = sizeof(s_clean_runs);
    if (nvs_get_blob(s_nvs, VERIFY_KEY_RUNS, s_clean_runs, &len) != ESP_OK ||
        len != sizeof(s_clean_runs)) {
        memset(s_clean_runs, 0, sizeof(s_clean_runs));
    }

    s_ready = true;
    ESP_LOGI(TAG, "stage=%s shadow=%s", verify_stage_name(s_stage),
             verify_shadow_name(s_shadow));
    return ESP_OK;
}

bool verify_is_enabled(void) {
#if defined(CONFIG_VERIFY_ENABLE) && !CONFIG_VERIFY_ENABLE
    return false;
#else
    return true;
#endif
}

shadow_policy_t verify_get_shadow_policy(void) { return s_shadow; }

esp_err_t verify_set_shadow_policy(shadow_policy_t policy) {
    if (policy > SHADOW_STEP) return ESP_ERR_INVALID_ARG;
    if (!s_ready) return ESP_ERR_INVALID_STATE;

    s_shadow = policy;
    esp_err_t err = nvs_set_u8(s_nvs, VERIFY_KEY_SHADOW, (uint8_t)policy);
    if (err == ESP_OK) err = nvs_commit(s_nvs);

    events_log(EVENT_USER_ACTION, "Verify shadow -> %s",
               verify_shadow_name(policy));
    return err;
}

verify_stage_t verify_get_stage(void) { return s_stage; }

esp_err_t verify_set_stage(verify_stage_t target) {
    if (target >= STAGE_MAX) return ESP_ERR_INVALID_ARG;
    if (!s_ready) return ESP_ERR_INVALID_STATE;
    if (target > s_stage + 1) {
        events_log(EVENT_WARNING, "Verify stage jump %d -> %d blocked",
                   (int)s_stage, (int)target);
        return ESP_ERR_INVALID_STATE;
    }
    if (target == s_stage + 1 &&
        s_clean_runs[s_stage] < VERIFY_MIN_CLEAN_RUNS_PER_STAGE) {
        events_log(EVENT_WARNING,
                   "Verify stage %s needs %d clean runs (have %u)",
                   verify_stage_name(s_stage), VERIFY_MIN_CLEAN_RUNS_PER_STAGE,
                   (unsigned)s_clean_runs[s_stage]);
        return ESP_ERR_INVALID_STATE;
    }

    s_stage = target;
    esp_err_t err = nvs_set_u8(s_nvs, VERIFY_KEY_STAGE, (uint8_t)target);
    if (err == ESP_OK) err = nvs_commit(s_nvs);
    events_log(EVENT_USER_ACTION, "Verify stage -> %s",
               verify_stage_name(target));
    return err;
}

bool verify_is_stage_enabled(verify_stage_t stage) {
    return stage < STAGE_MAX && s_stage >= stage;
}

void verify_report_clean_run(void) {
    if (!s_ready || s_stage >= STAGE_MAX) return;
    if (s_clean_runs[s_stage] < 255) s_clean_runs[s_stage]++;

    nvs_set_blob(s_nvs, VERIFY_KEY_RUNS, s_clean_runs, sizeof(s_clean_runs));
    nvs_commit(s_nvs);
    ESP_LOGI(TAG, "clean run for %s: %u", verify_stage_name(s_stage),
             (unsigned)s_clean_runs[s_stage]);
}

uint32_t verify_get_clean_runs(verify_stage_t stage) {
    return (stage < STAGE_MAX) ? s_clean_runs[stage] : 0;
}

bool verify_is_module_verified(const char *module_name) {
    if (!module_name || !s_ready) return false;

    char key[16];
    module_key(module_name, key, sizeof(key));
    uint8_t v = 0;
    return nvs_get_u8(s_nvs, key, &v) == ESP_OK && v != 0;
}

esp_err_t verify_mark_module_verified(const char *module_name) {
    if (!module_name || !module_name[0]) return ESP_ERR_INVALID_ARG;
    if (!s_ready) return ESP_ERR_INVALID_STATE;

    char key[16];
    module_key(module_name, key, sizeof(key));
    esp_err_t err = nvs_set_u8(s_nvs, key, 1);
    if (err == ESP_OK) err = nvs_commit(s_nvs);
    events_log(EVENT_USER_ACTION, "Verify module verified: %s", module_name);
    return err;
}

esp_err_t verify_clear_module_verified(const char *module_name) {
    if (!module_name || !module_name[0]) return ESP_ERR_INVALID_ARG;
    if (!s_ready) return ESP_ERR_INVALID_STATE;

    char key[16];
    module_key(module_name, key, sizeof(key));
    esp_err_t err = nvs_set_u8(s_nvs, key, 0);
    if (err == ESP_OK) err = nvs_commit(s_nvs);
    return err;
}

const char *verify_stage_name(verify_stage_t stage) {
    switch (stage) {
    case STAGE_SIMPLE_RELAY:      return "simple_relay";
    case STAGE_PID_MANUAL:        return "pid_manual";
    case STAGE_PID_AUTOTUNE:      return "pid_autotune";
    case STAGE_HEADS_DETECTOR:    return "heads_detector";
    case STAGE_BODY_TAILS_DETECT: return "body_tails_detector";
    case STAGE_ALCOHOL_BALANCE:   return "alcohol_balance";
    case STAGE_PREDICTIVE:        return "predictive";
    default:                      return "?";
    }
}

const char *verify_shadow_name(shadow_policy_t policy) {
    switch (policy) {
    case SHADOW_MANUAL: return "manual";
    case SHADOW_AUTO:   return "auto";
    case SHADOW_STEP:   return "step";
    case SHADOW_OFF:
    default:            return "off";
    }
}
