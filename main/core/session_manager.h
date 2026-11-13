#pragma once

#include <stdbool.h>
#include "esp_err.h"

// Состояние сессии — отдельный слой от UI.
//
// Процесс живёт в своих задачах и не зависит от того, какой экран открыт.
// SESSION_RUNNING_VISIBLE  — процесс идёт, пользователь на главном экране;
// SESSION_RUNNING_MINIMIZED — процесс идёт, пользователь ушёл на другой экран;
// SESSION_PAUSED            — процесс на паузе.
//
// session_start()/stop() — обёртки над process_control; process_control сам
// сообщает сюда о старте/остановке, поэтому прямые вызовы из REST/Telegram
// тоже корректно отражаются в состоянии сессии.

typedef enum {
    SESSION_IDLE = 0,
    SESSION_RUNNING_VISIBLE,
    SESSION_RUNNING_MINIMIZED,
    SESSION_PAUSED,
} session_state_t;

esp_err_t session_init(void);
esp_err_t session_start(const char *mode_id);
void      session_stop(void);
void      session_minimize(void);
void      session_restore(void);
void      session_note_process_running(bool running);

session_state_t session_get_state(void);
bool            session_is_running(void);
bool            session_is_visible(void);
bool            session_is_paused(void);
const char     *session_state_name(session_state_t state);
