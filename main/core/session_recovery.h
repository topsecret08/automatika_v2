#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

// Восстановление сессии после потери питания.
//
// Снимок пишется в NVS ("automatika"/"session") раз в 30 с во время работы.
// При загрузке:
//   * session_load() возвращает true только если снимок не старше 30 минут;
//   * восстановление НЕ включает нагрев: is_running=false, is_paused=true;
//     пользователь подтверждает продолжение (UI/Telegram/API), после чего
//     обычный /api/start снова проходит все проверки безопасности.

#define SESSION_MAGIC       0x53455331u   /* "SES1" */
#define SESSION_VALID_SEC   1800          /* 30 минут */
#define SESSION_AUTOSAVE_MS 30000u

typedef struct {
    uint32_t magic;
    float    setpoint_cube;
    float    setpoint_column;
    float    volume_done_ml;      /* суммарно отобрано */
    float    heads_ml;
    float    body_ml;
    float    tails_ml;
    int32_t  mode;
    int32_t  stage;
    int32_t  fraction;
    int64_t  saved_at;            /* Unix time, 0 если время недостоверно */
} session_snapshot_t;

esp_err_t session_recovery_init(void);
void      session_recovery_task(void *arg);

// Сохранить текущее состояние (вызывается задачей и по /api/stop).
esp_err_t session_save_now(void);

bool session_load(session_snapshot_t *s);
void session_clear(void);

// Есть ли валидный снимок (для UI «Продолжить?»).
bool session_has_valid(void);
