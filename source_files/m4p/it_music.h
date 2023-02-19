#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "it_structs.h"

enum
{
	MIDICOMMAND_START         = 0x0000,
	MIDICOMMAND_STOP          = 0x0020,
	MIDICOMMAND_TICK          = 0x0040,
	MIDICOMMAND_PLAYNOTE      = 0x0060,
	MIDICOMMAND_STOPNOTE      = 0x0080,
	MIDICOMMAND_CHANGEVOLUME  = 0x00A0,
	MIDICOMMAND_CHANGEPAN     = 0x00C0,
	MIDICOMMAND_BANKSELECT    = 0x00E0,
	MIDICOMMAND_PROGRAMSELECT = 0x0100,
	MIDICOMMAND_CHANGEPITCH   = 0xFFFF
};

// 8bb: 31 is possible through initial tempo (but 32 is general minimum)
#define LOWEST_BPM_POSSIBLE 31

#define MIX_FRAC_BITS 16
#define MIX_FRAC_MASK ((1 << MIX_FRAC_BITS)-1)

/* 8bb:
** Amount of extra bytes to allocate for every instrument sample,
** this is used for a hack for resampling interpolation to be
** branchless in the inner channel mixer loop.
** Warning: Do not change this!
*/
#define SMP_DAT_OFFSET 16
#define SAMPLE_PAD_LENGTH (SMP_DAT_OFFSET+16)

// 8bb: globalized
extern void (*DriverClose)(void);
extern void (*DriverMix)(int32_t, int16_t *);
extern void (*DriverResetMixer)(void);
extern int32_t (*DriverPostMix)(int16_t *, int32_t);
extern void (*DriverMixSamples)(void);
extern void (*DriverSetTempo)(uint8_t);
extern void (*DriverSetMixVolume)(uint8_t);
extern void (*DriverFixSamples)(void);
// --------------------------------------------

void Music_SetDefaultMIDIDataArea(void); // 8bb: added this
char *Music_GetMIDIDataArea(void);
void RecalculateAllVolumes(void);
void MIDITranslate(hostChn_t *hc, slaveChn_t *sc, uint16_t Input);
void InitPlayInstrument(hostChn_t *hc, slaveChn_t *sc, instrument_t *ins);
slaveChn_t *AllocateChannel(hostChn_t *hc, uint8_t *hcFlags);
uint8_t Random(void);
void GetLoopInformation(slaveChn_t *sc);
void ApplyRandomValues(hostChn_t *hc);

void PitchSlideUpLinear(hostChn_t *hc, slaveChn_t *sc, int16_t SlideValue);
void PitchSlideUp(hostChn_t *hc, slaveChn_t *sc, int16_t SlideValue);
void PitchSlideDown(hostChn_t *hc, slaveChn_t *sc, int16_t SlideValue);

void Update(void);
void Music_FillAudioBuffer(int16_t *buffer, int32_t numSamples);

bool Music_Init(int32_t mixingFrequency, int32_t mixingBufferSize);
void Music_Close(void); // 8bb: added this
void Music_Stop(void);
void Music_StopChannels(void);
void Music_PreviousOrder(void);
void Music_NextOrder(void);
void Music_PlaySong(uint16_t order);
void Music_InitTempo(void);

bool Music_AllocateSample(uint32_t sample, uint32_t length);
bool Music_AllocateRightSample(uint32_t sample, uint32_t length); // 8bb: added this
void Music_ReleaseSample(uint32_t sample);
void Music_ReleaseAllSamples(void);

bool Music_AllocatePattern(uint32_t pattern, uint32_t length);
void Music_ReleasePattern(uint32_t pattern);
void Music_ReleaseAllPatterns(void);
int32_t Music_GetActiveVoices(void); // 8bb: added this
void Music_FreeSong(void); // 8bb: added this