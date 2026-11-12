#pragma once
#include <stdbool.h>
#include <stddef.h>

// Обучающие модули: теория + вопрос с четырьмя вариантами ответа.
// Прогресс хранится в NVS (см. learning.c).

typedef struct {
    const char *title;
    const char *module;      // заполняется в learning_init()
    const char *theory_md;
    const char *quiz_q;
    const char *quiz_a[4];
    int         correct;     // индекс правильного ответа, 0..3
    const char *explanation;
} lesson_t;

typedef struct {
    const char *name;
    lesson_t    *lessons;
    int          count;
} module_t;

void           learning_init(void);
int            learning_module_count(void);
const module_t* learning_get_module(int idx);

void learning_start_module(size_t idx);
void learning_show_stats(void);
void learning_show_achievements(void);
void learning_show_certificate(void);

// Проверяет ответ и при правильном ответе сохраняет прогресс в NVS.
// Возвращает true, если ответ верный.
bool learning_answer(const lesson_t *lesson, int answer);
int  learning_passed_count(void);
bool learning_module_complete(int module_idx);
