#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LV_FLAG_SENSOR_PLAUSIBILITY   (1u << 0)
#define LV_FLAG_SENSOR_STUCK          (1u << 1)
#define LV_FLAG_SENSOR_CROSSCHECK     (1u << 2)
#define LV_FLAG_PROG_FSM_ILLEGAL      (1u << 3)
#define LV_FLAG_PROG_SP_SANITY        (1u << 4)
#define LV_FLAG_PROG_PID_SAT          (1u << 5)
#define LV_FLAG_ACT_MISMATCH          (1u << 6)
#define LV_FLAG_ACT_NO_FEEDBACK       (1u << 7)
#define LV_FLAG_LATENCY_OVER          (1u << 8)
#define LV_FLAG_NO_RESPONSE           (1u << 9)

#define LV_MAX_ACTUATORS 16

typedef struct {
    uint8_t  id;
    float    raw;
    float    filtered;
    float    confidence;
    uint32_t flags;
} lv_sensor_frame_t;

typedef struct {
    uint8_t  mode;
    float    sp;
    float    pv;
    float    out;
    uint32_t flags;
} lv_program_frame_t;

typedef struct {
    uint8_t  id;
    float    cmd;
    float    actual;
    uint32_t latency_us;
    uint32_t flags;
} lv_actuator_frame_t;

typedef struct {
    uint32_t sensor_flags;
    uint32_t program_flags;
    uint32_t actuator_flags;
    uint32_t aggregate;
    uint32_t latency_e2e_us;
    float    min_confidence;
} lv_result_t;

void loop_verify_init(void);

// Чистый расчёт (для тестов): now_us задаётся вызывающим.
void lv_tick_at(int64_t now_us, int64_t sensor_us,
                const lv_sensor_frame_t *s, size_t n_s,
                const lv_program_frame_t *p,
                const lv_actuator_frame_t *a, size_t n_a,
                lv_result_t *out);

void lv_tick(int64_t sensor_us,
             const lv_sensor_frame_t *s, size_t n_s,
             const lv_program_frame_t *p,
             const lv_actuator_frame_t *a, size_t n_a,
             lv_result_t *out);
void lv_escalate(const lv_result_t *r);
