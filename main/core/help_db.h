#pragma once
#include <stdbool.h>
#include <stddef.h>

// Встроенная справка (работает без SD-карты).
// Полный набор статей (100+) лежит на SD: /sdcard/help/<id>.md

typedef struct {
    const char *id;          // совпадает с help_id в menu_tree.c
    const char *title;
    const char *category;
    const char *content_md;
} help_article_t;

extern const help_article_t HELP_ARTICLES[];
extern const size_t HELP_ARTICLES_COUNT;

void help_init(void);

// Поиск во встроенной базе. NULL, если статьи нет.
const help_article_t* help_find(const char *id);

// Показ статьи: встроенная → SD → сообщение об отсутствии.
bool help_show_article(const char *id);
