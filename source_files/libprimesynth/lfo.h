#pragma once
#include "conversion.h"

namespace primesynth {
class LFO {
public:
    LFO(double outputRate, unsigned int interval)
        : outputRate_(outputRate), interval_(interval), steps_(0), delay_(0), delta_(0.0), value_(0.0), up_(true) {}

    double getValue() const {
        return value_;
    }

    void setDelay(double delay) {
        delay_ = static_cast<unsigned int>(outputRate_ * conv::timecentToSecond(delay));
    }

    void setFrequency(double freq) {
        delta_ = 4.0 * interval_ * conv::absoluteCentToHertz(freq) / outputRate_;
    }

    void update() {
        if (steps_ <= delay_) {
            ++steps_;
            return;
        }
        if (up_) {
            value_ += delta_;
            if (value_ > 1.0) {
                value_ = 2.0 - value_;
                up_ = false;
            }
        } else {
            value_ -= delta_;
            if (value_ < -1.0) {
                value_ = -2.0 - value_;
                up_ = true;
            }
        }
    }

private:
    const double outputRate_;
    const unsigned int interval_;
    unsigned int steps_, delay_;
    double delta_, value_;
    bool up_;
};
}
