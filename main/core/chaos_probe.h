#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    FAULT_I2C_DROP = 0,
    FAULT_ADC_SPIKE,
    FAULT_TASK_DELAY,
    FAULT_WDT,
    FAULT_MEM_PRESSURE,
    FAULT_COUNT
} fault_type_t;

typedef struct {
    uint32_t rate_per_min;
    uint32_t seed;
    uint32_t injected;
    uint32_t safe_count;
    uint64_t detect_ms_sum;
    int64_t  next_us;
} chaos_probe_t;

void  chaos_probe_start(chaos_probe_t *cp, uint32_t rate_per_min,
                        uint32_t seed, int64_t now_us);
bool  chaos_probe_should_inject(chaos_probe_t *cp, int64_t now_us,
                                fault_type_t *out);
void  chaos_probe_report(chaos_probe_t *cp, bool went_safe, uint64_t ms);
float chaos_probe_safe_rate(const chaos_probe_t *cp);
uint64_t chaos_probe_avg_ms(const chaos_probe_t *cp);
