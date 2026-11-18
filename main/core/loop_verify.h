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
#define LV_MAX_SENSORS   16

// Бюджет сквозной задержки: 1-Wire опрашивается ~1 Гц, поэтому 50 мс
// из ТЗ недостижимы физически. 1,5 с ловит зависание конвейера датчиков,
// но не срабатывает на штатном периоде опроса.
#define LV_LATENCY_BUDGET_US 1500000u

// Окно по умолчанию для NO_RESPONSE (секунды). Актуатор может задать
// своё окно в поле response_window_sec.
#define LV_DEFAULT_RESPONSE_SEC 30u

// Сколько подряд тактов значение датчика должно стоять на месте,
// чтобы получить SENSOR_STUCK. При 2 Гц это 5 минут.
#define LV_STUCK_TICKS 600u

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
    // Флаги уровня программы. PROG_FSM_ILLEGAL выставляет сама программа
    // (режим/FSM), потому что loop_verify не знает графа этапов.
    uint32_t flags;
} lv_program_frame_t;

typedef struct {
    uint8_t  id;
    float    cmd;
    float    actual;
    uint32_t latency_us;
    uint32_t flags;
    // true — команда бинарная (реле/контактор): cmd>0 ожидает actual=100 %.
    bool     binary;
    // 0 — LV_DEFAULT_RESPONSE_SEC.
    uint16_t response_window_sec;
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

// Последний результат (для API/телеметрии). Возвращает false, если
// проверок ещё не было.
bool loop_verify_last_result(lv_result_t *out);

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

// Антидребезг эскалации: true только при смене агрегата или маски.
// Обновляет *last_aggregate/*last_detail.
bool lv_escalation_changed(uint32_t aggregate, uint32_t detail,
                           uint32_t *last_aggregate,
                           uint32_t *last_detail);
