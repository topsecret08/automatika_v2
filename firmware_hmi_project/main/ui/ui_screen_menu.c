/*
 * Экран 2: "Главное меню" 2x2.
 * Активная кнопка — с неоновой светящейся рамкой.
 */

#include "ui_screen_menu.h"
#include "ui_fonts.h"
#include "ui_futuristic.h"
#include "ui_metrics.h"

static lv_obj_t *s_scr;
static lv_obj_t *s_btn[UI_MENU_COUNT];
static ui_menu_item_t s_ids[UI_MENU_COUNT] = {
    UI_MENU_WORK, UI_MENU_PREP, UI_MENU_DIAG, UI_MENU_SYS
};

static lv_color_t c_white, c_muted, c_cyan;

static void init_colors(void)
{
    c_white = lv_color_hex(UI_WHITE);
    c_muted = lv_color_hex(UI_MUTED_BLUE);
    c_cyan  = lv_color_hex(UI_NEON_CYAN);
}

static lv_obj_t *make_row(lv_obj_t *parent)
{
    lv_obj_t *r = lv_obj_create(parent);
    lv_obj_set_width(r, LV_PCT(100));
    lv_obj_set_flex_grow(r, 1);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(r, 0, 0);
    lv_obj_set_style_pad_gap(r, 12, 0);
    lv_obj_set_style_bg_opa(r, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(r, 0, 0);
    lv_obj_clear_flag(r, LV_OBJ_FLAG_SCROLLABLE);
    return r;
}

lv_obj_t *ui_screen_menu_create(lv_event_cb_t on_select)
{
    init_colors();

    s_scr = lv_obj_create(NULL);
    lv_obj_set_size(s_scr, 800, 480);
    lv_obj_set_flex_flow(s_scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_scr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(s_scr, 12, 0);
    lv_obj_set_style_pad_gap(s_scr, 10, 0);
    lv_obj_set_style_bg_color(s_scr, lv_color_hex(0x050A14), 0);
    lv_obj_clear_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);

    ui_fut_header(s_scr, "ГЛАВНОЕ МЕНЮ", "ЭТАП: ТЕЛО");

    lv_obj_t *grid = lv_obj_create(s_scr);
    lv_obj_set_width(grid, LV_PCT(100));
    lv_obj_set_flex_grow(grid, 1);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(grid, 0, 0);
    lv_obj_set_style_pad_gap(grid, 12, 0);
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grid, 0, 0);

    lv_obj_t *row1 = make_row(grid);
    lv_obj_t *row2 = make_row(grid);

    s_btn[0] = ui_fut_menu_button(row1, LV_SYMBOL_PLAY,
        "Работа", "Управление процессом", true, on_select, &s_ids[0]);
    s_btn[1] = ui_fut_menu_button(row1, LV_SYMBOL_LIST,
        "Подготовка", "Профили · Калибровка", false, on_select, &s_ids[1]);
    s_btn[2] = ui_fut_menu_button(row2, LV_SYMBOL_WARNING,
        "Диагностика", "Журнал · Приводы · Сеть", false, on_select, &s_ids[2]);
    s_btn[3] = ui_fut_menu_button(row2, LV_SYMBOL_SETTINGS,
        "Система", "Обновление · API · Язык", false, on_select, &s_ids[3]);

    lv_obj_t *footer = lv_obj_create(s_scr);
    lv_obj_set_width(footer, LV_PCT(100));
    lv_obj_set_height(footer, 24);
    lv_obj_set_style_bg_opa(footer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(footer, 0, 0);
    lv_obj_set_style_pad_all(footer, 0, 0);
    lv_obj_t *hint = ui_fut_label(footer,
        "● ВХОД  •  УДЕРЖ. 2с — НАЗАД  •  ЭНКОДЕР — НАВИГАЦИЯ",
        &cyr_14, c_muted);
    lv_obj_center(hint);

    lv_scr_load(s_scr);
    return s_scr;
}

void ui_screen_menu_set_active(ui_menu_item_t item)
{
    if (item < 0 || item >= UI_MENU_COUNT) return;
    for (int i = 0; i < UI_MENU_COUNT; i++) {
        bool active = (i == (int)item);
        lv_obj_set_style_bg_color(s_btn[i],
            lv_color_hex(active ? UI_GLASS_ALT : UI_GLASS), 0);
        lv_obj_set_style_border_color(s_btn[i],
            active ? c_cyan : lv_color_hex(0x1E3A5F), 0);
        lv_obj_set_style_border_width(s_btn[i], active ? 2 : 1, 0);
        lv_obj_set_style_shadow_width(s_btn[i], active ? 28 : 0, 0);
        lv_obj_set_style_shadow_color(s_btn[i], c_cyan, 0);
        lv_obj_set_style_shadow_opa(s_btn[i], active ? LV_OPA_65 : LV_OPA_0, 0);
    }
}
