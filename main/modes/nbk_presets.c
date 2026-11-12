#include "nbk_presets.h"
#include <stddef.h>

static const nbk_preset_t PRESETS[] = {
    { "Сахарная быстрая",  96.0f,  8.0f },
    { "Зерновая плотная",  98.0f,  5.0f },
    { "Фруктовая",         95.0f,  6.0f },
    { "Производительная",  97.0f, 15.0f },
    { "Разгон",            95.0f,  3.0f },
    { "Промывка",          90.0f, 20.0f },
};
#define N (sizeof(PRESETS)/sizeof(PRESETS[0]))

const nbk_preset_t* nbk_preset_get(int idx) {
    return (idx >= 0 && idx < (int)N) ? &PRESETS[idx] : NULL;
}
int nbk_preset_count(void) { return N; }