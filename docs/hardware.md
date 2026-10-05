# Hardware: peripherals and GPIO map

Target: ESP32-S3 (`esp32-s3-devkitm-1`), ESP-IDF 5.5.3.

## Peripherals

| Peripheral | Role | Interface | Address / config |
|---|---|---|---|
| BME280 | Temperature, humidity, pressure | I2C | `0x76` |
| DS3231 RTC | Timestamp source | I2C (same bus) | `0x68` |
| UART0 | Formatted log output | UART | 115200 8N1 |
| LED_OK | System healthy indicator | GPIO out | via 220 Ω |
| LED_ERR | Error / recovery indicator | GPIO out | via 220 Ω |
| Button | Short press: clear the error counter. Long press (3 s): watchdog test | GPIO in + interrupt | 3-pin module (VCC, GND, OUT) with an on-board pull-down, pressed = high |
| Timer | Measurement tick | `esp_timer` / `gptimer` | 5 s period |
| Task WDT | Hang detection | `esp_task_wdt` | see [architecture](architecture.md) |

No analog input: the photoresistor was intentionally dropped, because a temperature/humidity/pressure logger has no organic need for it (see [decisions](decisions.md)).

## GPIO map

| Signal | GPIO | Notes |
|---|---|---|
| I2C SDA | 8 | 400 kHz, shared by BME280 and DS3231 |
| I2C SCL | 9 | pull-ups are on the modules, no external resistors (see wiring notes) |
| UART0 TX | 43 | console / log output, 115200 baud |
| UART0 RX | 44 | default console RX (unused by the application) |
| LED_OK | 4 | active high, 220 Ω series resistor |
| LED_ERR | 5 | active high, 220 Ω series resistor |
| Button | 6 | module OUT pin; high while pressed, internal pull-down, any-edge interrupt with quiet-time debounce |

## Wiring

```
ESP32-S3                BME280 (0x76)         DS3231 (0x68)
3V3  ------------------ VCC ----------------- VCC
GND  ------------------ GND ----------------- GND
GPIO8 (SDA) ------------ SDA ----------------- SDA
GPIO9 (SCL) ------------ SCL ----------------- SCL

No external I2C pull-ups: they are already on the modules
(BME280: 10 kΩ per line, DS3231: 4.7 kΩ per line).

GPIO4 --[220R]--|>|-- GND   (LED_OK)
GPIO5 --[220R]--|>|-- GND   (LED_ERR)
GPIO6 ------------------ OUT  (button module: VCC to 3V3, GND to GND, pressed = high)
```

Notes:
- DS3231 needs a working backup battery. With a dead or missing battery it reports `2000-01-01 00:00:00`; the firmware must validate the timestamp before logging it.
- I2C pull-ups are provided by the modules (BME280 10 kΩ, DS3231 4.7 kΩ per line), giving about 3.2 kΩ per line in parallel, which is suitable for 400 kHz with short wires. Add external 4.7-10 kΩ pull-ups only if a different module set has none; do not stack extra pull-ups blindly. If 400 kHz is unstable, shorten the wires first, then fall back to 100 kHz.
- The DS3231 module has a battery charging circuit, so it is fitted with an LIR2032 (a plain CR2032 should not be used with it).
- Schematic: KiCad source in `hardware/kicad/`, exported to `docs/schematic/schematic.pdf` and `schematic.png`.
