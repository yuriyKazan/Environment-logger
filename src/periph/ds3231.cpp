#include "ds3231.h"

#include "config.h"
#include "esp_log.h"

static const char *TAG = "ds3231";

namespace {

constexpr uint8_t REG_TIME = 0x00;    // seconds, minutes, hours, weekday, date, month, year
constexpr uint8_t REG_STATUS = 0x0F;  // bit 7: OSF (oscillator stopped)
constexpr size_t TIME_LEN = 7;
constexpr uint8_t STATUS_OSF = 0x80;

constexpr uint8_t HOURS_12H_MODE = 0x40;
constexpr uint8_t HOURS_PM_OR_20H = 0x20;
constexpr uint8_t MONTH_CENTURY = 0x80;

inline bool bcd_ok(uint8_t v) { return (v & 0x0F) <= 9 && (v >> 4) <= 9; }
inline uint8_t from_bcd(uint8_t v) { return (uint8_t)((v >> 4) * 10 + (v & 0x0F)); }
inline uint8_t to_bcd(uint8_t v) { return (uint8_t)(((v / 10) << 4) | (v % 10)); }

}  // namespace

const char *rtc_status_name(RtcStatus status)
{
    switch (status) {
    case RtcStatus::Valid:             return "valid";
    case RtcStatus::OscillatorStopped: return "oscillator stopped";
    case RtcStatus::DefaultDate:       return "default date 2000-01-01 (battery?)";
    case RtcStatus::InvalidFields:     return "invalid fields";
    }
    return "?";
}

// Days from 1970-01-01 to a civil date (proleptic Gregorian), Howard Hinnant's algorithm.
time_t ds3231_to_time_t(const Ds3231Time &t)
{
    int y = t.year;
    const unsigned m = t.month;
    const unsigned d = t.day;
    y -= m <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    const int64_t days = (int64_t)era * 146097 + (int64_t)doe - 719468;
    return (time_t)(days * 86400 + t.hour * 3600 + t.minute * 60 + t.second);
}

void Ds3231::release()
{
    if (dev_ != nullptr) {
        i2c_master_bus_rm_device(dev_);
        dev_ = nullptr;
    }
}

Ds3231::~Ds3231()
{
    release();
}

esp_err_t Ds3231::read_regs(uint8_t reg, uint8_t *buf, size_t len)
{
    if (dev_ == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    return i2c_master_transmit_receive(dev_, &reg, 1, buf, len, config::I2C_XFER_TIMEOUT_MS);
}

esp_err_t Ds3231::write_regs(uint8_t reg, const uint8_t *buf, size_t len)
{
    if (dev_ == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    if (len > TIME_LEN) {
        return ESP_ERR_INVALID_SIZE;
    }
    uint8_t tx[TIME_LEN + 1];
    tx[0] = reg;
    for (size_t i = 0; i < len; i++) {
        tx[1 + i] = buf[i];
    }
    return i2c_master_transmit(dev_, tx, len + 1, config::I2C_XFER_TIMEOUT_MS);
}

esp_err_t Ds3231::init()
{
    if (dev_ != nullptr || bus_.handle() == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    i2c_device_config_t dev_cfg = {};
    dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address = addr_;
    dev_cfg.scl_speed_hz = config::I2C_FREQ_HZ;
    esp_err_t err = i2c_master_bus_add_device(bus_.handle(), &dev_cfg, &dev_);
    if (err != ESP_OK) {
        dev_ = nullptr;
        return err;
    }

    uint8_t status = 0;
    err = read_regs(REG_STATUS, &status, 1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "status read failed: %s", esp_err_to_name(err));
        release();  // a failed init leaves nothing behind, so init() can be called again
        return err;
    }
    ESP_LOGI(TAG, "ready at 0x%02X (OSF=%d)", addr_, (status & STATUS_OSF) ? 1 : 0);
    return ESP_OK;
}

esp_err_t Ds3231::read(Ds3231Reading &out)
{
    uint8_t r[TIME_LEN];
    esp_err_t err = read_regs(REG_TIME, r, sizeof(r));
    if (err != ESP_OK) {
        return err;
    }
    uint8_t status = 0;
    err = read_regs(REG_STATUS, &status, 1);
    if (err != ESP_OK) {
        return err;
    }

    out = {};
    out.status = RtcStatus::InvalidFields;

    // r[3] is the weekday: not needed. Hours: 24 h mode, or 12 h mode with the PM flag.
    const bool digits_ok = bcd_ok(r[0] & 0x7F) && bcd_ok(r[1] & 0x7F) && bcd_ok(r[2] & 0x1F) &&
                           bcd_ok(r[4] & 0x3F) && bcd_ok(r[5] & 0x1F) && bcd_ok(r[6]);
    if (!digits_ok) {
        return ESP_OK;
    }

    uint8_t hour;
    if (r[2] & HOURS_12H_MODE) {
        hour = from_bcd(r[2] & 0x1F) % 12;
        if (r[2] & HOURS_PM_OR_20H) {
            hour += 12;
        }
    } else {
        hour = from_bcd(r[2] & 0x3F);
    }
    out.time.second = from_bcd(r[0] & 0x7F);
    out.time.minute = from_bcd(r[1] & 0x7F);
    out.time.hour = hour;
    out.time.day = from_bcd(r[4] & 0x3F);
    out.time.month = from_bcd(r[5] & 0x1F);
    out.time.year = (uint16_t)(2000 + from_bcd(r[6]) + ((r[5] & MONTH_CENTURY) ? 100 : 0));

    const Ds3231Time &t = out.time;
    if (t.second > 59 || t.minute > 59 || t.hour > 23 || t.day < 1 || t.day > 31 || t.month < 1 || t.month > 12) {
        return ESP_OK;  // InvalidFields
    }

    if (status & STATUS_OSF) {
        out.status = RtcStatus::OscillatorStopped;
    } else if (t.year == 2000 && t.month == 1 && t.day == 1) {
        out.status = RtcStatus::DefaultDate;
    } else {
        out.status = RtcStatus::Valid;
    }
    return ESP_OK;
}

esp_err_t Ds3231::set_time(const Ds3231Time &t)
{
    if (t.year < 2000 || t.year > 2099 || t.month < 1 || t.month > 12 || t.day < 1 || t.day > 31 || t.hour > 23 ||
        t.minute > 59 || t.second > 59) {
        return ESP_ERR_INVALID_ARG;
    }
    const uint8_t regs[TIME_LEN] = {
        to_bcd(t.second), to_bcd(t.minute), to_bcd(t.hour),
        1,  // weekday is not used by this project
        to_bcd(t.day),    to_bcd(t.month),  to_bcd((uint8_t)(t.year - 2000)),
    };
    esp_err_t err = write_regs(REG_TIME, regs, sizeof(regs));
    if (err != ESP_OK) {
        return err;
    }

    uint8_t status = 0;
    err = read_regs(REG_STATUS, &status, 1);
    if (err != ESP_OK) {
        return err;
    }
    const uint8_t cleared = status & (uint8_t)~STATUS_OSF;
    return write_regs(REG_STATUS, &cleared, 1);
}
