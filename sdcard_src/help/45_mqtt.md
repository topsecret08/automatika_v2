# MQTT

## Назначение
Интеграция с Home Assistant, Node-RED.

## Настройки
- Брокер: IP/порт (1883)
- Client ID: automatika-01
- Логин/пароль

## Топики
- /automatika/state — JSON
- /automatika/cmd — команды
- /automatika/events — события
- /automatika/sensors — датчики

## MQTT Discovery
Home Assistant автоматически обнаружит:
- Датчики температуры
- Управление мощностью
- Статус процесса

## Пример
topic: automatika/state
payload: {
"cube": 82.3,
"column": 77.1,
"power": 65,
"stage": 4,
"running": true
}

## Команды
automatika/cmd/start → "1"
automatika/cmd/stop → "1"
automatika/cmd/power → "65"


## Ошибки
- Не подключается → порт
- Отключается → keep-alive
- Дубли сообщений → QoS
