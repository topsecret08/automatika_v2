#include "verify_db.h"

#include "db/db_manager.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "sqlite3.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "VERIFY_DB";
#define VERIFY_DB_PATH "/sdcard/automatika.db"

static sqlite3 *s_db;
static SemaphoreHandle_t s_mtx;
static bool s_ready;

static const char *SCHEMA =
    "CREATE TABLE IF NOT EXISTS verify_runs("
    " run_id INTEGER PRIMARY KEY AUTOINCREMENT,"
    " started_at_ms INTEGER NOT NULL,"
    " mode TEXT, policy INTEGER, stage INTEGER,"
    " result_metrics_json TEXT);"
    "CREATE TABLE IF NOT EXISTS verify_trace("
    " id INTEGER PRIMARY KEY AUTOINCREMENT,"
    " run_id INTEGER NOT NULL,"
    " ts_ms INTEGER NOT NULL,"
    " channel TEXT, setpoint REAL,"
    " sensor REAL, value_real REAL, value_shadow REAL, stage INTEGER);"
    "CREATE INDEX IF NOT EXISTS idx_vtrace_run_ts"
    " ON verify_trace(run_id, ts_ms);";

esp_err_t verify_db_init(void) {
    if (s_ready) return ESP_OK;
    if (!db_is_ready()) return ESP_ERR_INVALID_STATE;

    s_mtx = xSemaphoreCreateMutex();
    if (!s_mtx) return ESP_ERR_NO_MEM;

    int rc = sqlite3_open_v2(VERIFY_DB_PATH, &s_db,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
        NULL);
    if (rc != SQLITE_OK) {
        ESP_LOGE(TAG, "open: %s", sqlite3_errmsg(s_db));
        if (s_db) sqlite3_close(s_db);
        s_db = NULL;
        return ESP_FAIL;
    }

    char *err = NULL;
    rc = sqlite3_exec(s_db, SCHEMA, NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        ESP_LOGE(TAG, "schema: %s", err ? err : "?");
        if (err) sqlite3_free(err);
        sqlite3_close(s_db);
        s_db = NULL;
        return ESP_FAIL;
    }
    // Мягкая миграция: поле sensor появилось после первой версии таблицы.
    sqlite3_exec(s_db, "ALTER TABLE verify_trace ADD COLUMN sensor REAL;",
                 NULL, NULL, NULL);

    s_ready = true;
    ESP_LOGI(TAG, "ready");
    return ESP_OK;
}

bool verify_db_is_ready(void) { return s_ready; }

int64_t verify_db_start_run(const char *mode, int policy) {
    if (!s_ready) return 0;

    xSemaphoreTake(s_mtx, portMAX_DELAY);
    sqlite3_stmt *st = NULL;
    int64_t id = 0;
    const char *sql =
        "INSERT INTO verify_runs(started_at_ms, mode, policy, stage)"
        " VALUES(?,?,?,0)";
    if (sqlite3_prepare_v2(s_db, sql, -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(st, 1, esp_timer_get_time() / 1000);
        sqlite3_bind_text(st, 2, mode ? mode : "manual", -1, SQLITE_STATIC);
        sqlite3_bind_int(st, 3, policy);
        if (sqlite3_step(st) == SQLITE_DONE) id = sqlite3_last_insert_rowid(s_db);
    }
    sqlite3_finalize(st);
    xSemaphoreGive(s_mtx);
    return id;
}

esp_err_t verify_db_log_trace(const verify_trace_t *t) {
    if (!s_ready || !t) return ESP_ERR_INVALID_STATE;

    xSemaphoreTake(s_mtx, portMAX_DELAY);
    sqlite3_stmt *st = NULL;
    const char *sql =
        "INSERT INTO verify_trace(run_id,ts_ms,channel,setpoint,sensor,"
        "value_real,value_shadow,stage) VALUES(?,?,?,?,?,?,?,?)";
    esp_err_t err = ESP_FAIL;
    if (sqlite3_prepare_v2(s_db, sql, -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(st, 1, t->run_id);
        sqlite3_bind_int64(st, 2, t->ts_ms);
        sqlite3_bind_text(st, 3, t->channel, -1, SQLITE_STATIC);
        sqlite3_bind_double(st, 4, t->setpoint);
        sqlite3_bind_double(st, 5, t->sensor);
        sqlite3_bind_double(st, 6, t->value_real);
        sqlite3_bind_double(st, 7, t->value_shadow);
        sqlite3_bind_int(st, 8, t->stage);
        err = (sqlite3_step(st) == SQLITE_DONE) ? ESP_OK : ESP_FAIL;
    }
    sqlite3_finalize(st);
    xSemaphoreGive(s_mtx);
    return err;
}

esp_err_t verify_db_finish_run(int64_t run_id, const char *metrics_json) {
    if (!s_ready || run_id <= 0) return ESP_ERR_INVALID_STATE;

    xSemaphoreTake(s_mtx, portMAX_DELAY);
    sqlite3_stmt *st = NULL;
    const char *sql =
        "UPDATE verify_runs SET result_metrics_json=? WHERE run_id=?";
    esp_err_t err = ESP_FAIL;
    if (sqlite3_prepare_v2(s_db, sql, -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, metrics_json ? metrics_json : "{}", -1,
                          SQLITE_STATIC);
        sqlite3_bind_int64(st, 2, run_id);
        err = (sqlite3_step(st) == SQLITE_DONE) ? ESP_OK : ESP_FAIL;
    }
    sqlite3_finalize(st);
    xSemaphoreGive(s_mtx);
    return err;
}

esp_err_t verify_db_replay(int64_t run_id, float (*algo)(float, float),
                           float *max_err_out, int *rows_out) {
    if (!s_ready || !algo || run_id <= 0) return ESP_ERR_INVALID_ARG;
    if (max_err_out) *max_err_out = 0.0f;
    if (rows_out) *rows_out = 0;

    xSemaphoreTake(s_mtx, portMAX_DELAY);
    sqlite3_stmt *st = NULL;
    const char *sql =
        "SELECT setpoint,sensor,value_shadow FROM verify_trace"
        " WHERE run_id=? ORDER BY ts_ms";
    float max_err = 0.0f;
    int rows = 0;

    if (sqlite3_prepare_v2(s_db, sql, -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(st, 1, run_id);
        while (sqlite3_step(st) == SQLITE_ROW) {
            float sp = (float)sqlite3_column_double(st, 0);
            float sensor = (float)sqlite3_column_double(st, 1);
            float shadow = (float)sqlite3_column_double(st, 2);
            float recalc = algo(sp, sensor);
            float err = fabsf(recalc - shadow);
            if (err > max_err) max_err = err;
            rows++;
        }
    }
    sqlite3_finalize(st);
    xSemaphoreGive(s_mtx);

    if (max_err_out) *max_err_out = max_err;
    if (rows_out) *rows_out = rows;
    return (rows > 0 && max_err < 1e-3f) ? ESP_OK : ESP_FAIL;
}

int verify_db_run_count(void) {
    if (!s_ready) return 0;
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    sqlite3_stmt *st = NULL;
    int count = 0;
    if (sqlite3_prepare_v2(s_db, "SELECT COUNT(*) FROM verify_runs", -1,
                           &st, NULL) == SQLITE_OK) {
        if (sqlite3_step(st) == SQLITE_ROW) count = sqlite3_column_int(st, 0);
    }
    sqlite3_finalize(st);
    xSemaphoreGive(s_mtx);
    return count;
}
