/*
 * RS485 slave-интерфейс контроллера для внешней HMI-панели Waveshare 7".
 *
 * Важно:
 *  - панель — master шины, контроллер отвечает только на запросы;
 *  - HMI не обходит safety_gate и не сбрасывает E-STOP;
 *  - все команды проходят через те же проверки, что и REST /api/;
 *  - телеметрия собирается свежим state_snapshot() на КАЖДЫЙ POLL.
 */
#include "rs485_hmi.h"

#include "hmi_protocol.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "config.h"
#include "core/estop.h"
#include "core/process_control.h"
#include "core/state.h"
#include "control/ctrl_dose.h"
#include "control/ctrl_heater.h"
#include "control/ctrl_safety.h"
#include "control/safety_gate.h"
#include "hal/hal_safety_relay.h"
#include "hal/pressure_xgzp6897d.h"
#include "hal/pressure_pt506.h"
#include "hal/zmpt_rms.h"
#include "hal/hal_lsa_pwm.h"
#include "esp_system.h"
#include "modes/mode_base.h"

static const char *TAG = "RS485_HMI";

#ifndef CONFIG_HMI_RS485_ENABLE
#define CONFIG_HMI_RS485_ENABLE 0
#endif
#ifndef CONFIG_HMI_RS485_UART_NUM
#define CONFIG_HMI_RS485_UART_NUM 2
#endif
#ifndef CONFIG_HMI_RS485_BAUD
#define CONFIG_HMI_RS485_BAUD 115200
#endif
#ifndef CONFIG_HMI_RS485_TX_GPIO
#define CONFIG_HMI_RS485_TX_GPIO (-1)
#endif
#ifndef CONFIG_HMI_RS485_RX_GPIO
#define CONFIG_HMI_RS485_RX_GPIO (-1)
#endif
#ifndef CONFIG_HMI_RS485_DE_GPIO
#define CONFIG_HMI_RS485_DE_GPIO (-1)
#endif

#define RS485_STACK_BYTES   6144
#define RS485_TASK_PRIO     5
#define RS485_READ_TICK_MS  20

static bool s_ready;
static uint8_t s_rx[HMI_FRAME_MAX];
static size_t s_rx_len;

typedef struct {
    const char *id;
    process_type_t type;
} hmi_mode_map_t;

static const hmi_mode_map_t HMI_MODES[] = {
    {"distill", PROCESS_DISTILL}, {"rectify", PROCESS_RECTIFY},
    {"beer", PROCESS_BEER},       {"pvk", PROCESS_PVK},
    {"nbk", PROCESS_NBK},         {"vacuum", PROCESS_VACUUM},
    {"double", PROCESS_DOUBLE},   {"gabriel", PROCESS_GABRIEL},
    {"ferment", PROCESS_FERMENT},
};

static process_type_t find_mode_type(const char *id, bool *found)
{
    if (found) *found = false;
    if (!id || !*id) return PROCESS_IDLE;
    for (size_t i = 0; i < sizeof(HMI_MODES) / sizeof(HMI_MODES[0]); i++) {
        if (strcmp(HMI_MODES[i].id, id) == 0) {
            if (found) *found = true;
            return HMI_MODES[i].type;
        }
    }
    return PROCESS_IDLE;
}

static void send_frame(uint16_t seq, uint8_t type, const char *payload)
{
    uint8_t out[HMI_FRAME_MAX];
    size_t out_len = 0;
    uint16_t len = payload ? (uint16_t)strnlen(payload, HMI_MAX_PAYLOAD) : 0u;

    if (!hmi_frame_build(out, &out_len, seq, HMI_NODE_CONTROLLER, type,
                         (const uint8_t *)payload, len)) {
        ESP_LOGE(TAG, "не удалось собрать кадр type=0x%02X len=%u", type, len);
        return;
    }
    uart_write_bytes(CONFIG_HMI_RS485_UART_NUM, out, out_len);
}

static void send_ack(uint16_t seq, bool ok, const char *detail)
{
    char json[192];
    if (ok) {
        snprintf(json, sizeof(json), "{\"ok\":true%s%s}",
                 detail ? ",\"info\":\"" : "", detail ? detail : "");
        if (detail) strncat(json, "\"", sizeof(json) - strlen(json) - 1);
    } else {
        snprintf(json, sizeof(json), "{\"ok\":false,\"err\":\"%s\"}",
                 detail ? detail : "unknown");
    }
    send_frame(seq, HMI_TYPE_ACK, json);
}

static void fill_telemetry(char *buf, size_t len)
{
    process_state_t s;
    state_snapshot(&s);

    const char *stage = state_stage_name(s.stage);
    const char *mode = state_type_name(s.type);
    if (!stage) stage = "?";
    if (!mode) mode = "?";

    const bool estop_active = estop_is_pressed();
    const bool actions_blocked = estop_actions_blocked();
    const bool bench_mode = estop_bench_mode();
    const float pt506_bar = pt506_pressure_bar();
    const float xgzp_kpa = pressure_xgzp6897d_kpa();
    const float lsa_duty = hal_lsa_pwm_get_duty_pct();
    const float zmpt_ctrl = zmpt_rms_ctrl_v();
    snprintf(buf, len,
             "{\"v\":\"%s\",\"st\":\"%s\",\"mode\":\"%s\",\"type\":%d,"
             "\"run\":%d,\"pause\":%d,"
             "\"tc\":%.2f,\"tk\":%.2f,\"td\":%.2f,\"tj\":%.2f,"
             "\"p\":%.3f,\"vac\":%.1f,\"flow\":%.2f,"
             "\"power\":%.1f,\"hp\":%.0f,\"bp\":%.0f,\"tp\":%.0f,"
             "\"safety\":%d,\"estop\":%d,\"relay\":%d,"
             "\"estop_active\":%d,\"actions_blocked\":%d,\"bench_mode\":%d,"
             "\"estop_mode\":%d,\"reason\":\"%s\","
             "\"lsa_pwm\":%.1f,\"pt506_bar\":%.3f,\"xgzp_kpa\":%.3f,"
             "\"zmpt_v1\":%.1f,\"zmpt_v2\":%.1f,\"zmpt_v3\":%.1f,"
             "\"zmpt_ctrl\":%.1f,\"zmpt_fault\":%d,"
             "\"sa\":%s,\"es\":%s,"
             "\"ts\":%.2f,\"m\":%.3f,\"lv\":0,\"r\":%d,"
             "\"stg\":%d,\"heads\":%.0f,\"body\":%.0f,\"tails\":%.0f,"
             "\"sc_rdy\":%d,\"sc_act\":%d,\"sc_g\":%.1f,\"sc_flow\":%.2f,\"sc_ml\":%.1f,"
             "\"elapsed\":%u,\"frac\":\"\",\"baro\":0,\"sd\":%s,\"db\":false,"
             "\"heap\":%u,\"epoch\":0,\"ev\":\"\"}",
             FW_VERSION, stage, mode, (int)s.type,
             s.is_running ? 1 : 0, s.is_paused ? 1 : 0,
             s.cube_temp, s.column_temp, s.deph_temp, s.jacket_temp,
             s.pressure_bar, s.vacuum_mbar, s.flow_lpm,
             s.power_percent,
             s.heads_collected_ml, s.body_collected_ml, s.tails_collected_ml,
             ctrl_safety_ok() ? 1 : 0,
             estop_active ? 1 : 0,
             hal_safety_relay_is_closed() ? 1 : 0,
             estop_active ? 1 : 0, actions_blocked ? 1 : 0, bench_mode ? 1 : 0,
             (int)estop_get_mode(), s.actions_block_reason,
             lsa_duty, pt506_bar, xgzp_kpa,
             zmpt_rms_phase_v(0), zmpt_rms_phase_v(1), zmpt_rms_phase_v(2),
             zmpt_ctrl, zmpt_rms_fault() ? 1 : 0,
             ctrl_safety_ok() ? "true" : "false",
             estop_active ? "true" : "false",
             s.target_cube_t, s.pressure_bar, s.is_running ? 1 : 0,
             (int)s.stage,
             s.heads_collected_ml, s.body_collected_ml, s.tails_collected_ml,
             s.scale_ready ? 1 : 0, s.scale_indicator_active ? 1 : 0,
             (double)s.scale_weight_g, (double)s.scale_flow_ml_min, (double)s.scale_collected_ml,
             (unsigned)s.elapsed_sec,
             ctrl_safety_ok() ? "true" : "false",
             (unsigned)esp_get_free_heap_size());
}

static void send_config(uint16_t seq)
{
    char json[384];
    snprintf(json, sizeof(json),
             "{\"fw\":\"%s\",\"node\":%d,\"proto\":\"%s\","
             "\"baud\":%d,\"poll_ms\":500,\"timeout_ms\":100,"
             "\"modes\":[\"distill\",\"rectify\",\"beer\",\"pvk\","
             "\"nbk\",\"vacuum\",\"double\",\"gabriel\",\"ferment\"]}",
             FW_VERSION, HMI_NODE_CONTROLLER, HMI_PROTO_VERSION_STR,
             CONFIG_HMI_RS485_BAUD);
    send_frame(seq, HMI_TYPE_CONFIG, json);
}

static void send_pong(uint16_t seq)
{
    char json[128];
    snprintf(json, sizeof(json), "{\"fw\":\"%s\",\"node\":%d,\"proto\":\"%s\"}",
             FW_VERSION, HMI_NODE_CONTROLLER, HMI_PROTO_VERSION_STR);
    send_frame(seq, HMI_TYPE_PONG, json);
}

static void handle_cmd(uint16_t seq, const hmi_frame_t *fr)
{
    cJSON *root = cJSON_Parse((const char *)fr->payload);
    if (!root) {
        send_ack(seq, false, "json");
        return;
    }

    const cJSON *cj = cJSON_GetObjectItemCaseSensitive(root, "cmd");
    const char *cmd = cJSON_IsString(cj) ? cj->valuestring : "";
    char detail[128] = {0};
    bool ok = false;

    if (strcmp(cmd, "start") == 0) {
        const cJSON *mj = cJSON_GetObjectItemCaseSensitive(root, "mode");
        char mode_id[32] = {0};
        if (cJSON_IsString(mj)) {
            strncpy(mode_id, mj->valuestring, sizeof(mode_id) - 1);
        }
        const cJSON *tj = cJSON_GetObjectItemCaseSensitive(root, "type");
        bool have_mode = false;
        process_type_t type = have_mode ? PROCESS_IDLE : state_get()->type;
        if (cJSON_IsNumber(tj)) type = (process_type_t)tj->valueint;
        if (mode_id[0]) type = find_mode_type(mode_id, &have_mode);

        const process_mode_t *m = mode_id[0] ? mode_from_type(type) : mode_get();
        if (!m) m = mode_from_type(PROCESS_DISTILL);

        char why[96] = {0};
        if (!m) {
            snprintf(detail, sizeof(detail), "mode");
        } else if (!process_control_hw_ready(type, why, sizeof(why))) {
            snprintf(detail, sizeof(detail), "%s", why);
        } else if (!ctrl_safety_ok()) {
            snprintf(detail, sizeof(detail), "safety");
        } else if (estop_actions_blocked()) {
            snprintf(detail, sizeof(detail), estop_bench_mode() ? "bench_mode" : "estop");
        } else {
            mode_set(m);
            process_state_t *st = state_get();
            st->type = type;
            st->is_running = true;
            st->is_paused = false;
            ok = true;
        }
    } else if (strcmp(cmd, "pause") == 0) {
        if (!state_get()->is_running) {
            snprintf(detail, sizeof(detail), "not running");
        } else {
            state_get()->is_paused = true;
            ok = true;
        }
    } else if (strcmp(cmd, "stop") == 0) {
        ctrl_heater_force_off();
        ctrl_dose_stop();
        if (mode_get() && mode_get()->stop) mode_get()->stop();
        state_get()->is_running = false;
        state_get()->is_paused = false;
        ok = true;
    } else if (strcmp(cmd, "power") == 0) {
        if (estop_actions_blocked()) {
            snprintf(detail, sizeof(detail), estop_bench_mode() ? "bench_mode" : "estop");
        } else {
        const cJSON *v = cJSON_GetObjectItemCaseSensitive(root, "value");
        if (!v) v = cJSON_GetObjectItemCaseSensitive(root, "power");
        if (cJSON_IsString(v) && strcmp(v->valuestring, "auto") == 0) {
            ctrl_heater_set_auto();
            ok = true;
        } else if (cJSON_IsNumber(v)) {
            float pct = (float)v->valuedouble;
            if (pct < 0.0f) pct = 0.0f;
            if (pct > 100.0f) pct = 100.0f;
            esp_err_t err = safety_gate_submit("hmi_power", pct);
            ok = (err == ESP_OK);
            if (!ok) snprintf(detail, sizeof(detail), "gate");
        } else {
            snprintf(detail, sizeof(detail), "value");
        }
        }
    } else if (strcmp(cmd, "safety_reset") == 0) {
        if (estop_is_pressed()) {
            snprintf(detail, sizeof(detail), "estop");
        } else if (estop_bench_mode()) {
            snprintf(detail, sizeof(detail), "bench_mode");
        } else if (!ctrl_safety_ok()) {
            esp_err_t err = ctrl_safety_reset();
            ok = (err == ESP_OK);
            if (!ok) {
                snprintf(detail, sizeof(detail), "%s",
                         ctrl_safety_reset_error() ? ctrl_safety_reset_error()
                                                   : "refused");
            }
        } else {
            ok = true;
        }
    } else {
        snprintf(detail, sizeof(detail), "unknown cmd");
    }

    cJSON_Delete(root);
    send_ack(seq, ok, ok ? NULL : detail);
}

static void handle_frame(const hmi_frame_t *fr)
{
    static char json[HMI_MAX_PAYLOAD + 1];

    switch (fr->type) {
    case HMI_TYPE_POLL:
        fill_telemetry(json, sizeof(json));
        send_frame(fr->seq, HMI_TYPE_TELEMETRY, json);
        break;
    case HMI_TYPE_CMD:
        handle_cmd(fr->seq, fr);
        break;
    case HMI_TYPE_GET_CONFIG:
        send_config(fr->seq);
        break;
    case HMI_TYPE_PING:
        send_pong(fr->seq);
        break;
    default:
        ESP_LOGD(TAG, "неизвестный TYPE=0x%02X", fr->type);
        break;
    }
}

static void rs485_task(void *arg)
{
    (void)arg;

    while (1) {
        uint8_t byte = 0;
        int n = uart_read_bytes(CONFIG_HMI_RS485_UART_NUM, &byte, 1,
                                pdMS_TO_TICKS(RS485_READ_TICK_MS));
        if (n <= 0) {
            s_rx_len = 0; /* тайм-аут между кадрами — начинаем заново */
            continue;
        }

        if (s_rx_len == 0) {
            if (byte != HMI_MAGIC0) continue;
        } else if (s_rx_len == 1) {
            if (byte != HMI_MAGIC1) {
                s_rx_len = (byte == HMI_MAGIC0) ? 1u : 0u;
                continue;
            }
        }

        s_rx[s_rx_len++] = byte;

        if (s_rx_len >= 4) {
            uint16_t payload_len = (uint16_t)s_rx[2] | ((uint16_t)s_rx[3] << 8);
            size_t need = HMI_FRAME_OVERHEAD + payload_len;
            if (need > sizeof(s_rx)) {
                s_rx_len = 0;
                continue;
            }
            if (s_rx_len < need) continue;

            hmi_frame_t fr;
            if (hmi_frame_parse(s_rx, need, &fr)) {
                handle_frame(&fr);
            } else {
                ESP_LOGD(TAG, "отвергнут кадр len=%u crc/format", (unsigned)need);
            }
            s_rx_len = 0;
        }
    }
}

esp_err_t rs485_hmi_init(void)
{
#if !CONFIG_HMI_RS485_ENABLE
    ESP_LOGI(TAG, "RS485 HMI отключён CONFIG_HMI_RS485_ENABLE=n");
    return ESP_OK;
#else
    if (CONFIG_HMI_RS485_TX_GPIO < 0 || CONFIG_HMI_RS485_RX_GPIO < 0 ||
        CONFIG_HMI_RS485_DE_GPIO < 0) {
        ESP_LOGW(TAG,
                 "RS485 HMI не запущен: задайте TX/RX/DE GPIO в menuconfig "
                 "(не используйте 14..21 — это SPI дисплея контроллера)");
        return ESP_ERR_INVALID_ARG;
    }

    uart_config_t cfg = {
        .baud_rate = CONFIG_HMI_RS485_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };

    esp_err_t err = uart_param_config(CONFIG_HMI_RS485_UART_NUM, &cfg);
    if (err != ESP_OK) return err;

    /* DE и /RE связаны вместе: RTS драйвера UART управляет направлением. */
    err = uart_set_pin(CONFIG_HMI_RS485_UART_NUM,
                       CONFIG_HMI_RS485_TX_GPIO,
                       CONFIG_HMI_RS485_RX_GPIO,
                       CONFIG_HMI_RS485_DE_GPIO,
                       UART_PIN_NO_CHANGE);
    if (err != ESP_OK) return err;

    err = uart_set_mode(CONFIG_HMI_RS485_UART_NUM, UART_MODE_RS485_HALF_DUPLEX);
    if (err != ESP_OK) return err;

    err = uart_driver_install(CONFIG_HMI_RS485_UART_NUM,
                              1024, 512, 0, NULL, 0);
    if (err != ESP_OK) return err;

    BaseType_t rc = xTaskCreatePinnedToCore(
        rs485_task, "rs485_hmi", RS485_STACK_BYTES, NULL,
        RS485_TASK_PRIO, NULL, 1);
    if (rc != pdPASS) {
        uart_driver_delete(CONFIG_HMI_RS485_UART_NUM);
        return ESP_ERR_NO_MEM;
    }

    s_ready = true;
    ESP_LOGI(TAG, "RS485 HMI готов: UART%d %d 8N1, TX=%d RX=%d DE=%d",
             CONFIG_HMI_RS485_UART_NUM, CONFIG_HMI_RS485_BAUD,
             CONFIG_HMI_RS485_TX_GPIO, CONFIG_HMI_RS485_RX_GPIO,
             CONFIG_HMI_RS485_DE_GPIO);
    return ESP_OK;
#endif
}

bool rs485_hmi_ready(void)
{
    return s_ready;
}
