#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

typedef struct {
    int      pin_dout;
    int      pin_sck;
    uint8_t  gain;       // 128 / 64 / 32
    int32_t  offset;     // тара (raw)
    float    scale;      // отсчётов на грамм
    bool     calibrated;
} hx711_t;

esp_err_t hx711_init(hx711_t *h, int dout, int sck, uint8_t gain);
bool      hx711_is_ready(hx711_t *h);
int32_t   hx711_read_raw(hx711_t *h);
int32_t   hx711_read_avg(hx711_t *h, int n);
void      hx711_tare(hx711_t *h, int samples);
void      hx711_set_scale(hx711_t *h, float scale);
float     hx711_read_grams(hx711_t *h);
int32_t   hx711_read_offset(hx711_t *h);