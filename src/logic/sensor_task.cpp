#include "sensor_task.h"

#include "config.h"
#include "esp_log.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "sensor_validation.h"
#include "watchdog.h"

static const char *TAG = "T1";

SensorTask::SensorTask(I2cMutex &i2c_mutex, Bme280 &bme, Ds3231 &rtc, TimeSource &time_source, ErrorCounter &errors)
    : i2c_mutex_(i2c_mutex),
      bme_(bme),
      rtc_(rtc),
      time_(time_source),
      errors_(errors),
      ema_t_(config::EMA_ALPHA),
      ema_h_(config::EMA_ALPHA),
      ema_p_(config::EMA_ALPHA)
{
}

esp_err_t SensorTask::start(QueueHandle_t log_queue, QueueHandle_t mqtt_queue, QueueHandle_t supervisor_queue)
{
    if (queue_ != nullptr || log_queue == nullptr || mqtt_queue == nullptr || supervisor_queue == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    log_queue_ = log_queue;
    mqtt_queue_ = mqtt_queue;
    supervisor_queue_ = supervisor_queue;
    queue_ = xQueueCreate(config::ISR_QUEUE_LEN, sizeof(IsrEvent));
    if (queue_ == nullptr) {
        return ESP_ERR_NO_MEM;
    }

    struct TimerSpec {
        esp_timer_handle_t *handle;
        esp_timer_cb_t cb;
        const char *name;
    };
    const TimerSpec specs[] = {
        {&tick_timer_, &SensorTask::tick_cb, "tick"},
        {&conv_timer_, &SensorTask::conversion_cb, "bme_conv"},
        {&retry_timer_, &SensorTask::retry_cb, "i2c_retry"},
        {&recovery_timer_, &SensorTask::recovery_timeout_cb, "recovery_to"},
    };
    for (const TimerSpec &spec : specs) {
        esp_timer_create_args_t args = {};
        args.callback = spec.cb;
        args.arg = this;
        args.name = spec.name;
        const esp_err_t err = esp_timer_create(&args, spec.handle);
        if (err != ESP_OK) {
            return err;
        }
    }

    if (xTaskCreate(&SensorTask::task_entry, "T1_sensor", config::SENSOR_TASK_STACK, this, config::SENSOR_TASK_PRIO,
                    nullptr) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return esp_timer_start_periodic(tick_timer_, (uint64_t)config::MEASURE_INTERVAL_MS * 1000);
}

void SensorTask::task_entry(void *arg)
{
    static_cast<SensorTask *>(arg)->run();
}

// esp_timer callbacks only post events; all work happens in the task.
void SensorTask::tick_cb(void *arg) { static_cast<SensorTask *>(arg)->post(IsrEventType::Tick); }
void SensorTask::conversion_cb(void *arg) { static_cast<SensorTask *>(arg)->post(IsrEventType::ConversionDone); }
void SensorTask::retry_cb(void *arg) { static_cast<SensorTask *>(arg)->post(IsrEventType::Retry); }
void SensorTask::recovery_timeout_cb(void *arg) { static_cast<SensorTask *>(arg)->post(IsrEventType::RecoveryTimeout); }

void SensorTask::post(IsrEventType type)
{
    const IsrEvent ev = {type, esp_timer_get_time(), 0};
    if (xQueueSend(queue_, &ev, 0) != pdTRUE) {
        dropped_events_++;  // a timer event is lost only if T1 is badly behind; the next tick starts over
        ESP_LOGW(TAG, "event queue full, timer event dropped (total %u)", (unsigned)dropped_events_);
    }
}

void SensorTask::notify_supervisor(IsrEventType type, int32_t value)
{
    const IsrEvent ev = {type, esp_timer_get_time(), value};
    if (xQueueSend(supervisor_queue_, &ev, 0) != pdTRUE) {
        ESP_LOGW(TAG, "supervisor queue full, event dropped");
    }
}

void SensorTask::run()
{
    wdt_subscribe_current_task("T1");
    for (;;) {
        IsrEvent ev;
        // Finite wait, so the loop (and the watchdog feed) runs at least once per TASK_WAIT_MS.
        const bool got_event = xQueueReceive(queue_, &ev, pdMS_TO_TICKS(config::TASK_WAIT_MS)) == pdTRUE;
        wdt_feed();
        if (!got_event) {
            continue;
        }
        switch (ev.type) {
        case IsrEventType::Tick:            on_tick(); break;
        case IsrEventType::ConversionDone:  on_conversion_done(); break;
        case IsrEventType::Retry:           on_retry(); break;
        case IsrEventType::RecoveryDone:    on_recovery_done(ev); break;
        case IsrEventType::RecoveryTimeout: on_recovery_timeout(); break;
        case IsrEventType::TestHang:        test_hang(); break;
        default:                            break;  // events meant for T3
        }
    }
}

void SensorTask::on_tick()
{
    if (state_ != State::Idle) {
        ESP_LOGW(TAG, "tick while the previous cycle is still running: skipped");
        return;
    }
    attempt_ = 0;
    cycle_start_us_ = esp_timer_get_time();
    begin_attempt();
}

void SensorTask::begin_attempt()
{
    attempt_++;
    esp_err_t err;
    const int64_t t0 = esp_timer_get_time();
    {
        I2cLockGuard lock(i2c_mutex_, config::I2C_MUTEX_TIMEOUT_MS);
        err = lock.locked() ? bme_.start_measurement() : ESP_ERR_TIMEOUT;
    }
    if (err == ESP_OK) {
        start_stats_.add((uint32_t)(esp_timer_get_time() - t0));
    }
    if (err != ESP_OK) {
        attempt_failed(err, "BME280 start");
        return;
    }
    // Wait for the conversion with a one-shot timer instead of a delay.
    err = esp_timer_start_once(conv_timer_, config::BME280_CONVERSION_US);
    if (err != ESP_OK) {
        attempt_failed(err, "conversion timer");
        return;
    }
    state_ = State::WaitConversion;
}

void SensorTask::on_conversion_done()
{
    if (state_ != State::WaitConversion) {
        return;
    }

    Bme280Sample sample = {};
    Ds3231Reading rtc = {};
    esp_err_t bme_err;
    esp_err_t rtc_err = ESP_ERR_INVALID_STATE;
    const int64_t t0 = esp_timer_get_time();
    {
        I2cLockGuard lock(i2c_mutex_, config::I2C_MUTEX_TIMEOUT_MS);
        if (lock.locked()) {
            bme_err = bme_.read(sample);
            if (bme_err == ESP_OK) {
                rtc_err = rtc_.read(rtc);
            }
        } else {
            bme_err = ESP_ERR_TIMEOUT;
        }
    }
    const int64_t t1 = esp_timer_get_time();
    if (bme_err != ESP_OK) {
        attempt_failed(bme_err, "BME280 read");
        return;
    }
    // A well-formed but impossible sample (glitch, failed compensation) must not reach the filter:
    // it counts as a failed attempt, like a bus error.
    const SampleStatus status = validate_sample(sample.temp_c, sample.hum_pct, sample.press_hpa);
    if (status != SampleStatus::Ok) {
        ESP_LOGW(TAG, "BME280 sample rejected: %s (T=%.1f H=%.1f P=%.1f)", sample_status_name(status),
                 (double)sample.temp_c, (double)sample.hum_pct, (double)sample.press_hpa);
        attempt_failed(ESP_ERR_INVALID_RESPONSE, "BME280 data");
        return;
    }

    read_stats_.add((uint32_t)(t1 - t0));

    // Success: filter, timestamp, publish.
    state_ = State::Idle;
    bool trusted = false;
    LogEntry entry = {};
    entry.ts = time_.resolve(rtc_err, rtc, &trusted);
    entry.flags = LOG_FLAG_SENSOR_VALID | (trusted ? LOG_FLAG_TIME_TRUSTED : 0);
    entry.temp_c = ema_t_.update(sample.temp_c);
    entry.hum_pct = ema_h_.update(sample.hum_pct);
    entry.press_hpa = ema_p_.update(sample.press_hpa);
    entry.err_cnt = errors_.value();
    if (attempt_ > 1) {
        ESP_LOGI(TAG, "measurement succeeded on attempt %u", (unsigned)attempt_);
    }
    if (in_error_) {
        in_error_ = false;
        ESP_LOGI(TAG, "sensor is back");
        notify_supervisor(IsrEventType::MeasurementOk, 0);
    }
    publish(entry);
    record_cycle_done();
}

// Timing of a successful cycle: log min/avg/max once a minute and start over.
void SensorTask::record_cycle_done()
{
    cycle_stats_.add((uint32_t)(esp_timer_get_time() - cycle_start_us_));
    if (++stats_cycles_ < config::DIAG_T1_STATS_CYCLES) {
        return;
    }
    if (config::DIAG_ENABLED) {
        ESP_LOGI(TAG, "timing over %u cycles, us min/avg/max: I2C start %u/%u/%u, I2C read %u/%u/%u, cycle %u/%u/%u",
                 (unsigned)cycle_stats_.count(), (unsigned)start_stats_.min(), (unsigned)start_stats_.avg(),
                 (unsigned)start_stats_.max(), (unsigned)read_stats_.min(), (unsigned)read_stats_.avg(),
                 (unsigned)read_stats_.max(), (unsigned)cycle_stats_.min(), (unsigned)cycle_stats_.avg(),
                 (unsigned)cycle_stats_.max());
    }
    start_stats_.reset();
    read_stats_.reset();
    cycle_stats_.reset();
    stats_cycles_ = 0;
}

// One attempt failed: retry (bounded, after a timer) or, when the attempts are used up, ask T3 to reset the bus.
void SensorTask::attempt_failed(esp_err_t err, const char *what)
{
    ESP_LOGW(TAG, "%s failed: %s (attempt %u/%u)", what, esp_err_to_name(err), (unsigned)attempt_,
             (unsigned)config::I2C_MAX_ATTEMPTS);
    if (attempt_ < config::I2C_MAX_ATTEMPTS) {
        state_ = State::WaitRetry;
        if (esp_timer_start_once(retry_timer_, (uint64_t)config::I2C_RETRY_DELAY_MS * 1000) != ESP_OK) {
            begin_recovery(err);
        }
        return;
    }
    begin_recovery(err);
}

void SensorTask::on_retry()
{
    if (state_ == State::WaitRetry) {
        begin_attempt();
    }
}

void SensorTask::begin_recovery(esp_err_t err)
{
    ESP_LOGE(TAG, "I2C failed %u times in a row: requesting a bus reset", (unsigned)config::I2C_MAX_ATTEMPTS);
    state_ = State::Recovery;
    notify_supervisor(IsrEventType::I2cFailure, (int32_t)err);
    // If T3 never answers, T1 must not wait forever.
    if (esp_timer_start_once(recovery_timer_, (uint64_t)config::RECOVERY_TIMEOUT_MS * 1000) != ESP_OK) {
        finish_failed_cycle();
    }
}

void SensorTask::on_recovery_done(const IsrEvent &ev)
{
    if (state_ != State::Recovery) {
        return;
    }
    (void)esp_timer_stop(recovery_timer_);  // ESP_ERR_INVALID_STATE only means it had already fired
    if (ev.value == ESP_OK) {
        ESP_LOGI(TAG, "bus reset done, the next cycle will try again");
    } else {
        ESP_LOGE(TAG, "bus reset failed: %s", esp_err_to_name((esp_err_t)ev.value));
    }
    finish_failed_cycle();
}

void SensorTask::on_recovery_timeout()
{
    if (state_ != State::Recovery) {
        return;
    }
    ESP_LOGE(TAG, "no answer from the supervisor within %u ms", (unsigned)config::RECOVERY_TIMEOUT_MS);
    finish_failed_cycle();
}

// The cycle is lost, but it is still logged (with the error flag and the new error count).
void SensorTask::finish_failed_cycle()
{
    state_ = State::Idle;
    in_error_ = true;

    LogEntry entry = {};
    entry.ts = time(nullptr);  // no RTC reading in this cycle: internal clock, not trusted
    entry.temp_c = ema_t_.value();
    entry.hum_pct = ema_h_.value();
    entry.press_hpa = ema_p_.value();
    entry.err_cnt = errors_.increment();
    publish(entry);  // flags = 0: sensor not valid, time not trusted
}

// Deliberate hang for the watchdog demonstration (long button press): T1 blocks on a semaphore that
// is never given, like a deadlock, and so stops feeding the watchdog. This is the only unbounded
// wait in the code and it exists only for this test.
void SensorTask::test_hang()
{
    ESP_LOGE(TAG, "TEST: hanging on purpose, the watchdog should reset the chip");
    SemaphoreHandle_t never_given = xSemaphoreCreateBinary();
    xSemaphoreTake(never_given, portMAX_DELAY);
}

void SensorTask::publish(const LogEntry &entry)
{
    // Never block T1 on a slow consumer: count the loss instead.
    if (xQueueSend(log_queue_, &entry, 0) != pdTRUE) {
        dropped_log_++;
        ESP_LOGW(TAG, "Q_LOG full, entry dropped (total %u)", (unsigned)dropped_log_);
    }

    // Q_MQTT is best effort: when T4 is slow or offline, drop the OLDEST entry so the broker
    // gets the freshest data. T1 is the only producer, so removing one item makes room.
    if (xQueueSend(mqtt_queue_, &entry, 0) != pdTRUE) {
        LogEntry oldest;
        (void)xQueueReceive(mqtt_queue_, &oldest, 0);  // if T4 emptied the queue meanwhile, there is room anyway
        dropped_mqtt_++;
        if (xQueueSend(mqtt_queue_, &entry, 0) != pdTRUE) {
            ESP_LOGW(TAG, "Q_MQTT still full, entry dropped");
        }
    }
}
