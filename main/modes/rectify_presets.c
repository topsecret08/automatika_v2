#include "rectify_presets.h"
#include <stddef.h>

static const rectify_preset_t PRESETS[] = {
    { "Сахарная 20л",        82.0f, 77.0f, 30.0f },
    { "Зерновая 30л",        83.0f, 77.5f, 40.0f },
    { "Пивной дистиллят",    80.0f, 76.5f, 25.0f },
    { "Виноградная",         81.0f, 77.0f, 30.0f },
    { "Фруктовая",           79.0f, 76.0f, 25.0f },
};
#define N (sizeof(PRESETS)/sizeof(PRESETS[0]))

const rectify_preset_t* rectify_preset_get(int idx) {
    return (idx >= 0 && idx < (int)N) ? &PRESETS[idx] : NULL;
}
int rectify_preset_count(void) { return N; }