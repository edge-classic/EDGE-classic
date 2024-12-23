
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

// See if song in memory is a supported type (IT/S3M/XM/MOD)
int m4p_TestFromData(uint8_t *Data, uint32_t DataLen);

// Load song from memory and initialize appropriate replayer
bool m4p_LoadFromData(uint8_t *Data, uint32_t DataLen, int32_t mixingFrequency, int32_t mixingBufferSize);

// Set replayer status to Play (does not generate output)
void m4p_PlaySong(void);

// Generate samples and fill buffer
void m4p_GenerateSamples(int16_t *buffer, int32_t numSamples);

// Generate samples and fill buffer
void m4p_GenerateFloatSamples(float *buffer, int32_t numSamples);

// Set replayer status to Stop
void m4p_Stop(void);

// De-initialize replayer
void m4p_Close(void);

// Free song memfile
void m4p_FreeSong(void);

#ifdef __cplusplus
}
#endif