#include "pvk_presets.h"
#include <stddef.h>

static const pvk_preset_t PRESETS[] = {
    { "Затирание зерна",   65.0f, 0 },
    { "Варка пива",       100.0f, 2 },
    { "Дистилляция густая", 110.0f, 1 },
    { "Ректификация",     118.0f, 1 },
    { "Пивоварня кип.",   100.0f, 0 },
    { "Осахаривание",      63.0f, 0 },
    { "Стерилизация",     120.0f, 1 },
    { "Варка варенья",    105.0f, 0 },
    { "Томление",          85.0f, 0 },
    { "Кипячение",        100.0f, 0 },
};
#define N (sizeof(PRESETS)/sizeof(PRESETS[0]))

const pvk_preset_t* pvk_preset_get(int idx) {
    return (idx >= 0 && idx < (int)N) ? &PRESETS[idx] : NULL;
}
int pvk_preset_count(void) { return N; }