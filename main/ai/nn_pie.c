// PIE-ускорение нейросетей на ESP32-S3 (SIMD int8)
// Требует esp-dl для полноценной работы; иначе — scalar fallback.
#include <stdint.h>
#include "esp_log.h"

// Заглушка: реальная реализация использует esp_nn_fully_connected_s8
// из компонента esp-dl. Здесь — версия без ускорения.

void nn_forward_pie(const int8_t *input, const int8_t *w1, const int32_t *b1,
                    int8_t *hidden, const int8_t *w2, const int32_t *b2,
                    int8_t *output, int in_dim, int hid_dim, int out_dim) {
    for (int j = 0; j < hid_dim; j++) {
        int32_t s = b1 ? b1[j] : 0;
        for (int i = 0; i < in_dim; i++)
            s += (int32_t)input[i] * w1[i * hid_dim + j];
        s /= 127;
        hidden[j] = (s > 0) ? (s > 127 ? 127 : (int8_t)s) : 0;
    }
    for (int k = 0; k < out_dim; k++) {
        int32_t s = b2 ? b2[k] : 0;
        for (int j = 0; j < hid_dim; j++)
            s += (int32_t)hidden[j] * w2[j * out_dim + k];
        s /= 127;
        if (s > 127) s = 127;
        if (s < -128) s = -128;
        output[k] = (int8_t)s;
    }
}

void check_pie(void) {
    extern uint32_t esp_cpu_get_capabilities(void);
    if (esp_cpu_get_capabilities() & (1 << 1)) {   // PIE bit
        ESP_LOGI("NN", "PIE enabled: SIMD int8 acceleration available");
    } else {
        ESP_LOGI("NN", "PIE not available, scalar fallback");
    }
}