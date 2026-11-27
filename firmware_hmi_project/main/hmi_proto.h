#pragma once

#include "can_hmi_proto.h"

/* CAN/TWAI link parameters (Waveshare ESP32-S3-Touch-LCD-7: onboard TJA1051) */
#define HMI_CAN_TX_GPIO           19
#define HMI_CAN_RX_GPIO           20
#define HMI_CAN_BITRATE_KBPS      500

#define HMI_POLL_MS               200
#define HMI_REPLY_TIMEOUT_MS      150
#define HMI_LINK_MISS_LIMIT       3
