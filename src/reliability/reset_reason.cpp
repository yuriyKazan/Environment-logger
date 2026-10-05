#include "reset_reason.h"

#include "esp_log.h"

static const char *TAG = "reset";

const char *reset_reason_name(esp_reset_reason_t reason)
{
    switch (reason) {
    case ESP_RST_POWERON:   return "power-on";
    case ESP_RST_EXT:       return "external pin";
    case ESP_RST_SW:        return "software reset";
    case ESP_RST_PANIC:     return "panic (exception or abort)";
    case ESP_RST_INT_WDT:   return "interrupt watchdog";
    case ESP_RST_TASK_WDT:  return "task watchdog";
    case ESP_RST_WDT:       return "other watchdog";
    case ESP_RST_DEEPSLEEP: return "wake from deep sleep";
    case ESP_RST_BROWNOUT:  return "brownout";
    case ESP_RST_SDIO:      return "SDIO";
    case ESP_RST_USB:       return "USB";
    case ESP_RST_JTAG:      return "JTAG";
    case ESP_RST_EFUSE:     return "eFuse error";
    case ESP_RST_PWR_GLITCH: return "power glitch";
    case ESP_RST_CPU_LOCKUP: return "CPU lockup";
    case ESP_RST_UNKNOWN:
    default:                return "unknown";
    }
}

bool reset_reason_is_watchdog(esp_reset_reason_t reason)
{
    return reason == ESP_RST_TASK_WDT || reason == ESP_RST_INT_WDT || reason == ESP_RST_WDT;
}

esp_reset_reason_t log_reset_reason()
{
    const esp_reset_reason_t reason = esp_reset_reason();
    if (reset_reason_is_watchdog(reason)) {
        ESP_LOGE(TAG, "previous run ended with a WATCHDOG reset (%s): a task hung or deadlocked", reset_reason_name(reason));
    } else if (reason == ESP_RST_PANIC || reason == ESP_RST_BROWNOUT || reason == ESP_RST_CPU_LOCKUP) {
        ESP_LOGW(TAG, "previous run ended abnormally: %s", reset_reason_name(reason));
    } else {
        ESP_LOGI(TAG, "reset reason: %s", reset_reason_name(reason));
    }
    return reason;
}
