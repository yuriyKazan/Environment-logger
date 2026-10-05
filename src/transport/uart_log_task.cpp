#include "uart_log_task.h"

#include <cstdio>

#include "config.h"
#include "data_types.h"
#include "esp_log.h"
#include "log_format.h"
#include "esp_timer.h"
#include "freertos/task.h"
#include "watchdog.h"

static const char *TAG = "T2";

esp_err_t UartLogTask::start()
{
    if (log_queue_ == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    return xTaskCreate(&UartLogTask::task_entry, "T2_uart", config::UART_LOG_TASK_STACK, this,
                       config::UART_LOG_TASK_PRIO, nullptr) == pdPASS
               ? ESP_OK
               : ESP_ERR_NO_MEM;
}

void UartLogTask::task_entry(void *arg)
{
    static_cast<UartLogTask *>(arg)->run();
}

// Fault injection (config::FAULT_STALL_UART): stop reading Q_LOG for FAULT_STALL_UART_MS so that it fills up.
// The watchdog keeps being fed, because the test is about the queue policy, not about a hang.
void UartLogTask::stall()
{
    ESP_LOGW(TAG, "FAULT INJECTION: not reading Q_LOG for %u s", (unsigned)(config::FAULT_STALL_UART_MS / 1000));
    const int64_t until = esp_timer_get_time() + (int64_t)config::FAULT_STALL_UART_MS * 1000;
    while (esp_timer_get_time() < until) {
        wdt_feed();
        (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(config::TASK_WAIT_MS));  // timed wait, nobody notifies
    }
    ESP_LOGW(TAG, "FAULT INJECTION: reading Q_LOG again");
}

void UartLogTask::run()
{
    char line[config::LOG_LINE_MAX];
    uint32_t received = 0;
    wdt_subscribe_current_task("T2");
    for (;;) {
        LogEntry entry;
        // Finite wait, so the loop (and the watchdog feed) runs at least once per TASK_WAIT_MS.
        const bool got_entry = xQueueReceive(log_queue_, &entry, pdMS_TO_TICKS(config::TASK_WAIT_MS)) == pdTRUE;
        wdt_feed();
        if (!got_entry) {
            continue;
        }
        if (config::FAULT_STALL_UART && ++received == config::FAULT_STALL_UART_AFTER_ENTRIES) {
            stall();
        }
        if (format_log_line(entry, line, sizeof(line)) == 0) {
            ESP_LOGE(TAG, "log line did not fit into %u bytes", (unsigned)sizeof(line));
            continue;
        }
        std::printf("%s\n", line);
    }
}
