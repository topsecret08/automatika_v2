# Протокол HMI CAN (TWAI) — Automatika

Версия: 2.0
Статус: актуально (CAN/TWAI, заменяет прежний HMI link)

## 1. Назначение

Выносной HMI (Waveshare ESP32-S3-Touch-LCD-7, 800×480) подключается к контроллеру Automatika по **CAN 2.0A (TWAI)** через трансиверы TJA1051.

HMI — только панель отображения/управления. Safety, PID, нагрев 220 В, клапаны, E-STOP и процессная логика остаются на контроллере.

## 2. Физический уровень

| Параметр | Значение |
|---|---|
| Шина | CAN 2.0A, standard 11-bit IDs, DLC=8 |
| Скорость | 500 kbps по умолчанию (конфиг 250/500/1000) |
| Контроллер TX/RX | GPIO17 / GPIO18 (TWAI, GPIO matrix) |
| Контроллер трансивер | TJA1051 / CJMCU-1051, 3.3V логика |
| HMI TX/RX | GPIO19 / GPIO20 (Waveshare onboard TJA1051) |
| Терминаторы | 120 Ом на обоих концах шины |
| Общий GND | обязателен |

GPIO19/20 на контроллере ESP32-S3 не используются (native USB). GPIO15/16 на HMI не используются (панель/отладка).

## 3. Модель обмена

HMI — application master: периодически отправляет `POLL`, при необходимости отправляет `CMD`. Контроллер отвечает кадрами телеметрии и `ACK`. Контроллер не инициирует трафик сам.

- Poll period: 200 ms (HMI)
- Reply timeout: 150 ms
- Link lost: 3 пропущенных ответа

## 4. CAN IDs

| ID | Направление | Назначение |
|---:|---|---|
| `0x100` | HMI -> CTRL | CMD |
| `0x101` | HMI -> CTRL | POLL (data[0]=seq) |
| `0x180` | CTRL -> HMI | TLM_A state/flags |
| `0x181` | CTRL -> HMI | TLM_B temps |
| `0x182` | CTRL -> HMI | TLM_C temps/level |
| `0x183` | CTRL -> HMI | TLM_D pressure/flow/power |
| `0x184` | CTRL -> HMI | TLM_E volumes/LSA |
| `0x185` | CTRL -> HMI | TLM_F PT506/XGZP/ZMPT |
| `0x186` | CTRL -> HMI | TLM_G target/deph/elapsed |
| `0x18F` | CTRL -> HMI | ACK/PONG/SNAPSHOT/CONFIG |

## 5. Формат данных

Все multi-byte поля little-endian. `data[0]` — `seq` в телеметрии/командах/ACK.

### TLM_A (state/flags)
`seq, stage, mode_type, flags1, flags2, eq_phase, estop_mode|flags3_hi, reason_code`

### TLM_B (temps, x100 C, int16)
`seq, cube_x100, column_x100, deph_x100, reserved`

### TLM_C (temps/level)
`seq, jacket_x100, tank_x100, ambient_x100, level_pct`

### TLM_D (process)
`seq, pressure_x1000(bar), vacuum_x10(mbar), flow_x100(LPM), power_x2`

### TLM_E (volumes)
`seq, heads_ml_u16, body_ml_u16, tails_ml_u16, lsa_duty_x2`

### TLM_F (sensors)
`seq, pt506_x1000(bar), xgzp_x10(kPa), zmpt_v1, zmpt_v2, zmpt_v3, fault_phase`

### TLM_G (target/elapsed)
`seq, target_x100, deph_x100, elapsed_lo_u16, elapsed_hi_u8, reserved`

### CMD (HMI -> CTRL)
`seq, cmd_id, p0..p5`

`cmd_id`: 1=START, 2=STOP, 3=PAUSE, 4=POWER, 5=SAFETY_RESET, 6=PING, 7=GET_CONFIG.

POWER: `p0.bit0=auto`, `p1..p2=power_x10 u16`.

### ACK (CTRL -> HMI)
`seq, kind, ok, err_code, data[4]`

kind: 1=ACK, 2=PONG, 3=SNAPSHOT_END, 4=CONFIG.
SNAPSHOT_END data: `heap_kb_u16, elapsed_lo_u16`.
CONFIG data: `proto_major, proto_minor, proto_patch, tlm_frames`.

## 6. Безопасность

- Любая команда HMI проходит через те же проверки, что и REST/API: `safety_gate`, `ctrl_safety`, E-STOP, bench mode.
- HMI не может сбросить E-STOP/bench по шине.
- При потере CAN-связи контроллер продолжает автономную безопасную работу по своей логике; HMI показывает NO LINK.
- CAN имеет аппаратный CRC кадра; приложение использует seq и fixed-layout frames.

## 7. Исходники

- Протокол: `protocol/can_hmi/can_hmi_proto.h`
- Контроллер: `main/can_hmi.c`
- HMI: `firmware_hmi_project/main/hmi_can.c`
- Конфиг контроллера: `main/Kconfig.projbuild` (`HMI_CAN_*`)
- Конфиг HMI: `firmware_hmi_project/main/hmi_proto.h`
