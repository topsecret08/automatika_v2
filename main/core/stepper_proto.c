#include "stepper_proto.h"

#include <math.h>
#include <string.h>

#define CRC8_POLY 0x07u

uint8_t stepper_crc8(const uint8_t *data, size_t len) {
    uint8_t crc = 0;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++)
            crc = (crc & 0x80u) ? (uint8_t)((crc << 1) ^ CRC8_POLY)
                                : (uint8_t)(crc << 1);
    }
    return crc;
}

static void put_u16(uint8_t *out, uint16_t value) {
    out[0] = (uint8_t)(value & 0xFFu);
    out[1] = (uint8_t)(value >> 8);
}

static void put_i32(uint8_t *out, int32_t value) {
    uint32_t raw = (uint32_t)value;
    out[0] = (uint8_t)(raw & 0xFFu);
    out[1] = (uint8_t)((raw >> 8) & 0xFFu);
    out[2] = (uint8_t)((raw >> 16) & 0xFFu);
    out[3] = (uint8_t)((raw >> 24) & 0xFFu);
}

static int32_t get_i32(const uint8_t *in) {
    return (int32_t)((uint32_t)in[0] |
                     ((uint32_t)in[1] << 8) |
                     ((uint32_t)in[2] << 16) |
                     ((uint32_t)in[3] << 24));
}

static uint16_t get_u16(const uint8_t *in) {
    return (uint16_t)(in[0] | ((uint16_t)in[1] << 8));
}

size_t stepper_encode(const stepper_request_t *req, uint8_t *out, size_t cap) {
    if (!req || !out) return 0;
    size_t payload = req->len;
    if (payload > STEPPER_MAX_PAYLOAD) return 0;
    size_t total = 8 + payload;
    if (cap < total) return 0;

    out[0] = STEPPER_CMD_MAGIC0;
    out[1] = STEPPER_CMD_MAGIC1;
    out[2] = STEPPER_PROTO_VER;
    out[3] = req->seq;
    out[4] = req->cmd;
    out[5] = req->axis;
    out[6] = req->len;
    if (payload) memcpy(&out[7], req->payload, payload);
    out[7 + payload] = stepper_crc8(&out[2], 5 + payload);
    return total;
}

bool stepper_decode_status(const uint8_t *data, size_t len,
                           stepper_status_t *out) {
    if (!data || !out || len < STEPPER_STATUS_LEN) return false;
    if (data[0] != STEPPER_STATUS_MAGIC) {
        out->valid = false;
        return false;
    }
    uint8_t crc = stepper_crc8(&data[1], STEPPER_STATUS_LEN - 2);
    if (crc != data[STEPPER_STATUS_LEN - 1]) {
        out->valid = false;
        return false;
    }
    if (data[1] != STEPPER_PROTO_VER) {
        out->valid = false;
        return false;
    }
    out->ver = data[1];
    out->seq = data[2];
    out->axis = data[3];
    out->state = data[4];
    out->fault = data[5];
    out->limit = data[6];
    out->position = get_i32(&data[7]);
    out->speed_sps = get_u16(&data[11]);
    out->vin_mv = get_u16(&data[13]);
    out->valid = true;
    return true;
}

size_t stepper_build_move(stepper_request_t *req, uint8_t seq,
                          stepper_axis_t axis, int32_t steps,
                          uint16_t speed_sps, uint16_t accel_sps2) {
    if (!req) return 0;
    memset(req, 0, sizeof(*req));
    req->seq = seq;
    req->cmd = STEPPER_CMD_MOVE_REL;
    req->axis = (uint8_t)axis;
    req->len = 8;
    put_i32(&req->payload[0], steps);
    put_u16(&req->payload[4], speed_sps);
    put_u16(&req->payload[6], accel_sps2);
    return req->len;
}

size_t stepper_build_run(stepper_request_t *req, uint8_t seq,
                         stepper_axis_t axis, bool reverse,
                         uint16_t speed_sps) {
    if (!req) return 0;
    memset(req, 0, sizeof(*req));
    req->seq = seq;
    req->cmd = STEPPER_CMD_RUN_CONT;
    req->axis = (uint8_t)axis;
    req->len = 3;
    req->payload[0] = reverse ? 1u : 0u;
    put_u16(&req->payload[1], speed_sps);
    return req->len;
}

uint32_t stepper_ml_to_steps(float volume_ml, float steps_per_ml) {
    if (!isfinite(volume_ml) || !isfinite(steps_per_ml) ||
        volume_ml <= 0.0f || steps_per_ml <= 0.0f)
        return 0;
    float steps = volume_ml * steps_per_ml;
    if (steps > 2000000.0f) return 2000000u;
    return (uint32_t)(steps + 0.5f);
}

uint16_t stepper_rate_to_sps(float rate_ml_min, float steps_per_ml,
                             uint16_t max_sps) {
    if (!isfinite(rate_ml_min) || !isfinite(steps_per_ml) ||
        rate_ml_min <= 0.0f || steps_per_ml <= 0.0f)
        return 1;
    float sps = (rate_ml_min / 60.0f) * steps_per_ml;
    if (sps < 1.0f) sps = 1.0f;
    if (max_sps > 0 && sps > (float)max_sps) sps = (float)max_sps;
    return (uint16_t)(sps + 0.5f);
}

uint32_t stepper_move_timeout_ms(uint32_t steps, uint16_t speed_sps,
                                 uint32_t accel_sps2, uint32_t margin_ms) {
    if (steps == 0) return margin_ms;
    uint32_t speed = speed_sps ? speed_sps : 1u;
    // Время разгона/торможения и полка; берём двойной запас, потому что
    // реальная скорость зависит от драйвера и нагрузки.
    uint64_t cruise_ms = ((uint64_t)steps * 1000ull) / speed;
    uint64_t accel_ms = (accel_sps2 > 0)
                            ? ((uint64_t)speed * 1000ull) / accel_sps2
                            : 0;
    uint64_t total = cruise_ms + 2ull * accel_ms + margin_ms;
    if (total > 3600000ull) total = 3600000ull;
    return (uint32_t)total;
}
