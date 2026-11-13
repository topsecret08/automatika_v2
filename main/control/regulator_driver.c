#include "regulator_driver.h"
#include "regulator_math.h"

#include "config.h"
#include "core/control_config.h"
#include "control/ctrl_heater.h"

#include "driver/ledc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "REGULATOR";

#define REG_LEDC_MODE       LEDC_LOW_SPEED_MODE
#define REG_LEDC_TIMER      LEDC_TIMER_2
#define REG_LEDC_CHANNEL    LEDC_CHANNEL_3
#define REG_LEDC_RES        LEDC_TIMER_10_BIT
#define REG_PWM_DUTY_MAX    ((1U << 10) - 1)

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

static bool is_uart_type(regulator_type_t t) {
    return t == REGULATOR_KVIC || t == REGULATOR_RMVK ||
           t == REGULATOR_SEM_AVR;
}

static esp_err_t pwm_init(int gpio) {
    s_pwm_ready = false;
    if (gpio < 0) {
        ESP_LOGE(TAG, "SSR GPIO is not configured");
        return ESP_ERR_INVALID_ARG;
    }

    ledc_timer_config_t timer = {
        .speed_mode = REG_LEDC_MODE,
        .timer_num = REG_LEDC_TIMER,
        .duty_resolution = REG_LEDC_RES,
        .freq_hz = REG_PWM_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    esp_err_t err = ledc_timer_config(&timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ledc_timer_config: %s", esp_err_to_name(err));
        return err;
    }

    ledc_channel_config_t channel = {
        .gpio_num = gpio,
        .speed_mode = REG_LEDC_MODE,
        .channel = REG_LEDC_CHANNEL,
        .timer_sel = REG_LEDC_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    err = ledc_channel_config(&channel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ledc_channel_config: %s", esp_err_to_name(err));
        return err;
    }

    err = ledc_set_duty(REG_LEDC_MODE, REG_LEDC_CHANNEL, 0);
    if (err == ESP_OK) err = ledc_update_duty(REG_LEDC_MODE, REG_LEDC_CHANNEL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ledc zero duty: %s", esp_err_to_name(err));
        return err;
    }

    s_pwm_ready = true;
    ESP_LOGI(TAG, "SSR PWM on GPIO%d, %d Hz, 10-bit", gpio, REG_PWM_FREQ_HZ);
    return ESP_OK;
}

static esp_err_t pwm_set_power(float percent) {
    if (!s_pwm_ready) return ESP_ERR_INVALID_STATE;

    percent = clamp_pct(percent);
    uint32_t duty = regulator_power_to_duty(percent);
    if (duty > REG_PWM_DUTY_MAX) duty = REG_PWM_DUTY_MAX;

    esp_err_t err = ledc_set_duty(REG_LEDC_MODE, REG_LEDC_CHANNEL, duty);
    if (err == ESP_OK)
        err = ledc_update_duty(REG_LEDC_MODE, REG_LEDC_CHANNEL);
    return err;
}

static void uart_close(void) {
    if (is_uart_type(s_cfg.type) && uart_is_driver_installed(s_cfg.uart_num)) {
        uart_driver_delete(s_cfg.uart_num);
    }
}

void regulator_emergency_off(void) {
    s_power_set = 0.0f;

    if (s_pwm_ready) {
        ledc_set_duty(REG_LEDC_MODE, REG_LEDC_CHANNEL, 0);
        ledc_update_duty(REG_LEDC_MODE, REG_LEDC_CHANNEL);
    }

    if (s_ready && is_uart_type(s_cfg.type)) {
        const char *cmd = (s_cfg.type == REGULATOR_KVIC) ? "P000\n" : "P000\r\n";
        uart_write_bytes(s_cfg.uart_num, cmd, strlen(cmd));
    }
}

void regulator_deinit(void) {
    regulator_emergency_off();
    if (s_pwm_ready) s_pwm_ready = false;
    uart_close();
    s_ready = false;
    s_power_set = 0.0f;
    memset(&s_cfg, 0, sizeof(s_cfg));
}

esp_err_t regulator_init(const regulator_config_t *cfg) {
    if (!cfg) return ESP_ERR_INVALID_ARG;

    regulator_emergency_off();
    regulator_deinit();
    s_cfg = *cfg;

    if (s_cfg.type == REGULATOR_NONE) {
        s_ready = false;
        return ESP_OK;
    }

    if (s_cfg.type == REGULATOR_SSR_PWM) {
        esp_err_t err = pwm_init(cfg->tx_gpio);
        s_ready = (err == ESP_OK);
        if (!s_ready) regulator_emergency_off();
        return err;
    }

    if (!is_uart_type(s_cfg.type)) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (cfg->tx_gpio < 0 || cfg->rx_gpio < 0) {
        ESP_LOGE(TAG, "UART pins not configured (tx=%d rx=%d)",
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
    regulator_emergency_off();
    ESP_LOGI(TAG, "UART%d %d baud, type %s", (int)cfg->uart_num, cfg->baud,
             control_config_regulator_name(cfg->type));
    return ESP_OK;
}

static regulator_config_t config_from_control(regulator_type_t type) {
    regulator_config_t cfg = {
        .type = type,
        .uart_num = REG_UART_NUM,
        .tx_gpio = REG_UART_TX_GPIO,
        .rx_gpio = REG_UART_RX_GPIO,
        .baud = (int)control_config_get_regulator_baud(),
    };
    if (type == REGULATOR_SSR_PWM) cfg.tx_gpio = REG_SSR_GPIO;
    return cfg;
}

esp_err_t regulator_init_from_config(void) {
    regulator_config_t cfg = config_from_control(control_config_get_regulator());
    return regulator_init(&cfg);
}

esp_err_t regulator_select(regulator_type_t type) {
    if (type >= REGULATOR_COUNT) return ESP_ERR_INVALID_ARG;

    regulator_type_t old = control_config_get_regulator();
    if (type == old) {
        return regulator_init_from_config();
    }

    // Сначала гарантированно снимаем мощность со старого выхода.
    ctrl_heater_force_off();
    regulator_emergency_off();
    regulator_deinit();

    regulator_config_t cfg = config_from_control(type);
    esp_err_t err = regulator_init(&cfg);
    if (err != ESP_OK) {
        // Возвращаем старую конфигурацию и старый драйвер.
        regulator_config_t rollback = config_from_control(old);
        (void)regulator_init(&rollback);
        ESP_LOGE(TAG, "select %s failed: %s; rollback %s",
                 control_config_regulator_name(type), esp_err_to_name(err),
                 control_config_regulator_name(old));
        return err;
    }

    err = control_config_set_regulator(type);
    if (err != ESP_OK) {
        regulator_emergency_off();
        regulator_deinit();
        regulator_config_t rollback = config_from_control(old);
        (void)regulator_init(&rollback);
        return err;
    }

    ESP_LOGI(TAG, "selected %s", control_config_regulator_name(type));
    return ESP_OK;
}

esp_err_t regulator_set_power(float percent) {
    percent = clamp_pct(percent);

    if (!s_ready) return ESP_ERR_INVALID_STATE;

    esp_err_t err = ESP_OK;
    switch (s_cfg.type) {
    case REGULATOR_SSR_PWM:
        err = pwm_set_power(percent);
        break;

    case REGULATOR_KVIC:
    case REGULATOR_RMVK:
    case REGULATOR_SEM_AVR: {
        char cmd[16];
        int len = regulator_format_command(s_cfg.type == REGULATOR_KVIC,
                                           percent, cmd, sizeof(cmd));
        if (len <= 0) {
            err = ESP_FAIL;
        } else {
            int written = uart_write_bytes(s_cfg.uart_num, cmd, (size_t)len);
            err = (written == len) ? ESP_OK : ESP_FAIL;
        }
        break;
    }

    case REGULATOR_NONE:
    default:
        err = ESP_ERR_NOT_SUPPORTED;
        break;
    }

    if (err == ESP_OK) {
        s_power_set = percent;
    } else {
        regulator_emergency_off();
    }
    return err;
}

float regulator_get_power(void) { return s_power_set; }
bool  regulator_is_ready(void) { return s_ready; }
bool  regulator_is_external(void) {
    return s_ready && s_cfg.type != REGULATOR_NONE;
}
regulator_type_t regulator_get_type(void) { return s_cfg.type; }

int regulator_get_gpio(void) {
    if (s_cfg.type == REGULATOR_NONE) return -1;
    if (s_cfg.type == REGULATOR_SSR_PWM) return s_cfg.tx_gpio;
    return s_cfg.tx_gpio;
}

int regulator_get_pwm_freq(void) {
    return (s_cfg.type == REGULATOR_SSR_PWM) ? REG_PWM_FREQ_HZ : 0;
}
