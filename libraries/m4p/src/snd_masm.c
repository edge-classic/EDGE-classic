#include <stdint.h>
#include <stdbool.h>
#include "snd_masm.h"
#include "pmplay.h"

/* 8bb: This is done in a slightly different way, but the result
** is the same (bit-accurate to FT2.08/FT2.09 w/ SB16, and WAV-writer).
**
** Mixer macros are stored in snd_masm.h
*/

void PMPMix32Proc(CIType *v, int32_t numSamples, int32_t bufferPos)
{
	if (v->SType & SType_Off)
		return; // voice is not active

	uint32_t volStatus = v->SLVol1 | v->SRVol1;
	if (volumeRampingFlag)
		volStatus |= v->SLVol2 | v->SRVol2;

	if (volStatus == 0) // silence mix
	{
		const uint64_t samplesToMix = (uint64_t)v->SFrq * (uint32_t)numSamples; // 16.16fp

		const int32_t samples = (int32_t)(samplesToMix >> 16);
		const int32_t samplesFrac = (samplesToMix & 0xFFFF) + (v->SPosDec >> 16);

		int32_t realPos = v->SPos + samples + (samplesFrac >> 16);
		int32_t posFrac = samplesFrac & 0xFFFF;

		if (realPos >= v->SLen)
		{
			uint8_t SType = v->SType;
			if (SType & (SType_Fwd+SType_Rev))
			{
				do
				{
					SType ^= SType_RevDir;
					realPos -= v->SRepL;
				}
				while (realPos >= v->SLen);
				v->SType = SType;
			}
			else
			{
				v->SType = SType_Off;
				return;
			}
		}

		v->SPosDec = posFrac << 16;
		v->SPos = realPos;
	}
	else // normal mixing
	{
		bool mixInCenter;
		if (volumeRampingFlag)
			mixInCenter = (v->SLVol2 == v->SRVol2) && (v->SLVolIP == v->SRVolIP);
		else
			mixInCenter = v->SLVol1 == v->SRVol1;

		mixRoutineTable[(mixInCenter * 8) + v->SMixType](v, numSamples, bufferPos);
	}
}

static void mix8b(CIType *v, uint32_t numSamples, uint32_t bufferPos)
{
	int32_t sample;

	GET_VOL
	GET_MIXER_VARS
	SET_BASE8

	int32_t CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		CDA_BytesLeft -= samplesToMix;

		HANDLE_POS_START
		for (i = 0; i < (samplesToMix & 3); i++)
		{
			MIX_8BIT
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			MIX_8BIT
			MIX_8BIT
			MIX_8BIT
			MIX_8BIT
		}
		HANDLE_POS_END
	}

	SET_BACK_MIXER_POS
}

static void mix8bIntrp(CIType *v, uint32_t numSamples, uint32_t bufferPos)
{
	int32_t sample, sample2;

	GET_VOL
	GET_MIXER_VARS
	SET_BASE8

	int32_t CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		CDA_BytesLeft -= samplesToMix;

		HANDLE_POS_START
		for (i = 0; i < (samplesToMix & 3); i++)
		{
			MIX_8BIT_INTRP
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			MIX_8BIT_INTRP
			MIX_8BIT_INTRP
			MIX_8BIT_INTRP
			MIX_8BIT_INTRP
		}
		HANDLE_POS_END
	}

	SET_BACK_MIXER_POS
}

static void mix8bRamp(CIType *v, uint32_t numSamples, uint32_t bufferPos)
{
	int32_t sample;

	GET_MIXER_VARS
	GET_RAMP_VARS
	SET_BASE8

	int32_t CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_RAMP
		CDA_BytesLeft -= samplesToMix;

		GET_VOL_RAMP
		HANDLE_POS_START
		for (i = 0; i < (samplesToMix & 3); i++)
		{
			MIX_8BIT
			VOL_RAMP
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			MIX_8BIT
			VOL_RAMP
			MIX_8BIT
			VOL_RAMP
			MIX_8BIT
			VOL_RAMP
			MIX_8BIT
			VOL_RAMP
		}
		HANDLE_POS_END
		SET_VOL_BACK
	}

	SET_BACK_MIXER_POS
}

static void mix8bRampIntrp(CIType *v, uint32_t numSamples, uint32_t bufferPos)
{
	int32_t sample, sample2;

	GET_MIXER_VARS
	GET_RAMP_VARS
	SET_BASE8

	int32_t CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_RAMP
		CDA_BytesLeft -= samplesToMix;

		GET_VOL_RAMP
		HANDLE_POS_START
		for (i = 0; i < (samplesToMix & 3); i++)
		{
			MIX_8BIT_INTRP
			VOL_RAMP
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			MIX_8BIT_INTRP
			VOL_RAMP
			MIX_8BIT_INTRP
			VOL_RAMP
			MIX_8BIT_INTRP
			VOL_RAMP
			MIX_8BIT_INTRP
			VOL_RAMP
		}
		HANDLE_POS_END
		SET_VOL_BACK
	}

	SET_BACK_MIXER_POS
}

static void mix16b(CIType *v, uint32_t numSamples, uint32_t bufferPos)
{
	int32_t sample;

	GET_VOL
	GET_MIXER_VARS
	SET_BASE16

	int32_t CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		CDA_BytesLeft -= samplesToMix;

		HANDLE_POS_START
		for (i = 0; i < (samplesToMix & 3); i++)
		{
			MIX_16BIT
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			MIX_16BIT
			MIX_16BIT
			MIX_16BIT
			MIX_16BIT
		}
		HANDLE_POS_END
	}

	SET_BACK_MIXER_POS
}

static void mix16bIntrp(CIType *v, uint32_t numSamples, uint32_t bufferPos)
{
	int32_t sample, sample2;

	GET_VOL
	GET_MIXER_VARS
	SET_BASE16

	int32_t CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		CDA_BytesLeft -= samplesToMix;

		HANDLE_POS_START
		for (i = 0; i < (samplesToMix & 3); i++)
		{
			MIX_16BIT_INTRP
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			MIX_16BIT_INTRP
			MIX_16BIT_INTRP
			MIX_16BIT_INTRP
			MIX_16BIT_INTRP
		}
		HANDLE_POS_END
	}

	SET_BACK_MIXER_POS
}

static void mix16bRamp(CIType *v, uint32_t numSamples, uint32_t bufferPos)
{
	int32_t sample;

	GET_MIXER_VARS
	GET_RAMP_VARS
	SET_BASE16

	int32_t CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_RAMP
		CDA_BytesLeft -= samplesToMix;

		GET_VOL_RAMP
		HANDLE_POS_START
		for (i = 0; i < (samplesToMix & 3); i++)
		{
			MIX_16BIT
			VOL_RAMP
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			MIX_16BIT
			VOL_RAMP
			MIX_16BIT
			VOL_RAMP
			MIX_16BIT
			VOL_RAMP
			MIX_16BIT
			VOL_RAMP
		}
		HANDLE_POS_END
		SET_VOL_BACK
	}

	SET_BACK_MIXER_POS
}

static void mix16bRampIntrp(CIType *v, uint32_t numSamples, uint32_t bufferPos)
{
	int32_t sample, sample2;

	GET_MIXER_VARS
	GET_RAMP_VARS
	SET_BASE16

	int32_t CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_RAMP
		CDA_BytesLeft -= samplesToMix;

		GET_VOL_RAMP
		HANDLE_POS_START
		for (i = 0; i < (samplesToMix & 3); i++)
		{
			MIX_16BIT_INTRP
			VOL_RAMP
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			MIX_16BIT_INTRP
			VOL_RAMP
			MIX_16BIT_INTRP
			VOL_RAMP
			MIX_16BIT_INTRP
			VOL_RAMP
			MIX_16BIT_INTRP
			VOL_RAMP
		}
		HANDLE_POS_END
		SET_VOL_BACK
	}

	SET_BACK_MIXER_POS
}

static void mix8bCenter(CIType *v, uint32_t numSamples, uint32_t bufferPos)
{
	int32_t sample;

	GET_VOL_CENTER
	GET_MIXER_VARS
	SET_BASE8

	int32_t CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		CDA_BytesLeft -= samplesToMix;

		HANDLE_POS_START
		for (i = 0; i < (samplesToMix & 3); i++)
		{
			MIX_8BIT_M
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			MIX_8BIT_M
			MIX_8BIT_M
			MIX_8BIT_M
			MIX_8BIT_M
		}
		HANDLE_POS_END
	}

	SET_BACK_MIXER_POS
}

static void mix8bIntrpCenter(CIType *v, uint32_t numSamples, uint32_t bufferPos)
{
	int32_t sample, sample2;

	GET_VOL_CENTER
	GET_MIXER_VARS
	SET_BASE8

	int32_t CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		CDA_BytesLeft -= samplesToMix;

		HANDLE_POS_START
		for (i = 0; i < (samplesToMix & 3); i++)
		{
			MIX_8BIT_INTRP_M
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			MIX_8BIT_INTRP_M
			MIX_8BIT_INTRP_M
			MIX_8BIT_INTRP_M
			MIX_8BIT_INTRP_M
		}
		HANDLE_POS_END
	}

	SET_BACK_MIXER_POS
}

static void mix8bRampCenter(CIType *v, uint32_t numSamples, uint32_t bufferPos)
{
	int32_t sample;

	GET_MIXER_VARS
	GET_RAMP_VARS
	SET_BASE8

	int32_t CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_RAMP
		CDA_BytesLeft -= samplesToMix;

		GET_VOL_RAMP
		HANDLE_POS_START
		for (i = 0; i < (samplesToMix & 3); i++)
		{
			MIX_8BIT_M
			VOL_RAMP
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			MIX_8BIT_M
			VOL_RAMP
			MIX_8BIT_M
			VOL_RAMP
			MIX_8BIT_M
			VOL_RAMP
			MIX_8BIT_M
			VOL_RAMP
		}
		HANDLE_POS_END
		SET_VOL_BACK
	}

	SET_BACK_MIXER_POS
}

static void mix8bRampIntrpCenter(CIType *v, uint32_t numSamples, uint32_t bufferPos)
{
	int32_t sample, sample2;

	GET_MIXER_VARS
	GET_RAMP_VARS
	SET_BASE8

	int32_t CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_RAMP
		CDA_BytesLeft -= samplesToMix;

		GET_VOL_RAMP
		HANDLE_POS_START
		for (i = 0; i < (samplesToMix & 3); i++)
		{
			MIX_8BIT_INTRP_M
			VOL_RAMP
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			MIX_8BIT_INTRP_M
			VOL_RAMP
			MIX_8BIT_INTRP_M
			VOL_RAMP
			MIX_8BIT_INTRP_M
			VOL_RAMP
			MIX_8BIT_INTRP_M
			VOL_RAMP
		}
		HANDLE_POS_END
		SET_VOL_BACK
	}

	SET_BACK_MIXER_POS
}

static void mix16bCenter(CIType *v, uint32_t numSamples, uint32_t bufferPos)
{
	int32_t sample;

	GET_VOL_CENTER
	GET_MIXER_VARS
	SET_BASE16

	int32_t CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		CDA_BytesLeft -= samplesToMix;

		HANDLE_POS_START
		for (i = 0; i < (samplesToMix & 3); i++)
		{
			MIX_16BIT_M
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			MIX_16BIT_M
			MIX_16BIT_M
			MIX_16BIT_M
			MIX_16BIT_M
		}
		HANDLE_POS_END
	}

	SET_BACK_MIXER_POS
}

static void mix16bIntrpCenter(CIType *v, uint32_t numSamples, uint32_t bufferPos)
{
	int32_t sample, sample2;

	GET_VOL_CENTER
	GET_MIXER_VARS
	SET_BASE16

	int32_t CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		CDA_BytesLeft -= samplesToMix;

		HANDLE_POS_START
		for (i = 0; i < (samplesToMix & 3); i++)
		{
			MIX_16BIT_INTRP_M
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			MIX_16BIT_INTRP_M
			MIX_16BIT_INTRP_M
			MIX_16BIT_INTRP_M
			MIX_16BIT_INTRP_M
		}
		HANDLE_POS_END
	}

	SET_BACK_MIXER_POS
}

static void mix16bRampCenter(CIType *v, uint32_t numSamples, uint32_t bufferPos)
{
	int32_t sample;

	GET_MIXER_VARS
	GET_RAMP_VARS
	SET_BASE16

	int32_t CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_RAMP
		CDA_BytesLeft -= samplesToMix;

		GET_VOL_RAMP
		HANDLE_POS_START
		for (i = 0; i < (samplesToMix & 3); i++)
		{
			MIX_16BIT_M
			VOL_RAMP
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			MIX_16BIT_M
			VOL_RAMP
			MIX_16BIT_M
			VOL_RAMP
			MIX_16BIT_M
			VOL_RAMP
			MIX_16BIT_M
			VOL_RAMP
		}
		HANDLE_POS_END
		SET_VOL_BACK
	}

	SET_BACK_MIXER_POS
}

static void mix16bRampIntrpCenter(CIType *v, uint32_t numSamples, uint32_t bufferPos)
{
	int32_t sample, sample2;

	GET_MIXER_VARS
	GET_RAMP_VARS
	SET_BASE16

	int32_t CDA_BytesLeft = numSamples;
	while (CDA_BytesLeft > 0)
	{
		LIMIT_MIX_NUM
		LIMIT_MIX_NUM_RAMP
		CDA_BytesLeft -= samplesToMix;

		GET_VOL_RAMP
		HANDLE_POS_START
		for (i = 0; i < (samplesToMix & 3); i++)
		{
			MIX_16BIT_INTRP_M
			VOL_RAMP
		}
		samplesToMix >>= 2;
		for (i = 0; i < samplesToMix; i++)
		{
			MIX_16BIT_INTRP_M
			VOL_RAMP
			MIX_16BIT_INTRP_M
			VOL_RAMP
			MIX_16BIT_INTRP_M
			VOL_RAMP
			MIX_16BIT_INTRP_M
			VOL_RAMP
		}
		HANDLE_POS_END
		SET_VOL_BACK
	}

	SET_BACK_MIXER_POS
}

mixRoutine mixRoutineTable[16] =
{
	(mixRoutine)mix8b,
	(mixRoutine)mix8bIntrp,
	(mixRoutine)mix8bRamp,
	(mixRoutine)mix8bRampIntrp,
	(mixRoutine)mix16b,
	(mixRoutine)mix16bIntrp,
	(mixRoutine)mix16bRamp,
	(mixRoutine)mix16bRampIntrp,
	(mixRoutine)mix8bCenter,
	(mixRoutine)mix8bIntrpCenter,
	(mixRoutine)mix8bRampCenter,
	(mixRoutine)mix8bRampIntrpCenter,
	(mixRoutine)mix16bCenter,
	(mixRoutine)mix16bIntrpCenter,
	(mixRoutine)mix16bRampCenter,
	(mixRoutine)mix16bRampIntrpCenter
};
