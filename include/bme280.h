#pragma once
// Periph layer: BME280 temperature / humidity / pressure sensor over I2C.
//
// Forced mode, split in two non-blocking steps so the caller never needs a delay:
//   start_measurement()  -> wait config::BME280_CONVERSION_US (one-shot timer) -> read()
// Every method returns esp_err_t. The caller must hold the I2C bus mutex around each call.

#include <cstddef>
#include <cstdint>
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "i2c_bus.h"

struct Bme280Sample {
    float temp_c;
    float hum_pct;
    float press_hpa;
};

class Bme280 {
public:
    Bme280(const I2cBus &bus, uint8_t addr) : bus_(bus), addr_(addr) {}
    ~Bme280();
    Bme280(const Bme280 &) = delete;
    Bme280 &operator=(const Bme280 &) = delete;

    // Register the device on the bus, verify the chip ID, load calibration, set oversampling x1.
    esp_err_t init();

    // Trigger one forced-mode conversion.
    esp_err_t start_measurement();

    // Read and compensate the result of the last conversion.
    // ESP_ERR_INVALID_STATE: conversion still running or init() not done;
    // ESP_ERR_INVALID_RESPONSE: the sensor reports a skipped measurement.
    esp_err_t read(Bme280Sample &out);

private:
    struct Calib {
        uint16_t T1;
        int16_t T2, T3;
        uint16_t P1;
        int16_t P2, P3, P4, P5, P6, P7, P8, P9;
        uint8_t H1;
        int16_t H2;
        uint8_t H3;
        int16_t H4, H5;
        int8_t H6;
    };

    void release();
    esp_err_t read_regs(uint8_t reg, uint8_t *buf, size_t len);
    esp_err_t write_reg(uint8_t reg, uint8_t value);
    int32_t compensate_temperature(int32_t adc_t);
    uint32_t compensate_pressure(int32_t adc_p) const;
    uint32_t compensate_humidity(int32_t adc_h) const;

    const I2cBus &bus_;
    uint8_t addr_;
    i2c_master_dev_handle_t dev_ = nullptr;
    Calib calib_ = {};
    int32_t t_fine_ = 0;
};
