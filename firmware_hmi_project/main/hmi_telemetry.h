#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Нормализованное состояние, которое HMI показывает на экранах.
 * Заполняется из JSON-поля TELEMETRY, но НЕ трогает LVGL из RS485-задачи. */
typedef struct {
    bool     link;
    char     version[24];
    float    tc;        /* current temperature */
    float    ts;        /* set temperature */
    float    p;         /* power, % (from telemetry "power"; fallback "p") */
    float    m;         /* additional measured value */
    bool     sa;        /* safety ok */
    bool     es;        /* E-STOP pressed */
    bool     sd;        /* device/sensor ready */
    bool     db;        /* database/API ready */
    uint8_t  lv;        /* level, % */
    uint8_t  stg;       /* stage */
    uint8_t  run;
    uint8_t  pause;
    uint8_t  r;         /* run/regulator flag from controller */
    uint32_t heads;
    uint32_t body;
    uint32_t tails;
    uint32_t elapsed;   /* seconds */
    char     frac[16];
    float    baro;
    float    flow;
    uint32_t heap;
    uint32_t epoch_s;   /* optional time from controller */

    /* E-STOP / bench mode */
    bool     estop_active;
    bool     actions_blocked;
    bool     bench_mode;
    uint8_t  estop_mode;
    char     reason[48];

    /* expansion sensors */
    float    lsa_pwm;
    float    pt506_bar;
    float    xgzp_kpa;
    float    zmpt_v1;
    float    zmpt_v2;
    float    zmpt_v3;
    float    zmpt_ctrl;
    bool     zmpt_fault;

    /* scale indicator (HX711) */
    bool     scale_ready;
    bool     scale_active;
    float    scale_g;
    float    scale_flow;
    float    scale_ml;

    /* link diagnostics */
    uint32_t crc_err;
    uint32_t frames;
    uint16_t latency_ms;
} hmi_telemetry_t;

void hmi_telemetry_reset(void);
void hmi_telemetry_from_json(const uint8_t *payload, uint16_t len);
void hmi_telemetry_set_link(bool link);
void hmi_telemetry_set_stats(uint32_t crc_err, uint32_t frames,
                             uint16_t latency_ms);
const hmi_telemetry_t *hmi_telemetry_get(void);

/* Called from LVGL timer context only. */
void hmi_telemetry_apply_ui(void);
