#pragma once
// Diagnostics task (Reliability layer): once per DIAG_PERIOD_S it prints a snapshot of the system
// so that the numbers behind the performance table are measured, not guessed:
//   - free heap and the lowest free heap since boot
//   - CPU load (both cores averaged) over the last period
//   - per task: state (X running, R ready, B blocked, S suspended), priority, free stack
//     (high-water mark, bytes) and CPU share
// The "B" states in the table are also the first place to look for a deadlock.
//
// It runs at the lowest priority and is deliberately NOT subscribed to the watchdog, so it can
// never reset the logger. Its wait is a timed notification wait (no delay, no busy-wait).
// Needs CONFIG_FREERTOS_USE_TRACE_FACILITY and CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
// (set in sdkconfig.defaults).

#include "esp_err.h"

class DiagnosticsTask {
public:
    esp_err_t start();

private:
    static void task_entry(void *arg);
    void run();
    void report();

    uint64_t prev_total_ = 0;
    // Run-time counters of the previous report, indexed by FreeRTOS task number, to get per-period shares.
    static constexpr int MAX_TRACKED = 32;
    uint32_t prev_task_number_[MAX_TRACKED] = {};
    uint32_t prev_runtime_[MAX_TRACKED] = {};
    int prev_count_ = 0;
};
