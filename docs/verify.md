# Верификация алгоритмов и теневой режим

Документ описывает подсистему `verify/*`, `control/safety_gate.*` и
`verify/shadow.*`. Подсистема позволяет проверять сложные алгоритмы без
воздействия на оборудование.

## 1. Включение

`menuconfig` → **Verification / shadow mode** → `CONFIG_VERIFY_ENABLE`.

По умолчанию включено. При выключении:

- остаётся только критическая логика;
- `safety_gate_apply()` пропускает только `CRITICAL`;
- `QUALITY`/`EXPERIMENTAL` всегда блокируются;
- REST `/api/verify/*` возвращает состояние с `enabled:0`.

## 2. Теневой режим

Политики (`shadow_policy_t`):

| Политика | Значение | Поведение |
|---|---|---|
| `off` | 0 | сложная логика не считается |
| `manual` | 1 | считается после команды `run`, пишется в лог |
| `auto` | 2 | считается непрерывно, только лог |
| `step` | 3 | один расчёт по команде `step`, затем пассив |

Реальный выход всегда остаётся от простого регулятора. Теневой выход
пишется в `verify_trace` с частотой не более 1 Гц на канал.

```bash
curl -X POST -H "X-API-Key: $KEY" -H "Content-Type: application/json" \
  -d '{"policy":2}' http://automatika.local/api/verify/shadow

# ручной запуск / один шаг / отмена
curl -X POST -H "X-API-Key: $KEY" -H "Content-Type: application/json" \
  -d '{"action":"run"}'  http://automatika.local/api/verify/action
curl -X POST -H "X-API-Key: $KEY" -H "Content-Type: application/json" \
  -d '{"action":"step"}' http://automatika.local/api/verify/action
```

## 3. Поэтапное включение

Этапы (`verify_stage_t`) идут по возрастанию риска:

```
simple_relay → pid_manual → pid_autotune → heads_detector
→ body_tails_detector → alcohol_balance → predictive
```

Переход на следующий этап разрешён только после
`VERIFY_MIN_CLEAN_RUNS_PER_STAGE` (3) «чистых» прогонов текущего этапа.
Прыжок через этап отклоняется с `409 need_clean_runs` или
`ESP_ERR_INVALID_STATE`.

```bash
curl -H "X-API-Key: $KEY" http://automatika.local/api/verify/state

# отметить чистый прогон текущего этапа
curl -X POST -H "X-API-Key: $KEY" -H "Content-Type: application/json" \
  -d '{"action":"clean"}' http://automatika.local/api/verify/action

# перейти на следующий этап
curl -X POST -H "X-API-Key: $KEY" -H "Content-Type: application/json" \
  -d '{"stage":1}' http://automatika.local/api/verify/stage
```

## 4. Флаги модулей

Каждый модуль имеет класс:

| Класс | Применение |
|---|---|
| `critical` | всегда, после safety/E-STOP |
| `quality` | только после `verified=true` |
| `experimental` | только после `verified=true` и при shadow != off |

```bash
# отметить модуль проверенным
curl -X POST -H "X-API-Key: $KEY" -H "Content-Type: application/json" \
  -d '{"module":"heads_detector","verified":true}' \
  http://automatika.local/api/verify/module
```

`experimental` не может быть `critical`: реестр в `control/safety_gate.c`
статический, попытка зарегистрировать экспериментальный модуль как
критический требует изменения кода и ревью.

## 5. SQLite

Таблицы создаются в `/sdcard/automatika.db`:

```sql
CREATE TABLE verify_runs(
  run_id INTEGER PRIMARY KEY AUTOINCREMENT,
  started_at_ms INTEGER, mode TEXT, policy INTEGER, stage INTEGER,
  result_metrics_json TEXT);

CREATE TABLE verify_trace(
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  run_id INTEGER, ts_ms INTEGER, channel TEXT,
  setpoint REAL, sensor REAL, value_real REAL, value_shadow REAL,
  stage INTEGER);
```

Запись идёт через очередь и задачу `shadow_wr` на ядре 0. Потеря SD-карты
не блокирует управление: записи просто отбрасываются.

Replay:

```bash
curl -X POST -H "X-API-Key: $KEY" -H "Content-Type: application/json" \
  -d '{"run_id":12}' http://automatika.local/api/verify/replay
# {"ok":1,"run_id":12,"rows":340,"max_err":0.000000}
```

Replay заново считает `algo(setpoint, sensor)` и сравнивает с
`value_shadow`.

## 6. PID-метрики

`verify/pid_metrics.c` считает:

- `overshoot_pct` — перерегулирование, %;
- `settling_s` — время установления (5 %);
- `sse` — средняя ошибка на последних 20 %;
- `osc_index` — доля смен знака производной.

Пороги: `PID_OVERSHOOT_MAX=5 %`, `PID_SETTLING_MAX=120 с`,
`PID_SSE_MAX=2 °C`, `PID_OSC_MAX=0.10`. Функция возвращает `ESP_OK`, если
все метрики в норме, иначе `ESP_ERR_INVALID_RESPONSE`.

Использование:

```c
pid_metrics_t m;
esp_err_t ok = pid_compute_metrics(sp, y, n, dt, step_c, &m);
```

## 7. Fuzzing детектора аномалий

`verify/verify_fuzz.c` строит детерминированный «нормальный» ряд, добавляет
выбросы, обрыв `-127 °C` и дрейф, затем считает TP/FP/FN детектора.

```bash
curl -X POST -H "X-API-Key: $KEY" http://automatika.local/api/verify/fuzz
# {"ok":1,"tp":4,"fp":0,"fn":1,"injected":5}
```

## 8. Safety gate

Все внешние пути управления нагревом и клапанами (REST, Telegram, MQTT,
Lua, UI) вызывают `safety_gate_apply()` или `safety_gate_apply_valve()`.
Внутренний `ctrl_heater_set_percent()` также проходит через гейт как
`heater_core`; `valve_set_pct()` — как `valve_core`.
`ctrl_heater_set_raw()` / `valve_set_pct_raw()` — только для гейта и
safety-задачи.

Порядок проверок:

1. E-STOP / `ctrl_safety_ok()` → выход 0.
2. Класс `QUALITY`/`EXPERIMENTAL` без `verified` → отказ.
3. `EXPERIMENTAL` при `shadow=off` → отказ.
4. Применение через `ctrl_heater_set_raw()`.

## 9. Ограничения

- Без SD-карты SQLite-верификация недоступна, но управление работает.
- Теневой расчёт для нагрева использует упрощённый P-D предсказатель,
  а не полный PID; это демонстрация инфраструктуры, а не эталон.
- Метрики PID требуют подготовленного ступенчатого лога.
- Автоматическое «чистое» завершение процесса пока не отмечается:
  используйте `action=clean` после проверки.
- На реальном оборудовании подсистема не проверялась.
