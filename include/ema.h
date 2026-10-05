#pragma once
// Logic layer: exponential moving average, filtered = alpha * raw + (1 - alpha) * filtered.
//
// Header-only and free of ESP-IDF includes, so it is unit-tested on the PC (env:native).
//
// Choosing alpha: for a sample period T the time constant is tau = -T / ln(1 - alpha).
// T = 5 s and alpha = 0.2 give tau = 22 s: a real change in the room is followed within
// about a minute, while sensor noise is suppressed. alpha = 0.5 follows almost every wiggle,
// alpha = 0.05 (tau = 97 s) is too slow for a logger sampling every 5 s.

#include <cmath>

class Ema {
public:
    // alpha must be in (0, 1]. An invalid value disables filtering (alpha = 1, pass-through).
    explicit Ema(float alpha) : alpha_((alpha > 0.0f && alpha <= 1.0f) ? alpha : 1.0f) {}

    // Feed one sample and return the filtered value. The first sample initialises the filter.
    // A non-finite sample (NaN, inf) is ignored and the current value is returned.
    float update(float raw)
    {
        if (!std::isfinite(raw)) {
            return value_;
        }
        if (!primed_) {
            value_ = raw;
            primed_ = true;
        } else {
            value_ = alpha_ * raw + (1.0f - alpha_) * value_;
        }
        return value_;
    }

    float value() const { return value_; }
    bool primed() const { return primed_; }
    float alpha() const { return alpha_; }
    void reset()
    {
        value_ = 0.0f;
        primed_ = false;
    }

private:
    float alpha_;
    float value_ = 0.0f;
    bool primed_ = false;
};
