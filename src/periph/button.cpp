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
    cfg.intr_type = GPIO_INTR_NEGEDGE;

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
    if (now - self->last_accepted_us_ < config::BUTTON_DEBOUNCE_US) {
        return;  // contact bounce
    }
    self->last_accepted_us_ = now;

    const IsrEvent ev = {IsrEventType::Button, now};
    BaseType_t woken = pdFALSE;
    if (xQueueSendFromISR(self->queue_, &ev, &woken) != pdTRUE) {
        self->dropped_ = self->dropped_ + 1;
    }
    if (woken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}
