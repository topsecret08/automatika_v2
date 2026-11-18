#include "unity.h"
#include "stepper_proto.h"

#include <math.h>
#include <string.h>

TEST_CASE("stepper crc8 reference vector", "[stepper]") {
    const uint8_t data[] = "123456789";
    TEST_ASSERT_EQUAL_UINT8(0xF4, stepper_crc8(data, 9));
}

TEST_CASE("stepper encodes frame with crc", "[stepper]") {
    stepper_request_t req = {
        .seq = 7,
        .cmd = STEPPER_CMD_MOVE_REL,
        .axis = STEPPER_AXIS_DOSER,
        .len = 8,
    };
    uint8_t frame[16];
    size_t n = stepper_encode(&req, frame, sizeof(frame));
    TEST_ASSERT_EQUAL_UINT32(16, n);
    TEST_ASSERT_EQUAL_HEX8(0xAA, frame[0]);
    TEST_ASSERT_EQUAL_HEX8(0x55, frame[1]);
    TEST_ASSERT_EQUAL_HEX8(STEPPER_PROTO_VER, frame[2]);
    TEST_ASSERT_EQUAL_UINT8(7, frame[3]);
    TEST_ASSERT_EQUAL_UINT8(STEPPER_CMD_MOVE_REL, frame[4]);
    TEST_ASSERT_EQUAL_UINT8(STEPPER_AXIS_DOSER, frame[5]);
    TEST_ASSERT_EQUAL_UINT8(8, frame[6]);
    TEST_ASSERT_EQUAL_UINT8(stepper_crc8(&frame[2], 13), frame[15]);

    TEST_ASSERT_EQUAL_UINT32(0, stepper_encode(&req, frame, 8));
}

static size_t make_status(uint8_t *out, int32_t pos, uint8_t state,
                          uint8_t fault, uint8_t axis,
                          uint16_t speed, uint16_t vin) {
    out[0] = STEPPER_STATUS_MAGIC;
    out[1] = STEPPER_PROTO_VER;
    out[2] = 3;
    out[3] = axis;
    out[4] = state;
    out[5] = fault;
    out[6] = 0;
    out[7] = (uint8_t)(pos & 0xFF);
    out[8] = (uint8_t)((pos >> 8) & 0xFF);
    out[9] = (uint8_t)((pos >> 16) & 0xFF);
    out[10] = (uint8_t)((pos >> 24) & 0xFF);
    out[11] = (uint8_t)(speed & 0xFF);
    out[12] = (uint8_t)(speed >> 8);
    out[13] = (uint8_t)(vin & 0xFF);
    out[14] = (uint8_t)(vin >> 8);
    out[15] = stepper_crc8(&out[1], 14);
    return STEPPER_STATUS_LEN;
}

TEST_CASE("stepper decodes status", "[stepper]") {
    uint8_t frame[STEPPER_STATUS_LEN];
    make_status(frame, -12345, STEPPER_STATE_CRUISE, STEPPER_FAULT_NONE,
                STEPPER_AXIS_STIRRER, 800, 12000);

    stepper_status_t st;
    TEST_ASSERT_TRUE(stepper_decode_status(frame, sizeof(frame), &st));
    TEST_ASSERT_TRUE(st.valid);
    TEST_ASSERT_EQUAL_INT32(-12345, st.position);
    TEST_ASSERT_EQUAL_UINT16(800, st.speed_sps);
    TEST_ASSERT_EQUAL_UINT16(12000, st.vin_mv);
    TEST_ASSERT_EQUAL_UINT8(STEPPER_STATE_CRUISE, st.state);
}

TEST_CASE("stepper rejects corrupt status", "[stepper]") {
    uint8_t frame[STEPPER_STATUS_LEN];
    make_status(frame, 10, STEPPER_STATE_IDLE, STEPPER_FAULT_NONE,
                STEPPER_AXIS_DOSER, 0, 0);

    stepper_status_t st;
    frame[15] ^= 0xFF;
    TEST_ASSERT_FALSE(stepper_decode_status(frame, sizeof(frame), &st));
    TEST_ASSERT_FALSE(st.valid);

    make_status(frame, 10, STEPPER_STATE_IDLE, STEPPER_FAULT_NONE,
                STEPPER_AXIS_DOSER, 0, 0);
    frame[0] = 0;
    TEST_ASSERT_FALSE(stepper_decode_status(frame, sizeof(frame), &st));
    TEST_ASSERT_FALSE(stepper_decode_status(frame, 8, &st));
}

TEST_CASE("stepper move payload", "[stepper]") {
    stepper_request_t req;
    size_t len = stepper_build_move(&req, 9, STEPPER_AXIS_STIRRER,
                                    -1000, 500, 4000);
    TEST_ASSERT_EQUAL_UINT8(8, len);
    TEST_ASSERT_EQUAL_UINT8(9, req.seq);
    TEST_ASSERT_EQUAL_UINT8(STEPPER_AXIS_STIRRER, req.axis);

    uint8_t frame[32];
    size_t n = stepper_encode(&req, frame, sizeof(frame));
    TEST_ASSERT_EQUAL_UINT32(16, n);

    int32_t steps = (int32_t)((uint32_t)frame[7] |
                              ((uint32_t)frame[8] << 8) |
                              ((uint32_t)frame[9] << 16) |
                              ((uint32_t)frame[10] << 24));
    TEST_ASSERT_EQUAL_INT32(-1000, steps);
    TEST_ASSERT_EQUAL_UINT16(500, (uint16_t)(frame[11] | (frame[12] << 8)));
    TEST_ASSERT_EQUAL_UINT16(4000, (uint16_t)(frame[13] | (frame[14] << 8)));
}

TEST_CASE("stepper continuous run payload", "[stepper]") {
    stepper_request_t req;
    stepper_build_run(&req, 1, STEPPER_AXIS_STIRRER, true, 900);
    TEST_ASSERT_EQUAL_UINT8(STEPPER_CMD_RUN_CONT, req.cmd);
    TEST_ASSERT_EQUAL_UINT8(3, req.len);
    TEST_ASSERT_EQUAL_UINT8(1, req.payload[0]);
    TEST_ASSERT_EQUAL_UINT16(900, (uint16_t)(req.payload[1] |
                                             (req.payload[2] << 8)));
}

TEST_CASE("stepper application math", "[stepper]") {
    TEST_ASSERT_EQUAL_UINT32(2000, stepper_ml_to_steps(2.0f, 1000.0f));
    TEST_ASSERT_EQUAL_UINT32(0, stepper_ml_to_steps(-1.0f, 1000.0f));
    TEST_ASSERT_EQUAL_UINT32(0, stepper_ml_to_steps(1.0f, 0.0f));

    TEST_ASSERT_EQUAL_UINT16(1000, stepper_rate_to_sps(60.0f, 1000.0f, 1600));
    TEST_ASSERT_EQUAL_UINT16(1600, stepper_rate_to_sps(500.0f, 1000.0f, 1600));
    TEST_ASSERT_EQUAL_UINT16(1, stepper_rate_to_sps(0.0f, 1000.0f, 1600));

    uint32_t ms = stepper_move_timeout_ms(1600, 1600, 8000, 1000);
    TEST_ASSERT_TRUE(ms >= 2000);
    TEST_ASSERT_EQUAL_UINT32(500, stepper_move_timeout_ms(0, 1600, 8000, 500));
}
