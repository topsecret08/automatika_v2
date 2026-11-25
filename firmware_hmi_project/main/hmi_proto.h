#pragma once

/* Кадр, TYPE/NODE, CRC и hmi_frame_t живут в общем компоненте
 * rs485_protocol. Здесь остаются только параметры физической линии HMI. */
#include "hmi_protocol.h"

/* ---------------- RS485 link parameters (confirmed 2026-11-25) -------------- */
#define HMI_UART_BAUD             115200
#define HMI_UART_NUM              1
#define HMI_UART_TX_GPIO          15
#define HMI_UART_RX_GPIO          16
#define HMI_UART_RX_BUF_SIZE      1024
#define HMI_UART_TX_BUF_SIZE      512

#define HMI_POLL_MS               500
#define HMI_REPLY_TIMEOUT_MS      100
#define HMI_MAX_RETRIES           2
#define HMI_TX_GAP_US             2000
#define HMI_LINK_MISS_LIMIT       3
