#pragma once
// Periph layer: active-low button on a GPIO. A falling-edge ISR applies a time-window
// debounce and posts an IsrEvent{Button} to a queue. No polling, no delays.

#include <cstdint>
#include "driver/gpio.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

class Button {
public:
    explicit Button(gpio_num_t pin) : pin_(pin) {}

    // Configure the pin (input, pull-up, falling edge) and attach the ISR.
    // `queue` must hold IsrEvent items and outlive the button.
    esp_err_t init(QueueHandle_t queue);

    // Presses that could not be queued because the queue was full.
    uint32_t dropped() const { return dropped_; }

private:
    static void isr(void *arg);

    gpio_num_t pin_;
    QueueHandle_t queue_ = nullptr;
    volatile int64_t last_accepted_us_ = 0;  // shared with the ISR
    volatile uint32_t dropped_ = 0;          // shared with the ISR
};
