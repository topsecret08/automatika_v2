#include "menu_engine.h"
#include "roles.h"
#include "events.h"
#include "flags.h"
#include "help_db.h"
#include "ui/ui.h"
#include "ui/ui_theme.h"
#include "ui/ui_anim.h"
#include "nvs.h"
#include <string.h>
#include <stdio.h>

static nvs_handle_t g_nvs;

// Пул контекстов для callbacks (menu_item_t + menu_ctx_t)
typedef struct {
    const menu_item_t *item;
    menu_ctx_t        *ctx;
} menu_cb_t;

#define MENU_CB_POOL 128
static menu_cb_t s_cb_pool[MENU_CB_POOL];
static int s_cb_pool_idx = 0;

void menu_engine_init(menu_ctx_t *ctx, const menu_item_t *root, void *user_ctx) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->root = root;
    ctx->user_ctx = user_ctx;
    ctx->current_parent = root;
    nvs_open("menu", NVS_READWRITE, &g_nvs);
}

void menu_engine_open(menu_ctx_t *ctx, const menu_item_t *p) {
    if (ctx->depth < 16) ctx->stack[ctx->depth++] = ctx->current_parent;
    ctx->current_parent = p;
}

void menu_engine_back(menu_ctx_t *ctx) {
    if (ctx->depth > 0) ctx->current_parent = ctx->stack[--ctx->depth];
}

static void back_cb(lv_event_t *e) {
    menu_ctx_t *c = lv_event_get_user_data(e);
    menu_engine_back(c);
    menu_engine_render(c, c->list_container);
}

static void item_cb(lv_event_t *e) {
    menu_cb_t *d = (menu_cb_t*)lv_event_get_user_data(e);
    if (!d) return;
    const menu_item_t *it = d->item;
    menu_ctx_t *c = d->ctx;

    switch (it->type) {
    case MENU_ITEM_SUBMENU:
        menu_engine_open(c, it);
        menu_engine_render(c, c->list_container);
        break;

    case MENU_ITEM_ACTION:
    case MENU_ITEM_SCREEN:
        if (!roles_check(it->required_role)) {
            ui_toast("Недостаточно прав");
            events_log(EVENT_WARNING, "DENY %s", it->id);
            return;
        }
        if (it->on_action) it->on_action(c->user_ctx);
        break;

    case MENU_ITEM_TOGGLE: {
        if (!roles_check(it->required_role)) {
            ui_toast("Недостаточно прав");
            return;
        }
        uint8_t v = 0;
        nvs_get_u8(g_nvs, it->nvs_key, &v);
        v = !v;
        nvs_set_u8(g_nvs, it->nvs_key, v);
        nvs_commit(g_nvs);
        flags_set_by_key(it->nvs_key, v);
        events_log(EVENT_USER_ACTION, "TOGGLE %s=%d", it->nvs_key, v);
        menu_engine_render(c, c->list_container);
        break;
    }
    default: break;
    }
}

static void help_cb(lv_event_t *e) {
    const menu_item_t *it = lv_event_get_user_data(e);
    if (it && it->help_id) help_show_article(it->help_id);
}

void menu_engine_render(menu_ctx_t *ctx, lv_obj_t *parent) {
    if (!ctx || !parent) return;

    lv_obj_clean(parent);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(parent, 6, 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(parent, 0, 0);
    lv_obj_set_style_pad_all(parent, 8, 0);

    const menu_item_t *cur = ctx->current_parent;
    s_cb_pool_idx = 0;

    // Заголовок
    lv_obj_t *hdr = lv_label_create(parent);
    lv_label_set_text(hdr, cur->title_ru ? cur->title_ru : "Меню");
    ui_style_apply_title(hdr);
    lv_obj_align(hdr, LV_ALIGN_TOP_LEFT, 0, 0);

    // Кнопка "Назад"
    if (ctx->depth > 0) {
        lv_obj_t *btn = lv_btn_create(parent);
        lv_obj_set_size(btn, 60, 40);
        lv_obj_align(btn, LV_ALIGN_TOP_RIGHT, 0, 0);
        ui_style_apply_btn_secondary(btn);
        lv_obj_t *lb = lv_label_create(btn);
        lv_label_set_text(lb, LV_SYMBOL_LEFT);
        lv_obj_center(lb);
        lv_obj_add_event_cb(btn, back_cb, LV_EVENT_CLICKED, ctx);
    }

    if (cur->children) {
        for (uint16_t i = 0; i < cur->children_count; i++) {
            const menu_item_t *it = &cur->children[i];
            if (s_cb_pool_idx >= MENU_CB_POOL) break;

            // Текст
            char lbl[128];
            if (it->type == MENU_ITEM_TOGGLE) {
                uint8_t v = 0;
                nvs_get_u8(g_nvs, it->nvs_key, &v);
                snprintf(lbl, sizeof(lbl), "%s %s",
                         v ? LV_SYMBOL_OK : LV_SYMBOL_MINUS, it->title_ru);
            } else {
                snprintf(lbl, sizeof(lbl), "%s", it->title_ru);
            }

            // Кнопка-строка
            lv_obj_t *btn = lv_btn_create(parent);
            lv_obj_set_size(btn, LV_PCT(100), 56);
            ui_style_apply_card(btn);
            lv_obj_set_style_shadow_width(btn, 0, 0);
            lv_obj_set_style_pad_all(btn, 10, 0);

            // Иконка
            lv_obj_t *ico = lv_label_create(btn);
            lv_label_set_text(ico, it->icon ? it->icon : "");
            lv_obj_set_style_text_font(ico, &lv_font_montserrat_18, 0);
            lv_obj_align(ico, LV_ALIGN_LEFT_MID, 0, 0);

            // Название
            lv_obj_t *text = lv_label_create(btn);
            lv_label_set_text(text, lbl);
            lv_obj_set_style_text_font(text, &lv_font_montserrat_14, 0);
            lv_obj_align(text, LV_ALIGN_LEFT_MID, 30, 0);

            // Стрелка
            lv_obj_t *arr = lv_label_create(btn);
            lv_label_set_text(arr, LV_SYMBOL_RIGHT);
            lv_obj_set_style_text_color(arr, GLASS_GRAY, 0);
            lv_obj_align(arr, LV_ALIGN_RIGHT_MID, 0, 0);

            // Контекст
            menu_cb_t *d = &s_cb_pool[s_cb_pool_idx++];
            d->item = it;
            d->ctx = ctx;

            lv_obj_add_event_cb(btn, item_cb, LV_EVENT_CLICKED, d);
            if (it->help_id)
                lv_obj_add_event_cb(btn, help_cb, LV_EVENT_LONG_PRESSED, (void*)it);
        }
    } else if (cur->on_action) {
        cur->on_action(ctx->user_ctx);
    }
}