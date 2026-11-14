#include "state.h"
#include "nvs.h"
#include "events.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <math.h>
#include <string.h>

static const char *TAG = "STATE";
static process_state_t s;
static nvs_handle_t nvs;
static int64_t last_save_us = 0;
static float s_elapsed_frac = 0.0f;
static SemaphoreHandle_t s_mtx = NULL;

static void state_lock(void) {
    if (s_mtx) xSemaphoreTake(s_mtx, portMAX_DELAY);
}

static void state_unlock(void) {
    if (s_mtx) xSemaphoreGive(s_mtx);
}

void state_init(void) {
    if (!s_mtx) s_mtx = xSemaphoreCreateMutex();
    nvs_open("app_state", NVS_READWRITE, &nvs);
    memset(&s, 0, sizeof(s));
    s_elapsed_frac = 0.0f;
    s.type = PROCESS_IDLE;
    s.stage = ST_IDLE;
    s.target_cube_t = 98.0f;
    s.target_column_t = 77.0f;
    s.body_rate_ml_min = 30.0f;
    s.target_heads_ml = 200.0f;
    s.tail_stop_temp = 98.0f;
    s.predec_distance = 0.10f;
    s.predec_step = 0.10f;
    s.pid_kp = 15.0f; s.pid_ki = 0.5f; s.pid_kd = 2.0f;
}

process_state_t* state_get(void) { return &s; }

void state_snapshot(process_state_t *out) {
    if (!out) return;
    state_lock();
    memcpy(out, &s, sizeof(*out));
    state_unlock();
}

bool state_load(void) {
    process_state_t tmp;
    size_t sz = sizeof(tmp);
    memset(&tmp, 0, sizeof(tmp));
    esp_err_t err = nvs_get_blob(nvs, "proc", &tmp, &sz);
    if (err != ESP_OK) return false;
    tmp.is_running = false;
    tmp.is_paused = true;
    tmp.interrupted_ago_sec = 0;

    state_lock();
    s = tmp;
    s_elapsed_frac = 0.0f;
    state_unlock();

    ESP_LOGI(TAG, "Loaded: type=%d stage=%d", tmp.type, tmp.stage);
    return true;
}

void state_save(void) {
    process_state_t tmp;
    state_snapshot(&tmp);

    esp_err_t err = nvs_set_blob(nvs, "proc", &tmp, sizeof(tmp));
    if (err == ESP_OK) err = nvs_commit(nvs);
    if (err != ESP_OK)
        ESP_LOGW(TAG, "state save failed: %s", esp_err_to_name(err));
}

void state_clear(void) {
    state_lock();
    s_elapsed_frac = 0.0f;
    state_unlock();
    nvs_erase_key(nvs, "proc");
    nvs_commit(nvs);
}

bool state_has_saved(void) {
    size_t sz = 0;
    return nvs_get_blob(nvs, "proc", NULL, &sz) == ESP_OK;
}

void state_update_snapshot(float dt) {
    if (!isfinite(dt) || dt <= 0.0f) return;

    state_lock();
    s_elapsed_frac += dt;
    uint32_t whole = (uint32_t)s_elapsed_frac;
    if (whole > 0) {
        s.elapsed_sec += whole;
        s_elapsed_frac -= (float)whole;
    }
    bool running = s.is_running;
    state_unlock();

    if (running) {
        int64_t now = esp_timer_get_time();
        if (now - last_save_us > 30000000) {
            state_save();
            last_save_us = now;
        }
    }
}

const char* state_type_name(process_type_t t) {
    static const char *n[] = {
        "Ожидание","Дистилляция","Ректификация","Пивоварение","ПВК",
        "НБК","Вакуум","Двойная","Габриэль","Ферментация"
    };
    return (t < 10) ? n[t] : "?";
}

const char* state_stage_name(proc_stage_t s) {
    static const char *n[] = {
        "Ожидание","Нагрев","Стабилизация","Головы","Тело",
        "Хвосты","Охлаждение","Готово","Проверка","Разгон",
        "Работа","Ошибка"
    };
    return (s < 12) ? n[s] : "?";
}
