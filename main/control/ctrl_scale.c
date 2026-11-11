#include "ctrl_scale.h"
#include "core/events.h"
#include "core/watchdog.h"
#include "nvs.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include <string.h>

static scale_t s;
static nvs_handle_t nvs;
static uint32_t s_calib_start = 0;
static float s_calib_known_weight = 0;

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

    s.state = s.hw.calibrated ? SCALE_READY : SCALE_IDLE;
    s.session_start_ms = (uint32_t)(esp_timer_get_time() / 1000);
    events_log(EVENT_INFO, "Scale init: scale=%.3f offset=%d",
               s.hw.scale, s.hw.offset);
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
float scale_get_flow_ml_min(void)    { return s.flow_ml_per_min; }
float scale_get_total_ml(void)       { return s.total_ml; }

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