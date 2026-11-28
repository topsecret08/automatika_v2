#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "generated/protocol_version.h"

/*
 * Compact binary CAN protocol between Automatika controller and HMI panel.
 * Transport: classic CAN 2.0A (TWAI), standard 11-bit IDs, 8-byte DLC.
 * Default bitrate: 500 kbit/s.
 *
 * HMI is the link master (polls/commands). Controller is slave (responds).
 * Controller never initiates bus traffic except responses to HMI requests.
 *
 * All multi-byte fields are little-endian.
 * Temperatures: int16, 0.01 degC. Pressure: int16, 0.001 bar.
 * Vacuum: int16, 0.1 mbar. Flow: uint16, 0.01 LPM. Power/percent: uint16, 0.1%.
 * Volume/weight: int32, 0.1 unit (ml/g/ml_min as noted).
 */

#define CAN_HMI_BITRATE_DEFAULT     500000u

/* Standard 11-bit CAN IDs */
#define CAN_HMI_ID_CMD              0x100u  /* HMI -> CTRL: command */
#define CAN_HMI_ID_POLL             0x101u  /* HMI -> CTRL: poll/request */
#define CAN_HMI_ID_LOG_POLL         0x102u  /* HMI -> CTRL: poll event/log records */
#define CAN_HMI_ID_RECIPE           0x103u  /* HMI -> CTRL: recipe/params chunk */
#define CAN_HMI_ID_TLM_A            0x180u  /* CTRL -> HMI: state/flags */
#define CAN_HMI_ID_TLM_B            0x181u  /* CTRL -> HMI: temps */
#define CAN_HMI_ID_TLM_C            0x182u  /* CTRL -> HMI: pressure/flow/power/volumes */
#define CAN_HMI_ID_TLM_D            0x183u  /* CTRL -> HMI: sensors/scale */
#define CAN_HMI_ID_TLM_E            0x184u  /* CTRL -> HMI: guards/diag */
#define CAN_HMI_ID_TLM_F            0x185u  /* CTRL -> HMI: extra/reserved sensors */
#define CAN_HMI_ID_TLM_G            0x186u  /* CTRL -> HMI: targets/extra temps */
#define CAN_HMI_ID_LOG              0x187u  /* CTRL -> HMI: event/log chunk */
#define CAN_HMI_ID_ACK              0x18Fu  /* CTRL -> HMI: ack/pong/config/meta */

#define CAN_HMI_TLM_FRAMES          7u      /* A..G */
#define CAN_HMI_TLM_LAST_ID         CAN_HMI_ID_TLM_G

/* CMD codes (CMD frame byte 1) */
#define CAN_HMI_CMD_START           0x01u
#define CAN_HMI_CMD_STOP            0x02u
#define CAN_HMI_CMD_PAUSE           0x03u
#define CAN_HMI_CMD_POWER           0x04u
#define CAN_HMI_CMD_SAFETY_RESET    0x05u
#define CAN_HMI_CMD_PING            0x06u
#define CAN_HMI_CMD_GET_CONFIG      0x07u

/* ACK kinds (ACK frame byte 1) */
#define CAN_HMI_ACK_ACK             0x01u
#define CAN_HMI_ACK_PONG            0x02u
#define CAN_HMI_ACK_SNAPSHOT        0x03u  /* end of telemetry snapshot */
#define CAN_HMI_ACK_CONFIG          0x04u
#define CAN_HMI_ACK_LOG_END         0x05u  /* end of log poll response; data[0]=events, data[1]=more */
#define CAN_HMI_ACK_RECIPE          0x06u  /* recipe chunk ack; data[0]=idx, data[1]=status */

/* ACK error codes (ACK frame byte 3) */
#define CAN_HMI_ERR_OK              0x00u
#define CAN_HMI_ERR_UNKNOWN         0x01u
#define CAN_HMI_ERR_JSON            0x02u  /* legacy name kept as "bad payload" */
#define CAN_HMI_ERR_MODE            0x03u
#define CAN_HMI_ERR_SAFETY          0x04u
#define CAN_HMI_ERR_ESTOP           0x05u
#define CAN_HMI_ERR_BENCH           0x06u
#define CAN_HMI_ERR_NOT_RUNNING     0x07u
#define CAN_HMI_ERR_VALUE           0x08u
#define CAN_HMI_ERR_GATE            0x09u
#define CAN_HMI_ERR_HW              0x0Au
#define CAN_HMI_ERR_BUSY            0x0Bu

/* TLM_A flags1 byte 3 */
#define CAN_HMI_F1_RUN              0x01u
#define CAN_HMI_F1_PAUSE            0x02u
#define CAN_HMI_F1_SAFETY           0x04u
#define CAN_HMI_F1_ESTOP            0x08u
#define CAN_HMI_F1_ACTIONS_BLOCKED  0x10u
#define CAN_HMI_F1_BENCH            0x20u
#define CAN_HMI_F1_RELAY            0x40u
#define CAN_HMI_F1_SD_READY         0x80u

/* TLM_A flags2 byte 4 */
#define CAN_HMI_F2_FLOOD            0x01u
#define CAN_HMI_F2_EQ_LOCK          0x02u
#define CAN_HMI_F2_VOL_GUARD        0x04u
#define CAN_HMI_F2_VOL_NO_FLOW      0x08u
#define CAN_HMI_F2_VOL_OVERFLOW     0x10u
#define CAN_HMI_F2_SCALE_READY      0x20u
#define CAN_HMI_F2_SCALE_ACTIVE     0x40u
#define CAN_HMI_F2_ZMPT_FAULT       0x80u

/* TLM_A byte 6 high nibble flags3 */
#define CAN_HMI_F3_PRED_HEAT        0x10u
#define CAN_HMI_F3_ENERGY_ANOM      0x20u
#define CAN_HMI_F3_DB_READY         0x40u

/* Reason codes for TLM_E byte 7 (0 = none) */
#define CAN_HMI_REASON_NONE         0x00u
#define CAN_HMI_REASON_ESTOP        0x01u
#define CAN_HMI_REASON_BENCH        0x02u
#define CAN_HMI_REASON_SAFETY       0x03u
#define CAN_HMI_REASON_FLOOD        0x04u
#define CAN_HMI_REASON_EQ_LOCK      0x05u
#define CAN_HMI_REASON_VOL_GUARD    0x06u
#define CAN_HMI_REASON_VOL_NO_FLOW  0x07u
#define CAN_HMI_REASON_VOL_OVERFLOW 0x08u
#define CAN_HMI_REASON_ZMPT         0x09u
#define CAN_HMI_REASON_HW           0x0Au

typedef struct {
    uint8_t seq;
    uint8_t stage;
    uint8_t mode_type;
    uint8_t flags1;
    uint8_t flags2;
    uint8_t eq_phase;
    uint8_t estop_mode; /* low 4 bits */
    uint8_t flags3;     /* high nibble bits used, see CAN_HMI_F3_* */
    uint8_t reason_code;
} can_hmi_tlm_a_t;

typedef struct {
    uint8_t seq;
    int16_t cube_x100;
    int16_t column_x100;
    int16_t deph_x100;
    int8_t  reserved;
} can_hmi_tlm_b_t;

typedef struct {
    uint8_t seq;
    int16_t jacket_x100;
    int16_t tank_x100;
    int16_t ambient_x100;
    uint8_t level_pct;
} can_hmi_tlm_c_t;

typedef struct {
    uint8_t seq;
    int16_t pressure_x1000;   /* bar */
    int16_t vacuum_x10;       /* mbar */
    uint16_t flow_x100;       /* LPM */
    uint8_t power_x2;         /* 0..200 => 0..100% */
} can_hmi_tlm_d_t;

typedef struct {
    uint8_t seq;
    uint16_t heads_ml;
    uint16_t body_ml;
    uint16_t tails_ml;
    uint8_t  lsa_duty_x2;     /* 0..200 => 0..100% */
} can_hmi_tlm_e_t;

typedef struct {
    uint8_t seq;
    int16_t  pt506_x1000;     /* bar */
    int16_t  xgzp_x10;        /* kPa */
    uint8_t  zmpt_v1;         /* V, 0-255 */
    uint8_t  zmpt_v2;
    uint8_t  zmpt_v3;
    uint8_t  zmpt_fault_phase;/* bit0..2 phases, bit7 fault */
} can_hmi_tlm_f_t;

typedef struct {
    uint8_t seq;
    int16_t target_x100;      /* target cube temp */
    int16_t deph_x100;        /* dephlegmator temp */
    uint16_t elapsed_lo;      /* elapsed seconds low 16 */
    uint8_t  elapsed_hi;      /* elapsed seconds high 8 */
    uint8_t  reserved;
} can_hmi_tlm_g_t;

typedef struct {
    uint8_t seq;
    uint8_t kind;
    uint8_t ok;
    uint8_t err_code;
    uint8_t data[4];          /* kind-specific */
} can_hmi_ack_t;

typedef struct {
    uint8_t seq;
    uint8_t cmd;
    uint8_t p0, p1, p2, p3, p4, p5;
} can_hmi_cmd_t;

#define CAN_HMI_LOG_F_START 0x01u
#define CAN_HMI_LOG_F_END   0x02u

#define CAN_HMI_RECIPE_F_BEGIN 0x01u
#define CAN_HMI_RECIPE_F_END   0x02u
#define CAN_HMI_RECIPE_F_ABORT 0x04u
#define CAN_HMI_RECIPE_MAX_BYTES 2048u

typedef struct {
    uint8_t seq;    /* poll seq */
    uint8_t rec;    /* record index within response */
    uint8_t type;   /* event_type_t */
    uint8_t flags;  /* CAN_HMI_LOG_F_* */
    uint8_t data[4];/* START: ts_ms LE32; else: text chunk */
} can_hmi_log_t;

static inline uint16_t can_hmi_u16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
static inline int16_t can_hmi_i16(const uint8_t *p) {
    return (int16_t)can_hmi_u16(p);
}
static inline void can_hmi_put_u16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
}
static inline void can_hmi_put_i16(uint8_t *p, int16_t v) {
    can_hmi_put_u16(p, (uint16_t)v);
}
static inline uint32_t can_hmi_u32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static inline void can_hmi_put_u32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

static inline int16_t can_hmi_temp_to_x100(float t) {
    if (t < -1000.0f) t = -1000.0f;
    if (t > 1000.0f) t = 1000.0f;
    return (int16_t)(t * 100.0f);
}
static inline float can_hmi_x100_to_temp(int16_t v) { return (float)v / 100.0f; }

static inline uint8_t can_hmi_pct_to_x2(float pct) {
    if (pct < 0.0f) pct = 0.0f;
    if (pct > 100.0f) pct = 100.0f;
    return (uint8_t)(pct * 2.0f + 0.5f);
}
static inline float can_hmi_x2_to_pct(uint8_t v) { return (float)v * 0.5f; }

bool can_hmi_pack_tlm_a(uint8_t d[8], const can_hmi_tlm_a_t *s);
bool can_hmi_unpack_tlm_a(const uint8_t d[8], can_hmi_tlm_a_t *s);
bool can_hmi_pack_tlm_b(uint8_t d[8], const can_hmi_tlm_b_t *s);
bool can_hmi_unpack_tlm_b(const uint8_t d[8], can_hmi_tlm_b_t *s);
bool can_hmi_pack_tlm_c(uint8_t d[8], const can_hmi_tlm_c_t *s);
bool can_hmi_unpack_tlm_c(const uint8_t d[8], can_hmi_tlm_c_t *s);
bool can_hmi_pack_tlm_d(uint8_t d[8], const can_hmi_tlm_d_t *s);
bool can_hmi_unpack_tlm_d(const uint8_t d[8], can_hmi_tlm_d_t *s);
bool can_hmi_pack_tlm_e(uint8_t d[8], const can_hmi_tlm_e_t *s);
bool can_hmi_unpack_tlm_e(const uint8_t d[8], can_hmi_tlm_e_t *s);
bool can_hmi_pack_tlm_f(uint8_t d[8], const can_hmi_tlm_f_t *s);
bool can_hmi_unpack_tlm_f(const uint8_t d[8], can_hmi_tlm_f_t *s);
bool can_hmi_pack_ack(uint8_t d[8], const can_hmi_ack_t *s);
bool can_hmi_unpack_ack(const uint8_t d[8], can_hmi_ack_t *s);
bool can_hmi_pack_tlm_g(uint8_t d[8], const can_hmi_tlm_g_t *s);
bool can_hmi_unpack_tlm_g(const uint8_t d[8], can_hmi_tlm_g_t *s);
bool can_hmi_pack_cmd(uint8_t d[8], const can_hmi_cmd_t *s);
bool can_hmi_unpack_cmd(const uint8_t d[8], can_hmi_cmd_t *s);
bool can_hmi_pack_log(uint8_t d[8], const can_hmi_log_t *s);
bool can_hmi_unpack_log(const uint8_t d[8], can_hmi_log_t *s);
