#include "alcohol_balance.h"
#include "config.h"

#include "core/state.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "nvs.h"

#include <math.h>
#include <string.h>

static const char *TAG = "ABV";

#define ABV_NVS_NAMESPACE "abv"
#define ABV_NVS_KEY       "cfg"
#define ABV_CFG_MAGIC     0x41425631u   // "ABV1"
#define ABV_CFG_VERSION   1

#define M_ETHANOL 46.068f
#define M_WATER   18.015f
#define RHO_WATER 1.000f

// Коэффициенты Ван-Лаара для этанол(1)-вода(2).
// Дают заметное положительное отклонение от Рауля, без которого
// температура кипения 40 % смеси получается завышенной на ~10 °C.
#define VANLAAR_A12 1.6798f
#define VANLAAR_A21 0.9227f

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t reserved;
    alcohol_init_t init;
} abv_nvs_t;

static alcohol_init_t s_init;
static float    s_ethanol_ml;
static float    s_water_ml;
static float    s_ethanol_start_ml;
static float    s_abv_now;
static float    s_boiling_c;
static int64_t  s_last_us;
static bool     s_ready;

static int64_t now_us(void) { return esp_timer_get_time(); }

static float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

void abv_defaults(alcohol_init_t *init) {
    if (!init) return;
    init->volume_start_ml = ABV_VOLUME_START_ML;
    init->abv_start_percent = ABV_START_PERCENT;
    init->ethanol_density = ABV_ETHANOL_DENSITY;
}

static bool init_valid(const alcohol_init_t *init) {
    if (!init) return false;
    if (!isfinite(init->volume_start_ml) || init->volume_start_ml <= 0.0f)
        return false;
    if (init->volume_start_ml > 200000.0f) return false;
    if (!isfinite(init->abv_start_percent) ||
        init->abv_start_percent < 0.0f || init->abv_start_percent > 100.0f)
        return false;
    if (!isfinite(init->ethanol_density) || init->ethanol_density <= 0.0f)
        return false;
    return true;
}

esp_err_t abv_config_load(alcohol_init_t *out) {
    if (!out) return ESP_ERR_INVALID_ARG;

    nvs_handle_t h;
    esp_err_t err = nvs_open(ABV_NVS_NAMESPACE, NVS_READONLY, &h);
    if (err != ESP_OK) {
        abv_defaults(out);
        return ESP_ERR_NOT_FOUND;
    }

    abv_nvs_t stored;
    size_t len = sizeof(stored);
    err = nvs_get_blob(h, ABV_NVS_KEY, &stored, &len);
    nvs_close(h);

    if (err != ESP_OK || len != sizeof(stored) ||
        stored.magic != ABV_CFG_MAGIC ||
        stored.version != ABV_CFG_VERSION ||
        !init_valid(&stored.init)) {
        abv_defaults(out);
        return (err == ESP_OK) ? ESP_ERR_INVALID_STATE : err;
    }

    *out = stored.init;
    return ESP_OK;
}

esp_err_t abv_config_save(const alcohol_init_t *init) {
    if (!init_valid(init)) return ESP_ERR_INVALID_ARG;

    abv_nvs_t stored = {
        .magic = ABV_CFG_MAGIC,
        .version = ABV_CFG_VERSION,
        .reserved = 0,
        .init = *init,
    };

    nvs_handle_t h;
    esp_err_t err = nvs_open(ABV_NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;

    err = nvs_set_blob(h, ABV_NVS_KEY, &stored, sizeof(stored));
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

static float ethanol_vapor_pa(float t_c) {
    // Антуан: log10(P[bar]) = A - B/(T[K] + C), C < 0.
    float t_k = clampf(t_c, -20.0f, 200.0f) + 273.15f;
    float den = t_k - 46.424f;
    if (den < 1.0f) den = 1.0f;
    float log10p = 5.24677f - 1598.673f / den;
    return powf(10.0f, log10p) * 100000.0f;   // bar -> Pa
}

static float water_vapor_pa(float t_c) {
    float t_k = clampf(t_c, -20.0f, 200.0f) + 273.15f;
    float den = t_k - 31.737f;
    if (den < 1.0f) den = 1.0f;
    float log10p = 5.40221f - 1838.675f / den;
    return powf(10.0f, log10p) * 100000.0f;
}

float abv_ethanol_vapor_pressure(float t_c) { return ethanol_vapor_pa(t_c); }
float abv_water_vapor_pressure(float t_c) { return water_vapor_pa(t_c); }

// Мольные доли жидкости по текущим объёмам.
static void liquid_moles(float *n_e, float *n_w) {
    *n_e = s_ethanol_ml * s_init.ethanol_density / M_ETHANOL;
    *n_w = s_water_ml * RHO_WATER / M_WATER;
    if (*n_e < 0.0f) *n_e = 0.0f;
    if (*n_w < 0.0f) *n_w = 0.0f;
}

static void activity_coeffs(float x_e, float *g_e, float *g_w) {
    *g_e = 1.0f;
    *g_w = 1.0f;
    if (x_e <= 0.0f || x_e >= 1.0f) return;

    float x_w = 1.0f - x_e;
    float den = VANLAAR_A12 * x_e + VANLAAR_A21 * x_w;
    if (den < 1e-6f) return;

    float t1 = VANLAAR_A21 * x_w / den;
    float t2 = VANLAAR_A12 * x_e / den;
    *g_e = expf(VANLAAR_A12 * t1 * t1);
    *g_w = expf(VANLAAR_A21 * t2 * t2);
}

// Давление насыщения смеси при заданной T, Па.
static float bubble_pressure_pa(float t_c, float x_e, float x_w) {
    float g_e, g_w;
    activity_coeffs(x_e, &g_e, &g_w);
    return x_e * g_e * ethanol_vapor_pa(t_c) +
           x_w * g_w * water_vapor_pa(t_c);
}

static float boiling_point_c(float p_abs_pa, float x_e, float x_w) {
    float lo = 30.0f, hi = 130.0f;
    for (int i = 0; i < 40; i++) {
        float mid = (lo + hi) * 0.5f;
        if (bubble_pressure_pa(mid, x_e, x_w) < p_abs_pa) lo = mid;
        else hi = mid;
    }
    return (lo + hi) * 0.5f;
}

void abv_init(const alcohol_init_t *init) {
    if (!init_valid(init)) {
        abv_defaults(&s_init);
    } else {
        s_init = *init;
    }

    s_ethanol_ml = s_init.volume_start_ml * (s_init.abv_start_percent / 100.0f);
    s_ethanol_start_ml = s_ethanol_ml;
    s_water_ml = s_init.volume_start_ml - s_ethanol_ml;
    if (s_water_ml < 0.0f) s_water_ml = 0.0f;
    s_abv_now = s_init.abv_start_percent;
    s_boiling_c = 100.0f;
    s_last_us = 0;
    s_ready = true;

    ESP_LOGI(TAG, "init: %.0f мл, %.1f %%, этанол %.0f мл",
             s_init.volume_start_ml, s_init.abv_start_percent, s_ethanol_ml);
}

void abv_reset(void) {
    if (!s_ready) {
        abv_init(NULL);
        return;
    }
    abv_init(&s_init);
}

bool abv_ready(void) { return s_ready; }

float abv_update(float t_cube_c, float pressure_hpa) {
    if (!s_ready) abv_init(NULL);
    if (!isfinite(t_cube_c)) t_cube_c = 0.0f;
    if (!isfinite(pressure_hpa) || pressure_hpa < 300.0f ||
        pressure_hpa > 1200.0f) {
        pressure_hpa = 1013.25f;
    }

    int64_t now = now_us();
    int64_t last = s_last_us;
    s_last_us = now;

    float pressure_pa = pressure_hpa * 100.0f;
    float n_e, n_w;
    liquid_moles(&n_e, &n_w);

    float total_n = n_e + n_w;
    if (total_n < 1e-6f) {
        s_ethanol_ml = 0.0f;
        s_abv_now = 0.0f;
        return 0.0f;
    }

    float x_e = n_e / total_n;
    float x_w = 1.0f - x_e;
    float g_e, g_w;
    activity_coeffs(x_e, &g_e, &g_w);

    s_boiling_c = boiling_point_c(pressure_pa, x_e, x_w);

    // Первый вызов после сброса только запоминает время.
    if (last == 0) {
        float total_vol = s_ethanol_ml + s_water_ml;
        s_abv_now = (total_vol > 0.0f) ? (s_ethanol_ml / total_vol) * 100.0f : 0.0f;
        return s_ethanol_ml;
    }

    float dt_s = (float)(now - last) / 1e6f;
    if (dt_s <= 0.0f) return s_ethanol_ml;
    if (dt_s > 5.0f) dt_s = 5.0f;   // защита от длинной паузы

    if (t_cube_c < 50.0f) return s_ethanol_ml;

    process_state_t *st = state_get();
    float rate_ml_min = 0.0f;
    if (st->is_running && !st->is_paused) {
        rate_ml_min = st->body_rate_ml_min;
    }
    if (!isfinite(rate_ml_min) || rate_ml_min <= 0.01f) return s_ethanol_ml;

    // Куб кипит, только если давление насыщения смеси достигло
    // атмосферного. Иначе это нагрев, а не отбор.
    float p_bubble = bubble_pressure_pa(t_cube_c, x_e, x_w);
    if (p_bubble < pressure_pa * 0.90f) return s_ethanol_ml;

    float y_e = (x_e * g_e * ethanol_vapor_pa(t_cube_c)) / pressure_pa;
    y_e = clampf(y_e, 0.0f, 1.0f);

    float m_mix = y_e * M_ETHANOL + (1.0f - y_e) * M_WATER;
    float rho_mix = y_e * s_init.ethanol_density + (1.0f - y_e) * RHO_WATER;
    if (m_mix < 1e-3f) return s_ethanol_ml;

    // Сконденсированный объём отбора -> моли пара.
    float vapor_ml = rate_ml_min * (dt_s / 60.0f);
    float n_vap = vapor_ml * rho_mix / m_mix;

    float dn_e = n_vap * y_e;
    float dn_w = n_vap * (1.0f - y_e);

    s_ethanol_ml -= dn_e * M_ETHANOL / s_init.ethanol_density;
    s_water_ml -= dn_w * M_WATER / RHO_WATER;

    if (s_ethanol_ml < 0.0f) s_ethanol_ml = 0.0f;
    if (s_water_ml < 0.0f) s_water_ml = 0.0f;

    float total_vol = s_ethanol_ml + s_water_ml;
    s_abv_now = (total_vol > 0.0f) ? (s_ethanol_ml / total_vol) * 100.0f : 0.0f;

    return s_ethanol_ml;
}

float abv_get_remaining_ml(void) { return s_ethanol_ml; }

float abv_get_remaining_percent(void) {
    if (s_ethanol_start_ml <= 0.0f) return 0.0f;
    return (s_ethanol_ml / s_ethanol_start_ml) * 100.0f;
}

float abv_get_ethanol_g(void) { return s_ethanol_ml * s_init.ethanol_density; }
float abv_get_current_abv_percent(void) { return s_abv_now; }
float abv_get_start_volume_ml(void) { return s_init.volume_start_ml; }
float abv_get_start_abv_percent(void) { return s_init.abv_start_percent; }
float abv_get_boiling_point_c(void) { return s_boiling_c; }
