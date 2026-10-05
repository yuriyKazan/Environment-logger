# Defensive coding review

Review of the firmware against the three defensive-coding rules from the course material:

1. check every returned value (return codes, byte counts);
2. bound every retry loop (never retry forever);
3. put a timeout on every blocking wait.

## 1. Every wait has a timeout

| Where | Wait | Timeout |
|---|---|---|
| T1 `SensorTask::run` | `xQueueReceive` on its event queue | 1000 ms (`TASK_WAIT_MS`), then the watchdog is fed |
| T2 `UartLogTask::run` | `xQueueReceive` on Q_LOG | 1000 ms |
| T3 `SupervisorTask::run` | `xQueueReceive` on its queue | 500 ms (`SUPERVISOR_TICK_MS`), then LED_OK toggles |
| T4 `MqttTask::run` | `xQueueReceive` on Q_MQTT | 1000 ms |
| I2C bus | `I2cMutex::take` (always through `I2cLockGuard`) | 200 ms (`I2C_MUTEX_TIMEOUT_MS`) |
| I2C transfers | `i2c_master_transmit` / `_transmit_receive` | 100 ms (`I2C_XFER_TIMEOUT_MS`) |
| I2C probe | `i2c_master_probe` | 50 ms (`I2C_PROBE_TIMEOUT_MS`) |
| T1 waiting for T3 | one-shot recovery timer | 2000 ms (`RECOVERY_TIMEOUT_MS`) |
| Queue sends | `xQueueSend` | 0 (never blocks; a full queue is counted or the oldest entry is dropped) |

The only unbounded wait in the code is `SensorTask::test_hang()` (`xSemaphoreTake(..., portMAX_DELAY)`). It runs only after a long button press and exists to demonstrate the watchdog: T1 stops feeding it on purpose. It is documented in the code and in `decisions.md`.

Worst case between two watchdog feeds of T1: the queue wait (1 s) plus the longest event handler. The longest handler is the read after a conversion: mutex (200 ms) and up to four I2C transfers (4 x 100 ms), about 0.6 s. So about 1.6 s against a 10 s watchdog timeout.

## 2. Every retry is bounded

| Retry | Bound |
|---|---|
| BME280 measurement | `I2C_MAX_ATTEMPTS` = 3 per cycle, 100 ms apart (one-shot timer), then one bus reset request |
| Bus reset | one per failed cycle; T1 waits at most 2 s for the answer; the next cycle starts normally |
| Wi-Fi reconnect | `WIFI_FAST_RETRIES` = 5 immediate attempts, then one attempt every 30 s. This is deliberately unbounded in count: a logger that runs for hours must reconnect when the access point returns. It is driven by a one-shot timer and never blocks or delays another task |
| MQTT reconnect | handled by the esp-mqtt stack; T4 only publishes while connected |

## 3. Return values

- All driver and task methods return `esp_err_t` (or a status enum) and every caller checks it: I2C drivers, `I2cBus`, `I2cMutex`, `Led`, `Button::init`, task and timer creation, queue creation, `WifiManager`, `MqttPublisher`.
- The I2C drivers do not trust the transfer alone: BME280 chip ID is verified (`0x60`), the "measuring" status bit and the "skipped measurement" marker are checked, and a sample outside the physical range is rejected before it reaches the filter (`sensor_validation.h`, unit-tested).
- DS3231 time is classified (oscillator-stop flag, default date 2000-01-01, out-of-range fields) and the log marks untrusted time (`!TIME`).
- Calls whose result is intentionally ignored are marked `(void)` with the reason next to them: stopping a timer that may not be running (`esp_timer_stop`), and removing one old entry from a full queue (`xQueueReceive` on Q_MQTT).
- A failed `init()` of the BME280 or DS3231 releases its I2C device handle, so the call can be repeated.
- Nothing is left to `ESP_ERROR_CHECK` (which would abort); failures are logged and the system keeps running, with the UART log as the last thing to stop.

## 4. Shared state

| Data | Writers and readers | Protection |
|---|---|---|
| I2C bus | T1 (measurements), T3 (bus reset); before the tasks start, `app_main` | one mutex for the whole bus; a single lock, so there is no lock-ordering problem |
| `ErrorCounter` | T1 increments, T3 resets | `std::atomic`, saturating |
| Queues (Q_ISR/T1, Q_LOG, Q_MQTT, T3 queue) | producers and one consumer each | FreeRTOS queues; the button ISR uses `xQueueSendFromISR` |
| LEDs | T3 only | single owner |
| `TimeSource` state | T1 only after start | single owner |
| Wi-Fi and MQTT "connected" flags | event loop writes, T4 reads | `std::atomic<bool>` |
| Button debounce state | the ISR only (`volatile`) | single writer |

ISR code uses only ISR-safe calls (`xQueueSendFromISR`, `esp_timer_get_time`, `gpio_get_level`) and never takes a mutex.

## 5. Failure behaviour (summary)

| Failure | What happens | Evidence |
|---|---|---|
| BME280 does not answer | 3 attempts, bus reset, failed cycle logged with `ERR` + 1 and `!SENS`, LED_ERR on; recovers by itself | `docs/logs/phase4-i2c-recovery.txt` |
| A task hangs | the watchdog names it and resets the chip; the reset reason is logged after the restart | `docs/logs/phase4-wdt.txt` |
| RTC time invalid | internal clock, `!TIME` in the log | `docs/logs/phase3-rtc.txt` |
| Wi-Fi or broker lost | T4 drops entries, UART log unaffected, reconnects by itself | `docs/logs/phase3-mqtt.txt` |
| Implausible sensor value | rejected, counted as a failed attempt | unit tests (`test_sensor_validation`) |

## 6. Known limitations

- A button tap shorter than the 50 ms debounce quiet time loses its release event; the long-press timer then counts it as a short press after 3 s.
- A bus reset cannot revive a device that is permanently dead or unpowered; the cycle is logged as failed every time until it answers again.
- Stack sizes (4096 bytes per task) were chosen with margin and verified by running, not by measuring the high-water mark.
