#include "hal_usb.h"
#include "core/control_config.h"
#include "core/events.h"
#include "core/state.h"
#include "core/storage.h"
#include "version.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "tinyusb.h"
#include "tusb_msc_storage.h"
#include "tusb_cdc_acm.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "USB";
static usb_mode_t s_mode = USB_MODE_OFF;
static bool s_mounted = false;

#define CDC_LINE_MAX 64
#define CDC_INFO_CMD "AUTOMATIKA?"
static char s_cdc_line[CDC_LINE_MAX];
static size_t s_cdc_len = 0;

static void msc_event_cb(tinyusb_msc_event_t *event) {
    if (event->type == TINYUSB_MSC_EVENT_MOUNT_CHANGED) {
        s_mounted = event->mount_changed_data.is_mounted;
        if (s_mounted) {
            events_log(EVENT_INFO, "USB MSC: mounted by host");
        } else {
            events_log(EVENT_INFO, "USB MSC: unmounted");
        }
    }
}

/* Ответ на «AUTOMATIKA?»: одна строка JSON. Нужен конфигуратору на ПК, чтобы
 * найти плату среди COM-портов и взять API-ключ без ручного ввода. */
static void cdc_send_info(int itf) {
    char key[API_KEY_LEN] = {0};
    if (control_config_api_has_key()) control_config_api_copy(key, sizeof(key));

    char out[192];
    int n = snprintf(out, sizeof(out),
                     "{\"name\":\"Automatika\",\"version\":\"%s\",\"running\":%d,"
                     "\"api_key\":\"%s\"}\n",
                     FW_VERSION, state_get()->is_running ? 1 : 0, key);
    if (n <= 0 || n >= (int)sizeof(out)) return;
    tinyusb_cdcacm_write_queue(itf, (const uint8_t *)out, (size_t)n);
    tinyusb_cdcacm_write_flush(itf, 0);
}

static void cdc_rx_cb(int itf, cdcacm_event_t *event) {
    (void)event;
    uint8_t buf[64];
    size_t rx = 0;
    if (tinyusb_cdcacm_read(itf, buf, sizeof(buf), &rx) != ESP_OK) return;

    for (size_t i = 0; i < rx; i++) {
        char c = (char)buf[i];
        if (c == '\r') continue;
        if (c != '\n') {
            if (s_cdc_len < sizeof(s_cdc_line) - 1) s_cdc_line[s_cdc_len++] = c;
            continue;
        }
        s_cdc_line[s_cdc_len] = '\0';
        if (strcmp(s_cdc_line, CDC_INFO_CMD) == 0) cdc_send_info(itf);
        s_cdc_len = 0;
    }
}

static esp_err_t msc_storage_init(void) {
    sdmmc_card_t *card = storage_get_sd_card();
    if (!card) {
        ESP_LOGE(TAG, "SD card not available");
        return ESP_FAIL;
    }

    tinyusb_msc_sdmmc_config_t cfg = {
        .card = card,
        .callback_mount_changed = msc_event_cb,
        .mount_config.max_files = 4,
        .mount_config.format_if_mount_failed = false,
    };
    return tinyusb_msc_storage_init_sdmmc(&cfg);
}

bool hal_usb_init(usb_mode_t mode) {
    if (s_mode != USB_MODE_OFF) hal_usb_deinit();
    if (mode == USB_MODE_OFF) return true;

    static char serial[32];
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_BT);
    snprintf(serial, sizeof(serial), "%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    static const char *str_desc[5] = {
        "Automatika",
        "Automatika V0.9.5 beta",
        serial,
        "Automatika USB",
        NULL
    };

    tinyusb_config_t cfg = {
        .device_descriptor = NULL,
        .string_descriptor = str_desc,
        .external_phy = false,
    };
    esp_err_t err = tinyusb_driver_install(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "tinyusb install failed: %s", esp_err_to_name(err));
        events_log(EVENT_WARNING, "USB: init failed (%s)", esp_err_to_name(err));
        return false;
    }

    if (mode == USB_MODE_MSC || mode == USB_MODE_BOTH) {
        if (msc_storage_init() != ESP_OK) {
            ESP_LOGW(TAG, "MSC init failed");
        }
    }

    if (mode == USB_MODE_CDC || mode == USB_MODE_BOTH) {
        tinyusb_config_cdcacm_t acm_cfg = {
            .usb_dev = TINYUSB_USBDEV_0,
            .cdc_port = TINYUSB_CDC_ACM_0,
            .callback_rx = cdc_rx_cb,
            .callback_rx_wanted_char = NULL,
            .callback_line_state_changed = NULL,
            .callback_line_coding_changed = NULL,
        };
        err = tusb_cdc_acm_init(&acm_cfg);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "CDC init failed: %s", esp_err_to_name(err));
            events_log(EVENT_WARNING, "USB: CDC init failed (%s)", esp_err_to_name(err));
        }
    }

    s_mode = mode;
    ESP_LOGI(TAG, "USB initialized: mode=%d", mode);
    events_log(EVENT_INFO, "USB mode=%d", mode);
    return true;
}

void hal_usb_deinit(void) {
    tinyusb_driver_uninstall();
    s_mode = USB_MODE_OFF;
    s_mounted = false;
}

bool hal_usb_is_mounted(void) { return s_mounted; }
usb_mode_t hal_usb_get_mode(void) { return s_mode; }

void hal_usb_set_mode(usb_mode_t mode) {
    hal_usb_deinit();
    if (mode != USB_MODE_OFF) hal_usb_init(mode);
}

void hal_usb_task(void *arg) {
    (void)arg;
    while (1) vTaskDelay(pdMS_TO_TICKS(1000));
}
