#include "distill_presets.h"
#include <stddef.h>

static const distill_preset_t PRESETS[] = {
    { "Сахарная брага",  98.0f, 30.0f },
    { "Зерновая брага",  97.0f, 25.0f },
    { "Фруктовая",       96.0f, 20.0f },
    { "Виноградная",     95.0f, 25.0f },
    { "Медовая",         96.0f, 20.0f },
};
#define N (sizeof(PRESETS)/sizeof(PRESETS[0]))

const distill_preset_t* distill_preset_get(int idx) {
    return (idx >= 0 && idx < (int)N) ? &PRESETS[idx] : NULL;
}
int distill_preset_count(void) { return N; }