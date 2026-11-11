#pragma once
#include "driver/i2c_master.h"
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

esp_err_t hal_i2c_init(void);
i2c_master_bus_handle_t hal_i2c_bus(void);
bool hal_i2c_probe(uint8_t addr);