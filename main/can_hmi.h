#pragma once

#include <stdbool.h>
#include "esp_err.h"

/**
 * CAN (TWAI) slave interface for external HMI panel.
 * HMI is bus master for application protocol: it polls/commands, controller responds.
 * Safety logic and E-STOP remain on controller.
 */
esp_err_t can_hmi_init(void);
bool can_hmi_ready(void);
