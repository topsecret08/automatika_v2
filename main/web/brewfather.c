#include "brewfather.h"
#include "core/state.h"
#include "core/events.h"
#include "nvs.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include <string.h>
#include <stdlib.h>

static brewfather_config_t s_cfg;
static nvs_handle_t nvs;

esp_err_t brewfather_init(void) {
    nvs_open("brewfather", NVS_READWRITE, &nvs);
    size_t sz = sizeof(s_cfg);
    if (nvs_get_blob(nvs, "cfg", &s_cfg, &sz) != ESP_OK) memset(&s_cfg, 0, sizeof(s_cfg));
    return ESP_OK;
}

void brewfather_set_credentials(const char *uid, const char *key) {
    strncpy(s_cfg.user_id, uid, sizeof(s_cfg.user_id) - 1);
    strncpy(s_cfg.api_key, key, sizeof(s_cfg.api_key) - 1);
    s_cfg.enabled = true;
    nvs_set_blob(nvs, "cfg", &s_cfg, sizeof(s_cfg));
    nvs_commit(nvs);
}

esp_err_t brewfather_export_batch(const char *batch_name) {
    if (!s_cfg.enabled) return ESP_ERR_INVALID_STATE;
    process_state_t *s = state_get();

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "name", batch_name);
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

    esp_http_client_config_t cfg = {
        .url = "https://api.brewfather.app/v2/batches",
        .method = HTTP_METHOD_POST,
        .timeout_ms = 10000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, payload, strlen(payload));

    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);

    if (err == ESP_OK && status == 200)
        events_log(EVENT_INFO, "BrewFather export OK");
    else
        events_log(EVENT_ERROR, "BrewFather failed %d", status);

    esp_http_client_cleanup(client);
    cJSON_Delete(root);
    free(payload);
    return err;
}