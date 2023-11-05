#pragma once
#include <cstdint>

namespace primesynth {
namespace conv {
void initialize();

// attenuation: centibel
// amplitude:   normalized linear value in [0, 1]
double attenuationToAmplitude(double atten);
double amplitudeToAttenuation(double amp);

double keyToHertz(double key);
double timecentToSecond(double tc);
double absoluteCentToHertz(double ac);

double concave(double x);
double convex(double x);
}
}
