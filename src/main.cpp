#include "esp_log.h"
#include "esp_system.h"

#include <cstdlib>
#include <cstring>
#include <ctime>

#include "bme280.h"
#include "button.h"
#include "config.h"
#include "data_types.h"
#include "ds3231.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "i2c_bus.h"
#include "i2c_mutex.h"
#include "led.h"
#include "mqtt_publisher.h"
#include "mqtt_task.h"
#include "sensor_task.h"
#include "time_source.h"
#include "uart_log_task.h"
#include "wifi_manager.h"

static const char *TAG = config::APP_NAME;

static I2cBus g_i2c;
static I2cMutex g_i2c_mutex;
static Led g_led_ok(config::LED_OK_GPIO);
static Led g_led_err(config::LED_ERR_GPIO);
static Bme280 g_bme(g_i2c, config::I2C_ADDR_BME280);
static Ds3231 g_rtc(g_i2c, config::I2C_ADDR_DS3231);
static TimeSource g_time;
static Button g_button(config::BUTTON_GPIO);
static QueueHandle_t g_log_queue;   // Q_LOG: T1 -> T2
static QueueHandle_t g_mqtt_queue;  // Q_MQTT: T1 -> T4
static WifiManager g_wifi;
static MqttPublisher g_mqtt;
static MqttTask *g_mqtt_task;  // needs Q_MQTT, created in app_main
static SensorTask g_sensor_task(g_i2c_mutex, g_bme, g_rtc, g_time);
static UartLogTask *g_uart_task;  // needs Q_LOG, created in app_main

static const char *device_name(uint8_t addr)
{
    switch (addr) {
    case config::I2C_ADDR_BME280: return "BME280";
    case config::I2C_ADDR_DS3231: return "DS3231";
    case config::I2C_ADDR_AT24C32: return "AT24C32 EEPROM on DS3231 module";
    default:                      return "unknown";
    }
}

static bool was_found(const uint8_t *found, size_t n, uint8_t addr)
{
    for (size_t i = 0; i < n; i++) {
        if (found[i] == addr) {
            return true;
        }
    }
    return false;
}

// Parse __DATE__ ("Oct  5 2026") and __TIME__ ("11:42:51") of this build.
static bool build_time(Ds3231Time &t)
{
    static const char *months = "JanFebMarAprMayJunJulAugSepOctNovDec";
    const char *date = __DATE__;
    const char *found = nullptr;
    for (int i = 0; i < 12; i++) {
        if (strncmp(date, months + 3 * i, 3) == 0) {
            t.month = (uint8_t)(i + 1);
            found = date;
        }
    }
    if (found == nullptr) {
        return false;
    }
    t.day = (uint8_t)atoi(date + 4);
    t.year = (uint16_t)atoi(date + 7);
    const char *tm_str = __TIME__;
    t.hour = (uint8_t)atoi(tm_str);
    t.minute = (uint8_t)atoi(tm_str + 3);
    t.second = (uint8_t)atoi(tm_str + 6);
    return true;
}

// esp_timer callback: heartbeat on LED_OK, no blocking waits.
static void heartbeat_cb(void *)
{
    g_led_ok.toggle();
}

// Boot self-check (I2C scan, register reads), then start the tasks.
extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "boot, IDF %s", esp_get_idf_version());

    if (g_i2c.init() != ESP_OK) {
        ESP_LOGE(TAG, "I2C bus init failed");
        return;
    }

    uint8_t found[config::I2C_SCAN_MAX_FOUND];
    size_t count = 0;
    if (g_i2c.scan(found, config::I2C_SCAN_MAX_FOUND, &count) != ESP_OK) {
        ESP_LOGE(TAG, "I2C scan failed");
        return;
    }
    const size_t shown = count < config::I2C_SCAN_MAX_FOUND ? count : config::I2C_SCAN_MAX_FOUND;
    ESP_LOGI(TAG, "I2C scan (probe @ 100 kHz): %u device(s)", (unsigned)count);
    for (size_t i = 0; i < shown; i++) {
        ESP_LOGI(TAG, "  0x%02X (%s)", found[i], device_name(found[i]));
    }
    const uint8_t expected[] = {config::I2C_ADDR_BME280, config::I2C_ADDR_DS3231};
    for (uint8_t addr : expected) {
        if (!was_found(found, shown, addr)) {
            ESP_LOGE(TAG, "0x%02X %s: NOT FOUND (check wiring/power)", addr, device_name(addr));
            return;
        }
    }

    // Devices, RTC state and time source. The tasks are not running yet, so no mutex is needed here.
    if (g_bme.init() != ESP_OK || g_rtc.init() != ESP_OK) {
        ESP_LOGE(TAG, "BME280/DS3231 init failed");
        return;
    }
    Ds3231Reading rtc;
    if (g_rtc.read(rtc) == ESP_OK) {
        ESP_LOGI(TAG, "RTC %04u-%02u-%02u %02u:%02u:%02u: %s", rtc.time.year, rtc.time.month, rtc.time.day,
                 rtc.time.hour, rtc.time.minute, rtc.time.second, rtc_status_name(rtc.status));
        Ds3231Time bt;
        if (config::RTC_SET_FROM_BUILD_TIME_IF_INVALID && rtc.status != RtcStatus::Valid && build_time(bt)) {
            ESP_LOGW(TAG, "setting RTC from build time");
            if (g_rtc.set_time(bt) != ESP_OK) {
                ESP_LOGE(TAG, "RTC set_time failed");
            }
        }
    } else {
        ESP_LOGE(TAG, "RTC read failed");
    }
    g_time.init(g_rtc);

    // Synchronisation objects, then the tasks (T1 sensor, T2 UART log) and the button.
    g_log_queue = xQueueCreate(config::LOG_QUEUE_LEN, sizeof(LogEntry));
    g_mqtt_queue = xQueueCreate(config::MQTT_QUEUE_LEN, sizeof(LogEntry));
    g_uart_task = new UartLogTask(g_log_queue);
    g_mqtt_task = new MqttTask(g_mqtt, g_mqtt_queue);
    if (g_i2c_mutex.init() != ESP_OK || g_log_queue == nullptr || g_mqtt_queue == nullptr ||
        g_led_ok.init() != ESP_OK || g_led_err.init() != ESP_OK) {
        ESP_LOGE(TAG, "mutex/queue/LED init failed");
        return;
    }
    if (g_uart_task->start() != ESP_OK || g_sensor_task.start(g_log_queue, g_mqtt_queue) != ESP_OK ||
        g_button.init(g_sensor_task.isr_queue()) != ESP_OK) {
        ESP_LOGE(TAG, "task or button start failed");
        return;
    }

    // Network last and best effort: if anything fails here, the UART log keeps running.
    esp_err_t nvs = nvs_flash_init();
    if (nvs == ESP_ERR_NVS_NO_FREE_PAGES || nvs == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs = nvs_flash_init();
    }
    if (nvs != ESP_OK || g_mqtt.init() != ESP_OK || g_mqtt_task->start() != ESP_OK ||
        g_wifi.init(&MqttPublisher::on_wifi_connected, &g_mqtt) != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi/MQTT start failed: continuing with the UART log only");
    }

    esp_timer_handle_t hb = nullptr;
    esp_timer_create_args_t hb_args = {};
    hb_args.callback = heartbeat_cb;
    hb_args.name = "heartbeat";
    if (esp_timer_create(&hb_args, &hb) != ESP_OK ||
        esp_timer_start_periodic(hb, (uint64_t)config::HEARTBEAT_LED_PERIOD_MS * 1000) != ESP_OK) {
        ESP_LOGE(TAG, "heartbeat timer failed");
        return;
    }
    ESP_LOGI(TAG, "running: one log line every %u ms", (unsigned)config::MEASURE_INTERVAL_MS);
}
