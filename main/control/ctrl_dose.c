#include "ctrl_dose.h"
#include "config.h"

#include "control/ctrl_safety.h"
#include "control/ctrl_valve_control.h"
#include "core/control_config.h"
#include "core/estop.h"
#include "core/events.h"
#include "core/state.h"
#include "core/watchdog.h"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "DOSER";

#define LEDC_TIMER_DOSE     LEDC_TIMER_1
#define LEDC_CH_DOSE        LEDC_CHANNEL_1
#define LEDC_RES_DOSE       LEDC_TIMER_10_BIT     /* 0..1023 ⇒ 0.1 % */

typedef enum { DOSE_CMD_VOLUME = 1, DOSE_CMD_STOP } dose_cmd_t;

typedef struct {
    dose_cmd_t cmd;
    float      volume_ml;
    float      rate_mlmin;
} dose_msg_t;

static QueueHandle_t s_queue = NULL;

static volatile bool  s_busy = false;
static volatile float s_rate = 0.0f;
static volatile float s_total_ml = 0.0f;

static bool s_gpio_ready = false;
static bool s_ledc_ready = false;

static float   s_proc_accum_ml = 0.0f;
static int     s_proc_valve = VALVE_BODY;
static bool    s_proc_open = false;

// ------------------------------------------------------------------

static bool guard_ok(void) {
    return ctrl_safety_ok() && !estop_is_pressed();
}

static uint32_t rate_to_step_us(float rate_mlmin, uint32_t steps_per_ml) {
    if (rate_mlmin <= 0.0f) rate_mlmin = control_config_get_rate_ml_min();
    if (rate_mlmin > CTRL_DOSE_MAX_RATE_MLMIN) rate_mlmin = CTRL_DOSE_MAX_RATE_MLMIN;

    float steps_per_s = (rate_mlmin / 60.0f) * (float)steps_per_ml;
    if (steps_per_s < 0.1f) steps_per_s = 0.1f;

    float us = 1000000.0f / steps_per_s;
    if (us < (float)PUMP_MIN_STEP_US) us = (float)PUMP_MIN_STEP_US;
    if (us > 1000000.0f) us = 1000000.0f;
    return (uint32_t)us;
}

static void pump_enable(bool on) {
    if (!s_gpio_ready) return;
    gpio_set_level((gpio_num_t)PUMP_STEP_GPIO, 0);
    gpio_set_level((gpio_num_t)PUMP_EN_GPIO, on ? 0 : 1);   /* активен низким */
}

// Выдача шагов. Не блокирует httpd: всегда из задачи «doser».
static esp_err_t pump_run_steps(uint32_t total, uint32_t step_us) {
    if (total == 0) return ESP_OK;
    if (!s_gpio_ready) return ESP_ERR_INVALID_STATE;

    pump_enable(true);
    const gpio_num_t step = (gpio_num_t)PUMP_STEP_GPIO;

    esp_err_t err = ESP_OK;
    uint32_t guard = 0;

    for (uint32_t i = 0; i < total; i++) {
        gpio_set_level(step, 1);
        esp_rom_delay_us(PUMP_STEP_US);
        gpio_set_level(step, 0);

        if ((++guard & 0x1F) == 0) {
            WD_FEED("doser");
            if (!guard_ok()) { err = ESP_ERR_INVALID_STATE; break; }
        }
        esp_rom_delay_us(step_us);
    }

    gpio_set_level(step, 0);
    pump_enable(false);
    return err;
}

// ------------------------------------------------------------------

esp_err_t ctrl_dose_enable_solenoid_pwm(int gpio) {
    if (gpio < 0) return ESP_ERR_INVALID_ARG;

    ledc_timer_config_t t = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_RES_DOSE,
        .timer_num = LEDC_TIMER_DOSE,
        .freq_hz = DOSER_SOLENOID_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    if (ledc_timer_config(&t) != ESP_OK) return ESP_FAIL;

    ledc_channel_config_t c = {
        .gpio_num = gpio,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CH_DOSE,
        .timer_sel = LEDC_TIMER_DOSE,
        .duty = 0,
        .hpoint = 0,
    };
    if (ledc_channel_config(&c) != ESP_OK) return ESP_FAIL;

    s_ledc_ready = true;
    ESP_LOGI(TAG, "solenoid PWM on GPIO%d @ %u Hz", gpio, DOSER_SOLENOID_HZ);
    return ESP_OK;
}

static void solenoid_duty(float permille) {
    if (!s_ledc_ready) return;
    if (permille < 0) permille = 0;
    if (permille > 1000.0f) permille = 1000.0f;
    uint32_t duty = ((uint32_t)permille * 1023u) / 1000u;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CH_DOSE, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CH_DOSE);
}

// ------------------------------------------------------------------

static void exec_volume(float volume_ml, float rate_mlmin) {
    if (volume_ml <= 0.0f) return;
    if (volume_ml > DOSER_MAX_DOSE_ML) volume_ml = DOSER_MAX_DOSE_ML;
    if (rate_mlmin <= 0.0f) rate_mlmin = control_config_get_rate_ml_min();
    if (rate_mlmin > CTRL_DOSE_MAX_RATE_MLMIN) rate_mlmin = CTRL_DOSE_MAX_RATE_MLMIN;

    output_device_t dev = control_config_get_output_device();
    s_busy = true;
    s_rate = rate_mlmin;

    esp_err_t err = ESP_OK;
    float done_ml = 0.0f;
    int64_t t0 = esp_timer_get_time();
    float secs = volume_ml / rate_mlmin * 60.0f;

    if (dev == OUTPUT_DEVICE_PUMP) {
        uint32_t spm = control_config_get_steps_per_ml();
        uint32_t total = (uint32_t)(volume_ml * (float)spm);
        err = pump_run_steps(total, rate_to_step_us(rate_mlmin, spm));
        done_ml = (total < 0xFFFFFFFFu) ? (float)total / (float)spm : volume_ml;
    } else if (s_ledc_ready) {
        float full = control_config_get_flow_ml_min();
        float frac = (full > 0.0f) ? rate_mlmin / full : 0.05f;
        if (frac > 0.8f) frac = 0.8f;
        if (frac < 0.005f) frac = 0.005f;
        solenoid_duty(frac * 1000.0f);
        while ((float)(esp_timer_get_time() - t0) < secs * 1e6f) {
            if (!guard_ok()) { err = ESP_ERR_INVALID_STATE; break; }
            WD_FEED("doser");
            vTaskDelay(pdMS_TO_TICKS(100));
            done_ml = rate_mlmin * (float)(esp_timer_get_time() - t0) / 6e7f;
        }
        solenoid_duty(0);
    } else {
        /* серво-клапан: доля открытия задаётся на всё время выдачи */
        float full = control_config_get_flow_ml_min();
        float frac = (full > 0.0f) ? rate_mlmin / full : 0.05f;
        if (frac > 0.9f) frac = 0.9f;
        if (frac < 0.02f) frac = 0.02f;
        valve_set_pct(VALVE_BODY, frac * 100.0f);
        while ((float)(esp_timer_get_time() - t0) < secs * 1e6f) {
            if (!guard_ok()) { err = ESP_ERR_INVALID_STATE; break; }
            WD_FEED("doser");
            vTaskDelay(pdMS_TO_TICKS(100));
            done_ml = rate_mlmin * (float)(esp_timer_get_time() - t0) / 6e7f;
        }
        valve_set_pct(VALVE_BODY, 0);
    }

    s_total_ml += done_ml;
    s_busy = false;
    s_rate = 0.0f;

    if (err == ESP_OK)
        events_log(EVENT_INFO, "Dose %.1f ml @ %.1f ml/min", done_ml, rate_mlmin);
    else
        events_log(EVENT_WARNING, "Dose aborted after %.1f ml (safety/estop)", done_ml);
}

// ------------------------------------------------------------------

esp_err_t ctrl_dose_init(void) {
    s_queue = xQueueCreate(CTRL_DOSE_QUEUE_LEN, sizeof(dose_msg_t));
    if (!s_queue) return ESP_ERR_NO_MEM;

    bool step_ok = PUMP_STEP_GPIO >= 0 &&
                   GPIO_IS_VALID_OUTPUT_GPIO((gpio_num_t)PUMP_STEP_GPIO);
    bool en_ok = PUMP_EN_GPIO >= 0 &&
                 GPIO_IS_VALID_OUTPUT_GPIO((gpio_num_t)PUMP_EN_GPIO);

    if (!step_ok || !en_ok) {
        s_gpio_ready = false;
        ESP_LOGW(TAG, "pump disabled: STEP=%d ENABLE=%d (set real free GPIOs)",
                 PUMP_STEP_GPIO, PUMP_EN_GPIO);
        events_log(EVENT_WARNING,
                   "Doser pump disabled: STEP/ENABLE GPIO not configured");
    } else {
        uint64_t mask = (1ULL << PUMP_STEP_GPIO) | (1ULL << PUMP_EN_GPIO);
        gpio_config_t io = {
            .pin_bit_mask = mask,
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        esp_err_t err = gpio_config(&io);
        if (err != ESP_OK) return err;
        s_gpio_ready = true;
        pump_enable(false);
    }

    if (SOLENOID_PWM_GPIO >= 0) {
        if (ctrl_dose_enable_solenoid_pwm(SOLENOID_PWM_GPIO) != ESP_OK)
            ESP_LOGW(TAG, "solenoid PWM init failed");
    }

    ESP_LOGI(TAG, "ready: pump=%s steps/ml=%u device=%s",
             s_gpio_ready ? "on" : "off",
             (unsigned)control_config_get_steps_per_ml(),
             control_config_get_output_device() == OUTPUT_DEVICE_PUMP ? "pump" : "valve");
    return ESP_OK;
}

void ctrl_dose_task(void *arg) {
    (void)arg;
    wd_register("doser", 30000, false);

    dose_msg_t msg;
    while (1) {
        if (xQueueReceive(s_queue, &msg, pdMS_TO_TICKS(500)) != pdTRUE) continue;
        WD_FEED("doser");

        if (msg.cmd == DOSE_CMD_STOP) {
            pump_enable(false);
            solenoid_duty(0);
            valve_set_pct(VALVE_BODY, 0);
            s_busy = false;
            continue;
        }
        if (!guard_ok()) {
            events_log(EVENT_WARNING, "Dose rejected: safety/estop");
            continue;
        }
        exec_volume(msg.volume_ml, msg.rate_mlmin);
    }
}

bool ctrl_dose_volume(float volume_ml, float rate_ml_per_min) {
    if (!s_queue) return false;
    if (volume_ml <= 0.0f) return false;
    if (!guard_ok()) {
        events_log(EVENT_WARNING, "Dose %.1f ml rejected: safety/estop", volume_ml);
        return false;
    }

    dose_msg_t msg = { .cmd = DOSE_CMD_VOLUME,
                       .volume_ml = volume_ml,
                       .rate_mlmin = rate_ml_per_min };
    if (xQueueSend(s_queue, &msg, pdMS_TO_TICKS(100)) != pdTRUE) {
        events_log(EVENT_WARNING, "Dose queue full (%.1f ml dropped)", volume_ml);
        return false;
    }
    return true;
}

bool ctrl_dose_set_rate(float rate_ml_per_min) {
    return control_config_set_rate_ml_min(rate_ml_per_min) == ESP_OK;
}

void ctrl_dose_stop(void) {
    if (!s_queue) return;
    dose_msg_t msg = { .cmd = DOSE_CMD_STOP };
    (void)xQueueSend(s_queue, &msg, pdMS_TO_TICKS(50));
    pump_enable(false);
    solenoid_duty(0);
    valve_set_pct(VALVE_BODY, 0);
}

bool  ctrl_dose_busy(void) { return s_busy; }
float ctrl_dose_rate(void) { return s_rate; }
float ctrl_dose_total_ml(void) { return s_total_ml; }

uint32_t ctrl_dose_steps_per_ml(void) { return control_config_get_steps_per_ml(); }

esp_err_t ctrl_dose_set_steps_per_ml(uint32_t steps) {
    return control_config_set_steps_per_ml(steps);
}

esp_err_t ctrl_dose_volume_blocking(float volume_ml, float rate_ml_per_min) {
    if (!guard_ok()) return ESP_ERR_INVALID_STATE;
    if (volume_ml <= 0.0f) return ESP_ERR_INVALID_ARG;

    uint32_t spm = control_config_get_steps_per_ml();
    uint32_t total = (uint32_t)(volume_ml * (float)spm);
    esp_err_t err = pump_run_steps(total, rate_to_step_us(rate_ml_per_min, spm));
    if (err == ESP_OK) s_total_ml += volume_ml;
    return err;
}

// ------------------------------------------------------------------
// Непрерывный отбор из режима процесса (mode_task, ядро 1).
// ------------------------------------------------------------------
void ctrl_dose_process(float rate_ml_min, float dt, int valve_idx) {
    if (dt <= 0.0f) return;
    if (rate_ml_min < 0.0f) rate_ml_min = 0.0f;

    s_proc_valve = valve_idx;

    output_device_t dev = control_config_get_output_device();

    if (!guard_ok() || rate_ml_min <= 0.01f) {
        if (s_proc_open) {
            s_proc_open = false;
            if (dev == OUTPUT_DEVICE_VALVE) valve_set_pct(valve_idx, 0);
            solenoid_duty(0);
            pump_enable(false);
        }
        return;
    }

    float ml = rate_ml_min * dt / 60.0f;

    if (dev == OUTPUT_DEVICE_PUMP) {
        /* копим доли мл и дозируем раз в шаг */
        s_proc_accum_ml += ml;
        if (s_proc_accum_ml >= 1.0f) {
            float chunk = s_proc_accum_ml;
            s_proc_accum_ml = 0.0f;
            if (!s_busy) ctrl_dose_volume(chunk, rate_ml_min);
        }
        s_total_ml += ml;
        return;
    }

    /* клапан: доля открытия = rate / калиброванный расход */
    float full = control_config_get_flow_ml_min();
    float frac = (full > 0.0f) ? rate_ml_min / full : 0.05f;
    if (frac > 1.0f) frac = 1.0f;
    if (frac < 0.01f) frac = 0.01f;

    if (s_ledc_ready) solenoid_duty(frac * 1000.0f);
    else valve_set_pct(valve_idx, frac * 100.0f);

    s_proc_open = true;
    s_total_ml += ml;
}
