#pragma once
#include <stddef.h>

// Встроенные рецепты. Полные наборы лежат на SD:
// /sdcard/recipes/{spirits,mash,beer}.json

typedef struct {
    const char *id;
    const char *name;
    const char *category;     // "spirit" | "mash" | "beer"
    const char *description;
    const char *ingredients;
    const char *steps_md;
} recipe_t;

extern const recipe_t RECIPES[];
extern const size_t RECIPES_COUNT;

void              recipes_init(void);
const recipe_t*   recipes_find(const char *id);

// Показ карточки рецепта (UI).
void              recipes_show(const recipe_t *r);
