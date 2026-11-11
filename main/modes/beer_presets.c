#include "beer_presets.h"
#include <stddef.h>

// ==== Эль английский ====
static const beer_step_t EL[] = {
    { BST_MASH,  "Белковая пауза", 50.0f,  600, false, "" },
    { BST_MASH,  "β-амилаза",      63.0f, 2400, false, "" },
    { BST_MASH,  "α-амилаза",      72.0f, 1200, false, "" },
    { BST_MASH,  "Мэш-аут",        78.0f,  300, false, "" },
    { BST_BOIL,  "Кипячение",     100.0f, 3600, false, "" },
    { BST_HOP,   "Хмель 60",      100.0f,    0, false, "Горький" },
    { BST_HOP,   "Хмель 15",      100.0f, 2700, false, "Аромат" },
    { BST_HOP,   "Хмель 5",       100.0f,  600, false, "Вкус" },
    { BST_CHILL, "Охлаждение",     20.0f,    0, true,  "" },
};

// ==== Лагер ====
static const beer_step_t LAGER[] = {
    { BST_MASH, "Белковая пауза", 50.0f,  600, false, "" },
    { BST_MASH, "β-амилаза",      63.0f, 2400, false, "" },
    { BST_MASH, "α-амилаза",      72.0f, 1200, false, "" },
    { BST_MASH, "Мэш-аут",        78.0f,  300, false, "" },
    { BST_BOIL, "Кипячение",     100.0f, 3600, false, "" },
    { BST_HOP,  "Хмель 60",      100.0f,    0, false, "" },
    { BST_HOP,  "Хмель 15",      100.0f, 2700, false, "" },
    { BST_CHILL,"Охлаждение",     10.0f,    0, true,  "Лагер" },
};

// ==== IPA ====
static const beer_step_t IPA[] = {
    { BST_MASH, "Затирание",      65.0f, 3600, false, "" },
    { BST_MASH, "Мэш-аут",        78.0f,  300, false, "" },
    { BST_BOIL, "Кипячение",     100.0f, 3600, false, "" },
    { BST_HOP,  "Cascade 60",    100.0f,    0, false, "" },
    { BST_HOP,  "Centennial 15", 100.0f, 2700, false, "" },
    { BST_HOP,  "Citra 5",       100.0f,  600, false, "" },
    { BST_HOP,  "Dry hop",        20.0f, 259200, false, "3 дня" },
    { BST_CHILL,"Охлаждение",     20.0f,    0, true,  "" },
};

// ==== Стаут ====
static const beer_step_t STOUT[] = {
    { BST_MASH, "Затирание",      68.0f, 2700, false, "" },
    { BST_MASH, "Мэш-аут",        78.0f,  300, false, "" },
    { BST_BOIL, "Кипячение",     100.0f, 3600, false, "" },
    { BST_HOP,  "Хмель 60",      100.0f,    0, false, "" },
    { BST_HOP,  "Хмель 15",      100.0f, 2700, false, "" },
    { BST_CHILL,"Охлаждение",     20.0f,    0, true,  "" },
};

// ==== Пшеничное ====
static const beer_step_t WHEAT[] = {
    { BST_MASH, "Затирание",      50.0f,  900, false, "" },
    { BST_MASH, "Пшеничная",      65.0f, 2700, false, "" },
    { BST_MASH, "Мэш-аут",        78.0f,  300, false, "" },
    { BST_BOIL, "Кипячение",     100.0f, 3600, false, "" },
    { BST_HOP,  "Hallertau",     100.0f,    0, false, "" },
    { BST_CHILL,"Охлаждение",     20.0f,    0, true,  "" },
};

static const struct {
    const beer_step_t *steps;
    int count;
} PRESETS[] = {
    { EL,    sizeof(EL)/sizeof(*EL)       },
    { LAGER, sizeof(LAGER)/sizeof(*LAGER) },
    { IPA,   sizeof(IPA)/sizeof(*IPA)     },
    { STOUT, sizeof(STOUT)/sizeof(*STOUT) },
    { WHEAT, sizeof(WHEAT)/sizeof(*WHEAT) },
};
#define PRESET_COUNT (sizeof(PRESETS)/sizeof(PRESETS[0]))

const beer_step_t* beer_preset_get(int idx, int *out_count) {
    if (idx < 0 || idx >= PRESET_COUNT) {
        if (out_count) *out_count = 0;
        return NULL;
    }
    if (out_count) *out_count = PRESETS[idx].count;
    return PRESETS[idx].steps;
}

int beer_preset_count(void) { return PRESET_COUNT; }