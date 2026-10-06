# Self-check against the course checklist

The course checklist has 42 items. An item is ticked only when an artifact exists (code, log, measurement, document); items that do not fit this project are **not ticked** and carry a reason instead of being forced. The instructor said the 80 % threshold is not rigid and that adding a component only to cover an item would count as a weakness, so the "need" column says what the project itself requires, not which line it satisfies.

**Result: 30 of 42 ticked (71 %).** The four demonstration items (section 9) are ticked at the defence, after the live demo, which gives 34 of 42 (81 %). The remaining eight items are justified below.

## 1. Architecture

| # | Item | Done | Evidence | Need it covers |
|---|---|---|---|---|
| 1.1 | Superloop or RTOS | yes | FreeRTOS tasks T1-T4, `src/logic/sensor_task.cpp`, `src/transport/uart_log_task.cpp`; [`phase3-pipeline.txt`](logs/phase3-pipeline.txt) | Sensor, output, supervision and network are independent and wait differently; one fault must not stop the rest |
| 1.2 | No blocking `delay()` in the main logic | yes | no `delay`, `vTaskDelay` or busy-wait in `src/` and `include/` (waits are queue waits with timeouts and one-shot timers); [`defensive-review.md`](defensive-review.md) | An event-driven pause cannot hide a hang from the watchdog |
| 1.3 | State machine | yes | T1 FSM (Idle / WaitConversion / WaitRetry / Recovery); [`architecture.md`](architecture.md) section 4 | Conversion wait, retry and recovery are states, not blocking pauses |
| 1.4 | Architecture block diagram | yes | [`architecture.md`](architecture.md) section 10, README section 5 | Explain the system quickly |
| 1.5 | Logic split into modules | yes | `src/periph`, `src/logic`, `src/transport`, `src/reliability`, `include/config.h`; 37 unit tests on the PC | One driver can change without touching the rest |

## 2. Peripherals and interfaces

| # | Item | Done | Evidence | Need it covers |
|---|---|---|---|---|
| 2.1 | At least one analog subsystem (ADC / DAC / PWM / I2S / PDM) | **no** | The photoresistor was removed on 2026-09-29: a temperature, humidity and pressure logger has no need to measure light. Reason: [`decisions.md`](decisions.md) | Adding a sensor only for this item would make the design less coherent |
| 2.2 | At least one digital interface | yes | I2C with two devices on one bus, plus UART and Wi-Fi; [`phase2-bringup.txt`](logs/phase2-bringup.txt), [`logic-analyzer.md`](logic-analyzer.md) | BME280 and DS3231 share two wires |
| 2.3 | Interrupts | yes | Button ISR on GPIO6 posting to a queue with `xQueueSendFromISR`; `src/periph/button.cpp`; [`phase4-button.txt`](logs/phase4-button.txt) | Button gestures on demand: clear the error counter, trigger the watchdog test |
| 2.4 | Asynchronous data transfer (buffer / queue) | yes | T1 → `Q_LOG` → T2, T1 → `Q_MQTT` → T4; [`phase3-pipeline.txt`](logs/phase3-pipeline.txt) | The UART log does not depend on I2C timing or on the network |
| 2.5 | Timers or hardware events | yes | `esp_timer`: 5 s tick, 10 ms conversion, 100 ms retry, 2 s recovery, 3 s long press | Fixed 5 s period without drift; non-blocking waits |

## 3. RTOS

| # | Item | Done | Evidence | Need it covers |
|---|---|---|---|---|
| 3.1 | At least 2 tasks | yes | T1 sensor, T2 uart_log, T3 supervisor, T4 mqtt, diagnostics; [`architecture.md`](architecture.md) section 2 | Separation of responsibilities |
| 3.2 | Queue | yes | T1 queue, T3 queue, `Q_LOG`, `Q_MQTT`; overflow policy tested in [`phase6-queue-overflow.txt`](logs/phase6-queue-overflow.txt) | Task-to-task transfer without shared buffers |
| 3.3 | Mutex or other synchronization | yes | `I2cMutex` for the whole bus, 200 ms timeout, RAII guard; `src/reliability/i2c_mutex.cpp` | T1 (measurements) and T3 (bus reset) both reach the bus |
| 3.4 | No race conditions | yes | Review of every shared object in [`defensive-review.md`](defensive-review.md) section 4; 10 h run without a fault. There is no formal proof, only review and long runs | The logger runs for hours; a race would show up rarely and be hard to reproduce |

## 4. Control and algorithms

| # | Item | Done | Evidence | Need it covers |
|---|---|---|---|---|
| 4.1 | Feedback | yes | I2C error → 3 retries → bus reset by T3 → LED_ERR and `ERR` in the log; [`phase4-i2c-recovery.txt`](logs/phase4-i2c-recovery.txt) | The bus recovers on its own and the operator can see it |
| 4.2 | PID or another control algorithm | yes | EMA filter, `alpha` = 0.2, chosen with an experiment: [`ema-experiment.md`](ema-experiment.md), `include/ema.h`, `test/test_ema` | Smooth short disturbances and single outliers |
| 4.3 | Parameters can be configured | yes | All constants in `include/config.h` | Change interval, `alpha`, timeouts or pins in one place |
| 4.4 | Boundary conditions (overflow, saturation) | yes | Saturating `ErrorCounter`, sensor range and NaN checks, queue overflow policies; `include/error_counter.h`, `include/sensor_validation.h`, unit tests | The logger must not record a false value or wrap a counter |

## 5. Reliability

| # | Item | Done | Evidence | Need it covers |
|---|---|---|---|---|
| 5.1 | Watchdog | yes | Task WDT 10 s with panic, T1-T3 subscribed; triggered three times on purpose; `sdkconfig.defaults`, [`phase4-wdt.txt`](logs/phase4-wdt.txt) | A hung task must end in a restart, not in silence |
| 5.2 | Peripheral error handling | yes | Retries, bus reset, recovery; [`phase4-i2c-recovery.txt`](logs/phase4-i2c-recovery.txt), logic analyzer capture, [`defensive-review.md`](defensive-review.md) | A wire that comes loose |
| 5.3 | Invalid data is checked | yes | DS3231 validity (oscillator-stop flag, `2000-01-01`, field ranges) and `!TIME` / `!SENS` markers; [`phase3-rtc.txt`](logs/phase3-rtc.txt) | A dead RTC battery gives `2000-01-01`; a made-up time does more harm than a missing one |
| 5.4 | No hang on bad input | yes | SDA disconnected, a task hung on purpose, Wi-Fi lost; [`phase4-i2c-recovery.txt`](logs/phase4-i2c-recovery.txt), [`phase4-wdt.txt`](logs/phase4-wdt.txt), [`phase3-mqtt.txt`](logs/phase3-mqtt.txt) | Unattended operation |

## 6. Performance

| # | Item | Done | Evidence | Need it covers |
|---|---|---|---|---|
| 6.1 | Critical sections timed | yes | `esp_timer_get_time`: I2C start 534 µs, read 1230 µs, cycle 12 ms; confirmed on the wire; [`measurements.md`](measurements.md), [`logic-analyzer.md`](logic-analyzer.md) | Show the cycle fits into 5 s with a large margin |
| 6.2 | DMA (if appropriate) | **no, on purpose** | About 20 bytes of I2C per cycle, the bus is busy 0.035 % of the time, CPU 0.7 %; the time is driver overhead, not byte transfer ([`measurements.md`](measurements.md), "Is DMA appropriate?") | Measured: DMA would gain nothing here |
| 6.3 | No redundant data copies | yes | Only a 32-byte `LogEntry` is copied by value through queues once per 5 s; [`measurements.md`](measurements.md) | Confirm that passing by value costs nothing noticeable |
| 6.4 | CPU not overloaded (above 70 %) | yes | 0.5-0.8 % over a 10 h run (mean 0.63 %); [`phase7-long-run.txt`](logs/phase7-long-run.txt) | Headroom for the other tasks and the network |

## 7. PCB

| # | Item | Done | Evidence | Need it covers |
|---|---|---|---|---|
| 7.1 | Schematic | yes | `hardware/kicad/env-logger.kicad_sch`, exported to [`schematic/schematic.pdf`](schematic/schematic.pdf) and `schematic.png` | Document the wiring of the modules and the pins |
| 7.2 | 2-layer PCB routed | **no** | Skipped on purpose: the logger is built from ready-made modules on a breadboard | A PCB is not needed to meet the project's goal |
| 7.3 | Power filtering | **no** | Same as 7.2; no custom board, so nothing to filter on it | n/a |
| 7.4 | Power and logic separated | **no** | Same as 7.2; one 3.3 V rail from the board | n/a |
| 7.5 | High-speed signal routing | **no** | Same as 7.2; the fastest signal is I2C at 400 kHz | n/a |
| 7.6 | Test points | **no** | Same as 7.2; SDA and SCL were probed with a logic analyzer on the breadboard | n/a |
| 7.7 | Board made and tested | **no** | Same as 7.2 | n/a |

## 8. Documentation

| # | Item | Done | Evidence | Need it covers |
|---|---|---|---|---|
| 8.1 | README with the task description | yes | [`README.md`](../README.md) section 1 | What, why, how to run, who made it, in 30 seconds |
| 8.2 | Block diagram | yes | README section 5, [`architecture.md`](architecture.md) sections 10-11 | A clear picture of the system without reading the code |
| 8.3 | Architecture description | yes | [`architecture.md`](architecture.md), [`decisions.md`](decisions.md) | Explain the choice of Task, Queue and Mutex |
| 8.4 | Run instructions | yes | README section 10 | Reproducible by someone else |
| 8.5 | Explanation of the interfaces | yes | README section 6, [`hardware.md`](hardware.md) | Show that the interface settings were chosen deliberately |

## 9. Demonstration

| # | Item | Done | Evidence | Need it covers |
|---|---|---|---|---|
| 9.1 | Runs stably for 5 minutes or more | at the defence | Prepared evidence: a 10 h 18 min run with no reset ([`phase7-long-run.txt`](logs/phase7-long-run.txt)) | |
| 9.2 | Boundary modes demonstrated | at the defence | Prepared: SDA disconnect, long press (watchdog), Wi-Fi loss | |
| 9.3 | Architecture explained | at the defence | | |
| 9.4 | PCB decision explained | at the defence | The PCB items are skipped on purpose (section 7) | |

## Not ticked, and why

- **2.1:** no analog quantity belongs in this logger; see the decision above.
- **6.2:** measured and found pointless, so the item is deliberately left unticked.
- **7.2-7.7:** the project uses ready-made modules; only the schematic (7.1) was made.
- **9.1-9.4:** these are ticked at the live demonstration, not before.

## What the checklist does not cover

The MQTT telemetry (T4) and the diagnostics task are extras; they are not counted in the items above, and the MQTT path is explicitly outside the I2C + WDT reliability guarantees (see README section 8).
