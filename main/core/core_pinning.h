#pragma once

#define CORE_NETWORK_AI     0
#define CORE_REALTIME_UI    1

#define PRIO_WIFI           23
#define PRIO_SAFETY         19
#define PRIO_ESTOP          20
#define PRIO_MODE           10
#define PRIO_UI              8
#define PRIO_SCALE           6
#define PRIO_HTTP            5
#define PRIO_DOSER           4
#define PRIO_ANOMALY         4
#define PRIO_WEB_API         4
#define PRIO_FPS             4
#define PRIO_PREDICT         3
#define PRIO_EVENTS          3
#define PRIO_VALVE_CALIB     3
#define PRIO_PRELOADER       2
#define PRIO_AUTOLEARN       2
#define PRIO_TINYML          2
#define PRIO_TELEMETRY       3

#define STACK_ESTOP         2048
#define STACK_DOSER         4096
#define STACK_LUA           12288
#define STACK_SAFETY        4096
#define STACK_MODE          8192
#define STACK_UI            8192
#define STACK_SCALE         4096
#define STACK_ANOMALY       4096
#define STACK_PRELOADER     4096
#define STACK_AUTOLEARN     4096
#define STACK_TINYML        4096
#define STACK_WEB           8192
#define STACK_VALVE_CALIB   3072
#define STACK_FPS           3072
#define STACK_EVENTS        3072

#define SPAWN(fn, name, stack, prio, core) \
    xTaskCreatePinnedToCore(fn, name, stack, NULL, prio, NULL, core)
