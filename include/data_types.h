#pragma once
// Shared data structures.

#include <cstdint>
#include <ctime>

enum class IsrEventType : uint8_t {
    Tick,            // measurement timer
    ConversionDone,  // BME280 conversion time elapsed (one-shot timer)
    Button,          // debounced button press
};

// Item of Q_ISR: what happened and when (esp_timer time, microseconds).
struct IsrEvent {
    IsrEventType type;
    int64_t timestamp_us;
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
