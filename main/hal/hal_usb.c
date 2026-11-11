#include "hal_usb.h"
#include "core/events.h"
#include "core/storage.h"
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

static void msc_event_cb(tinyusb_msc_event_t *event) {
    switch (event->id) {
    case TINYUSB_MSC_EVENT_MOUNT_START:
        s_mounted = true;
        events_log(EVENT_INFO, "USB MSC: mounted by host");
        break;
    case TINYUSB_MSC_EVENT_UNMOUNT_START:
        s_mounted = false;
        events_log(EVENT_INFO, "USB MSC: unmounted");
        break;
    case TINYUSB_MSC_EVENT_MOUNT_FAILED:
        ESP_LOGW(TAG, "MSC mount failed");
        s_mounted = false;
        break;
    default: break;
    }
}

static void cdc_rx_cb(int itf, cdcacm_event_t *event) {
    uint8_t buf[64];
    size_t rx = 0;
    if (tinyusb_cdcacm_read(itf, buf, sizeof(buf), &rx) == ESP_OK && rx > 0) {
        tinyusb_cdcacm_write_queue(itf, buf, rx);
        tinyusb_cdcacm_write_flush(itf, 0);
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
        "Automatika v2.9",
        serial,
        "Automatika USB",
        NULL
    };

    tinyusb_config_t cfg = {
        .device_descriptor = NULL,
        .string_descriptor = str_desc,
        .external_phy = false,
    };
    ESP_ERROR_CHECK(tinyusb_driver_install(&cfg));

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
        ESP_ERROR_CHECK(tusb_cdcacm_init(&acm_cfg));
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