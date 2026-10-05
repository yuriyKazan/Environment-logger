#include "mqtt_task.h"

#include <cstdio>

#include "config.h"
#include "esp_log.h"

static const char *TAG = "T4";

esp_err_t MqttTask::start()
{
    if (queue_ == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    return xTaskCreate(&MqttTask::task_entry, "T4_mqtt", config::MQTT_TASK_STACK, this, config::MQTT_TASK_PRIO,
                       nullptr) == pdPASS
               ? ESP_OK
               : ESP_ERR_NO_MEM;
}

void MqttTask::task_entry(void *arg)
{
    static_cast<MqttTask *>(arg)->run();
}

void MqttTask::run()
{
    for (;;) {
        LogEntry entry;
        if (xQueueReceive(queue_, &entry, pdMS_TO_TICKS(config::TASK_WAIT_MS)) != pdTRUE) {
            continue;
        }
        if (!mqtt_.connected()) {
            dropped_++;  // offline: skip silently, nothing is buffered
            continue;
        }
        publish_entry(entry);
    }
}

esp_err_t MqttTask::publish_value(const char *name, const char *payload)
{
    char topic[config::MQTT_TOPIC_MAX];
    const int n = std::snprintf(topic, sizeof(topic), "%s/%s", config::MQTT_TOPIC_PREFIX, name);
    if (n < 0 || (size_t)n >= sizeof(topic)) {
        return ESP_ERR_INVALID_SIZE;
    }
    return mqtt_.publish(topic, payload);
}

void MqttTask::publish_entry(const LogEntry &e)
{
    char payload[config::MQTT_PAYLOAD_MAX];
    esp_err_t err = ESP_OK;

    if (e.flags & LOG_FLAG_SENSOR_VALID) {  // never publish stale values as if they were fresh
        std::snprintf(payload, sizeof(payload), "%.2f", (double)e.temp_c);
        err |= publish_value("temp", payload);
        std::snprintf(payload, sizeof(payload), "%.1f", (double)e.hum_pct);
        err |= publish_value("hum", payload);
        std::snprintf(payload, sizeof(payload), "%.1f", (double)e.press_hpa);
        err |= publish_value("pres", payload);
    }
    std::snprintf(payload, sizeof(payload), "%u", (unsigned)e.err_cnt);
    err |= publish_value("err", payload);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "publish failed");
    }
}
