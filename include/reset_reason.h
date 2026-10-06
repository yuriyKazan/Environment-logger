#pragma once
// Reliability layer: why did the chip (re)start? Logged once at boot so that a previous hang,
// deadlock or crash is visible after the restart.

#include "esp_system.h"

// Short human-readable name of a reset reason.
const char *reset_reason_name(esp_reset_reason_t reason);

// True for watchdog resets (task WDT, interrupt WDT, other WDT).
bool reset_reason_is_watchdog(esp_reset_reason_t reason);

// Read esp_reset_reason() and log it: ERROR for a watchdog reset, WARNING for a panic, brownout or
// CPU lockup, INFO for a normal start. Returns the reason.
esp_reset_reason_t log_reset_reason();
