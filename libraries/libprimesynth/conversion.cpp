#include "conversion.h"
#include <array>
#include <cmath>

namespace primesynth {
namespace conv {
std::array<double, 1441> attenToAmpTable;
std::array<double, 1200> centToHertzTable;

void initialize() {
    static bool initialized = false;
    if (!initialized) {
        initialized = true;

        for (std::size_t i = 0; i < attenToAmpTable.size(); ++i) {
            // -200 instead of -100 for compatibility
            attenToAmpTable.at(i) = std::pow(10.0, i / -200.0);
        }
        for (std::size_t i = 0; i < centToHertzTable.size(); i++) {
            centToHertzTable.at(i) = 6.875 * std::exp2(i / 1200.0);
        }
    }
}

double attenuationToAmplitude(double atten) {
    if (atten <= 0.0) {
        return 1.0;
    } else if (atten >= attenToAmpTable.size()) {
        return 0.0;
    } else {
        return attenToAmpTable.at(static_cast<std::size_t>(atten));
    }
}

double amplitudeToAttenuation(double amp) {
    return -200.0 * std::log10(amp);
}

double keyToHertz(double key) {
    if (key < 0.0) {
        return 1.0;
    }

    int offset = 300;
    int ratio = 1;
    for (int threshold = 900; threshold <= 14100; threshold += 1200) {
        if (key * 100.0 < threshold) {
            return ratio * centToHertzTable.at(static_cast<int>(key * 100.0) + offset);
        }
        offset -= 1200;
        ratio *= 2;
    }

    return 1.0;
}

double timecentToSecond(double tc) {
    return std::exp2(tc / 1200.0);
}

double absoluteCentToHertz(double ac) {
    return 8.176 * std::exp2(ac / 1200.0);
}

double concave(double x) {
    if (x <= 0.0) {
        return 0.0;
    } else if (x >= 1.0) {
        return 1.0;
    } else {
        return 2.0 * conv::amplitudeToAttenuation(1.0 - x) / 960.0;
    }
}

double convex(double x) {
    if (x <= 0.0) {
        return 0.0;
    } else if (x >= 1.0) {
        return 1.0;
    } else {
        return 1.0 - 2.0 * conv::amplitudeToAttenuation(x) / 960.0;
    }
}
}
}
