# Changelog

All notable changes to this project are recorded here. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project uses [Semantic Versioning](https://semver.org/).

## [1.0.0] - 2026-10-06

First complete version: the logger measures, timestamps, logs over UART, publishes over MQTT and recovers from an I2C failure and from a hung task. Evidence for each item is in `docs/` ([self-check](docs/self-check.md)).

### Added

- **Measurement pipeline.**
  - BME280 driver (forced mode, calibration, compensation) and DS3231 driver (time read/set with validity classification) on one shared I2C bus at 400 kHz, using the new `driver/i2c_master.h`.
  - T1 sensor task, an event-driven state machine (Idle / WaitConversion / WaitRetry / Recovery) with one-shot timers instead of any `delay()`.
  - EMA filter (`alpha` = 0.2, chosen with an experiment, `docs/ema-experiment.md`), sensor plausibility check (range and NaN), and a `TimeSource` that falls back to the internal clock and marks the entry `!TIME` when the RTC is not valid.
  - Log line format `[HH:MM:SS] T:.. H:..% P:.. ERR:..` with `!TIME` and `!SENS` markers.
- **Four layers and tasks.** Periph, Logic, Transport/Output and Reliability layers; tasks T1 sensor, T2 uart_log (the only writer of log lines), T3 supervisor, T4 mqtt, and a diagnostics task. Queues between tasks with a documented overflow policy per queue, and one mutex (200 ms timeout, RAII guard) for the whole I2C bus.
- **I2C recovery.** Three attempts 100 ms apart, then a bus reset requested from T3 through `i2c_master_bus_reset()`, LED_ERR while it is handled, and a saturating error counter that a short button press clears.
- **Task watchdog.** 10 s timeout with panic on timeout, T1, T2 and T3 subscribed, fed from every timed queue wait; the reset reason is logged at boot (watchdog, panic and brownout are flagged). A long button press hangs T1 on purpose to demonstrate it.
- **Wi-Fi and MQTT telemetry (T4).** Best-effort publishing of `temp`, `hum`, `pres` and `err` to a public broker; five immediate reconnects, then one attempt every 30 s without a limit. The UART log does not depend on it.
- **Documentation.** README with block diagram, state machine, three sequence diagrams and a class view; architecture, decisions, hardware, defensive-coding review, measurements, logic analyzer captures, and a self-check against the course checklist (30 of 42 items, the rest justified or pending the live demo).
- **Hardware design.** KiCad schematic (module level, ERC with 0 errors and 0 warnings) exported to PDF and PNG.
- **Evidence.** Logs of every scenario in `docs/logs/`, including a 10 h 18 min run with no reset, `ERR:0` throughout and a flat heap; logic analyzer captures with a decoding script (`tools/analyze_i2c.py`); `tools/summarize_long_run.py` that produces the redacted long-run summary from a raw capture.
- **Unit tests** for the pure logic (`Ema`, `ErrorCounter`, `TimingStats`, sensor validation, log formatting): 37 tests, run on the PC with `pio test -e native`.

### Changed

- Course documents set both the watchdog timeout and the measurement interval to 5 s; here the watchdog feed is decoupled from the interval and the timeout is 10 s.
- The photoresistor from the original brief was removed (decision of 2026-09-29), so the log has no `LUX` field and checklist item 2.1 is not applicable.
- The button is a 3-pin module with a pull-down, pressed = high (the first schematic assumed an active-low switch); schematic, hardware doc and `BUTTON_PRESSED_LEVEL` were updated to match the build.

### Fixed

- `FAULT_STALL_UART` (the fault injection that stops the UART task for 90 s) was committed as `true`; it is now `false`, so a fresh build logs normally.
- Button: a release bounce no longer produces a second press (quiet-time debounce on both edges); a failed `init()` of the BME280 or DS3231 now releases its I2C device handle.

### Known limitations

- The MQTT broker is public and unauthenticated, and delivery is QoS 0.
- A button tap shorter than the 50 ms debounce loses its release event.
- Light Sleep was deliberately not implemented (USB power, conflicts with the Wi-Fi link and the UART log); DMA was measured and found pointless; no PCB was made.
- A non-rechargeable CR2032 is fitted in a DS3231 module with a charging circuit: fine for a short demo, not for long unattended use.

Full list: README section 13.

[1.0.0]: https://github.com/yuriyKazan/Environment-logger/releases/tag/v1.0.0
