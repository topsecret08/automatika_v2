#pragma once
#include "esp_err.h"
#include <stdbool.h>

// Wi-Fi provisioning (SoftAP + captive portal + защищённый транспорт)
// на штатном esp_wifi_provisioning:
//   PROV_AP_SSID  ("Automatika-Setup")  ← точка доступа для настройки
//   PoP по умолчанию пустой; PROV_POP_SECRET + PROV_POP_TEXT при необходимости
//
// Логика:
//   * нет сохранённых credentials → сразу provisioning;
//   * STA не подключился за WIFI_STA_CONNECT_TIMEOUT_MS → fallback в AP;
//   * credentials получены → сохранены в NVS "nvs.net80211" (управляет IDF),
//     AP останавливается, STA-соединение удерживается с авто-переподключением.
//
// Модуль НЕ ищет пароли в собственных ключах wifi/ssid — источником истины
// служит esp_wifi_provisioning (совместимо с ранее сохранёнными сетями).

typedef enum {
    WIFI_STATE_OFF = 0,
    WIFI_STATE_CONNECTING,     // STA подключается
    WIFI_STATE_CONNECTED,      // STA с IP
    WIFI_STATE_PROVISIONING,   // поднят AP для настройки
    WIFI_STATE_FALLBACK_AP,    // STA не поднялся за отведённое время
} wifi_state_t;

esp_err_t  wifi_prov_init(void);
void       wifi_prov_task(void *arg);
wifi_state_t wifi_prov_state(void);
const char  *wifi_prov_state_name(void);
bool       wifi_prov_is_connected(void);
// Принудительный старт provisioning (кнопка/API), даже если credentials есть.
esp_err_t  wifi_prov_start(void);
// Забыть сеть и уйти в provisioning.
esp_err_t  wifi_prov_reset(void);
const char *wifi_prov_status_json(void);
