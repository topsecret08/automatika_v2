#include "ctrl_pid.h"
#include "adaptive_pid.h"
#include "ctrl_heater.h"
#include "ctrl_safety.h"
#include "safety_gate.h"
#include "verify/verify_config.h"
#include "verify/shadow.h"
#include "autopid_c.h"
#include "config.h"
#include "core/control_config.h"
#include "core/events.h"
#include "hal/hal_1wire.h"
#include "esp_timer.h"

#include <math.h>
#include <string.h>

// Класс AutoPID (C++) приходит через autopid_glue.cpp — прикладной код
// остаётся на C. Если компонент недоступен, используется тот же закон
// в C, чтобы прошивка собиралась и без регистра.

typedef struct {
    uintptr_t h;              // 0 — C++ AutoPID недоступен

    double kp, ki, kd;
    double sp, in, out;
    double out_min, out_max;
    double sample_ms;
    int64_t last_us;
    int    channel;

    // резервный C-PID
    double integral, prev_err;

    // режим BANG-BANG
    bool   bang_on;

    // автотюнинг (релейный)
    bool  at_active;
    float at_hyst;
    float at_output_hi, at_output_lo;
    float at_result_kp, at_result_ki, at_result_kd;
    float at_peak_hi, at_peak_lo;
    float at_last_peaks[4];
    int   at_peak_count;
    bool  at_done;
    float at_prev_sign;
} pid_t;

static pid_t p;
static adaptive_pid_t s_apid;
static bool s_apid_ready;

static void ensure_handle(void) {
    if (p.h != 0) return;
    if (!autopid_available()) return;
    p.h = autopid_create(&p.in, &p.sp, &p.out,
                         0.0, PID_OUTPUT_MAX,
                         p.kp, p.ki, p.kd,
                         (unsigned long)p.sample_ms);
    if (p.h != 0) {
        events_log(EVENT_INFO, "PID: AutoPID created (Kp=%.2f Ki=%.3f Kd=%.2f)",
                   p.kp, p.ki, p.kd);
    }
}

void ctrl_pid_init_ext(float kp, float ki, float kd, float sp,
                       float sample_ms, int temp_channel) {
    if (p.h) autopid_destroy(p.h);
    memset(&p, 0, sizeof(p));

    p.kp = (kp > 0.0f) ? (double)kp : PID_KP_DEFAULT;
    p.ki = (ki > 0.0f) ? (double)ki : PID_KI_DEFAULT;
    p.kd = (kd > 0.0f) ? (double)kd : PID_KD_DEFAULT;
    p.sp = sp;
    p.out_min = 0.0;
    p.out_max = PID_OUTPUT_MAX;
    p.sample_ms = (sample_ms > 0.0f) ? (double)sample_ms : (double)PID_TIMESTEP_MS;
    p.last_us = esp_timer_get_time();
    p.channel = temp_channel;
    p.at_hyst = 1.0f;
    p.at_output_hi = 100.0f;
    p.at_output_lo = 0.0f;
    p.bang_on = false;
    p.at_peak_hi = -1000.0f;
    p.at_peak_lo = 1000.0f;

    if (adaptive_pid_init(&s_apid, (float)p.kp, (float)p.ki, (float)p.kd,
                          0.0f, (float)p.out_max) == ESP_OK) {
        s_apid_ready = true;
    }

    ensure_handle();
}

void ctrl_pid_init(float kp, float ki, float kd, float sp) {
    ctrl_pid_init_ext(kp, ki, kd, sp, (float)PID_TIMESTEP_MS, 0);
}

void ctrl_pid_set_tunings(float kp, float ki, float kd) {
    if (kp > 0.0f) p.kp = kp;
    if (ki > 0.0f) p.ki = ki;
    if (kd > 0.0f) p.kd = kd;
    if (p.h) autopid_set_gains(p.h, p.kp, p.ki, p.kd);
}

void ctrl_pid_set_setpoint(float sp) {
    /* Упреждающая коррекция уставки: при росте температуры уставка
     * сдвигается вниз пропорционально тренду (PID_PREDEC_GAIN, °C на
     * 1 °C/мин). Это не заменяет трендовый ограничитель нагрева, а
     * работает вместе с ним. */
    float eff = sp;
    float slope = hal_1wire_slope(p.channel);
    if ((slope > (float)TREND_THRESHOLD || hal_1wire_rising(p.channel)) &&
        PID_PREDEC_GAIN > 0.0f) {
        float corr = slope * PID_PREDEC_GAIN * 0.01f;
        if (corr > 0.0f) eff = sp - corr;
    }

    p.sp = eff;
    if (p.h) autopid_set_setpoint(p.h, eff);
}

void ctrl_pid_set_limits(float lo, float hi) {
    p.out_min = lo;
    p.out_max = hi;
    if (p.h) autopid_set_output_range(p.h, lo, hi);
}

void ctrl_pid_set_sample_time(float ms) {
    p.sample_ms = ms;
    if (p.h) autopid_set_timestep(p.h, (unsigned long)ms);
}

void ctrl_pid_set_channel(int temp_channel) { p.channel = temp_channel; }

// ------------------------------------------------------------------
// Автотюнинг (релейный, Ziegler–Nichols)
// ------------------------------------------------------------------
static float at_run(float in) {
    float out;
    if (in > (float)p.sp + p.at_hyst)      out = p.at_output_lo;
    else if (in < (float)p.sp - p.at_hyst) out = p.at_output_hi;
    else                                   out = (float)p.out_min;

    if (in > p.at_peak_hi) p.at_peak_hi = in;
    if (in < p.at_peak_lo) p.at_peak_lo = in;

    float sign = (in > (float)p.sp) ? 1.0f : -1.0f;
    if (p.at_prev_sign != 0.0f && sign != p.at_prev_sign) {
        if (p.at_peak_count < 4) {
            p.at_last_peaks[p.at_peak_count++] =
                (float)(esp_timer_get_time() / 1000);
        }
        if (p.at_peak_count >= 4) {
            float period = (p.at_last_peaks[3] - p.at_last_peaks[1]) / 2.0f / 1000.0f;
            float amp = fabsf(p.at_peak_hi - p.at_peak_lo) / 2.0f;
            if (amp > 0.01f && period > 0.01f) {
                float Ku = 4.0f * (p.at_output_hi - p.at_output_lo) /
                           (M_PI * amp * 2.0f);
                float Tu = period;
                float kp = 0.6f * Ku;
                float ki = kp / (0.5f * Tu);
                float kd = kp * 0.125f * Tu;
                p.at_result_kp = kp;
                p.at_result_ki = ki;
                p.at_result_kd = kd;
                ctrl_pid_set_tunings(kp, ki, kd);
                p.at_done = true;
                p.at_active = false;
                events_log(EVENT_INFO, "PID tuned: Kp=%.2f Ki=%.3f Kd=%.2f",
                           kp, ki, kd);
            }
        }
        p.at_prev_sign = sign;
    } else if (p.at_prev_sign == 0.0f) {
        p.at_prev_sign = sign;
    }

    return out;
}

// ------------------------------------------------------------------
// Главный расчёт
// ------------------------------------------------------------------
float ctrl_pid_compute(float in) {
    // Отказ датчика — не трогаем нагрев, просто нулевой выход.
    if (!isfinite(in) || in <= -50.0f) {
        p.out = 0.0;
        p.bang_on = false;
        return 0.0f;
    }

    p.in = in;

    if (control_config_get_mode() == CONTROL_MODE_PWM_BANG_BANG) {
        float hys = control_config_get_hys();
        if (p.bang_on) {
            if (in > (float)p.sp + hys) p.bang_on = false;
        } else {
            if (in < (float)p.sp - hys) p.bang_on = true;
        }
        p.out = p.bang_on ? p.out_max : 0.0;
        return (float)p.out;
    }

    if (p.at_active) {
        p.out = at_run(in);
        return (float)p.out;
    }

    ensure_handle();

    if (p.h) {
        autopid_run(p.h);         /* обновляет p.out через указатель */
        return (float)p.out;
    }

    // Резервный C-PID — та же семантика, что у AutoPID.
    int64_t now = esp_timer_get_time();
    if ((double)(now - p.last_us) < p.sample_ms * 1000.0)
        return (float)p.out;

    double dt = (double)(now - p.last_us) / 1e6;
    if (dt <= 0.0) dt = 0.001;
    p.last_us = now;

    double err = p.sp - p.in;
    p.integral += (err + p.prev_err) / 2.0 * dt;
    p.prev_err = err;

    double out = p.kp * err + p.ki * p.integral;

    if (out > p.out_max) {
        out = p.out_max;
        if (p.ki > 0.0) p.integral -= err * dt;    /* anti-windup */
    }
    if (out < p.out_min) {
        out = p.out_min;
        if (p.ki > 0.0) p.integral -= err * dt;
    }

    p.out = out;
    return (float)out;
}

float ctrl_pid_compute_and_apply(float input) {
    return ctrl_pid_compute_and_apply_scaled(input, 1.0f);
}

float ctrl_pid_compute_and_apply_scaled(float input, float scale) {
    float legacy = ctrl_pid_compute(input) * scale;
    if (legacy < 0.0f) legacy = 0.0f;
    if (legacy > (float)p.out_max) legacy = (float)p.out_max;

    bool want_adaptive = s_apid_ready && adaptive_pid_is_enabled() &&
                         control_config_get_adaptive_pid();
    if (!want_adaptive) {
        (void)safety_gate_apply("heater_pid", legacy);
        return ctrl_heater_get_percent();
    }

    bool stage_ok = verify_is_stage_enabled(STAGE_PID_AUTOTUNE);
    bool allow = stage_ok &&
                 verify_get_shadow_policy() == SHADOW_OFF &&
                 verify_is_module_verified("adaptive_pid") &&
                 ctrl_safety_ok();

    float dt = (p.sample_ms > 0.0) ? (float)(p.sample_ms / 1000.0) : 1.0f;
    float out = adaptive_pid_step(&s_apid, (float)p.sp, input, dt, allow) * scale;
    if (out < 0.0f) out = 0.0f;
    if (out > (float)p.out_max) out = (float)p.out_max;

    shadow_log_adaptive("adaptive_pid", (float)p.sp, input, legacy, out,
                        s_apid.shadow_kp, s_apid.shadow_ki, s_apid.shadow_kd);

    if (allow) {
        (void)safety_gate_apply("adaptive_pid", out);
    } else {
        // Тень/неверифицированный режим: нагрев ведёт обычный PID.
        (void)safety_gate_apply("heater_pid", legacy);
    }
    return ctrl_heater_get_percent();
}

void ctrl_pid_autotune_start(void) {
    p.at_active = true;
    p.at_peak_count = 0;
    p.at_done = false;
    p.at_peak_hi = -1000.0f;
    p.at_peak_lo = 1000.0f;
    p.at_prev_sign = 0.0f;
    events_log(EVENT_INFO, "PID autotune started");
}

void ctrl_pid_autotune_stop(void) {
    if (!p.at_active) return;
    p.at_active = false;
    events_log(EVENT_WARNING, "PID autotune stopped");
}

bool ctrl_pid_autotune_done(void) { return p.at_done; }

void ctrl_pid_autotune_get(float *kp, float *ki, float *kd) {
    if (kp) *kp = p.at_result_kp;
    if (ki) *ki = p.at_result_ki;
    if (kd) *kd = p.at_result_kd;
}

bool ctrl_pid_uses_autopid(void) { return p.h != 0; }

float ctrl_pid_get_integral(void) {
    if (p.h) return (float)autopid_get_integral(p.h);
    return (float)p.integral;
}
