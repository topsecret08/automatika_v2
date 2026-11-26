#include "screen_registry.h"
#include <string.h>
#include <stdio.h>

static char s_name_buf[24];

const char *hmi_screen_name(hmi_screen_id_t id) {
    if (id >= HMI_SCR_COUNT) return "none";
    snprintf(s_name_buf, sizeof(s_name_buf), "scr_%u", (unsigned)id);
    return s_name_buf;
}

hmi_screen_id_t hmi_screen_from_name(const char *name) {
    if (!name) return HMI_SCR_NONE;
    unsigned id = 0;
    if (sscanf(name, "scr_%u", &id) == 1 && id < HMI_SCR_COUNT) {
        return (hmi_screen_id_t)id;
    }
    return HMI_SCR_NONE;
}
