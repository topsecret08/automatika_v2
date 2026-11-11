#include "calendar.h"
#include "core/events.h"
#include "core/state.h"
#include "nvs.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>

static bool s_enabled = false;
static nvs_handle_t nvs;

esp_err_t calendar_init(void) {
    nvs_open("calendar", NVS_READWRITE, &nvs);
    uint8_t en = 0;
    nvs_get_u8(nvs, "en", &en);
    s_enabled = en;
    return ESP_OK;
}

void calendar_set_enabled(bool en) {
    s_enabled = en;
    nvs_set_u8(nvs, "en", en);
    nvs_commit(nvs);
}

bool calendar_is_enabled(void) { return s_enabled; }

esp_err_t calendar_create_reminder(const char *title, const char *desc, int minutes_before) {
    if (!s_enabled) return ESP_ERR_INVALID_STATE;

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
    cJSON_AddStringToObject(root, "summary", title);
    cJSON_AddStringToObject(root, "description", desc);

    cJSON *so = cJSON_CreateObject();
    cJSON_AddStringToObject(so, "dateTime", start_iso);
    cJSON_AddItemToObject(root, "start", so);

    cJSON *eo = cJSON_CreateObject();
    cJSON_AddStringToObject(eo, "dateTime", end_iso);
    cJSON_AddItemToObject(root, "end", eo);

    char *payload = cJSON_PrintUnformatted(root);

    esp_http_client_config_t cfg = {
        .url = "https://www.googleapis.com/calendar/v3/calendars/primary/events",
        .method = HTTP_METHOD_POST,
        .timeout_ms = 10000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, payload, strlen(payload));

    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);

    if (err == ESP_OK && (status == 200 || status == 201))
        events_log(EVENT_INFO, "Calendar reminder created");

    esp_http_client_cleanup(client);
    cJSON_Delete(root);
    free(payload);
    return err;
}

esp_err_t calendar_list_upcoming(calendar_event_t *events, int max_count) {
    (void)events; (void)max_count;
    return ESP_OK;
}