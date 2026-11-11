#include "hal_display.h"
#include "config.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_commands.h"
#include "driver/spi_master.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "LCD";

static esp_lcd_panel_handle_t s_panel = NULL;
static SemaphoreHandle_t s_mtx = NULL;
static lv_disp_draw_buf_t s_draw_buf;
static lv_color_t *s_buf1 = NULL;
static lv_color_t *s_buf2 = NULL;
static bool s_ready = false;
static int s_backlight = 100;

// ============================================================
// LVGL flush callback
// ============================================================
static bool lcd_flush_ready_cb(esp_lcd_panel_io_handle_t io,
                               esp_lcd_panel_io_event_data_t *edata,
                               void *user_ctx) {
    lv_disp_drv_t *drv = (lv_disp_drv_t *)user_ctx;
    lv_disp_flush_ready(drv);
    return false;
}

static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *px) {
    esp_lcd_panel_draw_bitmap(s_panel,
                              area->x1, area->y1,
                              area->x2 + 1, area->y2 + 1, px);
}

// ============================================================
// Подсветка через LEDC (PWM)
// ============================================================
static void backlight_init(void) {
    ledc_timer_config_t t = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_13_BIT,
        .timer_num       = LEDC_TIMER_0,
        .freq_hz         = 5000,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&t);

    ledc_channel_config_t c = {
        .gpio_num   = PIN_LCD_BL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = LEDC_CHANNEL_0,
        .timer_sel  = LEDC_TIMER_0,
        .duty       = 8191,   // 100%
        .hpoint     = 0,
    };
    ledc_channel_config(&c);
}

void hal_display_backlight(int percent) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    s_backlight = percent;
    uint32_t duty = (uint32_t)((percent * 8191) / 100);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

int hal_display_get_backlight(void) { return s_backlight; }

// ============================================================
// Инициализация дисплея
// ============================================================
void hal_display_init(void) {
    if (s_ready) return;

    // === SPI2 шина для LCD ===
    spi_bus_config_t bus = {
        .mosi_io_num     = PIN_LCD_MOSI,
        .miso_io_num     = -1,
        .sclk_io_num     = PIN_LCD_CLK,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = 480 * 320 * 2 + 100,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));

    // === Panel IO ===
    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num       = PIN_LCD_DC,
        .cs_gpio_num       = PIN_LCD_CS,
        .pclk_hz           = 40 * 1000 * 1000,
        .lcd_cmd_bits      = 8,
        .lcd_param_bits    = 8,
        .spi_mode          = 0,
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST,
                                              &io_cfg, &io));

    // === ST7796 панель ===
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = PIN_LCD_RST,
        .color_space    = ESP_LCD_COLOR_SPACE_BGR,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7796(io, &panel_cfg, &s_panel));

    esp_lcd_panel_reset(s_panel);
    esp_lcd_panel_init(s_panel);
    esp_lcd_panel_invert_color(s_panel, true);
    esp_lcd_panel_swap_xy(s_panel, true);
    esp_lcd_panel_mirror(s_panel, true, false);
    esp_lcd_panel_set_gap(s_panel, 0, 0);
    esp_lcd_panel_disp_on_off(s_panel, true);

    // === Подсветка ===
    backlight_init();
    hal_display_backlight(100);

    // === LVGL ===
    lv_init();

    size_t buf_sz = 480 * 40;
    s_buf1 = heap_caps_malloc(buf_sz * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    s_buf2 = heap_caps_malloc(buf_sz * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    if (!s_buf1 || !s_buf2) {
        ESP_LOGE(TAG, "PSRAM alloc failed for LVGL buffers");
        return;
    }
    lv_disp_draw_buf_init(&s_draw_buf, s_buf1, s_buf2, buf_sz);

    static lv_disp_drv_t drv;
    lv_disp_drv_init(&drv);
    drv.hor_res  = 480;
    drv.ver_res  = 320;
    drv.flush_cb = lvgl_flush_cb;
    drv.draw_buf = &s_draw_buf;
    lv_disp_drv_register(&drv);

    s_mtx = xSemaphoreCreateMutex();
    s_ready = true;
    ESP_LOGI(TAG, "ST7796 480x320 initialized (SPI2, buffers in PSRAM)");
}

void hal_display_lock(void) {
    if (s_mtx) xSemaphoreTake(s_mtx, portMAX_DELAY);
}

void hal_display_unlock(void) {
    if (s_mtx) xSemaphoreGive(s_mtx);
}

bool hal_display_is_ready(void) { return s_ready; }