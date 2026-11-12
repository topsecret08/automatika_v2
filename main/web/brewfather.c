#include "brewfather.h"
#include "core/state.h"
#include "core/events.h"
#include "nvs.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"
#include "mbedtls/base64.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static brewfather_config_t s_cfg;
static nvs_handle_t nvs;

esp_err_t brewfather_init(void) {
    if (nvs_open("brewfather", NVS_READWRITE, &nvs) != ESP_OK) {
        memset(&s_cfg, 0, sizeof(s_cfg));
        return ESP_FAIL;
    }
    size_t sz = sizeof(s_cfg);
    if (nvs_get_blob(nvs, "cfg", &s_cfg, &sz) != ESP_OK) {
        memset(&s_cfg, 0, sizeof(s_cfg));
    }
    return ESP_OK;
}

void brewfather_set_credentials(const char *uid, const char *key) {
    if (uid) {
        strncpy(s_cfg.user_id, uid, sizeof(s_cfg.user_id) - 1);
        s_cfg.user_id[sizeof(s_cfg.user_id) - 1] = '\0';
    }
    if (key) {
        strncpy(s_cfg.api_key, key, sizeof(s_cfg.api_key) - 1);
        s_cfg.api_key[sizeof(s_cfg.api_key) - 1] = '\0';
    }
    s_cfg.enabled = s_cfg.user_id[0] && s_cfg.api_key[0];
    nvs_set_blob(nvs, "cfg", &s_cfg, sizeof(s_cfg));
    nvs_commit(nvs);
    events_log(EVENT_INFO, "BrewFather credentials %s",
               s_cfg.enabled ? "saved" : "cleared");
}

bool brewfather_is_configured(void) {
    return s_cfg.enabled;
}

esp_err_t brewfather_export_batch(const char *batch_name) {
    if (!s_cfg.enabled) return ESP_ERR_INVALID_STATE;
    process_state_t *s = state_get();

    cJSON *root = cJSON_CreateObject();
    if (!root) return ESP_ERR_NO_MEM;

    cJSON_AddStringToObject(root, "name", batch_name ? batch_name : "Automatika");
    cJSON_AddStringToObject(root, "status", "Brewing");
    cJSON_AddStringToObject(root, "brewer", "Automatika v2.9");
    cJSON_AddNumberToObject(root, "batchSize", 20.0f);

    cJSON *stats = cJSON_CreateObject();
    cJSON_AddNumberToObject(stats, "heads_ml", s->heads_collected_ml);
    cJSON_AddNumberToObject(stats, "body_ml", s->body_collected_ml);
    cJSON_AddNumberToObject(stats, "tails_ml", s->tails_collected_ml);
    cJSON_AddNumberToObject(stats, "duration_min", s->elapsed_sec / 60);
    cJSON_AddItemToObject(root, "processStats", stats);

    char *payload = cJSON_PrintUnformatted(root);
    if (!payload) {
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }

    // BrewFather требует HTTP Basic: user_id:api_key
    char creds[256];
    snprintf(creds, sizeof(creds), "%s:%s", s_cfg.user_id, s_cfg.api_key);

    unsigned char b64[512];
    size_t b64_len = 0;
    if (mbedtls_base64_encode(b64, sizeof(b64) - 1, &b64_len,
                              (const unsigned char*)creds, strlen(creds)) != 0) {
        events_log(EVENT_ERROR, "BrewFather: base64 encode failed");
        cJSON_Delete(root);
        free(payload);
        return ESP_FAIL;
    }
    b64[b64_len] = '\0';

    char auth[600];
    snprintf(auth, sizeof(auth), "Basic %s", (const char*)b64);

    esp_http_client_config_t cfg = {
        .url = "https://api.brewfather.app/v2/batches",
        .method = HTTP_METHOD_POST,
        .timeout_ms = 10000,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) {
        cJSON_Delete(root);
        free(payload);
        return ESP_FAIL;
    }

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Authorization", auth);
    esp_http_client_set_post_field(client, payload, (int)strlen(payload));

    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);

    if (err == ESP_OK && (status == 200 || status == 201))
        events_log(EVENT_INFO, "BrewFather export OK");
    else
        events_log(EVENT_ERROR, "BrewFather failed: err=%s HTTP %d",
                   esp_err_to_name(err), status);

    esp_http_client_cleanup(client);
    cJSON_Delete(root);
    free(payload);
    return err;
}
