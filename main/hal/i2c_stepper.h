#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "core/stepper_proto.h"

// Транспорт I2C для внешнего драйвера шагового двигателя.
// Протокол описан в core/stepper_proto.h; здесь только обмен кадрами.
//
// Драйвер не является критичным для загрузки: при отсутствии устройства
// i2c_stepper_init() возвращает ошибку, а ctrl_stepper остаётся offline.

esp_err_t i2c_stepper_init(uint8_t address, uint32_t timeout_ms);
esp_err_t i2c_stepper_deinit(void);
bool      i2c_stepper_online(void);
uint8_t   i2c_stepper_address(void);

// Один запрос-ответ. Возвращает ESP_ERR_TIMEOUT/ESP_FAIL при обрыве или
// повреждённом кадре.
esp_err_t i2c_stepper_transact(const stepper_request_t *req,
                               stepper_status_t *status,
                               uint32_t timeout_ms);
