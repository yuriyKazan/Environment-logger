#pragma once
// Periph layer: a single active-high LED on a GPIO.

#include "driver/gpio.h"
#include "esp_err.h"

class Led {
public:
    explicit Led(gpio_num_t pin) : pin_(pin) {}

    // Configure the pin as output, LED off.
    esp_err_t init();
    void set(bool on);
    void toggle() { set(!on_); }
    bool is_on() const { return on_; }

private:
    gpio_num_t pin_;
    bool on_ = false;
};
