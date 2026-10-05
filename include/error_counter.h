#pragma once
// Reliability layer: the error counter shown in every log line (ERR:<n>).
//
// Saturating: it stops at UINT32_MAX instead of wrapping around to 0, so a long-running logger
// never reports "no errors" because the counter overflowed. Atomic, so T1 can increment it
// while T3 resets it from a button press, without a mutex. Header-only and free of ESP-IDF
// includes, so it is unit-tested on the PC (env:native).

#include <atomic>
#include <cstdint>

class ErrorCounter {
public:
    explicit ErrorCounter(uint32_t initial = 0) : value_(initial) {}

    // Add one error (saturating) and return the new value.
    uint32_t increment()
    {
        uint32_t current = value_.load();
        while (current != UINT32_MAX && !value_.compare_exchange_weak(current, current + 1)) {
            // `current` was refreshed by compare_exchange_weak; try again
        }
        return current == UINT32_MAX ? current : current + 1;
    }

    uint32_t value() const { return value_.load(); }
    void reset() { value_.store(0); }

private:
    std::atomic<uint32_t> value_;
};
