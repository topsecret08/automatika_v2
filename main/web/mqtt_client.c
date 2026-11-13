#include "mqtt_app.h"
#include "core/state.h"
#include "core/flags.h"
#include "core/watchdog.h"
#include "core/events.h"
#include "control/ctrl_heater.h"
#include "control/safety_gate.h"
#include "control/ctrl_safety.h"
#include "modes/mode_base.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "MQTT";

#define MQTT_TOPIC_STATE "/automatika/state"
#define MQTT_TOPIC_CMD   "/automatika/cmd"

typedef struct {
    char uri[128];
    char user[64];
    char pass[64];
} mqtt_cfg_t;

static mqtt_cfg_t s_cfg;
static esp_mqtt_client_handle_t s_client = NULL;
static bool s_connected = false;
static bool s_started = false;
static nvs_handle_t s_nvs;

static void mqtt_load_cfg(void) {
    memset(&s_cfg, 0, sizeof(s_cfg));
    if (nvs_open("mqtt", NVS_READWRITE, &s_nvs) != ESP_OK) return;

    size_t sz = sizeof(s_cfg.uri);
    if (nvs_get_str(s_nvs, "uri", s_cfg.uri, &sz) != ESP_OK) s_cfg.uri[0] = '\0';
    sz = sizeof(s_cfg.user);
    if (nvs_get_str(s_nvs, "user", s_cfg.user, &sz) != ESP_OK) s_cfg.user[0] = '\0';
    sz = sizeof(s_cfg.pass);
    if (nvs_get_str(s_nvs, "pass", s_cfg.pass, &sz) != ESP_OK) s_cfg.pass[0] = '\0';
}

static void mqtt_publish_json(const char *json) {
    if (!s_connected || !s_client) return;
    esp_mqtt_client_publish(s_client, MQTT_TOPIC_STATE, json, 0, 0, 1);
}

static void mqtt_handle_cmd(const char *data, int len) {
    if (!data || len <= 0) return;

    char cmd[96];
    int n = (len < (int)sizeof(cmd) - 1) ? len : (int)sizeof(cmd) - 1;
    memcpy(cmd, data, (size_t)n);
    cmd[n] = '\0';

    process_state_t *st = state_get();

    if (strncmp(cmd, "start", 5) == 0) {
        if (!ctrl_safety_ok()) {
            events_log(EVENT_WARNING, "MQTT start rejected: safety not OK");
            return;
        }
        if (!mode_get()) mode_set(mode_from_type(PROCESS_DISTILL));
        st->is_running = true;
        st->is_paused = false;
        events_log(EVENT_INFO, "MQTT: start");
    } else if (strncmp(cmd, "stop", 4) == 0) {
        ctrl_heater_set_percent(0);
        if (mode_get() && mode_get()->stop) mode_get()->stop();
        st->is_running = false;
        events_log(EVENT_INFO, "MQTT: stop");
    } else if (strncmp(cmd, "pause", 5) == 0) {
        st->is_paused = true;
        events_log(EVENT_INFO, "MQTT: pause");
    } else if (strncmp(cmd, "power", 5) == 0) {
        const char *arg = cmd + 5;
        while (*arg == ' ' || *arg == ':' || *arg == '=') arg++;
        if (!ctrl_safety_ok()) {
            events_log(EVENT_WARNING, "MQTT power rejected: safety not OK");
            return;
        }
        safety_gate_apply("mqtt_power", (float)atof(arg));
        events_log(EVENT_INFO, "MQTT: power %.0f%%", ctrl_heater_get_percent());
    } else {
        events_log(EVENT_WARNING, "MQTT: unknown command '%s'", cmd);
    }
}

static void mqtt_event_handler(void *arg, esp_event_base_t base,
                               int32_t event_id, void *event_data) {
    (void)arg; (void)base;
    esp_mqtt_event_handle_t e = (esp_mqtt_event_handle_t)event_data;
    if (!e) return;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        s_connected = true;
        ESP_LOGI(TAG, "Connected to %s", s_cfg.uri);
        events_log(EVENT_INFO, "MQTT connected");
        esp_mqtt_client_subscribe(e->client, MQTT_TOPIC_CMD, 0);
        mqtt_publish_state();
        break;

    case MQTT_EVENT_DISCONNECTED:
        s_connected = false;
        ESP_LOGW(TAG, "Disconnected");
        break;

    case MQTT_EVENT_DATA:
        if (e->topic_len > 0 &&
            strncmp(e->topic, MQTT_TOPIC_CMD, (size_t)e->topic_len) == 0) {
            mqtt_handle_cmd(e->data, e->data_len);
        }
        break;

    case MQTT_EVENT_ERROR:
        s_connected = false;
        ESP_LOGE(TAG, "MQTT error");
        break;

    default:
        break;
    }
}

static bool mqtt_start_client(void) {
    if (s_started || !s_cfg.uri[0]) return false;

    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = s_cfg.uri,
        .session.keepalive = 30,
        .network.reconnect_timeout_ms = 5000,
    };
    if (s_cfg.user[0]) cfg.credentials.username = s_cfg.user;
    if (s_cfg.pass[0]) cfg.credentials.authentication.password = s_cfg.pass;

    s_client = esp_mqtt_client_init(&cfg);
    if (!s_client) {
        ESP_LOGE(TAG, "init failed");
        return false;
    }
    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID,
                                   mqtt_event_handler, NULL);
    if (esp_mqtt_client_start(s_client) != ESP_OK) {
        ESP_LOGE(TAG, "start failed");
        esp_mqtt_client_destroy(s_client);
        s_client = NULL;
        return false;
    }
    s_started = true;
    return true;
}

static void mqtt_stop_client(void) {
    if (s_client) {
        esp_mqtt_client_stop(s_client);
        esp_mqtt_client_destroy(s_client);
        s_client = NULL;
    }
    s_started = false;
    s_connected = false;
}

void mqtt_init(void) {
    mqtt_load_cfg();

    if (!flags_get_by_key("mqtt")) {
        events_log(EVENT_INFO, "MQTT disabled by flag");
        return;
    }
    if (!s_cfg.uri[0]) {
        events_log(EVENT_INFO, "MQTT: no broker URI configured");
        return;
    }
    if (mqtt_start_client()) {
        events_log(EVENT_INFO, "MQTT init: %s", s_cfg.uri);
    }
}

void mqtt_set_config(const char *uri, const char *user, const char *pass) {
    mqtt_stop_client();

    if (uri) {
        strncpy(s_cfg.uri, uri, sizeof(s_cfg.uri) - 1);
        s_cfg.uri[sizeof(s_cfg.uri) - 1] = '\0';
    }
    if (user) {
        strncpy(s_cfg.user, user, sizeof(s_cfg.user) - 1);
        s_cfg.user[sizeof(s_cfg.user) - 1] = '\0';
    }
    if (pass) {
        strncpy(s_cfg.pass, pass, sizeof(s_cfg.pass) - 1);
        s_cfg.pass[sizeof(s_cfg.pass) - 1] = '\0';
    }

    if (nvs_open("mqtt", NVS_READWRITE, &s_nvs) == ESP_OK) {
        nvs_set_str(s_nvs, "uri", s_cfg.uri);
        nvs_set_str(s_nvs, "user", s_cfg.user);
        nvs_set_str(s_nvs, "pass", s_cfg.pass);
        nvs_commit(s_nvs);
    }

    flags_set_by_key("mqtt", s_cfg.uri[0] != '\0');
    if (flags_get_by_key("mqtt")) mqtt_start_client();
}

void mqtt_publish_state(void) {
    if (!s_connected || !s_client) return;

    process_state_t *s = state_get();
    char json[256];
    snprintf(json, sizeof(json),
        "{\"type\":%d,\"stage\":%d,\"cube\":%.2f,\"col\":%.2f,"
        "\"power\":%.1f,\"running\":%d,\"paused\":%d,\"elapsed\":%u}",
        (int)s->type, (int)s->stage, s->cube_temp, s->column_temp,
        s->power_percent, s->is_running ? 1 : 0, s->is_paused ? 1 : 0,
        (unsigned)s->elapsed_sec);

    mqtt_publish_json(json);
}

bool mqtt_is_connected(void) { return s_connected; }

void mqtt_task(void *arg) {
    (void)arg;
    wd_register("mqtt", 30000, false);

    while (1) {
        WD_FEED("mqtt");
        if (flags_get_by_key("mqtt") && !s_started && s_cfg.uri[0]) {
            mqtt_start_client();
        }
        mqtt_publish_state();
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
