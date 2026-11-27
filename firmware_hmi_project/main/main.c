/*
 * Automatika HMI — Waveshare ESP32-S3-Touch-LCD-7
 * LVGL 8.3 + RGB 800x480 + GT911 + CAN/TWAI master (500 kbps, 200 ms poll).
 * Direct LVGL port (no esp_lvgl_adapter) for ESP-IDF 5.3.2 compatibility.
 */

#include <assert.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "lvgl.h"

#include "waveshare_rgb_lcd_port.h"

#include "hmi_proto.h"
#include "hmi_can.h"
#include "hmi_storage.h"
#include "hmi_telemetry.h"

#include "ai/page_predictor.h"
#include "ui_generated.h"
#include "ui_screen_menu.h"

static const char *TAG = "hmi_main";

static SemaphoreHandle_t s_lvgl_mux;
static esp_lcd_panel_handle_t s_panel;
static esp_lcd_touch_handle_t s_touch;

static bool lvgl_lock(int timeout_ms)
{
    if (!s_lvgl_mux) return false;
    TickType_t ticks = (timeout_ms < 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(s_lvgl_mux, ticks) == pdTRUE;
}

static void lvgl_unlock(void)
{
    if (s_lvgl_mux) xSemaphoreGiveRecursive(s_lvgl_mux);
}

static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t)drv->user_data;
    int x1 = area->x1, y1 = area->y1, x2 = area->x2 + 1, y2 = area->y2 + 1;
    esp_lcd_panel_draw_bitmap(panel, x1, y1, x2, y2, color_map);
    lv_disp_flush_ready(drv);
}

static void lvgl_touch_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    esp_lcd_touch_handle_t touch = (esp_lcd_touch_handle_t)drv->user_data;
    uint16_t x[1], y[1];
    uint8_t count = 0;

    esp_lcd_touch_read_data(touch);
    bool pressed = esp_lcd_touch_get_coordinates(touch, x, y, NULL, &count, 1);
    if (pressed && count > 0) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = x[0];
        data->point.y = y[0];
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static void lvgl_tick_cb(void *arg)
{
    (void)arg;
    lv_tick_inc(2);
}

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

    if (lvgl_lock(-1)) {
        ui_gen_screen_create(target, on_menu_select);
        lvgl_unlock();
    }
}

static void hmi_ui_tick_cb(lv_timer_t *timer)
{
    (void)timer;
    ui_gen_clock_tick();
    hmi_telemetry_apply_ui();
}

static void lvgl_task(void *arg)
{
    (void)arg;
    while (1) {
        if (lvgl_lock(-1)) {
            uint32_t ms = lv_timer_handler();
            lvgl_unlock();
            if (ms < 1) ms = 1;
            if (ms > 20) ms = 20;
            vTaskDelay(pdMS_TO_TICKS(ms));
        } else {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Automatika HMI starting");

    esp_err_t nvs_err = nvs_flash_init();
    if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES || nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_err);

    hmi_predictor_init();

    s_lvgl_mux = xSemaphoreCreateRecursiveMutex();
    assert(s_lvgl_mux);

    ESP_ERROR_CHECK(waveshare_esp32_s3_rgb_lcd_init(2, &s_panel, &s_touch));
    ESP_ERROR_CHECK(waveshare_rgb_lcd_backlight_on());

    esp_err_t sd_err = hmi_storage_init();
    if (sd_err != ESP_OK) {
        ESP_LOGW(TAG, "HMI SD not available: %s (continuing without SD)", esp_err_to_name(sd_err));
    }

    lv_init();

    // Two full frame buffers in PSRAM for RGB direct mode.
    size_t buf_pixels = EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES;
    lv_color_t *buf1 = heap_caps_malloc(buf_pixels * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    lv_color_t *buf2 = heap_caps_malloc(buf_pixels * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    assert(buf1 && buf2);

    static lv_disp_draw_buf_t draw_buf;
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, buf_pixels);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = EXAMPLE_LCD_H_RES;
    disp_drv.ver_res = EXAMPLE_LCD_V_RES;
    disp_drv.flush_cb = lvgl_flush_cb;
    disp_drv.draw_buf = &draw_buf;
    disp_drv.user_data = s_panel;
    disp_drv.full_refresh = 1;
    lv_disp_t *disp = lv_disp_drv_register(&disp_drv);
    assert(disp != NULL);
    (void)disp;

    if (s_touch) {
        static lv_indev_drv_t indev_drv;
        lv_indev_drv_init(&indev_drv);
        indev_drv.type = LV_INDEV_TYPE_POINTER;
        indev_drv.read_cb = lvgl_touch_cb;
        indev_drv.user_data = s_touch;
        lv_indev_t *indev = lv_indev_drv_register(&indev_drv);
        assert(indev != NULL);
        (void)indev;
    }

    const esp_timer_create_args_t tick_args = {
        .callback = &lvgl_tick_cb,
        .name = "lv_tick",
    };
    esp_timer_handle_t tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&tick_args, &tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(tick_timer, 2000));

    hmi_can_init();

    if (lvgl_lock(-1)) {
        ui_gen_init_overlays();
        ui_gen_screen_create(UI_GEN_MAIN, on_menu_select);
        ui_screen_menu_set_active(UI_MENU_WORK);
        lvgl_unlock();
    }

    lv_timer_create(hmi_ui_tick_cb, HMI_POLL_MS, NULL);

    xTaskCreatePinnedToCore(lvgl_task, "lvgl", 12 * 1024, NULL, 4, NULL, 1);

    ESP_LOGI(TAG, "HMI ready: 800x480, CAN %d kbps TX=%d RX=%d, poll %d ms",
             HMI_CAN_BITRATE_KBPS, HMI_CAN_TX_GPIO, HMI_CAN_RX_GPIO, HMI_POLL_MS);
}
