#pragma once

#include <stdint.h>
#include "../it_structs.h"

void UpdateNoLoop(slaveChn_t *sc, uint32_t numSamples);
void UpdateForwardsLoop(slaveChn_t *sc, uint32_t numSamples);
void UpdatePingPongLoop(slaveChn_t *sc, uint32_t numSamples);
