#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "generated/protocol_version.h"

/* -------------------------------------------------------------------------
 * Общий протокол RS485 между контроллером Automatika и панелью HMI.
 * Единственный источник истины для обеих прошивок. Не копировать в проекты.
 *
 * Кадр:
 *   AA 55 | LEN_LO LEN_HI | SEQ_LO SEQ_HI | NODE | TYPE | PAYLOAD[LEN] | CRC_LO CRC_HI
 * CRC-16/CCITT-FALSE: poly 0x1021, init 0xFFFF, no reflect, no xorout.
 * CRC считается по байтам от LEN_LO до последнего байта PAYLOAD включительно.
 * ------------------------------------------------------------------------- */

#define HMI_NODE_CONTROLLER       0x10u
#define HMI_NODE_HMI              0x20u

#define HMI_TYPE_POLL             0x01u
#define HMI_TYPE_TELEMETRY        0x81u
#define HMI_TYPE_CMD              0x02u
#define HMI_TYPE_ACK              0x82u
#define HMI_TYPE_GET_CONFIG       0x03u
#define HMI_TYPE_CONFIG           0x83u
#define HMI_TYPE_PING             0x04u
#define HMI_TYPE_PONG             0x84u

#define HMI_MAX_PAYLOAD           512u
#define HMI_FRAME_OVERHEAD        10u
#define HMI_FRAME_MAX             (HMI_FRAME_OVERHEAD + HMI_MAX_PAYLOAD)

#define HMI_MAGIC0                0xAAu
#define HMI_MAGIC1                0x55u

typedef struct {
    uint16_t seq;
    uint8_t  node;
    uint8_t  type;
    uint16_t len;
    uint8_t  payload[HMI_MAX_PAYLOAD + 1]; /* +1 под NUL для JSON */
} hmi_frame_t;

/* CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF). */
uint16_t hmi_crc16(const uint8_t *data, size_t len);

/* Собрать кадр в out[]. Возвращает false, если payload слишком длинный. */
bool hmi_frame_build(uint8_t *out, size_t *out_len,
                     uint16_t seq, uint8_t node, uint8_t type,
                     const uint8_t *payload, uint16_t len);

/* Разобрать готовый кадр. Проверяет magic, LEN и CRC. */
bool hmi_frame_parse(const uint8_t *buf, size_t len, hmi_frame_t *frame);
