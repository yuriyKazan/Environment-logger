#include "uart_log_task.h"

#include <cstdio>

#include "config.h"
#include "data_types.h"
#include "esp_log.h"
#include "log_format.h"

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

void UartLogTask::run()
{
    char line[config::LOG_LINE_MAX];
    for (;;) {
        LogEntry entry;
        // Finite wait: the timeout branch is where the watchdog is fed in Phase 4.
        if (xQueueReceive(log_queue_, &entry, pdMS_TO_TICKS(config::TASK_WAIT_MS)) != pdTRUE) {
            continue;
        }
        if (format_log_line(entry, line, sizeof(line)) == 0) {
            ESP_LOGE(TAG, "log line did not fit into %u bytes", (unsigned)sizeof(line));
            continue;
        }
        std::printf("%s\n", line);
    }
}
