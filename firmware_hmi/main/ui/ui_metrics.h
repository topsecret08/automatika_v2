#pragma once

/*
 * Гибридное масштабирование для Waveshare ESP32-S3-Touch-LCD-7 (800x480).
 *
 * Базовый UI был 480x320. Максимальный равномерный масштаб = 1.5,
 * потому что 320 * 1.5 = 480 (по высоте упираемся точно).
 *
 * Но безопасность и крупные значения лучше оставить крупными,
 * а вторичную информацию (лог, диагностику) можно уплотнить.
 */

#define UI_SCALE_SAFE   1.50f   // E-STOP, Start/Stop, KPI
#define UI_SCALE_MAIN   1.35f   // карточки, заголовки
#define UI_SCALE_DENSE  1.20f   // лог, таблицы, диагностика

#define UI_PX(x)        ((int)(((x) * UI_SCALE_SAFE)  + 0.5f))
#define UI_PX_MID(x)    ((int)(((x) * UI_SCALE_MAIN)  + 0.5f))
#define UI_PX_DENSE(x)  ((int)(((x) * UI_SCALE_DENSE) + 0.5f))

/* Базовые отступы */
#define UI_PAD_XS       UI_PX_DENSE(4)
#define UI_PAD_SM       UI_PX_DENSE(8)
#define UI_PAD_MD       UI_PX_MID(12)
#define UI_PAD_LG       UI_PX_MID(16)

/* Высоты зон */
#define UI_HEADER_H     UI_PX(56)      // 84 px
#define UI_CARDS_H      UI_PX_MID(104) // ~140 px
#define UI_POWER_H      UI_PX_DENSE(54)// ~65 px
#define UI_STATUS_W     UI_PX_DENSE(150)// 180 px
#define UI_EVENTS_H     UI_PX_DENSE(64) // 77 px

/* Элементы */
#define UI_BTN_H        UI_PX(40)      // 60 px — уверенное нажатие
#define UI_CARD_R       12
#define UI_ROW_R        10

/* Цвета (тёмная тема HMI) */
#define UI_BG           0x0B1020
#define UI_CARD         0x151B2E
#define UI_CARD_ALT     0x1B2340
#define UI_TEXT         0xE8EEF7
#define UI_MUTED        0x93A4BD
#define UI_BLUE         0x2E7CF6
#define UI_GREEN        0x28C76F
#define UI_RED          0xFF4D4F
#define UI_AMBER        0xFFA726

/* Futuristic industrial palette */
#define UI_NEON_CYAN    0x22D3EE
#define UI_NEON_BLUE    0x2E7CF6
#define UI_BRIGHT_GREEN 0x39FF88
#define UI_WARN_YELLOW  0xFFC857
#define UI_WARN_ORANGE  0xFF8A3D
#define UI_WHITE        0xF5FAFF
#define UI_MUTED_BLUE   0x8AA0BC
#define UI_GLASS        0x0C182C
#define UI_GLASS_ALT    0x101F38
