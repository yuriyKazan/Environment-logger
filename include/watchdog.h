#pragma once
// Reliability layer: thin wrappers over the ESP-IDF task watchdog.
//
// T1, T2 and T3 each subscribe themselves once at the start of their task function and feed the
// watchdog at the top of every loop iteration, i.e. after each queue wait (whose timeout is far
// shorter than the watchdog timeout, see sdkconfig.defaults). A task that blocks or deadlocks
// anywhere else stops feeding, and the watchdog resets the chip. T4 (network) is deliberately
// not subscribed: a bad Wi-Fi link must never reset the logger.

#include "esp_log.h"
#include "esp_task_wdt.h"

// Subscribe the calling task. Returns true on success; failure is logged, never silent.
inline bool wdt_subscribe_current_task(const char *name)
{
    const esp_err_t err = esp_task_wdt_add(nullptr);
    if (err != ESP_OK) {
        ESP_LOGE("wdt", "%s could not subscribe to the watchdog: %s", name, esp_err_to_name(err));
        return false;
    }
    return true;
}

// Feed the watchdog for the calling task.
inline void wdt_feed()
{
    esp_task_wdt_reset();
}
