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

# переход на следующий этап
curl -X POST -H "X-API-Key: $KEY" -H "Content-Type: application/json" \
  -d '{"stage":1}' http://automatika.local/api/verify/stage
```

«Чистый» прогон нельзя отметить вручную: счётчик увеличивается только
реальным завершением adaptive PID с успешными метриками
(`verify_report_clean_run()`), поэтому `action=clean` удалён.

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
4. Применение через `ctrl_heater_set_percent()` (auto-уставка) или
   `ctrl_heater_manual_set()` для ручных источников.

## 9. Активная верификация контура

Пассивный `verify_trace` фиксирует, что произошло. `loop_verify` проверяет,
что контур замкнут: датчик выдаёт правдоподобное значение, программа
считает корректный выход, исполнитель реально отработал команду.

Три слоя, у каждого своя маска флагов:

| Слой | Флаги | Источник |
|---|---|---|
| Датчик | `SENSOR_PLAUSIBILITY`, `SENSOR_STUCK`, `SENSOR_CROSSCHECK` | `sensor_trust`, `sensor_pipeline` |
| Программа | `PROG_FSM_ILLEGAL`, `PROG_SP_SANITY`, `PROG_PID_SAT` | режим, PID |
| Исполнитель | `ACT_MISMATCH`, `ACT_NO_FEEDBACK`, `LATENCY_OVER`, `NO_RESPONSE` | `hal_actuator_readback` |

Агрегация по окну, а не по одному такту:

- `ACT_MISMATCH` — расхождение `cmd`/`actual` > 2 % держится 3 такта;
  для бинарного канала (`binary=true`, GPIO/контактор) команда
  сравнивается как «вкл/выкл», иначе 50 % против 100 % давали бы
  ложный CRITICAL;
- `NO_RESPONSE` — команда нагрева изменилась, PV не реагирует дольше
  окна актуатора: по умолчанию 30 с, для нагрева в `mode_distill` — 120 с
  с учётом тепловой инерции;
- `LATENCY_OVER` — возраст сэмпла 1-Wire или задержка readback больше
  1,5 с (1-Wire опрашивается ~1 Гц, 50 мс физически недостижимы);
- `ACT_NO_FEEDBACK` — `NAN` от readback (канал без обратной связи),
  это WARN, а не mismatch;
- `SENSOR_STUCK` — значение не меняется 600 тактов (5 минут при 2 Гц);
- `SENSOR_CROSSCHECK` — два канала с одинаковым id расходятся > 2 °C;
- `confidence < 0.3` — CRITICAL по датчику.

`loop_verify_init()` вызывается на старте каждого прогона. Разрыв времени
больше 5 с между тактами также считается новой сессией: накопленное окно
`NO_RESPONSE` сбрасывается, поэтому пауза или второй запуск не дают
ложной аварии.

Итог: `WARN → DEGRADE → CRITICAL`. CRITICAL вызывает
`safety_gate_submit_flags()`, событие `LOOP_VERIFY` уходит в `event_bus`,
затем `ctrl_safety_trigger()`. Эскалация и запись в журнал выполняются
только при смене агрегата/маски — WARN не пишется каждые 500 мс.
При активном shadow run результат дополнительно попадает в `verify_trace`
как `channel='loop'` с полями `latency_us`, `flags`, `aggregate`.
`GET /api/safety` отдаёт текущее состояние. Значение readback нормировано
в 0..100 %:

| Backend | Источник | Примечание |
|---|---|---|
| `NONE` | — | `NAN`, `has_feedback=false` |
| `GPIO` | сухой контакт, оптопара | 0 или 100 % |
| `CURRENT` | датчик тока SSR через ADC | `raw / full_scale` |
| `PWM_CAPTURE` | захват скважности | зарезервирован |

## 10. Аппаратный рубеж

`hal_safety_relay` — нормально замкнутый контакт в цепи контактора.
Прошивка не «включает» реле: она постоянно подтверждает, что жива.

```
L ──► контактор ──► NC-реле ──► SSR/симистор ──► ТЭН
                        ▲
                        │ heartbeat каждые T/4
                   safety-задача
```

- старт всегда с разомкнутого контакта;
- `hal_safety_relay_heartbeat()` вызывается только при `ctrl_safety_ok()`
  и отпущенном E-STOP;
- пропал heartbeat, зависла задача, упало питание — контакт размыкается
  без участия процессора;
- `hal_safety_relay_force_open()` ставит защёлку; снимает её только
  `hal_safety_relay_rearm()` из явного сброса аварии (`POST /api/safety`)
  после остановки процесса, отпускания E-STOP и проверки уровня;
- после rearm реле остаётся разомкнутым до первого свежего heartbeat;
- `SAFETY_RELAY_GPIO=-1` — реле не установлено, поведение V0.6 сохраняется.

## 11. Ограничения

- Без SD-карты SQLite-верификация недоступна, но управление работает.
- Теневой расчёт для нагрева использует упрощённый P-D предсказатель,
  а не полный PID; это демонстрация инфраструктуры, а не эталон.
- Метрики PID требуют подготовленного ступенчатого лога.
- Автоматическое «чистое» завершение отмечается только при успешных
  метриках adaptive PID; ручного `action=clean` больше нет.
- Подсистема требует пусконаладки и проверки на конкретном процессе.
