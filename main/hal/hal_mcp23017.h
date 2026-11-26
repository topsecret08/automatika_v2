#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

esp_err_t hal_mcp23017_init(void);
bool      hal_mcp23017_is_ready(void);
esp_err_t hal_mcp23017_write_pin(uint8_t pin, bool high);
esp_err_t hal_mcp23017_write_mask(uint16_t mask, uint16_t value);
uint16_t  hal_mcp23017_read_inputs(void);
bool      hal_mcp23017_read_pin(uint8_t pin);
void      hal_mcp23017_all_off(void);
