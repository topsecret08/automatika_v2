#include "samovar.h"
#include "samovar_priv.h"

#include "core/state.h"
#include "core/events.h"
#include "core/watchdog.h"
#include "control/ctrl_heater.h"
#include "control/safety_gate.h"
#include "control/ctrl_safety.h"
#include "hal/hal_1wire.h"
#include "modes/mode_base.h"
#include "db/db_manager.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define SAMOVAR_NS "samovar"

static const char *TAG = "SAMOVAR";

extern int64_t g_current_process_id;

static samovar_cfg_t     s_cfg;
static SemaphoreHandle_t s_cfg_mtx = NULL;
static bool              s_initialized = false;

/* ------------------------------------------------------------------ */
/* config                                                             */
/* ------------------------------------------------------------------ */

static void cfg_defaults(samovar_cfg_t *c) {
    memset(c, 0, sizeof(*c));
    strncpy(c->host, SAMOVAR_DEFAULT_HOST, sizeof(c->host) - 1);
    c->port = SAMOVAR_DEFAULT_PORT;
    c->vref_mv = SAMOVAR_DEFAULT_VREF_MV;
}

static void load_str(nvs_handle_t h, const char *key, char *dst, size_t max,
                     const char *def) {
    char tmp[192];
    size_t sz = sizeof(tmp);
    if (nvs_get_str(h, key, tmp, &sz) == ESP_OK) {
        strncpy(dst, tmp, max - 1);
        dst[max - 1] = '\0';
    } else {
        strncpy(dst, def, max - 1);
        dst[max - 1] = '\0';
    }
}

static void cfg_load(samovar_cfg_t *c) {
    cfg_defaults(c);

    nvs_handle_t h;
    if (nvs_open(SAMOVAR_NS, NVS_READONLY, &h) != ESP_OK) return;

    load_str(h, "host", c->host, sizeof(c->host), SAMOVAR_DEFAULT_HOST);
    load_str(h, "token", c->token, sizeof(c->token), "");
    load_str(h, "muri", c->mqtt_uri, sizeof(c->mqtt_uri), "");
    load_str(h, "muser", c->mqtt_user, sizeof(c->mqtt_user), "");
    load_str(h, "mpass", c->mqtt_pass, sizeof(c->mqtt_pass), "");
    load_str(h, "mtok", c->mqtt_token, sizeof(c->mqtt_token), "");

    uint16_t v = 0;
    if (nvs_get_u16(h, "port", &v) == ESP_OK && v > 0) c->port = v;
    if (nvs_get_u16(h, "vref", &v) == ESP_OK) c->vref_mv = v;

    uint8_t en = 0;
    if (nvs_get_u8(h, "en", &en) == ESP_OK) c->enabled = (en != 0);

    nvs_close(h);

    if (!c->token[0]) c->enabled = false;
}

static esp_err_t cfg_save(const samovar_cfg_t *c) {
    nvs_handle_t h;
    esp_err_t err = nvs_open(SAMOVAR_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;

    nvs_set_u8(h, "en", c->enabled ? 1 : 0);
    nvs_set_str(h, "host", c->host);
    nvs_set_u16(h, "port", c->port);
    nvs_set_str(h, "token", c->token);
    nvs_set_u16(h, "vref", c->vref_mv);
    nvs_set_str(h, "muri", c->mqtt_uri);
    nvs_set_str(h, "muser", c->mqtt_user);
    nvs_set_str(h, "mpass", c->mqtt_pass);
    nvs_set_str(h, "mtok", c->mqtt_token);

    err = nvs_commit(h);
    nvs_close(h);
    return err;
}

static bool str_prefix(const char *s, const char *p) {
    return strncmp(s, p, strlen(p)) == 0;
}

static esp_err_t cfg_validate(const samovar_cfg_t *c) {
    if (c->host[0] == '\0' || strlen(c->host) >= sizeof(c->host))
        return ESP_ERR_INVALID_ARG;
    if (c->port == 0)
        return ESP_ERR_INVALID_ARG;
    if (c->vref_mv < 1000 || c->vref_mv > 12000)
        return ESP_ERR_INVALID_ARG;
    if (strlen(c->token) >= sizeof(c->token))
        return ESP_ERR_INVALID_ARG;
    if (c->enabled && c->token[0] == '\0')
        return ESP_ERR_INVALID_ARG;
    if (strlen(c->mqtt_uri) >= sizeof(c->mqtt_uri))
        return ESP_ERR_INVALID_ARG;
    if (c->mqtt_uri[0] &&
        !str_prefix(c->mqtt_uri, "mqtt://") &&
        !str_prefix(c->mqtt_uri, "mqtts://") &&
        !str_prefix(c->mqtt_uri, "ws://") &&
        !str_prefix(c->mqtt_uri, "wss://"))
        return ESP_ERR_INVALID_ARG;
    return ESP_OK;
}

void samovar_get_config(samovar_cfg_t *out) {
    if (!out) return;
    if (!s_cfg_mtx) {
        *out = s_cfg;
        return;
    }
    xSemaphoreTake(s_cfg_mtx, portMAX_DELAY);
    *out = s_cfg;
    xSemaphoreGive(s_cfg_mtx);
}

/* ------------------------------------------------------------------ */
/* commands                                                           */
/* ------------------------------------------------------------------ */

static bool process_start(const char *source) {
    if (!ctrl_safety_ok()) {
        events_log(EVENT_WARNING, "Samovar[%s]: START blocked by safety",
                   source ? source : "?");
        return false;
    }

    process_state_t *st = state_get();
    if (!st->is_running) {
        process_type_t type = st->type;
        if (type <= PROCESS_IDLE || type > PROCESS_FERMENT)
            type = PROCESS_DISTILL;

        const process_mode_t *m = mode_from_type(type);
        if (!m) {
            type = PROCESS_DISTILL;
            m = mode_from_type(type);
        }
        if (!m) return false;

        mode_set(m);
        st->type = type;
        st->start_cube_temp = st->cube_temp;
        st->is_paused = false;
        st->is_running = true;

        if (db_is_ready()) {
            g_current_process_id =
                db_process_start((uint8_t)type, "samovar");
            st->process_id = g_current_process_id;
        }
    } else {
        st->is_paused = false;
    }

    events_log(EVENT_USER_ACTION, "Samovar[%s]: START",
               source ? source : "?");
    return true;
}

static bool process_stop(const char *source) {
    ctrl_heater_force_off();
    if (mode_get() && mode_get()->stop) mode_get()->stop();

    process_state_t *st = state_get();
    st->is_running = false;
    st->is_paused = false;

    events_log(EVENT_USER_ACTION, "Samovar[%s]: STOP",
               source ? source : "?");
    return true;
}

bool samovar_set_power_percent(float pct, const char *source) {
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;

    if (pct > 0 && !ctrl_safety_ok()) {
        events_log(EVENT_WARNING, "Samovar[%s]: power %.0f%% blocked by safety",
                   source ? source : "?", pct);
        return false;
    }

    safety_gate_apply("mqtt_power", pct);
    events_log(EVENT_USER_ACTION, "Samovar[%s]: power %.0f%%",
               source ? source : "?", pct);
    return true;
}

bool samovar_set_power_volts(float volts, const char *source) {
    samovar_cfg_t c;
    samovar_get_config(&c);
    float vref = (c.vref_mv > 0) ? (float)c.vref_mv : 5000.0f;

    if (volts < 0) volts = 0;
    if (volts > vref) volts = vref;

    return samovar_set_power_percent(volts * 100.0f / vref, source);
}

bool samovar_command(const char *cmd, const char *source) {
    if (!cmd) return false;

    char buf[64];
    size_t i = 0;
    while (cmd[i] && isspace((unsigned char)cmd[i])) i++;
    strncpy(buf, cmd + i, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    size_t n = strlen(buf);
    while (n > 0 && isspace((unsigned char)buf[n - 1])) buf[--n] = '\0';
    if (n == 0) return false;

    if (strcasecmp(buf, "start") == 0 || strcasecmp(buf, "on") == 0)
        return process_start(source);
    if (strcasecmp(buf, "stop") == 0 || strcasecmp(buf, "off") == 0)
        return process_stop(source);

    if (strncasecmp(buf, "power", 5) == 0) {
        const char *p = buf + 5;
        if (*p && !isspace((unsigned char)*p) && *p != ':' && *p != '=')
            return false;
        while (*p && (isspace((unsigned char)*p) || *p == ':' || *p == '='))
            p++;
        if (!*p) return false;
        return samovar_set_power_percent(strtof(p, NULL), source);
    }

    if (isdigit((unsigned char)buf[0]) || buf[0] == '-' || buf[0] == '.')
        return samovar_set_power_percent(strtof(buf, NULL), source);

    return false;
}

/* ------------------------------------------------------------------ */
/* values / task                                                      */
/* ------------------------------------------------------------------ */

void samovar_get_values(samovar_values_t *out) {
    if (!out) return;

    process_state_t *st = state_get();
    samovar_cfg_t c;
    samovar_get_config(&c);

    float cube = hal_1wire_read(0);
    float steam = hal_1wire_read(1);
    if (steam < -50.0f) steam = cube;   /* second sensor is optional */

    float pct = ctrl_heater_get_percent();
    float vref = (c.vref_mv > 0) ? (float)c.vref_mv : 5000.0f;

    out->cube_temp = cube;
    out->steam_temp = steam;
    out->power_volt = pct / 100.0f * vref / 1000.0f;
    out->power_on = pct > 0.5f;
    out->stage = (int)st->stage;
}

esp_err_t samovar_init(void) {
    if (s_initialized) return ESP_OK;

    s_cfg_mtx = xSemaphoreCreateMutex();
    if (!s_cfg_mtx) return ESP_ERR_NO_MEM;

    cfg_load(&s_cfg);
    s_initialized = true;

    samovar_blynk_configure(&s_cfg);
    samovar_mqtt_configure(&s_cfg);

    events_log(EVENT_INFO,
               "Samovar: %s, blynk %s:%u, token %s, mqtt %s",
               s_cfg.enabled ? "ON" : "OFF", s_cfg.host, s_cfg.port,
               s_cfg.token[0] ? "set" : "none",
               s_cfg.mqtt_uri[0] ? "configured" : "off");
    ESP_LOGI(TAG, "init: enabled=%d host=%s:%u vref=%umV",
             s_cfg.enabled, s_cfg.host, s_cfg.port, s_cfg.vref_mv);
    return ESP_OK;
}

esp_err_t samovar_configure(const samovar_cfg_t *cfg) {
    if (!cfg || !s_cfg_mtx) return ESP_ERR_INVALID_ARG;

    samovar_cfg_t c = *cfg;
    if (!c.token[0]) c.enabled = false;

    esp_err_t err = cfg_validate(&c);
    if (err != ESP_OK) return err;

    xSemaphoreTake(s_cfg_mtx, portMAX_DELAY);
    s_cfg = c;
    xSemaphoreGive(s_cfg_mtx);

    err = cfg_save(&c);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS save failed: %s", esp_err_to_name(err));
        return err;
    }

    samovar_blynk_configure(&c);
    samovar_mqtt_configure(&c);

    events_log(EVENT_INFO, "Samovar: config updated (enabled=%d, mqtt=%d)",
               c.enabled, c.mqtt_uri[0] ? 1 : 0);
    return ESP_OK;
}

void samovar_get_status(samovar_status_t *out) {
    if (!out) return;
    memset(out, 0, sizeof(*out));

    samovar_cfg_t c;
    samovar_get_config(&c);

    out->enabled = c.enabled;
    out->token_set = c.token[0] != '\0';
    out->blynk_connected = samovar_blynk_connected();
    out->mqtt_configured = c.mqtt_uri[0] != '\0';
    out->mqtt_connected = samovar_mqtt_connected();
    strncpy(out->host, c.host, sizeof(out->host) - 1);
    out->port = c.port;
    out->vref_mv = c.vref_mv;
}

void samovar_task(void *arg) {
    (void)arg;
    wd_register("samovar", 30000, false);

    uint32_t last_pub = 0;
    int last_stage = -1;
    bool last_emergency = false;

    while (1) {
        WD_FEED("samovar");
        samovar_blynk_poll();

        uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
        if (last_pub == 0 || now - last_pub >= SAMOVAR_PUBLISH_MS) {
            last_pub = now;

            samovar_values_t v;
            samovar_get_values(&v);
            samovar_blynk_publish(&v);
            samovar_mqtt_publish_values(&v);

            process_state_t *st = state_get();
            if ((int)st->stage != last_stage) {
                last_stage = (int)st->stage;
                if (st->is_running) {
                    char msg[48];
                    snprintf(msg, sizeof(msg), "stage:%d", last_stage);
                    samovar_mqtt_log(msg);
                }
            }

            if (st->emergency && !last_emergency) {
                char msg[128];
                snprintf(msg, sizeof(msg), "EMERGENCY:%s",
                         st->emergency_reason);
                samovar_mqtt_log(msg);
            }
            last_emergency = st->emergency;
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
