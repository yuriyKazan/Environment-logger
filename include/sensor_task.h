#pragma once
// T1: the measurement task (Periph + Logic). Consumes its event queue and runs the measurement
// state machine without any blocking delay (every wait is a one-shot timer or a queue wait):
//
//   Idle --Tick--> start the BME280 conversion --> WaitConversion --ConversionDone--> read
//     read ok              : EMA, LogEntry --> Idle
//     start/read failed    : up to I2C_MAX_ATTEMPTS attempts, I2C_RETRY_DELAY_MS apart (WaitRetry)
//     attempts exhausted   : ask T3 for a bus reset (I2cFailure) --> Recovery
//   Recovery --RecoveryDone / RecoveryTimeout--> the cycle is logged as failed, err_cnt++ --> Idle
//
// Every I2C transaction runs under the I2C mutex with a finite timeout; the mutex is never held
// while waiting for a conversion, a retry or the recovery. Each cycle produces one LogEntry for
// Q_LOG (T2) and Q_MQTT (T4), also a failed one, so the log never goes silent.

#include <cstdint>
#include "bme280.h"
#include "data_types.h"
#include "ds3231.h"
#include "ema.h"
#include "error_counter.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "i2c_mutex.h"
#include "time_source.h"

class SensorTask {
public:
    SensorTask(I2cMutex &i2c_mutex, Bme280 &bme, Ds3231 &rtc, TimeSource &time_source, ErrorCounter &errors);

    // Create T1's queue, the timers and the task. Every LogEntry goes to `log_queue` (Q_LOG, T2) and
    // `mqtt_queue` (Q_MQTT, T4); error events go to `supervisor_queue` (T3).
    esp_err_t start(QueueHandle_t log_queue, QueueHandle_t mqtt_queue, QueueHandle_t supervisor_queue);

    // T1's own event queue: T3 posts RecoveryDone and TestHang here.
    QueueHandle_t queue() const { return queue_; }
    uint32_t dropped_log_entries() const { return dropped_log_; }
    uint32_t dropped_mqtt_entries() const { return dropped_mqtt_; }

private:
    enum class State : uint8_t { Idle, WaitConversion, WaitRetry, Recovery };

    static void task_entry(void *arg);
    static void tick_cb(void *arg);
    static void conversion_cb(void *arg);
    static void retry_cb(void *arg);
    static void recovery_timeout_cb(void *arg);

    void run();
    void post(IsrEventType type);
    void on_tick();
    void begin_attempt();
    void on_conversion_done();
    void attempt_failed(esp_err_t err, const char *what);
    void on_retry();
    void begin_recovery(esp_err_t err);
    void on_recovery_done(const IsrEvent &ev);
    void on_recovery_timeout();
    void finish_failed_cycle();
    void test_hang();
    void publish(const LogEntry &entry);
    void notify_supervisor(IsrEventType type, int32_t value);

    I2cMutex &i2c_mutex_;
    Bme280 &bme_;
    Ds3231 &rtc_;
    TimeSource &time_;
    ErrorCounter &errors_;
    QueueHandle_t log_queue_ = nullptr;
    QueueHandle_t mqtt_queue_ = nullptr;
    QueueHandle_t supervisor_queue_ = nullptr;
    QueueHandle_t queue_ = nullptr;
    esp_timer_handle_t tick_timer_ = nullptr;
    esp_timer_handle_t conv_timer_ = nullptr;
    esp_timer_handle_t retry_timer_ = nullptr;
    esp_timer_handle_t recovery_timer_ = nullptr;

    State state_ = State::Idle;
    uint8_t attempt_ = 0;
    bool in_error_ = false;  // the previous cycle failed: tell T3 when a measurement succeeds again
    Ema ema_t_;
    Ema ema_h_;
    Ema ema_p_;
    uint32_t dropped_log_ = 0;
    uint32_t dropped_mqtt_ = 0;
};
