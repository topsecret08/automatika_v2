#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef struct {
    int64_t id;
    int64_t started_ms, ended_ms;
    uint8_t type, stage_final;
    float   cube_start_t, cube_end_t;
    float   heads_ml, body_ml, tails_ml, power_avg;
    uint32_t predec_count;
    float   duration_sec;
    char    recipe_id[32], notes[128];
    uint8_t rating;
} db_process_t;

typedef struct {
    int64_t process_id, ts_ms;
    float   cube_temp, column_temp, jacket_temp, pressure;
    float   power, valve_body, flow;
} db_measurement_t;

typedef struct {
    int64_t id, ts_ms;
    uint8_t type;
    char    msg[128];
} db_event_t;

bool db_init(void);
void db_close(void);
bool db_is_ready(void);
int64_t db_process_start(uint8_t type, const char *recipe_id);
bool    db_process_update(int64_t id, const db_process_t *data);
bool    db_process_finish(int64_t id, const db_process_t *data);
bool    db_process_get(int64_t id, db_process_t *out);
int     db_process_list(db_process_t *out, int max, int offset);
int     db_process_count(void);
bool    db_process_delete(int64_t id);
bool    db_process_set_rating(int64_t id, uint8_t rating, const char *notes);
bool    db_meas_insert(int64_t process_id, const db_measurement_t *m);
int     db_meas_range(int64_t pid, int64_t from_ms, int64_t to_ms,
                      db_measurement_t *out, int max);
int     db_meas_count(int64_t pid);
bool    db_event_insert(const db_event_t *e);
int     db_event_list(db_event_t *out, int max, uint8_t type_filter);
int     db_event_count(uint8_t type_filter);
bool    db_exec(const char *sql);
bool    db_backup(const char *path);
bool    db_vacuum(void);
int64_t db_file_size(void);
void    db_stats(char *buf, size_t max);
bool    db_export_process_json(int64_t id, const char *path);