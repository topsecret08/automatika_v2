#pragma once
#include <stdint.h>

int16_t hal_adc_raw(int channel);          // channel 0..3
float   hal_adc_pressure_bar(void);        // 4-20 мА → 0-4 бар
float   hal_adc_vacuum_mbar(void);         // 0.5-4.5 В → 0-1000 мбар
float   hal_adc_spill_voltage(void);       // влажностный
float   hal_adc_vapor_ppm(void);           // MQ-3