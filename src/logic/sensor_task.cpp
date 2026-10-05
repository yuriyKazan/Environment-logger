#include "sensor_task.h"

#include "config.h"
#include "esp_log.h"
#include "freertos/task.h"

static const char *TAG = "T1";

SensorTask::SensorTask(I2cMutex &i2c_mutex, Bme280 &bme, Ds3231 &rtc, TimeSource &time_source)
    : i2c_mutex_(i2c_mutex),
      bme_(bme),
      rtc_(rtc),
      time_(time_source),
      ema_t_(config::EMA_ALPHA),
      ema_h_(config::EMA_ALPHA),
      ema_p_(config::EMA_ALPHA)
{
}

esp_err_t SensorTask::start(QueueHandle_t log_queue, QueueHandle_t mqtt_queue)
{
    if (isr_queue_ != nullptr || log_queue == nullptr || mqtt_queue == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    log_queue_ = log_queue;
    mqtt_queue_ = mqtt_queue;
    isr_queue_ = xQueueCreate(config::ISR_QUEUE_LEN, sizeof(IsrEvent));
    if (isr_queue_ == nullptr) {
        return ESP_ERR_NO_MEM;
    }

    esp_timer_create_args_t tick_args = {};
    tick_args.callback = &SensorTask::tick_cb;
    tick_args.arg = this;
    tick_args.name = "tick";
    esp_timer_create_args_t conv_args = {};
    conv_args.callback = &SensorTask::conversion_cb;
    conv_args.arg = this;
    conv_args.name = "bme_conv";
    esp_err_t err = esp_timer_create(&tick_args, &tick_timer_);
    if (err == ESP_OK) {
        err = esp_timer_create(&conv_args, &conv_timer_);
    }
    if (err != ESP_OK) {
        return err;
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
void SensorTask::tick_cb(void *arg)
{
    static_cast<SensorTask *>(arg)->post(IsrEventType::Tick);
}

void SensorTask::conversion_cb(void *arg)
{
    static_cast<SensorTask *>(arg)->post(IsrEventType::ConversionDone);
}

void SensorTask::post(IsrEventType type)
{
    const IsrEvent ev = {type, esp_timer_get_time()};
    xQueueSend(isr_queue_, &ev, 0);
}

void SensorTask::run()
{
    for (;;) {
        IsrEvent ev;
        // Finite wait: the timeout branch is where the watchdog is fed in Phase 4.
        if (xQueueReceive(isr_queue_, &ev, pdMS_TO_TICKS(config::TASK_WAIT_MS)) != pdTRUE) {
            continue;
        }
        switch (ev.type) {
        case IsrEventType::Tick:           on_tick(); break;
        case IsrEventType::ConversionDone: on_conversion_done(); break;
        case IsrEventType::Button:         on_button(ev); break;
        }
    }
}

void SensorTask::on_tick()
{
    if (state_ != State::Idle) {
        ESP_LOGW(TAG, "tick while a conversion is still pending: skipped");
        return;
    }

    esp_err_t err;
    {
        I2cLockGuard lock(i2c_mutex_, config::I2C_MUTEX_TIMEOUT_MS);
        err = lock.locked() ? bme_.start_measurement() : ESP_ERR_TIMEOUT;
    }
    if (err != ESP_OK) {
        fail(err, "BME280 start");
        return;
    }
    // Wait for the conversion with a one-shot timer instead of a delay.
    err = esp_timer_start_once(conv_timer_, config::BME280_CONVERSION_US);
    if (err != ESP_OK) {
        fail(err, "conversion timer");
        return;
    }
    state_ = State::WaitConversion;
}

void SensorTask::on_conversion_done()
{
    if (state_ != State::WaitConversion) {
        return;
    }
    state_ = State::Idle;

    Bme280Sample sample = {};
    Ds3231Reading rtc = {};
    esp_err_t bme_err;
    esp_err_t rtc_err;
    {
        I2cLockGuard lock(i2c_mutex_, config::I2C_MUTEX_TIMEOUT_MS);
        if (lock.locked()) {
            bme_err = bme_.read(sample);
            rtc_err = rtc_.read(rtc);
        } else {
            bme_err = rtc_err = ESP_ERR_TIMEOUT;
        }
    }

    bool trusted = false;
    LogEntry entry = {};
    entry.ts = time_.resolve(rtc_err, rtc, &trusted);
    if (trusted) {
        entry.flags |= LOG_FLAG_TIME_TRUSTED;
    }

    if (bme_err == ESP_OK) {
        entry.flags |= LOG_FLAG_SENSOR_VALID;
        entry.temp_c = ema_t_.update(sample.temp_c);
        entry.hum_pct = ema_h_.update(sample.hum_pct);
        entry.press_hpa = ema_p_.update(sample.press_hpa);
    } else {
        ESP_LOGE(TAG, "BME280 read failed: %s", esp_err_to_name(bme_err));
        err_cnt_++;
        // Keep the last filtered values so the line stays readable; the flag says they are stale.
        entry.temp_c = ema_t_.value();
        entry.hum_pct = ema_h_.value();
        entry.press_hpa = ema_p_.value();
    }
    entry.err_cnt = err_cnt_;
    publish(entry);
}

// A failed cycle still produces an entry, so the log shows the error instead of going silent.
void SensorTask::fail(esp_err_t err, const char *what)
{
    ESP_LOGE(TAG, "%s failed: %s", what, esp_err_to_name(err));
    err_cnt_++;
    state_ = State::Idle;

    LogEntry entry = {};
    entry.ts = time(nullptr);  // no RTC reading in this cycle: internal clock, not trusted
    entry.temp_c = ema_t_.value();
    entry.hum_pct = ema_h_.value();
    entry.press_hpa = ema_p_.value();
    entry.err_cnt = err_cnt_;
    publish(entry);
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
        xQueueReceive(mqtt_queue_, &oldest, 0);
        dropped_mqtt_++;
        if (xQueueSend(mqtt_queue_, &entry, 0) != pdTRUE) {
            ESP_LOGW(TAG, "Q_MQTT still full, entry dropped");
        }
    }
}

// Temporary: in Phase 3 the button only logs; its real role (manual events, error counter reset) comes in Phase 4.
void SensorTask::on_button(const IsrEvent &ev)
{
    presses_++;
    ESP_LOGI(TAG, "button pressed (#%u, t=%lld us)", (unsigned)presses_, (long long)ev.timestamp_us);
}
