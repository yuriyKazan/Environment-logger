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
    cfg.pull_up_en = GPIO_PULLUP_ENABLE;
    cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
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
    // bounce after a press and bounce after a release are both ignored.
    const int64_t quiet_us = now - self->last_edge_us_;
    self->last_edge_us_ = now;
    if (quiet_us < config::BUTTON_DEBOUNCE_US) {
        return;
    }
    if (gpio_get_level(self->pin_) != 0) {
        return;  // line is high: this is a release, not a press
    }

    const IsrEvent ev = {IsrEventType::Button, now};
    BaseType_t woken = pdFALSE;
    if (xQueueSendFromISR(self->queue_, &ev, &woken) != pdTRUE) {
        self->dropped_ = self->dropped_ + 1;
    }
    if (woken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}
