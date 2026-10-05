#include "bme280.h"

#include "config.h"
#include "esp_log.h"

static const char *TAG = "bme280";

namespace {

// Registers and values (Bosch BST-BME280-DS002).
constexpr uint8_t REG_CALIB_00 = 0x88;  // 26 bytes: 0x88..0xA1
constexpr uint8_t REG_CALIB_26 = 0xE1;  // 7 bytes: 0xE1..0xE7
constexpr uint8_t REG_CTRL_HUM = 0xF2;
constexpr uint8_t REG_STATUS = 0xF3;
constexpr uint8_t REG_CTRL_MEAS = 0xF4;
constexpr uint8_t REG_CONFIG = 0xF5;
constexpr uint8_t REG_DATA = 0xF7;  // 8 bytes: P[3] T[3] H[2]

constexpr uint8_t CTRL_HUM_X1 = 0x01;                                  // osrs_h = x1
constexpr uint8_t CTRL_MEAS_FORCED_X1 = (1u << 5) | (1u << 2) | 0x01;  // osrs_t x1, osrs_p x1, forced
constexpr uint8_t CONFIG_FILTER_OFF = 0x00;
constexpr uint8_t STATUS_MEASURING = 0x08;
constexpr int32_t RAW_SKIPPED_20BIT = 0x80000;  // value of a skipped T or P measurement
constexpr int32_t RAW_SKIPPED_16BIT = 0x8000;   // value of a skipped H measurement

constexpr size_t CALIB_BLOCK_1_LEN = 26;
constexpr size_t CALIB_BLOCK_2_LEN = 7;
constexpr size_t DATA_LEN = 8;

inline uint16_t u16le(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
inline int16_t s16le(const uint8_t *p) { return (int16_t)u16le(p); }

}  // namespace

void Bme280::release()
{
    if (dev_ != nullptr) {
        i2c_master_bus_rm_device(dev_);
        dev_ = nullptr;
    }
}

Bme280::~Bme280()
{
    release();
}

esp_err_t Bme280::read_regs(uint8_t reg, uint8_t *buf, size_t len)
{
    if (dev_ == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    return i2c_master_transmit_receive(dev_, &reg, 1, buf, len, config::I2C_XFER_TIMEOUT_MS);
}

esp_err_t Bme280::write_reg(uint8_t reg, uint8_t value)
{
    if (dev_ == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    const uint8_t tx[2] = {reg, value};
    return i2c_master_transmit(dev_, tx, sizeof(tx), config::I2C_XFER_TIMEOUT_MS);
}

esp_err_t Bme280::init()
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

    uint8_t id = 0;
    err = read_regs(config::BME280_REG_CHIP_ID, &id, 1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "chip ID read failed: %s", esp_err_to_name(err));
        release();  // a failed init leaves nothing behind, so init() can be called again
        return err;
    }
    if (id != config::BME280_CHIP_ID) {
        ESP_LOGE(TAG, "unexpected chip ID 0x%02X (expected 0x%02X): wrong device or address", id,
                 config::BME280_CHIP_ID);
        release();
        return ESP_ERR_NOT_FOUND;
    }

    uint8_t c1[CALIB_BLOCK_1_LEN];
    uint8_t c2[CALIB_BLOCK_2_LEN];
    err = read_regs(REG_CALIB_00, c1, sizeof(c1));
    if (err == ESP_OK) {
        err = read_regs(REG_CALIB_26, c2, sizeof(c2));
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "calibration read failed: %s", esp_err_to_name(err));
        release();
        return err;
    }

    calib_.T1 = u16le(&c1[0]);
    calib_.T2 = s16le(&c1[2]);
    calib_.T3 = s16le(&c1[4]);
    calib_.P1 = u16le(&c1[6]);
    calib_.P2 = s16le(&c1[8]);
    calib_.P3 = s16le(&c1[10]);
    calib_.P4 = s16le(&c1[12]);
    calib_.P5 = s16le(&c1[14]);
    calib_.P6 = s16le(&c1[16]);
    calib_.P7 = s16le(&c1[18]);
    calib_.P8 = s16le(&c1[20]);
    calib_.P9 = s16le(&c1[22]);
    calib_.H1 = c1[25];         // 0xA1
    calib_.H2 = s16le(&c2[0]);  // 0xE1..0xE2
    calib_.H3 = c2[2];          // 0xE3
    // H4 = 0xE4 (signed) << 4 | 0xE5[3:0];  H5 = 0xE6 (signed) << 4 | 0xE5[7:4]
    calib_.H4 = (int16_t)(((int16_t)(int8_t)c2[3] << 4) | (c2[4] & 0x0F));
    calib_.H5 = (int16_t)(((int16_t)(int8_t)c2[5] << 4) | (c2[4] >> 4));
    calib_.H6 = (int8_t)c2[6];  // 0xE7

    err = write_reg(REG_CONFIG, CONFIG_FILTER_OFF);
    if (err == ESP_OK) {
        err = write_reg(REG_CTRL_HUM, CTRL_HUM_X1);  // applied on the next ctrl_meas write
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "configuration failed: %s", esp_err_to_name(err));
        release();
        return err;
    }
    ESP_LOGI(TAG, "ready at 0x%02X (chip ID 0x%02X, forced mode, oversampling x1)", addr_, id);
    return ESP_OK;
}

esp_err_t Bme280::start_measurement()
{
    // ctrl_hum must be written before ctrl_meas to take effect.
    esp_err_t err = write_reg(REG_CTRL_HUM, CTRL_HUM_X1);
    if (err != ESP_OK) {
        return err;
    }
    return write_reg(REG_CTRL_MEAS, CTRL_MEAS_FORCED_X1);
}

esp_err_t Bme280::read(Bme280Sample &out)
{
    uint8_t status = 0;
    esp_err_t err = read_regs(REG_STATUS, &status, 1);
    if (err != ESP_OK) {
        return err;
    }
    if (status & STATUS_MEASURING) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t d[DATA_LEN];
    err = read_regs(REG_DATA, d, sizeof(d));
    if (err != ESP_OK) {
        return err;
    }

    const int32_t adc_p = ((int32_t)d[0] << 12) | ((int32_t)d[1] << 4) | (d[2] >> 4);
    const int32_t adc_t = ((int32_t)d[3] << 12) | ((int32_t)d[4] << 4) | (d[5] >> 4);
    const int32_t adc_h = ((int32_t)d[6] << 8) | d[7];
    if (adc_t == RAW_SKIPPED_20BIT || adc_p == RAW_SKIPPED_20BIT || adc_h == RAW_SKIPPED_16BIT) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    const int32_t t = compensate_temperature(adc_t);  // must run first: sets t_fine_
    const uint32_t p = compensate_pressure(adc_p);
    const uint32_t h = compensate_humidity(adc_h);
    out.temp_c = (float)t / 100.0f;
    out.press_hpa = (float)p / 100.0f;
    out.hum_pct = (float)h / 1024.0f;
    return ESP_OK;
}

// Integer compensation formulas from the datasheet (section 4.2.3).
int32_t Bme280::compensate_temperature(int32_t adc_t)
{
    const int32_t var1 = (((adc_t >> 3) - ((int32_t)calib_.T1 << 1)) * (int32_t)calib_.T2) >> 11;
    const int32_t var2 =
        (((((adc_t >> 4) - (int32_t)calib_.T1) * ((adc_t >> 4) - (int32_t)calib_.T1)) >> 12) * (int32_t)calib_.T3) >> 14;
    t_fine_ = var1 + var2;
    return (t_fine_ * 5 + 128) >> 8;  // 0.01 degC
}

uint32_t Bme280::compensate_pressure(int32_t adc_p) const
{
    int64_t var1 = (int64_t)t_fine_ - 128000;
    int64_t var2 = var1 * var1 * (int64_t)calib_.P6;
    var2 += (var1 * (int64_t)calib_.P5) << 17;
    var2 += (int64_t)calib_.P4 << 35;
    var1 = ((var1 * var1 * (int64_t)calib_.P3) >> 8) + ((var1 * (int64_t)calib_.P2) << 12);
    var1 = (((int64_t)1 << 47) + var1) * (int64_t)calib_.P1 >> 33;
    if (var1 == 0) {
        return 0;  // avoid division by zero
    }
    int64_t p = 1048576 - adc_p;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = ((int64_t)calib_.P9 * (p >> 13) * (p >> 13)) >> 25;
    var2 = ((int64_t)calib_.P8 * p) >> 19;
    p = ((p + var1 + var2) >> 8) + ((int64_t)calib_.P7 << 4);
    return (uint32_t)(p / 256);  // Pa
}

uint32_t Bme280::compensate_humidity(int32_t adc_h) const
{
    int32_t v = t_fine_ - 76800;
    v = (((((adc_h << 14) - ((int32_t)calib_.H4 << 20) - ((int32_t)calib_.H5 * v)) + 16384) >> 15) *
         (((((((v * (int32_t)calib_.H6) >> 10) * (((v * (int32_t)calib_.H3) >> 11) + 32768)) >> 10) + 2097152) *
               (int32_t)calib_.H2 +
           8192) >>
          14));
    v = v - (((((v >> 15) * (v >> 15)) >> 7) * (int32_t)calib_.H1) >> 4);
    v = v < 0 ? 0 : v;
    v = v > 419430400 ? 419430400 : v;
    return (uint32_t)(v >> 12);  // %RH * 1024
}
