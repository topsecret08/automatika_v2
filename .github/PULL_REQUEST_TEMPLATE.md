## Что меняется

<!-- Кратко: контроллер / HMI / протокол / документация -->

## Проверки

- [ ] `idf.py -C firmware_hmi_project set-target esp32s3 && idf.py -C firmware_hmi_project build`
- [ ] `bash firmware_hmi_project/tests/run_test_proto.sh` → `PROTO TEST OK`
- [ ] `idf.py set-target esp32s3 && idf.py build` (контроллер)
- [ ] `python3 protocol/tools/gen_protocol_version.py` не меняет `protocol/generated/protocol_version.h`
- [ ] 10 превью в `firmware_hmi/preview/ten/` — строго 800×480, без обрезки краёв

## Влияние на безопасность

<!-- HMI не обходит safety_gate, E-STOP остаётся на контроллере. Подтвердите. -->

## Тест на железе

- [ ] Дата и результат заполнены в `docs/user/FIRST_BOOT_HMI.md`
