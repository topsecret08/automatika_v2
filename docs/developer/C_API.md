# C API

Сгенерировано `tools/gen_api_docs.py` из комментариев в заголовках.
Всего функций: 337.

Не редактируйте вручную: измените комментарий в `.h` и перегенерируйте.

## `main/control/adaptive_pid.h`

```c
bool adaptive_pid_run_finish(adaptive_pid_t *p, int64_t run_id);
```

Возвращает true, если метрики прогона в допуске; сохраняет JSON в verify_runs.

```c
adaptive_pid_t *adaptive_pid_global(void);
```

Глобальный экземпляр, который использует ctrl_pid.

## `main/control/alcohol_balance.h`

```c
void abv_init(const alcohol_init_t *init);
```

Инициализировать/сбросить баланс. Повторный вызов начинает расчёт заново.

```c
void abv_reset(void);
```

Инициализировать/сбросить баланс. Повторный вызов начинает расчёт заново.
Сброс к сохранённым начальным параметрам (новый процесс).

```c
bool abv_ready(void);
```

Инициализировать/сбросить баланс. Повторный вызов начинает расчёт заново.
Сброс к сохранённым начальным параметрам (новый процесс).

```c
float abv_update(float t_cube_c, float pressure_hpa);
```

Обновить по T куба (°C) и P_абс (hPa). Возвращает остаток этанола, мл.

```c
float abv_ethanol_vapor_pressure(float t_c);
```

Парциальное давление насыщенного пара этанола, Па (T в °C).

```c
float abv_water_vapor_pressure(float t_c);
```

Парциальное давление насыщенного пара этанола, Па (T в °C).
Парциальное давление насыщенного пара воды, Па.

## `main/control/ctrl_dose.h`

```c
bool ctrl_dose_volume(float volume_ml, float rate_ml_per_min);
```

Выдать `volume_ml` со скоростью `rate_ml_per_min` (асинхронно).

```c
bool ctrl_dose_set_rate(float rate_ml_per_min);
```

Выдать `volume_ml` со скоростью `rate_ml_per_min` (асинхронно).

```c
void ctrl_dose_stop(void);
```

Выдать `volume_ml` со скоростью `rate_ml_per_min` (асинхронно).

```c
esp_err_t ctrl_dose_volume_blocking(float volume_ml, float rate_ml_per_min);
```

Блокирующая выдача — только из задачи «doser»/диагностики, не из httpd.

```c
void ctrl_dose_process(float rate_ml_min, float dt, int valve_idx);
```

Вызывается из mode_task: непрерывный отбор и учёт объёма.
- в режиме насоса дозирует порции n = rate·dt;
- в режиме клапана задаёт долю открытия и добавляет rate·dt/60 к объёму.

## `main/control/ctrl_pid.h`

```c
float ctrl_pid_compute_and_apply(float input);
```

Вычислить PID и применить через safety_gate (adaptive_pid или heater_pid).

```c
float ctrl_pid_compute_and_apply_scaled(float input, float scale);
```

Вычислить PID и применить через safety_gate (adaptive_pid или heater_pid).

```c
void ctrl_pid_autotune_start(void);
```

Автотюнинг (Ziegler–Nichols релейный) — как раньше.

```c
void ctrl_pid_autotune_stop(void);
```

Автотюнинг (Ziegler–Nichols релейный) — как раньше.

```c
bool ctrl_pid_autotune_done(void);
```

Автотюнинг (Ziegler–Nichols релейный) — как раньше.

```c
void ctrl_pid_autotune_get(float *kp, float *ki, float *kd);
```

Автотюнинг (Ziegler–Nichols релейный) — как раньше.

```c
bool ctrl_pid_uses_autopid(void);
```

Диагностика.

```c
float ctrl_pid_get_integral(void);
```

Диагностика.

## `main/control/ctrl_safety.h`

```c
esp_err_t ctrl_safety_reset(void);
```

ESP_OK — авария сброшена; ESP_ERR_INVALID_STATE — сброс запрещён
(процесс/нагрев/уровень/E-STOP). Причина — ctrl_safety_reset_error().

```c
const char *ctrl_safety_reset_error(void);
```

ESP_OK — авария сброшена; ESP_ERR_INVALID_STATE — сброс запрещён
(процесс/нагрев/уровень/E-STOP). Причина — ctrl_safety_reset_error().

```c
bool ctrl_safety_estop_latched(void);
```

E-STOP защёлкнут по железу (нажата кнопка) — нагрев и отбор запрещены.

## `main/control/fraction_periodic.h`

```c
esp_err_t fsel_set_param(const char *name, float value);
```

Точечное изменение параметра из REST/Telegram/LCD. Обновляет и
сохранённый конфиг, и текущее состояние этапа; сохраняет NVS.
Имена: open_time_sec, close_time_sec, period_sec, t_fix,
body_hysteresis_c, body_decrement_percent, body_min_open_time_sec,
heads_timer_sec, tails_temperature_end.

```c
esp_err_t fsel_set_algo(select_algo_t algo);
```

Точечное изменение параметра из REST/Telegram/LCD. Обновляет и
сохранённый конфиг, и текущее состояние этапа; сохраняет NVS.
Имена: open_time_sec, close_time_sec, period_sec, t_fix,
body_hysteresis_c, body_decrement_percent, body_min_open_time_sec,
heads_timer_sec, tails_temperature_end.

```c
select_algo_t fsel_get_algo(void);
```

Точечное изменение параметра из REST/Telegram/LCD. Обновляет и
сохранённый конфиг, и текущее состояние этапа; сохраняет NVS.
Имена: open_time_sec, close_time_sec, period_sec, t_fix,
body_hysteresis_c, body_decrement_percent, body_min_open_time_sec,
heads_timer_sec, tails_temperature_end.

```c
void fsel_apply_decrement(float percent);
```

Точечное изменение параметра из REST/Telegram/LCD. Обновляет и
сохранённый конфиг, и текущее состояние этапа; сохраняет NVS.
Имена: open_time_sec, close_time_sec, period_sec, t_fix,
body_hysteresis_c, body_decrement_percent, body_min_open_time_sec,
heads_timer_sec, tails_temperature_end.

```c
void fsel_apply_mode_defaults(process_type_t type);
```

Подставить тайминги под режим (только при auto_mode_defaults).

```c
bool fsel_mode_tick(process_state_t *st, float power_heads, float power_body, float power_tails);
```

Общая ветка FSEL для дистилляционных режимов: охлаждение, нагрев по
этапу, reflux и stage в state. Возвращает true, если режим перехвачен и
штатная логика режима должна выйти.

## `main/control/pid_autotune.h`

```c
bool pid_autotune_load_gains(float *kp, float *ki, float *kd);
```

Прочитать сохранённые gains; возвращает false, если в NVS их нет.

## `main/control/pump_doser.h`

```c
esp_err_t pump_dose_volume(float volume_ml, float rate_ml_per_min);
```

ESP_OK               — доза поставлена в очередь;
ESP_ERR_INVALID_ARG  — некорректный объём;
ESP_ERR_INVALID_SIZE — объём больше max_steps;
ESP_ERR_INVALID_STATE— авария/E-STOP/очередь занята.

## `main/control/reflux.h`

```c
void reflux_update(void);
```

Один шаг ШИМ; вызывается задачей, но безопасен и из цикла управления.

## `main/control/safety_gate.h`

```c
esp_err_t safety_gate_apply(const char *module, float value);
```

Единая точка применения выходов нагрева.
CRITICAL применяется всегда (после проверки safety/E-STOP).
QUALITY/EXPERIMENTAL — только после verify_mark_module_verified().
EXPERIMENTAL дополнительно требует ненулевой shadow policy.

```c
esp_err_t safety_gate_submit(const char *module, float value);
```

Единая точка применения выходов нагрева.
CRITICAL применяется всегда (после проверки safety/E-STOP).
QUALITY/EXPERIMENTAL — только после verify_mark_module_verified().
EXPERIMENTAL дополнительно требует ненулевой shadow policy.
Каноническая точка отправки команд управления. Сейчас алиас apply,
сохраняется для нового кода и внешних интеграций.

```c
esp_err_t safety_gate_submit_flags(uint32_t aggregate, uint32_t detail);
```

Единая точка применения выходов нагрева.
CRITICAL применяется всегда (после проверки safety/E-STOP).
QUALITY/EXPERIMENTAL — только после verify_mark_module_verified().
EXPERIMENTAL дополнительно требует ненулевой shadow policy.
Каноническая точка отправки команд управления. Сейчас алиас apply,
сохраняется для нового кода и внешних интеграций.

```c
esp_err_t safety_gate_apply_valve(const char *module, int idx, float value);
```

Единая точка применения выходов нагрева.
CRITICAL применяется всегда (после проверки safety/E-STOP).
QUALITY/EXPERIMENTAL — только после verify_mark_module_verified().
EXPERIMENTAL дополнительно требует ненулевой shadow policy.
Каноническая точка отправки команд управления. Сейчас алиас apply,
сохраняется для нового кода и внешних интеграций.

```c
bool safety_gate_can_apply(const char *module);
```

Единая точка применения выходов нагрева.
CRITICAL применяется всегда (после проверки safety/E-STOP).
QUALITY/EXPERIMENTAL — только после verify_mark_module_verified().
EXPERIMENTAL дополнительно требует ненулевой shadow policy.
Каноническая точка отправки команд управления. Сейчас алиас apply,
сохраняется для нового кода и внешних интеграций.

```c
safety_class_t safety_gate_class(const char *module);
```

Единая точка применения выходов нагрева.
CRITICAL применяется всегда (после проверки safety/E-STOP).
QUALITY/EXPERIMENTAL — только после verify_mark_module_verified().
EXPERIMENTAL дополнительно требует ненулевой shadow policy.
Каноническая точка отправки команд управления. Сейчас алиас apply,
сохраняется для нового кода и внешних интеграций.

```c
const char *safety_gate_class_name(safety_class_t cls);
```

Единая точка применения выходов нагрева.
CRITICAL применяется всегда (после проверки safety/E-STOP).
QUALITY/EXPERIMENTAL — только после verify_mark_module_verified().
EXPERIMENTAL дополнительно требует ненулевой shadow policy.
Каноническая точка отправки команд управления. Сейчас алиас apply,
сохраняется для нового кода и внешних интеграций.

```c
int safety_gate_module_count(void);
```

Единая точка применения выходов нагрева.
CRITICAL применяется всегда (после проверки safety/E-STOP).
QUALITY/EXPERIMENTAL — только после verify_mark_module_verified().
EXPERIMENTAL дополнительно требует ненулевой shadow policy.
Каноническая точка отправки команд управления. Сейчас алиас apply,
сохраняется для нового кода и внешних интеграций.

```c
const char *safety_gate_module_name(int index);
```

Единая точка применения выходов нагрева.
CRITICAL применяется всегда (после проверки safety/E-STOP).
QUALITY/EXPERIMENTAL — только после verify_mark_module_verified().
EXPERIMENTAL дополнительно требует ненулевой shadow policy.
Каноническая точка отправки команд управления. Сейчас алиас apply,
сохраняется для нового кода и внешних интеграций.

## `main/control/select_algorithm.h`

```c
algo_result_t algo_evaluate(const algo_config_t *cfg, float trend, float t_stage, float t_fix, float hysteresis);
```

trend — °C/мин по конвейеру датчика пара; t_stage — текущая T пара;
t_fix — зафиксированная T тела; hysteresis — допуск.

```c
float algo_decrement_multiplier(const algo_config_t *cfg, algo_result_t result);
```

Множитель декремента: тренд режет мягче, адаптивный — агрессивнее.

```c
float algo_close_time(float period, float open_time);
```

Согласование периода и open_time: close = period - open.

## `main/control/volume_feedback.h`

```c
float volume_feedback_ml(const volume_feedback_t *v);
```

Возвращает измеренный объём с начала этапа или -1, если весы недоступны.

## `main/core/antoine.h`

```c
bool antoine_inverse(const antoine_set_t *sets, int n, float p_mmhg, float *t_out);
```

Обратный решатель Антуана: f(T)=A-B/(C+T)-log10(P)=0.
P в мм рт. ст., T в °C. Возвращает false, если корень вне диапазонов.

## `main/core/boil_table.h`

```c
bool boil_table_lookup(float p_hpa, float *t_out);
```

Линейная интерполяция по таблице IAPWS-IF97.
false — давление вне диапазона, вызывающий использует Антуан.

## `main/core/config_backup.h`

```c
const char *config_backup_info(void);
```

Краткое описание содержимого последнего файла (для ответа API).

## `main/core/control_config.h`

```c
regulator_type_t control_config_get_regulator(void);
```

--- Внешний регулятор мощности -------------------------------------

```c
esp_err_t control_config_set_regulator(regulator_type_t type);
```

--- Внешний регулятор мощности -------------------------------------

```c
uint32_t control_config_get_regulator_baud(void);
```

--- Внешний регулятор мощности -------------------------------------

```c
esp_err_t control_config_set_regulator_baud(uint32_t baud);
```

--- Внешний регулятор мощности -------------------------------------

```c
esp_err_t control_config_get_regulator_settings(regulator_settings_t *out);
```

--- Внешний регулятор мощности -------------------------------------

```c
esp_err_t control_config_set_regulator_settings(const regulator_settings_t *in);
```

--- Внешний регулятор мощности -------------------------------------

```c
const char *control_config_regulator_name(regulator_type_t type);
```

--- Внешний регулятор мощности -------------------------------------

```c
bool control_config_api_has_key(void);
```

--- API key ---------------------------------------------------------

```c
bool control_config_api_check(const char *key);
```

--- API key ---------------------------------------------------------
Возвращает true, если ключ совпадает (сравнение за постоянное время).

```c
esp_err_t control_config_api_generate(char *out, size_t out_len);
```

--- API key ---------------------------------------------------------
Возвращает true, если ключ совпадает (сравнение за постоянное время).
Новый ключ (32 hex из esp_random) и сохранение в NVS.

```c
esp_err_t control_config_api_clear(void);
```

--- API key ---------------------------------------------------------
Возвращает true, если ключ совпадает (сравнение за постоянное время).
Новый ключ (32 hex из esp_random) и сохранение в NVS.
Пустой ключ: API закрыт полностью (любой запрос → 401).

```c
void control_config_api_copy(char *out, size_t out_len);
```

--- API key ---------------------------------------------------------
Возвращает true, если ключ совпадает (сравнение за постоянное время).
Новый ключ (32 hex из esp_random) и сохранение в NVS.
Пустой ключ: API закрыт полностью (любой запрос → 401).

```c
bool control_config_service_enabled(void);
```

--- сервисный режим -------------------------------------------------

```c
esp_err_t control_config_set_service(bool on);
```

--- сервисный режим -------------------------------------------------

```c
bool control_config_get_adaptive_pid(void);
```

--- Многоуровневый адаптивный PID ----------------------------------

```c
esp_err_t control_config_set_adaptive_pid(bool on);
```

--- Многоуровневый адаптивный PID ----------------------------------

## `main/core/estop.h`

```c
bool estop_is_pressed(void);
```

true — авария активна (кнопка нажата или защёлка не снята).

```c
bool estop_is_healthy(void);
```

true — авария активна (кнопка нажата или защёлка не снята).

```c
bool estop_button_down(void);
```

true — авария активна (кнопка нажата или защёлка не снята).
Состояние линии сейчас: true — кнопка нажата (разомкнут NC-контакт).

```c
int estop_raw_level(void);
```

true — авария активна (кнопка нажата или защёлка не снята).
Состояние линии сейчас: true — кнопка нажата (разомкнут NC-контакт).

```c
bool estop_acknowledge(void);
```

true — авария активна (кнопка нажата или защёлка не снята).
Состояние линии сейчас: true — кнопка нажата (разомкнут NC-контакт).
Сброс защёлки только если кнопка отпущена и авария снята.

```c
void estop_force(const char *reason);
```

true — авария активна (кнопка нажата или защёлка не снята).
Состояние линии сейчас: true — кнопка нажата (разомкнут NC-контакт).
Сброс защёлки только если кнопка отпущена и авария снята.
Программная блокировка (Lua/диагностика): защёлка ставится до reset.

```c
const char *estop_source_name(void);
```

true — авария активна (кнопка нажата или защёлка не снята).
Состояние линии сейчас: true — кнопка нажата (разомкнут NC-контакт).
Сброс защёлки только если кнопка отпущена и авария снята.
Программная блокировка (Lua/диагностика): защёлка ставится до reset.

```c
uint32_t estop_since_ms(void);
```

true — авария активна (кнопка нажата или защёлка не снята).
Состояние линии сейчас: true — кнопка нажата (разомкнут NC-контакт).
Сброс защёлки только если кнопка отпущена и авария снята.
Программная блокировка (Lua/диагностика): защёлка ставится до reset.

## `main/core/event_log.h`

```c
const char *event_log_dump_json(int limit);
```

Возвращает указатель на статический буфер (перезаписывается при
следующем вызове). limit ≤ 0 — все доступные записи.

```c
void event_log_add(int type, const char *msg);
```

Запись в журнал с уникальным типом (обёртка над events_log()).

## `main/core/events.h`

```c
int events_read_after(size_t *cursor, event_t *out, int max);
```

Читает события, появившиеся после позиции *cursor, и сдвигает курсор.

```c
void events_clear(void);
```

Читает события, появившиеся после позиции *cursor, и сдвигает курсор.

```c
void events_export_csv(const char *path);
```

Читает события, появившиеся после позиции *cursor, и сдвигает курсор.

```c
void events_task(void *arg);
```

Читает события, появившиеся после позиции *cursor, и сдвигает курсор.

```c
const char* events_type_name(event_type_t t);
```

Читает события, появившиеся после позиции *cursor, и сдвигает курсор.

## `main/core/help_db.h`

```c
const help_article_t* help_find(const char *id);
```

Поиск во встроенной базе. NULL, если статьи нет.

```c
bool help_show_article(const char *id);
```

Показ статьи: встроенная → SD → сообщение об отсутствии.

## `main/core/learning.h`

```c
bool learning_answer(const lesson_t *lesson, int answer);
```

Проверяет ответ и при правильном ответе сохраняет прогресс в NVS.
Возвращает true, если ответ верный.

```c
int learning_passed_count(void);
```

Проверяет ответ и при правильном ответе сохраняет прогресс в NVS.
Возвращает true, если ответ верный.

```c
bool learning_module_complete(int module_idx);
```

Проверяет ответ и при правильном ответе сохраняет прогресс в NVS.
Возвращает true, если ответ верный.

## `main/core/loop_verify.h`

```c
bool loop_verify_last_result(lv_result_t *out);
```

Последний результат (для API/телеметрии). Возвращает false, если
проверок ещё не было.

```c
void lv_tick_at(int64_t now_us, int64_t sensor_us, const lv_sensor_frame_t *s, size_t n_s, const lv_program_frame_t *p, const lv_actuator_frame_t *a, size_t n_a, lv_result_t *out);
```

Чистый расчёт (для тестов): now_us задаётся вызывающим.

```c
bool lv_escalation_changed(uint32_t aggregate, uint32_t detail, uint32_t *last_aggregate, uint32_t *last_detail);
```

Антидребезг эскалации: true только при смене агрегата или маски.
Обновляет *last_aggregate/*last_detail.

## `main/core/process_control.h`

```c
esp_err_t process_control_start(const char *mode_id);
```

Единая точка запуска/остановки процесса для UI и Telegram.

Веб-обработчик /api/start исторически содержит ту же логику; модуль
повторяет её для локального экрана и бота, чтобы не дублировать проверки
безопасности в двух местах.

start(mode_id):
mode_id == NULL  — текущий режим, а если он не выбран, distill;
mode_id != NULL  — "distill" | "rectify" | "beer" | "pvk" | "nbk" |
"vacuum" | "double" | "gabriel" | "ferment".

Возвращает ESP_ERR_INVALID_ARG для неизвестного режима,
ESP_ERR_INVALID_STATE при активном E-STOP или блокировке safety.

```c
void process_control_stop(void);
```

Снимает нагрев, останавливает дозатор и текущий режим.

```c
bool process_control_running(void);
```

true, если процесс запущен и не на паузе.

## `main/core/profiles.h`

```c
int profiles_list(char names[][PROFILES_MAX_NAME], int max);
```

Список имён без расширения. Возвращает количество (0..max).

```c
esp_err_t profiles_load(const char *name, char *buf, size_t max);
```

Читает файл в buf (NUL-терминирован). Возвращает ESP_ERR_NOT_FOUND и т.п.

```c
esp_err_t profiles_delete(const char *name);
```

Читает файл в buf (NUL-терминирован). Возвращает ESP_ERR_NOT_FOUND и т.п.

## `main/core/recipes_db.h`

```c
void recipes_show(const recipe_t *r);
```

Показ карточки рецепта (UI).

## `main/core/sd_content.h`

```c
const char* sd_content_path(content_type_t type);
```

Каталог раздела, например "/sdcard/recipes". Никогда не NULL.

```c
int sd_content_list(content_type_t type, char names[][SD_CONTENT_MAX_NAME], int max);
```

Список файлов раздела (без подкаталогов, скрытых и служебных).
Возвращает количество имён, записанных в names.

```c
bool sd_content_read(content_type_t type, const char *name, char *buf, size_t size);
```

Чтение файла раздела целиком в buf (с завершающим нулём).

## `main/core/sd_resilience.h`

```c
void sd_resilience_log(const char *line);
```

Записать строку в журнал устойчивости (с '\n' или без).

```c
int sd_resilience_pending(void);
```

Сколько записей ждёт сброса (диагностика /api/health).

## `main/core/session_recovery.h`

```c
esp_err_t session_save_now(void);
```

Сохранить текущее состояние (вызывается задачей и по /api/stop).

```c
bool session_has_valid(void);
```

Есть ли валидный снимок (для UI «Продолжить?»).

## `main/core/skill_auth.h`

```c
bool skill_token_check_header(httpd_req_t *req);
```

Проверка общего токена навыка (X-Skill-Token), заданного в NVS
(namespace automatika, ключ skill_token). Сравнение — постоянное по
времени. Пустой токен — эндпоинт закрыт.

```c
bool skill_token_is_set(void);
```

Проверка общего токена навыка (X-Skill-Token), заданного в NVS
(namespace automatika, ключ skill_token). Сравнение — постоянное по
времени. Пустой токен — эндпоинт закрыт.

## `main/core/state.h`

```c
void state_snapshot(process_state_t *out);
```

Атомарный снимок состояния: копия под mutex, пригодна для JSON/NVS.

```c
bool state_load(void);
```

Атомарный снимок состояния: копия под mutex, пригодна для JSON/NVS.

```c
void state_save(void);
```

Атомарный снимок состояния: копия под mutex, пригодна для JSON/NVS.

```c
void state_clear(void);
```

Атомарный снимок состояния: копия под mutex, пригодна для JSON/NVS.

```c
bool state_has_saved(void);
```

Атомарный снимок состояния: копия под mutex, пригодна для JSON/NVS.

```c
void state_update_snapshot(float dt);
```

Атомарный снимок состояния: копия под mutex, пригодна для JSON/NVS.

```c
const char* state_type_name(process_type_t t);
```

Атомарный снимок состояния: копия под mutex, пригодна для JSON/NVS.

```c
const char* state_stage_name(proc_stage_t s);
```

Атомарный снимок состояния: копия под mutex, пригодна для JSON/NVS.

## `main/core/storage.h`

```c
esp_err_t storage_remount(void);
```

Перемонтировать SD после сбоя (используется sd_resilience).

```c
bool storage_write_file(const char *path, const void *data, size_t len);
```

Перемонтировать SD после сбоя (используется sd_resilience).

```c
size_t storage_read_file(const char *path, void *buf, size_t max);
```

Перемонтировать SD после сбоя (используется sd_resilience).

```c
sdmmc_card_t* storage_get_sd_card(void);
```

Перемонтировать SD после сбоя (используется sd_resilience).

## `main/core/vle_ew.h`

```c
bool vle_bubble_point(const nrtl_params_t *p, float p_mmhg, float x1, float *T_c_out, float *y1_out);
```

Bubble point при заданном давлении (мм рт. ст.) и составе жидкости x1.
Возвращает false, если решение вне физического диапазона 40..100 °C.

## `main/core/watchdog.h`

```c
uint32_t wd_max_stale_ms(void);
```

Наибольшее «зависание» любой зарегистрированной задачи, мс
(используется health-check и автоподтверждением OTA).

## `main/hal/baro.h`

```c
float baro_boiling_point_c(float p_hpa);
```

Точка кипения воды при данном давлении, °C (100 °C при 1013.25 гПа).

```c
float baro_correct_boiling_point(float t_measured, float p_hpa);
```

Поправка измеренной температуры к нормальному давлению:
t_corr = t_measured + (1013.25 - p_hpa) * BOIL_CORRECTION_COEFF
Нужна, чтобы пороги фракций (заданные для 1013.25 гПа) не «уезжали»
в горах или при заметном изменении погоды.

## `main/hal/hal_1wire.h`

```c
esp_err_t hal_1wire_init(void);
```

Инициализация шины + поиск датчиков

```c
int hal_1wire_count(void);
```

Инициализация шины + поиск датчиков

```c
const char* hal_1wire_addr_str(int idx);
```

Инициализация шины + поиск датчиков

```c
const uint8_t* hal_1wire_rom(int idx);
```

Инициализация шины + поиск датчиков

```c
float hal_1wire_read_raw(int idx); // 0.0625°C шаг (блокирующее чтение) float hal_1wire_read_filtered(int idx); // фильтр TempFilter (окно FILTER_WINDOW) float hal_1wire_read_fast(int idx); // последнее сырое значение + калибровка int64_t hal_1wire_last_update_us(int idx); // время последнего сэмпла, us float hal_1wire_read(int idx); // медиана + Калман + калибровка float hal_1wire_read_compensated(int idx, float pressure_hpa);
```

Чтение. Фоновая задача опрашивает шину ~1 раз в секунду;
все функции ниже возвращают значение из кэша без блокировки.

```c
float hal_1wire_trend(int idx); // °C/мин по конвейеру int hal_1wire_read_all(float *out, int max);
```

Чтение. Фоновая задача опрашивает шину ~1 раз в секунду;
все функции ниже возвращают значение из кэша без блокировки.

```c
bool hal_1wire_rising(int idx);
```

Упреждающий анализ тренда (temp_filter_is_rising по каналу).

```c
float hal_1wire_slope(int idx); // °C/мин, линейный тренд const ds18b20_calib_t* hal_1wire_get_calib(int idx);
```

Упреждающий анализ тренда (temp_filter_is_rising по каналу).

```c
void hal_1wire_set_calib(int idx, float offset, float scale);
```

Упреждающий анализ тренда (temp_filter_is_rising по каналу).

```c
void hal_1wire_save_calib(void);
```

Упреждающий анализ тренда (temp_filter_is_rising по каналу).

```c
void hal_1wire_reset_calib(int idx);
```

Упреждающий анализ тренда (temp_filter_is_rising по каналу).

## `main/hal/hal_actuator_readback.h`

```c
bool hal_actuator_is_binary(uint8_t id);
```

true — канал бинарный (GPIO-сухой контакт/оптопара): 0 или 100 %.

## `main/hal/hal_safety_relay.h`

```c
esp_err_t hal_safety_relay_rearm(void);
```

Снять защёлку force_open. Вызывается только из явного сброса аварии
оператором; до первого heartbeat реле остаётся разомкнутым.

```c
void hal_safety_relay_force_open(void);
```

Снять защёлку force_open. Вызывается только из явного сброса аварии
оператором; до первого heartbeat реле остаётся разомкнутым.

## `main/hal/pressure_sensor.h`

```c
const char *pressure_sensor_source(void);
```

Диагностика для API: какой источник используется.

## `main/hal/sensor_pipeline.h`

```c
float temp_pipeline_update(TempPipeline *p, float raw, float pressure_hpa);
```

Возвращает скомпенсированную температуру или NAN, если значение невалидно.

```c
float temp_pipeline_compensate(const TempPipeline *p, float pressure_hpa);
```

Скомпенсировать последнюю оценку без нового отсчёта.

## `main/hal/temp_filter.h`

```c
float temp_filter_update(TempFilter *f, float new_sample);
```

Добавляет сэмпл. Значения <= -50 °C или > 200 °C считаются отказом
датчика: не входят в среднее (после 3 подряд — сброс окна).
Возвращает сглаженное значение; температура отказа — тоже результат.

```c
bool temp_filter_is_rising(TempFilter *f, float delta_threshold);
```

Упреждающий анализ: true — температура растёт быстрее порога.

```c
float temp_filter_feed(int ch, float raw);
```

Кормит канал сырым значением (вызывается фоновой задачей 1-Wire).

```c
float temp_filter_read(int ch); // сглаженное bool temp_filter_ch_rising(int ch); // флаг роста float temp_filter_ch_slope(int ch); // °C/мин bool temp_filter_ch_valid(int ch);
```

Кормит канал сырым значением (вызывается фоновой задачей 1-Wire).

## `main/level/level_sensor.h`

```c
bool level_is_required_fault(void);
```

Датчик включён и объявлен обязательным, но не инициализировался.

```c
const char *level_state_name(void);
```

Датчик включён и объявлен обязательным, но не инициализировался.

```c
bool level_is_fault(void);
```

Напоминание «датчик отсутствует/не отвечает» (для телеметрии/API).

## `main/modes/beer_recipe.h`

```c
void beer_recipe_start(int idx);
```

Runtime

```c
void beer_recipe_next_step(void);
```

Runtime

```c
void beer_recipe_prev_step(void);
```

Runtime

```c
int beer_recipe_current_step(void);
```

Runtime

## `main/net/mdns_service.h`

```c
esp_err_t mdns_service_start(void);
```

mDNS: automatika.local, услуги _http._tcp (80), _espressif._tcp.
Запускается задачей wifi_prov после получения IP.

```c
bool mdns_service_running(void);
```

mDNS: automatika.local, услуги _http._tcp (80), _espressif._tcp.
Запускается задачей wifi_prov после получения IP.

## `main/net/time_sync.h`

```c
bool rtc_ds3231_present(void);
```

DS3231

```c
esp_err_t rtc_read(uint32_t *unix_s);
```

DS3231

```c
esp_err_t rtc_write(uint32_t unix_s);
```

DS3231

## `main/net/wifi_prov.h`

```c
esp_err_t wifi_prov_start(void);
```

Принудительный старт provisioning (кнопка/API), даже если credentials есть.

```c
esp_err_t wifi_prov_reset(void);
```

Принудительный старт provisioning (кнопка/API), даже если credentials есть.
Забыть сеть и уйти в provisioning.

```c
const char *wifi_prov_status_json(void);
```

Принудительный старт provisioning (кнопка/API), даже если credentials есть.
Забыть сеть и уйти в provisioning.

## `main/ota/ota_guard.h`

```c
esp_err_t ota_guard_check(void);
```

Единая проверка перед любым запуском OTA.
Возвращает ESP_OK, если обновление разрешено, иначе ESP_ERR_INVALID_STATE.
Причина доступна через ota_guard_reason().

```c
const char *ota_guard_reason(void);
```

Единая проверка перед любым запуском OTA.
Возвращает ESP_OK, если обновление разрешено, иначе ESP_ERR_INVALID_STATE.
Причина доступна через ota_guard_reason().

```c
void ota_guard_set_web_running(bool on);
```

Единая проверка перед любым запуском OTA.
Возвращает ESP_OK, если обновление разрешено, иначе ESP_ERR_INVALID_STATE.
Причина доступна через ota_guard_reason().
Прямая загрузка через /api/ota занимает отдельный слот, пока идёт
HTTP-приём образа. Вызывать после успешного ota_guard_check().

```c
bool ota_guard_try_reserve(void);
```

Единая проверка перед любым запуском OTA.
Возвращает ESP_OK, если обновление разрешено, иначе ESP_ERR_INVALID_STATE.
Причина доступна через ota_guard_reason().
Прямая загрузка через /api/ota занимает отдельный слот, пока идёт
HTTP-приём образа. Вызывать после успешного ota_guard_check().
Атомарная проверка + резервирование OTA. После успеха вызывающий обязан
установить свой флаг и вызвать ota_guard_release().

```c
void ota_guard_release(void);
```

Единая проверка перед любым запуском OTA.
Возвращает ESP_OK, если обновление разрешено, иначе ESP_ERR_INVALID_STATE.
Причина доступна через ota_guard_reason().
Прямая загрузка через /api/ota занимает отдельный слот, пока идёт
HTTP-приём образа. Вызывать после успешного ota_guard_check().
Атомарная проверка + резервирование OTA. После успеха вызывающий обязан
установить свой флаг и вызвать ota_guard_release().

## `main/ota/ota_manager.h`

```c
esp_err_t ota_manager_confirm_running_app(void);
```

Явное подтверждение (например, из меню «Прошивка → Подтвердить»).

```c
esp_err_t ota_manager_start_update(const char *firmware_url);
```

Обновление по URL (HTTP/HTTPS), асинхронно через задачу «ota».
Возвращает ESP_OK, если задача запущена.

## `main/script/lua_engine.h`

```c
esp_err_t lua_engine_init(void);
```

Запуск диспетчера (создаёт очередь и задачу на ядре 0).

```c
esp_err_t lua_engine_set_script(const char *path);
```

Запуск диспетчера (создаёт очередь и задачу на ядре 0).
Кодирует путь (см. lua_engine_run_script). Пустой путь допустим:
значит, скрипт будет задан через lua_engine_set_script().

```c
void lua_engine_set_enabled(bool on);
```

Запуск диспетчера (создаёт очередь и задачу на ядре 0).
Кодирует путь (см. lua_engine_run_script). Пустой путь допустим:
значит, скрипт будет задан через lua_engine_set_script().

```c
bool lua_engine_enabled(void);
```

Запуск диспетчера (создаёт очередь и задачу на ядре 0).
Кодирует путь (см. lua_engine_run_script). Пустой путь допустим:
значит, скрипт будет задан через lua_engine_set_script().

```c
lua_run_result_t lua_engine_run_script(const char *path);
```

Постановка в очередь. path = NULL — повторить текущий скрипт.
Возвращает результат постановки (сам запуск — асинхронный).

```c
lua_run_result_t lua_engine_request(const char *script_path);
```

Выполнить строку (ограничение 4 КБ) — используется диспетчером и
REST-эндпоинтом /api/script/run.

```c
const char *lua_engine_status(void);
```

Состояние (для REST/логгеров).

```c
const char *lua_engine_last_error(void);
```

Состояние (для REST/логгеров).

```c
bool lua_engine_ready(void);
```

Состояние (для REST/логгеров).

```c
const char *lua_run_result_name(lua_run_result_t r);
```

Состояние (для REST/логгеров).

## `main/ui/haptic.h`

```c
void haptic_init(void);
```

Вибромотор на LCD. По умолчанию выключен: HAPTIC_GPIO = -1.

Не используйте GPIO 33–37 на ESP32-S3 N16R8: они заняты Octal PSRAM.
Если модуль вибрации подключён через MOSFET, задайте пин в config.h
и вызывайте haptic_init() из ui_physical_init().

```c
void haptic_short(void); // 15 мс — переключение/успех void haptic_error(void); // 60 мс — ошибка/опасное действие void haptic_pulse(uint32_t ms);
```

Вибромотор на LCD. По умолчанию выключен: HAPTIC_GPIO = -1.

Не используйте GPIO 33–37 на ESP32-S3 N16R8: они заняты Octal PSRAM.
Если модуль вибрации подключён через MOSFET, задайте пин в config.h
и вызывайте haptic_init() из ui_physical_init().

## `main/ui/ui.h`

```c
void ui_show_process(void);
```

Экраны

```c
void ui_show_graphs(void);
```

Экраны

```c
void ui_show_graphs_from_db(int64_t pid);
```

Экраны

```c
void ui_show_graphs_window(int minutes);
```

Экраны

```c
void ui_graph_history(void);
```

Экраны

```c
void ui_show_stats(void);
```

Экраны

```c
void ui_stats_extended(void);
```

Экраны

```c
void ui_show_logs(void);
```

Экраны

```c
void ui_show_scheme(void);
```

Экраны

```c
void ui_show_forecast(void);
```

Экраны

```c
void ui_show_heatmap(void);
```

Экраны

```c
void ui_show_manual(void);
```

Экраны

```c
void ui_show_gabriel(void);
```

Экраны

```c
void ui_mode_distill(void);
```

Режимы

```c
void ui_mode_rectify(void);
```

Режимы

```c
void ui_mode_beer(void);
```

Режимы

```c
void ui_mode_pvk(void);
```

Режимы

```c
void ui_mode_nbk(void);
```

Режимы

```c
void ui_mode_vacuum(void);
```

Режимы

```c
void ui_mode_double(void);
```

Режимы

```c
void ui_calc_dilution(void);
```

Калькуляторы

```c
void ui_calc_sugar(void);
```

Калькуляторы

```c
void ui_calc_grain(void);
```

Калькуляторы

```c
void ui_calc_yield(void);
```

Калькуляторы

```c
void ui_calc_abv_ibu_srm(void);
```

Калькуляторы

```c
void ui_calc_temp_abv(void);
```

Калькуляторы

```c
void ui_calc_pressure(void);
```

Калькуляторы

```c
void ui_calc_gabriel(void);
```

Калькуляторы

```c
void ui_settings_hw(void);
```

Настройки

```c
void ui_settings_kit(void);
```

Настройки

```c
void ui_settings_flags(void);
```

Настройки

```c
void ui_settings_calib(void);
```

Настройки

```c
void ui_settings_pid(void);
```

Настройки

```c
void ui_settings_general(void);
```

Настройки

```c
void ui_settings_export(void);
```

Настройки

```c
void ui_settings_restore(void);
```

Настройки

```c
void ui_settings_reset(void);
```

Настройки

```c
void ui_diag_overview(void);
```

Диагностика

```c
void ui_diag_i2c_scan(void);
```

Диагностика

```c
void ui_diag_ds18b20(void);
```

Диагностика

```c
void ui_diag_test_outputs(void);
```

Диагностика

```c
void ui_diag_monitor(void);
```

Диагностика

```c
void ui_diag_export(void);
```

Диагностика

```c
void ui_diag_perf(void);
```

Диагностика

```c
void ui_sensor_calib_main(void);
```

Калибровки

```c
void ui_sensor_calib_results(void);
```

Калибровки

```c
void ui_sensor_calib_manual(void);
```

Калибровки

```c
void ui_sensor_calib_list(void);
```

Калибровки

```c
void ui_valve_calib_menu(void);
```

Калибровки

```c
void ui_valve_calib_run(int idx);
```

Калибровки

```c
void ui_valve_calib_progress(int idx);
```

Калибровки

```c
void ui_alcohol_balance_show(void);
```

Баланс спирта

```c
void ui_alcohol_balance_input(void);
```

Баланс спирта

```c
void ui_scale_main(void);
```

Весы

```c
void ui_scale_calib(void);
```

Весы

```c
void ui_ai_stats(void);
```

AI

```c
void ui_ai_warnings(void);
```

AI

```c
void ui_autolearn_show(void);
```

AI

```c
void ui_tinyml_stats(void);
```

AI

```c
void ui_usb_config(void);
```

USB / FOTA

```c
void ui_fota_menu(void);
```

USB / FOTA

```c
void ui_fota_pick_sd(void);
```

USB / FOTA

```c
void ui_fota_progress(void);
```

USB / FOTA

```c
void ui_wd_status(void);
```

Watchdog / роли / обучение / рецепты / справка

```c
void ui_roles_login_dialog(void);
```

Watchdog / роли / обучение / рецепты / справка

```c
void ui_roles_current(void);
```

Watchdog / роли / обучение / рецепты / справка

```c
void ui_roles_matrix_screen(void);
```

Watchdog / роли / обучение / рецепты / справка

```c
void ui_roles_timeout_dialog(void);
```

Watchdog / роли / обучение / рецепты / справка

```c
void ui_roles_change_pin(void);
```

Watchdog / роли / обучение / рецепты / справка

```c
void ui_recipes_list(const char *cat);
```

Watchdog / роли / обучение / рецепты / справка

```c
void ui_beer_editor_list(void);
```

Watchdog / роли / обучение / рецепты / справка

```c
void ui_beer_editor_steps(int recipe_idx);
```

Watchdog / роли / обучение / рецепты / справка

```c
void ui_beer_editor_step_edit(void);
```

Watchdog / роли / обучение / рецепты / справка

```c
void ui_learning_show_lessons(const void *module);
```

Watchdog / роли / обучение / рецепты / справка

```c
void ui_learning_quiz(const void *lesson);
```

Watchdog / роли / обучение / рецепты / справка

```c
void ui_show_markdown(const char *title, const char *md);
```

Watchdog / роли / обучение / рецепты / справка

```c
void ui_show_markdown_from_file(const char *title, const char *path);
```

Watchdog / роли / обучение / рецепты / справка

```c
void ui_show_text_dialog(const char *title, const char *text);
```

Watchdog / роли / обучение / рецепты / справка

```c
void ui_help_list_category(const char *cat);
```

Watchdog / роли / обучение / рецепты / справка

```c
void ui_help_main(void);
```

Watchdog / роли / обучение / рецепты / справка

```c
void ui_help_article_list(void);
```

Watchdog / роли / обучение / рецепты / справка

## `main/ui/ui_gestures.h`

```c
void ui_gestures_feed(int16_t x, int16_t y, bool pressed);
```

Свайпы на LCD. Драйвер тача (FT6336) передаёт сюда каждое касание;
модуль сам считает расстояние/скорость, поэтому не зависит от того,
по какому LVGL-объекту пришлось касание.

Порог: 50 px и 0.3 px/мс (как в UI guidelines).

```c
void ui_gestures_process(void);
```

Выполняет отложенное действие свайпа. Вызывать из задачи UI под
hal_display_lock(); менять экран прямо в read_cb тача нельзя.

## `main/ui/ui_glass_theme.h`

```c
LV_FONT_DECLARE(lv_font_cyr_16);
```

Кириллические шрифты, сгенерированные из Segoe UI (16 и 24 px, bpp 2).
Montserrat в LVGL не содержит кириллицу, поэтому для русского текста
используем эти. Объявлены в ui/ui_glass_theme.h, лежат в ui/fonts/.

```c
LV_FONT_DECLARE(lv_font_cyr_24);
```

Кириллические шрифты, сгенерированные из Segoe UI (16 и 24 px, bpp 2).
Montserrat в LVGL не содержит кириллицу, поэтому для русского текста
используем эти. Объявлены в ui/ui_glass_theme.h, лежат в ui/fonts/.

```c
void ui_glass_theme_apply(lv_disp_t *disp);
```

Применяет тему к дисплею: акцент, destructive-цвет, тёмный режим,
шрифт по умолчанию. Вызывать после инициализации LVGL.

```c
lv_obj_t *ui_glass_card_create(lv_obj_t *parent);
```

Карточка: ширина 100 %, высота по содержимому, без скролла.

```c
lv_obj_t *ui_glass_btn_create(lv_obj_t *parent, const char *text, lv_color_t color);
```

Кнопка высотой 44 px (glass tap target).

```c
lv_obj_t *ui_glass_stat_create(lv_obj_t *parent, const char *caption, const char *value);
```

Колонка «подпись + крупное значение».

## `main/ui/ui_menu.h`

```c
void ui_menu_show_root(int selected_tab);
```

Реализованы в ui.c: очищают g_screen и вызывают build_*.

```c
void ui_menu_show_group(int tab, int selected_item);
```

Реализованы в ui.c: очищают g_screen и вызывают build_*.

```c
void ui_menu_build_root(lv_obj_t *parent, int selected_tab);
```

Реализованы в ui_menu.c.

```c
void ui_menu_build_group(lv_obj_t *parent, int tab, int selected_item);
```

Реализованы в ui_menu.c.

```c
int ui_menu_tab_count(void);
```

Реализованы в ui_menu.c.

```c
const char *ui_menu_tab_label(int tab);
```

Реализованы в ui_menu.c.

```c
void ui_menu_set_state(int tab, int level, int item);
```

Синхронизирует внутреннее состояние (нужно при входе из тача/меню).

```c
void ui_menu_encoder_rotate(int dir);
```

События энкодера. dir: -1 назад, +1 вперёд.

```c
void ui_menu_encoder_press(void);
```

События энкодера. dir: -1 назад, +1 вперёд.

```c
void ui_menu_encoder_long_press(void);
```

События энкодера. dir: -1 назад, +1 вперёд.

```c
void ui_menu_next_tab(void);
```

Жесты/свайпы LCD.

```c
void ui_menu_prev_tab(void);
```

Жесты/свайпы LCD.

```c
void ui_menu_enter(void);
```

Жесты/свайпы LCD.

```c
void ui_menu_back(void);
```

Жесты/свайпы LCD.

```c
int ui_menu_current_tab(void);
```

Жесты/свайпы LCD.

```c
int ui_menu_current_level(void);
```

Жесты/свайпы LCD.

## `main/ui/ui_physical.h`

```c
void ui_physical_poll_encoder(void);
```

Опрос энкодера (A/B/кнопка). Вызывать из задачи UI под hal_display_lock(),
частота 20–50 мс. Если ENCODER_*_GPIO == -1, функция ничего не делает.

```c
void ui_physical_beep(int ms);
```

Неблокирующий запрос звукового сигнала (0 — игнорировать).

```c
void ui_physical_set_led_power(bool on);
```

Ручное управление статусными LED (например, из UI/API).

```c
void ui_physical_set_led_heat(bool on);
```

Ручное управление статусными LED (например, из UI/API).

```c
void ui_physical_set_led_alarm(bool on);
```

Ручное управление статусными LED (например, из UI/API).

## `main/ui/ui_power_control.h`

```c
void ui_power_control_attach(lv_obj_t *parent);
```

Компактный блок ручной/автоматической мощности:
slider 0..100, статус, кнопка «Авто», ошибка неготового регулятора.

## `main/ui/ui_screen_advanced.h`

```c
void ui_screen_advanced_create(void);
```

Экран выбора алгоритма отбора и внешнего регулятора мощности.

## `main/ui/ui_screen_home.h`

```c
void ui_screen_home_build(lv_obj_t *parent);
```

Экран дашборда в стиле glass для LCD 480x320.

build() рисует содержимое в готовый parent (g_screen из ui.c).
create() объявлен здесь, но реализован в ui.c: ему нужен доступ к
статическому clear_screen(), чтобы не оставить висячий g_screen.

```c
void ui_screen_home_create(void);
```

Экран дашборда в стиле glass для LCD 480x320.

build() рисует содержимое в готовый parent (g_screen из ui.c).
create() объявлен здесь, но реализован в ui.c: ему нужен доступ к
статическому clear_screen(), чтобы не оставить висячий g_screen.

## `main/ui/ui_screen_selection.h`

```c
bool ui_screen_selection_modal_active(void);
```

Модальный редактор: энкодер перехватывает управление, пока окно открыто.

```c
bool ui_screen_selection_modal_rotate(int dir);
```

Модальный редактор: энкодер перехватывает управление, пока окно открыто.

```c
bool ui_screen_selection_modal_press(void);
```

Модальный редактор: энкодер перехватывает управление, пока окно открыто.

## `main/ui/ui_session_indicator.h`

```c
void ui_session_indicator_init(void);
```

Плавающий индикатор активной сессии на верхнем слое LCD.
Виден на всех экранах, кроме главного; тап возвращает на главный экран.

```c
void ui_session_indicator_update(void);
```

Плавающий индикатор активной сессии на верхнем слое LCD.
Виден на всех экранах, кроме главного; тап возвращает на главный экран.

## `main/verify/pid_metrics.h`

```c
esp_err_t pid_compute_metrics(const float *sp, const float *y, int n, float dt, float step_value, pid_metrics_t *out);
```

sp/y — массивы одинаковой длины; dt — шаг между отсчётами, с.
step_value — амплитуда ступеньки уставки.

## `main/verify/shadow.h`

```c
float shadow_compute(const char *channel, float setpoint, float sensor, float real_out, shadow_fn_t complex_fn);
```

Универсальная точка: возвращает только real_out, shadow пишется в лог.

```c
void shadow_tick_heater(float real_out);
```

Тик для нагрева: читает setpoint/температуру из state и пишет теневой
прогноз, не меняя реальный выход.

```c
void shadow_log_adaptive(const char *channel, float setpoint, float sensor, float real_out, float shadow_out, float kp, float ki, float kd);
```

Запись теневого результата сложного PID (канал adaptive_pid).

```c
float shadow_pid_preview(float setpoint, float sensor);
```

Простой предсказатель для тени и replay.

```c
void shadow_request_run(void);
```

Ручной запуск / один шаг / отмена.

```c
void shadow_request_step(void);
```

Ручной запуск / один шаг / отмена.

```c
void shadow_cancel(void);
```

Ручной запуск / один шаг / отмена.

## `main/verify/verify_db.h`

```c
esp_err_t verify_db_init(void);
```

Таблицы verify_trace / verify_runs в основной SQLite-базе.
Вызывается из задачи-писателя на ядре 0.

```c
bool verify_db_is_ready(void);
```

Таблицы verify_trace / verify_runs в основной SQLite-базе.
Вызывается из задачи-писателя на ядре 0.

```c
esp_err_t verify_db_replay(int64_t run_id, float (*algo)(float, float), float *max_err_out, int *rows_out);
```

Replay: входы из verify_trace прогоняются через algo(setpoint, value_real).

## `main/verify/verify_fuzz.h`

```c
esp_err_t verify_fuzz_anomaly(verify_fuzz_result_t *out);
```

Детерминированный fuzzing детектора аномалий: нормальный ряд с шумом,
затем выбросы, дрейф и пропуски. Возвращает TP/FP/FN.

## `main/web/api_auth.h`

```c
esp_err_t api_check_auth(httpd_req_t *req);
```

Проверка X-API-Key.

ESP_OK             — ключ совпадает с сохранённым в NVS.
ESP_ERR_INVALID_STATE — ключ ещё не задан (API закрыт).
ESP_FAIL           — заголовка нет или ключ неверный.

Заголовок ищем через esp_http_server; сравнение — за постоянное время
(см. control_config_api_check()).

```c
esp_err_t api_check_read_auth(httpd_req_t *req);
```

Проверка для GET-чтения: до провижининга ключа (bootstrap) чтение
разрешено, после — обязателен X-API-Key. Секретов в таких ответах нет,
но конфигурация и внутренняя телеметрия остаются закрытыми.

```c
esp_err_t api_auth_ws_token_issue(char *out, size_t out_len);
```

Короткоживущий токен для WebSocket: браузер не может передать
X-API-Key в handshake, поэтому PWA получает токен по защищённому REST
и передаёт его в Sec-WebSocket-Protocol (не постоянный ключ в URL).

```c
bool api_auth_ws_token_check(const char *token);
```

Короткоживущий токен для WebSocket: браузер не может передать
X-API-Key в handshake, поэтому PWA получает токен по защищённому REST
и передаёт его в Sec-WebSocket-Protocol (не постоянный ключ в URL).

```c
bool api_auth_ws_handshake_ok(httpd_req_t *req);
```

Проверка WebSocket-handshake: без ключа (bootstrap) — разрешено;
иначе нужен короткоживущий токен в Sec-WebSocket-Protocol
("automatika,<token>") или Authorization: Bearer <token>.

```c
esp_err_t api_reject_unauthorized(httpd_req_t *req);
```

Отправить 401 и вернуть ESP_FAIL — удобный однострочник для обработчиков.

```c
bool api_check_telegram_secret(httpd_req_t *req);
```

Webhook-исключения: Telegram и Яндекс.Алиса присылают запросы сами и
не могут знать X-API-Key. Для них проверяется секрет в URL/заголовке
(Telegram, X-Telegram-Bot-Api-Secret-Token) или строгий парсинг тела
(Алиса — только чтение состояния и старые команды, нагрев не трогают).

```c
void api_auth_get_key(char *out, size_t out_len);
```

Полный API-ключ (для QR-кода и настроек).

```c
void api_auth_show_qr(void);
```

QR-код с настройкой клиента: "API:<url>|<key>".

## `main/web/https_server.h`

```c
esp_err_t https_server_start(httpd_handle_t *out);
```

Запускает TLS-сервер на WEB_HTTPS_PORT. Обработчики (URI) регистрирует
вызывающая сторона: web_server.c навешивает один и тот же набор на HTTP
и HTTPS. Возвращает ESP_OK и заполняет *out при успехе.

```c
void https_server_stop(void);
```

Останавливает сервер и освобождает PEM-буферы. Можно не вызывать:
сервер живёт до перезагрузки.

```c
const char *https_server_cert_pem(void);
```

Публичный сертификат в PEM (для GET /cert.pem). NULL, если HTTPS не
запущен. Буфер принадлежит https_server и живёт до остановки.

## `main/web/mqtt_app.h`

```c
void mqtt_set_config(const char *uri, const char *user, const char *pass);
```

Сохраняет настройки в NVS и перезапускает клиент (uri == NULL — не менять).

```c
void mqtt_publish_state(void);
```

Сохраняет настройки в NVS и перезапускает клиент (uri == NULL — не менять).

```c
bool mqtt_is_connected(void);
```

Сохраняет настройки в NVS и перезапускает клиент (uri == NULL — не менять).

```c
void mqtt_task(void *arg);
```

Сохраняет настройки в NVS и перезапускает клиент (uri == NULL — не менять).

## `main/web/telegram_bot.h`

```c
void telegram_set_credentials(const char *token, const char *chat_id);
```

webhook_secret — необязательный секрет для входящих webhook-запросов
(Telegram отправляет его в X-Telegram-Bot-Api-Secret-Token).
NULL — не менять; "" — очистить.

```c
void telegram_set_webhook_secret(const char *secret);
```

webhook_secret — необязательный секрет для входящих webhook-запросов
(Telegram отправляет его в X-Telegram-Bot-Api-Secret-Token).
NULL — не менять; "" — очистить.

```c
bool telegram_is_configured(void);
```

webhook_secret — необязательный секрет для входящих webhook-запросов
(Telegram отправляет его в X-Telegram-Bot-Api-Secret-Token).
NULL — не менять; "" — очистить.

```c
bool telegram_get_webhook_secret(char *out, size_t *len);
```

webhook_secret — необязательный секрет для входящих webhook-запросов
(Telegram отправляет его в X-Telegram-Bot-Api-Secret-Token).
NULL — не менять; "" — очистить.

```c
void telegram_get_credentials(char *token, size_t token_len, char *chat_id, size_t chat_len);
```

webhook_secret — необязательный секрет для входящих webhook-запросов
(Telegram отправляет его в X-Telegram-Bot-Api-Secret-Token).
NULL — не менять; "" — очистить.
Копия токена/chat_id (без секретов в логах) — для telegram_notify.c.

```c
void telegram_send_legacy(const char *msg);
```

Устаревший синхронный путь (экраны/устаревшие вызовы). Новым кодом
используется telegram_notify() из telegram_notify.h (очередь).

```c
void telegram_task(void *arg);
```

Устаревший синхронный путь (экраны/устаревшие вызовы). Новым кодом
используется telegram_notify() из telegram_notify.h (очередь).

```c
void telegram_bot_task(void *arg);
```

Polling getUpdates + команды и inline-кнопки.
Задача работает на ядре 0 (приоритет 3), отдельно от telegram_task
(пересылка аварий в очередь уведомлений).

Безопасность: команды принимаются только из chat_id, сохранённого в NVS
через POST /api/telegram. Чужие чаты игнорируются без ответа.

## `main/web/telegram_notify.h`

```c
bool telegram_notify(const char *message);
```

Асинхронно поставить сообщение в очередь. Возвращает false, если
очередь полна или уведомления не настроены.

```c
bool telegram_notify_alert(const char *level, const char *message);
```

Асинхронно поставить сообщение с типом-префиксом
(«АВАРИЯ: …», «ВНИМАНИЕ: …», «ИНФО: …»).

```c
uint32_t telegram_notify_sent(void);
```

Статистика для /api/health и диагностики.

```c
uint32_t telegram_notify_failed(void);
```

Статистика для /api/health и диагностики.

```c
uint32_t telegram_notify_dropped(void);
```

Статистика для /api/health и диагностики.

## `main/web/ws_telemetry.h`

```c
esp_err_t ws_telemetry_attach(httpd_handle_t srv);
```

Регистрирует обработчик GET /ws/telemetry на уже запущенном httpd.

```c
int ws_telemetry_client_count(void);
```

Регистрирует обработчик GET /ws/telemetry на уже запущенном httpd.
