#pragma once
// Periph layer: DS3231 real-time clock over I2C.
//
// Reads the time registers and decides whether the time can be trusted:
//   - the oscillator-stop flag (OSF) is set      -> the clock was stopped, time is unreliable
//   - the date is the power-on default 2000-01-01 -> dead or missing backup battery
//   - a BCD digit or a field is out of range     -> corrupted read
// Every method returns esp_err_t. The caller must hold the I2C bus mutex around each call.

#include <cstdint>
#include <ctime>
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "i2c_bus.h"

struct Ds3231Time {
    uint16_t year;  // 2000..2099
    uint8_t month;  // 1..12
    uint8_t day;    // 1..31
    uint8_t hour;   // 0..23
    uint8_t minute;
    uint8_t second;
};

enum class RtcStatus : uint8_t {
    Valid,
    OscillatorStopped,
    DefaultDate,
    InvalidFields,
};

struct Ds3231Reading {
    Ds3231Time time;
    RtcStatus status;
};

const char *rtc_status_name(RtcStatus status);

// Seconds since 1970-01-01 for a calendar time, treating it as UTC (no time zone logic).
time_t ds3231_to_time_t(const Ds3231Time &t);

class Ds3231 {
public:
    Ds3231(const I2cBus &bus, uint8_t addr) : bus_(bus), addr_(addr) {}
    ~Ds3231();
    Ds3231(const Ds3231 &) = delete;
    Ds3231 &operator=(const Ds3231 &) = delete;

    // Register the device on the bus (400 kHz) and check that it answers.
    esp_err_t init();

    // Read the clock and classify it. The registers are read in one burst, so the fields
    // are consistent. The returned esp_err_t is about the bus; the time quality is in out.status.
    esp_err_t read(Ds3231Reading &out);

    // Set the clock and clear the oscillator-stop flag.
    esp_err_t set_time(const Ds3231Time &t);

private:
    void release();
    esp_err_t read_regs(uint8_t reg, uint8_t *buf, size_t len);
    esp_err_t write_regs(uint8_t reg, const uint8_t *buf, size_t len);

    const I2cBus &bus_;
    uint8_t addr_;
    i2c_master_dev_handle_t dev_ = nullptr;
};
