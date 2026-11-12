#include "samovar_priv.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "SAMOVAR_MQTT";

static esp_mqtt_client_handle_t s_client = NULL;
static SemaphoreHandle_t        s_mtx = NULL;
static volatile bool            s_connected = false;
static bool                     s_started = false;

static char s_uri[128];
static char s_user[48];
static char s_pass[48];
static char s_token[48];
static char s_prefix[96];
static char s_topic_set[112];

static void copy_token(const char *src, char *dst, size_t max) {
    size_t o = 0;
    if (src) {
        for (size_t i = 0; src[i] && o + 1 < max; i++) {
            char c = src[i];
            if (c == '/' || c == '+' || c == '#' || isspace((unsigned char)c))
                c = '_';
            dst[o++] = c;
        }
    }
    if (o == 0 && max > 1) {
        const char *def = "device";
        size_t n = strlen(def);
        if (n > max - 1) n = max - 1;
        memcpy(dst, def, n);
        o = n;
    }
    dst[o] = '\0';
}

static void build_topics(void) {
    char tok[48];
    copy_token(s_token, tok, sizeof(tok));
    snprintf(s_prefix, sizeof(s_prefix), "SMV/%s", tok);
    snprintf(s_topic_set, sizeof(s_topic_set), "%s/set/3", s_prefix);
}

static void mqtt_event_handler(void *arg, esp_event_base_t base,
                               int32_t event_id, void *event_data) {
    (void)arg;
    (void)base;
    esp_mqtt_event_handle_t e = (esp_mqtt_event_handle_t)event_data;
    if (!e) return;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        s_connected = true;
        if (s_topic_set[0])
            esp_mqtt_client_subscribe(e->client, s_topic_set, 1);
        ESP_LOGI(TAG, "connected, subscribed to %s", s_topic_set);
        break;

    case MQTT_EVENT_DISCONNECTED:
        s_connected = false;
        break;

    case MQTT_EVENT_DATA: {
        if (e->topic_len <= 0 || e->data_len <= 0) break;
        size_t tl = strlen(s_topic_set);
        if ((size_t)e->topic_len != tl ||
            memcmp(e->topic, s_topic_set, tl) != 0) break;

        char cmd[96];
        int n = e->data_len;
        if (n > (int)sizeof(cmd) - 1) n = (int)sizeof(cmd) - 1;
        memcpy(cmd, e->data, (size_t)n);
        cmd[n] = '\0';

        size_t len = strlen(cmd);
        while (len > 0 && isspace((unsigned char)cmd[len - 1]))
            cmd[--len] = '\0';

        if (len > 0) samovar_command(cmd, "mqtt");
        break;
    }

    default:
        break;
    }
}

static void start_client(void) {
    xSemaphoreTake(s_mtx, portMAX_DELAY);

    if (s_started || s_uri[0] == '\0') {
        xSemaphoreGive(s_mtx);
        return;
    }

    esp_mqtt_client_config_t cfg = {0};
    cfg.broker.address.uri = s_uri;
    if (s_user[0]) cfg.credentials.username = s_user;
    if (s_pass[0]) cfg.credentials.authentication.password = s_pass;
    cfg.session.keepalive = 30;
    cfg.network.reconnect_timeout_ms = 5000;

    s_client = esp_mqtt_client_init(&cfg);
    if (!s_client) {
        ESP_LOGE(TAG, "init failed");
        xSemaphoreGive(s_mtx);
        return;
    }

    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID,
                                   mqtt_event_handler, NULL);

    if (esp_mqtt_client_start(s_client) == ESP_OK) {
        s_started = true;
        ESP_LOGI(TAG, "client started: %s", s_uri);
    } else {
        ESP_LOGE(TAG, "start failed");
        esp_mqtt_client_destroy(s_client);
        s_client = NULL;
    }

    xSemaphoreGive(s_mtx);
}

void samovar_mqtt_configure(const samovar_cfg_t *cfg) {
    if (!cfg) return;
    if (!s_mtx) s_mtx = xSemaphoreCreateMutex();
    if (!s_mtx) return;

    const char *tok = cfg->mqtt_token[0] ? cfg->mqtt_token : cfg->token;

    xSemaphoreTake(s_mtx, portMAX_DELAY);

    bool changed = (strcmp(s_uri, cfg->mqtt_uri) != 0) ||
                   (strcmp(s_user, cfg->mqtt_user) != 0) ||
                   (strcmp(s_pass, cfg->mqtt_pass) != 0) ||
                   (strcmp(s_token, tok) != 0);

    if (changed) {
        if (s_client) {
            esp_mqtt_client_stop(s_client);
            esp_mqtt_client_destroy(s_client);
            s_client = NULL;
        }
        s_started = false;
        s_connected = false;

        strncpy(s_uri, cfg->mqtt_uri, sizeof(s_uri) - 1);
        s_uri[sizeof(s_uri) - 1] = '\0';
        strncpy(s_user, cfg->mqtt_user, sizeof(s_user) - 1);
        s_user[sizeof(s_user) - 1] = '\0';
        strncpy(s_pass, cfg->mqtt_pass, sizeof(s_pass) - 1);
        s_pass[sizeof(s_pass) - 1] = '\0';
        strncpy(s_token, tok, sizeof(s_token) - 1);
        s_token[sizeof(s_token) - 1] = '\0';
        build_topics();
    }

    bool want = s_uri[0] != '\0';
    xSemaphoreGive(s_mtx);

    if (want) start_client();
}

bool samovar_mqtt_connected(void) { return s_connected; }

static bool publish_locked(const char *topic, const char *payload) {
    if (!s_connected || !s_client) return false;
    int id = esp_mqtt_client_publish(s_client, topic, payload, 0, 2, 1);
    return id >= 0;
}

void samovar_mqtt_publish_values(const samovar_values_t *v) {
    if (!v || !s_mtx) return;

    xSemaphoreTake(s_mtx, portMAX_DELAY);
    if (!s_connected || !s_client || !s_prefix[0]) {
        xSemaphoreGive(s_mtx);
        return;
    }

    char topic[128];
    char buf[32];

    snprintf(buf, sizeof(buf), "%.1f", v->steam_temp);
    snprintf(topic, sizeof(topic), "%s/sensor/0/3", s_prefix);
    publish_locked(topic, buf);

    snprintf(buf, sizeof(buf), "%.1f", v->cube_temp);
    snprintf(topic, sizeof(topic), "%s/sensor/1/3", s_prefix);
    publish_locked(topic, buf);

    snprintf(buf, sizeof(buf), "%.2f", v->power_volt);
    snprintf(topic, sizeof(topic), "%s/sensor/2/3", s_prefix);
    publish_locked(topic, buf);

    snprintf(buf, sizeof(buf), "%d", v->power_on ? 1 : 0);
    snprintf(topic, sizeof(topic), "%s/sensor/3/3", s_prefix);
    publish_locked(topic, buf);

    xSemaphoreGive(s_mtx);
}

void samovar_mqtt_log(const char *msg) {
    if (!msg || !s_mtx) return;

    xSemaphoreTake(s_mtx, portMAX_DELAY);
    if (!s_connected || !s_client || !s_prefix[0]) {
        xSemaphoreGive(s_mtx);
        return;
    }

    char topic[128];
    snprintf(topic, sizeof(topic), "%s/log/3", s_prefix);
    publish_locked(topic, msg);
    xSemaphoreGive(s_mtx);
}

void samovar_mqtt_stop(void) {
    if (!s_mtx) return;
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    if (s_client) {
        esp_mqtt_client_stop(s_client);
        esp_mqtt_client_destroy(s_client);
        s_client = NULL;
    }
    s_started = false;
    s_connected = false;
    xSemaphoreGive(s_mtx);
}
