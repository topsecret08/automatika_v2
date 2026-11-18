# CLI и конфигуратор

`tools/automatika_cli.py` — кроссплатформенная утилита без внешних
зависимостей: HTTP через стандартную библиотеку, поиск устройства по mDNS,
опциональное окно на Tkinter.

## Установка

Ничего устанавливать не нужно, кроме Python 3.9+:

```bash
python tools/automatika_cli.py --help
```

## Поиск устройства

```bash
python tools/automatika_cli.py discover
```

Утилита сначала пробует системное разрешение `automatika.local`, затем
отправляет mDNS A-запрос в группу `224.0.0.251` и проверяет найденные
адреса через `GET /api/health`.

## Настройки

Хост и ключ хранятся в `~/.automatika.json`:

```bash
python tools/automatika_cli.py config --set-host 192.168.1.50 --set-key KEY
```

Переменные окружения имеют приоритет:

```bash
export AUTOMATIKA_HOST=automatika.local
export AUTOMATIKA_API_KEY=...
```

## Команды

```bash
# состояние
python tools/automatika_cli.py health
python tools/automatika_cli.py state
python tools/automatika_cli.py safety

# управление
python tools/automatika_cli.py power 40
python tools/automatika_cli.py power auto
python tools/automatika_cli.py start distill
python tools/automatika_cli.py stop

# сброс аварии (те же проверки, что в API)
python tools/automatika_cli.py safety --reset
```

`health` не требует ключа. Остальные команды отправляют `X-API-Key`, если
он задан; без ключа устройство ответит `401`.

## Окно конфигуратора

```bash
python tools/automatika_cli.py gui
```

Доступны поля «Устройство» и «API-ключ», кнопки поиска, health, старт,
стоп и safety. Окно не блокирует сетевые запросы: они выполняются в фоне.

## Коды возврата

| Код | Значение |
|---:|---|
| 0 | успех |
| 1 | устройство ответило ошибкой или не найдено |
| 2 | неверные аргументы / нет Tkinter |

## Ограничения

- mDNS может быть заблокирован гостевым Wi-Fi; тогда используйте IP.
- Для HTTPS с самоподписанным сертификатом применяйте `--host https://...`
  и доверяйте сертификату на уровне ОС.
