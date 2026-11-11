// Иконки (эмодзи как текст)
// Файл для возможного расширения — сейчас пуст

#include "lvgl.h"

const char* ui_icon_get(const char *name) {
    // Заглушка — используем эмодзи напрямую в UI
    (void)name;
    return "●";
}