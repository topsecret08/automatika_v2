#include "hmi_protocol.h"

#include <string.h>

uint16_t hmi_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int b = 0; b < 8; b++) {
            crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u)
                                  : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

bool hmi_frame_build(uint8_t *out, size_t *out_len,
                     uint16_t seq, uint8_t node, uint8_t type,
                     const uint8_t *payload, uint16_t len)
{
    if (!out || !out_len || len > HMI_MAX_PAYLOAD) return false;

    size_t pos = 0;
    out[pos++] = HMI_MAGIC0;
    out[pos++] = HMI_MAGIC1;
    out[pos++] = (uint8_t)(len & 0xFFu);
    out[pos++] = (uint8_t)(len >> 8);
    out[pos++] = (uint8_t)(seq & 0xFFu);
    out[pos++] = (uint8_t)(seq >> 8);
    out[pos++] = node;
    out[pos++] = type;

    if (len && payload) {
        memcpy(&out[pos], payload, len);
        pos += len;
    }

    /* CRC: LEN, SEQ, NODE, TYPE и PAYLOAD — с индекса 2. */
    uint16_t crc = hmi_crc16(&out[2], pos - 2);
    out[pos++] = (uint8_t)(crc & 0xFFu);
    out[pos++] = (uint8_t)(crc >> 8);

    *out_len = pos;
    return true;
}

bool hmi_frame_parse(const uint8_t *buf, size_t len, hmi_frame_t *frame)
{
    if (!buf || !frame || len < HMI_FRAME_OVERHEAD) return false;
    if (buf[0] != HMI_MAGIC0 || buf[1] != HMI_MAGIC1) return false;

    uint16_t payload_len = (uint16_t)buf[2] | ((uint16_t)buf[3] << 8);
    if (payload_len > HMI_MAX_PAYLOAD) return false;
    if (len != (size_t)(HMI_FRAME_OVERHEAD + payload_len)) return false;

    size_t crc_span = 6u + payload_len; /* LEN..PAYLOAD */
    uint16_t crc_calc = hmi_crc16(&buf[2], crc_span);
    uint16_t crc_rx = (uint16_t)buf[8u + payload_len]
                    | ((uint16_t)buf[9u + payload_len] << 8);
    if (crc_calc != crc_rx) return false;

    frame->seq  = (uint16_t)buf[4] | ((uint16_t)buf[5] << 8);
    frame->node = buf[6];
    frame->type = buf[7];
    frame->len  = payload_len;

    if (payload_len) {
        memcpy(frame->payload, &buf[8], payload_len);
    }
    frame->payload[payload_len] = '\0';
    return true;
}
