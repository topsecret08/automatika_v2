#include "wifi_prov.h"
#include "config.h"

#include "core/events.h"
#include "core/watchdog.h"
#include "net/mdns_service.h"
#include "web/telegram_notify.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "wifi_provisioning/manager.h"
#include "wifi_provisioning/scheme_softap.h"
#include "wifi_provisioning/wifi_config.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "WIFI";

#define CONNECTED_BIT BIT0

static EventGroupHandle_t s_net_eg;
static volatile wifi_state_t s_state = WIFI_STATE_OFF;
static volatile bool s_prov_running = false;
static bool s_prov_ready = false;
static bool s_ap_netif_ready = false;

static void set_state(wifi_state_t st) {
    if (s_state == st) return;
    s_state = st;
    ESP_LOGI(TAG, "state -> %s", wifi_prov_state_name());
}

const char *wifi_prov_state_name(void) {
    switch (s_state) {
    case WIFI_STATE_CONNECTING:   return "connecting";
    case WIFI_STATE_CONNECTED:    return "connected";
    case WIFI_STATE_PROVISIONING: return "provisioning";
    case WIFI_STATE_FALLBACK_AP:  return "fallback-ap";
    default:                      return "off";
    }
}

wifi_state_t wifi_prov_state(void) { return s_state; }
bool wifi_prov_is_connected(void) { return s_state == WIFI_STATE_CONNECTED; }

/* ------------------------------------------------------------------ */

static void sta_event_handler(void *arg, esp_event_base_t base,
                              int32_t id, void *data) {
    (void)arg;
    if (base == WIFI_EVENT) {
        switch (id) {
        case WIFI_EVENT_STA_START:
            esp_wifi_connect();
            break;
        case WIFI_EVENT_STA_DISCONNECTED: {
            wifi_event_sta_disconnected_t *e = data;
            ESP_LOGW(TAG, "STA disconnected (reason %d)", e ? e->reason : -1);
            if (!s_prov_running) {
                set_state(WIFI_STATE_CONNECTING);
                xEventGroupClearBits(s_net_eg, CONNECTED_BIT);
                esp_wifi_connect();   /* авто-переподключение */
            }
            break;
        }
        default:
            break;
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = data;
        ESP_LOGI(TAG, "got IP: " IPSTR, IP2STR(&e->ip_info.ip));
        set_state(WIFI_STATE_CONNECTED);
        xEventGroupSetBits(s_net_eg, CONNECTED_BIT);
        events_log(EVENT_NETWORK, "Wi-Fi connected: " IPSTR, IP2STR(&e->ip_info.ip));
    }
}

static void prov_event_handler(void *arg, esp_event_base_t base,
                               int32_t id, void *data) {
    (void)arg;
    if (base != WIFI_PROV_EVENT) return;

    switch (id) {
    case WIFI_PROV_START:
        ESP_LOGI(TAG, "provisioning started, AP '%s'", PROV_AP_SSID);
        set_state(WIFI_STATE_PROVISIONING);
        events_log(EVENT_NETWORK, "Wi-Fi provisioning started (AP %s)", PROV_AP_SSID);
        break;
    case WIFI_PROV_CRED_RECV:
        ESP_LOGI(TAG, "credentials received");
        break;
    case WIFI_PROV_CRED_FAIL: {
        wifi_prov_sta_fail_reason_t reason = WIFI_PROV_STA_AP_NOT_FOUND;
        if (data) reason = *(wifi_prov_sta_fail_reason_t *)data;
        ESP_LOGE(TAG, "provisioning failed: %s",
                 (reason == WIFI_PROV_STA_AUTH_ERROR) ? "auth" : "AP not found");
        events_log(EVENT_WARNING, "Wi-Fi provisioning failed");
        break;
    }
    case WIFI_PROV_CRED_SUCCESS:
        ESP_LOGI(TAG, "provisioning success, credentials stored");
        events_log(EVENT_NETWORK, "Wi-Fi credentials accepted");
        /* stop_provisioning() вызывается из задачи (менеджер активен) */
        break;
    case WIFI_PROV_END:
        s_prov_running = false;
        ESP_LOGI(TAG, "provisioning ended");
        events_log(EVENT_NETWORK, "Wi-Fi provisioning ended");
        break;
    default:
        break;
    }
}

/* ------------------------------------------------------------------ */

static bool credentials_stored(void) {
    bool provisioned = false;
    if (!s_prov_ready) return false;
    wifi_prov_mgr_is_provisioned(&provisioned);
    return provisioned;
}

static esp_err_t start_provisioning(void) {
    if (!s_prov_ready) return ESP_ERR_INVALID_STATE;
    if (s_prov_running) return ESP_OK;

    // SoftAP-схема требует AP-netif; при fallback после неудачного STA его
    // ещё нет. Создаём один раз, чтобы не плодить netif-объекты.
    if (!s_ap_netif_ready) {
        if (!esp_netif_create_default_wifi_ap()) {
            ESP_LOGE(TAG, "AP netif create failed");
            return ESP_FAIL;
        }
        s_ap_netif_ready = true;
    }
    esp_wifi_set_mode(WIFI_MODE_APSTA);
    esp_wifi_start();   /* ESP_ERR_INVALID_STATE, если уже запущен — норма */

    s_prov_running = true;
    set_state(WIFI_STATE_PROVISIONING);

    const char *pop = PROV_POP_SECRET ? PROV_POP_TEXT : NULL;
    esp_err_t err = wifi_prov_mgr_start_provisioning(
        PROV_POP_SECRET ? WIFI_PROV_SECURITY_1 : WIFI_PROV_SECURITY_0,
        pop, PROV_AP_SSID, NULL);
    if (err != ESP_OK) {
        s_prov_running = false;
        ESP_LOGE(TAG, "start_provisioning failed: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t wifi_prov_start(void) { return start_provisioning(); }

esp_err_t wifi_prov_reset(void) {
    ESP_LOGW(TAG, "forgetting Wi-Fi credentials");
    events_log(EVENT_WARNING, "Wi-Fi credentials reset");
    if (s_prov_running) wifi_prov_mgr_stop_provisioning();
    esp_wifi_restore();
    vTaskDelay(pdMS_TO_TICKS(200));
    esp_restart();
    return ESP_OK;
}

static void start_sta(void) {
    s_state = WIFI_STATE_OFF;
    set_state(WIFI_STATE_CONNECTING);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();          /* WIFI_EVENT_STA_START → esp_wifi_connect() */
}

esp_err_t wifi_prov_init(void) {
    s_net_eg = xEventGroupCreate();
    if (!s_net_eg) return ESP_ERR_NO_MEM;

    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;

    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;

    wifi_init_config_t wc = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&wc);
    if (err != ESP_OK) return err;

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, sta_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, sta_event_handler, NULL);
    esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID,
                               prov_event_handler, NULL);

    wifi_prov_mgr_config_t cfg = {
        .scheme = wifi_prov_scheme_softap,
        .scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE,
    };
    err = wifi_prov_mgr_init(cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "prov_mgr_init failed: %s — работаем без provisioning",
                 esp_err_to_name(err));
    } else {
        s_prov_ready = true;
    }

    if (s_prov_ready && credentials_stored()) {
        esp_netif_create_default_wifi_sta();
        start_sta();
        ESP_LOGI(TAG, "credentials present, connecting as STA");
    } else {
        esp_netif_create_default_wifi_sta();
        esp_wifi_set_mode(WIFI_MODE_STA);
        esp_wifi_start();
        start_provisioning();
        ESP_LOGW(TAG, "no credentials → provisioning AP '%s'", PROV_AP_SSID);
    }

    events_log(EVENT_INFO, "Wi-Fi manager ready (%s)",
               s_prov_ready ? "provisioning available" : "STA only");
    return ESP_OK;
}

const char *wifi_prov_status_json(void) {
    static char out[256];
    char ip[20] = "-";
    esp_netif_t *n = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (n) {
        esp_netif_ip_info_t info;
        if (esp_netif_get_ip_info(n, &info) == ESP_OK)
            snprintf(ip, sizeof(ip), IPSTR, IP2STR(&info.ip));
    }
    snprintf(out, sizeof(out),
             "{\"state\":\"%s\",\"ip\":\"%s\",\"connected\":%d,\"ap_ssid\":\"%s\","
             "\"version\":\"%s\"}",
             wifi_prov_state_name(), ip, wifi_prov_is_connected() ? 1 : 0,
             PROV_AP_SSID, FW_VERSION);
    return out;
}

/* ------------------------------------------------------------------ */
/* Задача: fallback AP через 60 с, остановка провижининга, mDNS        */
/* ------------------------------------------------------------------ */
void wifi_prov_task(void *arg) {
    (void)arg;
    wd_register("wifi_mgr", 10000, false);

    int64_t connecting_since = 0;
    bool mdns_done = false;
    bool fallback_done = false;

    while (1) {
        WD_FEED("wifi_mgr");

        /* успех провижининга: остановить AP, перейти в STA */
        if (s_prov_running && credentials_stored() && wifi_prov_is_connected()) {
            wifi_prov_mgr_stop_provisioning();
            s_prov_running = false;
            ESP_LOGI(TAG, "provisioning stopped, STA connected");
            telegram_notify_alert("ИНФО", "Устройство подключено к Wi-Fi");
        }

        if (s_state == WIFI_STATE_CONNECTING) {
            if (connecting_since == 0) connecting_since = esp_timer_get_time();
            if (!fallback_done &&
                esp_timer_get_time() - connecting_since >
                    (int64_t)WIFI_STA_CONNECT_TIMEOUT_MS * 1000LL) {
                fallback_done = true;
                connecting_since = 0;
                set_state(WIFI_STATE_FALLBACK_AP);
                start_provisioning();
                ESP_LOGW(TAG, "STA timeout: provisioning AP enabled");
                telegram_notify_alert("ВНИМАНИЕ",
                                      "Wi-Fi: нет подключения за 60 с, включена точка доступа");
            }
        } else if (s_state == WIFI_STATE_CONNECTED) {
            connecting_since = 0;
            fallback_done = false;
        }

        if (wifi_prov_is_connected() && !mdns_done) {
            mdns_done = true;
            mdns_service_start();
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
