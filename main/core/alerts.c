#include "alerts.h"
#include "esp_timer.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define ALERT_MAX 32
static alert_t s_buf[ALERT_MAX];
static size_t s_head = 0; // next write
static size_t s_count = 0;

void alerts_init(void) {
    memset(s_buf, 0, sizeof(s_buf));
    s_head = 0; s_count = 0;
}

void alerts_add(alert_sev_t sev, const char *code, const char *fmt, ...) {
    alert_t *a = &s_buf[s_head];
    memset(a, 0, sizeof(*a));
    a->ts_ms = (uint32_t)(esp_timer_get_time() / 1000);
    a->sev = sev;
    if (code) strncpy(a->code, code, sizeof(a->code)-1);
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(a->msg, sizeof(a->msg), fmt ? fmt : "", ap);
    va_end(ap);
    a->ack = false;
    s_head = (s_head + 1) % ALERT_MAX;
    if (s_count < ALERT_MAX) s_count++;
}

size_t alerts_count(void) { return s_count; }

size_t alerts_unacked(void) {
    size_t n = 0;
    for (size_t i = 0; i < s_count; i++) {
        const alert_t *a = alerts_get(i);
        if (a && !a->ack) n++;
    }
    return n;
}

const alert_t* alerts_get(size_t idx) {
    if (idx >= s_count) return NULL;
    // newest first: idx 0 is last written
    size_t pos = (s_head + ALERT_MAX - 1 - idx) % ALERT_MAX;
    return &s_buf[pos];
}

void alerts_ack_all(void) {
    for (size_t i = 0; i < s_count; i++) s_buf[i].ack = true;
}

size_t alerts_json(char *buf, size_t buf_sz, size_t limit) {
    if (!buf || buf_sz == 0) return 0;
    size_t off = 0;
    off += snprintf(buf + off, buf_sz - off, "{\"count\":%u,\"unacked\":%u,\"items\":[",
                    (unsigned)s_count, (unsigned)alerts_unacked());
    size_t n = s_count < limit ? s_count : limit;
    for (size_t i = 0; i < n && off + 2 < buf_sz; i++) {
        const alert_t *a = alerts_get(i);
        if (!a) continue;
        char msg[96];
        // minimal JSON escaping for quotes/backslashes
        size_t j = 0;
        for (size_t k = 0; a->msg[k] && j + 2 < sizeof(msg); k++) {
            char c = a->msg[k];
            if (c == '"' || c == '\\') { msg[j++]='\\'; msg[j++]=c; }
            else if ((unsigned char)c >= 32) msg[j++]=c;
        }
        msg[j]=0;
        off += snprintf(buf + off, buf_sz - off,
            "%s{\"ts\":%u,\"sev\":%d,\"code\":\"%s\",\"msg\":\"%s\",\"ack\":%d}",
            i?",":"", (unsigned)a->ts_ms, (int)a->sev, a->code, msg, a->ack?1:0);
    }
    if (off + 3 < buf_sz) off += snprintf(buf + off, buf_sz - off, "]}");
    else if (buf_sz >= 3) { buf[buf_sz-3]=']'; buf[buf_sz-2]='}'; buf[buf_sz-1]=0; }
    return off;
}