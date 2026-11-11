#include "ctrl_pid.h"
#include "esp_timer.h"
#include "core/events.h"
#include <math.h>
#include <string.h>

typedef struct {
    float kp, ki, kd;
    float sp, in;
    float out_min, out_max;
    float integral, prev_in;
    float sample_ms;
    int64_t last_us;

    bool  at_active;
    float at_hyst;
    float at_output_hi, at_output_lo;
    float at_result_kp, at_result_ki, at_result_kd;
    float at_peak_hi, at_peak_lo;
    float at_last_peaks[4];
    int   at_peak_count;
    int64_t at_phase_start;
    bool  at_done;
} pid_t;

static pid_t p;

void ctrl_pid_init(float kp, float ki, float kd, float sp) {
    memset(&p, 0, sizeof(p));
    p.kp = kp; p.ki = ki; p.kd = kd; p.sp = sp;
    p.out_min = 0; p.out_max = 100;
    p.sample_ms = 1000;
    p.at_output_hi = 100;
    p.at_output_lo = 0;
    p.at_hyst = 1.0f;
    p.last_us = esp_timer_get_time() / 1000;
}

void ctrl_pid_set_tunings(float kp, float ki, float kd) { p.kp = kp; p.ki = ki; p.kd = kd; }
void ctrl_pid_set_setpoint(float sp) { p.sp = sp; }
void ctrl_pid_set_limits(float lo, float hi) { p.out_min = lo; p.out_max = hi; }
void ctrl_pid_set_sample_time(float ms) { p.sample_ms = ms; }

static void at_run(float in) {
    if (!p.at_active) return;
    int64_t now = esp_timer_get_time() / 1000;

    float out;
    if (in > p.sp + p.at_hyst)       out = p.at_output_lo;
    else if (in < p.sp - p.at_hyst)  out = p.at_output_hi;
    else                             out = p.out_min;

    if (in > p.at_peak_hi) p.at_peak_hi = in;
    if (in < p.at_peak_lo || p.at_peak_lo == 0) p.at_peak_lo = in;

    static float prev_sign = 0;
    float sign = (in > p.sp) ? 1 : -1;
    if (sign != prev_sign && prev_sign != 0) {
        if (p.at_peak_count < 4) {
            p.at_last_peaks[p.at_peak_count++] = (float)now;
        }
        if (p.at_peak_count >= 4) {
            float period = (p.at_last_peaks[3] - p.at_last_peaks[1]) / 2.0f / 1000.0f;
            float amp = fabsf(p.at_peak_hi - p.at_peak_lo) / 2.0f;
            if (amp > 0.01f && period > 0.01f) {
                float Ku = 4.0f * (p.at_output_hi - p.at_output_lo) /
                           (M_PI * amp * 2.0f);
                float Tu = period;
                p.at_result_kp = 0.6f * Ku;
                p.at_result_ki = p.at_result_kp / (0.5f * Tu);
                p.at_result_kd = p.at_result_kp * 0.125f * Tu;
                p.at_done = true;
                p.at_active = false;
                p.kp = p.at_result_kp;
                p.ki = p.at_result_ki;
                p.kd = p.at_result_kd;
                events_log(EVENT_INFO, "PID tuned: Kp=%.2f Ki=%.3f Kd=%.2f",
                           p.kp, p.ki, p.kd);
            }
        }
        prev_sign = sign;
    }

    p.in = out;
}

float ctrl_pid_compute(float in) {
    int64_t now = esp_timer_get_time() / 1000;
    if (now - p.last_us < (int64_t)p.sample_ms) return p.in;

    at_run(in);
    if (p.at_active) return p.in;

    float dt = (now - p.last_us) / 1000.0f;
    p.last_us = now;

    float err = p.sp - in;
    p.integral += p.ki * err * dt;
    if (p.integral > p.out_max) p.integral = p.out_max;
    if (p.integral < p.out_min) p.integral = p.out_min;

    float d = (in - p.prev_in) / dt;
    p.prev_in = in;

    float out = p.kp * err + p.integral - p.kd * d;
    if (out > p.out_max) out = p.out_max;
    if (out < p.out_min) out = p.out_min;

    p.in = out;
    return out;
}

void ctrl_pid_autotune_start(void) {
    p.at_active = true;
    p.at_peak_count = 0;
    p.at_done = false;
    p.at_phase_start = esp_timer_get_time() / 1000;
    p.at_peak_hi = 0;
    p.at_peak_lo = 0;
    events_log(EVENT_INFO, "PID autotune started");
}

bool ctrl_pid_autotune_done(void) { return p.at_done; }

void ctrl_pid_autotune_get(float *kp, float *ki, float *kd) {
    *kp = p.at_result_kp; *ki = p.at_result_ki; *kd = p.at_result_kd;
}