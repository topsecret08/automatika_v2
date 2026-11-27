# Общий протокол CAN HMI (`protocol/`)

Единственный источник истины для бинарного CAN-протокола между контроллером и HMI-панелью.
Используется обеими прошивками через ESP-IDF-компонент `can_hmi`:

```cmake
# в корневом CMakeLists.txt каждого проекта, ДО project()
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../protocol/can_hmi")
```

Для контроллера это `"../protocol/can_hmi"`, для `firmware_hmi_project` —
`"../../protocol/can_hmi"`. В обоих проектах используется один приём:
добавить `EXTRA_COMPONENT_DIRS` с путём к `protocol/can_hmi` и указать
`can_hmi` в `REQUIRES` компонента `main`.

## Транспорт

- ESP32 TWAI (classic CAN 2.0A), standard 11-bit IDs, DLC=8
- Default bitrate: 500 kbit/s (configurable 250/500/1000)
- HMI is application master (poll/commands), controller responds

## CAN IDs

| ID | Dir | Purpose |
|---:|---|---|
| 0x100 | HMI->CTRL | CMD |
| 0x101 | HMI->CTRL | POLL |
| 0x180..0x186 | CTRL->HMI | TLM_A..TLM_G |
| 0x18F | CTRL->HMI | ACK/PONG/SNAPSHOT_END/CONFIG |

## Формат кадра

```
Standard CAN frame: 11-bit ID + 8 data bytes. Multi-byte values are little-endian.
```

- `LEN` — длина `PAYLOAD` (0..512).
- `SEQ` — счётчик запроса, увеличивается master (HMI).
- `NODE`: `0x10` контроллер, `0x20` HMI.
- `TYPE`:
  - `0x01 POLL`, `0x81 TELEMETRY`;
  - `0x02 CMD`, `0x82 ACK`;
  - `0x03 GET_CONFIG`, `0x83 CONFIG`;
  - `0x04 PING`, `0x84 PONG`.
- CRC-16/CCITT-FALSE, poly `0x1021`, init `0xFFFF`, без отражения и XOR-выхода.
  CRC считается от `LEN_LO` до конца `PAYLOAD`.

## Правила шины

- Master — только HMI. Контроллер отвечает **только** на запросы.
- Скорость `115200 8N1`, fallback `57600`.
- `POLL` каждые `500 мс`, timeout ответа `100 мс`, 2 повтора, `TX_GAP 2000 мкс`.
- Потеря связи: 3 пропущенных ответа = 1.5 с → HMI показывает «НЕТ СВЯЗИ».
- HMI не выполняет safety-логику и не обходит `safety_gate`; E-STOP остаётся
  на контроллере (GPIO7).

## Версия

`VERSION.json` в корне репозитория — источник версий. После его изменения:

```bash
python3 protocol/tools/gen_protocol_version.py
python3 firmware_hmi/tools/gen_ui.py
```

Сгенерированный `protocol/generated/protocol_version.h` коммитится в репозиторий.
