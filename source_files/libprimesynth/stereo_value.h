#pragma once

namespace primesynth {
struct StereoValue {
    double left, right;

    StereoValue() = delete;

    StereoValue operator*(double b) const;
    StereoValue& operator+=(const StereoValue& b);
};

StereoValue operator*(double a, const StereoValue& b);
}
