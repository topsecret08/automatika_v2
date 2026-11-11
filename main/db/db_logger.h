#pragma once
#include "core/events.h"
void db_logger_event(event_type_t type, const char *msg);
void db_logger_task(void *arg);