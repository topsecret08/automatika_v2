#include "run_report.h"
#include "core/alerts.h"
#include <stdio.h>
#include <math.h>

size_t run_report_json(const process_state_t *st, char *buf, size_t buf_sz) {
    if (!st || !buf || buf_sz == 0) return 0;
    const char *rec = "OK";
    if (st->emergency || st->quality_score < 50.0f) rec = "Review safety/guards before next run";
    else if (st->volume_no_flow) rec = "Check pump/valves/scale flow path";
    else if (st->volume_overflow) rec = "Check target volumes and overflow limits";
    else if (st->mass_balance_loss_pct > 25.0f) rec = "Check leaks, collection calibration, and ABV assumptions";
    else if (st->quality_score < 80.0f) rec = "Improve sensor confidence and stabilize process";

    unsigned unacked = (unsigned)alerts_unacked();
    int n = snprintf(buf, buf_sz,
        "{\"ok\":true,"
        "\"type\":%d,\"stage\":%d,\"elapsed_sec\":%u,"
        "\"heads_ml\":%.1f,\"body_ml\":%.1f,\"tails_ml\":%.1f,"
        "\"abv_pct\":%.2f,\"alcohol_ml\":%.1f,\"loss_pct\":%.2f,"
        "\"quality\":%.1f,\"warnings\":%u,\"alerts_unacked\":%u,"
        "\"flood\":%d,\"vol_no_flow\":%d,\"vol_overflow\":%d,"
        "\"recommendation\":\"%s\"}",
        (int)st->type, (int)st->stage, (unsigned)st->elapsed_sec,
        (double)st->heads_collected_ml, (double)st->body_collected_ml, (double)st->tails_collected_ml,
        (double)st->mass_balance_abv_pct, (double)st->mass_balance_alcohol_ml, (double)st->mass_balance_loss_pct,
        (double)st->quality_score, (unsigned)st->quality_warnings, unacked,
        st->flood_active?1:0, st->volume_no_flow?1:0, st->volume_overflow?1:0,
        rec);
    if (n < 0) return 0;
    if ((size_t)n >= buf_sz) { buf[buf_sz-1]=0; return buf_sz-1; }
    return (size_t)n;
}