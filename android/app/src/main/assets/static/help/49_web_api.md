# Web API

## Endpoints
- GET /api/state — состояние
- POST /api/start — запуск
- POST /api/stop — стоп
- POST /api/pause — пауза
- POST /api/power — мощность
- POST /api/ota — обновление
- GET /ws — WebSocket

## Авторизация
Bearer-токен:
Authorization: Bearer YOUR_TOKEN

Токен в QR-коде: Меню → QR → API-токен.

## Пример
curl -H "Authorization: Bearer TOKEN" http://192.168.1.100/api/state

## Ответ
{
"cube": 82.3,
"column": 77.1,
"power": 65,
"stage": 4,
"running": true,
"elapsed": 3600
}


## WebSocket
Подписка на обновления 1 раз/сек.
Полезно для графиков.

## Ошибки
- 401 → токен
- 404 → неверный путь
- 500 → внутренняя ошибка
