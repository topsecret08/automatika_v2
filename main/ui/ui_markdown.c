#include "ui.h"
#include "ui_builder.h"
#include "ui_theme.h"
#include <stdio.h>

extern lv_obj_t* ui_make_page(const char *title);

// Markdown-рендер (упрощённый — просто текст с переносами)
void ui_markdown_render(const char *title, const char *md) {
    lv_obj_t *c = ui_make_page(title);
    lv_obj_t *l = lv_label_create(c);
    lv_label_set_text(l, md);
    lv_obj_set_width(l, LV_PCT(100));
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
    lv_obj_align(l, LV_ALIGN_TOP_LEFT, 0, 0);
}