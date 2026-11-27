#include "ctrl_scale.h"
#include "core/events.h"
#include "core/watchdog.h"
#include "nvs.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static scale_t s;
static nvs_handle_t nvs;
static uint32_t s_calib_start = 0;
static float s_calib_known_weight = 0;

// ---- Multi-point calibration / linearization ----
#define SCALE_CAL_PTS_MAX 6
typedef struct { int32_t raw; float g; } scale_pt_t;
static scale_pt_t s_pts[SCALE_CAL_PTS_MAX];
static int s_pts_n = 0;

static void scale_pts_save(void) {
    nvs_set_blob(nvs, "pts_n", &s_pts_n, sizeof(s_pts_n));
    nvs_set_blob(nvs, "pts", s_pts, sizeof(s_pts));
    nvs_commit(nvs);
}

static void scale_pts_load(void) {
    size_t sz = sizeof(s_pts_n);
    if (nvs_get_blob(nvs, "pts_n", &s_pts_n, &sz) != ESP_OK) s_pts_n = 0;
    if (s_pts_n < 0 || s_pts_n > SCALE_CAL_PTS_MAX) s_pts_n = 0;
    sz = sizeof(s_pts);
    if (s_pts_n > 0 && nvs_get_blob(nvs, "pts", s_pts, &sz) != ESP_OK) s_pts_n = 0;
}


void scale_init(int pin_dout, int pin_sck) {
    nvs_open("scale", NVS_READWRITE, &nvs);
    memset(&s, 0, sizeof(s));
    s.density = 1.0f;
    s.auto_tare = true;
    s.auto_tare_threshold_g = 0.5f;

    hx711_init(&s.hw, pin_dout, pin_sck, 128);

    size_t sz = sizeof(float);
    nvs_get_blob(nvs, "scale", &s.hw.scale, &sz);
    int32_t off = 0;
    sz = 4;
    if (nvs_get_blob(nvs, "offset", &off, &sz) == ESP_OK) {
        s.hw.offset = off;
        s.hw.calibrated = true;
    }
    if (s.hw.scale == 0) s.hw.scale = 1.0f;

    scale_pts_load();
    s.state = s.hw.calibrated ? SCALE_READY : SCALE_IDLE;
    s.session_start_ms = (uint32_t)(esp_timer_get_time() / 1000);
    events_log(EVENT_INFO, "Scale init: scale=%.3f offset=%d pts=%d",
               s.hw.scale, s.hw.offset, s_pts_n);
}

scale_t* scale_get(void) { return &s; }

static void save_calib(void) {
    nvs_set_blob(nvs, "scale",  &s.hw.scale,  sizeof(float));
    nvs_set_blob(nvs, "offset", &s.hw.offset, sizeof(int32_t));
    nvs_commit(nvs);
}

void scale_tare_now(void) {
    s.state = SCALE_TARING;
    hx711_tare(&s.hw, 20);
    s.tare_grams = 0;
    s.total_grams = 0;
    s.total_ml = 0;
    s.state = SCALE_READY;
    save_calib();
    events_log(EVENT_INFO, "Scale: tare done (offset=%d)", s.hw.offset);
}

void scale_reset_session(void) {
    s.total_grams = 0;
    s.total_ml = 0;
    s.session_start_ms = (uint32_t)(esp_timer_get_time() / 1000);
    memset(s.window, 0, sizeof(s.window));
    s.window_idx = 0;
    events_log(EVENT_INFO, "Scale: session reset");
}

void scale_start_calibration(float known_weight_g) {
    s.state = SCALE_CALIBRATING;
    s_calib_known_weight = known_weight_g;
    s_calib_start = (uint32_t)(esp_timer_get_time() / 1000);
    events_log(EVENT_INFO, "Scale: calibration started (%.1f g)", known_weight_g);
}

bool scale_calibration_step(void) {
    if (s.state != SCALE_CALIBRATING) return false;

    int32_t raw = hx711_read_avg(&s.hw, 30);
    int32_t delta = raw - s.hw.offset;

    if (fabsf((float)delta) < 100) return false;

    s.hw.scale = (float)delta / s_calib_known_weight;
    s.state = SCALE_READY;
    save_calib();
    events_log(EVENT_INFO, "Scale: calibrated scale=%.3f (%.1f g)",
               s.hw.scale, s_calib_known_weight);
    return true;
}

void scale_set_density(float d) {
    if (d > 0.1f && d < 2.0f) s.density = d;
}

bool  scale_is_stable(void)          { return s.stable; }
bool  scale_is_ready(void)           { return s.state == SCALE_READY; }
float scale_get_flow_ml_min(void)    { return s.flow_ml_per_min; }
float scale_get_total_ml(void)       { return s.total_ml; }
float scale_get_weight_g(void)        { return s.grams_filtered; }

void scale_update_state(process_state_t *st, bool indicator_active, float collected_ml)
{
    if (!st) return;
    bool ready = scale_is_ready();
    st->scale_ready = ready;
    st->scale_indicator_active = ready && indicator_active;
    st->scale_weight_g = ready ? s.grams_filtered : 0.0f;
    st->scale_flow_ml_min = ready ? s.flow_ml_per_min : 0.0f;
    st->scale_collected_ml = (ready && indicator_active && isfinite(collected_ml) && collected_ml > 0.0f)
                                  ? collected_ml : 0.0f;
}

void scale_task(void *arg) {
    (void)arg;
    wd_register("scale", 5000, true);

    uint32_t last_ms = (uint32_t)(esp_timer_get_time() / 1000);
    float last_g = 0;
    float stable_timer = 0;
    int read_count = 0;

    while (1) {
        WD_FEED("scale");

        if (s.state == SCALE_CALIBRATING) {
            scale_calibration_step();
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        float g = hx711_read_grams(&s.hw);

        if (read_count < 5) s.grams_filtered = g;
        else s.grams_filtered = 0.7f * s.grams_filtered + 0.3f * g;
        read_count++;
        s.grams = g;

        uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
        float dt_s = (now_ms - last_ms) / 1000.0f;

        if (dt_s > 0.1f) {
            float dg = s.grams_filtered - last_g;
            float inst_flow = dg / dt_s * 60.0f;

            s.window[s.window_idx] = inst_flow;
            s.window_idx = (s.window_idx + 1) % 10;
            float sum = 0;
            for (int i = 0; i < 10; i++) sum += s.window[i];
            s.flow_g_per_min = sum / 10.0f;
            s.flow_ml_per_min = s.flow_g_per_min / s.density;

            s.total_grams += dg;
            s.total_ml = s.total_grams / s.density;

            if (fabsf(inst_flow) < 0.5f) {
                stable_timer += dt_s;
                s.stable = (stable_timer > 3.0f);
            } else {
                stable_timer = 0;
                s.stable = false;
            }

            last_g = s.grams_filtered;
            last_ms = now_ms;
        }

        if (s.auto_tare && s.stable && fabsf(s.grams_filtered) < s.auto_tare_threshold_g) {
            static uint32_t zero_since = 0;
            if (zero_since == 0) zero_since = now_ms;
            if (now_ms - zero_since > 10000) {
                hx711_tare(&s.hw, 10);
                save_calib();
                zero_since = 0;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

int scale_calib_points_count(void) { return s_pts_n; }

bool scale_calib_points_add(float known_g) {
    if (s_pts_n >= SCALE_CAL_PTS_MAX) return false;
    if (!isfinite(known_g) || known_g <= 0.0f) return false;
    if (s_pts_n == 0) scale_pts_load();
    int32_t raw = hx711_read_avg(&s.hw, 20);
    s_pts[s_pts_n].raw = raw;
    s_pts[s_pts_n].g = known_g;
    s_pts_n++;
    scale_pts_save();
    events_log(EVENT_INFO, "Scale: calib point raw=%d g=%.2f (%d pts)", (int)raw, (double)known_g, s_pts_n);
    return true;
}

void scale_calib_points_clear(void) {
    s_pts_n = 0;
    memset(s_pts, 0, sizeof(s_pts));
    scale_pts_save();
    events_log(EVENT_INFO, "Scale: calib points cleared");
}

bool scale_calib_points_apply(void) {
    if (s_pts_n == 0) scale_pts_load();
    if (s_pts_n < 2) return false;
    // Least-squares linear fit: g = a*raw + b  => scale=1/a, offset = -b/a
    double sum_x=0,sum_y=0,sum_xx=0,sum_xy=0;
    for (int i=0;i<s_pts_n;i++){ double x=s_pts[i].raw, y=s_pts[i].g; sum_x+=x; sum_y+=y; sum_xx+=x*x; sum_xy+=x*y; }
    double n=s_pts_n;
    double den = n*sum_xx - sum_x*sum_x;
    if (fabs(den) < 1e-9) return false;
    double a = (n*sum_xy - sum_x*sum_y) / den; // g per raw
    double b = (sum_y - a*sum_x) / n;
    if (fabs(a) < 1e-12) return false;
    s.hw.scale = (float)a;
    s.hw.offset = (int32_t)(-b/a);
    s.hw.calibrated = true;
    s.state = SCALE_READY;
    save_calib();
    events_log(EVENT_INFO, "Scale: multi-point calibrated scale=%.6f offset=%d (%d pts)", (double)s.hw.scale, (int)s.hw.offset, s_pts_n);
    return true;
}

size_t scale_calib_points_json(char *buf, size_t buf_sz) {
    if (!buf || buf_sz==0) return 0;
    if (s_pts_n == 0) scale_pts_load();
    size_t off = 0;
    off += snprintf(buf+off, buf_sz-off, "{\"count\":%d,\"points\":[", s_pts_n);
    for (int i=0;i<s_pts_n && off+2<buf_sz;i++) {
        off += snprintf(buf+off, buf_sz-off, "%s{\"raw\":%d,\"g\":%.2f}", i?",":"", (int)s_pts[i].raw, (double)s_pts[i].g);
    }
    if (off+40 < buf_sz) off += snprintf(buf+off, buf_sz-off, "],\"scale\":%.6f,\"offset\":%d}", (double)s.hw.scale, (int)s.hw.offset);
    else if (off+3<buf_sz) off += snprintf(buf+off, buf_sz-off, "]}");
    return off;
}
