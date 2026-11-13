# REST API и WebSocket

Все эндпоинты ниже реально зарегистрированы в `main/web/web_api.c`,
`main/web/actuator_test.c` и `main/web/ws_telemetry.c` (прошивка v2.9.3).
Никаких «предполагаемых» `/api/sensors/*` и `/api/openapi.json` в прошивке
нет.

## 1. Аутентификация

- Заголовок: `X-API-Key: <ключ>`.
- Ключ хранится в NVS, генерируется на устройстве:
  `POST /api/config/generate-key`.
- Пока ключ не создан, этот запрос разрешён без авторизации **один раз**
  (bootstrap). Дальше для ротации нужен текущий ключ.
- Ответ при ошибке: `401` и `{"error":"unauthorized","hint":"send X-API-Key"}`.

Обозначения в таблицах:

- 🔓 — без ключа;
- 🔑 — требуется `X-API-Key`;
- 🛠 — дополнительно требуется сервисный режим
  (`POST /api/diag/service {"enabled":true}`).

Rate limit: 10 запросов/с, burst 20 (на все `/api/*`).

## 2. Состояние и телеметрия

| Метод | Путь | Доступ | Описание |
|---|---|---|---|
| GET | `/api/state` | 🔓 | текущее состояние процесса |
| GET | `/api/health` | 🔓 | уровень здоровья, heap, RSSI, reset reason |
| GET | `/api/env` | 🔓 | барометр, давление колонны, проток, фракция, reflux |
| GET | `/api/ota` | 🔓 | статус OTA |
| GET | `/api/script/state` | 🔓 | состояние Lua-движка |
| GET | `/api/control` | 🔓 | текущие параметры регулирования |
| GET | `/api/config` | 🔓 | конфигурация без секретов |

`GET /api/state`:

```json
{"type":0,"stage":2,"cube":78.4,"col":77.9,"power":42.0,
 "heads":150,"body":820,"tails":0,"pressure":0.0,"vacuum":0,
 "running":1,"paused":0,"elapsed":3120,"safety":1,"estop":0,
 "slope0":0.12,"rising0":1}
```

`GET /api/health`:

```json
{"level":"ok","reason":"","free_heap":182344,"min_free_heap":154112,
 "free_psram":4123456,"uptime_s":9876,"reset":1,"wifi_rssi":-61,
 "sd":1,"db":1,"sensors":2,"safety":1,"estop":0,"running":1,
 "stage":2,"version":"2.9.3","tasks":14}
```

`level`: `ok` | `degraded` | `fail`. `reason` заполняется при `fail`.

`GET /api/env`:

```json
{"baro":{"present":1,"hpa":1012.40,"boiling_c":100.02},
 "column":{"kpa":0.00,"source":"none"},
 "flow":{"present":1,"lpm":2.10,"ok":1},
 "fraction":"body","reflux_ratio":3.00,"reflux":0}
```

## 3. Управление процессом

| Метод | Путь | Доступ | Тело / примечание |
|---|---|---|---|
| POST | `/api/start` | 🔑 | запуск по текущей конфигурации |
| POST | `/api/pause` | 🔑 | пауза |
| POST | `/api/stop` | 🔑 | остановка, нагрев снимается |
| POST | `/api/power` | 🔑 | `{"power":0..100}` |
| POST | `/api/safety` | 🔑 | сброс программной защёлки safety |
| POST | `/api/estop` | 🔑 | программная фиксация E-STOP |
| POST | `/api/dose` | 🔑 | `{"volume_ml":10,"rate_mlmin":60}` |
| POST | `/api/control` | 🔑 | параметры регулирования (см. ниже) |

`POST /api/control` / `POST /api/config`:

```json
{"ctrl_mode":1,"out_dev":0,"hys":0.5,
 "rate_mlmin":60,"flow_mlmin":2.0,"steps_per_ml":320}
```

Все поля необязательны; меняются только переданные.

## 4. События и сессии

| Метод | Путь | Доступ | Описание |
|---|---|---|---|
| GET | `/api/events?limit=N` | 🔑 | журнал, N ≤ 200 |
| GET | `/api/session` | 🔑 | снимок последней сессии |
| POST | `/api/session` | 🔑 | `{"action":"restore"}` или `{"action":"clear"}` |

`GET /api/events` возвращает массив:

```json
[{"type":0,"msg":"Web started (HTTPS)","ts":1731500000}]
```

`ts` — Unix-время (секунды), если время синхронизировано; иначе 0.

## 5. Профили и автотюн

| Метод | Путь | Доступ | Описание |
|---|---|---|---|
| GET | `/api/profiles` | 🔑 | массив имён |
| GET | `/api/profiles?name=NAME` | 🔑 | JSON профиля |
| POST | `/api/profiles` | 🔑 | создать/перезаписать (тело — JSON профиля) |
| DELETE | `/api/profiles?name=NAME` | 🔑 | удалить |
| GET | `/api/autotune` | 🔑 | `{running,done,kp,ki,kd}` |
| POST | `/api/autotune` | 🔑 | `{"action":"start","target":80}` или `{"action":"stop"}` |

## 6. OTA

| Метод | Путь | Доступ | Описание |
|---|---|---|---|
| POST | `/api/ota` | 🔑 | тело — сырой `.bin`, потоковая запись в OTA-слот |
| GET | `/api/ota` | 🔓 | статус и версия |
| POST | `/api/ota/url` | 🔑 | `{"url":"https://..."}` — managed OTA |
| POST | `/api/ota/secure` | 🔑 | `{"url":"https://..."}` — secure OTA |
| POST | `/api/ota/confirm` | 🔑 | подтвердить текущий образ вручную |

Ограничения: слот 3 МиБ, при `estop`/`safety fault` — `409`.

## 7. Настройки и сервис

| Метод | Путь | Доступ | Описание |
|---|---|---|---|
| POST | `/api/settings` | 🔑 | `{"key":"...","value":...}` |
| POST | `/api/wifi` | 🔑 | `{"ssid":"...","pass":"..."}` |
| POST | `/api/mqtt` | 🔑 | параметры MQTT |
| POST | `/api/telegram` | 🔑 | токен и chat_id |
| POST | `/api/telegram/test` | 🔑 | тестовое уведомление |
| POST | `/api/brewfather` | 🔑 | параметры интеграции |
| POST | `/api/calendar` | 🔑 | параметры календаря |
| GET/POST | `/api/samovar` | 🔓/🔑 | статус / настройка |
| GET/POST | `/api/i18n` | 🔑 | `{"lang":"ru"|"en"}` |
| POST | `/api/script/run` | 🔑 | запуск Lua-скрипта |
| POST | `/api/script/enable` | 🔑 | `{"enabled":true}` (нужен сервисный режим) |
| POST | `/api/config/export` | 🔑 | `{"path":"/sdcard/cfg.json","secrets":false}` |
| POST | `/api/config/import` | 🔑 | `{"path":"..."}` или `{"reset":true}` |
| POST | `/api/config/generate-key` | 🔑/bootstrap | создать или rotate ключ |
| POST | `/api/config/clear-key` | 🔑 | удалить ключ |
| POST | `/api/backup/telegram` | 🔑 | отправить резервную копию |

## 8. Диагностика приводов

| Метод | Путь | Доступ | Описание |
|---|---|---|---|
| GET | `/api/diag/status` | 🔑 | сервисный режим, safety, estop |
| POST | `/api/diag/service` | 🔑 | `{"enabled":true|false}` |
| POST | `/api/diag/actuators` | 🔑🛠 | команда приводу |

`POST /api/diag/actuators`:

```json
{"actuator":"heater","action":"on","pct":20,"seconds":5}
{"actuator":"heater","action":"off"}
{"actuator":"valve","action":"open"}
{"actuator":"valve","action":"close"}
{"actuator":"pump","action":"dose","ml":10,"rate":60}
{"actuator":"relay","action":"set","ch":0,"state":1}
```

## 9. WebSocket

| Путь | Доступ | Описание |
|---|---|---|
| `/ws/telemetry` | 🔓 | телеметрия раз в секунду |
| `/ws` | 🔓 | legacy-канал, тот же набор полей |

Кадр `/ws/telemetry`:

```json
{"tc":78.40,"ts":77.90,"p":42.0,"m":12.5,
 "sa":1,"es":0,"lv":68,"r":1,"stg":2}
```

- `tc` — куб, °C;
- `ts` — царга, °C;
- `p` — мощность, %;
- `m` — скорость отбора, мл/мин;
- `sa` — safety ok;
- `es` — E-STOP;
- `lv` — уровень, %;
- `r` — процесс запущен;
- `stg` — этап.

Соединение работает и по `ws://`, и по `wss://` (при поднятом HTTPS).
Сервер отслеживает клиентов отдельно для HTTP и HTTPS.

## 10. Статические ресурсы PWA

| Путь | Тип | Кэш |
|---|---|---|
| `/` | `text/html` | `no-cache` |
| `/js/app.js`, `/js/menu.js`, `/css/tokens.css`, `/css/components.css` | JS/CSS | `public, max-age=86400` |
| `/vendor/uPlot.iife.min.js`, `/vendor/uPlot.min.css` | JS/CSS | `public, max-age=604800` |
| `/app.js`, `/style.css` | алиасы совместимости | `public, max-age=86400` |
| `/manifest.json` | `application/manifest+json` | `public, max-age=3600` |
| `/sw.js` | JS + `Service-Worker-Allowed: /` | `no-cache` |
| `/offline.html` | HTML | `no-cache` |
| `/icons/*.png` | PNG | `public, max-age=604800` |
| `/cert.pem` | `application/x-pem-file` | `no-store` |

`/key.pem`, `/cert.pem` через статический обработчик не отдаются
(`/cert.pem` — отдельный обработчик, `/key.pem` — всегда 404).

## 11. Примеры

```bash
KEY=$(curl -s -X POST http://automatika.local/api/config/generate-key \
      | sed -E 's/.*"api_key":"([^"]+)".*/\1/')

curl -s -H "X-API-Key: $KEY" http://automatika.local/api/health

curl -s -X POST -H "X-API-Key: $KEY" -H "Content-Type: application/json" \
     -d '{"power":35}' http://automatika.local/api/power

curl -s -H "X-API-Key: $KEY" "http://automatika.local/api/events?limit=20"
```

## 12. Telegram-бот

Бот работает через long polling `getUpdates` в задаче `tg_bot`
(ядро 0, приоритет 3). Токен и chat_id хранятся в NVS и задаются через
`POST /api/telegram`.

**Авторизация:** команды и callback-кнопки принимаются только из chat_id,
сохранённого в NVS. Сообщения из других чатов игнорируются без ответа.

| Команда | Действие |
|---|---|
| `/start`, `/help` | справка и inline-клавиатура |
| `/status` | режим, этап, температуры, мощность, фракция, E-STOP |
| `/run` | запуск через `process_control_start()` |
| `/stop` | остановка, нагрев снимается |
| `/pause`, `/resume` | пауза/продолжение |
| `/power N` | мощность 0–100 %, через `ctrl_heater_set_percent()` |
| `/fraction` | текущая фракция и пороги |
| `/setpoint N` | `target_cube_t`, °C (порог хвостов) |
| `/log` | последние 8 событий |
| `/profile` | список профилей; `load` не поддерживается |

Inline-кнопки: `run`, `stop`, `restart`, `status`, `log`, `estop_ack`.
`estop_ack` сбрасывает защёлку только если физическая кнопка отпущена;
`ctrl_safety_reset()` вызывается после успешного подтверждения.

Ограничения: загрузка профиля и смена режима из бота не реализованы —
это меняет уставки и требует подтверждения в веб-интерфейсе.

## 13. SSVC0059

Дополнительный алгоритм отбора HEADS → BODY → TAILS, выключен по умолчанию.
При включении заменяет `fraction_update()`/`fraction_apply()` в
`mode_distill.c`; два алгоритма одновременно клапаном не управляют.

| Метод | Путь | Доступ | Описание |
|---|---|---|---|
| GET | `/api/ssvc/state` | 🔓 | этап, `t_fix`, `open_time`, состояние клапана |
| GET | `/api/ssvc/config` | 🔓 | полный конфиг SSVC |
| POST | `/api/ssvc/config` | 🔑 | частичное обновление параметров |
| POST | `/api/ssvc/options` | 🔑 | устройство, переходы, режим отбора, датчик, `enabled` |
| POST | `/api/ssvc/next` | 🔑 | принудительный следующий этап |

`GET /api/ssvc/state`:

```json
{"stage":1,"stage_name":"body","t_fix":78.30,
 "open_time":1.20,"period":10.00,"valve_open":1,
 "active":1,"enabled":1}
```

`stage`: 0 — heads, 1 — body, 2 — tails, 3 — done.

`POST /api/ssvc/options`:

```json
{"enabled":true,"out_device":0,"transition_mode":0,
 "select_mode":0,"sensor":0}
```

`out_device`: 0 — клапан, 1 — насос. `transition_mode`: 0 — авто,
1 — вручную. `select_mode`: 0 — периодический, 1 — непрерывный.
`sensor`: 0 — T куба, 1 — T пара, 2 — T колонны.

`POST /api/ssvc/config` принимает любые поля из таблицы:

```json
{"heads_timer_sec":3600,"heads_open_time_sec":2.0,"heads_period_sec":10.0,
 "body_open_time_sec":1.5,"body_period_sec":10.0,"body_hysteresis_c":0.2,
 "body_decrement_percent":10,"body_min_open_time_sec":0.1,
 "body_temp_fix_delay_sec":0,"tails_temperature_end":98.0,
 "tails_open_time_sec":1.0,"tails_period_sec":10.0}
```

Конфиг хранится в NVS (`ssvc/cfg`) с magic/version. При включённом SSVC
`out_device` синхронизируется с `control_config` (`out_dev`), чтобы
`ctrl_dose_process()` и `valve_set_pct()` управляли тем же устройством.

Логика и защита:

- BODY фиксирует `T_fix` через `body_temp_fix_delay_sec`; при
  `T > T_fix + body_hysteresis_c` выход закрывается, `open_time`
  уменьшается на `body_decrement_percent` (не чаще раза в секунду).
- Когда `open_time < body_min_open_time_sec`, этап завершается; в ручном
  режиме отбор остаётся закрытым до `POST /api/ssvc/next`.
- TAILS завершается при `T куба >= tails_temperature_end`.
- `apply_output()` закрывает выход при E-STOP или `!ctrl_safety_ok()`;
  это дополнение к safety-задаче, а не её замена.
- Непрерывный режим для клапана — постоянно открыт (без ШИМ 50 %, чтобы
  соленоид не дребезжал); для насоса — постоянная подача.
