#include "db_manager.h"
#include "db_psram_alloc.h"
#include "core/events.h"
#include "core/storage.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "sqlite3.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define DB_PATH "/sdcard/automatika.db"
static const char *TAG = "DB";

static sqlite3 *s_db = NULL;
static SemaphoreHandle_t s_mtx;
static bool s_ready = false;

static const char *SCHEMA =
    "PRAGMA journal_mode=WAL;"
    "PRAGMA synchronous=NORMAL;"
    "PRAGMA temp_store=MEMORY;"
    "PRAGMA cache_size=-8192;"
    "PRAGMA mmap_size=16777216;"
    "PRAGMA page_size=4096;"
    "PRAGMA auto_vacuum=INCREMENTAL;"
    "CREATE TABLE IF NOT EXISTS processes ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  started_ms INTEGER NOT NULL,"
    "  ended_ms INTEGER DEFAULT 0,"
    "  type INTEGER NOT NULL,"
    "  stage_final INTEGER DEFAULT 0,"
    "  cube_start_t REAL, cube_end_t REAL,"
    "  heads_ml REAL, body_ml REAL, tails_ml REAL,"
    "  power_avg REAL, predec_count INTEGER DEFAULT 0,"
    "  duration_sec REAL,"
    "  recipe_id TEXT, notes TEXT, rating INTEGER DEFAULT 0);"
    "CREATE INDEX IF NOT EXISTS idx_proc_started ON processes(started_ms DESC);"
    "CREATE INDEX IF NOT EXISTS idx_proc_type ON processes(type);"
    "CREATE TABLE IF NOT EXISTS measurements ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  process_id INTEGER NOT NULL,"
    "  ts_ms INTEGER NOT NULL,"
    "  cube_temp REAL, column_temp REAL, jacket_temp REAL,"
    "  pressure REAL, power REAL, valve_body REAL, flow REAL);"
    "CREATE INDEX IF NOT EXISTS idx_meas_proc_ts ON measurements(process_id, ts_ms);"
    "CREATE TABLE IF NOT EXISTS events ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  ts_ms INTEGER NOT NULL, type INTEGER NOT NULL, msg TEXT NOT NULL);"
    "CREATE INDEX IF NOT EXISTS idx_evt_ts ON events(ts_ms DESC);"
    "CREATE TABLE IF NOT EXISTS recipes ("
    "  id TEXT PRIMARY KEY, name TEXT, category TEXT, data TEXT,"
    "  created_ms INTEGER, used_count INTEGER DEFAULT 0);"
    "CREATE TABLE IF NOT EXISTS autolearn_history ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  ts_ms INTEGER NOT NULL, threshold_id INTEGER NOT NULL,"
    "  value REAL, reward REAL, note TEXT);";

static int64_t now_ms(void) { return esp_timer_get_time() / 1000; }

static bool exec(const char *sql) {
    char *err = NULL;
    int rc = sqlite3_exec(s_db, sql, NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        ESP_LOGE(TAG, "SQL error: %s", err);
        sqlite3_free(err);
        return false;
    }
    return true;
}

bool db_exec(const char *sql) {
    if (!s_ready) return false;
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    bool ok = exec(sql);
    xSemaphoreGive(s_mtx);
    return ok;
}

bool db_init(void) {
    if (s_ready) return true;
    if (!storage_sd_ready()) return false;

    if (db_psram_alloc_init() != SQLITE_OK) return false;
    sqlite3_initialize();

    s_mtx = xSemaphoreCreateMutex();

    int rc = sqlite3_open_v2(DB_PATH, &s_db,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, NULL);
    if (rc != SQLITE_OK) { s_db = NULL; return false; }

    if (!exec(SCHEMA)) { sqlite3_close(s_db); s_db = NULL; return false; }
    s_ready = true;
    ESP_LOGI(TAG, "SQLite ready in PSRAM: %lld KB",
             (long long)(db_file_size() / 1024));
    events_log(EVENT_INFO, "SQLite initialized");
    return true;
}

void db_close(void) {
    if (!s_db) return;
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    sqlite3_close(s_db);
    s_db = NULL; s_ready = false;
    xSemaphoreGive(s_mtx);
}

bool db_is_ready(void) { return s_ready; }

int64_t db_process_start(uint8_t type, const char *recipe_id) {
    if (!s_ready) return 0;
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    sqlite3_stmt *st;
    sqlite3_prepare_v2(s_db,
        "INSERT INTO processes(started_ms, type, recipe_id) VALUES(?,?,?)",
        -1, &st, NULL);
    sqlite3_bind_int64(st, 1, now_ms());
    sqlite3_bind_int(st, 2, type);
    sqlite3_bind_text(st, 3, recipe_id ? recipe_id : "", -1, SQLITE_STATIC);
    sqlite3_step(st);
    sqlite3_finalize(st);
    int64_t id = sqlite3_last_insert_rowid(s_db);
    xSemaphoreGive(s_mtx);
    return id;
}

bool db_process_update(int64_t id, const db_process_t *d) {
    if (!s_ready) return false;
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    sqlite3_stmt *st;
    sqlite3_prepare_v2(s_db,
        "UPDATE processes SET stage_final=?, cube_start_t=?, cube_end_t=?,"
        "heads_ml=?, body_ml=?, tails_ml=?, power_avg=?, predec_count=? WHERE id=?",
        -1, &st, NULL);
    sqlite3_bind_int(st, 1, d->stage_final);
    sqlite3_bind_double(st, 2, d->cube_start_t);
    sqlite3_bind_double(st, 3, d->cube_end_t);
    sqlite3_bind_double(st, 4, d->heads_ml);
    sqlite3_bind_double(st, 5, d->body_ml);
    sqlite3_bind_double(st, 6, d->tails_ml);
    sqlite3_bind_double(st, 7, d->power_avg);
    sqlite3_bind_int(st, 8, d->predec_count);
    sqlite3_bind_int64(st, 9, id);
    sqlite3_step(st);
    sqlite3_finalize(st);
    xSemaphoreGive(s_mtx);
    return true;
}

bool db_process_finish(int64_t id, const db_process_t *d) {
    if (!s_ready) return false;
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    sqlite3_stmt *st;
    sqlite3_prepare_v2(s_db,
        "UPDATE processes SET ended_ms=?, stage_final=?, cube_end_t=?,"
        "heads_ml=?, body_ml=?, tails_ml=?, power_avg=?, predec_count=?,"
        "duration_sec=? WHERE id=?", -1, &st, NULL);
    sqlite3_bind_int64(st, 1, now_ms());
    sqlite3_bind_int(st, 2, d->stage_final);
    sqlite3_bind_double(st, 3, d->cube_end_t);
    sqlite3_bind_double(st, 4, d->heads_ml);
    sqlite3_bind_double(st, 5, d->body_ml);
    sqlite3_bind_double(st, 6, d->tails_ml);
    sqlite3_bind_double(st, 7, d->power_avg);
    sqlite3_bind_int(st, 8, d->predec_count);
    sqlite3_bind_double(st, 9, d->duration_sec);
    sqlite3_bind_int64(st, 10, id);
    sqlite3_step(st);
    sqlite3_finalize(st);
    xSemaphoreGive(s_mtx);
    return true;
}

bool db_process_get(int64_t id, db_process_t *o) {
    if (!s_ready) return false;
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    sqlite3_stmt *st;
    sqlite3_prepare_v2(s_db, "SELECT * FROM processes WHERE id=?", -1, &st, NULL);
    sqlite3_bind_int64(st, 1, id);
    bool ok = false;
    if (sqlite3_step(st) == SQLITE_ROW) {
        memset(o, 0, sizeof(*o));
        o->id = sqlite3_column_int64(st, 0);
        o->started_ms = sqlite3_column_int64(st, 1);
        o->ended_ms = sqlite3_column_int64(st, 2);
        o->type = sqlite3_column_int(st, 3);
        o->stage_final = sqlite3_column_int(st, 4);
        o->cube_start_t = sqlite3_column_double(st, 5);
        o->cube_end_t = sqlite3_column_double(st, 6);
        o->heads_ml = sqlite3_column_double(st, 7);
        o->body_ml = sqlite3_column_double(st, 8);
        o->tails_ml = sqlite3_column_double(st, 9);
        o->power_avg = sqlite3_column_double(st, 10);
        o->predec_count = sqlite3_column_int(st, 11);
        o->duration_sec = sqlite3_column_double(st, 12);
        const char *r = (const char*)sqlite3_column_text(st, 13);
        if (r) strncpy(o->recipe_id, r, 31);
        const char *n = (const char*)sqlite3_column_text(st, 14);
        if (n) strncpy(o->notes, n, 127);
        o->rating = sqlite3_column_int(st, 15);
        ok = true;
    }
    sqlite3_finalize(st);
    xSemaphoreGive(s_mtx);
    return ok;
}

int db_process_list(db_process_t *out, int max, int offset) {
    if (!s_ready) return 0;
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    sqlite3_stmt *st;
    sqlite3_prepare_v2(s_db,
        "SELECT * FROM processes ORDER BY started_ms DESC LIMIT ? OFFSET ?",
        -1, &st, NULL);
    sqlite3_bind_int(st, 1, max);
    sqlite3_bind_int(st, 2, offset);
    int n = 0;
    while (sqlite3_step(st) == SQLITE_ROW && n < max) {
        db_process_t *o = &out[n++];
        memset(o, 0, sizeof(*o));
        o->id = sqlite3_column_int64(st, 0);
        o->started_ms = sqlite3_column_int64(st, 1);
        o->ended_ms = sqlite3_column_int64(st, 2);
        o->type = sqlite3_column_int(st, 3);
        o->cube_start_t = sqlite3_column_double(st, 5);
        o->cube_end_t = sqlite3_column_double(st, 6);
        o->heads_ml = sqlite3_column_double(st, 7);
        o->body_ml = sqlite3_column_double(st, 8);
        o->tails_ml = sqlite3_column_double(st, 9);
        o->duration_sec = sqlite3_column_double(st, 12);
        o->rating = sqlite3_column_int(st, 15);
    }
    sqlite3_finalize(st);
    xSemaphoreGive(s_mtx);
    return n;
}

int db_process_count(void) {
    if (!s_ready) return 0;
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    sqlite3_stmt *st;
    sqlite3_prepare_v2(s_db, "SELECT COUNT(*) FROM processes", -1, &st, NULL);
    int n = 0;
    if (sqlite3_step(st) == SQLITE_ROW) n = sqlite3_column_int(st, 0);
    sqlite3_finalize(st);
    xSemaphoreGive(s_mtx);
    return n;
}

bool db_process_delete(int64_t id) {
    if (!s_ready) return false;
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    sqlite3_stmt *st;
    sqlite3_prepare_v2(s_db, "DELETE FROM processes WHERE id=?", -1, &st, NULL);
    sqlite3_bind_int64(st, 1, id); sqlite3_step(st); sqlite3_finalize(st);
    sqlite3_prepare_v2(s_db, "DELETE FROM measurements WHERE process_id=?", -1, &st, NULL);
    sqlite3_bind_int64(st, 1, id); sqlite3_step(st); sqlite3_finalize(st);
    xSemaphoreGive(s_mtx);
    return true;
}

bool db_process_set_rating(int64_t id, uint8_t rating, const char *notes) {
    if (!s_ready) return false;
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    sqlite3_stmt *st;
    sqlite3_prepare_v2(s_db,
        "UPDATE processes SET rating=?, notes=? WHERE id=?", -1, &st, NULL);
    sqlite3_bind_int(st, 1, rating);
    sqlite3_bind_text(st, 2, notes ? notes : "", -1, SQLITE_STATIC);
    sqlite3_bind_int64(st, 3, id);
    sqlite3_step(st); sqlite3_finalize(st);
    xSemaphoreGive(s_mtx);
    return true;
}

bool db_meas_insert(int64_t pid, const db_measurement_t *m) {
    if (!s_ready || pid == 0) return false;
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    sqlite3_stmt *st;
    sqlite3_prepare_v2(s_db,
        "INSERT INTO measurements(process_id, ts_ms, cube_temp, column_temp,"
        "jacket_temp, pressure, power, valve_body, flow)"
        "VALUES(?,?,?,?,?,?,?,?,?)", -1, &st, NULL);
    sqlite3_bind_int64(st, 1, pid);
    sqlite3_bind_int64(st, 2, m->ts_ms);
    sqlite3_bind_double(st, 3, m->cube_temp);
    sqlite3_bind_double(st, 4, m->column_temp);
    sqlite3_bind_double(st, 5, m->jacket_temp);
    sqlite3_bind_double(st, 6, m->pressure);
    sqlite3_bind_double(st, 7, m->power);
    sqlite3_bind_double(st, 8, m->valve_body);
    sqlite3_bind_double(st, 9, m->flow);
    sqlite3_step(st); sqlite3_finalize(st);
    xSemaphoreGive(s_mtx);
    return true;
}

int db_meas_range(int64_t pid, int64_t from, int64_t to,
                  db_measurement_t *out, int max) {
    if (!s_ready) return 0;
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    sqlite3_stmt *st;
    sqlite3_prepare_v2(s_db,
        "SELECT ts_ms, cube_temp, column_temp, jacket_temp, pressure,"
        "power, valve_body, flow FROM measurements"
        " WHERE process_id=? AND ts_ms BETWEEN ? AND ? ORDER BY ts_ms",
        -1, &st, NULL);
    sqlite3_bind_int64(st, 1, pid);
    sqlite3_bind_int64(st, 2, from);
    sqlite3_bind_int64(st, 3, to);
    int n = 0;
    while (sqlite3_step(st) == SQLITE_ROW && n < max) {
        db_measurement_t *m = &out[n++];
        m->process_id = pid;
        m->ts_ms = sqlite3_column_int64(st, 0);
        m->cube_temp = sqlite3_column_double(st, 1);
        m->column_temp = sqlite3_column_double(st, 2);
        m->jacket_temp = sqlite3_column_double(st, 3);
        m->pressure = sqlite3_column_double(st, 4);
        m->power = sqlite3_column_double(st, 5);
        m->valve_body = sqlite3_column_double(st, 6);
        m->flow = sqlite3_column_double(st, 7);
    }
    sqlite3_finalize(st);
    xSemaphoreGive(s_mtx);
    return n;
}

int db_meas_count(int64_t pid) {
    if (!s_ready) return 0;
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    sqlite3_stmt *st;
    sqlite3_prepare_v2(s_db,
        "SELECT COUNT(*) FROM measurements WHERE process_id=?", -1, &st, NULL);
    sqlite3_bind_int64(st, 1, pid);
    int n = 0;
    if (sqlite3_step(st) == SQLITE_ROW) n = sqlite3_column_int(st, 0);
    sqlite3_finalize(st);
    xSemaphoreGive(s_mtx);
    return n;
}

bool db_event_insert(const db_event_t *e) {
    if (!s_ready) return false;
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    sqlite3_stmt *st;
    sqlite3_prepare_v2(s_db,
        "INSERT INTO events(ts_ms, type, msg) VALUES(?,?,?)", -1, &st, NULL);
    sqlite3_bind_int64(st, 1, e->ts_ms);
    sqlite3_bind_int(st, 2, e->type);
    sqlite3_bind_text(st, 3, e->msg, -1, SQLITE_STATIC);
    sqlite3_step(st); sqlite3_finalize(st);
    xSemaphoreGive(s_mtx);
    return true;
}

int db_event_list(db_event_t *out, int max, uint8_t tf) {
    if (!s_ready) return 0;
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    sqlite3_stmt *st;
    if (tf == 0xFF)
        sqlite3_prepare_v2(s_db,
            "SELECT id, ts_ms, type, msg FROM events ORDER BY ts_ms DESC LIMIT ?",
            -1, &st, NULL);
    else
        sqlite3_prepare_v2(s_db,
            "SELECT id, ts_ms, type, msg FROM events WHERE type=?"
            " ORDER BY ts_ms DESC LIMIT ?", -1, &st, NULL);
    int idx = 1;
    if (tf != 0xFF) sqlite3_bind_int(st, idx++, tf);
    sqlite3_bind_int(st, idx, max);
    int n = 0;
    while (sqlite3_step(st) == SQLITE_ROW && n < max) {
        out[n].id = sqlite3_column_int64(st, 0);
        out[n].ts_ms = sqlite3_column_int64(st, 1);
        out[n].type = sqlite3_column_int(st, 2);
        const char *s = (const char*)sqlite3_column_text(st, 3);
        if (s) strncpy(out[n].msg, s, 127);
        n++;
    }
    sqlite3_finalize(st);
    xSemaphoreGive(s_mtx);
    return n;
}

int db_event_count(uint8_t tf) {
    if (!s_ready) return 0;
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    sqlite3_stmt *st;
    if (tf == 0xFF)
        sqlite3_prepare_v2(s_db, "SELECT COUNT(*) FROM events", -1, &st, NULL);
    else {
        sqlite3_prepare_v2(s_db, "SELECT COUNT(*) FROM events WHERE type=?", -1, &st, NULL);
        sqlite3_bind_int(st, 1, tf);
    }
    int n = 0;
    if (sqlite3_step(st) == SQLITE_ROW) n = sqlite3_column_int(st, 0);
    sqlite3_finalize(st);
    xSemaphoreGive(s_mtx);
    return n;
}

bool db_vacuum(void) { return db_exec("VACUUM;"); }

int64_t db_file_size(void) {
    FILE *f = fopen(DB_PATH, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    int64_t s = ftell(f);
    fclose(f);
    return s;
}

bool db_backup(const char *path) {
    if (!s_ready) return false;
    db_exec("PRAGMA wal_checkpoint(TRUNCATE);");
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    FILE *src = fopen(DB_PATH, "rb");
    FILE *dst = fopen(path, "wb");
    bool ok = false;
    if (src && dst) {
        char buf[4096]; size_t n;
        while ((n = fread(buf, 1, sizeof(buf), src)) > 0) fwrite(buf, 1, n, dst);
        ok = true;
    }
    if (src) fclose(src);
    if (dst) fclose(dst);
    xSemaphoreGive(s_mtx);
    return ok;
}

void db_stats(char *buf, size_t max) {
    snprintf(buf, max, "Процессов: %d\nСобытий: %d\nРазмер: %lld КБ",
             db_process_count(), db_event_count(0xFF),
             (long long)(db_file_size() / 1024));
}

bool db_export_process_json(int64_t id, const char *path) {
    db_process_t p;
    if (!db_process_get(id, &p)) return false;
    FILE *f = fopen(path, "w");
    if (!f) return false;
    fprintf(f,
        "{\"id\":%lld,\"type\":%d,\"started_ms\":%lld,\"duration_sec\":%.1f,"
        "\"cube_start_t\":%.2f,\"cube_end_t\":%.2f,\"heads_ml\":%.1f,"
        "\"body_ml\":%.1f,\"tails_ml\":%.1f,\"power_avg\":%.1f,"
        "\"predec_count\":%u,\"rating\":%d}\n",
        (long long)p.id, p.type, (long long)p.started_ms, p.duration_sec,
        p.cube_start_t, p.cube_end_t, p.heads_ml, p.body_ml, p.tails_ml,
        p.power_avg, (unsigned)p.predec_count, p.rating);
    fclose(f);
    return true;
}