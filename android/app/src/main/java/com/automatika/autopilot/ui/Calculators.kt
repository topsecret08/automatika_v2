package com.automatika.autopilot.ui

import java.util.Locale

/**
 * Калькуляторы из ui_screens_full.c. Формулы те же; таблица температура → крепость
 * взята из ctrl_alcohol_balance.c. Работают без контроллера.
 */
object Calculators {
    private val TEMP_TABLE = listOf(
        78.3 to 96.0, 79.0 to 92.0, 80.0 to 87.0, 81.0 to 82.0, 82.0 to 78.0, 83.0 to 73.0,
        84.0 to 69.0, 85.0 to 65.0, 86.0 to 60.0, 87.0 to 56.0, 88.0 to 52.0, 89.0 to 47.0,
        90.0 to 43.0, 91.0 to 39.0, 92.0 to 34.0, 93.0 to 30.0, 94.0 to 26.0, 95.0 to 22.0,
        96.0 to 18.0, 97.0 to 14.0, 98.0 to 10.0, 99.0 to 6.0, 99.5 to 4.0, 100.0 to 0.0,
    )

    fun dilutionWater(volume: Double, fromPct: Double, toPct: Double): Double =
        volume * (fromPct / toPct - 1.0)

    fun sugarKg(volume: Double): Double = volume * 0.375

    /** Дрожжи: 20 г на кг сахара (2 % от массы). В прошивке была ошибка единиц, исправлено. */
    fun yeastGrams(sugarKg: Double): Double = sugarKg * 20.0

    fun yieldPure(volume: Double, abvPct: Double): Double = volume * abvPct / 100.0

    /** Плотность в единицах удельной плотности, например 1.050. */
    fun abvFromGravity(og: Double, fg: Double): Double = (og - fg) * 131.25

    fun tempToAbv(temp: Double): Double {
        if (temp <= TEMP_TABLE.first().first) return TEMP_TABLE.first().second
        if (temp >= TEMP_TABLE.last().first) return TEMP_TABLE.last().second
        for (i in 1 until TEMP_TABLE.size) {
            val (t1, a1) = TEMP_TABLE[i]
            if (temp <= t1) {
                val (t0, a0) = TEMP_TABLE[i - 1]
                val k = (temp - t0) / (t1 - t0)
                return a0 + k * (a1 - a0)
            }
        }
        return 0.0
    }

    fun boilingPoint(pressureHpa: Double): Double = 100.0 + (pressureHpa - 1013.0) * 0.028

    fun gabrielHeads(volume: Double, abvPct: Double): Double {
        val k = 0.30 + abvPct / 100.0 * 0.25
        return volume * k
    }

    fun gabrielAlcohol(volume: Double, abvPct: Double): Double = gabrielHeads(volume, abvPct) * abvPct / 100.0

    fun gabrielDilution(volume: Double, abvPct: Double): Double = gabrielAlcohol(volume, abvPct) / 0.28

    fun gabrielYield(volume: Double, abvPct: Double): Double = gabrielAlcohol(volume, abvPct) * 0.85 / 0.65
}

/** Ползунок калькулятора: подпись, границы, шаг, единица и значение по умолчанию (как в C). */
data class CalcSlider(
    val label: String,
    val min: Double,
    val max: Double,
    val step: Double,
    val unit: String,
    val default: Double,
)

fun formatNumber(value: Double, digits: Int): String =
    String.format(Locale.US, "%.${digits}f", value)

enum class CalcKind(
    val title: String,
    val sliders: List<CalcSlider>,
    val result: (List<Double>) -> List<String>,
) {
    DILUTION(
        "Разбавление",
        listOf(
            CalcSlider("Объём", 0.1, 50.0, 0.1, " л", 1.0),
            CalcSlider("Исходная", 20.0, 96.0, 1.0, "%", 96.0),
            CalcSlider("Целевая", 5.0, 90.0, 1.0, "%", 40.0),
        ),
        { v ->
            val water = Calculators.dilutionWater(v[0], v[1], v[2])
            listOf("Вода: ${formatNumber(water, 2)} л", "Итого: ${formatNumber(v[0] + water, 2)} л")
        },
    ),
    SUGAR(
        "Сахарная брага",
        listOf(CalcSlider("Объём", 5.0, 100.0, 1.0, " л", 20.0)),
        { v ->
            val sugar = Calculators.sugarKg(v[0])
            listOf(
                "Сахар: ${formatNumber(sugar, 2)} кг",
                "Дрожжи: ${formatNumber(Calculators.yeastGrams(sugar), 0)} г",
                "Вода: ${formatNumber(v[0], 0)} л",
                "Выход: ~${formatNumber(sugar * 0.6, 2)} л",
            )
        },
    ),
    GRAIN(
        "Зерновая брага",
        listOf(
            CalcSlider("Зерно", 1.0, 50.0, 0.5, " кг", 5.0),
            CalcSlider("Гидромодуль", 2.0, 6.0, 0.1, "", 4.0),
        ),
        { v ->
            val kg = v[0]
            listOf(
                "Вода: ${formatNumber(kg * v[1], 1)} л",
                "Ферменты А: ${formatNumber(kg * 3, 0)} г",
                "Ферменты Г: ${formatNumber(kg * 2, 0)} г",
                "Дрожжи: ${formatNumber(kg * 4, 0)} г",
                "Выход: ~${formatNumber(kg * 0.45, 1)} л",
            )
        },
    ),
    YIELD(
        "Выход",
        listOf(
            CalcSlider("Объём", 5.0, 100.0, 1.0, " л", 20.0),
            CalcSlider("Крепость", 5.0, 20.0, 0.5, "%", 12.0),
            CalcSlider("Целевая", 20.0, 96.0, 1.0, "%", 40.0),
        ),
        { v ->
            val pure = Calculators.yieldPure(v[0], v[1])
            listOf("Чистый: ${formatNumber(pure, 2)} л", "Итого: ${formatNumber(pure * 100.0 / v[2], 2)} л")
        },
    ),
    ABV(
        "ABV",
        listOf(
            CalcSlider("OG (плотность)", 1.000, 1.120, 0.001, "", 1.050),
            CalcSlider("FG (плотность)", 1.000, 1.030, 0.001, "", 1.010),
        ),
        { v -> listOf("ABV: ${formatNumber(Calculators.abvFromGravity(v[0], v[1]), 2)}%") },
    ),
    TEMP(
        "Крепость по T",
        listOf(CalcSlider("T кипения", 78.0, 100.0, 0.5, "°C", 85.0)),
        { v -> listOf("Крепость: ${formatNumber(Calculators.tempToAbv(v[0]), 1)}%") },
    ),
    PRESSURE(
        "Коррекция",
        listOf(CalcSlider("Давление", 950.0, 1050.0, 1.0, " гПа", 1013.0)),
        { v ->
            val boil = Calculators.boilingPoint(v[0])
            listOf(
                "T кипения воды: ${formatNumber(boil, 2)}°C",
                "Поправка: ${if (boil - 100.0 >= 0) "+" else ""}${formatNumber(boil - 100.0, 2)}°C",
            )
        },
    ),
    GABRIEL(
        "Метод Габриэля",
        listOf(
            CalcSlider("Объём", 5.0, 100.0, 1.0, " л", 20.0),
            CalcSlider("Крепость", 5.0, 20.0, 0.5, "%", 10.0),
        ),
        { v ->
            val heads = Calculators.gabrielHeads(v[0], v[1])
            listOf(
                "T1: ${formatNumber(heads, 1)} л",
                "T2: ${formatNumber(v[0] - heads, 1)} л",
                "Разбавить: ${formatNumber(Calculators.gabrielDilution(v[0], v[1]), 1)} л",
                "Выход: ${formatNumber(Calculators.gabrielYield(v[0], v[1]), 1)} л",
            )
        },
    ),
}
