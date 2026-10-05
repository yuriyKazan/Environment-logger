#pragma once
// Logic layer: plausibility check of one BME280 sample, before it reaches the filter.
//
// A bus glitch or a half-powered sensor can return values that are well formed but physically
// impossible (for example pressure 0 from a failed compensation, or humidity above 100 %).
// Feeding them to the EMA would poison the filtered value for a long time, so such a sample is
// rejected and counted as a failed attempt. Header-only and free of ESP-IDF includes, so it is
// unit-tested on the PC (env:native).
//
// The limits are the BME280 operating ranges from the datasheet (BST-BME280-DS002).

#include <cmath>
#include <cstdint>

namespace sensor_limits {
inline constexpr float TEMP_MIN_C = -40.0f;
inline constexpr float TEMP_MAX_C = 85.0f;
inline constexpr float HUM_MIN_PCT = 0.0f;
inline constexpr float HUM_MAX_PCT = 100.0f;
inline constexpr float PRESS_MIN_HPA = 300.0f;
inline constexpr float PRESS_MAX_HPA = 1100.0f;
}  // namespace sensor_limits

enum class SampleStatus : uint8_t {
    Ok,
    NotFinite,  // NaN or infinity
    TemperatureOutOfRange,
    HumidityOutOfRange,
    PressureOutOfRange,
};

// Limits are inclusive. The first problem found is reported.
inline SampleStatus validate_sample(float temp_c, float hum_pct, float press_hpa)
{
    using namespace sensor_limits;
    if (!std::isfinite(temp_c) || !std::isfinite(hum_pct) || !std::isfinite(press_hpa)) {
        return SampleStatus::NotFinite;
    }
    if (temp_c < TEMP_MIN_C || temp_c > TEMP_MAX_C) {
        return SampleStatus::TemperatureOutOfRange;
    }
    if (hum_pct < HUM_MIN_PCT || hum_pct > HUM_MAX_PCT) {
        return SampleStatus::HumidityOutOfRange;
    }
    if (press_hpa < PRESS_MIN_HPA || press_hpa > PRESS_MAX_HPA) {
        return SampleStatus::PressureOutOfRange;
    }
    return SampleStatus::Ok;
}

inline const char *sample_status_name(SampleStatus status)
{
    switch (status) {
    case SampleStatus::Ok:                    return "ok";
    case SampleStatus::NotFinite:             return "not a finite number";
    case SampleStatus::TemperatureOutOfRange: return "temperature out of range";
    case SampleStatus::HumidityOutOfRange:    return "humidity out of range";
    case SampleStatus::PressureOutOfRange:    return "pressure out of range";
    }
    return "?";
}
