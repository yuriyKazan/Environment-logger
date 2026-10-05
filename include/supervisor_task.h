#pragma once
// T3: the supervisor (Reliability layer). It reacts to errors and owns the status LEDs and the
// button gestures.
//
//   I2cFailure from T1     -> LED_ERR on, reset the I2C bus under the mutex, answer RecoveryDone
//   MeasurementOk from T1  -> the sensor is back: LED_ERR off, LED_OK blinks again
//   short button press     -> clear the error counter
//   long button press      -> watchdog test: ask T1 to hang on purpose
//
// Healthy: LED_OK blinks at 1 Hz (every SUPERVISOR_TICK_MS it toggles). Error: LED_ERR is on and
// LED_OK is off. Its queue wait has a finite timeout, so the loop also runs when nothing happens.

#include <cstdint>
#include "button.h"
#include "data_types.h"
#include "error_counter.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "i2c_bus.h"
#include "i2c_mutex.h"
#include "led.h"

class SupervisorTask {
public:
    SupervisorTask(I2cBus &bus, I2cMutex &mutex, Led &led_ok, Led &led_err, Button &button, ErrorCounter &errors)
        : bus_(bus), mutex_(mutex), led_ok_(led_ok), led_err_(led_err), button_(button), errors_(errors)
    {
    }

    // `queue` is T3's input queue (errors from T1, button events); `sensor_queue` is T1's queue.
    esp_err_t start(QueueHandle_t queue, QueueHandle_t sensor_queue);

    uint32_t bus_resets() const { return bus_resets_; }

private:
    static void task_entry(void *arg);
    static void long_press_cb(void *arg);

    void run();
    void tick();
    void on_i2c_failure(const IsrEvent &ev);
    void on_measurement_ok();
    void on_button_press();
    void on_button_release();
    void on_long_press_timeout();
    void short_press_action();
    void long_press_action();
    void update_leds();

    I2cBus &bus_;
    I2cMutex &mutex_;
    Led &led_ok_;
    Led &led_err_;
    Button &button_;
    ErrorCounter &errors_;
    QueueHandle_t queue_ = nullptr;
    QueueHandle_t sensor_queue_ = nullptr;
    esp_timer_handle_t long_press_timer_ = nullptr;

    bool error_active_ = false;
    bool press_active_ = false;
    uint32_t bus_resets_ = 0;
};
