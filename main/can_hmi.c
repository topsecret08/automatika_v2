#include "can_hmi.h"

#include "can_hmi_proto.h"

#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "esp_log.h"
#include "esp_system.h"
#include "driver/twai.h"
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
#include "modes/mode_base.h"
#include "core/events.h"

static const char *TAG = "CAN_HMI";

#ifndef CONFIG_HMI_CAN_ENABLE
#define CONFIG_HMI_CAN_ENABLE 0
#endif
#ifndef CONFIG_HMI_CAN_TX_GPIO
#define CONFIG_HMI_CAN_TX_GPIO (-1)
#endif
#ifndef CONFIG_HMI_CAN_RX_GPIO
#define CONFIG_HMI_CAN_RX_GPIO (-1)
#endif
#ifndef CONFIG_HMI_CAN_BITRATE_KBPS
#define CONFIG_HMI_CAN_BITRATE_KBPS 500
#endif

#define CAN_HMI_STACK_BYTES 6144
#define CAN_HMI_TASK_PRIO   5
#define CAN_HMI_RX_TIMEOUT_MS 100

static bool s_ready;

static const struct { const char *id; process_type_t type; } HMI_MODES[] = {
    {"distill", PROCESS_DISTILL}, {"rectify", PROCESS_RECTIFY},
    {"beer", PROCESS_BEER},       {"pvk", PROCESS_PVK},
    {"nbk", PROCESS_NBK},         {"vacuum", PROCESS_VACUUM},
    {"double", PROCESS_DOUBLE},   {"gabriel", PROCESS_GABRIEL},
    {"ferment", PROCESS_FERMENT},
};

static process_type_t find_mode_type(uint8_t type_id, bool *found)
{
    if (found) *found = false;
    /* type_id is process_type_t numeric value */
    for (size_t i = 0; i < sizeof(HMI_MODES) / sizeof(HMI_MODES[0]); i++) {
        if ((uint8_t)HMI_MODES[i].type == type_id) {
            if (found) *found = true;
            return HMI_MODES[i].type;
        }
    }
    return PROCESS_IDLE;
}

static uint8_t reason_code_from_state(const process_state_t *s)
{
    if (!s) return CAN_HMI_REASON_NONE;
    if (estop_is_pressed()) return CAN_HMI_REASON_ESTOP;
    if (estop_bench_mode()) return CAN_HMI_REASON_BENCH;
    if (s->volume_overflow) return CAN_HMI_REASON_VOL_OVERFLOW;
    if (s->volume_no_flow) return CAN_HMI_REASON_VOL_NO_FLOW;
    if (s->volume_guard_active) return CAN_HMI_REASON_VOL_GUARD;
    if (s->eq_lock_active) return CAN_HMI_REASON_EQ_LOCK;
    if (s->flood_active) return CAN_HMI_REASON_FLOOD;
    if (zmpt_rms_fault()) return CAN_HMI_REASON_ZMPT;
    if (!ctrl_safety_ok()) return CAN_HMI_REASON_SAFETY;
    if (s->actions_block_reason[0]) return CAN_HMI_REASON_HW;
    return CAN_HMI_REASON_NONE;
}

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

static void send_ack(uint8_t seq, uint8_t kind, bool ok, uint8_t err, const uint8_t data4[4])
{
    can_hmi_ack_t a = { .seq = seq, .kind = kind, .ok = ok ? 1 : 0, .err_code = err };
    if (data4) memcpy(a.data, data4, 4);
    uint8_t d[8];
    if (can_hmi_pack_ack(d, &a)) can_send(CAN_HMI_ID_ACK, d);
}

static void send_telemetry_snapshot(uint8_t seq)
{
    process_state_t s;
    state_snapshot(&s);

    const bool estop_active = estop_is_pressed();
    const bool actions_blocked = estop_actions_blocked();
    const bool bench_mode = estop_bench_mode();

    uint8_t flags1 = 0;
    if (s.is_running) flags1 |= CAN_HMI_F1_RUN;
    if (s.is_paused) flags1 |= CAN_HMI_F1_PAUSE;
    if (ctrl_safety_ok()) flags1 |= CAN_HMI_F1_SAFETY;
    if (estop_active) flags1 |= CAN_HMI_F1_ESTOP;
    if (actions_blocked) flags1 |= CAN_HMI_F1_ACTIONS_BLOCKED;
    if (bench_mode) flags1 |= CAN_HMI_F1_BENCH;
    if (hal_safety_relay_is_closed()) flags1 |= CAN_HMI_F1_RELAY;
    /* Controller SD is intentionally disabled; keep F1_SD_READY false. */

    uint8_t flags2 = 0;
    if (s.flood_active) flags2 |= CAN_HMI_F2_FLOOD;
    if (s.eq_lock_active) flags2 |= CAN_HMI_F2_EQ_LOCK;
    if (s.volume_guard_active) flags2 |= CAN_HMI_F2_VOL_GUARD;
    if (s.volume_no_flow) flags2 |= CAN_HMI_F2_VOL_NO_FLOW;
    if (s.volume_overflow) flags2 |= CAN_HMI_F2_VOL_OVERFLOW;
    if (s.scale_ready) flags2 |= CAN_HMI_F2_SCALE_READY;
    if (s.scale_indicator_active) flags2 |= CAN_HMI_F2_SCALE_ACTIVE;
    if (zmpt_rms_fault()) flags2 |= CAN_HMI_F2_ZMPT_FAULT;

    uint8_t flags3 = 0;
    if (s.predictive_heat_active) flags3 |= CAN_HMI_F3_PRED_HEAT;
    if (s.energy_anomaly_active) flags3 |= CAN_HMI_F3_ENERGY_ANOM;
    /* DB ready not directly in state; leave 0 or set from caller context */

    can_hmi_tlm_a_t a = {
        .seq = seq, .stage = (uint8_t)s.stage, .mode_type = (uint8_t)s.type,
        .flags1 = flags1, .flags2 = flags2, .eq_phase = s.eq_lock_phase,
        .estop_mode = (uint8_t)estop_get_mode(), .flags3 = flags3,
        .reason_code = reason_code_from_state(&s),
    };
    can_hmi_tlm_b_t b = {
        .seq = seq,
        .cube_x100 = can_hmi_temp_to_x100(s.cube_temp),
        .column_x100 = can_hmi_temp_to_x100(s.column_temp),
        .deph_x100 = can_hmi_temp_to_x100(s.deph_temp),
    };
    can_hmi_tlm_c_t c = {
        .seq = seq,
        .jacket_x100 = can_hmi_temp_to_x100(s.jacket_temp),
        .tank_x100 = can_hmi_temp_to_x100(s.tank_temp),
        .ambient_x100 = can_hmi_temp_to_x100(s.ambient_temp),
        .level_pct = 0, /* level not exported in process_state_t; reserved */
    };
    can_hmi_tlm_d_t td = {
        .seq = seq,
        .pressure_x1000 = (int16_t)(s.pressure_bar * 1000.0f),
        .vacuum_x10 = (int16_t)(s.vacuum_mbar * 10.0f),
        .flow_x100 = (uint16_t)(s.flow_lpm < 0 ? 0 : s.flow_lpm * 100.0f),
        .power_x2 = can_hmi_pct_to_x2(s.power_percent),
    };
    can_hmi_tlm_e_t e = {
        .seq = seq,
        .heads_ml = (uint16_t)(s.heads_collected_ml < 0 ? 0 : s.heads_collected_ml),
        .body_ml = (uint16_t)(s.body_collected_ml < 0 ? 0 : s.body_collected_ml),
        .tails_ml = (uint16_t)(s.tails_collected_ml < 0 ? 0 : s.tails_collected_ml),
        .lsa_duty_x2 = can_hmi_pct_to_x2(hal_lsa_pwm_get_duty_pct()),
    };
    can_hmi_tlm_f_t f = {
        .seq = seq,
        .pt506_x1000 = (int16_t)(pt506_pressure_bar() * 1000.0f),
        .xgzp_x10 = (int16_t)(pressure_xgzp6897d_kpa() * 10.0f),
        .zmpt_v1 = (uint8_t)(zmpt_rms_phase_v(0) < 0 ? 0 : (zmpt_rms_phase_v(0) > 255 ? 255 : zmpt_rms_phase_v(0))),
        .zmpt_v2 = (uint8_t)(zmpt_rms_phase_v(1) < 0 ? 0 : (zmpt_rms_phase_v(1) > 255 ? 255 : zmpt_rms_phase_v(1))),
        .zmpt_v3 = (uint8_t)(zmpt_rms_phase_v(2) < 0 ? 0 : (zmpt_rms_phase_v(2) > 255 ? 255 : zmpt_rms_phase_v(2))),
        .zmpt_fault_phase = (uint8_t)(zmpt_rms_fault() ? 0x80u : 0u),
    };
    uint32_t elapsed32 = s.elapsed_sec;
    can_hmi_tlm_g_t g = {
        .seq = seq,
        .target_x100 = can_hmi_temp_to_x100(s.target_cube_t),
        .deph_x100 = can_hmi_temp_to_x100(s.deph_temp),
        .elapsed_lo = (uint16_t)(elapsed32 & 0xFFFFu),
        .elapsed_hi = (uint8_t)((elapsed32 >> 16) & 0xFFu),
    };

    uint8_t d[8];
    if (can_hmi_pack_tlm_a(d, &a)) can_send(CAN_HMI_ID_TLM_A, d);
    if (can_hmi_pack_tlm_b(d, &b)) can_send(CAN_HMI_ID_TLM_B, d);
    if (can_hmi_pack_tlm_c(d, &c)) can_send(CAN_HMI_ID_TLM_C, d);
    if (can_hmi_pack_tlm_d(d, &td)) can_send(CAN_HMI_ID_TLM_D, d);
    if (can_hmi_pack_tlm_e(d, &e)) can_send(CAN_HMI_ID_TLM_E, d);
    if (can_hmi_pack_tlm_f(d, &f)) can_send(CAN_HMI_ID_TLM_F, d);
    if (can_hmi_pack_tlm_g(d, &g)) can_send(CAN_HMI_ID_TLM_G, d);

    uint8_t meta[4] = {0};
    uint16_t heap_kb = (uint16_t)(esp_get_free_heap_size() / 1024u);
    can_hmi_put_u16(&meta[0], heap_kb);
    can_hmi_put_u16(&meta[2], (uint16_t)(elapsed32 & 0xFFFFu));
    send_ack(seq, CAN_HMI_ACK_SNAPSHOT, true, CAN_HMI_ERR_OK, meta);
}

static void handle_cmd(const can_hmi_cmd_t *cmd)
{
    bool ok = false;
    uint8_t err = CAN_HMI_ERR_UNKNOWN;

    switch (cmd->cmd) {
    case CAN_HMI_CMD_START: {
        bool have_mode = false;
        process_type_t type = find_mode_type(cmd->p0, &have_mode);
        if (!have_mode) type = state_get()->type;
        const process_mode_t *m = mode_from_type(type);
        if (!m) m = mode_from_type(PROCESS_DISTILL);
        char why[96] = {0};
        if (!m) { err = CAN_HMI_ERR_MODE; }
        else if (!process_control_hw_ready(type, why, sizeof(why))) { err = CAN_HMI_ERR_HW; }
        else if (!ctrl_safety_ok()) { err = CAN_HMI_ERR_SAFETY; }
        else if (estop_actions_blocked()) { err = estop_bench_mode() ? CAN_HMI_ERR_BENCH : CAN_HMI_ERR_ESTOP; }
        else {
            mode_set(m);
            process_state_t *st = state_get();
            st->type = type;
            st->is_running = true;
            st->is_paused = false;
            ok = true; err = CAN_HMI_ERR_OK;
        }
        break;
    }
    case CAN_HMI_CMD_STOP:
        ctrl_heater_force_off();
        ctrl_dose_stop();
        if (mode_get() && mode_get()->stop) mode_get()->stop();
        state_get()->is_running = false;
        state_get()->is_paused = false;
        ok = true; err = CAN_HMI_ERR_OK;
        break;
    case CAN_HMI_CMD_PAUSE:
        if (!state_get()->is_running) { err = CAN_HMI_ERR_NOT_RUNNING; }
        else { state_get()->is_paused = true; ok = true; err = CAN_HMI_ERR_OK; }
        break;
    case CAN_HMI_CMD_POWER:
        if (estop_actions_blocked()) { err = estop_bench_mode() ? CAN_HMI_ERR_BENCH : CAN_HMI_ERR_ESTOP; }
        else if (cmd->p0 & 0x01u) { ctrl_heater_set_auto(); ok = true; err = CAN_HMI_ERR_OK; }
        else {
            uint16_t x10 = can_hmi_u16(&cmd->p1);
            float pct = (float)x10 / 10.0f;
            if (pct < 0.0f) pct = 0.0f;
            if (pct > 100.0f) pct = 100.0f;
            esp_err_t e = safety_gate_submit("hmi_power", pct);
            ok = (e == ESP_OK); err = ok ? CAN_HMI_ERR_OK : CAN_HMI_ERR_GATE;
        }
        break;
    case CAN_HMI_CMD_SAFETY_RESET:
        if (estop_is_pressed()) { err = CAN_HMI_ERR_ESTOP; }
        else if (estop_bench_mode()) { err = CAN_HMI_ERR_BENCH; }
        else if (!ctrl_safety_ok()) {
            esp_err_t e = ctrl_safety_reset();
            ok = (e == ESP_OK); err = ok ? CAN_HMI_ERR_OK : CAN_HMI_ERR_SAFETY;
        } else { ok = true; err = CAN_HMI_ERR_OK; }
        break;
    case CAN_HMI_CMD_PING:
        ok = true; err = CAN_HMI_ERR_OK;
        send_ack(cmd->seq, CAN_HMI_ACK_PONG, ok, err, NULL);
        return;
    case CAN_HMI_CMD_GET_CONFIG: {
        ok = true; err = CAN_HMI_ERR_OK;
        uint8_t data4[4] = { (uint8_t)HMI_PROTO_VERSION_MAJOR, (uint8_t)HMI_PROTO_VERSION_MINOR,
                             (uint8_t)HMI_PROTO_VERSION_PATCH, (uint8_t)CAN_HMI_TLM_FRAMES };
        send_ack(cmd->seq, CAN_HMI_ACK_CONFIG, ok, err, data4);
        return;
    }
    default:
        err = CAN_HMI_ERR_UNKNOWN;
        break;
    }

    send_ack(cmd->seq, CAN_HMI_ACK_ACK, ok, err, NULL);
}

static size_t s_log_cursor;
static size_t s_log_pending;

static uint8_t s_recipe_buf[CAN_HMI_RECIPE_MAX_BYTES];
static size_t s_recipe_len;
static uint16_t s_recipe_next_idx;
static bool s_recipe_active;

static void send_log_response(uint8_t seq, uint8_t acked)
{
    if (acked > 0 && acked <= s_log_pending) {
        s_log_cursor += acked;
        s_log_pending -= acked;
    } else if (acked > 0) {
        s_log_cursor += s_log_pending;
        s_log_pending = 0;
    }

    size_t peek = s_log_cursor;
    event_t ev[2];
    int n = events_read_after(&peek, ev, 2);
    s_log_pending = (size_t)n;

    uint8_t sent_events = 0;
    for (int i = 0; i < n; i++) {
        can_hmi_log_t logf = {0};
        logf.seq = seq;
        logf.rec = (uint8_t)i;
        logf.type = (uint8_t)ev[i].type;
        logf.flags = CAN_HMI_LOG_F_START;
        can_hmi_put_u32(logf.data, ev[i].timestamp_ms);
        uint8_t d[8];
        if (can_hmi_pack_log(d, &logf)) can_send(CAN_HMI_ID_LOG, d);

        const char *msg = ev[i].msg;
        size_t len = strnlen(msg, sizeof(ev[i].msg));
        size_t off = 0;
        if (len == 0) {
            memset(&logf, 0, sizeof(logf));
            logf.seq = seq; logf.rec = (uint8_t)i; logf.type = (uint8_t)ev[i].type;
            logf.flags = CAN_HMI_LOG_F_END;
            if (can_hmi_pack_log(d, &logf)) can_send(CAN_HMI_ID_LOG, d);
        } else {
            while (off < len) {
                memset(&logf, 0, sizeof(logf));
                logf.seq = seq; logf.rec = (uint8_t)i; logf.type = (uint8_t)ev[i].type;
                size_t chunk = len - off;
                if (chunk > 4) chunk = 4;
                memcpy(logf.data, msg + off, chunk);
                off += chunk;
                if (off >= len) logf.flags |= CAN_HMI_LOG_F_END;
                if (can_hmi_pack_log(d, &logf)) can_send(CAN_HMI_ID_LOG, d);
            }
        }
        sent_events++;
    }
    uint8_t ack_data[4] = { sent_events, (uint8_t)(n >= 2 ? 1 : 0), 0, 0 };
    send_ack(seq, CAN_HMI_ACK_LOG_END, true, CAN_HMI_ERR_OK, ack_data);
}

static void handle_recipe_chunk(const uint8_t d[8], uint8_t dlc)
{
    if (dlc < 4) return;
    uint8_t seq = d[0];
    uint8_t idx = d[1];
    uint8_t flags = d[2];
    const uint8_t *payload = &d[3];
    uint8_t payload_len = (dlc > 8 ? 8 : dlc) - 3;

    if (flags & CAN_HMI_RECIPE_F_ABORT) {
        s_recipe_active = false;
        s_recipe_len = 0;
        s_recipe_next_idx = 0;
        uint8_t ackd[4] = { idx, CAN_HMI_ERR_OK, 0, 0 };
        send_ack(seq, CAN_HMI_ACK_RECIPE, true, CAN_HMI_ERR_OK, ackd);
        return;
    }

    if (flags & CAN_HMI_RECIPE_F_BEGIN) {
        s_recipe_active = true;
        s_recipe_len = 0;
        s_recipe_next_idx = 0;
    }

    if (!s_recipe_active) {
        uint8_t ackd[4] = { idx, CAN_HMI_ERR_GATE, 0, 0 };
        send_ack(seq, CAN_HMI_ACK_RECIPE, false, CAN_HMI_ERR_GATE, ackd);
        return;
    }

    if (idx != (uint8_t)(s_recipe_next_idx & 0xFFu)) {
        uint8_t ackd[4] = { idx, CAN_HMI_ERR_BAD_ARGS, (uint8_t)(s_recipe_next_idx & 0xFFu), 0 };
        send_ack(seq, CAN_HMI_ACK_RECIPE, false, CAN_HMI_ERR_BAD_ARGS, ackd);
        return;
    }

    if (s_recipe_len + payload_len > sizeof(s_recipe_buf)) {
        s_recipe_active = false;
        uint8_t ackd[4] = { idx, CAN_HMI_ERR_BAD_ARGS, 0, 0 };
        send_ack(seq, CAN_HMI_ACK_RECIPE, false, CAN_HMI_ERR_BAD_ARGS, ackd);
        return;
    }

    memcpy(&s_recipe_buf[s_recipe_len], payload, payload_len);
    s_recipe_len += payload_len;
    s_recipe_next_idx++;

    if (flags & CAN_HMI_RECIPE_F_END) {
        if (s_recipe_len < sizeof(s_recipe_buf)) s_recipe_buf[s_recipe_len] = '\0';
        s_recipe_active = false;
        ESP_LOGI(TAG, "Recipe received: %u bytes", (unsigned)s_recipe_len);
    }

    uint8_t ackd[4] = { idx, CAN_HMI_ERR_OK, (uint8_t)(s_recipe_next_idx & 0xFFu), (flags & CAN_HMI_RECIPE_F_END) ? 1 : 0 };
    send_ack(seq, CAN_HMI_ACK_RECIPE, true, CAN_HMI_ERR_OK, ackd);
}

static void can_hmi_recover_if_bus_off(void)
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

static void can_hmi_task(void *arg)
{
    (void)arg;
    twai_message_t msg;
    while (1) {
        esp_err_t r = twai_receive(&msg, pdMS_TO_TICKS(CAN_HMI_RX_TIMEOUT_MS));
        if (r != ESP_OK) {
            if (r != ESP_ERR_TIMEOUT) can_hmi_recover_if_bus_off();
            continue;
        }
        if (msg.extd || msg.rtr || msg.data_length_code < 2) continue;

        if (msg.identifier == CAN_HMI_ID_POLL) {
            uint8_t seq = msg.data[0];
            send_telemetry_snapshot(seq);
        } else if (msg.identifier == CAN_HMI_ID_LOG_POLL) {
            uint8_t seq = msg.data[0];
            uint8_t acked = (msg.data_length_code >= 2) ? msg.data[1] : 0;
            send_log_response(seq, acked);
        } else if (msg.identifier == CAN_HMI_ID_RECIPE) {
            uint8_t d[8] = {0};
            uint8_t dlc = msg.data_length_code < 8 ? msg.data_length_code : 8;
            memcpy(d, msg.data, dlc);
            handle_recipe_chunk(d, dlc);
        } else if (msg.identifier == CAN_HMI_ID_CMD) {
            can_hmi_cmd_t cmd;
            uint8_t d[8] = {0};
            memcpy(d, msg.data, msg.data_length_code < 8 ? msg.data_length_code : 8);
            if (can_hmi_unpack_cmd(d, &cmd)) handle_cmd(&cmd);
        }
    }
}

esp_err_t can_hmi_init(void)
{
#if !CONFIG_HMI_CAN_ENABLE
    ESP_LOGI(TAG, "CAN HMI disabled by CONFIG_HMI_CAN_ENABLE=n");
    return ESP_OK;
#else
    if (CONFIG_HMI_CAN_TX_GPIO < 0 || CONFIG_HMI_CAN_RX_GPIO < 0) {
        ESP_LOGW(TAG, "CAN HMI not started: set TX/RX GPIO in menuconfig");
        return ESP_ERR_INVALID_ARG;
    }

    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
        (gpio_num_t)CONFIG_HMI_CAN_TX_GPIO, (gpio_num_t)CONFIG_HMI_CAN_RX_GPIO, TWAI_MODE_NORMAL);
    g_config.rx_queue_len = 16;
    g_config.tx_queue_len = 16;

    twai_timing_config_t t_config;
#if CONFIG_HMI_CAN_BITRATE_KBPS == 1000
    t_config = (twai_timing_config_t)TWAI_TIMING_CONFIG_1MBITS();
#elif CONFIG_HMI_CAN_BITRATE_KBPS == 250
    t_config = (twai_timing_config_t)TWAI_TIMING_CONFIG_250KBITS();
#else
    t_config = (twai_timing_config_t)TWAI_TIMING_CONFIG_500KBITS();
#endif

    twai_filter_config_t f_config = {
        .acceptance_code = (CAN_HMI_ID_CMD << 21),
        .acceptance_mask = ~(uint32_t)((0x7FCu) << 21), /* accept 0x100-0x103 */
        .single_filter = true,
    };

    esp_err_t err = twai_driver_install(&g_config, &t_config, &f_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "twai_driver_install failed: %s", esp_err_to_name(err));
        return err;
    }
    err = twai_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "twai_start failed: %s", esp_err_to_name(err));
        twai_driver_uninstall();
        return err;
    }

    BaseType_t rc = xTaskCreatePinnedToCore(can_hmi_task, "can_hmi", CAN_HMI_STACK_BYTES,
                                            NULL, CAN_HMI_TASK_PRIO, NULL, 1);
    if (rc != pdPASS) {
        twai_stop();
        twai_driver_uninstall();
        return ESP_ERR_NO_MEM;
    }

    s_ready = true;
    ESP_LOGI(TAG, "CAN HMI ready: TX=%d RX=%d %d kbps", CONFIG_HMI_CAN_TX_GPIO,
             CONFIG_HMI_CAN_RX_GPIO, CONFIG_HMI_CAN_BITRATE_KBPS);
    return ESP_OK;
#endif
}

bool can_hmi_ready(void) { return s_ready; }
