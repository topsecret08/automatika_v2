# Схемы модулей расширения

Логические схемы подключения новых модулей. Фактические номиналы/питание подтверждать по datasheet и стендовой проверке.

## 1. MCP23017 SSR/Inputs (I2C 0x21)

```
ESP32-S3 I2C (GPIO8 SDA, GPIO9 SCL, 3.3V pull-ups)
        │
        ├── MCP23017 @0x21
        │     ├─ GPA0..GPA7 -> 8-ch SSR module inputs (3.3V logic compatible)
        │     │    GPA0 heads, GPA1 body, GPA2 tails, GPA3 stirrer,
        │     │    GPA4 cooling, GPA5 phlegm pump, GPA6 boost 3ph SSR, GPA7 spare
        │     │    SSR outputs: AC/DC load per SSR module rating, separate power domain
        │     │
        │     └─ GPB0..GPB2 inputs (pull-up enabled)
        │          GPB0 <- XKC-Y25-V diopter (5V out -> 10k/22k divider + RC -> 3.3V)
        │          GPB1 <- XKC-Y25-V receiver overflow/NBK empty (divider + RC)
        │          GPB2 <- phlegm reed float (dry contact to GND, pull-up)

Safety: all GPA SSR outputs default OFF on boot, E-STOP, bench mode, or MCP init failure.
```

## 2. ADS1115 #2 (I2C 0x49)

```
ADS1115 #2 @0x49
  A0 <- PT-506 4-20mA through 165Ω 0.1% shunt to GND
        4mA = 0.66V, 20mA = 3.3V
        + wire-break detection in software (current < ~2mA)
  A1 <- ZMPT101B phase L1 conditioned AC RMS signal (offset/scale per module)
  A2 <- ZMPT101B phase L2
  A3 <- ZMPT101B phase L3

Note: ADS1115 sampling is limited; ZMPT RMS is monitoring-grade until bench-calibrated.
```

## 3. XGZP6897D pressure (I2C 0x6D)

```
XGZP6897D @0x6D (I2C)
  SDA/SCL on shared I2C bus
  VCC/GND per sensor variant
  Measurement command 0x30 0x0A, read 6 bytes from 0x06 (pressure+temp)

Use: cube pressure / choke detection (XGZP6897D_CHOKE_KPA).
Calibration required for specific sensor range/transfer function.
```

## 4. LSA-TH3P40Y control

```
ESP32 GPIO38 (LEDC PWM, 2kHz, 10-bit)
  -> PWM-to-0-5V/0-10V converter module (external)
  -> Loncont LSA-TH3P40Y control input

Safe state: PWM duty 0% => converter output 0V => regulator output off (verify on bench).
Do not apply ZMPT closed-loop control before RMS calibration.
```

## 5. Stepper direction interlock

```
ESP32 GPIO15 STEP
ESP32 GPIO16 EN (active low)
ESP32 GPIO39 DIR

One physical stepper shared by:
  - TAKEOFF (distillate takeoff direction)
  - NBK_FEED (feed mash into NBK direction)

Software interlock: direction change allowed only when doser idle.
E-STOP/bench blocks motion commands.
```

## 6. E-STOP / Bench mode

```
Physical NC E-STOP -> GPIO7, pull-up, active high (open/fault = active)
Default mode: REQUIRED_NC (fail-safe)

SENSOR_ONLY_BENCH (explicit config):
  - controller boots
  - sensors/telemetry/UI readable
  - all actuator outputs blocked
  - safety relay forced open, heater/PWM/SSR/stepper/doser off
  - reset refused while bench mode active
```