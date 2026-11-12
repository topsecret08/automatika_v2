#include "ui.h"
#include "ui_builder.h"
#include "ui_theme.h"

extern lv_obj_t* ui_make_page(const char *title);
extern void ui_scheme_show(const char *filename);

static void cb_scheme_open(void *ctx) {
    ui_scheme_show((const char*)ctx);
}

void ui_scheme_menu(void) {
    lv_obj_t *c = ui_make_page("Схемы установок");

    static ui_list_item_t items[] = {
        {"🍶","Дистилляция",NULL,NULL,0,0,0,NULL,NULL},
        {"⚗","Ректификация",NULL,NULL,0,0,0,NULL,NULL},
        {"🍺","Пивоварня",NULL,NULL,0,0,0,NULL,NULL},
        {"♨","ПВК",NULL,NULL,0,0,0,NULL,NULL},
        {"🏭","НБК",NULL,NULL,0,0,0,NULL,NULL},
        {"🧪","Вакуум",NULL,NULL,0,0,0,NULL,NULL},
        {"🥃","Двойная",NULL,NULL,0,0,0,NULL,NULL},
    };
    const char *files[] = {"distill.json", "rectify.json", "beer.json",
                          "pvk.json", "nbk.json", "vacuum.json", "double.json"};

    for (int i = 0; i < 7; i++) {
        items[i].on_click = cb_scheme_open;
        items[i].ctx = (void*)files[i];
    }

    ui_build_list(c, items, 7);
}