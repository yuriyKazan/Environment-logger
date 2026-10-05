#pragma once
// Shared data structures.

#include <cstdint>

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
