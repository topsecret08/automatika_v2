#include "hal_lsa_pwm.h"
#include "config.h"
#include "core/estop.h"
#include "control/ctrl_safety.h"
#include "driver/ledc.h"
#include "esp_log.h"

static const char *TAG = "LSA_PWM";
static bool s_ready = false;
static float s_duty = 0.0f;

#define LSA_LEDC_TIMER LEDC_TIMER_2
#define LSA_LEDC_CH    LEDC_CHANNEL_2
#define LSA_LEDC_MODE  LEDC_LOW_SPEED_MODE

esp_err_t hal_lsa_pwm_init(void) {
    if (LSA_PWM_GPIO < 0) return ESP_OK;
    ledc_timer_config_t t = {
        .speed_mode = LSA_LEDC_MODE,
        .duty_resolution = (ledc_timer_bit_t)LSA_PWM_RES_BITS,
        .timer_num = LSA_LEDC_TIMER,
        .freq_hz = LSA_PWM_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    esp_err_t err = ledc_timer_config(&t);
    if (err != ESP_OK) return err;
    ledc_channel_config_t c = {
        .gpio_num = LSA_PWM_GPIO,
        .speed_mode = LSA_LEDC_MODE,
        .channel = LSA_LEDC_CH,
        .timer_sel = LSA_LEDC_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    err = ledc_channel_config(&c);
    if (err == ESP_OK) {
        s_ready = true;
        hal_lsa_pwm_force_off();
        ESP_LOGI(TAG, "ready on GPIO%d", LSA_PWM_GPIO);
    }
    return err;
}

esp_err_t hal_lsa_pwm_set_duty_pct(float pct) {
    if (!s_ready) return ESP_ERR_INVALID_STATE;
    if (estop_actions_blocked() || !ctrl_safety_ok()) {
        hal_lsa_pwm_force_off();
        return ESP_ERR_INVALID_STATE;
    }
    if (pct < 0.0f) pct = 0.0f;
    if (pct > 100.0f) pct = 100.0f;
    uint32_t max_duty = (1u << LSA_PWM_RES_BITS) - 1u;
    uint32_t duty = (uint32_t)(pct / 100.0f * (float)max_duty);
    esp_err_t err = ledc_set_duty(LSA_LEDC_MODE, LSA_LEDC_CH, duty);
    if (err == ESP_OK) err = ledc_update_duty(LSA_LEDC_MODE, LSA_LEDC_CH);
    if (err == ESP_OK) s_duty = pct;
    return err;
}

float hal_lsa_pwm_get_duty_pct(void) { return s_duty; }

void hal_lsa_pwm_force_off(void) {
    if (!s_ready) return;
    ledc_set_duty(LSA_LEDC_MODE, LSA_LEDC_CH, 0);
    ledc_update_duty(LSA_LEDC_MODE, LSA_LEDC_CH);
    s_duty = 0.0f;
}
