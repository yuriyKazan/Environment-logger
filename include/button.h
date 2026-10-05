#pragma once
// Periph layer: push button on a GPIO (pressed level = config::BUTTON_PRESSED_LEVEL). An any-edge ISR applies quiet-time debounce
// (an edge counts as a press only after BUTTON_DEBOUNCE_US without any edge) and posts an
// IsrEvent{Button} (press) or IsrEvent{ButtonRelease} (release) to a queue. No polling, no delays.

#include <cstdint>
#include "config.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

class Button {
public:
    explicit Button(gpio_num_t pin) : pin_(pin) {}

    // Configure the pin (input, pull towards the idle level, both edges) and attach the ISR.
    // `queue` must hold IsrEvent items and outlive the button.
    esp_err_t init(QueueHandle_t queue);

    // Current level of the pin: true while the button is held down.
    bool is_pressed() const { return gpio_get_level(pin_) == config::BUTTON_PRESSED_LEVEL; }

    // Presses that could not be queued because the queue was full.
    uint32_t dropped() const { return dropped_; }

private:
    static void isr(void *arg);

    gpio_num_t pin_;
    QueueHandle_t queue_ = nullptr;
    volatile int64_t last_edge_us_ = 0;      // time of the previous edge, shared with the ISR
    volatile uint32_t dropped_ = 0;          // shared with the ISR
};
