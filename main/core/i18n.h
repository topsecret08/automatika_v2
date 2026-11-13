#pragma once

// Минимальная мультиязычность (RU/EN).
//
// tr() выбирает строку по текущему языку; для неизменяемых названий
// (например, "PID") передавайте один и тот же текст в оба аргумента.
// Язык хранится в NVS ("ui"/"lang") и меняется через /api/i18n.

typedef enum {
    LANG_RU = 0,
    LANG_EN = 1,
} lang_t;

void        i18n_init(void);
void        i18n_set_lang(lang_t lang);
lang_t      i18n_get_lang(void);
const char *i18n_lang_name(lang_t lang);

const char *tr(const char *ru, const char *en);
