#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    char        name[24];
    void       *handle;
    uint32_t    timeout_ms;
    uint32_t    last_feed_ms;
    uint32_t    max_stale_ms;
    uint32_t    restarts;
    bool        enabled;
    bool        critical;
} wd_task_t;

void  wd_init(void);
bool  wd_register(const char *name, uint32_t timeout_ms, bool critical);
void  wd_feed(const char *name);
void  wd_remove(const char *name);
void  wd_task(void *arg);
void  wd_stats_dump(void);
int   wd_count(void);
wd_task_t* wd_get(int idx);

// Наибольшее «зависание» любой зарегистрированной задачи, мс
// (используется health-check и автоподтверждением OTA).
uint32_t wd_max_stale_ms(void);

#define WD_FEED(name) wd_feed(name)