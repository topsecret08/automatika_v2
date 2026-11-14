# Многоуровневый адаптивный PID

`main/control/adaptive_pid.{h,c}` — три слоя:

| Слой | Что делает | Когда доступен |
|---|---|---|
| L1 | фазовый gain scheduling + bumpless | `STAGE_PID_AUTOTUNE` |
| L2 | RLS-идентификация + IMC-пересчёт | `STAGE_PREDICTIVE` |
| L3 | детектор деградации и откат | всегда |

Включается в PWA **Подготовка → Калибровка насоса → Многоуровневый
адаптивный PID**, по REST `POST /api/control {"adaptive_pid":true}`, в
Telegram `/adaptive on`, на LCD **Система → Отбор/регулятор**.

## API

```c
adaptive_pid_t pid;
adaptive_pid_init(&pid, kp, ki, kd, 0.0f, 100.0f);

// Вычисляет выход. Применение — отдельно, через safety_gate_apply().
float out = adaptive_pid_step(&pid, setpoint, measured, dt, allow_apply);

// Сохранить текущие коэффициенты как «верифицированные» для L3-отката.
adaptive_pid_freeze_current(&pid);

// Метрики прогона (для verify_runs).
adaptive_pid_run_reset(&pid);
bool clean = adaptive_pid_run_finish(&pid, run_id);
```

`adaptive_pid_step()` никогда не пишет в железо. Реальный выход применяет
вызывающий код, и только если `allow_apply=true`:

```c
bool allow = verify_get_shadow_policy() == SHADOW_OFF &&
             verify_is_module_verified("adaptive_pid") &&
             ctrl_safety_ok();
float out = adaptive_pid_step(&pid, sp, y, 1.0f, allow);
if (allow) {
    safety_gate_apply("adaptive_pid", out);
}
```

В прошивке это уже сделано в `ctrl_pid_compute_and_apply()`.
`ctrl_pid_compute_and_apply_scaled()` добавлен для ПВК, где выход
масштабируется по давлению.

## L1 — фазы

| Фаза | Условие | Коэффициенты |
|---|---|---|
| `HEAT_UP` | ошибка > 5 °C | усиленный Kp, слабый Ki |
| `SETTLING` | ошибка 0.8…5 °C | базовые |
| `STEADY` | ошибка < 0.8 °C | меньший Kp, больший Ki |
| `DISTURBANCE` | dT/dt > 1.5 при ошибке < 3 °C | усиленный Kp |

Переключение bumpless: интеграл корректируется так, чтобы выход не
скакнул.

## L2 — RLS и IMC

Модель: `y[k] = a·y[k-1] + b·u[k-1-d]`.

- забывание `λ = 0.97`;
- пересчёт раз в 30 шагов (по умолчанию 30 с);
- `τ = -dt/ln(a)`, `K = b/(1-a)`, `d = est_delay·dt`;
- `lambda = max(0.5τ, 3d)`;
- `Kp = τ/(K(λ+d))`, `Ti = τ`, `Td = 0.5d`;
- новые коэффициенты применяются не более чем на 25 % за пересчёт.

RLS и пересчёт выполняются в задаче `ad_pid_model` на **ядре 0**. PID-выход
считается в задаче управления на **ядре 1**.

## L3 — деградация и откат

Скользящие средние `sse_slow` и `osc_index_slow` (число смен знака ошибки
в окне). Если `sse_slow > 1.0` и `osc_index_slow > 1.5` дольше 60 циклов, коэффициенты
откатываются к сохранённым `verified_*`, интеграл сбрасывается bumpless,
пишется предупреждение в журнал.

Откат возможен только если коэффициенты были сохранены через
`adaptive_pid_freeze_current()` после успешного прогона
(`adaptive_pid_run_finish()` делает это автоматически при чистых метриках).

## Верификация

- модуль зарегистрирован в `safety_gate` как `QUALITY`;
- без `verified=true` нагрев ведёт обычный PID, а адаптивный считается в
  тени;
- при `SHADOW_AUTO/MANUAL/STEP` выход и `shadow_kp/shadow_ki/shadow_kd`
  уходят в `verify_trace` (канал `adaptive_pid`);
- `verify_is_stage_enabled(STAGE_PID_AUTOTUNE)` требуется для L1;
  `STAGE_PREDICTIVE` — для L2;
- `adaptive_pid_run_finish()` считает overshoot, settling, SSE и osc через
  `pid_compute_metrics()`, пишет JSON в `verify_runs.metrics_json` и только
  при успехе вызывает `verify_report_clean_run()`.

```bash
# разрешить модуль после проверки
curl -X POST -H "X-API-Key: $KEY" -H "Content-Type: application/json" \
  -d '{"module":"adaptive_pid","verified":true}' \
  http://automatika.local/api/verify/module

# зафиксировать текущие коэффициенты
curl -X POST -H "X-API-Key: $KEY" http://automatika.local/api/verify/freeze
```

## Ограничения

- L2 работает только после этапа `STAGE_PREDICTIVE`; до этого доступен
  только L1.
- RLS чувствителен к качеству входного сигнала; используйте
  `sensor_pipeline`.
- «Чистый» прогон требует метрик в допуске; короткие прогоны могут не
  дать метрик.
- Алгоритм требует пусконаладки на конкретном процессе.
- `CONFIG_ADAPTIVE_PID_ENABLE` в menuconfig отключает модуль целиком.
