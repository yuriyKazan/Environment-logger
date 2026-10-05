#include "button.h"

#include "config.h"
#include "data_types.h"
#include "esp_attr.h"
#include "esp_timer.h"

esp_err_t Button::init(QueueHandle_t queue)
{
    if (queue == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    queue_ = queue;

    gpio_config_t cfg = {};
    cfg.pin_bit_mask = 1ULL << pin_;
    cfg.mode = GPIO_MODE_INPUT;
    // Pull towards the idle level: pressed high -> pull-down, pressed low -> pull-up.
    const bool pressed_high = config::BUTTON_PRESSED_LEVEL != 0;
    cfg.pull_up_en = pressed_high ? GPIO_PULLUP_DISABLE : GPIO_PULLUP_ENABLE;
    cfg.pull_down_en = pressed_high ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE;
    cfg.intr_type = GPIO_INTR_ANYEDGE;  // both edges: release bounce must restart the quiet time

    esp_err_t err = gpio_config(&cfg);
    if (err != ESP_OK) {
        return err;
    }
    // ESP_ERR_INVALID_STATE means the ISR service was already installed: fine.
    err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }
    return gpio_isr_handler_add(pin_, &Button::isr, this);
}

void IRAM_ATTR Button::isr(void *arg)
{
    auto *self = static_cast<Button *>(arg);
    const int64_t now = esp_timer_get_time();

    // Quiet-time debounce: every edge (press, release, bounce) restarts the timer. An event
    // is produced only if the line was quiet for BUTTON_DEBOUNCE_US before this edge, so
    // bounce after a press and bounce after a release are both ignored. (A press shorter than
    // the quiet time loses its release event; the long-press timer in T3 covers that case.)
    const int64_t quiet_us = now - self->last_edge_us_;
    self->last_edge_us_ = now;
    if (quiet_us < config::BUTTON_DEBOUNCE_US) {
        return;
    }
    // The line level after the quiet time tells which edge this is: pressed level = press, otherwise release.
    const bool pressed = gpio_get_level(self->pin_) == config::BUTTON_PRESSED_LEVEL;

    const IsrEvent ev = {pressed ? IsrEventType::Button : IsrEventType::ButtonRelease, now, 0};
    BaseType_t woken = pdFALSE;
    if (xQueueSendFromISR(self->queue_, &ev, &woken) != pdTRUE) {
        self->dropped_ = self->dropped_ + 1;
    }
    if (woken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}
