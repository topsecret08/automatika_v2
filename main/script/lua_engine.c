#include "lua_engine.h"
#include "config.h"

#include "core/control_config.h"
#include "core/estop.h"
#include "core/events.h"
#include "core/state.h"
#include "core/watchdog.h"
#include "control/ctrl_heater.h"
#include "control/safety_gate.h"
#include "control/ctrl_safety.h"
#include "control/ctrl_dose.h"
#include "hal/hal_1wire.h"

#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "LUA";

#define LUA_QUEUE_LEN        4
#define LUA_SCRIPT_TIMEOUT_US (15LL * 1000000LL)

typedef struct {
    char path[LUA_SCRIPT_PATH_MAX];
    bool run_now;
} lua_job_t;

static QueueHandle_t s_queue = NULL;
static TaskHandle_t  s_task = NULL;
static lua_State    *L = NULL;

static volatile bool s_enabled = false;
static volatile bool s_busy = false;
static char          s_status[32] = "idle";
static char          s_error[160] = "";
static char          s_script[LUA_SCRIPT_PATH_MAX] = "";

// ------------------------------------------------------------------
// Ограничение ресурсов: крючок инструкций.
// ------------------------------------------------------------------
typedef struct {
    int64_t  start_us;
    uint32_t budget;    // инструкций
    uint32_t used;
    bool     aborted;
} lua_limits_t;

static lua_limits_t s_limits;

// ------------------------------------------------------------------
// Аллокатор с жёстким лимитом: Lua не может выйти за LUA_MEM_LIMIT_BYTES.
// ------------------------------------------------------------------
typedef struct {
    size_t used;
    size_t limit;
} lua_mem_t;

static lua_mem_t s_lua_mem = { 0, LUA_MEM_LIMIT_BYTES };

static void *lua_limited_alloc(void *ud, void *ptr, size_t osize, size_t nsize) {
    lua_mem_t *m = (lua_mem_t *)ud;
    if (!m) return NULL;

    if (nsize == 0) {
        if (ptr) {
            free(ptr);
            m->used = (osize <= m->used) ? m->used - osize : 0;
        }
        return NULL;
    }

    size_t base = ptr ? osize : 0;
    if (nsize > base && m->used + (nsize - base) > m->limit) {
        ESP_LOGE(TAG, "Lua memory limit %u KB exceeded",
                 (unsigned)(m->limit / 1024));
        return NULL;   // Lua корректно обработает нехватку памяти
    }

    void *np = realloc(ptr, nsize);
    if (!np) return NULL;

    m->used = m->used - base + nsize;
    return np;
}

static void lua_hook(lua_State *l, lua_Debug *ar) {
    (void)ar;
    s_limits.used++;
    if (s_limits.used > s_limits.budget ||
        esp_timer_get_time() - s_limits.start_us > LUA_SCRIPT_TIMEOUT_US) {
        s_limits.aborted = true;
        luaL_error(l, "script limit exceeded");
    }
}

static void start_limits(lua_State *l) {
    s_limits.start_us = esp_timer_get_time();
    s_limits.used = 0;
    s_limits.aborted = false;
    s_limits.budget = 200000;
    lua_sethook(l, lua_hook, LUA_MASKCOUNT, 1000);
}

static void stop_limits(lua_State *l) {
    lua_sethook(l, NULL, 0, 0);
}

// ------------------------------------------------------------------
// Белый список функций Lua для автоматики.
// ------------------------------------------------------------------
static int guard_ok(void) {
    return ctrl_safety_ok() && !estop_is_pressed();
}

static int l_set_percent(lua_State *l) {
    double pct = luaL_checknumber(l, 1);
    if (!guard_ok()) {
        lua_pushboolean(l, 0);
        lua_pushstring(l, "safety/estop");
        return 2;
    }
    safety_gate_apply("lua_power", (float)pct);
    lua_pushboolean(l, 1);
    return 1;
}

static int l_get_temp(lua_State *l) {
    int idx = (int)luaL_optinteger(l, 1, 0);
    lua_pushnumber(l, hal_1wire_read(idx));
    return 1;
}

static int l_get_rising(lua_State *l) {
    int idx = (int)luaL_optinteger(l, 1, 0);
    lua_pushboolean(l, hal_1wire_rising(idx));
    return 1;
}

static int l_get_power(lua_State *l) {
    lua_pushnumber(l, ctrl_heater_get_percent());
    return 1;
}

static int l_dose(lua_State *l) {
    double ml = luaL_checknumber(l, 1);
    double rate = luaL_optnumber(l, 2, (lua_Number)control_config_get_rate_ml_min());
    if (!guard_ok()) {
        lua_pushboolean(l, 0);
        return 1;
    }
    lua_pushboolean(l, ctrl_dose_volume((float)ml, (float)rate));
    return 1;
}

static int l_log(lua_State *l) {
    const char *msg = luaL_checkstring(l, 1);
    events_log(EVENT_USER_ACTION, "LUA: %s", msg);
    return 0;
}

static int l_delay(lua_State *l) {
    int ms = (int)luaL_checkinteger(l, 1);
    if (ms < 0) ms = 0;
    if (ms > 5000) ms = 5000;   // задача скрипта не может быть заблокирована надолго
    vTaskDelay(pdMS_TO_TICKS(ms));
    return 0;
}

static int l_millis(lua_State *l) {
    lua_pushinteger(l, (lua_Integer)(esp_timer_get_time() / 1000));
    return 1;
}

static int l_state(lua_State *l) {
    lua_pushinteger(l, (int)state_get()->is_running);
    lua_pushinteger(l, (int)state_get()->is_paused);
    lua_pushinteger(l, (int)state_get()->emergency);
    return 3;
}

static const luaL_Reg AUTOMATIKA_API[] = {
    { "set_percent", l_set_percent },
    { "get_temp",    l_get_temp },
    { "get_rising",  l_get_rising },
    { "get_power",   l_get_power },
    { "dose",        l_dose },
    { "log",         l_log },
    { "delay",       l_delay },
    { "millis",      l_millis },
    { "state",       l_state },
    { NULL, NULL },
};

static void open_libs(lua_State *l) {
    /* Намеренно без luaL_openlibs(): io/os/package/debug не нужны и
     * потенциально опасны. */
    static const luaL_Reg libs[] = {
        { "_G",       luaopen_base },
        { "math",     luaopen_math },
        { "string",   luaopen_string },
        { "table",    luaopen_table },
        { NULL, NULL },
    };
    for (const luaL_Reg *lib = libs; lib->func; lib++) {
        luaL_requiref(l, lib->name, lib->func, 1);
        lua_pop(l, 1);
    }

    luaL_newlib(l, AUTOMATIKA_API);
    lua_setglobal(l, "automatika");
}

// ------------------------------------------------------------------
// Путь скрипта: только /sdcard/scripts/... и /lfs/scripts/...
// ------------------------------------------------------------------
static bool path_allowed(const char *path) {
    if (!path || !path[0]) return false;
    if (strlen(path) >= LUA_SCRIPT_PATH_MAX) return false;
    if (strstr(path, "..")) return false;
    if (strncmp(path, PATH_SCRIPTS_SD "/", strlen(PATH_SCRIPTS_SD) + 1) == 0)
        return true;
    if (strncmp(path, PATH_SCRIPTS_FLASH "/", strlen(PATH_SCRIPTS_FLASH) + 1) == 0)
        return true;
    return false;
}

static void set_error(lua_run_result_t r, const char *msg) {
    snprintf(s_error, sizeof(s_error), "%s", msg ? msg : "");
    strncpy(s_status, lua_run_result_name(r), sizeof(s_status) - 1);
    s_status[sizeof(s_status) - 1] = '\0';
}

lua_run_result_t run_file(const char *path) {
    if (!L) return LUA_RUN_ERROR;
    if (!path_allowed(path)) {
        set_error(LUA_RUN_ERROR, "path not allowed");
        return LUA_RUN_ERROR;
    }

    FILE *f = fopen(path, "rb");
    if (!f) {
        set_error(LUA_RUN_NO_FILE, path);
        return LUA_RUN_NO_FILE;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > LUA_SCRIPT_MAX_BYTES) {
        fclose(f);
        set_error(LUA_RUN_TOO_BIG, "script size");
        return LUA_RUN_TOO_BIG;
    }

    /* делим память: читаем через PSRAM, чтобы не выедать внутреннюю RAM */
    char *src = heap_caps_malloc((size_t)sz + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!src) src = malloc((size_t)sz + 1);
    if (!src) {
        fclose(f);
        set_error(LUA_RUN_ERROR, "no memory for script");
        return LUA_RUN_ERROR;
    }
    size_t got = fread(src, 1, (size_t)sz, f);
    fclose(f);
    src[got] = '\0';

    strncpy(s_status, "running", sizeof(s_status) - 1);
    int rc = luaL_loadbuffer(L, src, got, path);
    free(src);

    if (rc != LUA_OK) {
        set_error(LUA_RUN_SYNTAX, lua_tostring(L, -1));
        lua_pop(L, 1);
        return LUA_RUN_SYNTAX;
    }

    start_limits(L);
    rc = lua_pcall(L, 0, 0, 0);
    stop_limits(L);

    if (rc != LUA_OK) {
        const char *err = lua_tostring(L, -1);
        lua_run_result_t res = s_limits.aborted ? LUA_RUN_LIMIT : LUA_RUN_SYNTAX;
        ESP_LOGE(TAG, "script error: %s", err ? err : "?");
        set_error(res, err);
        lua_pop(L, 1);
        events_log(EVENT_WARNING, "Lua %s: %s", path, s_error);
        return res;
    }

    lua_gc(L, LUA_GCCOLLECT, 0);
    s_status[0] = '\0';
    strncpy(s_status, "ok", sizeof(s_status) - 1);
    s_error[0] = '\0';
    return LUA_RUN_OK;
}

// ------------------------------------------------------------------

lua_run_result_t lua_engine_request(const char *script_path) {
    if (!s_queue) return LUA_RUN_ERROR;
    if (!s_enabled) return LUA_RUN_NOT_RUNNING;

    lua_job_t job = {0};
    if (script_path && script_path[0]) {
        if (!path_allowed(script_path)) {
            set_error(LUA_RUN_ERROR, "path not allowed");
            return LUA_RUN_ERROR;
        }
        strncpy(job.path, script_path, sizeof(job.path) - 1);
    }

    if (xQueueSend(s_queue, &job, pdMS_TO_TICKS(50)) != pdTRUE) {
        set_error(LUA_RUN_BUSY, "queue full");
        return LUA_RUN_BUSY;
    }
    return LUA_RUN_OK;
}

lua_run_result_t lua_engine_run_script(const char *path) {
    return lua_engine_request(path);
}

esp_err_t lua_engine_set_script(const char *path) {
    if (path && path[0] && !path_allowed(path)) return ESP_ERR_INVALID_ARG;
    strncpy(s_script, path ? path : "", sizeof(s_script) - 1);
    s_script[sizeof(s_script) - 1] = '\0';
    return ESP_OK;
}

void lua_engine_set_enabled(bool on) {
    if (on && !s_task) {
        events_log(EVENT_WARNING, "Lua: engine not initialised");
        return;
    }
    s_enabled = on;
    events_log(EVENT_INFO, "Lua engine %s", on ? "enabled" : "disabled");
}

bool lua_engine_enabled(void) { return s_enabled; }
bool lua_engine_ready(void) { return s_task != NULL; }

const char *lua_engine_status(void) { return s_status; }
const char *lua_engine_last_error(void) { return s_error; }

const char *lua_run_result_name(lua_run_result_t r) {
    switch (r) {
        case LUA_RUN_OK:          return "ok";
        case LUA_RUN_BUSY:        return "busy";
        case LUA_RUN_NOT_RUNNING: return "disabled";
        case LUA_RUN_NO_FILE:     return "no file";
        case LUA_RUN_TOO_BIG:     return "too big";
        case LUA_RUN_SYNTAX:      return "error";
        case LUA_RUN_LIMIT:       return "limit";
        case LUA_RUN_TIMEOUT:     return "timeout";
        default:                  return "fail";
    }
}

// Обработчик пустой задачи — держит Lua-стек выделенным.
static void lua_engine_task(void *arg) {
    (void)arg;
    wd_register("lua", 60000, false);

    lua_job_t job;
    while (1) {
        WD_FEED("lua");

        if (xQueueReceive(s_queue, &job, pdMS_TO_TICKS(1000)) != pdTRUE) {
            continue;
        }

        if (!s_enabled) continue;

        s_busy = true;
        const char *path = job.path[0] ? job.path : s_script;
        if (path[0]) {
            lua_run_result_t r = run_file(path);
            events_log(r == LUA_RUN_OK ? EVENT_INFO : EVENT_WARNING,
                       "Lua run %s → %s", path, lua_run_result_name(r));
        }
        s_busy = false;
    }
}

esp_err_t lua_engine_init(void) {
    if (s_task) return ESP_OK;

    s_queue = xQueueCreate(LUA_QUEUE_LEN, sizeof(lua_job_t));
    if (!s_queue) return ESP_ERR_NO_MEM;

    s_lua_mem.used = 0;
    s_lua_mem.limit = LUA_MEM_LIMIT_BYTES;
    L = lua_newstate(lua_limited_alloc, &s_lua_mem, 0);
    if (!L) {
        ESP_LOGE(TAG, "lua_newstate failed (limit %u KB, heap=%u KB)",
                 (unsigned)(LUA_MEM_LIMIT_BYTES / 1024),
                 (unsigned)(heap_caps_get_free_size(MALLOC_CAP_DEFAULT) / 1024));
        vQueueDelete(s_queue);
        s_queue = NULL;
        return ESP_ERR_NO_MEM;
    }
    open_libs(L);

    // Задача на ядре 0 (сеть/скрипты), стек побольше: Lua VM рекурсивен.
    BaseType_t ok = xTaskCreatePinnedToCore(lua_engine_task, "lua", 12288,
                                            NULL, 2, &s_task, 0);
    if (ok != pdPASS) {
        s_task = NULL;
        lua_close(L);
        L = NULL;
        vQueueDelete(s_queue);
        s_queue = NULL;
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Lua %s ready (path %s)", LUA_RELEASE, PATH_SCRIPTS_SD);
    events_log(EVENT_INFO, "Lua engine initialised (%s)", LUA_RELEASE);
    return ESP_OK;
}
