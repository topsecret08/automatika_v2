#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

/* Интеграция с внешними сервисами для Automatika V0.9 beta: Blynk и MQTT.
 *
 * Карта виртуальных пинов Blynk (мобильное приложение):
 *   V0  - steam temperature, read-only
 *   V3  - start (1) / stop (0)
 *   V4  - power state, read + write (1 = run, 0 = stop)
 *   V16 - target power voltage (0..vref_mv), read + write
 *   V38 - temperature colors, "R,G,B|R,G,B|R,G,B"
 *
 * MQTT topics:
 *   SMV/<token>/sensor/0/3  steam temperature
 *   SMV/<token>/sensor/1/3  cube temperature
 *   SMV/<token>/sensor/2/3  power voltage
 *   SMV/<token>/sensor/3/3  power state (0/1)
 *   SMV/<token>/log/3       process log
 *   SMV/<token>/set/3       commands: start | stop | power <percent>
 */

#define REMOTE_DEFAULT_HOST    "blynk.cloud"
#define REMOTE_DEFAULT_PORT    80
#define REMOTE_DEFAULT_VREF_MV 5000
#define REMOTE_PUBLISH_MS      5000

typedef struct {
    bool     enabled;
    char     host[64];
    uint16_t port;
    char     token[40];       /* Blynk auth token (also used for MQTT if mtok is empty) */
    uint16_t vref_mv;         /* DAC full-scale voltage, mV */
    char     mqtt_uri[128];   /* mqtt://host:1883, empty = disabled */
    char     mqtt_user[48];
    char     mqtt_pass[48];
    char     mqtt_token[48];  /* topic token; defaults to Blynk token */
} remote_cfg_t;

typedef struct {
    bool     enabled;
    bool     token_set;
    bool     blynk_connected;
    bool     mqtt_connected;
    bool     mqtt_configured;
    char     host[64];
    uint16_t port;
    uint16_t vref_mv;
} remote_status_t;

typedef struct {
    float steam_temp;   /* DS18B20 #1 (column/steam) */
    float cube_temp;    /* DS18B20 #0 */
    float power_volt;   /* estimated MCP4725 output voltage */
    bool  power_on;
    int   stage;
} remote_values_t;

esp_err_t remote_init(void);
void      remote_task(void *arg);

void      remote_get_config(remote_cfg_t *out);
esp_err_t remote_configure(const remote_cfg_t *cfg);
void      remote_get_status(remote_status_t *out);
void      remote_get_values(remote_values_t *out);

/* Commands come from Blynk V3/V4 and from MQTT "set" topic. */
bool remote_command(const char *cmd, const char *source);
bool remote_set_power_percent(float pct, const char *source);
bool remote_set_power_volts(float volts, const char *source);
