#include "conversion.h"
#include "modulator.h"
#include <stdexcept>

namespace primesynth {
Modulator::Modulator(const sf::ModList& param) : param_(param), source_(0.0), amountSource_(1.0), value_(0.0) {}

sf::Generator Modulator::getDestination() const {
    return param_.modDestOper;
}

std::int16_t Modulator::getAmount() const {
    return param_.modAmount;
}

bool Modulator::canBeNegative() const {
    if (param_.modTransOper == sf::Transform::AbsoluteValue || param_.modAmount == 0) {
        return false;
    }

    if (param_.modAmount > 0) {
        const bool noSrc = param_.modSrcOper.palette == sf::ControllerPalette::General &&
                           param_.modSrcOper.index.general == sf::GeneralController::NoController;
        const bool uniSrc = param_.modSrcOper.polarity == sf::SourcePolarity::Unipolar;
        const bool noAmt = param_.modAmtSrcOper.palette == sf::ControllerPalette::General &&
                           param_.modAmtSrcOper.index.general == sf::GeneralController::NoController;
        const bool uniAmt = param_.modAmtSrcOper.polarity == sf::SourcePolarity::Unipolar;

        if ((uniSrc && uniAmt) || (uniSrc && noAmt) || (noSrc && uniAmt) || (noSrc && noAmt)) {
            return false;
        }
    }

    return true;
}

double Modulator::getValue() const {
    return value_;
}

double map(double value, const sf::Modulator& mod) {
    if (mod.palette == sf::ControllerPalette::General && mod.index.general == sf::GeneralController::PitchWheel) {
        value /= 1 << 14;
    } else {
        value /= 1 << 7;
    }

    if (mod.type == sf::SourceType::Switch) {
        const double off = mod.polarity == sf::SourcePolarity::Unipolar ? 0.0 : -1.0;
        const double x = mod.direction == sf::SourceDirection::Positive ? value : 1.0 - value;
        return x >= 0.5 ? 1.0 : off;
    } else if (mod.polarity == sf::SourcePolarity::Unipolar) {
        const double x = mod.direction == sf::SourceDirection::Positive ? value : 1.0 - value;
        switch (mod.type) {
        case sf::SourceType::Linear:
            return x;
        case sf::SourceType::Concave:
            return conv::concave(x);
        case sf::SourceType::Convex:
            return conv::convex(x);
        }
    } else {
        const int dir = mod.direction == sf::SourceDirection::Positive ? 1 : -1;
        const int sign = value > 0.5 ? 1 : -1;
        const double x = 2.0 * value - 1.0;
        switch (mod.type) {
        case sf::SourceType::Linear:
            return dir * x;
        case sf::SourceType::Concave:
            return sign * dir * conv::concave(sign * x);
        case sf::SourceType::Convex:
            return sign * dir * conv::convex(sign * x);
        }
    }
    throw std::runtime_error("unknown modulator controller type");
}

bool Modulator::updateSFController(sf::GeneralController controller, double value) {
    bool updated = false;
    if (param_.modSrcOper.palette == sf::ControllerPalette::General && controller == param_.modSrcOper.index.general) {
        source_ = map(value, param_.modSrcOper);
        updated = true;
    }
    if (param_.modAmtSrcOper.palette == sf::ControllerPalette::General &&
        controller == param_.modAmtSrcOper.index.general) {
        amountSource_ = map(value, param_.modAmtSrcOper);
        updated = true;
    }

    if (updated) {
        calculateValue();
    }
    return updated;
}

bool Modulator::updateMIDIController(std::uint8_t controller, std::uint8_t value) {
    bool updated = false;
    if (param_.modSrcOper.palette == sf::ControllerPalette::MIDI && controller == param_.modSrcOper.index.midi) {
        source_ = map(value, param_.modSrcOper);
        updated = true;
    }
    if (param_.modAmtSrcOper.palette == sf::ControllerPalette::MIDI && controller == param_.modAmtSrcOper.index.midi) {
        amountSource_ = map(value, param_.modAmtSrcOper);
        updated = true;
    }

    if (updated) {
        calculateValue();
    }
    return updated;
}

double transform(double value, sf::Transform transform) {
    switch (transform) {
    case sf::Transform::Linear:
        return value;
    case sf::Transform::AbsoluteValue:
        return std::abs(value);
    }
    throw std::invalid_argument("unknown transform");
}

void Modulator::calculateValue() {
    value_ = transform(param_.modAmount * source_ * amountSource_, param_.modTransOper);
}
}
