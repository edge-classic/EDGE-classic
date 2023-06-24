#pragma once

namespace primesynth {
// 64 bit fixed-point number
// higher 32 bit for integer part and lower 32 bit for fractional part
class FixedPoint {
public:
    FixedPoint() = delete;

    explicit FixedPoint(std::uint32_t integer) : raw_(static_cast<std::uint64_t>(integer) << 32) {}

    explicit FixedPoint(double value)
        : raw_((static_cast<std::uint64_t>(value) << 32) |
               static_cast<std::uint32_t>((value - static_cast<std::uint32_t>(value)) * (UINT32_MAX + 1.0))) {}

    std::uint32_t getIntegerPart() const {
        return raw_ >> 32;
    }

    double getFractionalPart() const {
        return (raw_ & UINT32_MAX) / (UINT32_MAX + 1.0);
    }

    double getReal() const {
        return getIntegerPart() + getFractionalPart();
    }

    std::uint32_t getRoundedInteger() const {
        return ((raw_ + INT32_MAX) + 1) >> 32;
    }

    FixedPoint& operator+=(const FixedPoint& b) {
        raw_ += b.raw_;
        return *this;
    }

    FixedPoint& operator-=(const FixedPoint& b) {
        raw_ -= b.raw_;
        return *this;
    }

private:
    std::uint64_t raw_;
};
}
