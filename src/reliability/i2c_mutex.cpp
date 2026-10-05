#include "i2c_mutex.h"

I2cMutex::~I2cMutex()
{
    if (handle_ != nullptr) {
        vSemaphoreDelete(handle_);
    }
}

esp_err_t I2cMutex::init()
{
    if (handle_ != nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    handle_ = xSemaphoreCreateMutex();
    return handle_ != nullptr ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t I2cMutex::take(uint32_t timeout_ms)
{
    if (handle_ == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    return xSemaphoreTake(handle_, pdMS_TO_TICKS(timeout_ms)) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

void I2cMutex::give()
{
    if (handle_ != nullptr) {
        xSemaphoreGive(handle_);
    }
}

I2cLockGuard::I2cLockGuard(I2cMutex &mutex, uint32_t timeout_ms) : mutex_(mutex), locked_(mutex.take(timeout_ms) == ESP_OK) {}

I2cLockGuard::~I2cLockGuard()
{
    if (locked_) {
        mutex_.give();
    }
}
