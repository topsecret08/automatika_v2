#include "telegram_bot.h"

#include "config.h"
#include "core/control_config.h"
#include "core/events.h"
#include "core/estop.h"
#include "core/flags.h"
#include "core/health.h"
#include "core/process_control.h"
#include "core/profiles.h"
#include "core/state.h"
#include "core/watchdog.h"
#include "control/ctrl_heater.h"
#include "control/ctrl_safety.h"
#include "control/fraction_logic.h"
#include "control/fraction_periodic.h"
#include "control/param_registry.h"
#include "control/regulator_driver.h"
#include "control/safety_gate.h"
#include "control/alcohol_balance.h"
#include "control/pid_autotune.h"

#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "TG_BOT";

#define TG_RESPONSE_MAX (64 * 1024)
#define TG_POLL_TIMEOUT_S 25

static const char *KEYBOARD_MAIN =
    "{\"inline_keyboard\":["
    "[{\"text\":\"▶ Работа\",\"callback_data\":\"menu_work\"}],"
    "[{\"text\":\"⚙ Подготовка\",\"callback_data\":\"menu_prepare\"}],"
    "[{\"text\":\"🔍 Диагностика\",\"callback_data\":\"menu_diagnose\"}],"
    "[{\"text\":\"🛠 Система\",\"callback_data\":\"menu_system\"}]"
    "]}";

static const char *KEYBOARD_STATUS =
    "{\"inline_keyboard\":["
    "[{\"text\":\"Статус\",\"callback_data\":\"status\"},{\"text\":\"Фракция\",\"callback_data\":\"fraction\"}],"
    "[{\"text\":\"T фиксации\",\"callback_data\":\"edit_t_fix\"},{\"text\":\"Время открыт\",\"callback_data\":\"edit_open\"}],"
    "[{\"text\":\"Время закрыт\",\"callback_data\":\"edit_close\"},{\"text\":\"Гистерезис\",\"callback_data\":\"edit_hyst\"}],"
    "[{\"text\":\"Главное меню\",\"callback_data\":\"menu_main\"}]"
    "]}";

static const char *KEYBOARD_WORK =
    "{\"inline_keyboard\":["
    "[{\"text\":\"Дашборд\",\"callback_data\":\"status\"}],"
    "[{\"text\":\"Текущая фракция\",\"callback_data\":\"fraction\"}],"
    "[{\"text\":\"Запуск\",\"callback_data\":\"run\"},"
    "{\"text\":\"Стоп\",\"callback_data\":\"stop\"}],"
    "[{\"text\":\"Сбросить E-STOP\",\"callback_data\":\"estop_ack\"}],"
    "[{\"text\":\"← Назад\",\"callback_data\":\"menu_main\"}]"
    "]}";

static const char *KEYBOARD_PREPARE =
    "{\"inline_keyboard\":["
    "[{\"text\":\"Профили\",\"callback_data\":\"profiles\"}],"
    "[{\"text\":\"Калибровка насоса\",\"callback_data\":\"calibrate\"}],"
    "[{\"text\":\"PID-автотюн\",\"callback_data\":\"autotune\"}],"
    "[{\"text\":\"Проверка датчиков\",\"callback_data\":\"sensors\"}],"
    "[{\"text\":\"← Назад\",\"callback_data\":\"menu_main\"}]"
    "]}";

static const char *KEYBOARD_DIAGNOSE =
    "{\"inline_keyboard\":["
    "[{\"text\":\"Журнал событий\",\"callback_data\":\"log\"}],"
    "[{\"text\":\"Проверка приводов\",\"callback_data\":\"actuators\"}],"
    "[{\"text\":\"Состояние\",\"callback_data\":\"health\"}],"
    "[{\"text\":\"Сеть\",\"callback_data\":\"network\"}],"
    "[{\"text\":\"← Назад\",\"callback_data\":\"menu_main\"}]"
    "]}";

static const char *KEYBOARD_SYSTEM =
    "{\"inline_keyboard\":["
    "[{\"text\":\"Обновление\",\"callback_data\":\"ota\"}],"
    "[{\"text\":\"API-ключ\",\"callback_data\":\"api\"}],"
    "[{\"text\":\"Язык\",\"callback_data\":\"lang\"}],"
    "[{\"text\":\"О устройстве\",\"callback_data\":\"about\"}],"
    "[{\"text\":\"← Назад\",\"callback_data\":\"menu_main\"}]"
    "]}";

// ------------------------------------------------------------------
// Вспомогательные
// ------------------------------------------------------------------
static const char *fraction_ru(fraction_t f) {
    switch (f) {
    case FRAC_HEADS: return "головы";
    case FRAC_BODY:  return "тело";
    case FRAC_TAILS: return "хвосты";
    case FRAC_DONE:  return "готово";
    default:         return "нет данных";
    }
}

static bool chat_id_of(cJSON *chat, char *out, size_t out_len) {
    if (!chat || !out || out_len == 0) return false;

    cJSON *id = cJSON_GetObjectItemCaseSensitive(chat, "id");
    if (cJSON_IsString(id) && id->valuestring) {
        strncpy(out, id->valuestring, out_len - 1);
        out[out_len - 1] = '\0';
        return true;
    }
    if (cJSON_IsNumber(id)) {
        snprintf(out, out_len, "%.0f", id->valuedouble);
        return true;
    }
    return false;
}

static bool authorized(const char *chat) {
    char configured[32] = "";
    char token[128] = "";
    telegram_get_credentials(token, sizeof(token), configured, sizeof(configured));
    return configured[0] && chat && strcmp(configured, chat) == 0;
}

// ------------------------------------------------------------------
// HTTP
// ------------------------------------------------------------------
static int tg_post(const char *method, cJSON *payload) {
    char token[128] = "";
    char chat[32] = "";
    telegram_get_credentials(token, sizeof(token), chat, sizeof(chat));
    if (!token[0] || !payload) return -1;

    char url[256];
    snprintf(url, sizeof(url), "https://api.telegram.org/bot%s/%s",
             token, method);

    char *body = cJSON_PrintUnformatted(payload);
    if (!body) return -1;

    esp_http_client_config_t cfg = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 10000,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) {
        cJSON_free(body);
        return -1;
    }

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, body, (int)strlen(body));

    esp_err_t err = esp_http_client_perform(client);
    int status = (err == ESP_OK) ? esp_http_client_get_status_code(client) : -1;
    esp_http_client_cleanup(client);
    cJSON_free(body);

    if (status != 200) {
        events_log(EVENT_WARNING, "Telegram %s: %s HTTP %d",
                   method, esp_err_to_name(err), status);
    }
    return status;
}

static void tg_send(const char *chat, const char *text, const char *keyboard_json) {
    if (!chat || !text) return;

    cJSON *root = cJSON_CreateObject();
    if (!root) return;

    cJSON_AddStringToObject(root, "chat_id", chat);
    cJSON_AddStringToObject(root, "text", text);
    cJSON_AddBoolToObject(root, "disable_web_page_preview", true);

    if (keyboard_json) {
        cJSON *keyboard = cJSON_Parse(keyboard_json);
        if (keyboard) {
            cJSON_AddItemToObject(root, "reply_markup", keyboard);
        }
    }

    tg_post("sendMessage", root);
    cJSON_Delete(root);
}

static void tg_answer(const char *callback_id, const char *text) {
    if (!callback_id) return;
    cJSON *root = cJSON_CreateObject();
    if (!root) return;
    cJSON_AddStringToObject(root, "callback_query_id", callback_id);
    if (text && text[0]) cJSON_AddStringToObject(root, "text", text);
    tg_post("answerCallbackQuery", root);
    cJSON_Delete(root);
}

static char *tg_get_updates(const char *token, long long offset) {
    char url[256];
    snprintf(url, sizeof(url),
             "https://api.telegram.org/bot%s/getUpdates?offset=%lld&timeout=%d",
             token, offset, TG_POLL_TIMEOUT_S);

    esp_http_client_config_t cfg = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = (TG_POLL_TIMEOUT_S + 10) * 1000,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) return NULL;

    char *buffer = NULL;
    if (esp_http_client_open(client, 0) == ESP_OK) {
        (void)esp_http_client_fetch_headers(client);

        size_t capacity = 4096;
        size_t length = 0;
        buffer = malloc(capacity);
        if (buffer) {
            char chunk[1024];
            while (1) {
                int n = esp_http_client_read(client, chunk, sizeof(chunk));
                if (n <= 0) break;

                if (length + (size_t)n + 1 > capacity) {
                    size_t next = capacity * 2;
                    if (next > TG_RESPONSE_MAX) next = TG_RESPONSE_MAX;
                    if (length + (size_t)n + 1 > next) break;

                    char *grown = realloc(buffer, next);
                    if (!grown) break;
                    buffer = grown;
                    capacity = next;
                }
                memcpy(buffer + length, chunk, (size_t)n);
                length += (size_t)n;
            }
            buffer[length] = '\0';
        }
    }

    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return buffer;
}

// ------------------------------------------------------------------
// Тексты
// ------------------------------------------------------------------
static void send_help(const char *chat) {
    const char *text =
        "Automatika V0.6 beta\n\n"
        "Выберите задачу. Внутри группы — действия; текстовые команды\n"
        "/status, /run, /stop, /power, /param, /algo, /regulator, /log\n"
        "сохранены для совместимости.";
    tg_send(chat, text, KEYBOARD_MAIN);
}

static void send_work_menu(const char *chat) {
    tg_send(chat, "Работа\nУправление процессом:", KEYBOARD_WORK);
}

static void send_prepare_menu(const char *chat) {
    tg_send(chat, "Подготовка\nНастройка перед запуском:", KEYBOARD_PREPARE);
}

static void send_diagnose_menu(const char *chat) {
    tg_send(chat, "Диагностика\nПроверка состояния:", KEYBOARD_DIAGNOSE);
}

static void send_system_menu(const char *chat) {
    tg_send(chat, "Система\nОбслуживание и доступ:", KEYBOARD_SYSTEM);
}

static void send_sensors(const char *chat) {
    process_state_t *s = state_get();
    char buf[320];
    snprintf(buf, sizeof(buf),
             "Проверка датчиков\n"
             "Куб: %.1f °C\n"
             "Пар: %.1f °C\n"
             "Давление: %.3f бар\n"
             "Проток: %.2f л/мин",
             s->cube_temp, s->column_temp, s->pressure_bar, s->flow_lpm);
    tg_send(chat, buf, KEYBOARD_PREPARE);
}

static void send_health(const char *chat) {
    char buf[320];
    snprintf(buf, sizeof(buf),
             "Состояние: %s\nПричина: %s\nE-STOP: %s\nSafety: %s",
             health_level_name(),
             health_last_reason()[0] ? health_last_reason() : "—",
             estop_is_pressed() ? "активен" : "нет",
             ctrl_safety_ok() ? "ok" : "сработала");
    tg_send(chat, buf, KEYBOARD_DIAGNOSE);
}

static void send_network(const char *chat) {
    int rssi = 0;
    cJSON *root = cJSON_Parse(health_json());
    if (root) {
        cJSON *v = cJSON_GetObjectItemCaseSensitive(root, "wifi_rssi");
        if (cJSON_IsNumber(v)) rssi = v->valueint;
        cJSON_Delete(root);
    }
    char buf[160];
    snprintf(buf, sizeof(buf), "Wi-Fi RSSI: %d dBm\nMQTT/Telegram — в веб-интерфейсе.", rssi);
    tg_send(chat, buf, KEYBOARD_DIAGNOSE);
}

static void send_autotune_status(const char *chat) {
    process_state_t *s = state_get();
    char buf[220];
    snprintf(buf, sizeof(buf),
             "PID-автотюн: %s\nKp %.3f / Ki %.4f / Kd %.3f\n\n"
             "Запуск и остановка — в PWA: Подготовка → PID-автотюн.",
             pid_autotune_is_running() ? "выполняется" :
             (pid_autotune_done() ? "готово" : "ожидание"),
             s->pid_kp, s->pid_ki, s->pid_kd);
    tg_send(chat, buf, KEYBOARD_PREPARE);
}

static void send_about(const char *chat) {
    int64_t uptime = esp_timer_get_time() / 1000000;
    char buf[220];
    snprintf(buf, sizeof(buf),
             "%s\nВерсия: %s\nUptime: %lld ч %lld мин\nHeap: %u КБ",
             FW_NAME, FW_VERSION,
             (long long)(uptime / 3600), (long long)((uptime % 3600) / 60),
             (unsigned)(esp_get_free_heap_size() / 1024));
    tg_send(chat, buf, KEYBOARD_SYSTEM);
}

static void send_status(const char *chat) {
    process_state_t *s = state_get();

    char buf[768];
    snprintf(buf, sizeof(buf),
        "Automatika\n"
        "Режим: %s\n"
        "Этап: %s\n"
        "Куб: %.1f °C\n"
        "Пар: %.1f °C\n"
        "Мощность: %.0f %%\n"
        "Фракция: %s\n"
        "Отбор: %.1f мл/мин\n"
        "Клапан: %s\n"
        "Остаток спирта: %.0f мл (%.0f %%)\n"
        "E-STOP: %s\n"
        "Защита: %s\n"
        "Время: %u мин",
        state_type_name(s->type),
        state_stage_name(s->stage),
        s->cube_temp, s->column_temp,
        s->power_percent,
        fraction_ru(fraction_get()),
        s->body_rate_ml_min,
        fsel_is_valve_open() ? "открыт" : "закрыт",
        abv_get_remaining_ml(), abv_get_remaining_percent(),
        estop_is_pressed() ? "активен" : "нет",
        ctrl_safety_ok() ? "ok" : "сработала",
        (unsigned)(s->elapsed_sec / 60));

    tg_send(chat, buf, KEYBOARD_STATUS);
}

static void send_log(const char *chat) {
    size_t count = 0;
    event_t *events = events_get(&count);

    char buf[960];
    int off = snprintf(buf, sizeof(buf), "Журнал (%u):\n", (unsigned)count);

    int start = (count > 8) ? (int)count - 8 : 0;
    for (size_t i = (size_t)start; i < count && off < (int)sizeof(buf) - 1; i++) {
        off += snprintf(buf + off, sizeof(buf) - (size_t)off, "%s: %.80s\n",
                        events_type_name(events[i].type), events[i].msg);
    }
    if (start == 0 && count == 0) {
        snprintf(buf, sizeof(buf), "Журнал пуст.");
    }

    tg_send(chat, buf, KEYBOARD_MAIN);
}

static void send_fraction(const char *chat) {
    char buf[320];
    snprintf(buf, sizeof(buf),
        "Фракция: %s\n\n"
        "Пороги:\n"
        "головы → тело: %.1f °C или %.0f мл\n"
        "тело → хвосты: %.1f °C или %.0f мл\n"
        "хвосты → готово: %.1f °C",
        fraction_ru(fraction_get()),
        (double)FRAC_HEADS_END_T, (double)FRAC_HEADS_MAX_ML,
        (double)FRAC_BODY_END_T, (double)FRAC_BODY_MAX_ML,
        (double)FRAC_TAILS_END_T);
    tg_send(chat, buf, NULL);
}

static void send_profiles(const char *chat, const char *args) {
    if (args && strncmp(args, "load", 4) == 0) {
        tg_send(chat,
            "Загрузка профиля из Telegram не поддерживается: профиль меняет "
            "уставки и режим, это делает веб-интерфейс с подтверждением.",
            NULL);
        return;
    }

    char names[PROFILES_MAX_COUNT][PROFILES_MAX_NAME];
    int count = profiles_list(names, PROFILES_MAX_COUNT);

    char buf[640];
    int off = snprintf(buf, sizeof(buf), "Профили (%d):\n", count);
    for (int i = 0; i < count && off < (int)sizeof(buf) - 1; i++) {
        off += snprintf(buf + off, sizeof(buf) - (size_t)off, "• %s\n", names[i]);
    }
    if (count == 0) {
        snprintf(buf, sizeof(buf), "Профилей нет. Создайте их в веб-интерфейсе.");
    }
    tg_send(chat, buf, NULL);
}

// ------------------------------------------------------------------
// Команды
// ------------------------------------------------------------------
static void handle_command(const char *chat, const char *text) {
    while (*text == ' ') text++;
    if (*text != '/') return;

    char command[24] = "";
    const char *p = text + 1;
    size_t ci = 0;
    while (*p && *p != ' ' && *p != '@' && ci + 1 < sizeof(command)) {
        command[ci++] = *p++;
    }
    command[ci] = '\0';

    while (*p && *p != ' ') p++;   // пропускаем @botname
    while (*p == ' ') p++;
    const char *args = p;

    if (strcmp(command, "start") == 0 || strcmp(command, "help") == 0) {
        send_help(chat);
        return;
    }
    if (strcmp(command, "status") == 0) {
        send_status(chat);
        return;
    }
    if (strcmp(command, "run") == 0) {
        esp_err_t err = process_control_start(NULL);
        tg_send(chat,
                err == ESP_OK ? "Процесс запущен." :
                "Запуск запрещён: E-STOP или защита.", NULL);
        return;
    }
    if (strcmp(command, "stop") == 0) {
        process_control_stop();
        tg_send(chat, "Процесс остановлен, нагрев снят.", NULL);
        return;
    }
    if (strcmp(command, "pause") == 0) {
        process_state_t *s = state_get();
        if (!s->is_running) {
            tg_send(chat, "Процесс не запущен.", NULL);
        } else {
            s->is_paused = true;
            tg_send(chat, "Пауза.", NULL);
        }
        return;
    }
    if (strcmp(command, "resume") == 0) {
        process_state_t *s = state_get();
        s->is_paused = false;
        tg_send(chat, s->is_running ? "Продолжаю." : "Процесс не запущен.", NULL);
        return;
    }
    if (strcmp(command, "power") == 0) {
        if (!args[0]) {
            tg_send(chat, "Формат: /power 50 или /power auto", NULL);
            return;
        }
        char buf[128];
        if (strcmp(args, "auto") == 0) {
            ctrl_heater_set_auto();
            snprintf(buf, sizeof(buf), "Мощность: авто, %.0f %%.",
                     (double)ctrl_heater_get_percent());
        } else {
            float value = (float)atof(args);
            if (value < 0) value = 0;
            if (value > 100) value = 100;
            esp_err_t err = ctrl_heater_manual_set(value);
            if (err == ESP_ERR_INVALID_STATE) {
                tg_send(chat, "Регулятор не готов или активна защита.", NULL);
                return;
            }
            if (err != ESP_OK) {
                tg_send(chat, "Не удалось задать мощность.", NULL);
                return;
            }
            snprintf(buf, sizeof(buf), "Ручная мощность: %.0f %%.",
                     (double)ctrl_heater_get_percent());
        }
        tg_send(chat, buf, NULL);
        return;
    }
    if (strcmp(command, "fraction") == 0) {
        send_fraction(chat);
        return;
    }
    if (strcmp(command, "setpoint") == 0) {
        if (!args[0]) {
            tg_send(chat, "Формат: /setpoint 83.0", NULL);
            return;
        }
        float value = (float)atof(args);
        if (!isfinite(value) || value < 0 || value > 150) {
            tg_send(chat, "Недопустимая уставка.", NULL);
            return;
        }
        state_get()->target_cube_t = value;
        char buf[96];
        snprintf(buf, sizeof(buf), "Порог куба: %.1f °C.", value);
        tg_send(chat, buf, NULL);
        return;
    }
    if (strcmp(command, "adaptive") == 0) {
        bool on = (strcmp(args, "on") == 0);
        esp_err_t err = control_config_set_adaptive_pid(on);
        tg_send(chat, err == ESP_OK
                ? (on ? "Адаптивный PID включён." : "Адаптивный PID выключен.")
                : "Не удалось сохранить.", NULL);
        return;
    }
    if (strcmp(command, "algo") == 0) {
        if (!args[0]) {
            char buf[96];
            snprintf(buf, sizeof(buf), "Алгоритм: %s",
                     algo_name(fsel_get_algo()));
            tg_send(chat, buf, NULL);
            return;
        }
        select_algo_t algo = algo_parse(args);
        esp_err_t err = fsel_set_algo(algo);
        tg_send(chat, err == ESP_OK ? "Алгоритм изменён." :
                "Не удалось изменить алгоритм.", NULL);
        return;
    }
    if (strcmp(command, "regulator") == 0) {
        if (!args[0]) {
            char buf[96];
            snprintf(buf, sizeof(buf), "Регулятор: %s",
                     control_config_regulator_name(
                         control_config_get_regulator()));
            tg_send(chat, buf, NULL);
            return;
        }
        regulator_type_t type = REGULATOR_NONE;
        if (strcmp(args, "kvic") == 0) type = REGULATOR_KVIC;
        else if (strcmp(args, "rmvk") == 0) type = REGULATOR_RMVK;
        else if (strcmp(args, "sem") == 0) type = REGULATOR_SEM_AVR;
        else if (strcmp(args, "ssr") == 0) type = REGULATOR_SSR_PWM;
        else if (strcmp(args, "finglai") == 0 ||
                 strcmp(args, "ssvr") == 0) type = REGULATOR_FINGLAI_SSVR;
        esp_err_t err = regulator_select(type);
        char buf[96];
        snprintf(buf, sizeof(buf), "Регулятор: %s",
                 control_config_regulator_name(type));
        tg_send(chat, err == ESP_OK ? buf : "Не удалось сохранить.", NULL);
        return;
    }
    if (strcmp(command, "log") == 0) {
        send_log(chat);
        return;
    }
    if (strcmp(command, "param") == 0) {
        char name[40] = "";
        float value = 0.0f;
        if (sscanf(args, "%39s %f", name, &value) != 2) {
            tg_send(chat,
                    "Формат: /param open_time 1.5\n"
                    "Имена: t_fix, open_time, close_time, hysteresis,\n"
                    "decrement, min_open, heads_timer, tails_end,\n"
                    "reflux, power, setpoint.",
                    NULL);
            return;
        }

        const param_def_t *def = param_get_by_name(name);
        char buf[160];
        if (!def) {
            snprintf(buf, sizeof(buf), "Неизвестный параметр: %s", name);
        } else {
            param_id_t id = (param_id_t)(def - param_get_at(0));
            if (param_write(id, value)) {
                float actual = param_read(id);
                if (fabsf(actual - value) > 0.01f) {
                    snprintf(buf, sizeof(buf),
                             "%s = %.2f %s — скорректировано до диапазона.",
                             def->label, (double)actual, def->unit);
                } else {
                    snprintf(buf, sizeof(buf), "%s = %.2f %s — сохранено.",
                             def->label, (double)actual, def->unit);
                }
            } else {
                snprintf(buf, sizeof(buf),
                         "Параметр «%s» заблокирован во время работы.",
                         def->label);
            }
        }
        tg_send(chat, buf, NULL);
        return;
    }
    if (strcmp(command, "profile") == 0) {
        send_profiles(chat, args);
        return;
    }

    tg_send(chat, "Неизвестная команда. /help — список.", NULL);
}

// ------------------------------------------------------------------
// Inline-кнопки
// ------------------------------------------------------------------
static void handle_callback(cJSON *callback) {
    cJSON *message = cJSON_GetObjectItemCaseSensitive(callback, "message");
    cJSON *chat_obj = message ? cJSON_GetObjectItemCaseSensitive(message, "chat") : NULL;
    char chat[32] = "";
    if (!chat_id_of(chat_obj, chat, sizeof(chat))) return;

    if (!authorized(chat)) {
        ESP_LOGW(TAG, "callback from unauthorized chat %s", chat);
        return;
    }

    cJSON *id_obj = cJSON_GetObjectItemCaseSensitive(callback, "id");
    const char *callback_id = cJSON_IsString(id_obj) ? id_obj->valuestring : NULL;
    cJSON *data_obj = cJSON_GetObjectItemCaseSensitive(callback, "data");
    const char *data = cJSON_IsString(data_obj) ? data_obj->valuestring : "";

    tg_answer(callback_id, "");

    if (strcmp(data, "menu_main") == 0) {
        send_help(chat);
    } else if (strcmp(data, "menu_work") == 0) {
        send_work_menu(chat);
    } else if (strcmp(data, "fraction") == 0) {
        send_fraction(chat);
    } else if (strcmp(data, "edit_open") == 0 ||
               strcmp(data, "edit_close") == 0 ||
               strcmp(data, "edit_t_fix") == 0 ||
               strcmp(data, "edit_hyst") == 0) {
        const char *name = strcmp(data, "edit_close") == 0 ? "close_time" :
                           strcmp(data, "edit_t_fix") == 0 ? "t_fix" :
                           strcmp(data, "edit_hyst") == 0 ? "hysteresis" :
                           "open_time";
        char buf[160];
        snprintf(buf, sizeof(buf),
                 "Введите новое значение:\n/param %s <значение>", name);
        tg_send(chat, buf, NULL);
    } else if (strcmp(data, "menu_prepare") == 0) {
        send_prepare_menu(chat);
    } else if (strcmp(data, "menu_diagnose") == 0) {
        send_diagnose_menu(chat);
    } else if (strcmp(data, "menu_system") == 0) {
        send_system_menu(chat);
    } else if (strcmp(data, "profiles") == 0) {
        send_profiles(chat, "");
    } else if (strcmp(data, "calibrate") == 0) {
        tg_send(chat,
            "Калибровка насоса:\n"
            "1. Трубка в мерный цилиндр.\n"
            "2. Доза 10 мл.\n"
            "3. steps = old × факт / 10.\n"
            "4. Сохраните steps_per_ml в PWA.",
            KEYBOARD_PREPARE);
    } else if (strcmp(data, "autotune") == 0) {
        send_autotune_status(chat);
    } else if (strcmp(data, "sensors") == 0) {
        send_sensors(chat);
    } else if (strcmp(data, "actuators") == 0) {
        tg_send(chat,
            "Проверка приводов выполняется в PWA: Диагностика → Проверка приводов. "
            "Требуется сервисный режим и подтверждение.",
            KEYBOARD_DIAGNOSE);
    } else if (strcmp(data, "health") == 0) {
        send_health(chat);
    } else if (strcmp(data, "network") == 0) {
        send_network(chat);
    } else if (strcmp(data, "ota") == 0) {
        tg_send(chat,
            "OTA: PWA → Система → Обновление. Нужен HTTPS-URL на .bin.",
            KEYBOARD_SYSTEM);
    } else if (strcmp(data, "api") == 0) {
        tg_send(chat,
            "API-ключ: PWA → Система → API-ключ. Бот использует chat_id, "
            "но не хранит REST-ключ.",
            KEYBOARD_SYSTEM);
    } else if (strcmp(data, "lang") == 0) {
        tg_send(chat, "Язык: PWA → Система → Язык.", KEYBOARD_SYSTEM);
    } else if (strcmp(data, "about") == 0) {
        send_about(chat);
    } else if (strcmp(data, "frac_0") == 0 || strcmp(data, "frac_1") == 0 ||
               strcmp(data, "frac_2") == 0) {
        tg_send(chat,
            "Ручной выбор фракции не поддерживается: переключение делает "
            "автоматика по температуре и объёму.",
            KEYBOARD_WORK);
    } else if (strcmp(data, "run") == 0 || strcmp(data, "start") == 0) {
        esp_err_t err = process_control_start(NULL);
        tg_send(chat, err == ESP_OK ? "Процесс запущен." :
                "Запуск запрещён: E-STOP или защита.", KEYBOARD_MAIN);
    } else if (strcmp(data, "stop") == 0) {
        process_control_stop();
        tg_send(chat, "Процесс остановлен.", KEYBOARD_MAIN);
    } else if (strcmp(data, "restart") == 0) {
        process_control_stop();
        esp_err_t err = process_control_start(NULL);
        tg_send(chat, err == ESP_OK ? "Процесс перезапущен." :
                "Перезапуск запрещён: E-STOP или защита.", KEYBOARD_MAIN);
    } else if (strcmp(data, "status") == 0) {
        send_status(chat);
    } else if (strcmp(data, "log") == 0) {
        send_log(chat);
    } else if (strcmp(data, "estop_ack") == 0) {
        if (estop_button_down()) {
            tg_send(chat, "E-STOP физически нажат. Отпустите кнопку.", NULL);
        } else if (estop_acknowledge()) {
            ctrl_safety_reset();
            tg_send(chat, "E-STOP сброшен. Убедитесь, что причина устранена.", NULL);
            events_log(EVENT_USER_ACTION, "Telegram: estop acknowledged");
        } else {
            tg_send(chat, "Сброс невозможен: E-STOP удерживается программно.", NULL);
        }
    }
}

// ------------------------------------------------------------------
// Разбор update
// ------------------------------------------------------------------
static void handle_update(cJSON *update) {
    cJSON *callback = cJSON_GetObjectItemCaseSensitive(update, "callback_query");
    if (callback) {
        handle_callback(callback);
        return;
    }

    cJSON *message = cJSON_GetObjectItemCaseSensitive(update, "message");
    if (!message) return;

    cJSON *chat_obj = cJSON_GetObjectItemCaseSensitive(message, "chat");
    char chat[32] = "";
    if (!chat_id_of(chat_obj, chat, sizeof(chat))) return;

    if (!authorized(chat)) {
        ESP_LOGW(TAG, "message from unauthorized chat %s", chat);
        return;
    }

    cJSON *text_obj = cJSON_GetObjectItemCaseSensitive(message, "text");
    if (cJSON_IsString(text_obj) && text_obj->valuestring) {
        handle_command(chat, text_obj->valuestring);
    }
}

// ------------------------------------------------------------------
// Задача polling
// ------------------------------------------------------------------
void telegram_bot_task(void *arg) {
    (void)arg;
    wd_register("tg_bot", 90000, false);

    long long offset = 0;
    int failures = 0;
    bool webhook_cleared = false;

    while (1) {
        WD_FEED("tg_bot");

        char token[128] = "";
        char chat[32] = "";
        telegram_get_credentials(token, sizeof(token), chat, sizeof(chat));

        if (!flags_get_by_key("telegram") || !token[0] || !chat[0]) {
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        // getUpdates не работает, пока у бота установлен webhook. Один раз
        // снимаем его: этот режим — polling, а не webhook.
        if (!webhook_cleared) {
            cJSON *empty = cJSON_CreateObject();
            if (empty) {
                int status = tg_post("deleteWebhook", empty);
                cJSON_Delete(empty);
                if (status == 200) webhook_cleared = true;
            }
        }

        char *response = tg_get_updates(token, offset);
        if (!response) {
            failures++;
            if (failures == 1 || failures % 10 == 0)
                events_log(EVENT_WARNING, "Telegram getUpdates unavailable");
            vTaskDelay(pdMS_TO_TICKS(failures > 3 ? 10000 : 2000));
            continue;
        }
        failures = 0;

        cJSON *root = cJSON_Parse(response);
        free(response);
        if (root) {
            cJSON *ok = cJSON_GetObjectItemCaseSensitive(root, "ok");
            cJSON *result = cJSON_GetObjectItemCaseSensitive(root, "result");
            if (cJSON_IsTrue(ok) && cJSON_IsArray(result)) {
                cJSON *update;
                cJSON_ArrayForEach(update, result) {
                    cJSON *id = cJSON_GetObjectItemCaseSensitive(update, "update_id");
                    if (cJSON_IsNumber(id)) {
                        offset = (long long)id->valuedouble + 1;
                    }
                    handle_update(update);
                    WD_FEED("tg_bot");
                }
            }
            cJSON_Delete(root);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
