#include "event_log.h"
#include "events.h"

#include <stdio.h>
#include <string.h>

static char s_out[EVENT_LOG_JSON_MAX];

static void json_escape_append(char *dst, size_t max, size_t *off, const char *src) {
    for (const char *p = src; p && *p && *off + 7 < max; p++) {
        unsigned char c = (unsigned char)*p;
        switch (c) {
        case '"':  dst[(*off)++] = '\\'; dst[(*off)++] = '"'; break;
        case '\\': dst[(*off)++] = '\\'; dst[(*off)++] = '\\'; break;
        case '\n': dst[(*off)++] = '\\'; dst[(*off)++] = 'n'; break;
        case '\r': dst[(*off)++] = '\\'; dst[(*off)++] = 'r'; break;
        case '\t': dst[(*off)++] = '\\'; dst[(*off)++] = 't'; break;
        default:
            if (c < 0x20) {
                int n = snprintf(dst + *off, max - *off, "\\u%04x", c);
                if (n > 0) *off += (size_t)n;
            } else {
                dst[(*off)++] = (char)c;
            }
        }
    }
    dst[*off] = '\0';
}

/* Копирует последние `limit` событий кольца events.c в buf, соблюдая
 * порядок по времени (старые → новые). Возвращает число копий. */
static int copy_recent(event_t *buf, int max) {
    size_t total = 0;
    event_t *ring = events_get(&total);
    if (!ring || total == 0 || max <= 0) return 0;

    int n = (total < (size_t)max) ? (int)total : max;
    /* кольцо: events.c хранит голову отдельно, поэтому порядок
     * гарантируем через events_read_after() по курсору. */
    size_t cursor = (total > (size_t)n) ? (total - (size_t)n) : 0;
    int got = events_read_after(&cursor, buf, n);
    return got;
}

const char *event_log_dump_json(int limit) {
    static event_t batch[EVENTS_RING_MAX];

    if (limit <= 0 || limit > EVENTS_RING_MAX) limit = 64;

    int n = copy_recent(batch, limit);

    size_t off = 0;
    s_out[off++] = '[';
    s_out[off] = '\0';

    for (int i = 0; i < n && off + 176 < sizeof(s_out); i++) {
        event_t *e = &batch[i];
        int w = snprintf(s_out + off, sizeof(s_out) - off,
                         "%s{\"ts_ms\":%u,\"type\":%d,\"name\":\"%s\",\"msg\":\"",
                         i ? "," : "", (unsigned)e->timestamp_ms, (int)e->type,
                         events_type_name(e->type));
        if (w > 0) off += (size_t)w;

        json_escape_append(s_out, sizeof(s_out), &off, e->msg);

        if (off + 2 < sizeof(s_out)) {
            s_out[off++] = '"';
            s_out[off++] = '}';
            s_out[off] = '\0';
        }
    }

    if (off + 1 < sizeof(s_out)) {
        s_out[off++] = ']';
        s_out[off] = '\0';
    }
    return s_out;
}

void event_log_add(int type, const char *msg) {
    events_log((event_type_t)type, "%s", msg ? msg : "");
}
