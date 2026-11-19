package com.automatika.autopilot.ui

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class GraphCardTest {

    @Test
    fun axisRangeAddsMarginAroundValues() {
        val (lo, hi) = axisRange(listOf(78.0, 90.0))!!
        assertEquals(76.0, lo, 1e-9)
        assertEquals(92.0, hi, 1e-9)
    }

    @Test
    fun flatSeriesGetsVisibleRange() {
        val (lo, hi) = axisRange(listOf(80.0, 80.0))!!
        assertEquals(78.0, lo, 1e-9)
        assertEquals(83.0, hi, 1e-9)
    }

    @Test
    fun noValuesGiveNoRange() {
        assertNull(axisRange(emptyList()))
    }
}
