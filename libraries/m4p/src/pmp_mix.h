#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "pmplay.h"

enum
{
	Status_SetVol = 1,
	Status_SetPan = 2,
	Status_SetFrq = 4,
	Status_StartTone = 8,
	Status_StopTone = 16,
	Status_QuickVol = 32,

	SType_Fwd = 1,
	SType_Rev = 2,
	SType_RevDir = 4,
	SType_Off = 8,
	SType_16 = 16,
	SType_Fadeout = 32
};

typedef struct
{
	const void *SBase, *SRevBase;
	uint8_t SType, SPan, SVol;
	int32_t SLVol1, SRVol1, SLVol2, SRVol2, SLVolIP, SRVolIP, SVolIPLen;
	int32_t SLen, SRepS, SRepL, SPos, SMixType;
	uint32_t SPosDec, SFrq;
} CIType;

typedef struct
{
	const void *SBase;
	uint8_t Status, SType;
	int16_t SVol, SPan;
	int32_t SFrq, SLen, SRepS, SRepL, SStartPos;
} WaveChannelInfoType;

extern int16_t chnReloc[32];
extern int32_t *CDA_MixBuffer;
extern CIType CI[32 * 2];

void P_SetSpeed(uint16_t bpm);
void P_StartTone(sampleTyp *s, int32_t smpStartPos);

// 8bb: added these two
bool mix_Init(int32_t audioBufferSize);
void mix_Free(void);
// -------------------

void mix_ClearChannels(void);
void mix_UpdateBuffer(int16_t *buffer, int32_t numSamples);
void mix_UpdateBufferFloat(float *buffer, int32_t numSamples);

bool dump_Init(int32_t frq, int32_t amp, int16_t songPos);
void dump_Close(void);
bool dump_EndOfTune(int32_t endSongPos);
int32_t dump_GetFrame(int16_t *p);
