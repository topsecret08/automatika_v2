# ТЗ. Работа в GitHub с двумя прошивками (дисплей и контроллер) в одном проекте

Версия: 1.0
Дата: 2026-11-24
Статус: проект
Связано с: `docs/developer/CAN_PROTOCOL.md` (legacy, см. CAN_HMI)

---

## 1. Назначение

Организовать разработку, сборку, тестирование и релизы двух независимых ESP-IDF-прошивок в одном GitHub-репозитории:

1. **Controller** — существующая прошивка Automatika (ядро управления, датчики, PID, safety, REST/MQTT/OTA).
2. **HMI** — новая прошивка для Waveshare ESP32-S3-Touch-LCD-7 (LVGL 8.3, 800×480, GT911, CAN/TWAI-пульт).

Обе прошивки живут в одном репозитории, но **собираются, версионируются, прошиваются и обновляются раздельно**.

---

## 2. Исходное состояние и принятое решение

### 2.1. Что есть сейчас

- Репозиторий `topsecret08/automatika_v2` — один ESP-IDF-проект в корне:
  - `CMakeLists.txt`, `main/`, `components/`, `sdkconfig.defaults`, `partitions.csv`;
  - `idf.py build` в корне собирает `automatika_v2.bin`.
- CI: `.github/workflows/build.yml`, `release.yml`, `app.yml`, `android.yml`, `codeql.yml`.
- Контейнер сборки: `espressif/idf:v5.3.2`.
- Релиз по тегу `v*` публикует:
  - `automatika-<версия>.zip`, `Automatika-<версия>.exe`, `Autopilot-<версия>.apk`,
  - `build-info.json`, `sbom.spdx.json`, `SHA256SUMS*.txt`.
- Версия прошивки: `V1.0.1`, app-раздел почти заполнен (3% свободно).

### 2.2. Решение

**Монорепозиторий без переезда корневого проекта.**

- Корень остаётся **прошивкой контроллера** — это сохраняет все текущие пути, скрипты, конфигуратор и workflow-ы.
- HMI добавляется в отдельную папку `firmware_hmi/` как второй самостоятельный ESP-IDF-проект.
- Общий CAN-протокол выносится в `protocol/can_hmi/` и подключается к обоим проектам через `EXTRA_COMPONENT_DIRS`.
- **Git submodules не используются**: протокол меняется атомарно вместе с обеими сторонами, submodule создал бы рассогласование pin-ов.

### 2.3. Почему не менять структуру сразу

Переезд `main/`, `components/`, `littlefs/`, `tools/configurator/usb.py` в `firmware_controller/` сломает:

- относительные пути в `CMakeLists.txt` и `tools/`;
- `FLASH_PARTS` в конфигураторе;
- все существующие workflow-ы и документацию.

Поэтому переезд — отдельная фаза 2, не часть этого ТЗ.

---

## 3. Структура репозитория

```text
automatika_v2/
├─ CMakeLists.txt                 # НЕ трогаем: проект automatika_v2 = controller
├─ main/                          # НЕ трогаем: код контроллера
├─ components/                    # НЕ трогаем: esp_sqlite3 и др.
├─ littlefs/, sdcard_src/         # PWA и SD-контент контроллера
├─ partitions.csv
├─ sdkconfig.defaults
├─ test/                          # host + QEMU тесты контроллера
├─ tools/                         # конфигуратор, генераторы, build_all.py (новое)
│
├─ firmware_hmi/                  # НОВЫЙ второй ESP-IDF-проект
│  ├─ CMakeLists.txt              # project(automatika_hmi)
│  ├─ sdkconfig.defaults          # LVGL 8.3, RGB 800x480, PSRAM
│  ├─ partitions.csv              # свой размер app (графика вынесена на панель)
│  ├─ main/
│  │  ├─ main.c
│  │  ├─ hmi_can.c/.h             # CAN master, POLL/CMD/ACK
│  │  ├─ ui/                      # экраны 800x480
│  │  └─ hal/                     # RGB, GT911, CH422G, TWAI CAN
│  ├─ components/
│  │  └─ waveshare_rgb_lcd_port/  # из примера Waveshare 08_lvgl_v8_demo
│  └─ test/                       # тесты HMI на host
│
├─ protocol/                      # НОВЫЙ общий источник истины
│  ├─ can_hmi_proto.h/.c       # бинарные CAN кадры, IDs, pack/unpack
│  ├─ can_hmi_ids.h            # (в can_hmi_proto.h) IDs/команды/флаги
│  ├─ crc16.c/.h                  # CRC-16/CCITT-FALSE
│  ├─ component/CMakeLists.txt    # компонент can_hmi
│  ├─ tests/                      # golden vectors, corrupt frame, timeout
│  └─ generated/protocol_version.h# ГЕНЕРИРУЕТСЯ из VERSION.json
│
├─ docs/
│  ├─ developer/MONOREPO.md       # настоящий ТЗ
│  ├─ developer/CAN_PROTOCOL.md # формат кадра
│  ├─ developer/RELEASING.md      # пошаговый релиз
│  ├─ COMPATIBILITY.md            # матрица совместимости
│  └─ FLASHING.md                 # какой bin куда прошивать
│
├─ VERSION.json                   # НОВЫЙ единый манифест версий
└─ .github/
   ├─ CODEOWNERS
   ├─ PULL_REQUEST_TEMPLATE.md
   └─ workflows/
      ├─ build.yml                # расширить: controller + hmi + protocol
      ├─ release.yml              # расширить: два zip + manifest
      ├─ app.yml                  # без изменений
      ├─ android.yml              # без изменений
      └─ codeql.yml               # без изменений
```

Правила:

1. `firmware_hmi/` — **полноценный проект**, а не подпапка `main/`. У него свой `sdkconfig`, свой `build/`, свой `partition table`.
2. `protocol/` — **не копируется** в оба проекта. Подключается через CMake:
   ```cmake
   set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../protocol/can_hmi")
   ```
3. `project()` в HMI называется `automatika_hmi`. Это попадёт в `esp_app_desc` и защитит от перепрошивки не того устройства.
4. Папки `build/` и `firmware_hmi/build/` оба игнорируются `.gitignore`.

---

## 4. Версионирование и совместимость

### 4.1. Единый манифест `VERSION.json`

```json
{
  "product": "0.9.6",
  "protocol": { "name": "can-hmi", "version": 1 },
  "firmwares": {
    "controller": {
      "version": "0.9.6",
      "project_name": "automatika_v2",
      "artifact": "Automatika-Controller-v0.9.6.zip",
      "min_protocol": 1
    },
    "hmi": {
      "version": "1.0.0",
      "project_name": "automatika_hmi",
      "artifact": "Automatika-HMI-v1.0.0.zip",
      "min_protocol": 1
    }
  },
  "compatibility": {
    "hmi_1_0_requires_controller": ">=0.9.6",
    "controller_0_9_6_requires_hmi": ">=1.0.0"
  }
}
```

### 4.2. Правила версий

| Объект | Правило |
|---|---|
| Продукт / тег GitHub | `v0.9.6` — синхронный релиз пары прошивок |
| Контроллер | семвер, совпадает с продуктовым при релизе |
| HMI | **независимый семвер** `1.0.0`, `1.0.1`, `1.1.0` |
| Протокол CAN | отдельное целое число `1`, `2`, … |
| HMI-only hotfix | новый тег `v0.9.6-hmi1`, в манифесте меняется только `firmwares.hmi.version` |
| Controller-only hotfix | новый тег `v1.0.1-ctrl1`, меняется только `firmwares.controller.version` |

### 4.3. Generating header

`tools/gen_protocol_version.py` читает `VERSION.json` и пишет:

```c
// protocol/generated/protocol_version.h — НЕ редактировать вручную
#define CAN_PROTOCOL_NAME    "can-hmi"
#define CAN_PROTOCOL_VERSION 1
```

CI проверяет: если header отличается от сгенерированного — сборка падает.

### 4.4. матрица совместимости

`docs/COMPATIBILITY.md` — обязательная таблица:

| HMI | Controller | Протокол | Статус |
|---|---|---|---|
| 1.0.0 | 0.9.6+ | 1 | ✅ рекомендуется |
| 1.0.0 | <0.9.6 | — | ❌ HMI покажет `CONTROLLER UNSUPPORTED` |
| <1.0.0 | 0.9.6 | 1 | ❌ ревизия кадра неизвестна |

HMI при `PING/GET_CONFIG` сверяет `protocol_version`. Контроллер при несовместимом version отвечает `ACK{"ok":false,"err":"proto_mismatch"}`.

---

## 5. Ветки, коммиты и Pull Request

### 5.1. Ветки

| Ветка | Назначение |
|---|---|
| `main` | единственная стабильная. Релизы только отсюда |
| `feat/hmi-<имя>` | изменения только в `firmware_hmi/` |
| `feat/ctrl-<имя>` | изменения только в контроллере |
| `feat/can-hmi-<имя>` | изменения в `protocol/` и обеих сторонах одновременно |
| `fix/hmi-<имя>`, `fix/ctrl-<имя>` | hotfix по аналогии |

Запрещены долгоживущие ветки `hmi` и `controller` — они гарантированно расходятся с `main`.

### 5.2. Префиксы коммитов

```
[ctrl]   ...   только контроллер
[hmi]    ...   только HMI-прошивка
[proto]  ...   общий протокол / VERSION.json
[ci]     ...   workflow
[docs]   ...   документация
[tools]  ...   скрипты сборки/прошивки
```

Пример: `[proto] bump CAN protocol to 2 and update both firmwares`.

### 5.3. Pull Request

Шаблон `.github/PULL_REQUEST_TEMPLATE.md`:

```markdown
## Что затронуто
- [ ] controller (main/)
- [ ] hmi (firmware_hmi/)
- [ ] protocol (protocol/)
- [ ] ci/docs/tools

## Версии
- [ ] VERSION.json обновлён (если менялась версия/протокол)
- [ ] docs/COMPATIBILITY.md обновлён
- [ ] protocol_version.h сгенерирован заново

## Проверки
- [ ] controller-firmware зелёный
- [ ] hmi-firmware зелёный
- [ ] protocol-tests зелёный
- [ ] golden vectors добавлены при изменении кадра

## Прошивка
- [ ] Описано, куда прошивать новый bin (контроллер / HMI / оба)
```

### 5.4. CODEOWNERS

```text
/main/                     @topsecret08
/components/               @topsecret08
/firmware_hmi/             @hmi-maintainer
/protocol/                 @topsecret08 @hmi-maintainer
/VERSION.json              @topsecret08 @hmi-maintainer
/.github/workflows/        @topsecret08 @hmi-maintainer
/docs/COMPATIBILITY.md     @topsecret08 @hmi-maintainer
```

Правило: PR, меняющий `protocol/`, требует одобрения обоих владельцев.

---

## 6. Общий протокол как источник истины

### 6.1. Что лежит в `protocol/`

| Файл | Ответственность |
|---|---|
| `can_hmi_proto.h` | CAN IDs, pack/unpack helpers |
| `crc16.c/.h` | CRC-16/CCITT-FALSE, poly `0x1021`, init `0xFFFF` |
| `can_hmi_proto.h` | command/ack/flag constants |
| `generated/protocol_version.h` | версия протокола |
| `tests/` | тесты кадра на host (CRC, LEN, SEQ, NODE, TYPE, timeout) |

### 6.2. Подключение к контроллеру

В корневом `main/CMakeLists.txt`:

```cmake
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../protocol/can_hmi")
...
REQUIRES ... can_hmi
```

В `main/can_hmi.c` — только транспорт (UART2, DE, задача, диспетчер команд), **не своя CRC**.

### 6.3. Подключение к HMI

В `firmware_hmi/main/CMakeLists.txt` — так же:

```cmake
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../protocol/can_hmi")
...
REQUIRES ... can_hmi
```

### 6.4. Запреты

- ❌ Дублировать `crc16()` в двух прошивках.
- ❌ Хранить JSON-поля телеметрии сразу в двух `struct` без общего header.
- ❌ Менять формат кадра без bump `protocol.version` и обновления `docs/COMPATIBILITY.md`.
- ❌ Хранить пароли/API-ключи в `protocol/`.

---

## 7. Сборка и прошивка

### 7.1. Локальные команды

| Действие | Команда |
|---|---|
| Собрать контроллер | `idf.py set-target esp32s3 && idf.py build` |
| Собрать HMI | `idf.py -C firmware_hmi set-target esp32s3 && idf.py -C firmware_hmi build` |
| Собрать оба | `python tools/build_all.py --all` |
| Размеры | `idf.py size` / `idf.py -C firmware_hmi size` |
| Прошить контроллер | `Automatika.exe` или `idf.py flash` |
| Прошить HMI | `python tools/flash_hmi.py --port COMx` или `idf.py -C firmware_hmi flash monitor` |
| Запустить тесты протокола | `python -m pytest protocol/tests -q` |

`tools/build_all.py` — новый враппер, который последовательно вызывает оба `idf.py` и печатает сводку:

```text
controller : OK  2.65 MB / 3.00 MB  (12% free)
hmi        : OK  1.81 MB / 3.00 MB  (40% free)
protocol   : v1  OK
```

### 7.2. Защита от перепрошивки

| Механизм | Реализация |
|---|---|
| `project()` | `automatika_v2` vs `automatika_hmi` → разные `esp_app_desc.project_name` |
| `board_id` | в `VERSION.json` и в NVS: `CTRL_BOARD_V1`, `WAVESHARE_7` |
| OTA guard | перед записью проверить `project_name` и `board_id` из манифеста |
| USB-конфигуратор | выпадающий список «Controller / HMI», не один общий `flash` |
| Имя bin | `automatika_v2.bin` vs `automatika_hmi.bin` — never interchangeable |

При несовпадении: `ESP_ERR_INVALID_ARG`, в лог `WRONG_FIRMWARE_FOR_BOARD`, обновление прервано, прежняя прошивка сохранена.

### 7.3. Разделы flash

- Контроллер: **не менять** `partitions.csv` в этом ТЗ (3 МБ слоты OTA, assets 1 МБ, storage 6 МБ).
- HMI: свой `firmware_hmi/partitions.csv`. Рекомендуется 2 слота по 2 МБ + NVS; `assets` не нужен — UI встроен в прошивку.
- OTA-пути на сервере обновлений:
  - `.../controller/manifest.json` → `automatika_v2.bin`;
  - `.../hmi/manifest.json` → `automatika_hmi.bin`.
- OTA контроллера **не должна** обновлять HMI и наоборот.

---

## 8. CI/CD в GitHub Actions

### 8.1. `build.yml` — обязательные job-ы

Сохранить существующие:

- `host-tests`
- `static-analysis`
- `python-tools-and-docs`
- `pwa-e2e`
- `config-matrix`
- `qemu-tests`

Добавить:

| job | Что делает |
|---|---|
| `controller-firmware` | существующий `firmware`, переименовать, artifact `controller-${sha}` |
| `hmi-firmware` | `idf.py -C firmware_hmi ...`, artifact `hmi-${sha}` |
| `protocol-tests` | `pytest protocol/tests`, проверка golden vectors |
| `version-check` | сгенерировать `protocol_version.h`, сравнить, проверить `VERSION.json` |
| `compat-check` | проверить, что версии в `VERSION.json`, `docs/COMPATIBILITY.md` и прошивках совпадают |

Минимальный вид нового job-а:

```yaml
  hmi-firmware:
    name: hmi firmware (esp32s3)
    runs-on: ubuntu-latest
    container: espressif/idf:v5.3.2
    steps:
      - uses: actions/checkout@v4
      - name: Build HMI
        shell: bash
        run: |
          set -euo pipefail
          . "$IDF_PATH/export.sh"
          idf.py -C firmware_hmi set-target esp32s3
          idf.py -C firmware_hmi build
          idf.py -C firmware_hmi size | tee firmware_hmi/build/size.txt
      - uses: actions/upload-artifact@v4
        with:
          name: automatika-hmi-${{ github.sha }}
          path: |
            firmware_hmi/build/automatika_hmi.bin
            firmware_hmi/build/automatika_hmi.elf
            firmware_hmi/build/automatika_hmi.map
            firmware_hmi/build/bootloader/bootloader.bin
            firmware_hmi/build/partition_table/partition-table.bin
            firmware_hmi/build/size.txt
          if-no-files-found: error
```

### 8.2. Оптимизация по путям

Использовать `dorny/paths-filter@v3`:

```yaml
- uses: dorny/paths-filter@v3
  id: changes
  with:
    filters: |
      controller:
        - 'main/**'
        - 'components/**'
        - 'tools/**'
        - 'sdkconfig.defaults'
        - 'partitions.csv'
      hmi:
        - 'firmware_hmi/**'
      protocol:
        - 'protocol/**'
        - 'VERSION.json'
```

Правила:

- `protocol-tests` и `version-check` запускаются **всегда**.
- `controller-firmware` — при `controller` или `protocol`.
- `hmi-firmware` — при `hmi` или `protocol`.

### 8.3. Branch protection `main`

- Require PR, 1 approval.
- Required checks: `controller-firmware`, `hmi-firmware`, `protocol-tests`, `version-check`.
- Require branches up to date before merge.
- No force push, no direct commit.

---

## 9. Релизы

### 9.1. Один тег — два артефакта

Тег `v0.9.6` публикует GitHub Release с файлами:

```text
Automatika-Controller-v0.9.6.zip
Automatika-HMI-v1.0.0.zip
Automatika-v0.9.6.exe          # существующий конфигуратор
Autopilot-v0.9.6.apk           # существующее Android-приложение
manifest.json                  # из VERSION.json
sbom-controller.spdx.json
sbom-hmi.spdx.json
build-info-controller.json
build-info-hmi.json
SHA256SUMS-controller.txt
SHA256SUMS-hmi.txt
SHA256SUMS-app.txt
SHA256SUMS-apk.txt
```

### 9.2. Изменения в `release.yml`

1. Сохранить `SOURCE_DATE_EPOCH` из `git log -1 --format=%ct` — он уже есть, применить к обеим сборкам.
2. Собрать контролдер командой `idf.py build`.
3. Собрать HMI командой `idf.py -C firmware_hmi build`.
4. Вызвать `tools/gen_release_metadata.py` дважды:
   - с `--target controller`;
   - с `--target hmi`.
5. Упаковать `dist/controller/` и `dist/hmi/` в отдельные zip.
6. Сгенерировать `manifest.json` из `VERSION.json`, добавить в него `git_sha` и `sha256` обоих zip.
7. Прикрепить всё к одному релизу через `softprops/action-gh-release@v2`.

### 9.3. Проверка релиза

Перед публикацией CI должен проверить:

- оба zip содержат `automatika_v2.bin` / `automatika_hmi.bin`;
- `project_name` в `build-info-*.json` различается;
- `protocol.version` одинаков в обоих `build-info`;
- `sha256` в `SHA256SUMS-*.txt` совпадают с файлами;
- `manifest.json` проходит `python -m json.tool`.

### 9.4. Откат

- Откат контроллера: предыдущий `Automatika-Controller-*.zip`, OTA с откатом.
- Откат HMI: предыдущий `Automatika-HMI-*.zip`.
- Откат протокола: только согласованно — вернуть обе прошивки к совместимой паре из `docs/COMPATIBILITY.md`.

---

## 10. Документация

| Файл | Содержание |
|---|---|
| `docs/developer/MONOREPO.md` | этот ТЗ, структура, правила работы |
| `docs/developer/CAN_PROTOCOL.md` | CAN IDs, бинарные кадры, telemetry/commands |
| `docs/developer/RELEASING.md` | чек-лист релиза: bump → PR → tag → проверить assetы |
| `docs/COMPATIBILITY.md` | матрица версий прошивок и протокола |
| `docs/FLASHING.md` | таблица «плата → bin → команда» |
| `README.md` | добавить строку скачивания HMI рядом с контроллером |

Обязательное предупреждение в `README.md` и `docs/FLASHING.md`:

> Контроллер и HMI — разные прошивки для разных плат. Не прошивайте `automatika_hmi.bin` в контроллер и наоборот. E-STOP и безопасность остаются на контроллере.

---

## 11. Тестирование и приёмка

### 11.1. Тесты

| Уровень | Что проверяем |
|---|---|
| Host `protocol/tests` | CRC, LEN, SEQ, NODE, TYPE, corrupt frame, таймаут |
| Host-тесты HMI | парсер telemetry, отображение экранов 800×480 |
| Host-тесты контроллера | существующие `test/host` + новый `can_hmi_dispatcher` |
| QEMU | существующие Unity-тесты контроллера |
| Bench | кабель 10 м, E-STOP, обрыв A/B, 24 ч CRC-счётчик |
| Release | два zip, манифест, контрольные суммы, project_name |

### 11.2. Критерии приёмки

| Критерий | Значение |
|---|---|
| `build.yml` | все обязательные job-ы зелёные |
| `release.yml` | по тегу публикуются оба zip и manifest |
| Перекрёстная прошивка | отклоняется с `WRONG_FIRMWARE_FOR_BOARD` |
| Протокол | версия совпадает в обоих `build-info.json` |
| HMI без CAN | показывает `NO LINK`, контроллер продолжает работу |
| Документация | `README`, `COMPATIBILITY`, `FLASHING` обновлены |
| OTA | оба канала обновляются независимо, rollback работает |

---

## 12. Роли и доступы

| Роль | Права |
|---|---|
| Owner | релизы, branch protection, секреты |
| Controller maintainer | PR в `main/`, `components/`, `tools/` |
| HMI maintainer | PR в `firmware_hmi/` |
| Protocol reviewer | обязателен на `protocol/` и `VERSION.json` |
| CI bot | запись artifact-ов, публикация релиза по тегу |

---

## 13. Риски и митигация

| Риск | Митигация |
|---|---|
| Два `idf.py` в одном репо путают разработчика | `tools/build_all.py`, явные `-C firmware_hmi`, документ `MONOREPO.md` |
| Расхождение версий протокола | единый `VERSION.json` + генерация header + CI `version-check` |
| Прошивка HMI в контроллер | `project_name`, `board_id`, отдельные zip, OTA guard |
| HMI-PR ломает контроллер | paths-filter + обязательные checks; протокол всегда проверяется |
| Раздувание релиза | два zip вместо одного, artifact retention 30 дней |
| Потеря совместимости | `docs/COMPATIBILITY.md`, `PROTOCOL_VERSION` в ACK/PONG |
| Конфликт LVGL 8 (HMI) и удаление LVGL из контроллера | разные `sdkconfig.defaults`, разные `idf_component.yml` |

---

## 14. План внедрения

| Фаза | Работы | Результат |
|---|---|---|
| 0 | Зафиксировать `VERSION.json`, `docs/COMPATIBILITY.md`, CODEOWNERS, PR-шаблон | правила зафиксированы |
| 1 | Добавить `protocol/` и `can_hmi` компонент, golden tests | общий код протокола |
| 2 | Добавить `firmware_hmi/` со скелетом Waveshare + CAN | вторая прошивка собирается |
| 3 | Расширить `build.yml`: hmi-firmware, protocol-tests, version-check | CI видит обе прошивки |
| 4 | Добавить CAN-стек в контроллер, вырезать графику из сборки | контроллер получает ~0,4 МБ flash |
| 5 | Расширить `release.yml`: два zip + manifest | один тег = две прошивки |
| 6 | Обновить README/FLASHING/конфигуратор | пользователь понимает, что куда прошивать |
| 7 (опция) | Переезд контроллера в `firmware_controller/` | симметричная структура, после стабилизации |

---

## 15. Доставка

1. Настоящий ТЗ (`TZ_GITHUB_MONOREPO.md` → `docs/developer/MONOREPO.md`).
2. `VERSION.json` + `tools/gen_protocol_version.py` + `docs/COMPATIBILITY.md`.
3. `protocol/` с компонентом и тестами.
4. `firmware_hmi/` — второй ESP-IDF-проект.
5. Обновлённые `.github/workflows/build.yml` и `.github/workflows/release.yml`.
6. `tools/build_all.py`, `tools/flash_hmi.py`, обновлённый конфигуратор.
7. Протокол приёмки: логи CI, скриншот релиза с двумя zip, акт стендового теста.
