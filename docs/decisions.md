# Design Decisions

Format: "I chose X because Y; the alternative was Z, but because of W...".

## 1. FreeRTOS instead of a superloop
I chose FreeRTOS (Tasks + Queue + Mutex) because BME280 polling, formatted UART output and WDT feeding are independent responsibilities with different waiting disciplines. The alternative was a superloop, but with a shared I2C bus and the need to keep a fault in one part from starving the WDT feeder, it scales worse.

## 2. Four layers
I chose Periph / Logic / Transport-Output / Reliability because this satisfies the "at least 3 layers" requirement and gives the WDT, error recovery and mutexes a place of their own. The alternative was three layers, but reliability code would then be spread across the other layers.

## 3. ESP-IDF instead of Arduino
I chose ESP-IDF 5.5.3 (`framework = espidf`) because the project needs direct access to `esp_task_wdt`, `esp_timer`, the new I2C master driver and the ESP-IDF Wi-Fi/MQTT stack. `gptimer` and light sleep are not used: one-shot and periodic `esp_timer`s are enough for the 5 s cadence, and light sleep was judged not worthwhile (item 10). The alternative was Arduino, but the course examples would have to be ported to the IDF API anyway.

## 4. `i2c_master` driver
I chose the new `driver/i2c_master.h` because the legacy `i2c_cmd_link` API is deprecated, and `i2c_master` is already used in my earlier projects (module5). The alternative was the legacy driver from the assignment brief, but it is no longer being developed.

## 5. No `delay()`
I chose event-driven waiting (Queue, Task Notification, timers) plus an FSM because it is a project rule. The alternative was `vTaskDelay`, but it hides the event-driven model and complicates WDT logic.

## 6. `portMAX_DELAY` and the WDT
Mutexes are always taken with a finite timeout (`pdMS_TO_TICKS(200)`); `portMAX_DELAY` is allowed only in tasks that do not feed the WDT, with an explanatory comment. The one deliberate exception is `SensorTask::test_hang()`, which blocks T1 forever on purpose to demonstrate the watchdog (long button press). The WDT timeout is chosen with margin and decoupled from the measurement interval (the course documents set both to 5 s, which is a documentation error).

## 7. README: the most informative variant
The README merges all section lists from the course documents instead of using the minimal set.

## 8. MQTT telemetry (Task 4) and web page are extensions
MQTT is a mandatory best-effort channel alongside UART (reusing `module5`); the web page is optional. Neither counts toward the I2C+WDT reliability criterion.

## 9. Task watchdog: 10 s, resets the chip, application tasks only
I chose a 10 s task WDT timeout with `CONFIG_ESP_TASK_WDT_PANIC=y`, fed about once per second by T1, T2 and T3 from their timed queue waits, because the timeout must be much longer than the longest legal pause in the system but short enough to catch a real hang, and it must be independent of the 5 s measurement interval (the course documents set both to 5 s, so a normal cycle could trigger a reset). Without `PANIC` a timeout only prints a warning and the chip never resets, so `ESP_RST_TASK_WDT` would never be seen. The idle tasks are not monitored, so a timeout always points at one of T1-T3; T4 (network) is deliberately not subscribed, so a bad Wi-Fi link can never reset the logger. The alternative, feeding the WDT from T3 based on heartbeats of the other tasks, has a single feed point but hides which task hung. The settings live in `sdkconfig.defaults`; the generated `sdkconfig.<env>` is not tracked.

## 10. No Light Sleep
I chose not to implement Light Sleep between measurements, because the board is powered over USB, so saving energy has no practical meaning for this setup, and my earlier project (module6) showed that UART output together with Light Sleep needs workarounds (the UART and the console can be cut or garbled around a sleep), which would put the main deliverable, the UART log, at risk. The alternative was to add `esp_light_sleep_start()` after each cycle, but it would also conflict with the continuous Wi-Fi/MQTT link and with the event-driven timers that the design is built on. No checklist item requires it (it is only mentioned in the course Q&A).
