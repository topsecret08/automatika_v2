#include "regulator_driver.h"

#include "config.h"
#include "core/control_config.h"

#include "driver/ledc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "REGULATOR";

#define REG_LEDC_MODE       LEDC_LOW_SPEED_MODE
#define REG_LEDC_TIMER      LEDC_TIMER_2
#define REG_LEDC_CHANNEL    LEDC_CHANNEL_3
#define REG_LEDC_RES        LEDC_TIMER_10_BIT
#define REG_LEDC_FREQ_HZ    5000

static regulator_config_t s_cfg;
static bool    s_ready = false;
static bool    s_pwm_ready = false;
static float   s_power_set = 0.0f;

static float clamp_pct(float p) {
    if (!isfinite(p)) return 0.0f;
    if (p < 0.0f) return 0.0f;
    if (p > 100.0f) return 100.0f;
    return p;
}

static void uart_close(void) {
    if (s_cfg.type == REGULATOR_KVIC || s_cfg.type == REGULATOR_RMVK ||
        s_cfg.type == REGULATOR_SEM_AVR) {
        if (uart_is_driver_installed(s_cfg.uart_num)) {
            uart_driver_delete(s_cfg.uart_num);
        }
    }
}

static esp_err_t pwm_init(int gpio) {
    if (gpio < 0) {
        ESP_LOGW(TAG, "SSR GPIO not configured");
        return ESP_ERR_INVALID_ARG;
    }

    ledc_timer_config_t t = {
        .speed_mode = REG_LEDC_MODE,
        .timer_num = REG_LEDC_TIMER,
        .duty_resolution = REG_LEDC_RES,
        .freq_hz = REG_LEDC_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    esp_err_t err = ledc_timer_config(&t);
    if (err != ESP_OK) return err;

    ledc_channel_config_t ch = {
        .gpio_num = gpio,
        .speed_mode = REG_LEDC_MODE,
        .channel = REG_LEDC_CHANNEL,
        .timer_sel = REG_LEDC_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    err = ledc_channel_config(&ch);
    if (err != ESP_OK) return err;

    s_pwm_ready = true;
    return ESP_OK;
}

void regulator_deinit(void) {
    if (s_pwm_ready) {
        ledc_set_duty(REG_LEDC_MODE, REG_LEDC_CHANNEL, 0);
        ledc_update_duty(REG_LEDC_MODE, REG_LEDC_CHANNEL);
        s_pwm_ready = false;
    }
    uart_close();
    s_ready = false;
    s_power_set = 0.0f;
}

esp_err_t regulator_init(const regulator_config_t *cfg) {
    if (!cfg) return ESP_ERR_INVALID_ARG;

    regulator_deinit();
    s_cfg = *cfg;

    if (s_cfg.type == REGULATOR_NONE) {
        s_ready = false;
        return ESP_OK;
    }

    if (s_cfg.type == REGULATOR_SSR_PWM) {
        esp_err_t err = pwm_init(cfg->tx_gpio);
        s_ready = (err == ESP_OK);
        return err;
    }

    if (cfg->tx_gpio < 0 || cfg->rx_gpio < 0) {
        ESP_LOGW(TAG, "UART pins not configured (tx=%d rx=%d)",
                 cfg->tx_gpio, cfg->rx_gpio);
        return ESP_ERR_INVALID_ARG;
    }
    if (cfg->baud < 1200 || cfg->baud > 115200) {
        return ESP_ERR_INVALID_ARG;
    }

    uart_config_t uart = {
        .baud_rate = cfg->baud,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t err = uart_param_config(cfg->uart_num, &uart);
    if (err != ESP_OK) return err;

    err = uart_set_pin(cfg->uart_num, cfg->tx_gpio, cfg->rx_gpio,
                       UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) return err;

    err = uart_driver_install(cfg->uart_num, 256, 0, 0, NULL, 0);
    if (err != ESP_OK) return err;

    s_ready = true;
    ESP_LOGI(TAG, "UART%d %d baud, type %s", (int)cfg->uart_num, cfg->baud,
             control_config_regulator_name(cfg->type));
    return ESP_OK;
}

esp_err_t regulator_init_from_config(void) {
    regulator_config_t cfg = {
        .type = control_config_get_regulator(),
        .uart_num = REG_UART_NUM,
        .tx_gpio = REG_UART_TX_GPIO,
        .rx_gpio = REG_UART_RX_GPIO,
        .baud = (int)control_config_get_regulator_baud(),
    };

    if (cfg.type == REGULATOR_SSR_PWM) {
        cfg.tx_gpio = REG_SSR_GPIO;
    } else if (cfg.type == REGULATOR_KVIC && cfg.baud == 9600) {
        // KVIC допускает 38400; оставляем выбранную скорость.
    }
    return regulator_init(&cfg);
}

esp_err_t regulator_set_power(float percent) {
    percent = clamp_pct(percent);
    s_power_set = percent;

    if (!s_ready) return ESP_ERR_INVALID_STATE;

    switch (s_cfg.type) {
    case REGULATOR_SSR_PWM: {
        if (!s_pwm_ready) return ESP_ERR_INVALID_STATE;
        uint32_t duty = (uint32_t)((percent / 100.0f) * 1023.0f);
        esp_err_t err = ledc_set_duty(REG_LEDC_MODE, REG_LEDC_CHANNEL, duty);
        if (err == ESP_OK) err = ledc_update_duty(REG_LEDC_MODE, REG_LEDC_CHANNEL);
        return err;
    }

    case REGULATOR_KVIC:
    case REGULATOR_RMVK:
    case REGULATOR_SEM_AVR: {
        char cmd[16];
        const char *eol = (s_cfg.type == REGULATOR_KVIC) ? "\n" : "\r\n";
        int len = snprintf(cmd, sizeof(cmd), "P%03d%s", (int)(percent + 0.5f), eol);
        if (len <= 0) return ESP_FAIL;
        int written = uart_write_bytes(s_cfg.uart_num, cmd, (size_t)len);
        return (written == len) ? ESP_OK : ESP_FAIL;
    }

    case REGULATOR_NONE:
    default:
        return ESP_ERR_NOT_SUPPORTED;
    }
}

float regulator_get_power(void) { return s_power_set; }
bool  regulator_is_ready(void) { return s_ready; }
bool  regulator_is_external(void) {
    return s_ready && s_cfg.type != REGULATOR_NONE;
}
regulator_type_t regulator_get_type(void) { return s_cfg.type; }
