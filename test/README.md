# Автоматика V0.6 beta — тесты (Unity) для чистой логики

Проверяются модули без зависимости от NVS/железа:

| Файл | Что проверяется |
|---|---|
| `test_temp_filter.c` | сглаживание шума, отказ датчика (-127 °C), детекция роста, сброс trend при спаде, «плоский» сигнал не считается ростом |
| `test_level_logic.c` | отсутствие датчика не блокирует нагрев, пороги из `config.h` согласованы |
| `test_event_log.c` | JSON-экранирование журнала событий, ограничение `limit` |
| `test_pump_doser.c` | значения по умолчанию `PumpCalib`, отказ при нулевом/отрицательном объёме и превышении `DOSER_MAX_DOSE_ML`, валидация `pump_calib_save(NULL)` |
| `test_regulator.c` | duty 0/25/50/100 %, NaN/диапазон, инверсия, допустимые GPIO ESP32-S3, Finglai sqrt/калибровка |
| `test_heater_logic.c` | ручной приоритет, E-STOP/safety/уровень, обязательный датчик, запрет нагрева вне процесса |
| `test_estop_logic.c` | активный высокий уровень NC-кнопки |

## Host-тесты

`test/host` — обычная CMake/CTest-сборка без ESP-IDF, запускается в CI:

```bash
cmake -S test/host -B test/host/build
cmake --build test/host/build
ctest --test-dir test/host/build --output-on-failure
```

Покрывает duty mapping, Finglai `sqrt(P)`, калибровку, GPIO и блокировки
нагрева. Unity-тесты `test/` собираются в ESP-IDF и запускаются на
устройстве.

## Запуск

```bash
cd test
idf.py set-target esp32s3
idf.py build
idf.py -p COM5 flash monitor
```

В мониторе — меню Unity: `Unity test run <n> tests`, результат
`--------- Tests 14 ---------, OK`.

## Почему отдельный проект

- Основной `idf.py build` в корне не включает `UNITY_INCLUDE_CONFIG_H` и
  не собирает `test/` — продуктивная прошивка не разрастается.
- Тестовый компонент `test/main` компилирует только нужные production-модули
  (`temp_filter.c`, `level_sensor.c`, `event_log.c`, `events.c`,
  `pump_doser.c`) и лёгкие заглушки `stubs.c`. Продуктивный компонент `main`
  сюда не добавляется: он содержит свой `app_main()` и тянет весь проект.

## Что не покрыто юнит-тестами (и почему)

`ctrl_dose.c`, `nvs_migration.c` и `config_backup.c` требуют `nvs_flash`,
GPIO и файловой системы. Их проверка требует интеграционных сценариев
на плате (или host-сборки с моками) — это следующий шаг. `pump_dose_volume()`
с корректным объёмом тоже не тестируется: он ставит работу в очередь задачи
`doser`, а её нужно поднимать в интеграционном тесте.

Проверено на этой итерации (реальная сборка ESP-IDF 5.3.2, esp32s3):

- продуктивная прошивка: `idf.py build` — OK, образ ≈2,80 МБ (`0x2ABE80`);
- тестовый проект: `test/` — OK, образ `0x3D340`.
