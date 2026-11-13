#include "sensor_pipeline.h"

#include "esp_timer.h"

#include <math.h>

#define PIPELINE_WINDOW      5
#define PRESSURE_REF_HPA     1013.25f
#define PRESSURE_COEFF_C_HPA 0.035f
#define TREND_MAX_GAP_S      10.0f

static float median5(const float *b, int n) {
    if (n <= 0) return NAN;
    float tmp[PIPELINE_WINDOW];
    for (int i = 0; i < n; i++) tmp[i] = b[i];

    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            if (tmp[j] < tmp[i]) {
                float t = tmp[i];
                tmp[i] = tmp[j];
                tmp[j] = t;
            }
        }
    }
    return tmp[n / 2];
}

void temp_pipeline_init(TempPipeline *p, float q, float r) {
    if (!p) return;
    p->idx = 0;
    p->cnt = 0;
    p->x_est = 20.0f;
    p->p_est = 1.0f;
    p->q = (q > 0.0f) ? q : 0.01f;
    p->r = (r > 0.0f) ? r : 0.1f;
    p->trend = 0.0f;
    p->last_x = p->x_est;
    p->last_t = 0;
    p->t_corrected = 0.0f;
    p->raw = NAN;
    p->valid = false;
}

static bool raw_valid(float raw) {
    if (!isfinite(raw)) return false;
    if (raw <= -100.0f || raw >= 125.0f) return false;
    if (raw == -127.0f || raw == 85.0f) return false;
    return true;
}

float temp_pipeline_compensate(const TempPipeline *p, float pressure_hpa) {
    if (!p || !p->valid) return NAN;
    if (!isfinite(pressure_hpa) || pressure_hpa < 300.0f ||
        pressure_hpa > 1200.0f) {
        pressure_hpa = PRESSURE_REF_HPA;
    }
    return p->x_est + (pressure_hpa - PRESSURE_REF_HPA) * PRESSURE_COEFF_C_HPA;
}

float temp_pipeline_update(TempPipeline *p, float raw, float pressure_hpa) {
    if (!p) return NAN;

    // 1. Валидация.
    if (!raw_valid(raw)) {
        p->valid = false;
        p->raw = raw;
        return NAN;
    }

    p->raw = raw;
    p->valid = true;

    // 2. Медиана по окну 5.
    p->buf[p->idx] = raw;
    p->idx = (p->idx + 1) % PIPELINE_WINDOW;
    if (p->cnt < PIPELINE_WINDOW) p->cnt++;
    float med = median5(p->buf, p->cnt);

    // 3. Калман.
    float x_pred = p->x_est;
    float p_pred = p->p_est + p->q;
    float k = p_pred / (p_pred + p->r);
    p->x_est = x_pred + k * (med - x_pred);
    p->p_est = (1.0f - k) * p_pred;

    // Тренд по разности оценок. Длинные паузы сбрасывают тренд, иначе
    // редкий отсчёт даёт выброс производной.
    int64_t now = esp_timer_get_time();
    if (p->last_t > 0 && now > p->last_t) {
        float dt_s = (float)(now - p->last_t) / 1e6f;
        if (dt_s > 0.001f && dt_s <= TREND_MAX_GAP_S) {
            p->trend = (p->x_est - p->last_x) * 60.0f / dt_s;
        } else if (dt_s > TREND_MAX_GAP_S) {
            p->trend = 0.0f;
        }
    }
    p->last_x = p->x_est;
    p->last_t = now;

    // 4. Компенсация давления. Ниже 1013.25 °C кипения ниже — поправка
    // отрицательная; выше — положительная.
    p->t_corrected = temp_pipeline_compensate(p, pressure_hpa);
    return p->t_corrected;
}

float temp_pipeline_get_trend(const TempPipeline *p) {
    return p ? p->trend : 0.0f;
}

float temp_pipeline_filtered(const TempPipeline *p) {
    return (p && p->valid) ? p->x_est : NAN;
}

bool temp_pipeline_is_stable(const TempPipeline *p, float threshold_c_per_min) {
    if (!p || !p->valid) return false;
    return fabsf(p->trend) < fabsf(threshold_c_per_min);
}

bool temp_pipeline_is_valid(const TempPipeline *p) {
    return p && p->valid;
}
