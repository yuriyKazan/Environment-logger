#pragma once
// Shared data structures.

#include <cstdint>
#include <ctime>

// Events passed through queues. Which queue carries which type:
//   T1 queue (sensor task):  Tick, ConversionDone, Retry, RecoveryDone, RecoveryTimeout, TestHang
//   T3 queue (supervisor):   I2cFailure, MeasurementOk, Button, ButtonRelease, LongPressTimeout
enum class IsrEventType : uint8_t {
    Tick,             // measurement timer
    ConversionDone,   // BME280 conversion time elapsed (one-shot timer)
    Retry,            // retry delay elapsed (one-shot timer)
    RecoveryDone,     // T3 finished the bus reset; value = esp_err_t of the reset
    RecoveryTimeout,  // T3 did not answer in time
    TestHang,         // T3 asks T1 to hang on purpose (watchdog demonstration)
    I2cFailure,       // T1 gave up after the retries; value = esp_err_t of the last failure
    MeasurementOk,    // T1 completed a good measurement after an error
    Button,           // debounced button press
    ButtonRelease,    // debounced button release
    LongPressTimeout, // the button has been held for BUTTON_LONG_PRESS_MS
};

// Item of the event queues: what happened, when (esp_timer time, microseconds) and one value.
struct IsrEvent {
    IsrEventType type;
    int64_t timestamp_us;
    int32_t value = 0;
};

// LogEntry flags.
inline constexpr uint8_t LOG_FLAG_SENSOR_VALID = 1u << 0;  // BME280 values are from a successful read
inline constexpr uint8_t LOG_FLAG_TIME_TRUSTED = 1u << 1;  // timestamp comes from a valid DS3231 reading

// One measurement, produced by T1 and consumed by T2 (UART) and T4 (MQTT).
struct LogEntry {
    time_t ts;          // seconds since 1970, from the DS3231 or, as a fallback, the internal clock
    float temp_c;       // EMA-filtered
    float hum_pct;      // EMA-filtered
    float press_hpa;    // EMA-filtered
    uint32_t err_cnt;   // cumulative measurement errors since boot
    uint8_t flags;      // LOG_FLAG_*
};
