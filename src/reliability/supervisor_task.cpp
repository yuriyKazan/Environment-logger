#include "supervisor_task.h"

#include "config.h"
#include "esp_log.h"

static const char *TAG = "T3";

esp_err_t SupervisorTask::start(QueueHandle_t queue, QueueHandle_t sensor_queue)
{
    if (queue_ != nullptr || queue == nullptr || sensor_queue == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    queue_ = queue;
    sensor_queue_ = sensor_queue;

    esp_timer_create_args_t args = {};
    args.callback = &SupervisorTask::long_press_cb;
    args.arg = this;
    args.name = "long_press";
    esp_err_t err = esp_timer_create(&args, &long_press_timer_);
    if (err != ESP_OK) {
        return err;
    }
    return xTaskCreate(&SupervisorTask::task_entry, "T3_supervisor", config::SUPERVISOR_TASK_STACK, this,
                       config::SUPERVISOR_TASK_PRIO, nullptr) == pdPASS
               ? ESP_OK
               : ESP_ERR_NO_MEM;
}

void SupervisorTask::task_entry(void *arg)
{
    static_cast<SupervisorTask *>(arg)->run();
}

void SupervisorTask::long_press_cb(void *arg)
{
    auto *self = static_cast<SupervisorTask *>(arg);
    const IsrEvent ev = {IsrEventType::LongPressTimeout, esp_timer_get_time(), 0};
    xQueueSend(self->queue_, &ev, 0);
}

void SupervisorTask::run()
{
    for (;;) {
        IsrEvent ev;
        // Finite wait: the timeout is the heartbeat of this task (LED blink now, watchdog feed in step 4.3).
        if (xQueueReceive(queue_, &ev, pdMS_TO_TICKS(config::SUPERVISOR_TICK_MS)) != pdTRUE) {
            tick();
            continue;
        }
        switch (ev.type) {
        case IsrEventType::I2cFailure:       on_i2c_failure(ev); break;
        case IsrEventType::MeasurementOk:    on_measurement_ok(); break;
        case IsrEventType::Button:           on_button_press(); break;
        case IsrEventType::ButtonRelease:    on_button_release(); break;
        case IsrEventType::LongPressTimeout: on_long_press_timeout(); break;
        default:                             break;  // events meant for T1
        }
    }
}

void SupervisorTask::tick()
{
    if (!error_active_) {
        led_ok_.toggle();  // 1 Hz heartbeat: a blinking LED_OK shows that T3 is alive
    }
}

void SupervisorTask::update_leds()
{
    led_err_.set(error_active_);
    if (error_active_) {
        led_ok_.set(false);
    }
}

// T1 gave up after its retries: reset the bus. The mutex is taken with a finite timeout, and
// RecoveryDone is always sent, also when the reset fails, so T1 never waits for nothing.
void SupervisorTask::on_i2c_failure(const IsrEvent &ev)
{
    ESP_LOGE(TAG, "I2C failure (%s): resetting the bus", esp_err_to_name((esp_err_t)ev.value));
    error_active_ = true;
    update_leds();

    esp_err_t result;
    {
        I2cLockGuard lock(mutex_, config::I2C_MUTEX_TIMEOUT_MS);
        result = lock.locked() ? bus_.reset() : ESP_ERR_TIMEOUT;
    }
    bus_resets_++;
    ESP_LOGW(TAG, "bus reset #%u: %s", (unsigned)bus_resets_, esp_err_to_name(result));

    const IsrEvent done = {IsrEventType::RecoveryDone, esp_timer_get_time(), (int32_t)result};
    if (xQueueSend(sensor_queue_, &done, 0) != pdTRUE) {
        ESP_LOGE(TAG, "T1 queue full: RecoveryDone lost (T1 will time out)");
    }
}

void SupervisorTask::on_measurement_ok()
{
    if (error_active_) {
        ESP_LOGI(TAG, "sensor recovered: error state cleared");
    }
    error_active_ = false;
    update_leds();
}

// Button gestures. A press starts the long-press timer; a release before it expires is a short
// press. The timer handler checks the pin level too, because a very short press loses its release.
void SupervisorTask::on_button_press()
{
    if (press_active_) {
        return;
    }
    press_active_ = true;
    esp_timer_stop(long_press_timer_);
    esp_timer_start_once(long_press_timer_, (uint64_t)config::BUTTON_LONG_PRESS_MS * 1000);
}

void SupervisorTask::on_button_release()
{
    if (!press_active_) {
        return;  // the long press was already handled
    }
    press_active_ = false;
    esp_timer_stop(long_press_timer_);
    short_press_action();
}

void SupervisorTask::on_long_press_timeout()
{
    if (!press_active_) {
        return;
    }
    press_active_ = false;
    if (button_.is_pressed()) {
        long_press_action();
    } else {
        short_press_action();  // the button was released but its release event was filtered out
    }
}

void SupervisorTask::short_press_action()
{
    errors_.reset();
    ESP_LOGI(TAG, "button: error counter cleared");
}

void SupervisorTask::long_press_action()
{
    ESP_LOGW(TAG, "button: long press, watchdog test requested");
    const IsrEvent ev = {IsrEventType::TestHang, esp_timer_get_time(), 0};
    if (xQueueSend(sensor_queue_, &ev, 0) != pdTRUE) {
        ESP_LOGE(TAG, "T1 queue full: test request lost");
    }
}
