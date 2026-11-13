#include "learning.h"
#include "ui/ui.h"
#include "esp_log.h"
#include "nvs.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static const char *TAG    = "LEARN";
static const char *NVS_NS = "learning";
static const char *NVS_KEY = "progress";

#define LESSONS_PER_MODULE 4

static lesson_t s_l1[LESSONS_PER_MODULE] = {
    {
        .title = "Что такое перегонка?",
        .theory_md =
            "Разделение жидкостей по температуре кипения.\n"
            "Этанол кипит при 78,4 °C, вода — при 100 °C.\n"
            "Поэтому спирт уходит в пар раньше воды.",
        .quiz_q = "При какой температуре кипит этанол?",
        .quiz_a = {"100 °C", "78,4 °C", "50 °C", "120 °C"},
        .correct = 1,
        .explanation = "78,4 °C — ключевая цифра для всей перегонки.",
    },
    {
        .title = "Головы, тело, хвосты",
        .theory_md =
            "Погон делится на три части:\n"
            "- головы — 5…10 % объёма, пить нельзя;\n"
            "- тело — основная фракция;\n"
            "- хвосты — отбираются после 92 °C.",
        .quiz_q = "Сколько обычно отбирают голов?",
        .quiz_a = {"1 %", "5–10 %", "20 %", "50 %"},
        .correct = 1,
        .explanation = "5–10 % от расчётного спирта — стандарт.",
    },
    {
        .title = "Температуры отбора",
        .theory_md =
            "Ориентиры по пару: головы до 78 °C, тело 78…92 °C,\n"
            "хвосты выше 92 °C. Значения зависят от браги и давления.",
        .quiz_q = "Когда начинаются хвосты?",
        .quiz_a = {"78 °C", "85 °C", "92 °C", "100 °C"},
        .correct = 2,
        .explanation = "92 °C — практическая граница тела.",
    },
    {
        .title = "Крепость и объём",
        .theory_md =
            "Выход спирта = объём браги × крепость × КПД.\n"
            "Из 20 л браги 10 % об. получается около 2 л спирта-сырца.",
        .quiz_q = "Сколько спирта в 20 л браги крепостью 10 %?",
        .quiz_a = {"0,5 л", "1 л", "2 л", "5 л"},
        .correct = 2,
        .explanation = "20 × 0,10 = 2 л абсолютного спирта.",
    },
};

static lesson_t s_l2[LESSONS_PER_MODULE] = {
    {
        .title = "Дистилляция",
        .theory_md =
            "Простая перегонка сохраняет ароматы браги.\n"
            "Продукт получается 40–70 % об. с выраженным вкусом.",
        .quiz_q = "Что даёт дистилляция?",
        .quiz_a = {"96 %", "40–70 % с ароматами", "Воду", "Ничего"},
        .correct = 1,
        .explanation = "Ароматика — главное отличие от ректификации.",
    },
    {
        .title = "Ректификация",
        .theory_md =
            "Глубокая очистка на колонне до 96,6 %.\n"
            "Мощность постоянная, регулируется скорость отбора.",
        .quiz_q = "Чем регулируют ректификацию?",
        .quiz_a = {"Мощностью", "Скоростью отбора", "Давлением", "Хмелем"},
        .correct = 1,
        .explanation = "Менять мощность во время отбора нельзя.",
    },
    {
        .title = "Пивоварение",
        .theory_md =
            "Затирание → кипячение с хмелем → охлаждение → брожение.\n"
            "Паузы задают тело и сбраживаемость будущего пива.",
        .quiz_q = "При какой температуре работает β-амилаза?",
        .quiz_a = {"50 °C", "63 °C", "72 °C", "78 °C"},
        .correct = 1,
        .explanation = "63 °C — максимум β-амилазы, больше сбраживаемых сахаров.",
    },
    {
        .title = "Брожение",
        .theory_md =
            "Термостатирование поддерживает температуру брожения.\n"
            "Для элей 18…22 °C, для лагеров 8…12 °C.",
        .quiz_q = "Типичная температура для эля?",
        .quiz_a = {"4 °C", "10 °C", "20 °C", "35 °C"},
        .correct = 2,
        .explanation = "18…22 °C — рабочий диапазон дрожжей для эля.",
    },
};

static lesson_t s_l3[LESSONS_PER_MODULE] = {
    {
        .title = "ПВК",
        .theory_md =
            "Пароводяной котёл греет густые браги через рубашку.\n"
            "Пригорания нет: пар не даёт локального перегрева.",
        .quiz_q = "Что можно перегонять в ПВК?",
        .quiz_a = {"Только сахарную", "Густые зерновые", "Только воду", "Спирт"},
        .correct = 1,
        .explanation = "Основной сценарий ПВК — густые зерновые браги.",
    },
    {
        .title = "Метод Габриэля",
        .theory_md =
            "Дробная перегонка на три части.\n"
            "T1 = V × K, где K = 0,30 + ABV / 100 × 0,25.",
        .quiz_q = "Сколько частей даёт метод Габриэля?",
        .quiz_a = {"1", "2", "3", "4"},
        .correct = 2,
        .explanation = "Головы, тело и хвосты — три части.",
    },
    {
        .title = "НБК",
        .theory_md =
            "Непрерывная бражная колонна работает без остановок.\n"
            "Брага подаётся постоянно, отбор идёт непрерывно.",
        .quiz_q = "Что подаётся в НБК постоянно?",
        .quiz_a = {"Продукт", "Брага", "Только вода", "Только пар"},
        .correct = 1,
        .explanation = "Непрерывный режим — главное отличие НБК.",
    },
    {
        .title = "Вакуум",
        .theory_md =
            "При пониженном давлении температура кипения падает:\n"
            "на 100 мбар спирт кипит около 40 °C.",
        .quiz_q = "Температура кипения при 100 мбар?",
        .quiz_a = {"30 °C", "40 °C", "60 °C", "78 °C"},
        .correct = 1,
        .explanation = "40 °C — мягкий режим для ароматных дистиллятов.",
    },
};

static lesson_t s_l4[LESSONS_PER_MODULE] = {
    {
        .title = "Правила безопасности",
        .theory_md =
            "Никогда не перекрывайте выход пара.\n"
            "При запахе спирта — стоп и проветривание.",
        .quiz_q = "Что делать при запахе спирта в помещении?",
        .quiz_a = {"Закурить", "Проветрить и остановить процесс", "Продолжить", "Игнорировать"},
        .correct = 1,
        .explanation = "Спиртовые пары взрывоопасны.",
    },
    {
        .title = "Аварийные пороги",
        .theory_md =
            "Куб — до 130 °C, колонна — до 105 °C,\n"
            "давление — до 3 бар, минимальный расход — 0,5 л/мин.",
        .quiz_q = "Максимальное давление для системы?",
        .quiz_a = {"1 бар", "3 бар", "5 бар", "10 бар"},
        .correct = 1,
        .explanation = "Порог задан в config.h: SAFE_MAX_PRESSURE 3.0f.",
    },
    {
        .title = "Сторожевой таймер",
        .theory_md =
            "Watchdog следит за десятью задачами.\n"
            "Если задача зависла, событие попадает в журнал.",
        .quiz_q = "Зачем нужен watchdog?",
        .quiz_a = {"Для красоты", "Чтобы найти зависшую задачу", "Для нагрева", "Для Wi-Fi"},
        .correct = 1,
        .explanation = "Он фиксирует зависания и помогает уйти в безопасный режим.",
    },
    {
        .title = "Электрика",
        .theory_md =
            "ESP32-S3 питается от 3,3 В, силовая часть — отдельно.\n"
            "Общий минус, развязка по питанию, предохранитель на нагреве.",
        .quiz_q = "Что обязательно для силовой части?",
        .quiz_a = {"Красивый корпус", "Предохранитель и заземление", "Wi-Fi", "Экран"},
        .correct = 1,
        .explanation = "Без этого нельзя эксплуатировать нагрев.",
    },
};

static lesson_t s_l5[LESSONS_PER_MODULE] = {
    {
        .title = "Что такое PID",
        .theory_md =
            "Пропорционально-интегрально-дифференциальный регулятор.\n"
            "Держит температуру без «качелей».",
        .quiz_q = "Где PID применяют чаще всего?",
        .quiz_a = {"Брожение и куб", "Вакуум", "Wi-Fi", "Экран"},
        .correct = 0,
        .explanation = "Основные контуры — нагрев куба и термостатирование.",
    },
    {
        .title = "Автотюнинг PID",
        .theory_md =
            "Релейный метод Циглера — Никольса: регулятор сам раскачивает\n"
            "температуру и по периоду колебаний вычисляет Kp, Ki, Kd.",
        .quiz_q = "Какой метод автотюнинга используется?",
        .quiz_a = {"Ziegler–Nichols", "Ньютон", "Эйлер", "Рунге — Кутта"},
        .correct = 0,
        .explanation = "Релейный Z-N — стандарт для нагрева с инерцией.",
    },
    {
        .title = "Предекремент",
        .theory_md =
            "Упреждающее снижение отбора перед скачком температуры.\n"
            "Сглаживает переход тело → хвосты.",
        .quiz_q = "Зачем нужен предекремент?",
        .quiz_a = {"Быстрее", "Сглаживает скачок температуры", "Меньше спирта", "Красивее"},
        .correct = 1,
        .explanation = "Он компенсирует инерцию колонны.",
    },
    {
        .title = "Авто-92+",
        .theory_md =
            "Расчёт отбора по формуле V = Vmax × (6,04 − 0,06 × T).\n"
            "При 90 °C множитель 0,64.",
        .quiz_q = "Множитель при 90 °C?",
        .quiz_a = {"0,64", "0,5", "0,8", "0,9"},
        .correct = 0,
        .explanation = "6,04 − 0,06 × 90 = 0,64.",
    },
};

static lesson_t s_l6[LESSONS_PER_MODULE] = {
    {
        .title = "Экспорт и импорт",
        .theory_md =
            "Настройки, рецепты и история выгружаются на SD.\n"
            "Формат — JSON (рецепты) и бинарный образ с CRC (бэкап).",
        .quiz_q = "Формат пользовательских рецептов?",
        .quiz_a = {"JSON", "BIN+CRC", "CSV", "XML"},
        .correct = 0,
        .explanation = "Рецепты — JSON, полный бэкап — BIN с контрольной суммой.",
    },
    {
        .title = "Режимы работы",
        .theory_md =
            "Восемь режимов: дистилляция, ректификация, пиво, ПВК,\n"
            "НБК, вакуум, двойная перегонка и брожение.",
        .quiz_q = "Сколько режимов работы?",
        .quiz_a = {"4", "6", "8", "10"},
        .correct = 2,
        .explanation = "Восемь — от дистилляции до брожения.",
    },
    {
        .title = "Схемы подключения",
        .theory_md =
            "Схемы лежат в /sdcard/schemes/ и открываются\n"
            "в разделе «Схемы» — с подписями узлов.",
        .quiz_q = "Где хранятся схемы?",
        .quiz_a = {"/sdcard/schemes/", "/lfs/", "В NVS", "В прошивке"},
        .correct = 0,
        .explanation = "Схемы — обычные JSON-файлы на карте.",
    },
    {
        .title = "С чего начать",
        .theory_md =
            "Первая практика — сахарная брага и дистилляция.\n"
            "Дальше — ректификация и дробные перегонки.",
        .quiz_q = "С чего лучше начать новичку?",
        .quiz_a = {"Ректификация", "Сахарная брага", "ПВК", "НБК"},
        .correct = 1,
        .explanation = "Сахарная брага прощает ошибки и дешева.",
    },
};

static module_t s_modules[] = {
    {"Модуль 1: Основы",       s_l1, LESSONS_PER_MODULE},
    {"Модуль 2: Режимы",       s_l2, LESSONS_PER_MODULE},
    {"Модуль 3: Продвинутые",  s_l3, LESSONS_PER_MODULE},
    {"Модуль 4: Безопасность", s_l4, LESSONS_PER_MODULE},
    {"Модуль 5: Настройка",    s_l5, LESSONS_PER_MODULE},
    {"Модуль 6: Дополнительно",s_l6, LESSONS_PER_MODULE},
};

#define MODULE_COUNT ((int)(sizeof(s_modules) / sizeof(s_modules[0])))

// Битовая маска пройденных уроков для каждого модуля.
static uint8_t s_passed[MODULE_COUNT];

static void learning_save(void) {
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_blob(h, NVS_KEY, s_passed, sizeof(s_passed));
    nvs_commit(h);
    nvs_close(h);
}

static bool learning_find(const lesson_t *lesson, int *module, int *index) {
    if (!lesson) return false;
    for (int m = 0; m < MODULE_COUNT; m++) {
        for (int i = 0; i < s_modules[m].count; i++) {
            if (&s_modules[m].lessons[i] == lesson) {
                *module = m;
                *index  = i;
                return true;
            }
        }
    }
    return false;
}

void learning_init(void) {
    int total = 0;
    for (int m = 0; m < MODULE_COUNT; m++) {
        for (int i = 0; i < s_modules[m].count; i++) {
            s_modules[m].lessons[i].module = s_modules[m].name;
            total++;
        }
    }

    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) == ESP_OK) {
        size_t sz = sizeof(s_passed);
        nvs_get_blob(h, NVS_KEY, s_passed, &sz);
        nvs_close(h);
    }

    ESP_LOGI(TAG, "Модулей: %d, уроков: %d, пройдено: %d",
             MODULE_COUNT, total, learning_passed_count());
}

int learning_module_count(void) { return MODULE_COUNT; }

const module_t* learning_get_module(int idx) {
    if (idx < 0 || idx >= MODULE_COUNT) return NULL;
    return &s_modules[idx];
}

void learning_start_module(size_t idx) {
    if (idx >= (size_t)MODULE_COUNT) {
        ui_toast("Модуль не найден");
        return;
    }
    ui_learning_show_lessons(&s_modules[idx]);
}

bool learning_answer(const lesson_t *lesson, int answer) {
    int m, i;
    if (!learning_find(lesson, &m, &i)) return false;

    if (answer != lesson->correct) return false;

    uint8_t bit = (uint8_t)(1u << (i & 7));
    if (!(s_passed[m] & bit)) {
        s_passed[m] |= bit;
        learning_save();
        ESP_LOGI(TAG, "Урок пройден: %s / %s", s_modules[m].name, lesson->title);
    }
    return true;
}

int learning_passed_count(void) {
    int n = 0;
    for (int m = 0; m < MODULE_COUNT; m++) {
        for (int i = 0; i < s_modules[m].count && i < 8; i++) {
            if (s_passed[m] & (1u << i)) n++;
        }
    }
    return n;
}

bool learning_module_complete(int module_idx) {
    if (module_idx < 0 || module_idx >= MODULE_COUNT) return false;
    uint8_t mask = (s_modules[module_idx].count >= 8)
                 ? 0xFF
                 : (uint8_t)((1u << s_modules[module_idx].count) - 1);
    return (s_passed[module_idx] & mask) == mask;
}

void learning_show_stats(void) {
    int lessons = 0;
    for (int m = 0; m < MODULE_COUNT; m++) lessons += s_modules[m].count;

    char buf[768];
    int off = snprintf(buf, sizeof(buf),
             "# Статистика обучения\n\n"
             "- Модулей: **%d**\n"
             "- Уроков: **%d**\n"
             "- Пройдено: **%d**\n\n"
             "Прогресс сохраняется в NVS при верном ответе.\n\n",
             MODULE_COUNT, lessons, learning_passed_count());

    for (int m = 0; m < MODULE_COUNT && off > 0 && off < (int)sizeof(buf); m++) {
        int done = 0;
        for (int i = 0; i < s_modules[m].count && i < 8; i++) {
            if (s_passed[m] & (1u << i)) done++;
        }
        off += snprintf(buf + off, sizeof(buf) - (size_t)off,
                        "- %s: %d/%d %s\n",
                        s_modules[m].name, done, s_modules[m].count,
                        learning_module_complete(m) ? "✅" : "");
    }
    ui_show_markdown("Статистика", buf);
}

void learning_show_achievements(void) {
    ui_show_markdown("Достижения",
        "# Достижения\n\n"
        "- 🥇 **Первый урок** — пройден любой урок\n"
        "- 🔥 **Пять подряд** — пять верных ответов без ошибки\n"
        "- 📖 **Теоретик** — пройдены все модули\n"
        "- 🎯 **Практик** — пройдена калибровка клапанов\n"
        "- 🛡 **Безопасник** — пройден модуль безопасности\n\n"
        "Достижения выдаются автоматически во время обучения.");
}

void learning_show_certificate(void) {
    bool complete = true;
    for (int m = 0; m < MODULE_COUNT; m++) {
        if (!learning_module_complete(m)) { complete = false; break; }
    }

    if (complete) {
        ui_show_markdown("Сертификат",
            "# Сертификат\n\n"
            "Курс пройден полностью — все шесть модулей.\n\n"
            "**Automatika V0.5 alfa**\n"
            "Курс: основы дистилляции и ректификации\n\n"
            "🏆 Поздравляем!");
        return;
    }

    char buf[512];
    snprintf(buf, sizeof(buf),
        "# Сертификат\n\n"
        "Сертификат выдаётся после прохождения всех шести модулей.\n\n"
        "Сейчас пройдено **%d** уроков.\n\n"
        "**Automatika V0.5 alfa**\n"
        "Курс: основы дистилляции и ректификации",
        learning_passed_count());
    ui_show_markdown("Сертификат", buf);
}
