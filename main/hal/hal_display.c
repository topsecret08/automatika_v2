#include "hal_display.h"
#include "config.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_st7796.h"
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

#define LCD_W 480
#define LCD_H 320
#define LVGL_LINES_PSRAM 40
#define LVGL_LINES_SRAM 10

static esp_lcd_panel_handle_t s_panel = NULL;
static esp_lcd_panel_io_handle_t s_io = NULL;
static SemaphoreHandle_t s_mtx = NULL;
static lv_disp_draw_buf_t s_draw_buf;
static lv_color_t *s_buf1 = NULL;
static lv_color_t *s_buf2 = NULL;
static bool s_ready = false;
static bool s_lv_inited = false;
static int s_backlight = 100;
static uint32_t s_flush_errors = 0;

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
    esp_err_t err = esp_lcd_panel_draw_bitmap(s_panel,
                                              area->x1, area->y1,
                                              area->x2 + 1, area->y2 + 1, px);
    if (err != ESP_OK) {
        // Без этого LVGL ждёт завершения кадра, который не придёт: экран замрёт.
        if (s_flush_errors++ == 0) {
            ESP_LOGE(TAG, "draw_bitmap failed: %s", esp_err_to_name(err));
        }
        lv_disp_flush_ready(drv);
    }
}

// ============================================================
// Подсветка через LEDC (PWM)
// ============================================================
static esp_err_t backlight_init(void) {
    ledc_timer_config_t t = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_13_BIT,
        .timer_num       = LEDC_TIMER_0,
        .freq_hz         = 5000,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    esp_err_t err = ledc_timer_config(&t);
    if (err != ESP_OK) return err;

    ledc_channel_config_t c = {
        .gpio_num   = PIN_LCD_BL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = LEDC_CHANNEL_0,
        .timer_sel  = LEDC_TIMER_0,
        .duty       = 8191,   // 100%
        .hpoint     = 0,
    };
    return ledc_channel_config(&c);
}

void hal_display_backlight(int percent) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    s_backlight = percent;
    uint32_t duty = (uint32_t)((percent * 8191) / 100);
    esp_err_t err = ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    if (err == ESP_OK) err = ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    if (err != ESP_OK) ESP_LOGW(TAG, "backlight: %s", esp_err_to_name(err));
}

int hal_display_get_backlight(void) { return s_backlight; }

// ============================================================
// Инициализация дисплея
// ============================================================
#define LCD_CHECK(expr, what)                                              \
    do {                                                                   \
        err = (expr);                                                      \
        if (err != ESP_OK) {                                               \
            ESP_LOGE(TAG, "%s: %s", (what), esp_err_to_name(err));         \
            goto fail;                                                     \
        }                                                                  \
    } while (0)

esp_err_t hal_display_init(void) {
    if (s_ready) return ESP_OK;

    if (!s_mtx) {
        s_mtx = xSemaphoreCreateMutex();
        if (!s_mtx) {
            ESP_LOGE(TAG, "mutex alloc failed");
            return ESP_ERR_NO_MEM;
        }
    }

    esp_err_t err = ESP_OK;
    bool bus_up = false;
    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_handle_t panel = NULL;
    lv_color_t *b1 = NULL;
    lv_color_t *b2 = NULL;
    static lv_disp_drv_t drv;
    static esp_lcd_panel_io_callbacks_t io_cbs = {
        .on_color_trans_done = lcd_flush_ready_cb,
    };

    // === SPI2 шина для LCD ===
    spi_bus_config_t bus = {
        .mosi_io_num     = PIN_LCD_MOSI,
        .miso_io_num     = -1,
        .sclk_io_num     = PIN_LCD_CLK,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = LCD_W * LCD_H * 2 + 100,
    };
    LCD_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO), "spi bus");
    bus_up = true;

    // === Panel IO ===
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num       = PIN_LCD_DC,
        .cs_gpio_num       = PIN_LCD_CS,
        // 20 МГц: на проводах и макетной плате 40 МГц даёт помехи на картинке.
        .pclk_hz           = 20 * 1000 * 1000,
        .lcd_cmd_bits      = 8,
        .lcd_param_bits    = 8,
        .spi_mode          = 0,
        .trans_queue_depth = 10,
    };
    LCD_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST,
                                       &io_cfg, &io), "panel io");

    // === ST7796 панель ===
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = PIN_LCD_RST,
        .color_space    = ESP_LCD_COLOR_SPACE_BGR,
        .bits_per_pixel = 16,
    };
    LCD_CHECK(esp_lcd_new_panel_st7796(io, &panel_cfg, &panel), "st7796 create");
    LCD_CHECK(esp_lcd_panel_reset(panel), "panel reset");
    LCD_CHECK(esp_lcd_panel_init(panel), "panel init");
    LCD_CHECK(esp_lcd_panel_invert_color(panel, true), "invert color");
    LCD_CHECK(esp_lcd_panel_swap_xy(panel, true), "swap xy");
    LCD_CHECK(esp_lcd_panel_mirror(panel, true, false), "mirror");
    LCD_CHECK(esp_lcd_panel_set_gap(panel, 0, 0), "set gap");
    LCD_CHECK(esp_lcd_panel_disp_on_off(panel, true), "display on");

    // === LVGL ===
    if (!s_lv_inited) {
        lv_init();
        s_lv_inited = true;
    }

    // Сначала большие буферы в PSRAM; если их нет, — маленькие во внутренней RAM.
    size_t lines = LVGL_LINES_PSRAM;
    b1 = heap_caps_malloc(LCD_W * lines * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    b2 = heap_caps_malloc(LCD_W * lines * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    if (!b1 || !b2) {
        heap_caps_free(b1);
        heap_caps_free(b2);
        lines = LVGL_LINES_SRAM;
        b1 = heap_caps_malloc(LCD_W * lines * sizeof(lv_color_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        b2 = heap_caps_malloc(LCD_W * lines * sizeof(lv_color_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        if (!b1 || !b2) {
            ESP_LOGE(TAG, "LVGL buffers: no memory");
            err = ESP_ERR_NO_MEM;
            goto fail;
        }
        ESP_LOGW(TAG, "PSRAM недоступна, буфер LVGL %u строк во внутренней RAM", (unsigned)lines);
    }
    lv_disp_draw_buf_init(&s_draw_buf, b1, b2, LCD_W * lines);

    lv_disp_drv_init(&drv);
    drv.hor_res  = LCD_W;
    drv.ver_res  = LCD_H;
    drv.flush_cb = lvgl_flush_cb;
    drv.draw_buf = &s_draw_buf;

    LCD_CHECK(esp_lcd_panel_io_register_event_callbacks(io, &io_cbs, &drv), "io callbacks");

    // === Подсветка === (без неё экран работает, просто на полной яркости)
    if (backlight_init() != ESP_OK) {
        ESP_LOGW(TAG, "подсветка не настроена");
    }
    hal_display_backlight(100);

    if (!lv_disp_drv_register(&drv)) {
        err = ESP_FAIL;
        ESP_LOGE(TAG, "lv_disp_drv_register failed");
        goto fail;
    }

    s_io = io;
    s_panel = panel;
    s_buf1 = b1;
    s_buf2 = b2;
    s_ready = true;
    ESP_LOGI(TAG, "ST7796 480x320 initialized (SPI2, LVGL buffer %u lines)", (unsigned)lines);
    return ESP_OK;

fail:
    if (panel) esp_lcd_panel_del(panel);
    if (io) esp_lcd_panel_io_del(io);
    if (bus_up) spi_bus_free(SPI2_HOST);
    heap_caps_free(b1);
    heap_caps_free(b2);
    return err;
}

void hal_display_lock(void) {
    if (s_mtx) xSemaphoreTake(s_mtx, portMAX_DELAY);
}

void hal_display_unlock(void) {
    if (s_mtx) xSemaphoreGive(s_mtx);
}

bool hal_display_is_ready(void) { return s_ready; }
