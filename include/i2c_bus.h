#pragma once
// Periph layer: shared I2C master bus (one bus for BME280 and DS3231).

#include <cstddef>
#include <cstdint>
#include "driver/i2c_master.h"
#include "esp_err.h"

class I2cBus {
public:
    I2cBus() = default;
    ~I2cBus();
    I2cBus(const I2cBus &) = delete;
    I2cBus &operator=(const I2cBus &) = delete;

    // Create the bus. Returns ESP_ERR_INVALID_STATE if already created.
    esp_err_t init();

    // Handle for device drivers; nullptr until init() succeeded.
    i2c_master_bus_handle_t handle() const { return bus_; }

    // ESP_OK if a device ACKs at this 7-bit address, ESP_ERR_NOT_FOUND on NACK,
    // other codes on bus errors.
    esp_err_t probe(uint8_t addr) const;

    // Probe config::I2C_SCAN_FIRST..I2C_SCAN_LAST. Stores up to `max` addresses in
    // `found`; *count receives the total number of devices that answered.
    esp_err_t scan(uint8_t *found, size_t max, size_t *count) const;

    // Read `len` bytes starting at register `reg` at config::I2C_FREQ_HZ.
    // Bring-up helper: creates a short-lived device handle per call.
    esp_err_t read_reg(uint8_t addr, uint8_t reg, uint8_t *buf, size_t len) const;

private:
    i2c_master_bus_handle_t bus_ = nullptr;
};
