#include "i2c_bus.h"

#include "config.h"
#include "esp_log.h"

static const char *TAG = "i2c_bus";

I2cBus::~I2cBus()
{
    if (bus_ != nullptr) {
        i2c_del_master_bus(bus_);
    }
}

esp_err_t I2cBus::init()
{
    if (bus_ != nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    i2c_master_bus_config_t cfg = {};
    cfg.i2c_port = config::I2C_PORT;
    cfg.sda_io_num = config::I2C_SDA_GPIO;
    cfg.scl_io_num = config::I2C_SCL_GPIO;
    cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    cfg.glitch_ignore_cnt = config::I2C_GLITCH_FILTER;
    // Pull-ups are on the modules (see docs/hardware.md): internal ones stay off.
    cfg.flags.enable_internal_pullup = false;

    esp_err_t err = i2c_new_master_bus(&cfg, &bus_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_new_master_bus failed: %s", esp_err_to_name(err));
        bus_ = nullptr;
    }
    return err;
}

esp_err_t I2cBus::reset()
{
    if (bus_ == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    const esp_err_t err = i2c_master_bus_reset(bus_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_master_bus_reset failed: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t I2cBus::probe(uint8_t addr) const
{
    if (bus_ == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    return i2c_master_probe(bus_, addr, config::I2C_PROBE_TIMEOUT_MS);
}

esp_err_t I2cBus::read_reg(uint8_t addr, uint8_t reg, uint8_t *buf, size_t len) const
{
    if (bus_ == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    if (buf == nullptr || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    i2c_device_config_t dev_cfg = {};
    dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address = addr;
    dev_cfg.scl_speed_hz = config::I2C_FREQ_HZ;

    i2c_master_dev_handle_t dev = nullptr;
    esp_err_t err = i2c_master_bus_add_device(bus_, &dev_cfg, &dev);
    if (err != ESP_OK) {
        return err;
    }
    err = i2c_master_transmit_receive(dev, &reg, 1, buf, len, config::I2C_XFER_TIMEOUT_MS);
    i2c_master_bus_rm_device(dev);
    return err;
}

esp_err_t I2cBus::scan(uint8_t *found, size_t max, size_t *count) const
{
    if (found == nullptr || count == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    *count = 0;
    for (uint8_t addr = config::I2C_SCAN_FIRST; addr <= config::I2C_SCAN_LAST; addr++) {
        esp_err_t err = probe(addr);
        if (err == ESP_OK) {
            if (*count < max) {
                found[*count] = addr;
            }
            (*count)++;
        } else if (err != ESP_ERR_NOT_FOUND && err != ESP_ERR_TIMEOUT) {
            ESP_LOGE(TAG, "probe 0x%02X: %s", addr, esp_err_to_name(err));
            return err;
        }
    }
    return ESP_OK;
}
