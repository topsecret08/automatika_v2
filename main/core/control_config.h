#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

// Единая конфигурация автоматики. Все ключи хранятся в NVS namespace
// "automatika" (раньше часть лежала в отдельных namespace-ах).
//
// Ключи NVS:
//   api_key      строк(64)  REST API key, X-API-Key. Пусто = ключ не задан.
//   ctrl_mode    u8         control_mode_t: 0 PID, 1 PWM_BANG_BANG
//   out_dev      u8         output_device_t: 0 клапан, 1 насос-дозатор
//   hys          i32        гистерезис BANG_BANG, 0.01 °C (100 = 1.0 °C)
//   rate_mlmin   i32        скорость отбора, 0.01 мл/мин
//   flow_mlmin   i32        расход клапана при 100 %, 0.01 мл/мин
//   steps_per_ml u32        калибровка дозатора, шагов на мл
//   svc          u8         сервисный режим (диагностика исполнителей)

typedef enum {
    CONTROL_MODE_PID = 0,        // AutoPID → ШИМ/сигнал мощности
    CONTROL_MODE_PWM_BANG_BANG,  // двухпозиционный, порог HYS
    CONTROL_MODE_COUNT
} control_mode_t;

typedef enum {
    OUTPUT_DEVICE_VALVE = 0,     // клапан отбора (серво/соленоид)
    OUTPUT_DEVICE_PUMP,          // шаговый дозатор
    OUTPUT_DEVICE_COUNT
} output_device_t;

typedef enum {
    REGULATOR_NONE = 0,          // внутренний DAC/ШИМ
    REGULATOR_KVIC,              // внешний UART-регулятор
    REGULATOR_RMVK,              // внешний UART-регулятор
    REGULATOR_SEM_AVR,           // внешний UART-регулятор
    REGULATOR_SSR_PWM,           // внешний SSR по ШИМ
    REGULATOR_COUNT
} regulator_type_t;

#define API_KEY_LEN        33        // 32 hex + '\0'
#define API_KEY_NVS_NS     "automatika"
#define API_KEY_NVS_KEY    "api_key"

esp_err_t       control_config_init(void);
bool            control_config_ready(void);

control_mode_t  control_config_get_mode(void);
esp_err_t       control_config_set_mode(control_mode_t m);

output_device_t control_config_get_output_device(void);
esp_err_t       control_config_set_output_device(output_device_t d);

float           control_config_get_hys(void);
esp_err_t       control_config_set_hys(float hys);

float           control_config_get_rate_ml_min(void);
esp_err_t       control_config_set_rate_ml_min(float rate);
float           control_config_get_flow_ml_min(void);
esp_err_t       control_config_set_flow_ml_min(float flow);

uint32_t        control_config_get_steps_per_ml(void);
esp_err_t       control_config_set_steps_per_ml(uint32_t steps);

// --- Внешний регулятор мощности -------------------------------------
regulator_type_t control_config_get_regulator(void);
esp_err_t        control_config_set_regulator(regulator_type_t type);
uint32_t         control_config_get_regulator_baud(void);
esp_err_t        control_config_set_regulator_baud(uint32_t baud);
const char      *control_config_regulator_name(regulator_type_t type);

// --- API key ---------------------------------------------------------
bool            control_config_api_has_key(void);
// Возвращает true, если ключ совпадает (сравнение за постоянное время).
bool            control_config_api_check(const char *key);
// Новый ключ (32 hex из esp_random) и сохранение в NVS.
esp_err_t       control_config_api_generate(char *out, size_t out_len);
// Пустой ключ: API закрыт полностью (любой запрос → 401).
esp_err_t       control_config_api_clear(void);
void            control_config_api_copy(char *out, size_t out_len);

// --- сервисный режим -------------------------------------------------
bool            control_config_service_enabled(void);
esp_err_t       control_config_set_service(bool on);
