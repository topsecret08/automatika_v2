#include "ui.h"
#include "ui_builder.h"
#include "ui_theme.h"
#include "core/help_db.h"
#include "core/sd_content.h"
#include <stdio.h>
#include <string.h>

extern lv_obj_t* ui_make_page(const char *title);

void ui_help_main(void) {
    lv_obj_t *c = ui_make_page("Справка");

    static ui_list_item_t items[] = {
        {"📖","Основы перегонки",NULL,NULL,0,0,0,NULL,NULL},
        {"🔧","Работа с автоматикой",NULL,NULL,0,0,0,NULL,NULL},
        {"⚙","Полное описание",NULL,NULL,0,0,0,NULL,NULL},
        {"🔧","Оборудование",NULL,NULL,0,0,0,NULL,NULL},
        {"🔌","Схемы подключения",NULL,NULL,0,0,0,NULL,NULL},
        {"🛡","Безопасность",NULL,NULL,0,0,0,NULL,NULL},
        {"🍶","Рецепты",NULL,NULL,0,0,0,NULL,NULL},
        {"🍺","Пивоварение",NULL,NULL,0,0,0,NULL,NULL},
        {"🧪","Вакуум",NULL,NULL,0,0,0,NULL,NULL},
        {"🥃","Двойная дистилляция",NULL,NULL,0,0,0,NULL,NULL},
        {"🏭","НБК",NULL,NULL,0,0,0,NULL,NULL},
        {"📊","Сравнение режимов",NULL,NULL,0,0,0,NULL,NULL},
    };
    ui_build_list(c, items, 12);
}

void ui_help_list_category(const char *cat) {
    (void)cat;
    ui_help_article_list();
}

void ui_help_article_list(void) {
    lv_obj_t *c = ui_make_page("Справка");

    static ui_list_item_t items[100];
    int n = 0;
    for (size_t i = 0; i < HELP_ARTICLES_COUNT && n < 100; i++) {
        items[n++] = (ui_list_item_t){
            .icon = "📖",
            .title = HELP_ARTICLES[i].title,
            .subtitle = HELP_ARTICLES[i].category,
            .on_click = [](void *ctx) {
                const help_article_t *a = (const help_article_t*)ctx;
                ui_show_markdown(a->title, a->content_md);
            },
            .ctx = (void*)&HELP_ARTICLES[i]
        };
    }

    // Дополнительно — статьи с SD
    static char names[64][64];
    int sd_n = sd_content_list(CT_HELP, names, 64);
    for (int i = 0; i < sd_n && n < 100; i++) {
        items[n].icon = "📄";
        items[n].title = names[i];
        items[n].subtitle = "SD-статья";
        items[n].on_click = [](void *ctx) {
            const char *name = (const char*)ctx;
            char path[256];
            snprintf(path, sizeof(path), "/sdcard/help/%s", name);
            ui_show_markdown_from_file(name, path);
        };
        items[n].ctx = names[i];
        n++;
    }

    ui_build_list(c, items, n);
}