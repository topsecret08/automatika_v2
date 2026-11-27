# Схема подключения модулей

Текстовая схема контроллера Automatika (ESP32-S3, headless + внешняя HMI по CAN/TWAI).
Источник истины — `main/config.h`; при расхождении правьте код и документацию.
Полная распиновка: [`pinout.md`](pinout.md). Новое расширение: [`SENSORS_ACTUATORS.md`](SENSORS_ACTUATORS.md), [`SCHEMATIC_MODULES.md`](SCHEMATIC_MODULES.md).

```
                         ┌──────────────────────────────────────────┐
                         │        ESP32-S3 Controller (headless)    │
I2C bus (GPIO8 SDA,      │                                          │
         GPIO9 SCL)      │  GPIO8/9  I2C ──┬─ ADS1115 #1 0x48       │
  ADS1115 #1 0x48 ────────┼─────────────────┼─ ADS1115 #2 0x49       │
  ADS1115 #2 0x49 ────────┼─────────────────┼─ MCP23017   0x21       │
  MCP23017   0x21 ────────┼─────────────────┼─ XGZP6897D  0x6D       │
  XGZP6897D  0x6D ────────┼─────────────────┼─ MCP4725    0x60       │
  MCP4725    0x60 ────────┼─────────────────┼─ BMP280     0x77       │
  BMP280     0x77 ────────┼─────────────────┼─ PCA9685    0x40       │
  PCA9685    0x40 ────────┼─────────────────┼─ Grove SSR  0x11       │
  Grove SSR  0x11 ────────┼─────────────────┼─ DS3231     0x68       │
  DS3231     0x68 ────────┼─────────────────┘                         │
                         │                                          │
DS18B20 1-Wire ◄─────────┼── GPIO4 (+4.7k to 3.3V)                  │
HX711 DOUT/SCK ◄─────────┼── GPIO5 / GPIO6                          │
E-STOP NC->GND ◄─────────┼── GPIO7 (pull-up, 100nF, active high)    │
Level float ◄────────────┼── GPIO40                                 │
Buzzer ◄─────────────────┼── GPIO42                                 │
SSR/MOSFET heat ◄────────┼── GPIO14 (REG_SSR_GPIO)                  │
Stepper STEP/EN/DIR ◄────┼── GPIO15 / GPIO16 / GPIO39               │
CAN HMI TX/RX/DE ◄─────┼── GPIO17 / GPIO18 / GPIO21               │
LSA PWM->0-10V ◄─────────┼── GPIO38                                 │
Solenoid PWM ◄───────────┼── GPIO41                                 │
Safety relay ◄───────────┼── GPIO47                                 │
Ext WDT DONE ◄───────────┼── GPIO48                                 │
                         └──────────────────────────────────────────┘

MCP23017 @0x21 (expansion module)
  GPA0..GPA7 -> 8-ch SSR module (heads/body/tails/stirrer/cooling/phlegm/boost3ph/spare), OFF by default
  GPB0 <- XKC-Y25-V diopter (5V->10k/22k divider+RC)
  GPB1 <- XKC-Y25-V receiver overflow/NBK empty
  GPB2 <- phlegm reed float

ADS1115 #2 @0x49
  A0 <- PT-506 4-20mA, 165Ω 0.1% shunt (jacket pressure)
  A1-A3 <- ZMPT101B phase RMS (monitoring; calibrate before control)

XGZP6897D @0x6D
  Cube pressure / choke detection (I2C)
```

## Примечания

- **E-STOP**: NC-цепь, активный высокий. Режим `SENSOR_ONLY_BENCH` позволяет запуск без физического E-STOP только для диагностики: датчики читаются, все выходы заблокированы.
- **Безопасное состояние**: SSR MCP23017 OFF, LSA PWM 0%, safety relay open, heater/doser/stepper off при E-STOP/bench/fault.
- **XKC-Y25-V**: питание 5V, выход через делитель 10k+22k + RC до 3.3V на MCP23017 GPB.
- **PT-506**: 4-20mA через 165Ω 0.1% в A0 ADS1115#2.
- **ZMPT101B**: RMS мониторинг ограничен скоростью ADS1115; перед контуром регулирования LSA нужна стендовая калибровка.
- **HMI panel GPIO15/16** не трогаются. CAN только GPIO17/18/21 на контроллере.
- Механический разрыв фазы E-STOP обязателен. Программная защёлка его не заменяет.
