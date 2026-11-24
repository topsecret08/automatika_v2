# Watchdog

## Назначение
Следит за задачами. При зависании — перезапуск.

## Пороги
- Safety: 2 сек (критично)
- Mode: 3 сек (критично)
- Scale: 5 сек (критично)
- UI: 2 сек
- Preloader: 10 сек
- Autolearn: 15 сек
- TinyML: 60 сек
- Web: 10 сек

## Действия
**Обычная задача** → перезапуск
**Критичная** → полная перезагрузка

## Регистрация
wd_register("name", timeout_ms, critical);

## Feeding
WD_FEED("name") в цикле задачи.

## Диагностика
Watchdog → Статус задач.
Видно: имя, timeout, max_stale, restarts.

## Логи
Все зависания пишутся в журнал:
WD: mode stale 3100ms restart #1
