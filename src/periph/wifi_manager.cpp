#include "wifi_manager.h"

#include <cstring>

#include "config.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "secrets.h"

static const char *TAG = "wifi";

esp_err_t WifiManager::init(ConnectedCb on_got_ip, void *arg)
{
    on_got_ip_ = on_got_ip;
    on_got_ip_arg_ = arg;

    esp_err_t err = esp_netif_init();
    if (err != ESP_OK) {
        return err;
    }
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }
    esp_netif_create_default_wifi_sta();

    const wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&init_cfg);
    if (err != ESP_OK) {
        return err;
    }

    esp_timer_create_args_t timer_args = {};
    timer_args.callback = &WifiManager::retry_timer_cb;
    timer_args.arg = this;
    timer_args.name = "wifi_retry";
    err = esp_timer_create(&timer_args, &retry_timer_);
    if (err != ESP_OK) {
        return err;
    }

    err = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &WifiManager::event_handler, this);
    if (err == ESP_OK) {
        err = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &WifiManager::event_handler, this);
    }
    if (err != ESP_OK) {
        return err;
    }

    wifi_config_t wifi_cfg = {};
    std::strncpy(reinterpret_cast<char *>(wifi_cfg.sta.ssid), WIFI_SSID, sizeof(wifi_cfg.sta.ssid) - 1);
    std::strncpy(reinterpret_cast<char *>(wifi_cfg.sta.password), WIFI_PASSWORD, sizeof(wifi_cfg.sta.password) - 1);
    wifi_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err == ESP_OK) {
        err = esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
    }
    if (err == ESP_OK) {
        err = esp_wifi_start();  // WIFI_EVENT_STA_START triggers the first connect
    }
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "connecting to SSID \"%s\"", WIFI_SSID);
    }
    return err;
}

void WifiManager::event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    auto *self = static_cast<WifiManager *>(arg);
    if (base == WIFI_EVENT) {
        switch (id) {
        case WIFI_EVENT_STA_START:
            esp_wifi_connect();
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            self->on_disconnected();
            break;
        default:
            break;
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        const auto *event = static_cast<ip_event_got_ip_t *>(data);
        ESP_LOGI(TAG, "got IP " IPSTR, IP2STR(&event->ip_info.ip));
        self->connected_ = true;
        self->fast_retries_ = 0;
        esp_timer_stop(self->retry_timer_);
        if (self->on_got_ip_ != nullptr) {
            self->on_got_ip_(self->on_got_ip_arg_);
        }
    }
}

void WifiManager::on_disconnected()
{
    connected_ = false;
    if (fast_retries_ < config::WIFI_FAST_RETRIES) {
        fast_retries_++;
        ESP_LOGW(TAG, "disconnected, retry %u/%u", (unsigned)fast_retries_, (unsigned)config::WIFI_FAST_RETRIES);
        esp_wifi_connect();
    } else if (!esp_timer_is_active(retry_timer_)) {
        ESP_LOGW(TAG, "still offline, next attempt in %u s", (unsigned)(config::WIFI_SLOW_RETRY_MS / 1000));
        esp_timer_start_once(retry_timer_, (uint64_t)config::WIFI_SLOW_RETRY_MS * 1000);
    }
}

void WifiManager::retry_timer_cb(void *)
{
    esp_wifi_connect();  // a failure raises WIFI_EVENT_STA_DISCONNECTED, which re-arms the timer
}
