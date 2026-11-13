# Документация Automatika v2

Документация разделена на три уровня. Начинайте с того, который совпадает
с вашей ролью; каждый раздел самодостаточен.

| Уровень | Для кого | Что внутри |
|---|---|---|
| [Пользователь](user/QUICK_START.md) | Владелец аппарата | быстрый старт, PWA на телефон, FAQ, видео |
| [Сервис](service/SERVICE_MANUAL.md) | Монтажник, ремонтник | разборка, замена узлов, тесты, дерево поиска неисправностей |
| [Разработчик](developer/ARCHITECTURE.md) | Программист | архитектура, API, сборка, тесты |
| [Железо](hardware/pinout.md) | Инженер-схемотехник | распиновка, BOM, принципиальная схема |

## Быстрые ссылки

- [QUICK_START.md](user/QUICK_START.md) — от распаковки до первого запуска.
- [PWA_INSTALL.md](user/PWA_INSTALL.md) — установка веб-интерфейса как приложения.
- [FAQ.md](user/FAQ.md) — частые вопросы.
- [TROUBLESHOOTING.md](service/TROUBLESHOOTING.md) — дерево диагностики.
- [API.md](developer/API.md) — реальные эндпоинты прошивки.
- [BUILD.md](developer/BUILD.md) — `idf.py build`, тесты, littlefs-образ.
- [LVGL9_MIGRATION.md](developer/LVGL9_MIGRATION.md) — проверенные версии
  LVGL/esp_lvgl_port и план перехода с LVGL 8.
- [wiring_diagram.svg](hardware/wiring_diagram.svg) — схема подключения.
- [bom.csv](hardware/bom.csv) — спецификация компонентов.
- [PRODUCTION_MODULES.md](PRODUCTION_MODULES.md) — обзор production-модулей
  прошивки и отличий от исходного ТЗ.

> **Статус документации.** Схема и BOM описывают референсную сборку на
> модулях, которые поддержаны кодом. Видео-туториалы пока не записаны —
> см. [video_links.md](user/video_links.md), там перечислены темы и
> требования к записи, без фиктивных ссылок.
