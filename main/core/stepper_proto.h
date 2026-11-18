#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Протокол I2C-драйвера шагового двигателя Automatika.
//
// Кадр запроса:  AA 55 VER SEQ CMD AXIS LEN PAYLOAD... CRC8
// Кадр ответа:   5A VER SEQ AXIS STATE FAULT LIMIT POS[4] SPEED[2] VIN[2] CRC8
//
// Все многобайтные поля — little-endian. CRC8: полином 0x07, init 0,
// считается по VER..PAYLOAD (без магии).

#define STEPPER_CMD_MAGIC0   0xAAu
#define STEPPER_CMD_MAGIC1   0x55u
#define STEPPER_STATUS_MAGIC 0x5Au
#define STEPPER_PROTO_VER    1u

#define STEPPER_MAX_PAYLOAD  16u
#define STEPPER_STATUS_LEN   16u

typedef enum {
    STEPPER_CMD_PING = 1,
    STEPPER_CMD_ENABLE = 2,
    STEPPER_CMD_MOVE_REL = 3,
    STEPPER_CMD_MOVE_ABS = 4,
    STEPPER_CMD_STOP = 5,
    STEPPER_CMD_HOME = 6,
    STEPPER_CMD_SET_POS = 7,
    STEPPER_CMD_HEARTBEAT = 8,
    STEPPER_CMD_STATUS = 9,
    STEPPER_CMD_SET_LIMITS = 10,
    STEPPER_CMD_RUN_CONT = 11,
} stepper_cmd_id_t;

typedef enum {
    STEPPER_AXIS_DOSER = 0,
    STEPPER_AXIS_STIRRER = 1,
    STEPPER_AXIS_COUNT
} stepper_axis_t;

typedef enum {
    STEPPER_STATE_IDLE = 0,
    STEPPER_STATE_ACCEL,
    STEPPER_STATE_CRUISE,
    STEPPER_STATE_DECEL,
    STEPPER_STATE_HOMING,
    STEPPER_STATE_FAULT
} stepper_state_t;

typedef enum {
    STEPPER_FAULT_NONE = 0,
    STEPPER_FAULT_OVERCURRENT,
    STEPPER_FAULT_STALL,
    STEPPER_FAULT_LIMIT,
    STEPPER_FAULT_OVERTEMP,
    STEPPER_FAULT_DRIVER,
    STEPPER_FAULT_PROTOCOL
} stepper_fault_t;

typedef struct {
    uint8_t seq;
    uint8_t cmd;
    uint8_t axis;
    uint8_t len;
    uint8_t payload[STEPPER_MAX_PAYLOAD];
} stepper_request_t;

typedef struct {
    uint8_t  ver;
    uint8_t  seq;
    uint8_t  axis;
    uint8_t  state;
    uint8_t  fault;
    uint8_t  limit;
    int32_t  position;
    uint16_t speed_sps;
    uint16_t vin_mv;
    bool     valid;
} stepper_status_t;

uint8_t stepper_crc8(const uint8_t *data, size_t len);

size_t stepper_encode(const stepper_request_t *req, uint8_t *out, size_t cap);

bool stepper_decode_status(const uint8_t *data, size_t len,
                           stepper_status_t *out);

// Помощники заполнения запросов.
size_t stepper_build_move(stepper_request_t *req, uint8_t seq,
                          stepper_axis_t axis, int32_t steps,
                          uint16_t speed_sps, uint16_t accel_sps2);

size_t stepper_build_run(stepper_request_t *req, uint8_t seq,
                         stepper_axis_t axis, bool reverse,
                         uint16_t speed_sps);

// Прикладные расчёты (чистые функции для тестов).
uint32_t stepper_ml_to_steps(float volume_ml, float steps_per_ml);

uint16_t stepper_rate_to_sps(float rate_ml_min, float steps_per_ml,
                             uint16_t max_sps);

uint32_t stepper_move_timeout_ms(uint32_t steps, uint16_t speed_sps,
                                 uint32_t accel_sps2, uint32_t margin_ms);
