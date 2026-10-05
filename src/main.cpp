#include "esp_log.h"
#include "esp_system.h"

#include "config.h"
#include "i2c_bus.h"

static const char *TAG = config::APP_NAME;

static I2cBus g_i2c;

static const char *device_name(uint8_t addr)
{
    switch (addr) {
    case config::I2C_ADDR_BME280: return "BME280";
    case config::I2C_ADDR_DS3231: return "DS3231";
    case config::I2C_ADDR_AT24C32: return "AT24C32 EEPROM on DS3231 module";
    default:                      return "unknown";
    }
}

static bool was_found(const uint8_t *found, size_t n, uint8_t addr)
{
    for (size_t i = 0; i < n; i++) {
        if (found[i] == addr) {
            return true;
        }
    }
    return false;
}

// Phase 2.2: I2C scanner. Replaced by the task-based design in Phase 3.
extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "boot, IDF %s", esp_get_idf_version());

    if (g_i2c.init() != ESP_OK) {
        ESP_LOGE(TAG, "I2C bus init failed");
        return;
    }

    uint8_t found[config::I2C_SCAN_MAX_FOUND];
    size_t count = 0;
    if (g_i2c.scan(found, config::I2C_SCAN_MAX_FOUND, &count) != ESP_OK) {
        ESP_LOGE(TAG, "I2C scan failed");
        return;
    }

    const size_t shown = count < config::I2C_SCAN_MAX_FOUND ? count : config::I2C_SCAN_MAX_FOUND;
    ESP_LOGI(TAG, "I2C scan (probe @ 100 kHz): %u device(s)", (unsigned)count);
    for (size_t i = 0; i < shown; i++) {
        ESP_LOGI(TAG, "  0x%02X (%s)", found[i], device_name(found[i]));
    }

    const uint8_t expected[] = {config::I2C_ADDR_BME280, config::I2C_ADDR_DS3231};
    for (uint8_t addr : expected) {
        if (was_found(found, shown, addr)) {
            ESP_LOGI(TAG, "0x%02X %s: OK", addr, device_name(addr));
        } else {
            ESP_LOGE(TAG, "0x%02X %s: NOT FOUND (check wiring/power)", addr, device_name(addr));
        }
    }

    // Register reads at the configured bus speed prove the 400 kHz setup works.
    ESP_LOGI(TAG, "register reads @ %u Hz:", (unsigned)config::I2C_FREQ_HZ);
    uint8_t id = 0;
    esp_err_t err = g_i2c.read_reg(config::I2C_ADDR_BME280, config::BME280_REG_CHIP_ID, &id, 1);
    if (err == ESP_OK && id == config::BME280_CHIP_ID) {
        ESP_LOGI(TAG, "  BME280 chip ID 0x%02X: OK", id);
    } else {
        ESP_LOGE(TAG, "  BME280 chip ID read failed (%s, id=0x%02X, expected 0x%02X)",
                 esp_err_to_name(err), id, config::BME280_CHIP_ID);
    }
    uint8_t sec = 0;
    err = g_i2c.read_reg(config::I2C_ADDR_DS3231, config::DS3231_REG_SECONDS, &sec, 1);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "  DS3231 seconds register 0x%02X: OK", sec);
    } else {
        ESP_LOGE(TAG, "  DS3231 read failed (%s)", esp_err_to_name(err));
    }
}
