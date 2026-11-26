#pragma once
#include <stdint.h>
#include "ui_generated.h"

typedef uint16_t hmi_screen_id_t;

#define HMI_SCR_NONE  0xFFFF
#define HMI_SCR_COUNT UI_GEN_COUNT

const char *hmi_screen_name(hmi_screen_id_t id);
hmi_screen_id_t hmi_screen_from_name(const char *name);
