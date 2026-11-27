# Релиз двух прошивок

Версии задаются в [`VERSION.json`](../../VERSION.json). Теги создаются только
после зелёного CI и успешного первого запуска 2026-12-06.

## Порядок

1. Обновить `VERSION.json` (`controller.version`, `hmi.version`, `protocol.version`).
2. `python3 protocol/tools/gen_protocol_version.py` — обновить заголовок.
3. `python3 firmware_hmi/tools/gen_ui.py` — перегенерировать экраны из
   `firmware_hmi/ui_tree/menu_tree.json`.
4. Проверить рендер 10 превью и закоммитить изменения.
5. Создать два тега:

```bash
git tag -a v1.0.1-ctrl1 -m "Controller: CAN HMI slave"
git tag -a v0.9.6-hmi1  -m "HMI panel: first 800x480 build"
git push origin v1.0.1-ctrl1 v0.9.6-hmi1
```

## Артефакты

- Контроллер: `automatika_v0.9.6.zip` (существующий workflow `release.yml`).
- HMI: `hmi_0.9.6-hmi1.zip` — добавляется отдельным job в `release.yml`
  на следующем этапе; пока архив собирается вручную из
  `firmware_hmi_project/build/` + `firmware_hmi/preview/ten/`.

## Правило

Не выпускать HMI-тег без контроллерного тега с поддержкой `POLL 500 мс`.
Несовместимость версий протокола означает «НЕТ СВЯЗИ» на панели.
