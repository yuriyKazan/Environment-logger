#include "led.h"

esp_err_t Led::init()
{
    gpio_config_t cfg = {};
    cfg.pin_bit_mask = 1ULL << pin_;
    cfg.mode = GPIO_MODE_OUTPUT;
    cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    cfg.intr_type = GPIO_INTR_DISABLE;

    esp_err_t err = gpio_config(&cfg);
    if (err != ESP_OK) {
        return err;
    }
    on_ = false;
    return gpio_set_level(pin_, 0);
}

void Led::set(bool on)
{
    if (gpio_set_level(pin_, on ? 1 : 0) == ESP_OK) {
        on_ = on;
    }
}
