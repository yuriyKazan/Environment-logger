#include "time_source.h"

#include <sys/time.h>

#include "esp_log.h"

static const char *TAG = "time_source";

esp_err_t TimeSource::init(Ds3231 &rtc)
{
    trusted_ = false;
    rtc_status_ = RtcStatus::InvalidFields;

    Ds3231Reading reading;
    const esp_err_t err = rtc.read(reading);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "RTC read failed (%s): using the internal clock, time is NOT trusted", esp_err_to_name(err));
        return err;
    }

    rtc_status_ = reading.status;
    if (reading.status != RtcStatus::Valid) {
        ESP_LOGW(TAG, "RTC time not valid (%s): using the internal clock, time is NOT trusted",
                 rtc_status_name(reading.status));
        return ESP_OK;
    }

    const struct timeval tv = {ds3231_to_time_t(reading.time), 0};
    if (settimeofday(&tv, nullptr) != 0) {
        ESP_LOGW(TAG, "settimeofday failed: using the internal clock, time is NOT trusted");
        return ESP_FAIL;
    }
    trusted_ = true;
    ESP_LOGI(TAG, "system clock set from RTC: %04u-%02u-%02u %02u:%02u:%02u", reading.time.year, reading.time.month,
             reading.time.day, reading.time.hour, reading.time.minute, reading.time.second);
    return ESP_OK;
}

time_t TimeSource::resolve(esp_err_t read_err, const Ds3231Reading &reading, bool *trusted)
{
    const bool ok = (read_err == ESP_OK && reading.status == RtcStatus::Valid);
    trusted_ = ok;
    rtc_status_ = (read_err == ESP_OK) ? reading.status : RtcStatus::InvalidFields;
    if (trusted != nullptr) {
        *trusted = ok;
    }
    return ok ? ds3231_to_time_t(reading.time) : time(nullptr);
}

time_t TimeSource::now(bool *trusted) const
{
    if (trusted != nullptr) {
        *trusted = trusted_;
    }
    return time(nullptr);
}
