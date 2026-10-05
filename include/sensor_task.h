#pragma once
// T1: the measurement task (Periph + Logic). Consumes Q_ISR (timer and button events) and runs
// the measurement state machine without any blocking delay:
//
//   Idle --Tick--> start BME280 conversion, arm one-shot timer --> WaitConversion
//   WaitConversion --ConversionDone--> read BME280 + DS3231, EMA, build LogEntry --> Idle
//
// Every I2C transaction is made under the I2C mutex with a finite timeout; the mutex is not held
// while waiting for the conversion. The result goes to Q_LOG as a LogEntry.
// Error recovery (retries, bus reset, supervisor) arrives with Phase 4; for now a failed
// cycle increments err_cnt and produces an entry flagged as having no valid sensor data.

#include <cstdint>
#include "bme280.h"
#include "data_types.h"
#include "ds3231.h"
#include "ema.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "i2c_mutex.h"
#include "time_source.h"

class SensorTask {
public:
    SensorTask(I2cMutex &i2c_mutex, Bme280 &bme, Ds3231 &rtc, TimeSource &time_source);

    // Create Q_ISR, the timers and the task; every LogEntry goes to `log_queue` (Q_LOG, T2) and
    // `mqtt_queue` (Q_MQTT, T4). Call button.init(isr_queue()) afterwards.
    esp_err_t start(QueueHandle_t log_queue, QueueHandle_t mqtt_queue);

    QueueHandle_t isr_queue() const { return isr_queue_; }
    uint32_t dropped_log_entries() const { return dropped_log_; }
    uint32_t dropped_mqtt_entries() const { return dropped_mqtt_; }

private:
    enum class State : uint8_t { Idle, WaitConversion };

    static void task_entry(void *arg);
    static void tick_cb(void *arg);
    static void conversion_cb(void *arg);

    void run();
    void post(IsrEventType type);
    void on_tick();
    void on_conversion_done();
    void on_button(const IsrEvent &ev);
    void fail(esp_err_t err, const char *what);
    void publish(const LogEntry &entry);

    I2cMutex &i2c_mutex_;
    Bme280 &bme_;
    Ds3231 &rtc_;
    TimeSource &time_;
    QueueHandle_t log_queue_ = nullptr;
    QueueHandle_t mqtt_queue_ = nullptr;
    QueueHandle_t isr_queue_ = nullptr;
    esp_timer_handle_t tick_timer_ = nullptr;
    esp_timer_handle_t conv_timer_ = nullptr;

    State state_ = State::Idle;
    Ema ema_t_;
    Ema ema_h_;
    Ema ema_p_;
    uint32_t err_cnt_ = 0;
    uint32_t dropped_log_ = 0;
    uint32_t dropped_mqtt_ = 0;
    uint32_t presses_ = 0;
};
