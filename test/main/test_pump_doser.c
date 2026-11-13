#include "unity.h"

#include "config.h"
#include "control/pump_doser.h"

// Тесты публичного фасада дозатора (control/pump_doser.c).
//
// Здесь проверяется только «сухая» логика: значения по умолчанию и
// отклонение некорректных доз ДО обращения к очереди задачи `doser`.
// Реальная выдача STEP-импульсов требует запущенной FreeRTOS-задачи и
// NVS, поэтому в unit-тестах не выполняется.

TEST_CASE("pump_calib_defaults", "[pump_doser]") {
    PumpCalib c;
    TEST_ASSERT_EQUAL(ESP_OK, pump_calib_load(&c));
    TEST_ASSERT_EQUAL_UINT32(PUMP_STEPS_PER_ML_DEFAULT, c.steps_per_ml);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)(DOSER_MAX_DOSE_ML * c.steps_per_ml),
                             c.max_steps);
}

TEST_CASE("pump_dose_rejects_zero_and_negative", "[pump_doser]") {
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, pump_dose_volume(0.0f, 60.0f));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, pump_dose_volume(-1.0f, 60.0f));
}

TEST_CASE("pump_dose_rejects_over_limit", "[pump_doser]") {
    // DOSER_MAX_DOSE_ML + 1 мл не должно доходить до очереди дозатора.
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE,
                      pump_dose_volume(DOSER_MAX_DOSE_ML + 1.0f, 60.0f));
}

TEST_CASE("pump_calib_save_validates_input", "[pump_doser]") {
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, pump_calib_save(NULL));
}
