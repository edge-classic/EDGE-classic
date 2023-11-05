#pragma once

#include <stdint.h>

extern const uint32_t PitchTable[120];
extern const int8_t FineSineData[3 * 256]; // 8bb: sine/ramp/square

#ifndef USEFPUCODE
extern const uint32_t FineLinearSlideUpTable[16];
extern const uint32_t LinearSlideUpTable[257];
extern const uint16_t FineLinearSlideDownTable[16];
extern const uint16_t LinearSlideDownTable[257];
#endif
