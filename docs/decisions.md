# Design Decisions

Format: "I chose X because Y; the alternative was Z, but because of W...".

## 1. FreeRTOS instead of a superloop
I chose FreeRTOS (Tasks + Queue + Mutex) because BME280 polling, formatted UART output and WDT feeding are independent responsibilities with different waiting disciplines. The alternative was a superloop, but with a shared I2C bus and the need to keep a fault in one part from starving the WDT feeder, it scales worse.

## 2. Four layers
I chose Periph / Logic / Transport-Output / Reliability because this satisfies the "at least 3 layers" requirement and gives the WDT, error recovery and mutexes a place of their own. The alternative was three layers, but reliability code would then be spread across the other layers.

## 3. ESP-IDF instead of Arduino
I chose ESP-IDF 5.5.3 (`framework = espidf`) because the project needs direct access to `esp_task_wdt`, `gptimer`, the I2C driver and light sleep. The alternative was Arduino, but the course examples would have to be ported to the IDF API anyway.

## 4. `i2c_master` driver
I chose the new `driver/i2c_master.h` because the legacy `i2c_cmd_link` API is deprecated, and `i2c_master` is already used in my earlier projects (module5). The alternative was the legacy driver from the assignment brief, but it is no longer being developed.

## 5. No `delay()`
I chose event-driven waiting (Queue, Task Notification, timers) plus an FSM because it is a project rule. The alternative was `vTaskDelay`, but it hides the event-driven model and complicates WDT logic.

## 6. `portMAX_DELAY` and the WDT
Mutexes are always taken with a finite timeout (`pdMS_TO_TICKS(200)`); `portMAX_DELAY` is allowed only in tasks that do not feed the WDT, with an explanatory comment. The WDT timeout is chosen with margin and decoupled from the measurement interval (the course documents set both to 5 s, which is a documentation error).

## 7. README: the most informative variant
The README merges all section lists from the course documents instead of using the minimal set.

## 8. MQTT telemetry (Task 4) and web page are extensions
MQTT is a mandatory best-effort channel alongside UART (reusing `module5`); the web page is optional. Neither counts toward the I2C+WDT reliability criterion.
