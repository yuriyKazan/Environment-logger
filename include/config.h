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

// ---- BME280 measurement ----
// Datasheet t_measure,max for T/P/H oversampling x1: 1.25 + 2.3 + (2.3 + 0.575) + (2.3 + 0.575) = 9.3 ms;
// rounded up with margin. The wait is a one-shot timer, never a blocking delay.
inline constexpr uint64_t BME280_CONVERSION_US = 10 * 1000;
inline constexpr uint32_t MEASURE_INTERVAL_MS = 5000;   // time between measurements

// ---- RTC ----
// Bring-up helper: if the DS3231 time is not valid at boot, set it to the firmware build time
// (local time of the build machine, treated as UTC). Leave false to test the invalid-time fallback.
inline constexpr bool RTC_SET_FROM_BUILD_TIME_IF_INVALID = false;

// ---- LEDs and button ----
inline constexpr gpio_num_t LED_OK_GPIO = GPIO_NUM_4;
inline constexpr gpio_num_t LED_ERR_GPIO = GPIO_NUM_5;
inline constexpr gpio_num_t BUTTON_GPIO = GPIO_NUM_6;       // active low, internal pull-up
inline constexpr int64_t BUTTON_DEBOUNCE_US = 50 * 1000;    // edges closer than this are ignored
inline constexpr uint32_t HEARTBEAT_LED_PERIOD_MS = 500;    // bring-up: LED_OK toggle period

// ---- Queues and tasks ----
inline constexpr size_t ISR_QUEUE_LEN = 8;                  // Q_ISR: timer/button ISR -> task
inline constexpr uint32_t BRINGUP_TASK_STACK = 3072;
inline constexpr unsigned BRINGUP_TASK_PRIO = 5;

}  // namespace config
