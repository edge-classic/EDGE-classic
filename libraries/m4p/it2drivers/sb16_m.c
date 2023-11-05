#include <stdint.h>
#include <stdbool.h>
#include "../it_structs.h"
#include "../it_music.h"
#include "sb16_m.h"

static void M32Mix8(slaveChn_t *sc, int32_t *mixBufPtr, int32_t numSamples);
static void M32Mix16(slaveChn_t *sc, int32_t *mixBufPtr, int32_t numSamples);
static void M32Mix8S(slaveChn_t *sc, int32_t *mixBufPtr, int32_t numSamples);
static void M32Mix16S(slaveChn_t *sc, int32_t *mixBufPtr, int32_t numSamples);
static void M32Mix8I(slaveChn_t *sc, int32_t *mixBufPtr, int32_t numSamples);
static void M32Mix16I(slaveChn_t *sc, int32_t *mixBufPtr, int32_t numSamples);
static void M32Mix8IS(slaveChn_t *sc, int32_t *mixBufPtr, int32_t numSamples);
static void M32Mix16IS(slaveChn_t *sc, int32_t *mixBufPtr, int32_t numSamples);

#define Get32Bit8Waveform \
	sample = smp[0]; \
	sample2 = smp[1]; \
	sample2 -= sample; \
	sample2 *= (int32_t)sc->Frac32; \
	sample2 >>= MIX_FRAC_BITS-8; \
	sample <<= 8; \
	sample += sample2;

#define Get32Bit16Waveform \
	sample = smp[0]; \
	sample2 = smp[1]; \
	sample2 -= sample; \
	sample2 >>= 1; \
	sample2 *= (int32_t)sc->Frac32; \
	sample2 >>= MIX_FRAC_BITS-1; \
	sample += sample2;

#define UpdatePos \
	sc->Frac32 += Driver.Delta32; \
	smp += (int32_t)sc->Frac32 >> MIX_FRAC_BITS; \
	sc->Frac32 &= MIX_FRAC_MASK;

#define M32Mix8_M \
	sample = *smp << 8; \
	(*MixBufPtr++) -= sample * sc->LeftVolume; \
	(*MixBufPtr++) -= sample * sc->RightVolume; \
	UpdatePos

#define M32Mix16_M \
	sample = *smp; \
	(*MixBufPtr++) -= sample * sc->LeftVolume; \
	(*MixBufPtr++) -= sample * sc->RightVolume; \
	UpdatePos

#define M32Mix8S_M \
	sample = *smp << 8; \
	(*MixBufPtr++) -= sample * sc->LeftVolume; \
	(*MixBufPtr++) += sample * sc->RightVolume; \
	UpdatePos

#define M32Mix16S_M \
	sample = *smp; \
	(*MixBufPtr++) -= sample * sc->LeftVolume; \
	(*MixBufPtr++) += sample * sc->RightVolume; \
	UpdatePos

#define M32Mix8I_M \
	Get32Bit8Waveform \
	(*MixBufPtr++) -= sample * sc->LeftVolume; \
	(*MixBufPtr++) -= sample * sc->RightVolume; \
	UpdatePos

#define M32Mix16I_M \
	Get32Bit16Waveform \
	(*MixBufPtr++) -= sample * sc->LeftVolume; \
	(*MixBufPtr++) -= sample * sc->RightVolume; \
	UpdatePos

#define M32Mix8IS_M \
	Get32Bit8Waveform \
	(*MixBufPtr++) -= sample * sc->LeftVolume; \
	(*MixBufPtr++) += sample * sc->RightVolume; \
	UpdatePos

#define M32Mix16IS_M \
	Get32Bit16Waveform \
	(*MixBufPtr++) -= sample * sc->LeftVolume; \
	(*MixBufPtr++) += sample * sc->RightVolume; \
	UpdatePos

const mixFunc SB16_MixFunctionTables[8] =
{
	(mixFunc)M32Mix8,
	(mixFunc)M32Mix16,
	(mixFunc)M32Mix8S,
	(mixFunc)M32Mix16S,
	(mixFunc)M32Mix8I,
	(mixFunc)M32Mix16I,
	(mixFunc)M32Mix8IS,
	(mixFunc)M32Mix16IS
};

void M32Mix8(slaveChn_t *sc, int32_t *MixBufPtr, int32_t NumSamples)
{
	const int8_t *base = (int8_t *)sc->SmpPtr->Data;
	const int8_t *smp = base + sc->SamplingPosition;
	int32_t sample;

	for (int32_t i = 0; i < (NumSamples & 3); i++)
	{
		M32Mix8_M
	}
	NumSamples >>= 2;

	for (int32_t i = 0; i < NumSamples; i++)
	{
		M32Mix8_M
		M32Mix8_M
		M32Mix8_M
		M32Mix8_M
	}

	sc->SamplingPosition = (int32_t)(smp - base);
}

void M32Mix16(slaveChn_t *sc, int32_t *MixBufPtr, int32_t NumSamples)
{
	const int16_t *base = (int16_t *)sc->SmpPtr->Data;
	const int16_t *smp = base + sc->SamplingPosition;
	int32_t sample;

	for (int32_t i = 0; i < (NumSamples & 3); i++)
	{
		M32Mix16_M
	}
	NumSamples >>= 2;

	for (int32_t i = 0; i < NumSamples; i++)
	{
		M32Mix16_M
		M32Mix16_M
		M32Mix16_M
		M32Mix16_M
	}

	sc->SamplingPosition = (int32_t)(smp - base);
}

void M32Mix8S(slaveChn_t *sc, int32_t *MixBufPtr, int32_t NumSamples)
{
	const int8_t *base = (int8_t *)sc->SmpPtr->Data;
	const int8_t *smp = base + sc->SamplingPosition;
	int32_t sample;

	for (int32_t i = 0; i < (NumSamples & 3); i++)
	{
		M32Mix8S_M
	}
	NumSamples >>= 2;

	for (int32_t i = 0; i < NumSamples; i++)
	{
		M32Mix8S_M
		M32Mix8S_M
		M32Mix8S_M
		M32Mix8S_M
	}

	sc->SamplingPosition = (int32_t)(smp - base);
}

void M32Mix16S(slaveChn_t *sc, int32_t *MixBufPtr, int32_t NumSamples)
{
	const int16_t *base = (int16_t *)sc->SmpPtr->Data;
	const int16_t *smp = base + sc->SamplingPosition;
	int32_t sample;

	for (int32_t i = 0; i < (NumSamples & 3); i++)
	{
		M32Mix16S_M
	}
	NumSamples >>= 2;

	for (int32_t i = 0; i < NumSamples; i++)
	{
		M32Mix16S_M
		M32Mix16S_M
		M32Mix16S_M
		M32Mix16S_M
	}

	sc->SamplingPosition = (int32_t)(smp - base);
}

void M32Mix8I(slaveChn_t *sc, int32_t *MixBufPtr, int32_t NumSamples)
{
	const int8_t *base = (int8_t *)sc->SmpPtr->Data;
	const int8_t *smp = base + sc->SamplingPosition;
	int32_t sample, sample2;

	for (int32_t i = 0; i < (NumSamples & 3); i++)
	{
		M32Mix8I_M
	}
	NumSamples >>= 2;

	for (int32_t i = 0; i < NumSamples; i++)
	{
		M32Mix8I_M
		M32Mix8I_M
		M32Mix8I_M
		M32Mix8I_M
	}

	sc->SamplingPosition = (int32_t)(smp - base);
}

void M32Mix16I(slaveChn_t *sc, int32_t *MixBufPtr, int32_t NumSamples)
{
	const int16_t *base = (int16_t *)sc->SmpPtr->Data;
	const int16_t *smp = base + sc->SamplingPosition;
	int32_t sample, sample2;

	for (int32_t i = 0; i < (NumSamples & 3); i++)
	{
		M32Mix16I_M
	}
	NumSamples >>= 2;

	for (int32_t i = 0; i < NumSamples; i++)
	{
		M32Mix16I_M
		M32Mix16I_M
		M32Mix16I_M
		M32Mix16I_M
	}

	sc->SamplingPosition = (int32_t)(smp - base);
}

void M32Mix8IS(slaveChn_t *sc, int32_t *MixBufPtr, int32_t NumSamples)
{
	const int8_t *base = (int8_t *)sc->SmpPtr->Data;
	const int8_t *smp = base + sc->SamplingPosition;
	int32_t sample, sample2;

	for (int32_t i = 0; i < (NumSamples & 3); i++)
	{
		M32Mix8IS_M
	}
	NumSamples >>= 2;

	for (int32_t i = 0; i < NumSamples; i++)
	{
		M32Mix8IS_M
		M32Mix8IS_M
		M32Mix8IS_M
		M32Mix8IS_M
	}

	sc->SamplingPosition = (int32_t)(smp - base);
}

void M32Mix16IS(slaveChn_t *sc, int32_t *MixBufPtr, int32_t NumSamples)
{
	const int16_t *base = (int16_t *)sc->SmpPtr->Data;
	const int16_t *smp = base + sc->SamplingPosition;
	int32_t sample, sample2;

	for (int32_t i = 0; i < (NumSamples & 3); i++)
	{
		M32Mix16IS_M
	}
	NumSamples >>= 2;

	for (int32_t i = 0; i < NumSamples; i++)
	{
		M32Mix16IS_M
		M32Mix16IS_M
		M32Mix16IS_M
		M32Mix16IS_M
	}

	sc->SamplingPosition = (int32_t)(smp - base);
}
