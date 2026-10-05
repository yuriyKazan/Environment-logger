#include "wifi_manager.h"

#include <cstring>

#include "config.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "secrets.h"

static const char *TAG = "wifi";

// Short names for the disconnect reasons that matter when a connection never comes up.
static const char *reason_name(uint8_t reason)
{
    switch (reason) {
    case WIFI_REASON_NO_AP_FOUND:             return "AP not found (switched off, out of range, wrong SSID, or a 5 GHz network: the ESP32-S3 is 2.4 GHz only)";
    case WIFI_REASON_AUTH_FAIL:               return "authentication failed (wrong password?)";
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:  return "4-way handshake timeout (wrong password?)";
    case WIFI_REASON_HANDSHAKE_TIMEOUT:       return "handshake timeout (wrong password?)";
    case WIFI_REASON_ASSOC_FAIL:              return "association failed";
    case WIFI_REASON_CONNECTION_FAIL:         return "connection failed";
    case WIFI_REASON_BEACON_TIMEOUT:          return "beacon timeout (signal lost)";
    case WIFI_REASON_ASSOC_LEAVE:             return "left by request";
    default:                                  return "other";
    }
}

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
            self->connect();
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            self->on_disconnected(static_cast<const wifi_event_sta_disconnected_t *>(data)->reason);
            break;
        default:
            break;
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        const auto *event = static_cast<ip_event_got_ip_t *>(data);
        ESP_LOGI(TAG, "got IP " IPSTR, IP2STR(&event->ip_info.ip));
        self->connected_ = true;
        self->fast_retries_ = 0;
        (void)esp_timer_stop(self->retry_timer_);  // not running is fine
        if (self->on_got_ip_ != nullptr) {
            self->on_got_ip_(self->on_got_ip_arg_);
        }
    }
}

void WifiManager::on_disconnected(uint8_t reason)
{
    connected_ = false;
    if (fast_retries_ < config::WIFI_FAST_RETRIES) {
        fast_retries_++;
        ESP_LOGW(TAG, "disconnected, reason %u: %s; retry %u/%u", (unsigned)reason, reason_name(reason),
                 (unsigned)fast_retries_, (unsigned)config::WIFI_FAST_RETRIES);
        connect();
    } else if (!esp_timer_is_active(retry_timer_)) {
        ESP_LOGW(TAG, "still offline (reason %u: %s), next attempt in %u s", (unsigned)reason, reason_name(reason),
                 (unsigned)(config::WIFI_SLOW_RETRY_MS / 1000));
        if (esp_timer_start_once(retry_timer_, (uint64_t)config::WIFI_SLOW_RETRY_MS * 1000) != ESP_OK) {
            ESP_LOGE(TAG, "retry timer could not start: no further attempts until the next event");
        }
    }
}

void WifiManager::connect()
{
    const esp_err_t err = esp_wifi_connect();
    if (err != ESP_OK && err != ESP_ERR_WIFI_CONN) {  // ESP_ERR_WIFI_CONN: already connecting
        ESP_LOGW(TAG, "esp_wifi_connect failed: %s", esp_err_to_name(err));
    }
}

void WifiManager::retry_timer_cb(void *arg)
{
    // A failed attempt raises WIFI_EVENT_STA_DISCONNECTED, which re-arms the timer.
    static_cast<WifiManager *>(arg)->connect();
}
