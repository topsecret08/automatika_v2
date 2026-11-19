#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"

typedef enum {
    MENU_ITEM_SUBMENU,
    MENU_ITEM_ACTION,
    MENU_ITEM_TOGGLE,
    MENU_ITEM_SLIDER,
    MENU_ITEM_VALUE,
    MENU_ITEM_SCREEN,
} menu_item_type_t;

typedef struct menu_item menu_item_t;

struct menu_item {
    const char *id;
    const char *icon;
    const char *title_ru;
    menu_item_type_t type;
    const menu_item_t *children;
    uint16_t children_count;
    void (*on_action)(void *ctx);
    const char *nvs_key;
    float min, max, step;
    const char *unit;
    uint8_t required_role;
    const char *help_id;
    uint8_t color_category;
};

typedef struct {
    const menu_item_t *root;
    const menu_item_t *stack[16];
    uint8_t depth;
    const menu_item_t *current_parent;
    lv_obj_t *list_container;
    void *user_ctx;
} menu_ctx_t;

void menu_engine_init(menu_ctx_t *ctx, const menu_item_t *root, void *user_ctx);
void menu_engine_open(menu_ctx_t *ctx, const menu_item_t *parent);
void menu_engine_back(menu_ctx_t *ctx);
void menu_engine_render(menu_ctx_t *ctx, lv_obj_t *parent);

// Тумблеры меню по всему дереву (для API). Записывают то же, что и экран.
int          menu_engine_list_toggles(const menu_item_t **out, int max);
const menu_item_t *menu_engine_find_toggle(const char *key);
bool         menu_engine_toggle_get(const char *key, uint8_t *value);
bool         menu_engine_toggle_set(const char *key, uint8_t value);