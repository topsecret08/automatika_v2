#include "hmi_can.h"

#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "driver/twai.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "can_hmi_proto.h"
#include "hmi_proto.h"
#include "hmi_telemetry.h"
#include "hmi_storage.h"

static const char *TAG = "hmi_can";

#define HMI_CMD_QUEUE_LEN 8
#define HMI_CMD_STR_LEN   128

static QueueHandle_t s_cmd_q;
static uint8_t s_seq;
static uint32_t s_rx_err;
static uint32_t s_frames;
static uint16_t s_latency_ms;
static bool s_link;

static esp_err_t can_send(uint32_t id, const uint8_t data[8])
{
    twai_message_t msg = {0};
    msg.identifier = id;
    msg.extd = 0;
    msg.rtr = 0;
    msg.data_length_code = 8;
    memcpy(msg.data, data, 8);
    return twai_transmit(&msg, pdMS_TO_TICKS(50));
}

static void send_poll(uint8_t seq)
{
    uint8_t d[8] = { seq, 0, 0, 0, 0, 0, 0, 0 };
    can_send(CAN_HMI_ID_POLL, d);
}

static bool parse_cmd_json(const char *json, can_hmi_cmd_t *out)
{
    memset(out, 0, sizeof(*out));
    cJSON *root = cJSON_Parse(json);
    if (!root) return false;
    const cJSON *cj = cJSON_GetObjectItemCaseSensitive(root, "cmd");
    const char *cmd = cJSON_IsString(cj) ? cj->valuestring : "";
    bool ok = true;

    if (strcmp(cmd, "start") == 0) {
        out->cmd = CAN_HMI_CMD_START;
        const cJSON *tj = cJSON_GetObjectItemCaseSensitive(root, "type");
        if (cJSON_IsNumber(tj)) out->p0 = (uint8_t)tj->valueint;
        else {
            const cJSON *mj = cJSON_GetObjectItemCaseSensitive(root, "mode");
            const char *mode = cJSON_IsString(mj) ? mj->valuestring : "";
            const struct { const char *id; uint8_t type; } modes[] = {
                {"distill",1},{"rectify",2},{"beer",3},{"pvk",4},{"nbk",5},
                {"vacuum",6},{"double",7},{"gabriel",8},{"ferment",9},
            };
            for (size_t i = 0; i < sizeof(modes)/sizeof(modes[0]); i++)
                if (strcmp(mode, modes[i].id) == 0) { out->p0 = modes[i].type; break; }
        }
    } else if (strcmp(cmd, "stop") == 0) {
        out->cmd = CAN_HMI_CMD_STOP;
    } else if (strcmp(cmd, "pause") == 0) {
        out->cmd = CAN_HMI_CMD_PAUSE;
    } else if (strcmp(cmd, "power") == 0) {
        out->cmd = CAN_HMI_CMD_POWER;
        const cJSON *v = cJSON_GetObjectItemCaseSensitive(root, "value");
        if (!v) v = cJSON_GetObjectItemCaseSensitive(root, "power");
        if (cJSON_IsString(v) && strcmp(v->valuestring, "auto") == 0) {
            out->p0 = 0x01;
        } else if (cJSON_IsNumber(v)) {
            float pct = (float)v->valuedouble;
            if (pct < 0) pct = 0;
            if (pct > 100) pct = 100;
            uint16_t x10 = (uint16_t)(pct * 10.0f);
            out->p0 = 0x00;
            can_hmi_put_u16(&out->p1, x10);
        } else ok = false;
    } else if (strcmp(cmd, "safety_reset") == 0) {
        out->cmd = CAN_HMI_CMD_SAFETY_RESET;
    } else if (strcmp(cmd, "ping") == 0) {
        out->cmd = CAN_HMI_CMD_PING;
    } else {
        ok = false;
    }

    cJSON_Delete(root);
    return ok;
}

static void hmi_can_recover_if_bus_off(void);

static bool hmi_can_write_event(uint32_t ts_ms, uint8_t type, const char *msg)
{
    if (!hmi_storage_sd_ready()) return false;
    char safe[96];
    size_t j = 0;
    for (size_t i = 0; msg && msg[i] && j < sizeof(safe) - 1; i++) {
        char c = msg[i];
        if (c == '\n' || c == '\r' || c == ',') c = ';';
        safe[j++] = c;
    }
    safe[j] = '\0';
    char line[160];
    snprintf(line, sizeof(line), "%lu,%u,%s",
             (unsigned long)ts_ms, (unsigned)type, safe);
    return hmi_storage_append_line(HMI_SD_MOUNT_POINT "/logs/events.csv", line);
}

static uint8_t hmi_can_poll_logs(uint8_t seq, uint8_t acked)
{
    uint8_t d[8] = { seq, acked, 0, 0, 0, 0, 0, 0 };
    can_send(CAN_HMI_ID_LOG_POLL, d);

    struct {
        bool active;
        bool complete;
        bool written;
        uint32_t ts_ms;
        uint8_t type;
        char msg[96];
        size_t len;
    } recs[2];
    memset(recs, 0, sizeof(recs));

    int64_t deadline = esp_timer_get_time() + 250000; /* 250ms */
    bool done = false;
    uint8_t sent_events = 0;
    while (esp_timer_get_time() < deadline && !done) {
        twai_message_t msg;
        int64_t remain_ms = (deadline - esp_timer_get_time()) / 1000;
        if (remain_ms < 1) remain_ms = 1;
        esp_err_t rr = twai_receive(&msg, pdMS_TO_TICKS((uint32_t)remain_ms));
        if (rr != ESP_OK) {
            if (rr != ESP_ERR_TIMEOUT) hmi_can_recover_if_bus_off();
            continue;
        }
        if (msg.extd || msg.rtr || msg.data_length_code < 2) continue;
        uint8_t raw[8] = {0};
        memcpy(raw, msg.data, msg.data_length_code < 8 ? msg.data_length_code : 8);
        if (raw[0] != seq) continue;

        if (msg.identifier == CAN_HMI_ID_LOG) {
            can_hmi_log_t lf;
            if (!can_hmi_unpack_log(raw, &lf) || lf.rec >= 2) continue;
            if (lf.flags & CAN_HMI_LOG_F_START) {
                recs[lf.rec].active = true;
                recs[lf.rec].complete = false;
                recs[lf.rec].written = false;
                recs[lf.rec].ts_ms = can_hmi_u32(lf.data);
                recs[lf.rec].type = lf.type;
                recs[lf.rec].len = 0;
                recs[lf.rec].msg[0] = '\0';
                if (lf.flags & CAN_HMI_LOG_F_END) {
                    recs[lf.rec].complete = true;
                    recs[lf.rec].active = false;
                    recs[lf.rec].written = hmi_can_write_event(recs[lf.rec].ts_ms, recs[lf.rec].type, recs[lf.rec].msg);
                }
            } else if (recs[lf.rec].active) {
                size_t space = sizeof(recs[lf.rec].msg) - 1 - recs[lf.rec].len;
                size_t chunk = space < 4 ? space : 4;
                for (size_t i = 0; i < chunk; i++) {
                    char c = (char)lf.data[i];
                    if (c == '\0') { chunk = i; break; }
                }
                if (chunk > 0) {
                    memcpy(recs[lf.rec].msg + recs[lf.rec].len, lf.data, chunk);
                    recs[lf.rec].len += chunk;
                    recs[lf.rec].msg[recs[lf.rec].len] = '\0';
                }
                if (lf.flags & CAN_HMI_LOG_F_END) {
                    recs[lf.rec].complete = true;
                    recs[lf.rec].active = false;
                    recs[lf.rec].written = hmi_can_write_event(recs[lf.rec].ts_ms, recs[lf.rec].type, recs[lf.rec].msg);
                }
            }
        } else if (msg.identifier == CAN_HMI_ID_ACK) {
            can_hmi_ack_t ack;
            if (can_hmi_unpack_ack(raw, &ack) && ack.kind == CAN_HMI_ACK_LOG_END) {
                sent_events = ack.data[0];
                done = true;
            }
        }
    }

    if (!done) return 0;
    uint8_t acked_out = 0;
    for (uint8_t i = 0; i < sent_events && i < 2; i++) {
        if (recs[i].complete && recs[i].written) acked_out++;
        else break;
    }
    return acked_out;
}

static void hmi_can_recover_if_bus_off(void)
{
    twai_status_info_t st = {0};
    if (twai_get_status_info(&st) != ESP_OK) return;
    if (st.state == TWAI_STATE_BUS_OFF) {
        ESP_LOGW(TAG, "TWAI bus-off, initiating recovery");
        twai_initiate_recovery();
    } else if (st.state == TWAI_STATE_STOPPED) {
        ESP_LOGW(TAG, "TWAI stopped, restarting");
        twai_start();
    }
}

static void hmi_can_task(void *arg)
{
    (void)arg;
    TickType_t last_wake = xTaskGetTickCount();
    char cmd_json[HMI_CMD_STR_LEN];
    int misses = 0;
    int log_div = 0;
    uint8_t log_acked = 0;

    while (1) {
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(HMI_POLL_MS));

        if (xQueueReceive(s_cmd_q, cmd_json, 0) == pdTRUE) {
            can_hmi_cmd_t cmd;
            if (parse_cmd_json(cmd_json, &cmd)) {
                s_seq++;
                cmd.seq = s_seq;
                uint8_t d[8];
                if (can_hmi_pack_cmd(d, &cmd)) {
                    can_send(CAN_HMI_ID_CMD, d);
                    /* wait briefly for ACK */
                    twai_message_t ack;
                    int64_t t0 = esp_timer_get_time();
                    while (esp_timer_get_time() - t0 < (int64_t)HMI_REPLY_TIMEOUT_MS * 1000) {
                        esp_err_t rr = twai_receive(&ack, pdMS_TO_TICKS(20));
                        if (rr != ESP_OK) {
                            if (rr != ESP_ERR_TIMEOUT) hmi_can_recover_if_bus_off();
                            continue;
                        }
                        if (!ack.extd && !ack.rtr && ack.identifier == CAN_HMI_ID_ACK &&
                            ack.data_length_code >= 4 && ack.data[0] == cmd.seq) {
                            s_frames++;
                            break;
                        }
                    }
                }
            }
        }

        s_seq++;
        uint8_t poll_seq = s_seq;
        int64_t tx_us = esp_timer_get_time();
        send_poll(poll_seq);

        bool got_a=false, got_b=false, got_c=false, got_d=false, got_e=false, got_f=false, got_g=false, got_snap=false;
        can_hmi_tlm_a_t ta; can_hmi_tlm_b_t tb; can_hmi_tlm_c_t tc; can_hmi_tlm_d_t td;
        can_hmi_tlm_e_t te; can_hmi_tlm_f_t tf; can_hmi_tlm_g_t tg;
        uint16_t heap_kb = 0; uint32_t elapsed_s = 0;

        int64_t deadline = esp_timer_get_time() + (int64_t)HMI_REPLY_TIMEOUT_MS * 1000;
        while (esp_timer_get_time() < deadline) {
            twai_message_t msg;
            int64_t remain_ms = (deadline - esp_timer_get_time()) / 1000;
            if (remain_ms < 1) remain_ms = 1;
            esp_err_t rr = twai_receive(&msg, pdMS_TO_TICKS((uint32_t)remain_ms));
            if (rr != ESP_OK) {
                if (rr != ESP_ERR_TIMEOUT) hmi_can_recover_if_bus_off();
                continue;
            }
            if (msg.extd || msg.rtr || msg.data_length_code < 2) continue;
            if (msg.data[0] != poll_seq) continue;
            uint8_t d[8] = {0};
            memcpy(d, msg.data, msg.data_length_code < 8 ? msg.data_length_code : 8);
            s_frames++;
            switch (msg.identifier) {
            case CAN_HMI_ID_TLM_A: got_a = can_hmi_unpack_tlm_a(d, &ta); break;
            case CAN_HMI_ID_TLM_B: got_b = can_hmi_unpack_tlm_b(d, &tb); break;
            case CAN_HMI_ID_TLM_C: got_c = can_hmi_unpack_tlm_c(d, &tc); break;
            case CAN_HMI_ID_TLM_D: got_d = can_hmi_unpack_tlm_d(d, &td); break;
            case CAN_HMI_ID_TLM_E: got_e = can_hmi_unpack_tlm_e(d, &te); break;
            case CAN_HMI_ID_TLM_F: got_f = can_hmi_unpack_tlm_f(d, &tf); break;
            case CAN_HMI_ID_TLM_G: got_g = can_hmi_unpack_tlm_g(d, &tg); break;
            case CAN_HMI_ID_ACK: {
                can_hmi_ack_t ack;
                if (can_hmi_unpack_ack(d, &ack) && ack.kind == CAN_HMI_ACK_SNAPSHOT) {
                    heap_kb = can_hmi_u16(&ack.data[0]);
                    elapsed_s = can_hmi_u16(&ack.data[2]);
                    got_snap = true;
                }
                break;
            }
            default: break;
            }
            if (got_a && got_b && got_c && got_d && got_e && got_f && got_g && got_snap) break;
        }

        if (got_a && got_b && got_c && got_d && got_e && got_f && got_g && got_snap) {
            misses = 0;
            s_link = true;
            s_latency_ms = (uint16_t)((esp_timer_get_time() - tx_us) / 1000);
            hmi_telemetry_apply_can(&ta, &tb, &tc, &td, &te, &tf, &tg, heap_kb, elapsed_s);
        } else {
            misses++;
            if (misses >= HMI_LINK_MISS_LIMIT) s_link = false;
        }

        hmi_telemetry_set_link(s_link);
        hmi_telemetry_set_stats(s_rx_err, s_frames, s_latency_ms);

        if (++log_div >= 5) {
            log_div = 0;
            s_seq++;
            log_acked = hmi_can_poll_logs(s_seq, log_acked);
        }
    }
}

void hmi_can_init(void)
{
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
        (gpio_num_t)HMI_CAN_TX_GPIO, (gpio_num_t)HMI_CAN_RX_GPIO, TWAI_MODE_NORMAL);
    g_config.rx_queue_len = 32;
    g_config.tx_queue_len = 16;

    twai_timing_config_t t_config;
#if HMI_CAN_BITRATE_KBPS == 1000
    t_config = (twai_timing_config_t)TWAI_TIMING_CONFIG_1MBITS();
#elif HMI_CAN_BITRATE_KBPS == 250
    t_config = (twai_timing_config_t)TWAI_TIMING_CONFIG_250KBITS();
#else
    t_config = (twai_timing_config_t)TWAI_TIMING_CONFIG_500KBITS();
#endif

    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    ESP_ERROR_CHECK(twai_driver_install(&g_config, &t_config, &f_config));
    ESP_ERROR_CHECK(twai_start());

    s_cmd_q = xQueueCreate(HMI_CMD_QUEUE_LEN, HMI_CMD_STR_LEN);
    configASSERT(s_cmd_q);
    hmi_telemetry_reset();
    xTaskCreatePinnedToCore(hmi_can_task, "hmi_can", 4096, NULL, 5, NULL, 0);
    ESP_LOGI(TAG, "CAN HMI ready: TX=%d RX=%d %d kbps, poll %d ms",
             HMI_CAN_TX_GPIO, HMI_CAN_RX_GPIO, HMI_CAN_BITRATE_KBPS, HMI_POLL_MS);
}

esp_err_t hmi_can_enqueue_cmd(const char *json)
{
    if (!json || !s_cmd_q) return ESP_FAIL;
    char buf[HMI_CMD_STR_LEN];
    strncpy(buf, json, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    return xQueueSend(s_cmd_q, buf, 0) == pdTRUE ? ESP_OK : ESP_FAIL;
}

void hmi_can_get_stats(uint32_t *rx_err, uint32_t *frames,
                       uint16_t *latency_ms, bool *link)
{
    if (rx_err) *rx_err = s_rx_err;
    if (frames) *frames = s_frames;
    if (latency_ms) *latency_ms = s_latency_ms;
    if (link) *link = s_link;
}
