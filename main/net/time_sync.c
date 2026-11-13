#include "time_sync.h"
#include "config.h"

#include "core/events.h"
#include "core/watchdog.h"
#include "hal/hal_i2c.h"

#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_sntp.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

static const char *TAG = "TIME";

#define RTC_ADDR 0x68
#define RTC_REG_SECONDS 0x00

static volatile bool s_synced = false;
static bool s_sntp_started = false;
static bool s_rtc_present = false;

/* ------------------------------------------------------------------ */
/* DS3231: BCD, 24h, day-of-week игнорируется (Unix времени хватает)   */
/* ------------------------------------------------------------------ */

static i2c_master_dev_handle_t s_rtc_dev = NULL;

static esp_err_t rtc_open(void) {
    if (s_rtc_dev) return ESP_OK;
    if (!hal_i2c_bus()) return ESP_ERR_INVALID_STATE;
    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = RTC_ADDR,
        .scl_speed_hz = 100000,
    };
    esp_err_t err = i2c_master_bus_add_device(hal_i2c_bus(), &cfg, &s_rtc_dev);
    if (err != ESP_OK) s_rtc_dev = NULL;
    return err;
}

bool rtc_ds3231_present(void) {
    if (s_rtc_present) return true;
    if (!hal_i2c_bus()) return false;
    s_rtc_present = (i2c_master_probe(hal_i2c_bus(), RTC_ADDR, 100) == ESP_OK);
    return s_rtc_present;
}

static uint8_t bcd2bin(uint8_t v) { return (uint8_t)((v >> 4) * 10 + (v & 0x0F)); }
static uint8_t bin2bcd(uint8_t v) { return (uint8_t)(((v / 10) << 4) | (v % 10)); }

esp_err_t rtc_read(uint32_t *unix_s) {
    if (!unix_s || rtc_open() != ESP_OK) return ESP_FAIL;

    uint8_t reg = RTC_REG_SECONDS;
    uint8_t b[7];
    esp_err_t err = i2c_master_transmit_receive(s_rtc_dev, &reg, 1, b, sizeof(b), 100);
    if (err != ESP_OK) return err;

    struct tm t = {0};
    t.tm_sec  = bcd2bin(b[0] & 0x7F);
    t.tm_min  = bcd2bin(b[1] & 0x7F);
    t.tm_hour = bcd2bin(b[2] & 0x3F);
    t.tm_mday = bcd2bin(b[3] & 0x3F);
    t.tm_mon  = bcd2bin(b[5] & 0x1F) - 1;
    t.tm_year = bcd2bin(b[6]) + 100;
    t.tm_isdst = -1;

    time_t v = mktime(&t);
    if (v <= 0) return ESP_FAIL;
    *unix_s = (uint32_t)v;
    return ESP_OK;
}

esp_err_t rtc_write(uint32_t unix_s) {
    if (rtc_open() != ESP_OK) return ESP_FAIL;

    time_t v = (time_t)unix_s;
    struct tm t;
    localtime_r(&v, &t);

    uint8_t b[8];
    b[0] = RTC_REG_SECONDS;
    b[1] = bin2bcd((uint8_t)t.tm_sec);
    b[2] = bin2bcd((uint8_t)t.tm_min);
    b[3] = bin2bcd((uint8_t)t.tm_hour);
    b[4] = bin2bcd((uint8_t)t.tm_mday);
    b[5] = bin2bcd((uint8_t)(t.tm_wday + 1));
    b[6] = bin2bcd((uint8_t)(t.tm_mon + 1));
    b[7] = bin2bcd((uint8_t)((t.tm_year + 1900) - 2000));

    return i2c_master_transmit(s_rtc_dev, b, sizeof(b), 100);
}

/* ------------------------------------------------------------------ */

int64_t time_now_unix(void) { return (int64_t)time(NULL); }

bool time_is_valid(void) {
    /* 2024-01-01: всё, что меньше — это build time / 1970 */
    return time_now_unix() > 1704067200;
}

void time_sync_init(void) {
    setenv("TZ", TZ_DEFAULT, 1);
    tzset();

    if (rtc_ds3231_present()) {
        uint32_t s;
        if (rtc_read(&s) == ESP_OK && s > 1704067200) {
            struct timeval tv = { .tv_sec = (time_t)s, .tv_usec = 0 };
            settimeofday(&tv, NULL);
            ESP_LOGI(TAG, "time restored from DS3231: %u", (unsigned)s);
            events_log(EVENT_INFO, "Time restored from RTC");
        }
    }
}

static void sntp_start(void) {
    if (s_sntp_started) return;
    esp_sntp_config_t cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG(SNTP_SERVER);
    cfg.start = true;
    cfg.sync_cb = NULL;
    esp_err_t err = esp_netif_sntp_init(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "sntp init: %s", esp_err_to_name(err));
        return;
    }
    s_sntp_started = true;
    ESP_LOGI(TAG, "SNTP started (%s)", SNTP_SERVER);
}

const char *time_status_json(void) {
    static char out[160];
    char iso[32] = "-";
    if (time_is_valid()) {
        time_t v = (time_t)time_now_unix();
        struct tm tm;
        localtime_r(&v, &tm);
        strftime(iso, sizeof(iso), "%Y-%m-%dT%H:%M:%S", &tm);
    }
    snprintf(out, sizeof(out),
             "{\"unix\":%lld,\"valid\":%d,\"synced\":%d,\"rtc\":%d,\"utc\":\"%s\"}",
             (long long)time_now_unix(), time_is_valid() ? 1 : 0,
             s_synced ? 1 : 0, rtc_ds3231_present() ? 1 : 0, iso);
    return out;
}

void time_sync_task(void *arg) {
    (void)arg;
    wd_register("time", 60000, false);

    bool written_rtc = false;
    while (1) {
        WD_FEED("time");

        /* SNTP после получения IP */
        if (!s_sntp_started && time_is_valid() == false) {
            esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
            esp_netif_ip_info_t ip;
            if (netif && esp_netif_get_ip_info(netif, &ip) == ESP_OK && ip.ip.addr) {
                sntp_start();
            }
        }

        if (!s_synced && time_is_valid()) {
            s_synced = true;
            events_log(EVENT_INFO, "Time synchronised");
            if (rtc_ds3231_present() && !written_rtc) {
                written_rtc = true;
                if (rtc_write((uint32_t)time_now_unix()) == ESP_OK)
                    ESP_LOGI(TAG, "DS3231 updated");
            }
        }

        /* периодическая запись RTC раз в 30 минут */
        static int64_t last_rtc_us = 0;
        if (s_synced && rtc_ds3231_present() &&
            esp_timer_get_time() - last_rtc_us > 30LL * 60 * 1000000LL) {
            last_rtc_us = esp_timer_get_time();
            rtc_write((uint32_t)time_now_unix());
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
