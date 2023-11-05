/*
** ---- SB16 IT2 driver ----
*/

#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "../cpu.h"
#include "../it_structs.h"
#include "../it_music.h" // Update()
#include "sb16_m.h"
#include "zerovol.h"

static uint16_t MixVolume;
static int32_t BytesToMix, *MixBuffer, MixTransferRemaining, MixTransferOffset;

static void SB16_MixSamples(void)
{
	MixTransferOffset = 0;

	memset(MixBuffer, 0, BytesToMix * 2 * sizeof (int32_t));

	slaveChn_t *sc = sChn;
	for (uint32_t i = 0; i < Driver.NumChannels; i++, sc++)
	{
		if (!(sc->Flags & SF_CHAN_ON) || sc->Smp == 100)
			continue;

		if (sc->Flags & SF_NOTE_STOP)
		{
			sc->Flags &= ~SF_CHAN_ON;
			continue;
		}

		if (sc->Flags & SF_FREQ_CHANGE)
		{
			if ((uint32_t)sc->Frequency>>MIX_FRAC_BITS >= Driver.MixSpeed)
			{
				sc->Flags = SF_NOTE_STOP;
				if (!(sc->HostChnNum & CHN_DISOWNED))
					((hostChn_t *)sc->HostChnPtr)->Flags &= ~HF_CHAN_ON; // Turn off channel

				continue;
			}

			// 8bb: calculate mixer delta
			uint32_t Quotient = (uint32_t)sc->Frequency / Driver.MixSpeed;
			uint32_t Remainder = (uint32_t)sc->Frequency % Driver.MixSpeed;
			sc->Delta32 = (Quotient << MIX_FRAC_BITS) | (uint16_t)((Remainder << MIX_FRAC_BITS) / Driver.MixSpeed);
		}

		if (sc->Flags & (SF_RECALC_FINALVOL | SF_LOOP_CHANGED | SF_PAN_CHANGED))
		{
			if (!(sc->Flags & SF_CHN_MUTED))
			{
				if (!(Song.Header.Flags & ITF_STEREO)) // 8bb: mono?
				{
					sc->LeftVolume = sc->RightVolume = (sc->FinalVol15Bit * MixVolume) >> 8; // 8bb: 0..16384
				}
				else if (sc->FinalPan == PAN_SURROUND)
				{
					sc->LeftVolume = sc->RightVolume = (sc->FinalVol15Bit * MixVolume) >> 9; // 8bb: 0..8192
				}
				else // 8bb: normal (panned)
				{
					sc->LeftVolume  = ((64-sc->FinalPan) * MixVolume * sc->FinalVol15Bit) >> 14; // 8bb: 0..16384
					sc->RightVolume = (    sc->FinalPan  * MixVolume * sc->FinalVol15Bit) >> 14;
				}
			}
		}

		if (sc->Delta32 == 0) // 8bb: added this protection just in case (shouldn't happen)
			continue;

		uint32_t MixBlockSize = BytesToMix;
		const uint32_t LoopLength = sc->LoopEnd - sc->LoopBegin; // 8bb: also length for non-loopers

		if ((sc->Flags & SF_CHN_MUTED) || (sc->LeftVolume == 0 && sc->RightVolume == 0))
		{
			if ((int32_t)LoopLength > 0)
			{
				if (sc->LoopMode == LOOP_PINGPONG)
					UpdatePingPongLoop(sc, MixBlockSize);
				else if (sc->LoopMode == LOOP_FORWARDS)
					UpdateForwardsLoop(sc, MixBlockSize);
				else
					UpdateNoLoop(sc, MixBlockSize);
			}

			sc->Flags &= ~(SF_RECALC_PAN      | SF_RECALC_VOL | SF_FREQ_CHANGE |
			               SF_RECALC_FINALVOL | SF_NEW_NOTE   | SF_NOTE_STOP   |
			               SF_LOOP_CHANGED    | SF_PAN_CHANGED);

			continue;
		}

		const bool Surround = (sc->FinalPan == PAN_SURROUND);
		const bool Sample16it = !!(sc->SmpBitDepth & SMPF_16BIT);
		const mixFunc Mix = SB16_MixFunctionTables[(Driver.MixMode << 2) + (Surround << 1) + Sample16it];
		int32_t *MixBufferPtr = MixBuffer;

		if ((int32_t)LoopLength > 0)
		{
			if (sc->LoopMode == LOOP_PINGPONG)
			{
				while (MixBlockSize > 0)
				{
					uint32_t NewLoopPos;
					if (sc->LoopDirection == DIR_BACKWARDS)
					{
						if (sc->SamplingPosition <= sc->LoopBegin)
						{
							NewLoopPos = (uint32_t)(sc->LoopBegin - sc->SamplingPosition) % (LoopLength << 1);
							if (NewLoopPos >= LoopLength)
							{
								sc->SamplingPosition = (sc->LoopEnd - 1) - (NewLoopPos - LoopLength);
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
						if ((uint32_t)sc->SamplingPosition >= (uint32_t)sc->LoopEnd)
						{
							NewLoopPos = (uint32_t)(sc->SamplingPosition - sc->LoopEnd) % (LoopLength << 1);
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

					uint32_t SamplesToMix;
					if (sc->LoopDirection == DIR_BACKWARDS)
					{
						SamplesToMix = sc->SamplingPosition - (sc->LoopBegin + 1);
#if CPU_32BIT
						if (SamplesToMix > UINT16_MAX) // 8bb: limit it so we can do a hardware 32-bit div (instead of slow software 64-bit div)
							SamplesToMix = UINT16_MAX;
#endif
						SamplesToMix = ((((uintCPUWord_t)SamplesToMix << MIX_FRAC_BITS) | (uint16_t)sc->Frac32) / sc->Delta32) + 1;
						Driver.Delta32 = 0 - sc->Delta32;
					}
					else // 8bb: forwards
					{
						SamplesToMix = (sc->LoopEnd - 1) - sc->SamplingPosition;
#if CPU_32BIT
						if (SamplesToMix > UINT16_MAX)
							SamplesToMix = UINT16_MAX;
#endif
						SamplesToMix = ((((uintCPUWord_t)SamplesToMix << MIX_FRAC_BITS) | ((uint16_t)sc->Frac32 ^ MIX_FRAC_MASK)) / sc->Delta32) + 1;
						Driver.Delta32 = sc->Delta32;
					}

					if (SamplesToMix > MixBlockSize)
						SamplesToMix = MixBlockSize;

					Mix(sc, MixBufferPtr, SamplesToMix);
					MixBufferPtr += SamplesToMix << 1;

					MixBlockSize -= SamplesToMix;
				}
			}
			else if (sc->LoopMode == LOOP_FORWARDS)
			{
				while (MixBlockSize > 0)
				{
					if ((uint32_t)sc->SamplingPosition >= (uint32_t)sc->LoopEnd)
						sc->SamplingPosition = sc->LoopBegin + ((uint32_t)(sc->SamplingPosition - sc->LoopEnd) % LoopLength);

					uint32_t SamplesToMix = (sc->LoopEnd - 1) - sc->SamplingPosition;
#if CPU_32BIT
					if (SamplesToMix > UINT16_MAX)
						SamplesToMix = UINT16_MAX;
#endif
					SamplesToMix = ((((uintCPUWord_t)SamplesToMix << MIX_FRAC_BITS) | ((uint16_t)sc->Frac32 ^ MIX_FRAC_MASK)) / sc->Delta32) + 1;
					if (SamplesToMix > MixBlockSize)
						SamplesToMix = MixBlockSize;

					Driver.Delta32 = sc->Delta32;
					Mix(sc, MixBufferPtr, SamplesToMix);
					MixBufferPtr += SamplesToMix << 1;

					MixBlockSize -= SamplesToMix;
				}
			}
			else // 8bb: no loop
			{
				while (MixBlockSize > 0)
				{
					if ((uint32_t)sc->SamplingPosition >= (uint32_t)sc->LoopEnd) // 8bb: LoopEnd = sample end, even for non-loopers
					{
						sc->Flags = SF_NOTE_STOP;
						if (!(sc->HostChnNum & CHN_DISOWNED))
							((hostChn_t *)sc->HostChnPtr)->Flags &= ~HF_CHAN_ON; // Signify channel off

						break;
					}

					uint32_t SamplesToMix = (sc->LoopEnd - 1) - sc->SamplingPosition;
#if CPU_32BIT
					if (SamplesToMix > UINT16_MAX)
						SamplesToMix = UINT16_MAX;
#endif
					SamplesToMix = ((((uintCPUWord_t)SamplesToMix << MIX_FRAC_BITS) | ((uint16_t)sc->Frac32 ^ MIX_FRAC_MASK)) / sc->Delta32) + 1;
					if (SamplesToMix > MixBlockSize)
						SamplesToMix = MixBlockSize;

					Driver.Delta32 = sc->Delta32;
					Mix(sc, MixBufferPtr, SamplesToMix);
					MixBufferPtr += SamplesToMix << 1;

					MixBlockSize -= SamplesToMix;
				}
			}
		}

		sc->Flags &= ~(SF_RECALC_PAN      | SF_RECALC_VOL | SF_FREQ_CHANGE |
		               SF_RECALC_FINALVOL | SF_NEW_NOTE   | SF_NOTE_STOP   |
		               SF_LOOP_CHANGED    | SF_PAN_CHANGED);
	}
}

static void SB16_SetTempo(uint8_t Tempo)
{
	assert(Tempo >= LOWEST_BPM_POSSIBLE);
	BytesToMix = ((Driver.MixSpeed << 1) + (Driver.MixSpeed >> 1)) / Tempo;
}

static void SB16_SetMixVolume(uint8_t vol)
{
	MixVolume = vol;
	RecalculateAllVolumes();
}

static void SB16_ResetMixer(void) // 8bb: added this
{
	MixTransferRemaining = 0;
	MixTransferOffset = 0;
}

static int32_t SB16_PostMix(int16_t *AudioOut16, int32_t SamplesToOutput) // 8bb: added this
{
	const uint8_t SampleShiftValue = (Song.Header.Flags & ITF_STEREO) ? 13 : 14;

	int32_t SamplesTodo = (SamplesToOutput == 0) ? BytesToMix : SamplesToOutput;
	for (int32_t i = 0; i < SamplesTodo * 2; i++)
	{
		int32_t Sample = MixBuffer[MixTransferOffset++] >> SampleShiftValue;

		if (Sample < INT16_MIN)
			Sample = INT16_MIN;
		else if (Sample > INT16_MAX)
			Sample = INT16_MAX;

		*AudioOut16++ = (int16_t)Sample;
	}

	return SamplesTodo;
}

static void SB16_Mix(int32_t numSamples, int16_t *audioOut) // 8bb: added this (original SB16 driver uses IRQ callback)
{
	int32_t SamplesLeft = numSamples;
	while (SamplesLeft > 0)
	{
		if (MixTransferRemaining == 0)
		{
			Update();
			SB16_MixSamples();
			MixTransferRemaining = BytesToMix;
		}

		int32_t SamplesToTransfer = SamplesLeft;
		if (SamplesToTransfer > MixTransferRemaining)
			SamplesToTransfer = MixTransferRemaining;

		SB16_PostMix(audioOut, SamplesToTransfer);
		audioOut += SamplesToTransfer * 2;

		MixTransferRemaining -= SamplesToTransfer;
		SamplesLeft -= SamplesToTransfer;
	}
}

/* 8bb:
** Fixes sample end bytes for interpolation (yes, we have room after the data).
** Sustain loops are always handled as non-looping during fix in IT2.
*/
static void SB16_FixSamples(void)
{
	sample_t *s = Song.Smp;
	for (int32_t i = 0; i < Song.Header.SmpNum; i++, s++)
	{
		if (s->Data == NULL || s->Length == 0)
			continue;

		int8_t *data8 = (int8_t *)s->Data;
		const bool Sample16Bit = !!(s->Flags & SMPF_16BIT);
		const bool HasLoop = !!(s->Flags & SMPF_USE_LOOP);

		int8_t *smp8Ptr = &data8[s->Length << Sample16Bit];

		// 8bb: added this protection for looped samples
		if (HasLoop && s->LoopEnd-s->LoopBegin < 2)
		{
			*smp8Ptr++ = 0;
			*smp8Ptr++ = 0;
			return;
		}

		int8_t byte1 = 0;
		int8_t byte2 = 0;

		if (HasLoop)
		{
			int32_t src;
			if (s->Flags & SMPF_LOOP_PINGPONG)
			{
				src = s->LoopEnd - 2;
				if (src < 0)
					src = 0;
			}
			else // 8bb: forward loop
			{
				src = s->LoopBegin;
			}

			if (Sample16Bit)
				src <<= 1;

			byte1 = data8[src+0];
			byte2 = data8[src+1];
		}

		*smp8Ptr++ = byte1;
		*smp8Ptr++ = byte2;
	}
}

static void SB16_CloseDriver(void)
{
	if (MixBuffer != NULL)
	{
		free(MixBuffer);
		MixBuffer = NULL;
	}

	DriverClose = NULL;
	DriverMix = NULL;
	DriverSetTempo = NULL;
	DriverSetMixVolume = NULL;
	DriverFixSamples = NULL;
	DriverResetMixer = NULL;
	DriverPostMix = NULL;
	DriverMixSamples = NULL;
}

bool SB16_InitDriver(int32_t mixingFrequency)
{
	if (mixingFrequency < 8000)
		mixingFrequency = 8000;
	else if (mixingFrequency > 64000)
		mixingFrequency = 64000;

	const int32_t MaxSamplesToMix = (((mixingFrequency << 1) + (mixingFrequency >> 1)) / LOWEST_BPM_POSSIBLE) + 1;

	MixBuffer = (int32_t *)malloc(MaxSamplesToMix * 2 * sizeof (int32_t));
	if (MixBuffer == NULL)
		return false;

	Driver.Flags = DF_SUPPORTS_MIDI;
	Driver.NumChannels = 64;
	Driver.MixSpeed = mixingFrequency;

	// 8bb: setup driver functions
	DriverClose = SB16_CloseDriver;
	DriverMix = SB16_Mix; // 8bb: added this (original driver uses IRQ callback)
	DriverSetTempo = SB16_SetTempo;
	DriverSetMixVolume = SB16_SetMixVolume;
	DriverFixSamples = SB16_FixSamples;
	DriverResetMixer = SB16_ResetMixer; // 8bb: added this
	DriverPostMix = SB16_PostMix; // 8bb: added this
	DriverMixSamples = SB16_MixSamples; // 8bb: added this

	/*
	** MixMode 0 = "32 Bit Non-interpolated"
	** MixMode 1 = "32 Bit Interpolated"
	*/
	Driver.MixMode = 1; // 8bb: "32 Bit Interpolated"
	return true;
}
