#include "session_manager.h"

#include "core/process_control.h"
#include "core/state.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "SESSION";

static session_state_t s_state = SESSION_IDLE;
static SemaphoreHandle_t s_mutex = NULL;

static void lock(void) {
    if (s_mutex) xSemaphoreTake(s_mutex, portMAX_DELAY);
}

static void unlock(void) {
    if (s_mutex) xSemaphoreGive(s_mutex);
}

// Привести сохранённое состояние к фактическому состоянию процесса.
static void sync_locked(void) {
    process_state_t *st = state_get();
    if (!st->is_running) {
        s_state = SESSION_IDLE;
        return;
    }
    if (st->is_paused) {
        s_state = SESSION_PAUSED;
        return;
    }
    if (s_state != SESSION_RUNNING_VISIBLE &&
        s_state != SESSION_RUNNING_MINIMIZED) {
        s_state = SESSION_RUNNING_VISIBLE;
    }
}

esp_err_t session_init(void) {
    if (!s_mutex) {
        s_mutex = xSemaphoreCreateMutex();
        if (!s_mutex) return ESP_ERR_NO_MEM;
    }
    s_state = SESSION_IDLE;
    ESP_LOGI(TAG, "init");
    return ESP_OK;
}

esp_err_t session_start(const char *mode_id) {
    esp_err_t err = process_control_start(mode_id);
    if (err != ESP_OK) return err;

    lock();
    s_state = SESSION_RUNNING_VISIBLE;
    unlock();
    return ESP_OK;
}

void session_stop(void) {
    process_control_stop();
    lock();
    s_state = SESSION_IDLE;
    unlock();
}

void session_minimize(void) {
    lock();
    sync_locked();
    if (s_state == SESSION_RUNNING_VISIBLE) {
        s_state = SESSION_RUNNING_MINIMIZED;
    }
    unlock();
}

void session_restore(void) {
    lock();
    sync_locked();
    if (s_state == SESSION_RUNNING_MINIMIZED) {
        s_state = SESSION_RUNNING_VISIBLE;
    }
    unlock();
}

void session_note_process_running(bool running) {
    lock();
    if (running) {
        if (s_state == SESSION_IDLE) s_state = SESSION_RUNNING_VISIBLE;
        sync_locked();
    } else {
        s_state = SESSION_IDLE;
    }
    unlock();
}

session_state_t session_get_state(void) {
    lock();
    sync_locked();
    session_state_t state = s_state;
    unlock();
    return state;
}

bool session_is_running(void) {
    session_state_t state = session_get_state();
    return state == SESSION_RUNNING_VISIBLE ||
           state == SESSION_RUNNING_MINIMIZED;
}

bool session_is_visible(void) {
    return session_get_state() == SESSION_RUNNING_VISIBLE;
}

bool session_is_paused(void) {
    return session_get_state() == SESSION_PAUSED;
}

const char *session_state_name(session_state_t state) {
    switch (state) {
    case SESSION_RUNNING_VISIBLE:   return "running";
    case SESSION_RUNNING_MINIMIZED: return "minimized";
    case SESSION_PAUSED:            return "paused";
    case SESSION_IDLE:
    default:                        return "idle";
    }
}
