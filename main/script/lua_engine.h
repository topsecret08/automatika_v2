#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

// Lua-движок для пользовательских сценариев.
//
// Безопасность:
//   * скрипты выполняются в ОТДЕЛЬНОЙ задаче (ядро 0) со своим стеком,
//     без доступа к SD/сети;
//   * лимиты: количество инструкций и время хука (LUA_SCRIPT_MAX_BYTES,
//     LUA_HOOK_MIN_PERIOD_MS), вызовы нагрева/отбора проходят через те же
//     инварианты, что и REST API (ctrl_heater_set_percent, ctrl_safety_ok,
//     E-STOP);
//   * доступен только белый список функций — нет luaL_openlibs() (io, os,
//     package, debug не открываются);
//   * путь ограничен /sdcard/scripts и /lfs/scripts; размер файла —
//     LUA_SCRIPT_MAX_BYTES.
//
// E-STOP обязателен для включения Lua-режима (см. estop.h).

#define LUA_SCRIPT_PATH_MAX 128

typedef enum {
    LUA_RUN_OK = 0,
    LUA_RUN_BUSY,        // движок занят
    LUA_RUN_NOT_RUNNING, // движок не разрешён
    LUA_RUN_NO_FILE,
    LUA_RUN_TOO_BIG,
    LUA_RUN_SYNTAX,      // ошибка компиляции/выполнения
    LUA_RUN_LIMIT,       // превышен лимит инструкций/времени
    LUA_RUN_TIMEOUT,
    LUA_RUN_ERROR,
} lua_run_result_t;

// Запуск диспетчера (создаёт очередь и задачу на ядре 0).
esp_err_t lua_engine_init(void);
// Кодирует путь (см. lua_engine_run_script). Пустой путь допустим:
// значит, скрипт будет задан через lua_engine_set_script().
esp_err_t lua_engine_set_script(const char *path);
void      lua_engine_set_enabled(bool on);
bool      lua_engine_enabled(void);

// Постановка в очередь. path = NULL — повторить текущий скрипт.
// Возвращает результат постановки (сам запуск — асинхронный).
lua_run_result_t lua_engine_run_script(const char *path);

// Выполнить строку (ограничение 4 КБ) — используется диспетчером и
// REST-эндпоинтом /api/script/run.
lua_run_result_t lua_engine_request(const char *script_path);

// Состояние (для REST/логгеров).
const char *lua_engine_status(void);
const char *lua_engine_last_error(void);
bool lua_engine_ready(void);
const char *lua_run_result_name(lua_run_result_t r);
