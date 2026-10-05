#include "diagnostics_task.h"

#include <cstdlib>
#include <cstring>

#include "config.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "diag";

esp_err_t DiagnosticsTask::start()
{
    return xTaskCreate(&DiagnosticsTask::task_entry, "diag", config::DIAG_TASK_STACK, this, config::DIAG_TASK_PRIO,
                       nullptr) == pdPASS
               ? ESP_OK
               : ESP_ERR_NO_MEM;
}

void DiagnosticsTask::task_entry(void *arg)
{
    static_cast<DiagnosticsTask *>(arg)->run();
}

void DiagnosticsTask::run()
{
    for (;;) {
        // A timed notification wait is the period: nobody notifies, the timeout elapses (no delay, no busy-wait).
        (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(config::DIAG_PERIOD_S * 1000));
        report();
    }
}

static char state_letter(eTaskState state)
{
    switch (state) {
    case eRunning:   return 'X';
    case eReady:     return 'R';
    case eBlocked:   return 'B';
    case eSuspended: return 'S';
    case eDeleted:   return 'D';
    default:         return '?';
    }
}

void DiagnosticsTask::report()
{
    const UBaseType_t capacity = uxTaskGetNumberOfTasks() + 4;
    auto *tasks = static_cast<TaskStatus_t *>(std::malloc(capacity * sizeof(TaskStatus_t)));
    if (tasks == nullptr) {
        ESP_LOGW(TAG, "no memory for the task snapshot");
        return;
    }
    configRUN_TIME_COUNTER_TYPE total = 0;
    const UBaseType_t count = uxTaskGetSystemState(tasks, capacity, &total);
    const uint64_t d_total = static_cast<configRUN_TIME_COUNTER_TYPE>(total - static_cast<configRUN_TIME_COUNTER_TYPE>(prev_total_));

    // CPU share of every task over the last period, and the idle share of both cores.
    float share[48] = {};
    float idle_sum = 0.0f;
    for (UBaseType_t i = 0; i < count && i < 48; i++) {
        uint32_t prev = 0;
        for (int k = 0; k < prev_count_; k++) {
            if (prev_task_number_[k] == tasks[i].xTaskNumber) {
                prev = prev_runtime_[k];
                break;
            }
        }
        const uint32_t d = static_cast<uint32_t>(tasks[i].ulRunTimeCounter) - prev;
        share[i] = d_total > 0 ? 100.0f * (float)d / (float)d_total : 0.0f;
        if (std::strncmp(tasks[i].pcTaskName, "IDLE", 4) == 0) {
            idle_sum += share[i];
        }
    }
    const float load = 100.0f - idle_sum / (float)configNUM_CORES;

    ESP_LOGI(TAG, "uptime %lld s, heap free %u B (lowest %u B), CPU load %.1f %% (%d cores averaged), %u tasks",
             (long long)(esp_timer_get_time() / 1000000), (unsigned)esp_get_free_heap_size(),
             (unsigned)esp_get_minimum_free_heap_size(), (double)load, (int)configNUM_CORES, (unsigned)count);
    for (UBaseType_t i = 0; i < count && i < 48; i++) {
        ESP_LOGI(TAG, "  %-16s %c prio %2u  stack free %5u B  cpu %5.1f %%", tasks[i].pcTaskName,
                 state_letter(tasks[i].eCurrentState), (unsigned)tasks[i].uxCurrentPriority,
                 (unsigned)tasks[i].usStackHighWaterMark, (double)share[i]);
    }

    // Remember this snapshot for the next period.
    prev_count_ = count < MAX_TRACKED ? (int)count : MAX_TRACKED;
    for (int k = 0; k < prev_count_; k++) {
        prev_task_number_[k] = tasks[k].xTaskNumber;
        prev_runtime_[k] = static_cast<uint32_t>(tasks[k].ulRunTimeCounter);
    }
    prev_total_ = total;
    std::free(tasks);
}
