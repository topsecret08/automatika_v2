#include "adaptive_pid.h"

#include "safety_gate.h"
#include "verify/pid_metrics.h"
#include "verify/verify_config.h"
#include "verify/verify_db.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "ADAPTIVE_PID";

#define ADAPT_NS        "adaptive"
#define ADAPT_KEY       "verified"
#define HIST_SIZE       32
#define OSC_WINDOW      60
#define DEGRADE_LIMIT   60

static adaptive_pid_t *s_global;
static SemaphoreHandle_t s_mtx;
static TaskHandle_t s_worker;
static bool s_nvs_ready;

static void lock(void) {
    if (s_mtx) xSemaphoreTake(s_mtx, portMAX_DELAY);
}
static void unlock(void) {
    if (s_mtx) xSemaphoreGive(s_mtx);
}

bool adaptive_pid_is_enabled(void) {
#if defined(CONFIG_ADAPTIVE_PID_ENABLE) && !CONFIG_ADAPTIVE_PID_ENABLE
    return false;
#else
    return true;
#endif
}

const char *adaptive_pid_phase_name(adaptive_phase_t phase) {
    switch (phase) {
    case PHASE_HEAT_UP:     return "heat_up";
    case PHASE_SETTLING:    return "settling";
    case PHASE_STEADY:      return "steady";
    case PHASE_DISTURBANCE: return "disturbance";
    default:                return "?";
    }
}

static adaptive_phase_t detect_phase(float err, float d_err) {
    float ae = fabsf(err);
    float ade = fabsf(d_err);
    if (ade > 1.5f && ae < 3.0f) return PHASE_DISTURBANCE;
    if (ae > 5.0f) return PHASE_HEAT_UP;
    if (ae > 0.8f) return PHASE_SETTLING;
    return PHASE_STEADY;
}

static void set_phase_defaults(adaptive_pid_t *p) {
    p->phase_kp[PHASE_HEAT_UP] = p->kp * 2.0f;
    p->phase_ki[PHASE_HEAT_UP] = p->ki * 0.4f;
    p->phase_kd[PHASE_HEAT_UP] = p->kd * 0.4f;

    p->phase_kp[PHASE_SETTLING] = p->kp;
    p->phase_ki[PHASE_SETTLING] = p->ki;
    p->phase_kd[PHASE_SETTLING] = p->kd;

    p->phase_kp[PHASE_STEADY] = p->kp * 0.5f;
    p->phase_ki[PHASE_STEADY] = p->ki * 2.0f;
    p->phase_kd[PHASE_STEADY] = p->kd * 1.6f;

    p->phase_kp[PHASE_DISTURBANCE] = p->kp * 1.25f;
    p->phase_ki[PHASE_DISTURBANCE] = p->ki;
    p->phase_kd[PHASE_DISTURBANCE] = p->kd;
}

static void rls_update(adaptive_pid_t *p, float y, float u_delayed) {
    int i1 = (p->hist_idx - 1 + HIST_SIZE) % HIST_SIZE;
    float phi[2] = { p->y_hist[i1], u_delayed };
    float y_pred = p->theta[0] * phi[0] + p->theta[1] * phi[1];
    float err = y - y_pred;

    float Pphi[2] = {
        p->P[0][0] * phi[0] + p->P[0][1] * phi[1],
        p->P[1][0] * phi[0] + p->P[1][1] * phi[1]
    };
    float denom = p->forgetting + phi[0] * Pphi[0] + phi[1] * Pphi[1];
    if (fabsf(denom) < 1e-6f) return;
    float K[2] = { Pphi[0] / denom, Pphi[1] / denom };

    p->theta[0] += K[0] * err;
    p->theta[1] += K[1] * err;

    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            p->P[i][j] = (p->P[i][j] -
                K[i] * (phi[0] * p->P[0][j] + phi[1] * p->P[1][j]))
                / p->forgetting;
        }
    }

    if (p->theta[0] > 0.999f) p->theta[0] = 0.999f;
    if (p->theta[0] < 0.5f) p->theta[0] = 0.5f;
    if (p->theta[1] > 0.1f) p->theta[1] = 0.1f;
    if (p->theta[1] < 1e-5f) p->theta[1] = 1e-5f;
}

static void recompute_gains_from_model(adaptive_pid_t *p, float dt) {
    float a = p->theta[0];
    float b = p->theta[1];
    if (b < 1e-6f || a < 0.01f || a > 0.999f || dt <= 0.0f) return;

    float tau = -dt / logf(a);
    if (tau < 5.0f) tau = 5.0f;
    if (tau > 600.0f) tau = 600.0f;

    float Kg = b / (1.0f - a);
    if (Kg < 1e-4f) Kg = 1e-4f;

    float delay = (float)p->est_delay * dt;
    if (delay < dt) delay = dt;

    float lambda = 0.5f * tau;
    if (lambda < 3.0f * delay) lambda = 3.0f * delay;

    float kp_new = tau / (Kg * (lambda + delay));
    float ti_new = tau;
    float td_new = 0.5f * delay;

    // Не более 25 % шага за пересчёт.
    p->pending_kp = p->kp + (kp_new - p->kp) * 0.25f;
    p->pending_ki = p->ki + ((kp_new / ti_new) - p->ki) * 0.25f;
    p->pending_kd = p->kd + ((kp_new * td_new) - p->kd) * 0.25f;
    p->pending_gains = true;

    ESP_LOGD(TAG, "a=%.4f b=%.5f tau=%.1f K=%.4f -> kp=%.3f",
             a, b, tau, Kg, p->pending_kp);
}

static void model_worker(void *arg) {
    (void)arg;
    TickType_t last = xTaskGetTickCount();

    while (1) {
        vTaskDelayUntil(&last, pdMS_TO_TICKS(1000));

        lock();
        adaptive_pid_t *p = s_global;
        if (p && p->hist_count > 8 &&
            verify_is_stage_enabled(STAGE_PREDICTIVE)) {
            p->est_counter++;
            if (p->est_counter >= p->est_period) {
                p->est_counter = 0;
                int ud = (p->hist_idx - 1 - p->est_delay + HIST_SIZE * 2)
                         % HIST_SIZE;
                rls_update(p, p->y_hist[(p->hist_idx - 1 + HIST_SIZE) % HIST_SIZE],
                           p->u_hist[ud]);
                recompute_gains_from_model(p, p->last_dt > 0.0f
                                               ? p->last_dt : 1.0f);
            }
        }
        unlock();
    }
}

esp_err_t adaptive_pid_init(adaptive_pid_t *p, float kp, float ki, float kd,
                            float out_min, float out_max) {
    if (!p) return ESP_ERR_INVALID_ARG;
    if (!adaptive_pid_is_enabled()) return ESP_ERR_NOT_SUPPORTED;

    if (!s_mtx) s_mtx = xSemaphoreCreateMutex();
    if (!s_mtx) return ESP_ERR_NO_MEM;

    memset(p, 0, sizeof(*p));
    p->kp = kp;
    p->ki = ki;
    p->kd = kd;
    p->out_min = out_min;
    p->out_max = out_max;
    p->forgetting = 0.97f;
    p->est_period = 30;
    p->est_delay = 1;
    p->P[0][0] = 1000.0f;
    p->P[1][1] = 1000.0f;
    p->theta[0] = 0.98f;
    p->theta[1] = 0.01f;
    p->last_phase = PHASE_HEAT_UP;
    p->last_meas = NAN;
    p->last_dt = 1.0f;
    set_phase_defaults(p);

    nvs_handle_t h;
    if (nvs_open(ADAPT_NS, NVS_READWRITE, &h) == ESP_OK) {
        s_nvs_ready = true;
        float gains[3];
        size_t len = sizeof(gains);
        if (nvs_get_blob(h, ADAPT_KEY, gains, &len) == ESP_OK &&
            len == sizeof(gains)) {
            p->verified_kp = gains[0];
            p->verified_ki = gains[1];
            p->verified_kd = gains[2];
            p->verified_valid = true;
        }
        nvs_close(h);
    }

    s_global = p;
    if (!s_worker) {
        BaseType_t ok = xTaskCreatePinnedToCore(model_worker, "ad_pid_model",
                                                4096, NULL, 2, &s_worker, 0);
        if (ok != pdPASS) {
            s_worker = NULL;
            return ESP_ERR_NO_MEM;
        }
    }
    ESP_LOGI(TAG, "init kp=%.2f ki=%.3f kd=%.2f verified=%d",
             p->kp, p->ki, p->kd, p->verified_valid);
    return ESP_OK;
}

static void apply_pending_gains(adaptive_pid_t *p) {
    if (!p->pending_gains) return;
    p->kp = p->pending_kp;
    p->ki = p->pending_ki;
    p->kd = p->pending_kd;
    p->pending_gains = false;
    p->bumpless_pending = true;
}

static void record_metric(adaptive_pid_t *p, float sp, float y) {
    if (!p->metric_active) return;
    if (p->metric_n < (int)(sizeof(p->metric_sp) / sizeof(p->metric_sp[0]))) {
        p->metric_sp[p->metric_n] = sp;
        p->metric_y[p->metric_n] = y;
        p->metric_n++;
    }
}

static bool quality_degraded(adaptive_pid_t *p, float err) {
    p->sse_slow = 0.99f * p->sse_slow + 0.01f * fabsf(err);

    // Индекс колебаний: доля смен знака ошибки в окне.
    p->err_hist[p->err_idx] = err;
    p->err_idx = (p->err_idx + 1) % OSC_WINDOW;
    if (p->err_count < OSC_WINDOW) p->err_count++;
    int changes = 0;
    int prev = 0;
    for (int i = 1; i < p->err_count; i++) {
        int idx = (p->err_idx - p->err_count + i + OSC_WINDOW * 2) % OSC_WINDOW;
        int idx_prev = (idx - 1 + OSC_WINDOW) % OSC_WINDOW;
        int sg = (p->err_hist[idx] > p->err_hist[idx_prev]) ? 1 :
                 ((p->err_hist[idx] < p->err_hist[idx_prev]) ? -1 : 0);
        if (sg && prev && sg != prev) changes++;
        if (sg) prev = sg;
    }
    float inst = (float)changes;   // смен знака в окне
    p->osc_index_slow = 0.99f * p->osc_index_slow + 0.01f * inst;

    if (p->sse_slow > 1.0f && p->osc_index_slow > 1.5f) p->bad_cycles++;
    else p->bad_cycles = 0;
    return p->bad_cycles > DEGRADE_LIMIT;
}

float adaptive_pid_step(adaptive_pid_t *p, float sp, float meas,
                        float dt, bool allow_apply) {
    if (!p || !adaptive_pid_is_enabled()) return 0.0f;
    if (!isfinite(sp) || !isfinite(meas) || dt <= 0.0f) return p->last_out;
    p->last_dt = dt;

    lock();
    apply_pending_gains(p);
    unlock();

    float err = sp - meas;
    float d_err = isfinite(p->last_meas) ? (meas - p->last_meas) / dt : 0.0f;
    p->last_meas = meas;

    // L1: фаза и bumpless.
    adaptive_phase_t ph = detect_phase(err, d_err);
    if (ph != p->last_phase) {
        p->phase = ph;
        p->kp = p->phase_kp[ph];
        p->ki = p->phase_ki[ph];
        p->kd = p->phase_kd[ph];
        p->bumpless_pending = true;
        p->last_phase = ph;
    } else {
        p->phase = ph;
    }

    // История для RLS (читается задачей ядра 0).
    lock();
    int idx = p->hist_idx % HIST_SIZE;
    p->y_hist[idx] = meas;
    p->u_hist[idx] = p->last_out;
    p->hist_idx++;
    if (p->hist_count < HIST_SIZE) p->hist_count++;
    unlock();

    // PID.
    p->integral += p->ki * err * dt;
    if (p->integral > p->out_max) p->integral = p->out_max;
    if (p->integral < p->out_min) p->integral = p->out_min;

    float deriv = -p->kd * d_err;
    float out = p->kp * err + p->integral + deriv;
    if (out > p->out_max) out = p->out_max;
    if (out < p->out_min) out = p->out_min;

    if (p->bumpless_pending) {
        p->integral += p->last_out - out;
        if (p->integral > p->out_max) p->integral = p->out_max;
        if (p->integral < p->out_min) p->integral = p->out_min;
        out = p->kp * err + p->integral + deriv;
        if (out > p->out_max) out = p->out_max;
        if (out < p->out_min) out = p->out_min;
        p->bumpless_pending = false;
    }

    // L3: деградация и откат.
    if (quality_degraded(p, err) && allow_apply && p->verified_valid) {
        ESP_LOGW(TAG, "degraded: sse=%.2f osc=%.2f, rollback",
                 p->sse_slow, p->osc_index_slow);
        p->kp = p->verified_kp;
        p->ki = p->verified_ki;
        p->kd = p->verified_kd;
        p->integral = p->last_out - p->kp * err - deriv;
        if (p->integral > p->out_max) p->integral = p->out_max;
        if (p->integral < p->out_min) p->integral = p->out_min;
        out = p->kp * err + p->integral + deriv;
        p->bad_cycles = 0;
    }

    p->last_out = out;
    if (allow_apply) p->metric_was_applied = true;
    p->shadow_kp = p->kp;
    p->shadow_ki = p->ki;
    p->shadow_kd = p->kd;
    p->shadow_out = out;
    record_metric(p, sp, meas);
    return out;
}

void adaptive_pid_freeze_current(adaptive_pid_t *p) {
    if (!p) return;
    p->verified_kp = p->kp;
    p->verified_ki = p->ki;
    p->verified_kd = p->kd;
    p->verified_valid = true;

    if (s_nvs_ready) {
        nvs_handle_t h;
        if (nvs_open(ADAPT_NS, NVS_READWRITE, &h) == ESP_OK) {
            float gains[3] = { p->verified_kp, p->verified_ki, p->verified_kd };
            nvs_set_blob(h, ADAPT_KEY, gains, sizeof(gains));
            nvs_commit(h);
            nvs_close(h);
        }
    }
    ESP_LOGI(TAG, "frozen: kp=%.3f ki=%.4f kd=%.3f",
             p->verified_kp, p->verified_ki, p->verified_kd);
}

void adaptive_pid_run_reset(adaptive_pid_t *p) {
    if (!p) return;
    p->metric_n = 0;
    p->metric_active = true;
    p->metric_was_applied = false;
}

bool adaptive_pid_run_finish(adaptive_pid_t *p, int64_t run_id) {
    if (!p || p->metric_n < 2) return false;

    pid_metrics_t m;
    float sp_min = p->metric_sp[0], sp_max = p->metric_sp[0];
    for (int i = 1; i < p->metric_n; i++) {
        if (p->metric_sp[i] < sp_min) sp_min = p->metric_sp[i];
        if (p->metric_sp[i] > sp_max) sp_max = p->metric_sp[i];
    }
    float step = sp_max - sp_min;
    esp_err_t err = pid_compute_metrics(p->metric_sp, p->metric_y,
                                        p->metric_n, 1.0f, step, &m);
    bool ok = (err == ESP_OK);

    char json[192];
    snprintf(json, sizeof(json),
             "{\"overshoot\":%.2f,\"settling\":%.1f,\"sse\":%.2f,"
             "\"osc\":%.3f,\"ok\":%d}",
             (double)m.overshoot_pct, (double)m.settling_s,
             (double)m.sse, (double)m.osc_index, ok ? 1 : 0);
    if (run_id > 0) verify_db_finish_run(run_id, json);
    if (ok && p->metric_was_applied) {
        adaptive_pid_freeze_current(p);
        verify_report_clean_run();
    }
    p->metric_active = false;
    return ok;
}

adaptive_pid_t *adaptive_pid_global(void) { return s_global; }
