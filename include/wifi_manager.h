#pragma once
// Periph layer: Wi-Fi station. Event driven, never blocks.
//
// After a disconnect it retries immediately WIFI_FAST_RETRIES times, then once every
// WIFI_SLOW_RETRY_MS (one-shot timer) for as long as needed: a logger that runs for hours must
// reconnect when the access point comes back. Credentials come from include/secrets.h.

#include <cstdint>
#include "esp_err.h"
#include "esp_event.h"
#include "esp_timer.h"

class WifiManager {
public:
    using ConnectedCb = void (*)(void *arg);

    WifiManager() = default;
    WifiManager(const WifiManager &) = delete;
    WifiManager &operator=(const WifiManager &) = delete;

    // Bring up the station and start connecting. `on_got_ip` is called (from the event loop task)
    // every time an IP address is obtained, including after a reconnect.
    esp_err_t init(ConnectedCb on_got_ip, void *arg);

    bool connected() const { return connected_; }

private:
    static void event_handler(void *arg, esp_event_base_t base, int32_t id, void *data);
    static void retry_timer_cb(void *arg);
    void on_disconnected();

    volatile bool connected_ = false;
    uint8_t fast_retries_ = 0;
    esp_timer_handle_t retry_timer_ = nullptr;
    ConnectedCb on_got_ip_ = nullptr;
    void *on_got_ip_arg_ = nullptr;
};
