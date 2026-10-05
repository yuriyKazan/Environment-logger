#pragma once
// Logic layer: text format of one log line (pure function, unit-tested on the PC).
//
//   [HH:MM:SS] T:23.4 H:48% P:1013 ERR:0
//
// Time is the UTC time of day of the timestamp. Two markers are appended when needed:
//   " !TIME"  the timestamp does not come from a valid RTC
//   " !SENS"  the sensor values are not from a successful read

#include <cstddef>
#include "data_types.h"

// Writes a NUL-terminated line (without newline) into buf. Returns the number of characters
// written, or 0 if the buffer is too small or invalid.
size_t format_log_line(const LogEntry &entry, char *buf, size_t size);
