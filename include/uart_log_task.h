#pragma once
// T2: the only task that writes log lines to the UART (Transport layer). Receives LogEntry items
// from Q_LOG, formats them and prints one line each, so lines from different tasks never mix.

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

class UartLogTask {
public:
    explicit UartLogTask(QueueHandle_t log_queue) : log_queue_(log_queue) {}

    esp_err_t start();

private:
    static void task_entry(void *arg);
    void run();

    QueueHandle_t log_queue_;
};
