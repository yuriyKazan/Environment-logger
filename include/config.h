#pragma once
// Single place for all tunable constants (no magic numbers elsewhere).

#include <cstddef>
#include <cstdint>
#include "driver/gpio.h"
#include "driver/i2c_master.h"

namespace config {

inline constexpr const char *APP_NAME = "env-logger";

// ---- I2C bus ----
inline constexpr i2c_port_t I2C_PORT = I2C_NUM_0;
inline constexpr gpio_num_t I2C_SDA_GPIO = GPIO_NUM_8;
inline constexpr gpio_num_t I2C_SCL_GPIO = GPIO_NUM_9;
inline constexpr uint32_t I2C_FREQ_HZ = 400000;
inline constexpr uint8_t I2C_GLITCH_FILTER = 7;   // clock cycles, noise immunity
inline constexpr int I2C_PROBE_TIMEOUT_MS = 50;   // per-address probe timeout
inline constexpr int I2C_XFER_TIMEOUT_MS = 100;   // register read/write timeout
// Note: i2c_master_probe() always runs at 100 kHz (fixed by ESP-IDF);
// I2C_FREQ_HZ applies to device handles (register reads/writes).

// ---- I2C addresses ----
inline constexpr uint8_t I2C_ADDR_BME280 = 0x76;
inline constexpr uint8_t I2C_ADDR_DS3231 = 0x68;
inline constexpr uint8_t I2C_ADDR_AT24C32 = 0x50;  // EEPROM on the DS3231 module (unused)
// ---- Registers used for the bring-up check ----
inline constexpr uint8_t BME280_REG_CHIP_ID = 0xD0;
inline constexpr uint8_t BME280_CHIP_ID = 0x60;
inline constexpr uint8_t DS3231_REG_SECONDS = 0x00;

inline constexpr uint8_t I2C_SCAN_FIRST = 0x08;
inline constexpr uint8_t I2C_SCAN_LAST = 0x77;
inline constexpr size_t I2C_SCAN_MAX_FOUND = 16;

// ---- I2C error handling (T1 retries, T3 recovers) ----
inline constexpr uint8_t I2C_MAX_ATTEMPTS = 3;              // measurement attempts per cycle before a bus reset
inline constexpr uint32_t I2C_RETRY_DELAY_MS = 100;         // gap between attempts, a one-shot timer (never a delay)
inline constexpr uint32_t RECOVERY_TIMEOUT_MS = 2000;       // how long T1 waits for T3's bus reset

// ---- Supervisor (T3) and button gestures ----
inline constexpr uint32_t SUPERVISOR_TICK_MS = 500;         // wake-up period: LED_OK blink (1 Hz) and, later, WDT feed
inline constexpr uint32_t BUTTON_LONG_PRESS_MS = 3000;      // short press: clear the error counter; long: watchdog test

// ---- BME280 measurement ----
// Datasheet t_measure,max for T/P/H oversampling x1: 1.25 + 2.3 + (2.3 + 0.575) + (2.3 + 0.575) = 9.3 ms;
// rounded up with margin. The wait is a one-shot timer, never a blocking delay.
inline constexpr uint64_t BME280_CONVERSION_US = 10 * 1000;
inline constexpr uint32_t MEASURE_INTERVAL_MS = 5000;   // time between measurements

// ---- EMA filter ----
// alpha = 0.2 at a 5 s period gives a time constant of 22 s (see include/ema.h).
inline constexpr float EMA_ALPHA = 0.2f;

// ---- RTC ----
// Bring-up helper: if the DS3231 time is not valid at boot, set it to the firmware build time
// (local time of the build machine, treated as UTC). Leave false to test the invalid-time fallback.
inline constexpr bool RTC_SET_FROM_BUILD_TIME_IF_INVALID = false;

// ---- LEDs and button ----
inline constexpr gpio_num_t LED_OK_GPIO = GPIO_NUM_4;
inline constexpr gpio_num_t LED_ERR_GPIO = GPIO_NUM_5;
inline constexpr gpio_num_t BUTTON_GPIO = GPIO_NUM_6;
// Pin level while the button is held. This build: the button module reads low at rest and high when
// pressed (measured from the logs), so 1. A bare switch between the pin and GND would be 0.
inline constexpr int BUTTON_PRESSED_LEVEL = 1;
inline constexpr int64_t BUTTON_DEBOUNCE_US = 50 * 1000;    // edges closer than this are ignored

// ---- Queues, tasks, mutex ----
inline constexpr size_t ISR_QUEUE_LEN = 8;                  // Q_ISR: timer/button events -> T1
inline constexpr size_t LOG_QUEUE_LEN = 8;                  // Q_LOG: T1 -> T2
inline constexpr size_t ERR_QUEUE_LEN = 8;                  // T3 queue: errors and button events
inline constexpr size_t MQTT_QUEUE_LEN = 4;                 // Q_MQTT: T1 -> T4 (drops the oldest when full)
inline constexpr uint32_t SENSOR_TASK_STACK = 4096;
inline constexpr unsigned SENSOR_TASK_PRIO = 5;
inline constexpr uint32_t UART_LOG_TASK_STACK = 4096;
inline constexpr unsigned UART_LOG_TASK_PRIO = 4;
inline constexpr uint32_t SUPERVISOR_TASK_STACK = 4096;
inline constexpr unsigned SUPERVISOR_TASK_PRIO = 6;     // highest: must always be able to run the recovery
inline constexpr uint32_t MQTT_TASK_STACK = 4096;
inline constexpr unsigned MQTT_TASK_PRIO = 3;           // lowest: best effort, never competes with T1-T2
inline constexpr uint32_t I2C_MUTEX_TIMEOUT_MS = 200;       // never wait for the bus forever
inline constexpr uint32_t TASK_WAIT_MS = 1000;              // finite wait on queues (WDT feed point)
inline constexpr size_t LOG_LINE_MAX = 96;

// ---- Wi-Fi and MQTT (T4) ----
// Credentials and the broker URI live in include/secrets.h (git-ignored, template: secrets.h.example).
inline constexpr uint8_t WIFI_FAST_RETRIES = 5;             // immediate reconnect attempts after a drop
inline constexpr uint32_t WIFI_SLOW_RETRY_MS = 30 * 1000;   // then one attempt per 30 s, forever (one-shot timer)
// Public broker, no authentication: anyone can read or write these topics (see README, known limitations).
inline constexpr const char *MQTT_TOPIC_PREFIX = "envlogger/ykazan";
inline constexpr int MQTT_QOS = 0;                          // best effort, no retain
inline constexpr size_t MQTT_TOPIC_MAX = 48;
inline constexpr size_t MQTT_PAYLOAD_MAX = 24;

}  // namespace config
