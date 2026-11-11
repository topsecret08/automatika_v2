#include "db_manager.h"

// Предсобранные запросы для UI

int db_query_processes_by_type(uint8_t type, db_process_t *out, int max) {
    // Упрощённо: фильтруем в коде
    db_process_t tmp[64];
    int n = db_process_list(tmp, 64, 0);
    int k = 0;
    for (int i = 0; i < n && k < max; i++)
        if (tmp[i].type == type) out[k++] = tmp[i];
    return k;
}

int db_query_top_rated(db_process_t *out, int max) {
    db_process_t tmp[64];
    int n = db_process_list(tmp, 64, 0);
    int k = 0;
    for (int i = 0; i < n && k < max; i++)
        if (tmp[i].rating >= 4) out[k++] = tmp[i];
    return k;
}