#include "session_recovery.h"
#include "config.h"
#include "state.h"
#include "events.h"
#include "watchdog.h"

#include "net/time_sync.h"

#include "esp_log.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>

static const char *TAG = "SESSION";

static nvs_handle_t s_nvs;
static bool s_nvs_ok = false;

esp_err_t session_recovery_init(void) {
    if (s_nvs_ok) return ESP_OK;
    s_nvs_ok = (nvs_open("automatika", NVS_READWRITE, &s_nvs) == ESP_OK);
    if (!s_nvs_ok) {
        ESP_LOGW(TAG, "NVS open failed");
        return ESP_FAIL;
    }
    return ESP_OK;
}

static void snapshot_from_state(session_snapshot_t *s) {
    process_state_t *st = state_get();
    memset(s, 0, sizeof(*s));
    s->magic = SESSION_MAGIC;
    s->setpoint_cube = st->target_cube_t;
    s->setpoint_column = st->target_column_t;
    s->heads_ml = st->heads_collected_ml;
    s->body_ml = st->body_collected_ml;
    s->tails_ml = st->tails_collected_ml;
    s->volume_done_ml = st->heads_collected_ml + st->body_collected_ml +
                        st->tails_collected_ml;
    s->mode = (int32_t)st->type;
    s->stage = (int32_t)st->stage;
    s->fraction = (int32_t)st->stage;   /* совместимо с прежними сборками */
    s->saved_at = time_is_valid() ? time_now_unix() : 0;
}

esp_err_t session_save_now(void) {
    if (!s_nvs_ok) return ESP_ERR_INVALID_STATE;

    session_snapshot_t s;
    snapshot_from_state(&s);

    esp_err_t err = nvs_set_blob(s_nvs, "session", &s, sizeof(s));
    if (err == ESP_OK) err = nvs_commit(s_nvs);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "save failed: %s", esp_err_to_name(err));
    }
    return err;
}

bool session_load(session_snapshot_t *s) {
    if (!s_nvs_ok || !s) return false;

    session_snapshot_t tmp;
    size_t len = sizeof(tmp);
    if (nvs_get_blob(s_nvs, "session", &tmp, &len) != ESP_OK) return false;
    if (len != sizeof(tmp) || tmp.magic != SESSION_MAGIC) return false;

    // Проверка возраста возможна только при валидных часах. Если SNTP ещё
    // не синхронизирован, снимок принимается: лучше предложить продолжение,
    // чем потерять данные сессии.
    if (tmp.saved_at > 0 && time_is_valid()) {
        int64_t age = time_now_unix() - tmp.saved_at;
        if (age < 0 || age > SESSION_VALID_SEC) {
            ESP_LOGI(TAG, "snapshot too old (%lld s)", (long long)age);
            return false;
        }
    }

    *s = tmp;
    return true;
}

void session_clear(void) {
    if (!s_nvs_ok) return;
    nvs_erase_key(s_nvs, "session");
    nvs_commit(s_nvs);
}

bool session_has_valid(void) {
    session_snapshot_t s;
    return session_load(&s);
}

void session_recovery_task(void *arg) {
    (void)arg;
    wd_register("session", SESSION_AUTOSAVE_MS * 3, false);

    bool restored_announced = false;

    while (1) {
        WD_FEED("session");

        process_state_t *st = state_get();

        if (st->is_running) {
            session_save_now();
        } else if (!restored_announced) {
            // Один раз сообщаем о найденном снимке: UI покажет «Продолжить?».
            session_snapshot_t s;
            if (session_load(&s)) {
                restored_announced = true;
                events_log(EVENT_INFO,
                           "Session snapshot available: %.0f ml, stage %d",
                           s.volume_done_ml, (int)s.stage);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(SESSION_AUTOSAVE_MS));
    }
}
