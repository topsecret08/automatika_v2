#include "state.h"
#include "nvs.h"
#include "events.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>

static const char *TAG = "STATE";
static process_state_t s;
static nvs_handle_t nvs;
static int64_t last_save_us = 0;

void state_init(void) {
    nvs_open("app_state", NVS_READWRITE, &nvs);
    memset(&s, 0, sizeof(s));
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

bool state_load(void) {
    size_t sz = sizeof(s);
    esp_err_t err = nvs_get_blob(nvs, "proc", &s, &sz);
    if (err != ESP_OK) return false;
    s.is_running = false;
    s.is_paused = true;
    s.interrupted_ago_sec = 0;
    ESP_LOGI(TAG, "Loaded: type=%d stage=%d", s.type, s.stage);
    return true;
}

void state_save(void) {
    nvs_set_blob(nvs, "proc", &s, sizeof(s));
    nvs_commit(nvs);
}

void state_clear(void) {
    nvs_erase_key(nvs, "proc");
    nvs_commit(nvs);
}

bool state_has_saved(void) {
    size_t sz = 0;
    return nvs_get_blob(nvs, "proc", NULL, &sz) == ESP_OK;
}

void state_update_snapshot(float dt) {
    s.elapsed_sec += (uint32_t)dt;
    if (s.is_running) {
        int64_t now = esp_timer_get_time();
        if (now - last_save_us > 30000000) {
            state_save();
            last_save_us = now;
        }
    }
}

const char* state_type_name(process_type_t t) {
    static const char *n[] = {
        "Простой","Дистилляция","Ректификация","Пивоварня","ПВК",
        "НБК","Вакуум","Двойная","Габриэль","Брожение"
    };
    return (t < 10) ? n[t] : "?";
}

const char* state_stage_name(proc_stage_t s) {
    static const char *n[] = {
        "Простой","Нагрев","Стабилизация","Головы","Тело",
        "Хвосты","Охлаждение","Готово","Проверка","Разогрев",
        "Работа","Ошибка"
    };
    return (s < 12) ? n[s] : "?";
}