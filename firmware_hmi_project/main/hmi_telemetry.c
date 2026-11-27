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

void hmi_telemetry_set_stats(uint32_t rx_err, uint32_t frames,
                             uint16_t latency_ms)
{
    s_t.rx_err     = rx_err;
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
    /* Контроллер отправляет "power" (проценты) и "p" (давление, бар).
     * Предпочитаем "power" для UI мощности; fallback на "p" для старых кадров. */
    {
        cJSON *jp = cJSON_GetObjectItemCaseSensitive(root, "power");
        s_t.p = cJSON_IsNumber(jp) ? (float)jp->valuedouble : jnum(root, "p", 0.0f);
    }
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

    /* guards / diagnostics */
    s_t.flood_active        = jbool(root, "fl", false);
    s_t.eq_lock_active      = jbool(root, "eq", false);
    s_t.eq_lock_phase       = (uint8_t)juint(root, "eq_ph", 0);
    s_t.volume_guard_active = jbool(root, "vg", false);
    s_t.volume_no_flow      = jbool(root, "vnf", false);
    s_t.volume_overflow     = jbool(root, "vof", false);

    cJSON_Delete(root);
}

static const char *reason_str(uint8_t code)
{
    switch (code) {
        case CAN_HMI_REASON_ESTOP: return "E-STOP";
        case CAN_HMI_REASON_BENCH: return "BENCH";
        case CAN_HMI_REASON_SAFETY: return "SAFETY";
        case CAN_HMI_REASON_FLOOD: return "FLOOD";
        case CAN_HMI_REASON_EQ_LOCK: return "EQ_LOCK";
        case CAN_HMI_REASON_VOL_GUARD: return "VOL_GUARD";
        case CAN_HMI_REASON_VOL_NO_FLOW: return "VOL_NO_FLOW";
        case CAN_HMI_REASON_VOL_OVERFLOW: return "VOL_OVERFLOW";
        case CAN_HMI_REASON_ZMPT: return "ZMPT_FAULT";
        case CAN_HMI_REASON_HW: return "HW_FAULT";
        default: return "";
    }
}

void hmi_telemetry_apply_can(const can_hmi_tlm_a_t *a, const can_hmi_tlm_b_t *b,
                             const can_hmi_tlm_c_t *c, const can_hmi_tlm_d_t *d,
                             const can_hmi_tlm_e_t *e, const can_hmi_tlm_f_t *f,
                             const can_hmi_tlm_g_t *g,
                             uint16_t heap_kb, uint32_t elapsed_s)
{
    if (!a || !b || !c || !d || !e || !f || !g) return;

    s_t.stg = a->stage;
    s_t.run = (a->flags1 & CAN_HMI_F1_RUN) ? 1 : 0;
    s_t.pause = (a->flags1 & CAN_HMI_F1_PAUSE) ? 1 : 0;
    s_t.sa = (a->flags1 & CAN_HMI_F1_SAFETY) != 0;
    s_t.es = (a->flags1 & CAN_HMI_F1_ESTOP) != 0;
    s_t.actions_blocked = (a->flags1 & CAN_HMI_F1_ACTIONS_BLOCKED) != 0;
    s_t.bench_mode = (a->flags1 & CAN_HMI_F1_BENCH) != 0;
    s_t.estop_active = s_t.es;
    s_t.estop_mode = (uint8_t)(a->estop_mode & 0x0Fu);
    s_t.sd = (a->flags1 & CAN_HMI_F1_SD_READY) != 0;
    s_t.db = (a->flags3 & CAN_HMI_F3_DB_READY) != 0;
    s_t.r = s_t.run;

    s_t.flood_active = (a->flags2 & CAN_HMI_F2_FLOOD) != 0;
    s_t.eq_lock_active = (a->flags2 & CAN_HMI_F2_EQ_LOCK) != 0;
    s_t.eq_lock_phase = a->eq_phase;
    s_t.volume_guard_active = (a->flags2 & CAN_HMI_F2_VOL_GUARD) != 0;
    s_t.volume_no_flow = (a->flags2 & CAN_HMI_F2_VOL_NO_FLOW) != 0;
    s_t.volume_overflow = (a->flags2 & CAN_HMI_F2_VOL_OVERFLOW) != 0;
    s_t.scale_ready = (a->flags2 & CAN_HMI_F2_SCALE_READY) != 0;
    s_t.scale_active = (a->flags2 & CAN_HMI_F2_SCALE_ACTIVE) != 0;
    s_t.zmpt_fault = (a->flags2 & CAN_HMI_F2_ZMPT_FAULT) != 0;

    s_t.tc = can_hmi_x100_to_temp(b->cube_x100);
    s_t.ts = can_hmi_x100_to_temp(g->target_x100);
    s_t.m = can_hmi_x100_to_temp(b->column_x100);

    s_t.lv = c->level_pct;
    s_t.p = can_hmi_x2_to_pct(d->power_x2);
    s_t.flow = (float)d->flow_x100 / 100.0f;
    s_t.baro = (float)d->pressure_x1000 / 1000.0f;

    s_t.heads = e->heads_ml;
    s_t.body = e->body_ml;
    s_t.tails = e->tails_ml;
    s_t.lsa_pwm = can_hmi_x2_to_pct(e->lsa_duty_x2);

    s_t.pt506_bar = (float)f->pt506_x1000 / 1000.0f;
    s_t.xgzp_kpa = (float)f->xgzp_x10 / 10.0f;
    s_t.zmpt_v1 = (float)f->zmpt_v1;
    s_t.zmpt_v2 = (float)f->zmpt_v2;
    s_t.zmpt_v3 = (float)f->zmpt_v3;

    uint32_t elapsed32 = (uint32_t)g->elapsed_lo | ((uint32_t)g->elapsed_hi << 16);
    s_t.heap = (uint32_t)heap_kb * 1024u;
    s_t.elapsed = elapsed32 ? elapsed32 : elapsed_s;
    snprintf(s_t.reason, sizeof(s_t.reason), "%s", reason_str(a->reason_code));
    snprintf(s_t.version, sizeof(s_t.version), "CAN v%u.%u.%u",
             (unsigned)HMI_PROTO_VERSION_MAJOR, (unsigned)HMI_PROTO_VERSION_MINOR,
             (unsigned)HMI_PROTO_VERSION_PATCH);
}

void hmi_telemetry_apply_ui(void)
{
    if (s_t.epoch_s) ui_fut_clock_set_epoch(s_t.epoch_s);

    rect_state_t r = {0};
    r.ready      = s_t.link && s_t.sa && !s_t.actions_blocked;
    {
        const char *stage = "—";
        switch (s_t.stg) {
            case 0:  stage = "ПРОСТОЙ"; break;       /* ST_IDLE */
            case 1:  stage = "НАГРЕВ"; break;        /* ST_HEATUP */
            case 2:  stage = "СТАБИЛИЗАЦИЯ"; break;  /* ST_STABILIZE */
            case 3:  stage = "ГОЛОВЫ"; break;        /* ST_HEADS */
            case 4:  stage = "ТЕЛО"; break;          /* ST_BODY */
            case 5:  stage = "ХВОСТЫ"; break;        /* ST_TAILS */
            case 6:  stage = "ОХЛАЖДЕНИЕ"; break;    /* ST_COOLDOWN */
            case 7:  stage = "ГОТОВО"; break;        /* ST_DONE */
            case 8:  stage = "ПРОВЕРКА"; break;      /* ST_PRECHECK */
            case 9:  stage = "ПРОГРЕВ"; break;       /* ST_WARMUP */
            case 10: stage = "РАБОТА"; break;        /* ST_RUNNING */
            case 11: stage = "ОШИБКА"; break;       /* ST_ERROR */
            default: stage = "РАБОТА"; break;
        }
        if (s_t.pause && !s_t.bench_mode && !s_t.estop_active) stage = "ПАУЗА";
        r.stage = s_t.bench_mode ? "СТЕНД: ДЕЙСТВИЯ ЗАБЛОКИРОВАНЫ" :
                  (s_t.estop_active ? "E-STOP АКТИВЕН" : stage);
    }
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
    d.can_ok     = s_t.link;
    d.rx_err     = s_t.rx_err;
    d.frames     = s_t.frames;
    d.latency_ms = s_t.latency_ms;
    d.heap_kb    = s_t.heap / 1024;
    d.valve_open = s_t.stg >= 2;
    d.pump_on    = s_t.r != 0;
    d.mixer      = s_t.stg;
    d.heater     = (uint8_t)s_t.p;
    d.flood      = s_t.flood_active;
    d.eq_lock    = s_t.eq_lock_active;
    d.eq_phase   = s_t.eq_lock_phase;
    d.vol_guard  = s_t.volume_guard_active;
    d.vol_no_flow = s_t.volume_no_flow;
    d.vol_overflow = s_t.volume_overflow;
    d.reason     = s_t.reason;
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
