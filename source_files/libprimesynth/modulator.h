#pragma once
#include "soundfont_spec.h"

namespace primesynth {
class Modulator {
public:
    explicit Modulator(const sf::ModList& param);

    sf::Generator getDestination() const;
    std::int16_t getAmount() const;
    bool canBeNegative() const;
    double getValue() const;

    bool updateSFController(sf::GeneralController controller, double value);
    bool updateMIDIController(std::uint8_t controller, std::uint8_t value);

private:
    const sf::ModList param_;
    double source_, amountSource_, value_;

    void calculateValue();
};
}
