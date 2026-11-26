#include "hmi_telemetry.h"

#include <stdio.h>
#include <string.h>

#include "cJSON.h"

#include "ui_futuristic.h"
#include "ui_screen_rectification.h"
#include "ui_screens_modes.h"

static hmi_telemetry_t s_t;

static float jnum(cJSON *root, const char *name, float def)
{
    cJSON *j = cJSON_GetObjectItemCaseSensitive(root, name);
    return cJSON_IsNumber(j) ? (float)j->valuedouble : def;
}

static uint32_t juint(cJSON *root, const char *name, uint32_t def)
{
    cJSON *j = cJSON_GetObjectItemCaseSensitive(root, name);
    return cJSON_IsNumber(j) ? (uint32_t)j->valuedouble : def;
}

static bool jbool(cJSON *root, const char *name, bool def)
{
    cJSON *j = cJSON_GetObjectItemCaseSensitive(root, name);
    return cJSON_IsBool(j) ? cJSON_IsTrue(j) : def;
}

static void jstr(cJSON *root, const char *name, char *dst, size_t dst_len,
                 const char *def)
{
    cJSON *j = cJSON_GetObjectItemCaseSensitive(root, name);
    if (cJSON_IsString(j) && j->valuestring) {
        snprintf(dst, dst_len, "%s", j->valuestring);
    } else {
        snprintf(dst, dst_len, "%s", def ? def : "");
    }
}

void hmi_telemetry_reset(void)
{
    memset(&s_t, 0, sizeof(s_t));
    strcpy(s_t.version, "—");
    strcpy(s_t.frac, "—");
}

void hmi_telemetry_set_link(bool link)
{
    s_t.link = link;
}

void hmi_telemetry_set_stats(uint32_t crc_err, uint32_t frames,
                             uint16_t latency_ms)
{
    s_t.crc_err    = crc_err;
    s_t.frames     = frames;
    s_t.latency_ms = latency_ms;
}

const hmi_telemetry_t *hmi_telemetry_get(void)
{
    return &s_t;
}

void hmi_telemetry_from_json(const uint8_t *payload, uint16_t len)
{
    if (!payload || len == 0) return;

    cJSON *root = cJSON_ParseWithLength((const char *)payload, len);
    if (!root) return;

    jstr(root, "v", s_t.version, sizeof(s_t.version), "—");
    s_t.tc     = jnum(root, "tc", 0.0f);
    s_t.ts     = jnum(root, "ts", 0.0f);
    s_t.p      = jnum(root, "p", 0.0f);
    s_t.m      = jnum(root, "m", 0.0f);
    s_t.sa     = jbool(root, "sa", false);
    s_t.es     = jbool(root, "es", false);
    s_t.lv     = (uint8_t)juint(root, "lv", 0);
    s_t.r      = (uint8_t)juint(root, "r", 0);
    s_t.stg    = (uint8_t)juint(root, "stg", 0);
    s_t.run    = (uint8_t)juint(root, "run", 0);
    s_t.pause  = (uint8_t)juint(root, "pause", 0);
    s_t.heads  = juint(root, "heads", 0);
    s_t.body   = juint(root, "body", 0);
    s_t.tails  = juint(root, "tails", 0);
    s_t.elapsed = juint(root, "elapsed", 0);
    jstr(root, "frac", s_t.frac, sizeof(s_t.frac), "—");
    s_t.baro   = jnum(root, "baro", 0.0f);
    s_t.flow   = jnum(root, "flow", 0.0f);
    s_t.sd     = jbool(root, "sd", false);
    s_t.db     = jbool(root, "db", false);
    s_t.heap   = juint(root, "heap", 0);
    s_t.epoch_s = juint(root, "epoch", 0);

    /* E-STOP / bench mode (backward-compatible) */
    s_t.estop_active    = jbool(root, "estop_active", s_t.es);
    s_t.actions_blocked = jbool(root, "actions_blocked", s_t.es);
    s_t.bench_mode      = jbool(root, "bench_mode", false);
    s_t.estop_mode      = (uint8_t)juint(root, "estop_mode", 0);
    jstr(root, "reason", s_t.reason, sizeof(s_t.reason), "");

    /* expansion sensors */
    s_t.lsa_pwm   = jnum(root, "lsa_pwm", 0.0f);
    s_t.pt506_bar = jnum(root, "pt506_bar", 0.0f);
    s_t.xgzp_kpa  = jnum(root, "xgzp_kpa", 0.0f);
    s_t.zmpt_v1   = jnum(root, "zmpt_v1", 0.0f);
    s_t.zmpt_v2   = jnum(root, "zmpt_v2", 0.0f);
    s_t.zmpt_v3   = jnum(root, "zmpt_v3", 0.0f);
    s_t.zmpt_ctrl = jnum(root, "zmpt_ctrl", 0.0f);
    s_t.zmpt_fault = jbool(root, "zmpt_fault", false);

    /* scale indicator (HX711) */
    s_t.scale_ready  = jbool(root, "sc_rdy", false);
    s_t.scale_active = jbool(root, "sc_act", false);
    s_t.scale_g      = jnum(root, "sc_g", 0.0f);
    s_t.scale_flow   = jnum(root, "sc_flow", 0.0f);
    s_t.scale_ml     = jnum(root, "sc_ml", 0.0f);

    cJSON_Delete(root);
}

void hmi_telemetry_apply_ui(void)
{
    if (s_t.epoch_s) ui_fut_clock_set_epoch(s_t.epoch_s);

    rect_state_t r = {0};
    r.ready      = s_t.link && s_t.sa && !s_t.actions_blocked;
    r.stage      = s_t.bench_mode ? "СТЕНД: ДЕЙСТВИЯ ЗАБЛОКИРОВАНЫ" :
                   (s_t.estop_active ? "E-STOP АКТИВЕН" : "ЭТАП: ТЕЛО");
    r.steam      = s_t.tc;
    r.steam_set  = s_t.ts;
    r.steam_fix  = s_t.ts > 1.5f ? s_t.ts - 1.5f : s_t.ts;
    r.t_fix      = r.steam_fix;
    r.open_s     = 1.5f;   /* TODO: взять из настроек режима */
    r.close_s    = 8.5f;   /* TODO: взять из настроек режима */
    r.remain_s   = 0.0f;
    r.valve_open = s_t.stg >= 2;
    r.alcohol_ml = s_t.body;
    r.strength   = (uint8_t)s_t.p;
    r.kub        = s_t.tc;
    r.par        = s_t.ts;
    ui_screen_rectification_update(&r);

    prep_state_t p = {0};
    p.profile    = "—";
    p.t1         = s_t.tc;
    p.t2         = s_t.ts;
    p.level      = s_t.lv;
    p.flow       = s_t.flow;
    p.rinse_done = s_t.sd;
    p.sealed     = s_t.sa;
    p.preheat    = s_t.ts;
    ui_screen_preparation_update(&p);

    diag_state_t d = {0};
    d.rs485_ok   = s_t.link;
    d.crc_err    = s_t.crc_err;
    d.frames     = s_t.frames;
    d.latency_ms = s_t.latency_ms;
    d.heap_kb    = s_t.heap / 1024;
    d.valve_open = s_t.stg >= 2;
    d.pump_on    = s_t.r != 0;
    d.mixer      = s_t.stg;
    d.heater     = (uint8_t)s_t.p;
    ui_screen_diagnostics_update(&d);

    sys_state_t y = {0};
    y.version        = s_t.version;
    y.available      = "—";
    y.api_ok         = s_t.db;
    y.uptime_h       = (uint16_t)(s_t.elapsed / 3600);
    y.cpu_temp       = 0;
    y.psram_mb       = 4;
    y.update_progress = 0;
    ui_screen_system_update(&y);
}
