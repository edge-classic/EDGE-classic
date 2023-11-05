/*
** 8bb:
** ---- Code for "zero-vol" update routines ----
**
** These are used when the final volume is zero, and they'll only update
** the sampling position instead of doing actual mixing. They are the same
** for SB16/"SB16 MMX"/"WAV writer".
*/

#include <stdint.h>
#include "../it_structs.h"
#include "../it_music.h"

void UpdateNoLoop(slaveChn_t *sc, uint32_t numSamples)
{
	const uint64_t SamplesToMix = (uint64_t)sc->Delta32 * (uint32_t)numSamples;

	uint32_t SampleOffset = sc->SamplingPosition + (uint32_t)(SamplesToMix >> MIX_FRAC_BITS);
	sc->Frac32 += SamplesToMix & MIX_FRAC_MASK;
	SampleOffset += (uint32_t)sc->Frac32 >> MIX_FRAC_BITS;
	sc->Frac32 &= MIX_FRAC_MASK;

	if (SampleOffset >= (uint32_t)sc->LoopEnd)
	{
		sc->Flags = SF_NOTE_STOP;
		if (!(sc->HostChnNum & CHN_DISOWNED))
		{
			((hostChn_t *)sc->HostChnPtr)->Flags &= ~HF_CHAN_ON; // Signify channel off
			return;
		}
	}

	sc->SamplingPosition = SampleOffset;
}

void UpdateForwardsLoop(slaveChn_t *sc, uint32_t numSamples)
{
	const uint64_t SamplesToMix = (uint64_t)sc->Delta32 * (uint32_t)numSamples;

	sc->Frac32 += SamplesToMix & MIX_FRAC_MASK;
	sc->SamplingPosition += sc->Frac32 >> MIX_FRAC_BITS;
	sc->SamplingPosition += (uint32_t)(SamplesToMix >> MIX_FRAC_BITS);
	sc->Frac32 &= MIX_FRAC_MASK;

	if ((uint32_t)sc->SamplingPosition >= (uint32_t)sc->LoopEnd) // Reset position...
	{
		const uint32_t LoopLength = sc->LoopEnd - sc->LoopBegin;
		if (LoopLength == 0)
			sc->SamplingPosition = 0;
		else
			sc->SamplingPosition = sc->LoopBegin + ((sc->SamplingPosition - sc->LoopEnd) % LoopLength);
	}
}

void UpdatePingPongLoop(slaveChn_t *sc, uint32_t numSamples)
{
	const uint32_t LoopLength = sc->LoopEnd - sc->LoopBegin;

	const uint64_t SamplesToMix = (uint64_t)sc->Delta32 * (uint32_t)numSamples;
	uint32_t IntSamples = (uint32_t)(SamplesToMix >> MIX_FRAC_BITS);
	uint16_t FracSamples = (uint16_t)(SamplesToMix & MIX_FRAC_MASK);

	if (sc->LoopDirection == DIR_BACKWARDS)
	{
		sc->Frac32 -= FracSamples;
		sc->SamplingPosition += ((int32_t)sc->Frac32 >> MIX_FRAC_BITS);
		sc->SamplingPosition -= IntSamples;
		sc->Frac32 &= MIX_FRAC_MASK;

		if (sc->SamplingPosition <= sc->LoopBegin)
		{
			uint32_t NewLoopPos = (uint32_t)(sc->LoopBegin - sc->SamplingPosition) % (LoopLength << 1);
			if (NewLoopPos >= LoopLength)
			{
				sc->SamplingPosition = (sc->LoopEnd - 1) + (LoopLength - NewLoopPos);
			}
			else
			{
				sc->LoopDirection = DIR_FORWARDS;
				sc->SamplingPosition = sc->LoopBegin + NewLoopPos;
				sc->Frac32 = (uint16_t)(0 - sc->Frac32);
			}
		}
	}
	else // 8bb: forwards
	{
		sc->Frac32 += FracSamples;
		sc->SamplingPosition += sc->Frac32 >> MIX_FRAC_BITS;
		sc->SamplingPosition += IntSamples;
		sc->Frac32 &= MIX_FRAC_MASK;

		if ((uint32_t)sc->SamplingPosition >= (uint32_t)sc->LoopEnd)
		{
			uint32_t NewLoopPos = (uint32_t)(sc->SamplingPosition - sc->LoopEnd) % (LoopLength << 1);
			if (NewLoopPos >= LoopLength)
			{
				sc->SamplingPosition = sc->LoopBegin + (NewLoopPos - LoopLength);
			}
			else
			{
				sc->LoopDirection = DIR_BACKWARDS;
				sc->SamplingPosition = (sc->LoopEnd - 1) - NewLoopPos;
				sc->Frac32 = (uint16_t)(0 - sc->Frac32);
			}
		}
	}
}
