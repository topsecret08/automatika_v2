#include "temp_filter.h"
#include "config.h"

#include <math.h>
#include <string.h>

#define SENSOR_FAIL_LOW   (-50.0f)
#define SENSOR_FAIL_HIGH  (200.0f)

static TempFilter s_ch[TEMP_FILTER_MAX_CHANNELS];
static bool       s_init = false;

static bool sample_ok(float v) {
    return v > SENSOR_FAIL_LOW && v < SENSOR_FAIL_HIGH;
}

static uint8_t clamp_window(int w) {
    if (w > TEMP_FILTER_MAX_WINDOW) return TEMP_FILTER_MAX_WINDOW;
    if (w < 3) return 3;
    return (uint8_t)w;
}

void temp_filter_init(TempFilter *f, uint8_t window_size,
                      float trend_threshold, float rise_delta,
                      float drop_delta) {
    if (!f) return;
    memset(f, 0, sizeof(*f));
    f->window_size = clamp_window((int)window_size);
    f->trend_threshold = trend_threshold;
    f->rise_delta = (rise_delta > 0.0f) ? rise_delta : 0.5f;
    f->drop_delta = (drop_delta > 0.0f) ? drop_delta : 0.2f;
    f->value = NAN;
    f->trend_start = NAN;
}

void temp_filter_reset(TempFilter *f) {
    if (!f) return;
    const uint8_t w = f->window_size;
    temp_filter_init(f, w, f->trend_threshold, f->rise_delta, f->drop_delta);
}

/* Линейный тренд по последним до 3 валидным сэмплам.
 * Возвращает °C на шаг опроса; положительный — рост. */
static float linear_slope(const TempFilter *f, int *out_n) {
    float y[3];
    int n = 0;

    for (int k = 0; k < f->count && n < 3; k++) {
        int idx = (int)f->index - 1 - k;
        if (idx < 0) idx += f->window_size;
        if (!f->ok[idx]) continue;
        y[n++] = f->buffer[idx];
    }
    if (out_n) *out_n = n;
    if (n < 2) return 0.0f;

    float mean_y = 0.0f;
    for (int i = 0; i < n; i++) mean_y += y[i];
    mean_y /= (float)n;

    float num = 0.0f, den = 0.0f;
    for (int i = 0; i < n; i++) {
        float x = (float)(n - 1 - i);               /* 0 у самого свежего */
        float dx = x - (float)(n - 1) / 2.0f;
        num += dx * (y[i] - mean_y);
        den += dx * dx;
    }
    return (den > 0.0f) ? -num / den : 0.0f;        /* минус: свежий = x=0 */
}

float temp_filter_update(TempFilter *f, float new_sample) {
    if (!f) return NAN;

    if (!sample_ok(new_sample)) {
        f->bad_count++;
        if (f->bad_count >= 3) {
            temp_filter_reset(f);
            f->bad_count = 3;
        }
        f->value = NAN;
        f->rising = false;
        return NAN;
    }
    f->bad_count = 0;

    f->buffer[f->index] = new_sample;
    f->ok[f->index] = true;
    f->index = (uint8_t)((f->index + 1) % f->window_size);
    if (f->count < f->window_size) f->count++;

    float sum = 0.0f;
    int n = 0;
    for (int i = 0; i < f->count; i++) {
        if (!f->ok[i]) continue;
        sum += f->buffer[i];
        n++;
    }
    f->value = (n > 0) ? sum / (float)n : NAN;

    /* шаг опроса 1-Wire ~1 с */
    f->slope = linear_slope(f, NULL) * 60.0f;

    temp_filter_is_rising(f, 0.0f);
    return f->value;
}

bool temp_filter_is_rising(TempFilter *f, float delta_threshold) {
    if (!f || !isfinite(f->value)) return false;

    float thr = (delta_threshold > 0.0f) ? delta_threshold : f->rise_delta;

    if (f->slope > f->trend_threshold) {
        if (!f->trend_active) {
            f->trend_active = true;
            f->trend_start = f->value;
        }
        if (!f->rising && (f->value - f->trend_start) >= thr)
            f->rising = true;
    } else if (f->trend_active) {
        if ((f->trend_start - f->value) >= f->drop_delta) {
            f->trend_active = false;
            f->rising = false;
        }
    }

    return f->rising;
}

float temp_filter_value(const TempFilter *f) { return f ? f->value : NAN; }
float temp_filter_slope(const TempFilter *f) { return f ? f->slope : 0.0f; }
bool  temp_filter_rising(const TempFilter *f) { return f ? f->rising : false; }

int temp_filter_valid_count(const TempFilter *f) {
    if (!f) return 0;
    int n = 0;
    for (int i = 0; i < f->count; i++) if (f->ok[i]) n++;
    return n;
}

// ------------------------------------------------------------------

void temp_filter_module_init(uint8_t channels, uint8_t window) {
    (void)channels;
    memset(s_ch, 0, sizeof(s_ch));
    s_init = true;
    for (int i = 0; i < TEMP_FILTER_MAX_CHANNELS; i++)
        temp_filter_init(&s_ch[i], window, (float)TREND_THRESHOLD,
                         TREND_RISE_DELTA, TREND_DROP_DELTA);
}

static TempFilter *ch(int idx) {
    if (idx < 0 || idx >= TEMP_FILTER_MAX_CHANNELS) return NULL;
    if (!s_init) temp_filter_module_init(8, FILTER_WINDOW);
    return &s_ch[idx];
}

float temp_filter_feed(int idx, float raw) {
    TempFilter *f = ch(idx);
    return f ? temp_filter_update(f, raw) : NAN;
}

float temp_filter_read(int idx) {
    TempFilter *f = ch(idx);
    return f ? f->value : NAN;
}

bool temp_filter_ch_rising(int idx) {
    TempFilter *f = ch(idx);
    return f ? f->rising : false;
}

float temp_filter_ch_slope(int idx) {
    TempFilter *f = ch(idx);
    return f ? f->slope : 0.0f;
}

bool temp_filter_ch_valid(int idx) {
    TempFilter *f = ch(idx);
    return f ? (temp_filter_valid_count(f) > 0) : false;
}
