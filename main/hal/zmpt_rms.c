#include "zmpt_rms.h"
#include "hal_adc.h"
#include "config.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <math.h>
#include <string.h>

static const char *TAG = "ZMPT";
static float s_phase_v[3] = {NAN, NAN, NAN};
static float s_ctrl_v = NAN;
static float s_hist[3][ZMPT_MEDIAN_WINDOW];
static int s_hist_idx[3] = {0,0,0};
static int s_hist_cnt[3] = {0,0,0};
static bool s_fault = false;

esp_err_t zmpt_rms_init(void) {
#if !ZMPT_ENABLED
    return ESP_OK;
#endif
    for (int p=0;p<3;p++) for (int i=0;i<ZMPT_MEDIAN_WINDOW;i++) s_hist[p][i]=NAN;
    ESP_LOGI(TAG, "ZMPT RMS init (median window=%d)", ZMPT_MEDIAN_WINDOW);
    return ESP_OK;
}

static float medianf(float *v, int n) {
    float tmp[ZMPT_MEDIAN_WINDOW];
    int m = 0;
    for (int i=0;i<n;i++) if (isfinite(v[i])) tmp[m++]=v[i];
    if (m==0) return NAN;
    for (int i=0;i<m;i++) for (int j=i+1;j<m;j++) if (tmp[j]<tmp[i]) { float t=tmp[i]; tmp[i]=tmp[j]; tmp[j]=t; }
    return tmp[m/2];
}

// Rough RMS estimate from ADS1115 samples: sample window, remove DC, compute RMS.
// Limited by ADS1115 sampling rate; suitable for monitoring, calibrate before control.
static float sample_rms_phase(int ads2_ch) {
    const int N = 8; // limited by ADS1115 rate; calibrate on bench
    float samples[8];
    float mean = 0.0f;
    for (int i=0;i<N;i++) {
        float v = hal_adc2_voltage(ads2_ch);
        samples[i] = v;
        mean += v;
    }
    mean /= (float)N;
    float acc = 0.0f;
    for (int i=0;i<N;i++) {
        float d = samples[i] - mean;
        acc += d*d;
    }
    float vrms_adc = sqrtf(acc / (float)N);
    return vrms_adc * 250.0f * ZMPT_CALIB_DEFAULT;
}

void zmpt_rms_poll(void) {
#if !ZMPT_ENABLED
    return;
#endif
    // Use ADS1115#2 channels A1-A3 for 3 phases; A0 reserved for PT-506.
    int ch[3] = {1, 2, 3};
    bool any_fault = false;
    for (int p=0;p<ZMPT_PHASE_COUNT && p<3;p++) {
        float v = sample_rms_phase(ch[p]);
        s_phase_v[p] = v;
        s_hist[p][s_hist_idx[p]] = v;
        s_hist_idx[p] = (s_hist_idx[p] + 1) % ZMPT_MEDIAN_WINDOW;
        if (s_hist_cnt[p] < ZMPT_MEDIAN_WINDOW) s_hist_cnt[p]++;
        if (!isfinite(v) || v < ZMPT_RMS_MIN_V || v > ZMPT_RMS_MAX_V) any_fault = true;
    }
    float m[3];
    for (int p=0;p<3;p++) m[p] = medianf(s_hist[p], s_hist_cnt[p]);
    float sum = 0; int n=0;
    for (int p=0;p<3;p++) if (isfinite(m[p])) { sum += m[p]; n++; }
    s_ctrl_v = n ? (sum / (float)n) : NAN;
    s_fault = any_fault || !isfinite(s_ctrl_v);
}

float zmpt_rms_phase_v(int phase) { return (phase>=0 && phase<3) ? s_phase_v[phase] : NAN; }
float zmpt_rms_ctrl_v(void) { return s_ctrl_v; }
bool zmpt_rms_fault(void) { return s_fault; }
