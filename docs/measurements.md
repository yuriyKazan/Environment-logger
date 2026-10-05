# Measurements

All numbers come from the firmware's own diagnostics (`DiagnosticsTask`, and the timing log of T1) running on the real board, ESP32-S3-DevKitM-1, ESP-IDF 5.5.3, one measurement every 5 s. Raw logs: [`logs/phase6-diagnostics.txt`](logs/phase6-diagnostics.txt) and [`logs/phase6-queue-overflow.txt`](logs/phase6-queue-overflow.txt).

## Summary

| Quantity | Value | Where it comes from |
|---|---|---|
| Measurement cycle, tick to published entry | 11.9 / 12.0 / 12.4 ms (min / avg / max) | T1 timing, 12 cycles |
| ... of which waiting for the BME280 conversion | 10 ms (one-shot timer, the CPU is idle) | `config::BME280_CONVERSION_US` |
| I2C, start of a conversion (mutex wait + 2 register writes) | 529 / 534 / 540 us | T1 timing |
| I2C, read of BME280 and DS3231 (mutex wait + 4 transfers) | 1204 / 1230 / 1343 us | T1 timing |
| Time the I2C bus is busy per cycle | about 1.8 ms of 5000 ms (0.035 %) | sum of the two lines above |
| CPU load, both cores averaged | 0.7 % (target: below 70 %) | diagnostics, run-time counters |
| Free heap / lowest since boot | 234.8 KB / 226.6 KB after 77 minutes | diagnostics |
| Number of tasks | 15 (4 application tasks, the rest are ESP-IDF system tasks) | diagnostics |

In the second run (fault injection test, 60 s after boot) the numbers were the same: start 521/523/528 us, read 1200/1225/1454 us, one cycle took up to 21.6 ms (a single outlier, probably Wi-Fi activity), CPU load 0.9 %.

## Stack usage (high-water mark)

Free stack is the smallest amount that was free since the task started, so used = size - free.

| Task | Stack size | Free (minimum) | Used | Verdict |
|---|---|---|---|---|
| T1_sensor | 4096 B | 1828 B | 2268 B | enough margin |
| T2_uart | 4096 B | 1876 B | 2220 B | enough margin |
| T3_supervisor | 4096 B | 3156 B | 940 B | could be 2048 B |
| T4_mqtt | 4096 B | 2000 B | 2096 B | enough margin |
| diag | 4096 B | 1760 B | 2336 B | enough margin |

The rule of thumb used here is to enlarge a stack that has less than 128 B free: none of the application tasks is close. The lowest margins in the system belong to ESP-IDF tasks that are not ours: `ipc0` (396 B free), `sys_evt` (576 B free; our Wi-Fi event handler runs in it) and `IDLE0`/`IDLE1` (740-812 B). They are above the threshold and stable, but `sys_evt` is the one to watch if more work is ever added to the Wi-Fi/MQTT callbacks.

## Long run

The snapshot above was taken 4627 s (77 minutes) after the last boot, with `ERR:0` in the log lines around it, so the logger ran for more than an hour without a reset or an error. Heap use was steady: the lowest free heap of 226.6 KB after 77 minutes is only 2.6 KB below the value 60 s after boot (229.3 KB), which is the Wi-Fi/TLS start-up peak, not a leak. A complete long-run log file is still to be captured.

## Is DMA appropriate?

No. One cycle moves about 20 bytes over I2C (BME280: 2 + 2 bytes written, 1 + 8 bytes read; DS3231: 1 + 7 + 1 + 1 bytes). The whole I2C phase takes 1.8 ms per 5 s and the CPU is busy 0.7 % of the time overall. DMA removes per-byte CPU work, but the measured time is dominated by per-transaction driver overhead, not by moving bytes, so DMA would save nothing measurable and add complexity. It would make sense for large buffers (for example a display frame buffer), which this project does not have. The checklist item "DMA (if appropriate)" is therefore not ticked, on purpose.

## Copies of data

The only data that moves between tasks is one `LogEntry` (about 32 bytes) per cycle:

- T1 builds it once and the queues copy it by value into Q_LOG (to T2) and Q_MQTT (to T4); each consumer reads it directly from the queue into a local variable;
- T2 formats it once into a 96-byte line buffer and prints it;
- T4 formats the numbers into a 24-byte payload buffer per topic.

A copy by value is what a FreeRTOS queue does, and it keeps the tasks independent (no shared buffer, no locking). At 32 bytes every 5 s this is negligible, so no redundant copies were removed.

## Queue overflow (fault injection)

With `config::FAULT_STALL_UART = true`, T2 stops reading Q_LOG for 90 s. The policy of Q_LOG is to drop the newest entry and count it:

- the queue (8 places) filled up and the first drop was logged at 60 s; ten entries were dropped in total;
- T1 was never blocked: the warnings arrived exactly every 5 s;
- when T2 resumed, it printed the oldest buffered entries first;
- the watchdog did not fire, because T2 kept feeding it.

Q_MQTT has the opposite policy (drop the oldest entry) so the broker always gets the freshest data.

## Not measured

- Current consumption: the board is powered over USB and Light Sleep was not implemented, so there is nothing to compare (see `decisions.md`, item 10).
