#pragma once
// T4: publishes every LogEntry from Q_MQTT to the broker (Transport layer), best effort.
// It holds no mutex and never touches I2C. If the broker is unreachable the entries are
// simply dropped: UART stays the source of truth and T1-T3 are never slowed down.
//
// Topics: <prefix>/temp, /hum, /pres (only when the sensor data is valid) and /err (always).

#include <cstdint>
#include "data_types.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "mqtt_publisher.h"

class MqttTask {
public:
    MqttTask(MqttPublisher &mqtt, QueueHandle_t mqtt_queue) : mqtt_(mqtt), queue_(mqtt_queue) {}

    esp_err_t start();
    uint32_t dropped() const { return dropped_; }

private:
    static void task_entry(void *arg);
    void run();
    void publish_entry(const LogEntry &entry);
    esp_err_t publish_value(const char *name, const char *payload);

    MqttPublisher &mqtt_;
    QueueHandle_t queue_;
    uint32_t dropped_ = 0;
};
