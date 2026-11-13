#include "hal_touch.h"
#include "hal_i2c.h"
#include "config.h"
#include "ui/ui_fps.h"
#include "ui/ui_gestures.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "TOUCH";
static lv_indev_drv_t s_indev;
static i2c_master_dev_handle_t s_dev = NULL;

#define FT6336_REG_TD_STATUS  0x02
#define FT6336_REG_TOUCH1_XH  0x03

static uint8_t s_last_pressed = 0;
static int16_t s_last_x = 0;
static int16_t s_last_y = 0;

static bool ft_read_reg(uint8_t reg, uint8_t *buf, size_t len) {
    if (!s_dev) return false;
    return i2c_master_transmit_receive(s_dev, &reg, 1, buf, len, 100) == ESP_OK;
}

static void touch_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data) {
    (void)drv;
    uint8_t buf[5] = {0};

    if (!ft_read_reg(FT6336_REG_TD_STATUS, buf, 5)) {
        data->state = LV_INDEV_STATE_RELEASED;
        ui_gestures_feed(s_last_x, s_last_y, false);
        return;
    }

    uint8_t touches = buf[0] & 0x0F;
    if (touches > 0) {
        int16_t x = ((buf[1] & 0x0F) << 8) | buf[2];
        int16_t y = ((buf[3] & 0x0F) << 8) | buf[4];
        data->point.x = 320 - x;   // если тач перевёрнут
        data->point.y = y;
        data->state = LV_INDEV_STATE_PRESSED;

        s_last_x = data->point.x;
        s_last_y = data->point.y;
        ui_gestures_feed(s_last_x, s_last_y, true);

        if (!s_last_pressed) {
            ui_fps_touch_event();
            s_last_pressed = 1;
        }
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
        ui_gestures_feed(s_last_x, s_last_y, false);
        s_last_pressed = 0;
    }
}

esp_err_t hal_touch_init(void) {
    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = I2C_ADDR_FT6336,
        .scl_speed_hz = 400000,
    };
    esp_err_t err = i2c_master_bus_add_device(hal_i2c_bus(), &cfg, &s_dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Не удалось добавить устройство тача: %s", esp_err_to_name(err));
        return err;
    }

    lv_indev_drv_init(&s_indev);
    s_indev.type = LV_INDEV_TYPE_POINTER;
    s_indev.read_cb = touch_read_cb;
    lv_indev_drv_register(&s_indev);

    ESP_LOGI(TAG, "FT6336U touch initialized");
    return ESP_OK;
}

void hal_touch_task(void *arg) {
    (void)arg;
    while (1) vTaskDelay(pdMS_TO_TICKS(1000));
}