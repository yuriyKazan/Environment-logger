#pragma once
// Transport layer: thin wrapper over the ESP-IDF MQTT client. Publish only (no subscriptions).
// Reconnection to the broker is handled by the esp-mqtt stack itself.

#include "esp_err.h"
#include "mqtt_client.h"

class MqttPublisher {
public:
    MqttPublisher() = default;
    MqttPublisher(const MqttPublisher &) = delete;
    MqttPublisher &operator=(const MqttPublisher &) = delete;

    // Create the client for MQTT_BROKER_URI (secrets.h). Does not connect yet.
    esp_err_t init();

    // Start connecting. Idempotent: safe to call on every IP acquisition.
    esp_err_t start();

    // Publish a text payload (QoS config::MQTT_QOS, no retain).
    // ESP_ERR_INVALID_STATE if not connected: the message is dropped, never queued.
    esp_err_t publish(const char *topic, const char *payload);

    bool connected() const { return connected_; }

    // Adapter for WifiManager::init(): starts the client when Wi-Fi gets an IP.
    static void on_wifi_connected(void *self);

private:
    static void event_handler(void *arg, esp_event_base_t base, int32_t id, void *data);

    esp_mqtt_client_handle_t client_ = nullptr;
    volatile bool connected_ = false;
    bool started_ = false;
};
