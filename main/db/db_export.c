#include "db_manager.h"
#include <stdio.h>

bool db_export_all_csv(const char *dir) {
    if (!db_is_ready()) return false;
    char path[256];
    snprintf(path, sizeof(path), "%s/processes.csv", dir);
    FILE *f = fopen(path, "w");
    if (!f) return false;
    fprintf(f, "id,started,type,duration,heads,body,tails,rating\n");
    db_process_t list[64];
    int n = db_process_list(list, 64, 0);
    for (int i = 0; i < n; i++) {
        fprintf(f, "%lld,%lld,%d,%.0f,%.0f,%.0f,%.0f,%d\n",
            (long long)list[i].id, (long long)list[i].started_ms,
            list[i].type, list[i].duration_sec,
            list[i].heads_ml, list[i].body_ml, list[i].tails_ml,
            list[i].rating);
    }
    fclose(f);
    return true;
}