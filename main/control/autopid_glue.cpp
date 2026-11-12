/*
 * C++-обвязка вокруг hayschan/autopid_for_esp_idf (class AutoPID).
 *
 * Единственный файл C++ в приложении: режимы, меню и REST пишутся на C и
 * работают через непрозрачный хэндл (uintptr_t). AutoPID — библиотека
 * Arduino-стиля, привязанная к указателям на input/setpoint/output,
 * поэтому вызывающий код обязан обновить эти переменные до autopid_run().
 */
#include "AutoPID-for-ESP-IDF.h"

#include <cmath>
#include <new>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AUTOPID_MAX_INSTANCES 2

namespace {

struct Slot {
    bool     used;
    double  *input;
    double  *setpoint;
    double  *output;
    AutoPID *pid;
};

Slot s_slots[AUTOPID_MAX_INSTANCES];

AutoPID *find_slot(uintptr_t h) {
    if (h == 0 || h > AUTOPID_MAX_INSTANCES) return nullptr;
    Slot &s = s_slots[h - 1];
    return s.used ? s.pid : nullptr;
}

}  // namespace

extern "C" {

uintptr_t autopid_create(double *input, double *setpoint, double *output,
                         double out_min, double out_max,
                         double kp, double ki, double kd,
                         unsigned long ts_ms) {
    for (int i = 0; i < AUTOPID_MAX_INSTANCES; i++) {
        if (s_slots[i].used) continue;
        AutoPID *pid = new (std::nothrow)
            AutoPID(input, setpoint, output, out_min, out_max, kp, ki, kd);
        if (!pid) return 0;
        pid->setTimeStep(ts_ms);
        s_slots[i].used = true;
        s_slots[i].input = input;
        s_slots[i].setpoint = setpoint;
        s_slots[i].output = output;
        s_slots[i].pid = pid;
        return static_cast<uintptr_t>(i + 1);
    }
    return 0;
}

void autopid_destroy(uintptr_t h) {
    if (h == 0 || h > AUTOPID_MAX_INSTANCES) return;
    Slot &s = s_slots[h - 1];
    delete s.pid;
    s.pid = nullptr;
    s.used = false;
}

void autopid_set_gains(uintptr_t h, double kp, double ki, double kd) {
    AutoPID *pid = find_slot(h);
    if (pid) pid->setGains(kp, ki, kd);
}

void autopid_set_setpoint(uintptr_t h, double sp) {
    if (h == 0 || h > AUTOPID_MAX_INSTANCES) return;
    if (s_slots[h - 1].setpoint) *s_slots[h - 1].setpoint = sp;
}

void autopid_set_output_range(uintptr_t h, double lo, double hi) {
    AutoPID *pid = find_slot(h);
    if (pid) pid->setOutputRange(lo, hi);
}

void autopid_set_timestep(uintptr_t h, unsigned long ms) {
    AutoPID *pid = find_slot(h);
    if (pid) pid->setTimeStep(ms);
}

void autopid_run(uintptr_t h) {
    AutoPID *pid = find_slot(h);
    if (pid) pid->run();
}

double autopid_get_integral(uintptr_t h) {
    AutoPID *pid = find_slot(h);
    return pid ? pid->getIntegral() : 0.0;
}

void autopid_set_integral(uintptr_t h, double integral) {
    AutoPID *pid = find_slot(h);
    if (pid) pid->setIntegral(integral);
}

void autopid_reset(uintptr_t h) {
    AutoPID *pid = find_slot(h);
    if (pid) pid->reset();
}

bool autopid_available(void) { return true; }

}  // extern "C"
