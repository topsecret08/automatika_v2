#pragma once
#include <stdint.h>
#include <stddef.h>

typedef enum {
    EVENT_INFO = 0, EVENT_WARNING, EVENT_ERROR, EVENT_EMERGENCY,
    EVENT_STAGE_CHANGE, EVENT_USER_ACTION, EVENT_SENSOR, EVENT_NETWORK,
} event_type_t;

typedef struct {
    uint32_t     timestamp_ms;
    event_type_t type;
    char         msg[96];
} event_t;

void        events_init(void);
void        events_log(event_type_t t, const char *fmt, ...);
event_t*    events_get(size_t *count);
// Читает события, появившиеся после позиции *cursor, и сдвигает курсор.
int         events_read_after(size_t *cursor, event_t *out, int max);
void        events_clear(void);
void        events_export_csv(const char *path);
void        events_task(void *arg);
const char* events_type_name(event_type_t t);