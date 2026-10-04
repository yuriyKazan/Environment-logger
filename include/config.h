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
// ---- Registers used for the bring-up check ----
inline constexpr uint8_t BME280_REG_CHIP_ID = 0xD0;
inline constexpr uint8_t BME280_CHIP_ID = 0x60;
inline constexpr uint8_t DS3231_REG_SECONDS = 0x00;

inline constexpr uint8_t I2C_SCAN_FIRST = 0x08;
inline constexpr uint8_t I2C_SCAN_LAST = 0x77;
inline constexpr size_t I2C_SCAN_MAX_FOUND = 16;

}  // namespace config
