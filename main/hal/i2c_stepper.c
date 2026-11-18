#include "i2c_stepper.h"

#include "hal/hal_i2c.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <string.h>

static const char *TAG = "I2C_STEP";

static i2c_master_dev_handle_t s_dev;
static SemaphoreHandle_t s_mtx;
static uint32_t s_timeout_ms = 50;
static uint8_t s_addr;
static bool s_online;

esp_err_t i2c_stepper_init(uint8_t address, uint32_t timeout_ms) {
    if (!s_mtx) s_mtx = xSemaphoreCreateMutex();
    if (!s_mtx) return ESP_ERR_NO_MEM;

    i2c_master_bus_handle_t bus = hal_i2c_bus();
    if (!bus) return ESP_ERR_INVALID_STATE;

    if (s_dev) {
        (void)i2c_master_bus_rm_device(s_dev);
        s_dev = NULL;
    }

    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = 100000,
    };
    esp_err_t err = i2c_master_bus_add_device(bus, &cfg, &s_dev);
    if (err != ESP_OK) {
        s_dev = NULL;
        s_online = false;
        return err;
    }

    s_addr = address;
    s_timeout_ms = timeout_ms ? timeout_ms : 50;
    s_online = false;

    // Проверка присутствия: PING и валидный статус.
    stepper_request_t req = { .seq = 0, .cmd = STEPPER_CMD_PING,
                              .axis = STEPPER_AXIS_DOSER, .len = 0 };
    stepper_status_t st;
    err = i2c_stepper_transact(&req, &st, s_timeout_ms);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "no stepper at 0x%02X (%s)", address,
                 esp_err_to_name(err));
        return err;
    }

    s_online = true;
    ESP_LOGI(TAG, "online at 0x%02X, vin=%u mV", address, st.vin_mv);
    return ESP_OK;
}

esp_err_t i2c_stepper_deinit(void) {
    if (!s_dev) return ESP_OK;
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    esp_err_t err = i2c_master_bus_rm_device(s_dev);
    s_dev = NULL;
    s_online = false;
    xSemaphoreGive(s_mtx);
    return err;
}

bool i2c_stepper_online(void) { return s_online; }

uint8_t i2c_stepper_address(void) { return s_addr; }

esp_err_t i2c_stepper_transact(const stepper_request_t *req,
                               stepper_status_t *status,
                               uint32_t timeout_ms) {
    if (!req || !status || !s_dev || !s_mtx)
        return ESP_ERR_INVALID_STATE;

    uint8_t frame[8 + STEPPER_MAX_PAYLOAD];
    size_t frame_len = stepper_encode(req, frame, sizeof(frame));
    if (frame_len == 0) return ESP_ERR_INVALID_SIZE;

    uint8_t response[STEPPER_STATUS_LEN];
    memset(response, 0, sizeof(response));

    if (xSemaphoreTake(s_mtx, pdMS_TO_TICKS(timeout_ms)) != pdTRUE)
        return ESP_ERR_TIMEOUT;

    esp_err_t err = i2c_master_transmit_receive(
        s_dev, frame, frame_len, response, sizeof(response),
        (int)timeout_ms);
    xSemaphoreGive(s_mtx);

    if (err != ESP_OK) {
        s_online = false;
        return err;
    }

    if (!stepper_decode_status(response, sizeof(response), status)) {
        s_online = false;
        return ESP_ERR_INVALID_RESPONSE;
    }

    s_online = true;
    return ESP_OK;
}
