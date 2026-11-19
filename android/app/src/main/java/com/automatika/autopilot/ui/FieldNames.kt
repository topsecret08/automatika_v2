package com.automatika.autopilot.ui

/**
 * Понятные названия настроек контроллера (ключи API → подписи для человека).
 * Ключи без подписи показываются как есть, но уже с пометкой, что это ключ API.
 */
object FieldNames {

    private val labels = mapOf(
        "schema" to "Версия схемы настроек",
        "ctrl_mode" to "Режим управления нагревом (0 — PID, 1 — гистерезис)",
        "out_dev" to "Устройство отбора (0 — клапан, 1 — шаговый насос)",
        "hys" to "Гистерезис, °C",
        "rate_mlmin" to "Скорость отбора, мл/мин",
        "flow_mlmin" to "Полный поток, мл/мин",
        "steps_per_ml" to "Шагов двигателя на 1 мл",
        "service" to "Сервисный режим",
        "api_key_set" to "API-ключ задан",
        "lua" to "Lua-скрипты включены",
        "algo" to "Алгоритм отбора",
        "regulator" to "Внешний регулятор мощности (тип)",
        "reg_baud" to "Скорость связи с регулятором, бод",
        "reg_gpio" to "Вывод ШИМ регулятора (GPIO, -1 — не задан)",
        "reg_freq" to "Частота ШИМ, Гц",
        "reg_resolution" to "Разрядность ШИМ, бит",
        "reg_inverted" to "Инверсия ШИМ (0 — нет, 1 — да)",
        "analog_range" to "Диапазон аналогового выхода",
        "analog_min_pct" to "Аналоговый выход: минимум, %",
        "analog_max_pct" to "Аналоговый выход: максимум, %",
        "ssvr_power_sqrt" to "Регулятор SSVR: мощность по корню",
        "regulator_ready" to "Регулятор готов",
        "regulator_error" to "Ошибка регулятора",
        "rate_ml_min" to "Скорость отбора, мл/мин",
        "flow_ml_min" to "Полный поток, мл/мин",
    )

    /** Значения, которые контроллер только сообщает; менять их через приложение нельзя. */
    private val readOnly = setOf(
        "schema", "api_key_set", "service", "regulator_ready", "regulator_error",
    )

    fun label(key: String): String = labels[key] ?: "$key (ключ API)"

    fun isReadOnly(key: String): Boolean = key in readOnly
}
