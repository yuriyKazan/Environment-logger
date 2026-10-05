#include "mqtt_publisher.h"

#include "config.h"
#include "esp_log.h"
#include "secrets.h"

static const char *TAG = "mqtt";

esp_err_t MqttPublisher::init()
{
    if (client_ != nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_mqtt_client_config_t cfg = {};
    cfg.broker.address.uri = MQTT_BROKER_URI;
    client_ = esp_mqtt_client_init(&cfg);
    if (client_ == nullptr) {
        return ESP_FAIL;
    }
    return esp_mqtt_client_register_event(client_, static_cast<esp_mqtt_event_id_t>(ESP_EVENT_ANY_ID),
                                          &MqttPublisher::event_handler, this);
}

esp_err_t MqttPublisher::start()
{
    if (client_ == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    if (started_) {
        return ESP_OK;
    }
    const esp_err_t err = esp_mqtt_client_start(client_);
    if (err == ESP_OK) {
        started_ = true;
        ESP_LOGI(TAG, "connecting to %s", MQTT_BROKER_URI);
    }
    return err;
}

esp_err_t MqttPublisher::publish(const char *topic, const char *payload)
{
    if (client_ == nullptr || !connected_) {
        return ESP_ERR_INVALID_STATE;
    }
    return esp_mqtt_client_publish(client_, topic, payload, 0, config::MQTT_QOS, 0) >= 0 ? ESP_OK : ESP_FAIL;
}

void MqttPublisher::on_wifi_connected(void *self)
{
    const esp_err_t err = static_cast<MqttPublisher *>(self)->start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "start failed: %s", esp_err_to_name(err));
    }
}

void MqttPublisher::event_handler(void *arg, esp_event_base_t, int32_t id, void *)
{
    auto *self = static_cast<MqttPublisher *>(arg);
    switch (static_cast<esp_mqtt_event_id_t>(id)) {
    case MQTT_EVENT_CONNECTED:
        self->connected_ = true;
        ESP_LOGI(TAG, "connected to the broker");
        break;
    case MQTT_EVENT_DISCONNECTED:
        self->connected_ = false;
        ESP_LOGW(TAG, "disconnected from the broker");
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGW(TAG, "client error");
        break;
    default:
        break;
    }
}
