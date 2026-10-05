#pragma once
// Logic layer: where timestamps come from.
//
// At init the DS3231 is read once. If its time is valid, the ESP32 system clock is set from it
// and timestamps are "trusted". If not (dead battery, default date, stopped oscillator, bus error)
// the ESP32 internal clock keeps running from boot (time since 1970) and timestamps are
// flagged as untrusted, so the log never silently pretends the time is right.

#include <ctime>
#include "ds3231.h"
#include "esp_err.h"

class TimeSource {
public:
    // Read the RTC and synchronise the system clock. Returns the bus error if the RTC could not
    // be read; in that case, as for an invalid RTC time, the clock is flagged untrusted.
    esp_err_t init(Ds3231 &rtc);

    // Timestamp for one log entry. `read_err` and `reading` are the result of Ds3231::read().
    // A valid reading gives the RTC time (*trusted = true); anything else gives the internal
    // clock (*trusted = false), so the log never silently pretends the time is right.
    time_t resolve(esp_err_t read_err, const Ds3231Reading &reading, bool *trusted);

    // Current time (seconds since 1970) and whether it came from a valid RTC.
    time_t now(bool *trusted = nullptr) const;

    bool trusted() const { return trusted_; }
    RtcStatus rtc_status() const { return rtc_status_; }

private:
    bool trusted_ = false;
    RtcStatus rtc_status_ = RtcStatus::InvalidFields;
};
