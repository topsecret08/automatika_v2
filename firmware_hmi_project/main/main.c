/*
 * Automatika HMI — Waveshare ESP32-S3-Touch-LCD-7
 * LVGL 8.3 + RGB 800x480 + GT911 + RS485 master (115200, 500 ms poll).
 */

#include <assert.h>

#include "esp_log.h"
#include "esp_lv_adapter.h"
#include "lvgl.h"

#include "waveshare_rgb_lcd_port.h"

#include "hmi_proto.h"
#include "hmi_rs485.h"
#include "hmi_telemetry.h"

#include "ui_generated.h"
#include "ui_screen_menu.h"

static const char *TAG = "hmi_main";

static void on_menu_select(lv_event_t *e)
{
    ui_menu_item_t *item = (ui_menu_item_t *)lv_event_get_user_data(e);
    if (!item) return;

    ui_gen_screen_t target = UI_GEN_MAIN;
    switch (*item) {
        case UI_MENU_WORK: target = UI_GEN_WORK_RECTIFICATION; break;
        case UI_MENU_PREP: target = UI_GEN_PREPARATION; break;
        case UI_MENU_DIAG: target = UI_GEN_DIAGNOSTICS; break;
        case UI_MENU_SYS:  target = UI_GEN_SYSTEM; break;
        default: break;
    }

    if (esp_lv_adapter_lock(-1) == ESP_OK) {
        ui_gen_screen_create(target, on_menu_select);
        esp_lv_adapter_unlock();
    }
}

static void hmi_ui_tick_cb(lv_timer_t *timer)
{
    (void)timer;
    ui_gen_clock_tick();
    hmi_telemetry_apply_ui();
}

void app_main(void)
{
    ESP_LOGI(TAG, "Automatika HMI starting");

    const esp_lv_adapter_rotation_t rotation = ESP_LV_ADAPTER_ROTATE_0;
    const esp_lv_adapter_tear_avoid_mode_t tear_mode =
        ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT_RGB;

    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_touch_handle_t touch_handle = NULL;

    ESP_ERROR_CHECK(waveshare_esp32_s3_rgb_lcd_init(
        tear_mode, rotation, &panel_handle, &touch_handle));
    ESP_ERROR_CHECK(waveshare_rgb_lcd_backlight_on());

    esp_lv_adapter_config_t adapter_config = ESP_LV_ADAPTER_DEFAULT_CONFIG();
    adapter_config.task_stack_size = 12 * 1024;
    adapter_config.stack_in_psram = true;
    ESP_ERROR_CHECK(esp_lv_adapter_init(&adapter_config));

    esp_lv_adapter_display_config_t disp_config =
        ESP_LV_ADAPTER_DISPLAY_RGB_DEFAULT_CONFIG(
            panel_handle, NULL, EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES, rotation);
    disp_config.profile.use_psram = true;
    lv_display_t *disp = esp_lv_adapter_register_display(&disp_config);
    assert(disp != NULL);

    if (touch_handle != NULL) {
        esp_lv_adapter_touch_config_t touch_config =
            ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(disp, touch_handle);
        lv_indev_t *touch = esp_lv_adapter_register_touch(&touch_config);
        assert(touch != NULL);
    }

    ESP_ERROR_CHECK(esp_lv_adapter_start());

    /* RS485 master task: 115200, POLL every 500 ms, 100 ms reply timeout. */
    hmi_rs485_init();

    /* UI must be created inside the LVGL task context. */
    if (esp_lv_adapter_lock(-1) == ESP_OK) {
        ui_gen_init_overlays();
        ui_gen_screen_create(UI_GEN_MAIN, on_menu_select);
        ui_screen_menu_set_active(UI_MENU_WORK);
        esp_lv_adapter_unlock();
    }

    lv_timer_create(hmi_ui_tick_cb, HMI_POLL_MS, NULL);

    ESP_LOGI(TAG, "HMI ready: 800x480, RS485 %d baud, poll %d ms",
             HMI_UART_BAUD, HMI_POLL_MS);
}
