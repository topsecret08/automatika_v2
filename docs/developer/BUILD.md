# Сборка и прошивка

Проект: ESP-IDF 5.3.2, цель `esp32s3`, модуль ESP32-S3-WROOM-1 N16R8
(16 МБ flash, 8 МБ PSRAM).

## 1. Требования

- ESP-IDF **5.3.2** (другая версия может не собрать mbedTLS/littlefs API).
- Python 3.10–3.14 (см. примечание про 3.14 ниже).
- Git, CMake/Ninja из состава IDF.
- Свободное место ~2 ГБ на сборку.
- USB-кабель с передачей данных (не «только зарядка»).

Переменные окружения:

```bash
# Linux/macOS
. $IDF_PATH/export.sh

# Windows (PowerShell)
C:\Espressif\frameworks\esp-idf-v5.3.2\export.ps1
```

## 2. Основная сборка

```bash
cd automatika_v2
idf.py set-target esp32s3      # один раз, создаёт sdkconfig
idf.py build
```

Результат:

- `build/automatika_v2.bin` — приложение;
- `build/partition_table/partition-table.bin`;
- `build/bootloader/bootloader.bin`;
- `build/assets.bin` — образ littlefs 1 МиБ (PWA-оболочка).

`idf.py flash` пишет всё, включая `assets.bin` по смещению `0x920000`.

## 3. Прошивка и монитор

```bash
idf.py -p COM5 flash monitor
# Linux: idf.py -p /dev/ttyUSB0 flash monitor
```

Выход из монитора — `Ctrl+]`.

Только приложение (без образа littlefs):

```bash
idf.py -p COM5 app-flash monitor
```

> `idf.py flash` **перезаписывает раздел `assets` целиком**. Если на
> устройстве есть пользовательские скрипты/профили в `/lfs`, сначала
> сделайте резервную копию: `POST /api/config/export`.

## 4. Образ littlefs

В `main/CMakeLists.txt`:

```cmake
set(WEB_DIR "${CMAKE_CURRENT_LIST_DIR}/../littlefs")
# ...
automatika_littlefs_image(assets "${WEB_DIR}" FLASH_IN_PROJECT)
```

Что происходит:

1. CMake создаёт venv `build/littlefs_py_venv` и ставит `littlefs-python`.
2. Цель `littlefs_assets_bin` собирает `build/assets.bin` из `littlefs/`.
3. `FLASH_IN_PROJECT` добавляет образ в `idf.py flash`.

Проверить содержимое образа:

```bash
build/littlefs_py_venv/Scripts/littlefs-python.exe list build/assets.bin
```

Собрать вручную:

```bash
build/littlefs_py_venv/Scripts/littlefs-python.exe create littlefs build/assets.bin \
  --fs-size=1048576 --name-max=64 --block-size=4096
```

### Почему не `littlefs_create_partition_image` из компонента

Функция `joltwallet/littlefs` ставит `littlefs-python==0.11.0` из sdist.
На Python 3.14 у 0.11.0 нет wheel, и pip пытается собрать модуль через
MSVC. Если MSVC нет, команда падает, а ninja, не записав успешный результат
в `.ninja_log`, запускает её на каждой сборке.

Поэтому проект ставит `littlefs-python==0.19.0` (есть wheel
`cp314-win_amd64`) и собирает образ собственной целью
`automatika_littlefs_image()`, повторяющей интеграцию с `idf.py flash`.
CLI `create` совместим; образ получается ровно 1 МиБ.

## 5. Тестовая сборка

```bash
cd test
idf.py set-target esp32s3
idf.py build
idf.py -p COM5 flash monitor
```

`test/` — отдельный проект с Unity-тестами (ядро, парсеры, логика).
Он использует те же компоненты через `EXTRA_COMPONENT_DIRS`. Тесты
запускаются на устройстве; без платы их можно только собрать.

## 6. Частые ошибки

| Ошибка | Причина | Решение |
|---|---|---|
| `littlefs-python` wheel build failed | Python 3.14 + pin 0.11.0 | используется 0.19.0; удалите `build/littlefs_py_venv` и пересоберите |
| `implicit declaration mbedtls_x509write_crt_set_serial` | API mbedTLS 3.x | используется `set_serial_raw` |
| `undefined reference to httpd_ssl_*` | нет `esp_https_server` | компонент уже в `REQUIRES` |
| `region overflowed` | образ больше 3 МиБ | уменьшить функциональность/сменить таблицу |
| `littlefs mount failed` | раздел не прошит/повреждён | `idf.py flash`, при необходимости стереть `assets` |
| `CMake Error: partition 'assets' not found` | другая таблица разделов | проверить `partitions.csv` и `CONFIG_PARTITION_TABLE_CUSTOM` |
| `ninja: build stopped` после смены sdkconfig | устаревший build | `idf.py fullclean` |

Полная очистка:

```bash
idf.py fullclean
rm -rf build/littlefs_py_venv
idf.py build
```

## 7. Конфигурация

- `sdkconfig` — локальный, **не коммитится** (в `.gitignore`).
- `sdkconfig.defaults` — отслеживается, задаёт целевую конфигурацию.
- `partitions.csv` — таблица разделов, отслеживается.
- `dependencies.lock` — версии managed-компонентов.

После изменения `sdkconfig.defaults`:

```bash
idf.py fullclean
idf.py set-target esp32s3
idf.py build
```

## 8. Размеры и запас

Ориентиры для текущей сборки:

| Образ | Размер | Слот |
|---|---:|---:|
| `automatika_v2.bin` | ~2.4 МиБ | 3 МиБ (OTA) |
| `assets.bin` | 1.0 МиБ | 1 МиБ (`assets`) |
| bootloader | ~23 КБ | — |

Запас OTA-слота ~0.6 МиБ. При добавлении крупных функций проверяйте
`idf.py size` и не превышайте размер слота.

## 9. CI

В репозитории есть workflow `.github/workflows/docs.yml` — проверка
Markdown-ссылок. Сборка прошивки в CI не настроена: нужен образ ESP-IDF
и скачивание managed-компонентов; это делает локальная машина или
отдельный self-hosted runner.

Локальная проверка ссылок:

```bash
npx markdown-link-check -q docs/**/*.md README.md USER_GUIDE.md
```

## 10. Чек-лист перед коммитом

1. `idf.py build` — зелёный.
2. `build/assets.bin` существует и ровно 1 МиБ.
3. `test/` собирается.
4. Версия обновлена в `main/config.h` (`FW_VERSION`).
5. `CACHE_NAME` в `littlefs/sw.js` обновлён (иначе старая оболочка).
6. Ссылки в документации не битые.
7. В diff нет ключей, токенов и паролей.
