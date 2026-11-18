#include "hw_profile_rules.h"

#include <stdio.h>

static bool temp_valid(const hw_profile_t *p, hw_temp_role_t role,
                       uint8_t count) {
    int idx = p->temp[role];
    return idx >= 0 && idx < (int)count;
}

void hw_profile_check(const hw_profile_t *p, uint8_t onewire_count,
                      process_type_t type, hw_mode_status_t *out) {
    if (!out) return;
    out->ready = false;
    out->reason[0] = '\0';
    if (!p) {
        snprintf(out->reason, sizeof(out->reason), "нет карты оборудования");
        return;
    }

    bool cube = temp_valid(p, HW_TEMP_CUBE, onewire_count);
    bool column = temp_valid(p, HW_TEMP_COLUMN, onewire_count);
    bool jacket = temp_valid(p, HW_TEMP_JACKET, onewire_count);
    bool tank = temp_valid(p, HW_TEMP_TANK, onewire_count);

    bool reflux = p->devices & (1u << HW_DEV_REFLUX);
    bool valves = p->devices & (1u << HW_DEV_VALVES);
    bool doser = p->devices & (1u << HW_DEV_DOSER);
    bool vacuum = p->devices & (1u << HW_DEV_VACUUM);
    bool vac_pump = p->devices & (1u << HW_DEV_VACUUM_PUMP);

    const char *reason = NULL;
    switch (type) {
    case PROCESS_RECTIFY:
        if (!cube) reason = "нужен датчик куба";
        else if (!column) reason = "нужен датчик колонны";
        else if (!reflux && !valves) reason = "нужен reflux или клапаны";
        break;
    case PROCESS_NBK:
        if (!cube) reason = "нужен датчик куба";
        else if (!column) reason = "нужен датчик колонны";
        else if (!doser) reason = "нужен дозатор/подача";
        break;
    case PROCESS_PVK:
        if (!jacket) reason = "нужен датчик рубашки";
        break;
    case PROCESS_VACUUM:
        if (!cube) reason = "нужен датчик куба";
        else if (!vacuum) reason = "нужен датчик вакуума";
        else if (!vac_pump) reason = "нужен вакуумный насос";
        break;
    case PROCESS_FERMENT:
        if (!cube && !tank) reason = "нужен датчик температуры";
        break;
    case PROCESS_DISTILL:
    case PROCESS_BEER:
    case PROCESS_DOUBLE:
    case PROCESS_GABRIEL:
        if (!cube) reason = "нужен датчик куба";
        break;
    default:
        if (!cube) reason = "нужен датчик температуры";
        break;
    }

    if (reason) {
        snprintf(out->reason, sizeof(out->reason), "%s", reason);
        return;
    }
    out->ready = true;
}
