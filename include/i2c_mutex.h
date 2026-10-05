#pragma once
// Reliability layer: one mutex for the whole I2C bus (BME280 and DS3231 share it).
//
// Rules (see docs/architecture.md):
//   - always taken with a finite timeout, never portMAX_DELAY
//   - always given back on every exit path: use I2cLockGuard
//   - hold it only during an I2C transaction, never across a conversion wait
//   - xSemaphoreCreateMutex() (not a binary semaphore) for priority inheritance

#include <cstdint>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

class I2cMutex {
public:
    I2cMutex() = default;
    ~I2cMutex();
    I2cMutex(const I2cMutex &) = delete;
    I2cMutex &operator=(const I2cMutex &) = delete;

    esp_err_t init();

    // ESP_OK if taken, ESP_ERR_TIMEOUT if the bus stayed busy.
    esp_err_t take(uint32_t timeout_ms);
    void give();

private:
    SemaphoreHandle_t handle_ = nullptr;
};

// RAII: takes the mutex in the constructor, gives it back in the destructor.
class I2cLockGuard {
public:
    I2cLockGuard(I2cMutex &mutex, uint32_t timeout_ms);
    ~I2cLockGuard();
    I2cLockGuard(const I2cLockGuard &) = delete;
    I2cLockGuard &operator=(const I2cLockGuard &) = delete;

    bool locked() const { return locked_; }

private:
    I2cMutex &mutex_;
    bool locked_;
};
