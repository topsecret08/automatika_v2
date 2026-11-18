#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    EV_CRITICAL = 0,
    EV_MAJOR,
    EV_MINOR,
    EV_INFO
} ev_prio_t;

typedef struct {
    ev_prio_t   prio;
    const char *type;
    uint8_t     source_id;
    const char *msg;
    int64_t     ts_us;
} bus_event_t;

typedef void (*ev_sink_t)(const bus_event_t *e);

void event_bus_reset(void);
bool event_bus_register(ev_sink_t sink, ev_prio_t min_prio);
void event_bus_publish(const bus_event_t *e);
uint32_t event_bus_dropped(void);
