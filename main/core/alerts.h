#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef enum { ALERT_INFO=0, ALERT_WARN=1, ALERT_CRIT=2 } alert_sev_t;

typedef struct {
    uint32_t ts_ms;
    alert_sev_t sev;
    char code[24];
    char msg[96];
    bool ack;
} alert_t;

void alerts_init(void);
void alerts_add(alert_sev_t sev, const char *code, const char *fmt, ...);
size_t alerts_count(void);
size_t alerts_unacked(void);
const alert_t* alerts_get(size_t idx); // 0 = newest
void alerts_ack_all(void);
size_t alerts_json(char *buf, size_t buf_sz, size_t limit);