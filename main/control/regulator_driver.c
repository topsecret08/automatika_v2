#include "regulator_driver.h"
#include "regulator_math.h"
#include "regulator_settings_logic.h"

#include "config.h"
#include "core/control_config.h"
#include "control/ctrl_heater.h"

#include "driver/gpio.h"
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

static regulator_config_t s_cfg;
static bool    s_ready = false;
static bool    s_pwm_ready = false;
static float   s_power_set = 0.0f;
static char    s_error[96] = "";

static void set_error(const char *msg) {
    snprintf(s_error, sizeof(s_error), "%s", msg ? msg : "");
}

static void set_error_err(const char *what, esp_err_t err) {
    snprintf(s_error, sizeof(s_error), "%s: %s", what, esp_err_to_name(err));
}

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

static esp_err_t pwm_write_duty(uint32_t logical_duty) {
    if (!s_pwm_ready) return ESP_ERR_INVALID_STATE;

    uint32_t duty = regulator_apply_inversion(logical_duty, s_cfg.pwm_res,
                                              s_cfg.pwm_inverted);
    esp_err_t err = ledc_set_duty(REG_LEDC_MODE, REG_LEDC_CHANNEL, duty);
    if (err == ESP_OK)
        err = ledc_update_duty(REG_LEDC_MODE, REG_LEDC_CHANNEL);
    return err;
}

static esp_err_t pwm_init(const regulator_config_t *cfg) {
    s_pwm_ready = false;
    if (!cfg) return ESP_ERR_INVALID_ARG;

    if (cfg->pwm_gpio < 0) {
        set_error("PWM GPIO is not configured (-1)");
        ESP_LOGE(TAG, "%s", s_error);
        return ESP_ERR_INVALID_ARG;
    }
    if (!regulator_pwm_gpio_valid(cfg->pwm_gpio) ||
        !GPIO_IS_VALID_OUTPUT_GPIO((gpio_num_t)cfg->pwm_gpio)) {
        set_error("PWM GPIO is not a valid/free ESP32-S3 output");
        ESP_LOGE(TAG, "%s (gpio=%d)", s_error, cfg->pwm_gpio);
        return ESP_ERR_INVALID_ARG;
    }
    if (!regulator_pwm_freq_valid(cfg->pwm_freq_hz, cfg->pwm_res)) {
        set_error("PWM frequency/resolution is invalid");
        ESP_LOGE(TAG, "%s (freq=%lu res=%u)", s_error,
                 (unsigned long)cfg->pwm_freq_hz, (unsigned)cfg->pwm_res);
        return ESP_ERR_INVALID_ARG;
    }

    ledc_timer_config_t timer = {
        .speed_mode = REG_LEDC_MODE,
        .timer_num = REG_LEDC_TIMER,
        .duty_resolution = (ledc_timer_bit_t)cfg->pwm_res,
        .freq_hz = cfg->pwm_freq_hz,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    esp_err_t err = ledc_timer_config(&timer);
    if (err != ESP_OK) {
        set_error_err("ledc_timer_config", err);
        ESP_LOGE(TAG, "%s", s_error);
        return err;
    }

    uint32_t zero = regulator_apply_inversion(
        regulator_power_to_duty_bits(0.0f, cfg->pwm_res),
        cfg->pwm_res, cfg->pwm_inverted);

    ledc_channel_config_t channel = {
        .gpio_num = cfg->pwm_gpio,
        .speed_mode = REG_LEDC_MODE,
        .channel = REG_LEDC_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = REG_LEDC_TIMER,
        .duty = zero,
        .hpoint = 0,
        .flags.output_invert = 0,   // инверсия уже учтена в duty
    };
    err = ledc_channel_config(&channel);
    if (err != ESP_OK) {
        set_error_err("ledc_channel_config", err);
        ESP_LOGE(TAG, "%s", s_error);
        return err;
    }

    s_pwm_ready = true;
    err = pwm_write_duty(0);
    if (err != ESP_OK) {
        s_pwm_ready = false;
        set_error_err("ledc zero duty", err);
        ESP_LOGE(TAG, "%s", s_error);
        return err;
    }

    s_error[0] = '\0';
    ESP_LOGI(TAG, "SSR PWM on GPIO%d, %lu Hz, %u-bit, inverted=%d",
             cfg->pwm_gpio, (unsigned long)cfg->pwm_freq_hz,
             (unsigned)cfg->pwm_res, cfg->pwm_inverted ? 1 : 0);
    return ESP_OK;
}

static esp_err_t pwm_set_power(float percent) {
    if (!s_pwm_ready) return ESP_ERR_INVALID_STATE;

    percent = clamp_pct(percent);
    uint32_t duty = regulator_power_to_duty_bits(percent, s_cfg.pwm_res);
    esp_err_t err = pwm_write_duty(duty);
    if (err != ESP_OK) {
        set_error_err("ledc_set_duty", err);
        ESP_LOGE(TAG, "%s", s_error);
    }
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
        esp_err_t err = pwm_write_duty(0);
        if (err != ESP_OK)
            ESP_LOGE(TAG, "PWM emergency off failed: %s", esp_err_to_name(err));
    }

    if (s_ready && is_uart_type(s_cfg.type)) {
        const char *cmd = (s_cfg.type == REGULATOR_KVIC) ? "P000\n" : "P000\r\n";
        uart_write_bytes(s_cfg.uart_num, cmd, strlen(cmd));
    }
}

void regulator_deinit(void) {
    regulator_emergency_off();
    s_pwm_ready = false;
    uart_close();
    s_ready = false;
    s_power_set = 0.0f;
    memset(&s_cfg, 0, sizeof(s_cfg));
}

static bool config_valid(const regulator_config_t *cfg) {
    if (!cfg || cfg->type >= REGULATOR_COUNT) return false;

    if (cfg->type == REGULATOR_SSR_PWM) {
        if (cfg->pwm_gpio < 0) return false;
        if (!regulator_pwm_gpio_valid(cfg->pwm_gpio)) return false;
        if (!regulator_pwm_freq_valid(cfg->pwm_freq_hz, cfg->pwm_res))
            return false;
    }
    if (is_uart_type(cfg->type)) {
        if (cfg->tx_gpio < 0 || cfg->rx_gpio < 0) return false;
        if (!GPIO_IS_VALID_OUTPUT_GPIO((gpio_num_t)cfg->tx_gpio)) return false;
        if (!GPIO_IS_VALID_GPIO((gpio_num_t)cfg->rx_gpio)) return false;
        if (cfg->baud < 1200 || cfg->baud > 115200) return false;
    }
    return true;
}

esp_err_t regulator_init(const regulator_config_t *cfg) {
    if (!cfg) return ESP_ERR_INVALID_ARG;

    regulator_emergency_off();
    regulator_deinit();
    s_cfg = *cfg;

    if (s_cfg.type == REGULATOR_NONE) {
        s_ready = false;
        s_error[0] = '\0';
        return ESP_OK;
    }

    if (!config_valid(cfg)) {
        set_error("regulator configuration is invalid");
        ESP_LOGE(TAG, "%s (type=%d)", s_error, (int)cfg->type);
        s_ready = false;
        return ESP_ERR_INVALID_ARG;
    }

    if (s_cfg.type == REGULATOR_SSR_PWM) {
        esp_err_t err = pwm_init(cfg);
        s_ready = (err == ESP_OK);
        if (!s_ready) regulator_emergency_off();
        return err;
    }

    if (!is_uart_type(s_cfg.type)) {
        set_error("unsupported regulator type");
        return ESP_ERR_NOT_SUPPORTED;
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
    if (err != ESP_OK) {
        set_error_err("uart_param_config", err);
        return err;
    }

    err = uart_set_pin(cfg->uart_num, cfg->tx_gpio, cfg->rx_gpio,
                       UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        set_error_err("uart_set_pin", err);
        return err;
    }

    err = uart_driver_install(cfg->uart_num, 256, 0, 0, NULL, 0);
    if (err != ESP_OK) {
        set_error_err("uart_driver_install", err);
        return err;
    }

    s_ready = true;
    s_error[0] = '\0';
    regulator_emergency_off();
    ESP_LOGI(TAG, "UART%d %d baud, type %s", (int)cfg->uart_num, cfg->baud,
             control_config_regulator_name(cfg->type));
    return ESP_OK;
}

static regulator_config_t config_from_settings(const regulator_settings_t *s) {
    regulator_config_t cfg = {
        .type = s->type,
        .uart_num = REG_UART_NUM,
        .tx_gpio = REG_UART_TX_GPIO,
        .rx_gpio = REG_UART_RX_GPIO,
        .baud = (int)s->uart_baud,
        .pwm_gpio = s->pwm_gpio,
        .pwm_freq_hz = s->pwm_freq_hz,
        .pwm_res = s->pwm_resolution,
        .pwm_inverted = s->pwm_inverted,
    };
    return cfg;
}

esp_err_t regulator_init_from_config(void) {
    regulator_settings_t settings;
    esp_err_t err = control_config_get_regulator_settings(&settings);
    if (err != ESP_OK) {
        set_error("control_config is not ready");
        return err;
    }
    regulator_config_t cfg = config_from_settings(&settings);
    return regulator_init(&cfg);
}

esp_err_t regulator_apply_settings(const regulator_settings_t *settings) {
    if (!settings) {
        set_error("settings is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (settings->type >= REGULATOR_COUNT) {
        set_error("unknown regulator type");
        return ESP_ERR_INVALID_ARG;
    }
    if (settings->pwm_gpio < -1 || settings->pwm_gpio > REG_PWM_MAX_GPIO) {
        set_error("PWM GPIO out of ESP32-S3 range");
        return ESP_ERR_INVALID_ARG;
    }
    if (settings->pwm_gpio >= 0 &&
        !regulator_pwm_gpio_valid(settings->pwm_gpio)) {
        set_error("PWM GPIO is reserved/occupied/invalid");
        return ESP_ERR_INVALID_ARG;
    }
    if (!regulator_pwm_freq_valid(settings->pwm_freq_hz,
                                  settings->pwm_resolution)) {
        set_error("PWM frequency/resolution is invalid");
        return ESP_ERR_INVALID_ARG;
    }
    if (settings->uart_baud < 1200 || settings->uart_baud > 115200) {
        set_error("UART baud is invalid");
        return ESP_ERR_INVALID_ARG;
    }

    regulator_settings_t old;
    if (control_config_get_regulator_settings(&old) != ESP_OK)
        return ESP_ERR_INVALID_STATE;

    // Сначала гарантированно снимаем мощность со старого выхода.
    ctrl_heater_force_off();
    regulator_deinit();

    regulator_config_t cfg = config_from_settings(settings);
    esp_err_t err = regulator_init(&cfg);
    if (err != ESP_OK) {
        char fail[96];
        snprintf(fail, sizeof(fail), "%s", s_error);
        regulator_settings_t rollback = old;
        regulator_config_t old_cfg = config_from_settings(&rollback);
        esp_err_t rb = regulator_init(&old_cfg);
        if (rb != ESP_OK) {
            regulator_deinit();
            set_error("new regulator failed; rollback also failed");
        } else {
            snprintf(s_error, sizeof(s_error), "%s", fail);
        }
        ESP_LOGE(TAG, "apply settings failed: %s; rollback %s",
                 esp_err_to_name(err), rb == ESP_OK ? "ok" : "failed");
        return err;
    }

    // NVS сохраняем только после успешной инициализации.
    err = control_config_set_regulator_settings(settings);
    if (err != ESP_OK) {
        regulator_deinit();
        regulator_config_t old_cfg = config_from_settings(&old);
        (void)regulator_init(&old_cfg);
        set_error_err("save regulator settings", err);
        return err;
    }

    s_error[0] = '\0';
    ESP_LOGI(TAG, "applied %s", control_config_regulator_name(settings->type));
    return ESP_OK;
}

esp_err_t regulator_select(regulator_type_t type) {
    if (type >= REGULATOR_COUNT) return ESP_ERR_INVALID_ARG;

    regulator_settings_t settings;
    esp_err_t err = control_config_get_regulator_settings(&settings);
    if (err != ESP_OK) return err;
    settings.type = type;
    return regulator_apply_settings(&settings);
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
        set_error_err("regulator_set_power", err);
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
    if (s_cfg.type == REGULATOR_SSR_PWM) return s_cfg.pwm_gpio;
    return s_cfg.tx_gpio;
}

int regulator_get_pwm_freq(void) {
    return (s_cfg.type == REGULATOR_SSR_PWM) ? (int)s_cfg.pwm_freq_hz : 0;
}

int regulator_get_pwm_resolution(void) {
    return (s_cfg.type == REGULATOR_SSR_PWM) ? (int)s_cfg.pwm_res : 0;
}

bool regulator_get_pwm_inverted(void) {
    return (s_cfg.type == REGULATOR_SSR_PWM) ? s_cfg.pwm_inverted : false;
}

const char *regulator_last_error(void) { return s_error; }
