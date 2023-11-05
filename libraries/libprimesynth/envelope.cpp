#include "conversion.h"
#include "envelope.h"
#include <stdexcept>

namespace primesynth {
Envelope::Envelope(double outputRate, unsigned int interval)
    : effectiveOutputRate_(outputRate / interval), params_(), phase_(Phase::Delay), phaseSteps_(0), value_(1.0) {}

Envelope::Phase Envelope::getPhase() const {
    return phase_;
}

double Envelope::getValue() const {
    return value_;
}

void Envelope::setParameter(Phase phase, double param) {
    if (phase == Phase::Sustain) {
        params_.at(static_cast<std::size_t>(Phase::Sustain)) = 1.0 - 0.001 * param;
    } else if (phase < Phase::Finished) {
        params_.at(static_cast<std::size_t>(phase)) = effectiveOutputRate_ * conv::timecentToSecond(param);
    } else {
        throw std::invalid_argument("unknown phase");
    }
}

void Envelope::release() {
    if (phase_ < Phase::Release) {
        changePhase(Phase::Release);
    }
}

void Envelope::update() {
    if (phase_ == Phase::Finished) {
        return;
    }

    ++phaseSteps_;

    auto i = static_cast<std::size_t>(phase_);
    while (phase_ < Phase::Finished && phase_ != Phase::Sustain && phaseSteps_ >= params_.at(i)) {
        changePhase(static_cast<Phase>(++i));
    }

    const double& sustain = params_.at(static_cast<std::size_t>(Phase::Sustain));
    switch (phase_) {
    case Phase::Delay:
    case Phase::Finished:
        value_ = 0.0;
        return;
    case Phase::Attack:
        value_ = phaseSteps_ / params_.at(i);
        return;
    case Phase::Hold:
        value_ = 1.0;
        return;
    case Phase::Decay:
        value_ = 1.0 - phaseSteps_ / params_.at(i);
        if (value_ <= sustain) {
            value_ = sustain;
            changePhase(Phase::Sustain);
        }
        return;
    case Phase::Sustain:
        value_ = sustain;
        return;
    case Phase::Release:
        value_ -= 1.0 / params_.at(i);
        if (value_ <= 0.0) {
            value_ = 0.0;
            changePhase(Phase::Finished);
        }
        return;
    }

    throw std::logic_error("unreachable");
}

void Envelope::changePhase(Phase phase) {
    phase_ = phase;
    phaseSteps_ = 0;
}
}
