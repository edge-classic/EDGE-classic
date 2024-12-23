#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "pmp_mix.h"

#define GET_VOL \
	const int32_t CDA_LVol = v->SLVol1; \
	const int32_t CDA_RVol = v->SRVol1; \

#define GET_VOL_CENTER \
	const int32_t CDA_LVol = v->SLVol1; \

#define GET_VOL_RAMP \
	int32_t CDA_LVol = v->SLVol2; \
	int32_t CDA_RVol = v->SRVol2; \

#define SET_VOL_BACK \
	v->SLVol2 = CDA_LVol; \
	v->SRVol2 = CDA_RVol; \

#define GET_MIXER_VARS \
	int32_t *audioMix = CDA_MixBuffer + (bufferPos << 1); \
	int32_t realPos = v->SPos; \
	uint32_t pos = v->SPosDec; \
	uint16_t CDA_MixBuffPos = (32768+96)-8; /* address of FT2 mix buffer minus mix sample size (used for quirky LERP) */ \

#define GET_RAMP_VARS \
	int32_t CDA_LVolIP = v->SLVolIP; \
	int32_t CDA_RVolIP = v->SRVolIP; \

#define SET_BASE8 \
	const int8_t *CDA_LinearAdr = (int8_t *)v->SBase; \
	const int8_t *CDA_LinAdrRev = (int8_t *)v->SRevBase; \
	const int8_t *smpPtr = CDA_LinearAdr + realPos; \

#define SET_BASE16 \
	const int16_t *CDA_LinearAdr = (int16_t *)v->SBase; \
	const int16_t *CDA_LinAdrRev = (int16_t *)v->SRevBase; \
	const int16_t *smpPtr = CDA_LinearAdr + realPos; \

#define INC_POS \
	smpPtr += CDA_IPValH; \
	smpPtr += (CDA_IPValL > (uint32_t)~pos); /* if pos would 32-bit overflow after CDA_IPValL add, add one to smpPtr (branchless) */ \
	pos += CDA_IPValL; \

#define SET_BACK_MIXER_POS \
	v->SPosDec = pos & 0xFFFF0000; \
	v->SPos = realPos; \

#define VOL_RAMP \
	CDA_LVol += CDA_LVolIP; \
	CDA_RVol += CDA_RVolIP; \

// stereo mixing without interpolation

#define MIX_8BIT \
	sample = (*smpPtr) << (28-8); \
	*audioMix++ += ((int64_t)sample * (int32_t)CDA_LVol) >> 32; \
	*audioMix++ += ((int64_t)sample * (int32_t)CDA_RVol) >> 32; \
	INC_POS \

#define MIX_16BIT \
	sample = (*smpPtr) << (28-16); \
	*audioMix++ += ((int64_t)sample * (int32_t)CDA_LVol) >> 32; \
	*audioMix++ += ((int64_t)sample * (int32_t)CDA_RVol) >> 32; \
	INC_POS \

// center mixing without interpolation

#define MIX_8BIT_M \
	sample = (*smpPtr) << (28-8); \
	sample = ((int64_t)sample * (int32_t)CDA_LVol) >> 32; \
	*audioMix++ += sample; \
	*audioMix++ += sample; \
	INC_POS \

#define MIX_16BIT_M \
	sample = (*smpPtr) << (28-16); \
	sample = ((int64_t)sample * (int32_t)CDA_LVol) >> 32; \
	*audioMix++ += sample; \
	*audioMix++ += sample; \
	INC_POS \

// linear interpolation with bit-accurate results to FT2.08/FT2.09
#define LERP(s1, s2, f) \
{ \
	s2 -= s1; \
	f >>= 1; \
	s2 = ((int64_t)s2 * (int32_t)f) >> 32; \
	f += f; \
	s2 += s2; \
	s2 += s1; \
} \

// stereo mixing w/ linear interpolation

#define MIX_8BIT_INTRP \
	sample = smpPtr[0] << 8; \
	sample2 = smpPtr[1] << 8; \
	LERP(sample, sample2, pos) \
	sample2 <<= (28-16); \
	*audioMix++ += ((int64_t)sample2 * (int32_t)CDA_LVol) >> 32; \
	*audioMix++ += ((int64_t)sample2 * (int32_t)CDA_RVol) >> 32; \
	INC_POS \

#define MIX_16BIT_INTRP \
	sample = smpPtr[0]; \
	sample2 = smpPtr[1]; \
	LERP(sample, sample2, pos) \
	sample2 <<= (28-16); \
	*audioMix++ += ((int64_t)sample2 * (int32_t)CDA_LVol) >> 32; \
	*audioMix++ += ((int64_t)sample2 * (int32_t)CDA_RVol) >> 32; \
	INC_POS \

// center mixing w/ linear interpolation

#define MIX_8BIT_INTRP_M \
	sample = smpPtr[0] << 8; \
	sample2 = smpPtr[1] << 8; \
	LERP(sample, sample2, pos) \
	sample2 <<= (28-16); \
	sample = ((int64_t)sample2 * (int32_t)CDA_LVol) >> 32; \
	*audioMix++ += sample; \
	*audioMix++ += sample; \
	INC_POS \

#define MIX_16BIT_INTRP_M \
	sample = smpPtr[0]; \
	sample2 = smpPtr[1]; \
	LERP(sample, sample2, pos) \
	sample2 <<= (28-16); \
	sample = ((int64_t)sample2 * (int32_t)CDA_LVol) >> 32; \
	*audioMix++ += sample; \
	*audioMix++ += sample; \
	INC_POS \

// ------------------------

#define LIMIT_MIX_NUM \
	int32_t samplesToMix; \
	int32_t SFrq = v->SFrq; \
	int32_t i = (v->SLen-1) - realPos; \
	if (i > UINT16_MAX) i = UINT16_MAX; /* 8bb: added this to prevent 64-bit div (still bit-accurate mixing results) */ \
	if (SFrq != 0) \
	{ \
		const uint32_t tmp32 = (i << 16) | ((0xFFFF0000 - pos) >> 16); \
		samplesToMix = (tmp32 / (uint32_t)SFrq) + 1; \
	} \
	else \
	{ \
		samplesToMix = 65535; \
	} \
	\
	if (samplesToMix > CDA_BytesLeft) \
		samplesToMix = CDA_BytesLeft; \

#define LIMIT_MIX_NUM_RAMP \
	if (v->SVolIPLen == 0) \
	{ \
		CDA_LVolIP = 0; \
		CDA_RVolIP = 0; \
	} \
	else \
	{ \
		if (samplesToMix > v->SVolIPLen) \
			samplesToMix = v->SVolIPLen; \
		\
		v->SVolIPLen -= samplesToMix; \
	} \

#define HANDLE_POS_START \
	const bool backwards = (v->SType & (SType_Rev+SType_RevDir)) == SType_Rev+SType_RevDir; \
	if (backwards) \
	{ \
		SFrq = 0 - SFrq; \
		realPos = ~realPos; \
		smpPtr = CDA_LinAdrRev + realPos; \
		pos ^= 0xFFFF0000; \
	} \
	else \
	{ \
		smpPtr = CDA_LinearAdr + realPos; \
	} \
	\
	pos += CDA_MixBuffPos; \
	const int32_t CDA_IPValH = (int32_t)SFrq >> 16; \
	const uint32_t CDA_IPValL = ((uint32_t)(SFrq & 0xFFFF) << 16) + 8; /* 8 = mixer buffer increase (for LERP to be bit-accurate to FT2) */ \

#define HANDLE_POS_END \
	if (backwards) \
	{ \
		pos ^= 0xFFFF0000; \
		realPos = ~(int32_t)(smpPtr - CDA_LinAdrRev); \
	} \
	else \
	{ \
		realPos = (int32_t)(smpPtr - CDA_LinearAdr); \
	} \
	CDA_MixBuffPos = pos & 0xFFFF; \
	pos &= 0xFFFF0000; \
	\
	if (realPos >= v->SLen) \
	{ \
		uint8_t SType = v->SType; \
		if (SType & (SType_Fwd+SType_Rev)) \
		{ \
			do \
			{ \
				realPos -= v->SRepL; \
				SType ^= SType_RevDir; \
			} \
			while (realPos >= v->SLen); \
			v->SType = SType; \
		} \
		else \
		{ \
			v->SType = SType_Off; \
			return; \
		} \
	} \

typedef void (*mixRoutine)(void *, int32_t, int32_t);

extern mixRoutine mixRoutineTable[16];

void PMPMix32Proc(CIType *v, int32_t numSamples, int32_t bufferPos);
