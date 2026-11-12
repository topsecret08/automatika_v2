#include "calendar.h"
#include "core/events.h"
#include "core/state.h"
#include "nvs.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static bool s_enabled = false;
static char s_token[512] = "";
static nvs_handle_t nvs;

esp_err_t calendar_init(void) {
    if (nvs_open("calendar", NVS_READWRITE, &nvs) != ESP_OK) return ESP_FAIL;

    uint8_t en = 0;
    if (nvs_get_u8(nvs, "en", &en) == ESP_OK) s_enabled = en != 0;

    size_t sz = sizeof(s_token);
    if (nvs_get_str(nvs, "token", s_token, &sz) != ESP_OK) s_token[0] = '\0';

    return ESP_OK;
}

void calendar_set_enabled(bool en) {
    s_enabled = en;
    nvs_set_u8(nvs, "en", en ? 1 : 0);
    nvs_commit(nvs);
}

bool calendar_is_enabled(void) { return s_enabled; }

void calendar_set_token(const char *token) {
    if (token) {
        strncpy(s_token, token, sizeof(s_token) - 1);
        s_token[sizeof(s_token) - 1] = '\0';
    }
    nvs_set_str(nvs, "token", s_token);
    nvs_commit(nvs);
    events_log(EVENT_INFO, "Calendar token %s", s_token[0] ? "saved" : "cleared");
}

bool calendar_is_configured(void) { return s_token[0] != '\0'; }

esp_err_t calendar_create_reminder(const char *title, const char *desc, int minutes_before) {
    if (!s_enabled) return ESP_ERR_INVALID_STATE;
    if (!s_token[0]) {
        events_log(EVENT_WARNING, "Calendar: OAuth token is not configured");
        return ESP_ERR_INVALID_STATE;
    }

    if (minutes_before < 0) minutes_before = 0;
    if (minutes_before > 40320) minutes_before = 40320; // 4 недели

    time_t now = time(NULL);
    time_t start = now + 300;
    time_t end = start + 3600;

    struct tm tm_start, tm_end;
    gmtime_r(&start, &tm_start);
    gmtime_r(&end, &tm_end);

    char start_iso[32], end_iso[32];
    strftime(start_iso, sizeof(start_iso), "%Y-%m-%dT%H:%M:%SZ", &tm_start);
    strftime(end_iso, sizeof(end_iso), "%Y-%m-%dT%H:%M:%SZ", &tm_end);

    cJSON *root = cJSON_CreateObject();
    if (!root) return ESP_ERR_NO_MEM;
    cJSON_AddStringToObject(root, "summary", title ? title : "Automatika");
    cJSON_AddStringToObject(root, "description", desc ? desc : "");

    cJSON *so = cJSON_CreateObject();
    cJSON_AddStringToObject(so, "dateTime", start_iso);
    cJSON_AddStringToObject(so, "timeZone", "UTC");
    cJSON_AddItemToObject(root, "start", so);

    cJSON *eo = cJSON_CreateObject();
    cJSON_AddStringToObject(eo, "dateTime", end_iso);
    cJSON_AddStringToObject(eo, "timeZone", "UTC");
    cJSON_AddItemToObject(root, "end", eo);

    // Напоминание за minutes_before минут до начала события.
    cJSON *rem = cJSON_CreateObject();
    cJSON_AddBoolToObject(rem, "useDefault", false);
    cJSON *ovr = cJSON_CreateArray();
    cJSON *one = cJSON_CreateObject();
    cJSON_AddStringToObject(one, "method", "popup");
    cJSON_AddNumberToObject(one, "minutes", minutes_before);
    cJSON_AddItemToArray(ovr, one);
    cJSON_AddItemToObject(rem, "overrides", ovr);
    cJSON_AddItemToObject(root, "reminders", rem);

    char *payload = cJSON_PrintUnformatted(root);
    if (!payload) {
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }

    char auth[600];
    snprintf(auth, sizeof(auth), "Bearer %s", s_token);

    esp_http_client_config_t cfg = {
        .url = "https://www.googleapis.com/calendar/v3/calendars/primary/events",
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
        events_log(EVENT_INFO, "Calendar reminder created");
    else
        events_log(EVENT_WARNING, "Calendar failed: err=%s HTTP %d",
                   esp_err_to_name(err), status);

    esp_http_client_cleanup(client);
    cJSON_Delete(root);
    free(payload);
    return err;
}

esp_err_t calendar_list_upcoming(calendar_event_t *events, int max_count) {
    if (!events || max_count <= 0) return ESP_ERR_INVALID_ARG;
    if (!s_token[0]) return ESP_ERR_INVALID_STATE;

    char auth[600];
    snprintf(auth, sizeof(auth), "Bearer %s", s_token);

    esp_http_client_config_t cfg = {
        .url = "https://www.googleapis.com/calendar/v3/calendars/primary/events"
               "?maxResults=20&singleEvents=true&orderBy=startTime&timeMin=",
        .timeout_ms = 10000,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    // timeMin требует RFC3339; собираем URL вручную.
    time_t now = time(NULL);
    struct tm tmv;
    gmtime_r(&now, &tmv);
    char iso[32];
    strftime(iso, sizeof(iso), "%Y-%m-%dT%H:%M:%SZ", &tmv);

    char url[256];
    snprintf(url, sizeof(url),
             "https://www.googleapis.com/calendar/v3/calendars/primary/events"
             "?maxResults=%d&singleEvents=true&orderBy=startTime&timeMin=%s",
             max_count, iso);
    cfg.url = url;

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) return ESP_FAIL;
    esp_http_client_set_header(client, "Authorization", auth);

    // Ответ не разбираем: функция используется только для проверки связи.
    esp_err_t err = esp_http_client_perform(client);
    esp_http_client_cleanup(client);
    return err;
}
