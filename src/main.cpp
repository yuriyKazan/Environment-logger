#include "esp_log.h"
#include "esp_system.h"

#include "button.h"
#include "config.h"
#include "data_types.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "i2c_bus.h"
#include "led.h"

static const char *TAG = config::APP_NAME;

static I2cBus g_i2c;
static Led g_led_ok(config::LED_OK_GPIO);
static Led g_led_err(config::LED_ERR_GPIO);
static Button g_button(config::BUTTON_GPIO);
static QueueHandle_t g_isr_queue;

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

// Bring-up consumer of Q_ISR (Phase 2.3). Replaced by T1 in Phase 3.
static void bringup_task(void *)
{
    uint32_t presses = 0;
    IsrEvent ev;
    for (;;) {
        // Blocks until an event arrives. portMAX_DELAY is acceptable here: this task does
        // not feed the WDT and has nothing else to do (see docs/decisions.md, item 6).
        if (xQueueReceive(g_isr_queue, &ev, portMAX_DELAY) == pdTRUE && ev.type == IsrEventType::Button) {
            presses++;
            g_led_err.toggle();
            ESP_LOGI(TAG, "button pressed (#%u, t=%lld us, dropped=%u), LED_ERR %s", (unsigned)presses,
                     (long long)ev.timestamp_us, (unsigned)g_button.dropped(), g_led_err.is_on() ? "on" : "off");
        }
    }
}

// esp_timer callback: heartbeat on LED_OK, no blocking waits.
static void heartbeat_cb(void *)
{
    g_led_ok.toggle();
}

// Phase 2.2: I2C scanner. Phase 2.3: LEDs and button. Replaced by the task-based design in Phase 3.
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
        if (was_found(found, shown, addr)) {
            ESP_LOGI(TAG, "0x%02X %s: OK", addr, device_name(addr));
        } else {
            ESP_LOGE(TAG, "0x%02X %s: NOT FOUND (check wiring/power)", addr, device_name(addr));
        }
    }

    // Register reads at the configured bus speed prove the 400 kHz setup works.
    ESP_LOGI(TAG, "register reads @ %u Hz:", (unsigned)config::I2C_FREQ_HZ);
    uint8_t id = 0;
    esp_err_t err = g_i2c.read_reg(config::I2C_ADDR_BME280, config::BME280_REG_CHIP_ID, &id, 1);
    if (err == ESP_OK && id == config::BME280_CHIP_ID) {
        ESP_LOGI(TAG, "  BME280 chip ID 0x%02X: OK", id);
    } else {
        ESP_LOGE(TAG, "  BME280 chip ID read failed (%s, id=0x%02X, expected 0x%02X)",
                 esp_err_to_name(err), id, config::BME280_CHIP_ID);
    }
    uint8_t sec = 0;
    err = g_i2c.read_reg(config::I2C_ADDR_DS3231, config::DS3231_REG_SECONDS, &sec, 1);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "  DS3231 seconds register 0x%02X: OK", sec);
    } else {
        ESP_LOGE(TAG, "  DS3231 read failed (%s)", esp_err_to_name(err));
    }

    // ---- Phase 2.3: LEDs, button, event queue ----
    g_isr_queue = xQueueCreate(config::ISR_QUEUE_LEN, sizeof(IsrEvent));
    if (g_isr_queue == nullptr || g_led_ok.init() != ESP_OK || g_led_err.init() != ESP_OK ||
        g_button.init(g_isr_queue) != ESP_OK) {
        ESP_LOGE(TAG, "LED/button init failed");
        return;
    }
    if (xTaskCreate(bringup_task, "bringup", config::BRINGUP_TASK_STACK, nullptr, config::BRINGUP_TASK_PRIO,
                    nullptr) != pdPASS) {
        ESP_LOGE(TAG, "bringup task creation failed");
        return;
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
    ESP_LOGI(TAG, "LED_OK blinks every %u ms; press the button to toggle LED_ERR",
             (unsigned)config::HEARTBEAT_LED_PERIOD_MS);
}
