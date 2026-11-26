#include "hmi_rs485.h"

#include <assert.h>
#include <string.h>

#include "driver/uart.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "hmi_proto.h"
#include "hmi_telemetry.h"

static const char *TAG = "hmi_rs485";

#define HMI_CMD_QUEUE_LEN   8
#define HMI_CMD_STR_LEN     128

static QueueHandle_t s_cmd_q;
static uint16_t s_seq;
static uint32_t s_crc_err;
static uint32_t s_frames;
static uint16_t s_latency_ms;
static bool s_link;
static int64_t s_last_tx_us;

static uint8_t s_rx[HMI_MAX_PAYLOAD + HMI_FRAME_OVERHEAD];
static size_t s_rx_have;

static void send_frame(uint8_t type, const uint8_t *payload, uint16_t len)
{
    uint8_t out[HMI_MAX_PAYLOAD + HMI_FRAME_OVERHEAD];
    size_t out_len = 0;

    if (!hmi_frame_build(out, &out_len, s_seq, HMI_NODE_HMI, type,
                         payload, len)) {
        return;
    }

    s_last_tx_us = esp_timer_get_time();
    uart_write_bytes(HMI_UART_NUM, out, out_len);
    /* Пауза после TX обязательна для half-duplex линии. */
    esp_rom_delay_us(HMI_TX_GAP_US);
}

static bool wait_frame(hmi_frame_t *frame, uint32_t timeout_ms)
{
    int64_t start = esp_timer_get_time();
    int64_t deadline_us = (int64_t)timeout_ms * 1000;

    while (esp_timer_get_time() - start < deadline_us) {
        uint8_t b;
        int n = uart_read_bytes(HMI_UART_NUM, &b, 1, 5);
        if (n <= 0) continue;

        if (s_rx_have == 0) {
            if (b == 0xAA) {
                s_rx[0] = b;
                s_rx_have = 1;
            }
            continue;
        }

        if (s_rx_have == 1) {
            if (b == 0x55) {
                s_rx[1] = b;
                s_rx_have = 2;
            } else if (b == 0xAA) {
                s_rx[0] = b; /* остаёмся в состоянии «нашли AA» */
            } else {
                s_rx_have = 0;
            }
            continue;
        }

        if (s_rx_have < sizeof(s_rx)) {
            s_rx[s_rx_have++] = b;
        } else {
            s_rx_have = 0;
            continue;
        }

        if (s_rx_have >= 4) {
            uint16_t payload_len =
                (uint16_t)s_rx[2] | ((uint16_t)s_rx[3] << 8);
            if (payload_len > HMI_MAX_PAYLOAD) {
                s_rx_have = 0;
                continue;
            }

            size_t expected = HMI_FRAME_OVERHEAD + payload_len;
            if (s_rx_have >= expected) {
                bool ok = hmi_frame_parse(s_rx, expected, frame);
                s_rx_have = 0;
                if (ok) {
                    s_latency_ms = (uint16_t)
                        ((esp_timer_get_time() - s_last_tx_us) / 1000);
                    s_frames++;
                    return true;
                }
                s_crc_err++;
            }
        }
    }
    return false;
}

static void hmi_rs485_task(void *arg)
{
    (void)arg;
    TickType_t last_wake = xTaskGetTickCount();
    char cmd[HMI_CMD_STR_LEN];
    int misses = 0;

    while (1) {
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(HMI_POLL_MS));

        /* Одна команда из UI между опросами. */
        if (xQueueReceive(s_cmd_q, cmd, 0) == pdTRUE) {
            s_seq++;
            uint16_t cmd_seq = s_seq;
            send_frame(HMI_TYPE_CMD, (const uint8_t *)cmd,
                       (uint16_t)strlen(cmd));

            hmi_frame_t ack;
            if (wait_frame(&ack, HMI_REPLY_TIMEOUT_MS) &&
                ack.type == HMI_TYPE_ACK && ack.seq == cmd_seq) {
                ESP_LOGI(TAG, "CMD ack: %.*s", ack.len, ack.payload);
            } else {
                ESP_LOGW(TAG, "CMD ack timeout/bad seq=%u", cmd_seq);
            }
        }

        /* POLL с повторами. */
        bool got = false;
        hmi_frame_t frame;
        for (int attempt = 0; attempt <= HMI_MAX_RETRIES && !got; attempt++) {
            s_seq++;
            send_frame(HMI_TYPE_POLL, NULL, 0);
            if (wait_frame(&frame, HMI_REPLY_TIMEOUT_MS) &&
                frame.type == HMI_TYPE_TELEMETRY) {
                got = true;
            }
        }

        if (got) {
            misses = 0;
            s_link = true;
            hmi_telemetry_from_json(frame.payload, frame.len);
        } else {
            misses++;
            if (misses >= HMI_LINK_MISS_LIMIT) s_link = false;
        }

        hmi_telemetry_set_link(s_link);
        hmi_telemetry_set_stats(s_crc_err, s_frames, s_latency_ms);
    }
}

void hmi_rs485_init(void)
{
    uart_config_t cfg = {
        .baud_rate  = HMI_UART_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_param_config(HMI_UART_NUM, &cfg));
    /* На плате Waveshare направление RS485 переключается автоматически. */
    ESP_ERROR_CHECK(uart_set_pin(HMI_UART_NUM,
                                 HMI_UART_TX_GPIO, HMI_UART_RX_GPIO,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(HMI_UART_NUM,
                                        HMI_UART_RX_BUF_SIZE,
                                        HMI_UART_TX_BUF_SIZE,
                                        0, NULL, 0));

    s_cmd_q = xQueueCreate(HMI_CMD_QUEUE_LEN, HMI_CMD_STR_LEN);
    assert(s_cmd_q != NULL);

    hmi_telemetry_reset();
    xTaskCreatePinnedToCore(hmi_rs485_task, "hmi_rs485", 4096, NULL, 5, NULL, 0);

    ESP_LOGI(TAG, "UART%d %d baud, TX=%d RX=%d, poll=%d ms, timeout=%d ms",
             HMI_UART_NUM, HMI_UART_BAUD,
             HMI_UART_TX_GPIO, HMI_UART_RX_GPIO,
             HMI_POLL_MS, HMI_REPLY_TIMEOUT_MS);
}

esp_err_t hmi_rs485_enqueue_cmd(const char *json)
{
    if (!json || !s_cmd_q) return ESP_FAIL;

    char buf[HMI_CMD_STR_LEN];
    strncpy(buf, json, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    return xQueueSend(s_cmd_q, buf, 0) == pdTRUE ? ESP_OK : ESP_FAIL;
}

void hmi_rs485_get_stats(uint32_t *crc_err, uint32_t *frames,
                         uint16_t *latency_ms, bool *link)
{
    if (crc_err)    *crc_err    = s_crc_err;
    if (frames)     *frames     = s_frames;
    if (latency_ms) *latency_ms = s_latency_ms;
    if (link)       *link       = s_link;
}
