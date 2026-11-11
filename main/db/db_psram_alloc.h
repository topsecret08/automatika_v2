#pragma once
#include "sqlite3.h"
#include <stddef.h>

int db_psram_alloc_init(void);
size_t db_psram_used(void);
size_t db_psram_peak(void);
int    db_psram_alloc_count(void);
void   db_psram_stats(char *buf, size_t max);