#pragma once
// Reliability layer: running min / average / max of a duration, in microseconds.
// Used to measure how long the critical sections take (I2C transactions, a whole measurement
// cycle). Header-only and free of ESP-IDF includes, so it is unit-tested on the PC (env:native).

#include <cstdint>

class TimingStats {
public:
    void add(uint32_t us)
    {
        if (count_ == 0 || us < min_) {
            min_ = us;
        }
        if (us > max_) {
            max_ = us;
        }
        sum_ += us;
        count_++;
    }

    uint32_t count() const { return count_; }
    uint32_t min() const { return count_ == 0 ? 0 : min_; }
    uint32_t max() const { return max_; }
    uint32_t avg() const { return count_ == 0 ? 0 : static_cast<uint32_t>(sum_ / count_); }

    void reset()
    {
        count_ = 0;
        min_ = 0;
        max_ = 0;
        sum_ = 0;
    }

private:
    uint32_t count_ = 0;
    uint32_t min_ = 0;
    uint32_t max_ = 0;
    uint64_t sum_ = 0;
};
