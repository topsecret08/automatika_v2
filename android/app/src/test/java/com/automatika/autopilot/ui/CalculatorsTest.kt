package com.automatika.autopilot.ui

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class CalculatorsTest {

    private val delta = 1e-6

    @Test
    fun dilutionMatchesFirmware() {
        assertEquals(1.4, Calculators.dilutionWater(1.0, 96.0, 40.0), delta)
    }

    @Test
    fun sugarYeastIsGramsPerKilogram() {
        val sugar = Calculators.sugarKg(20.0)
        assertEquals(7.5, sugar, delta)
        assertEquals(150.0, Calculators.yeastGrams(sugar), delta)
    }

    @Test
    fun yieldPureAlcohol() {
        assertEquals(2.4, Calculators.yieldPure(20.0, 12.0), delta)
    }

    @Test
    fun abvFromGravity() {
        assertEquals(5.25, Calculators.abvFromGravity(1.050, 1.010), 1e-3)
    }

    @Test
    fun tempToAbvUsesTheTable() {
        assertEquals(96.0, Calculators.tempToAbv(78.3), delta)
        assertEquals(65.0, Calculators.tempToAbv(85.0), delta)
        assertEquals(89.5, Calculators.tempToAbv(79.5), delta)
        assertEquals(0.0, Calculators.tempToAbv(101.0), delta)
    }

    @Test
    fun boilingPointCorrection() {
        assertEquals(100.0, Calculators.boilingPoint(1013.0), delta)
        assertEquals(100.28, Calculators.boilingPoint(1023.0), 1e-6)
    }

    @Test
    fun gabrielMethod() {
        assertEquals(6.5, Calculators.gabrielHeads(20.0, 10.0), delta)
        assertEquals(0.65, Calculators.gabrielAlcohol(20.0, 10.0), delta)
        assertEquals(0.65 / 0.28, Calculators.gabrielDilution(20.0, 10.0), delta)
        assertEquals(0.85, Calculators.gabrielYield(20.0, 10.0), delta)
    }

    @Test
    fun formatUsesDotDecimalSeparator() {
        assertEquals("1.40", formatNumber(1.4, 2))
    }

    @Test
    fun sugarResultShowsGrams() {
        val lines = CalcKind.SUGAR.result(listOf(20.0))
        assertTrue(lines.contains("Дрожжи: 150 г"))
    }

    @Test
    fun everyCalculatorHasSlidersWithValidRanges() {
        CalcKind.values().forEach { kind ->
            kind.sliders.forEach { s ->
                assertTrue("${kind.name} ${s.label}", s.min < s.max && s.step > 0 && s.default in s.min..s.max)
            }
        }
    }
}
