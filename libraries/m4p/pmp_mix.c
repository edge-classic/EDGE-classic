#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "pmplay.h"
#include "pmp_main.h"
#include "snd_masm.h"
#include "ft_tables.h"

// fast 32-bit -> 16-bit clamp
#define CLAMP16(i) if ((int16_t)(i) != i) i = 0x7FFF ^ (i >> 31)

static bool dump_Flag;
static int32_t oldReplayRate;

// globalized
int16_t chnReloc[32];
int32_t *CDA_MixBuffer = NULL;
CIType CI[32 * 2];
// ------------

static void mix_UpdateChannel(int32_t nr, WaveChannelInfoType *WCI);

void P_SetSpeed(uint16_t bpm)
{
	// 8bb: added this
	if (bpm == 0)
		bpm = 125;

	speedVal = ((realReplayRate + realReplayRate) + (realReplayRate >> 1)) / bpm; // 8bb: same as doing "((realReplayRate * 5) / 2) / bpm"
}

void P_StartTone(sampleTyp *s, int32_t smpStartPos)
{
	WaveChannelInfoType WCI;

	WCI.SStartPos = smpStartPos;
	WCI.SBase = s->pek;
	WCI.SLen = s->len;
	WCI.SRepS = s->repS;
	WCI.SRepL = s->repL;
	WCI.SType = s->typ;
	WCI.Status = Status_StartTone+Status_StopTone;

	mix_UpdateChannel(PMPTmpActiveChannel, &WCI);
}

// 8bb: added these two
bool mix_Init(int32_t audioBufferSize)
{
	CDA_MixBuffer = (int32_t *)malloc(audioBufferSize * 2 * sizeof (int32_t));
	if (CDA_MixBuffer == NULL)
		return false;

	PMPLeft = 0;
	return true;
}

void mix_Free(void)
{
	if (CDA_MixBuffer != NULL)
	{
		free(CDA_MixBuffer);
		CDA_MixBuffer = NULL;
	}
}
// --------------------

static void updateVolume(CIType *v, int32_t volIPLen)
{
	const uint32_t vol = v->SVol * CDA_Amp;

	v->SLVol1 = (vol * panningTab[256-v->SPan]) >> (32-28);
	v->SRVol1 = (vol * panningTab[    v->SPan]) >> (32-28);

	if (volumeRampingFlag)
	{
		v->SLVolIP = (v->SLVol1 - v->SLVol2) / volIPLen;
		v->SRVolIP = (v->SRVol1 - v->SRVol2) / volIPLen;
		v->SVolIPLen = volIPLen;
	}
}

static void mix_UpdateChannel(int32_t nr, WaveChannelInfoType *WCI)
{
	CIType *v = &CI[chnReloc[nr]];
	const uint8_t status = WCI->Status;

	if (status & Status_StopTone)
	{
		if (volumeRampingFlag)
		{
			// 8bb: fade out current voice
			v->SType |= SType_Fadeout;
			v->SVol = 0;
			updateVolume(v, quickVolSizeVal);

			// 8bb: swap current voice with neighbor
			chnReloc[nr] ^= 1;
			v = &CI[chnReloc[nr]];
		}

		v->SType = SType_Off;
	}

	if (status & Status_SetPan)
		v->SPan = (uint8_t)WCI->SPan;

	if (status & Status_SetVol)
	{
		uint16_t vol = WCI->SVol;
		if (vol > 0) vol--; // 8bb: 0..256 -> 0..255 ( FT2 does this to prevent mul overflow in updateVolume() )
		v->SVol = (uint8_t)vol;
	}

	if (status & (Status_SetVol+Status_SetPan))
		updateVolume(v, (status & Status_QuickVol) ? quickVolSizeVal : speedVal);

	if (status & Status_SetFrq)
		v->SFrq = WCI->SFrq;

	if (status & Status_StartTone)
	{
		int32_t len;

		uint8_t type = WCI->SType;
		const bool sample16Bit = (type >> 4) & 1;

		if (type & (SType_Fwd+SType_Rev))
		{
			int32_t repL = WCI->SRepL;
			int32_t repS = WCI->SRepS;

			if (sample16Bit)
			{
				repL >>= 1;
				repS >>= 1;

				v->SRevBase = (int16_t *)WCI->SBase + (repS+repS+repL);
			}
			else
			{
				v->SRevBase = (int8_t *)WCI->SBase + (repS+repS+repL);
			}

			v->SRepL = repL;
			v->SRepS = repS;

			len = repS + repL;
		}
		else
		{
			type &= ~(SType_Fwd+SType_Rev); // 8bb: keep loop flags only

			len = WCI->SLen;
			if (sample16Bit)
				len >>= 1;

			if (len == 0)
				return;
		}
		
		// 8bb: overflown 9xx (set sample offset), cut voice (voice got ended earlier in "if (status & Status_StopTone)")
		if (WCI->SStartPos >= len)
			return;

		v->SLen = len;
		v->SPos = WCI->SStartPos;
		v->SPosDec = 0;
		v->SBase = WCI->SBase;
		v->SMixType = (sample16Bit * 4) + (volumeRampingFlag * 2) + interpolationFlag;
		v->SType = type;
	}
}

static void mix_UpdateChannelVolPanFrq(void)
{
	WaveChannelInfoType WCI;

	stmTyp *ch = stm;
	for (int32_t i = 0; i < song.antChn; i++, ch++)
	{
		uint8_t newStatus = 0;

		const uint8_t status = ch->status;
		ch->status = 0;

		if (status == 0)
			continue;

		if (status & IS_Vol)
		{
			WCI.SVol = ch->finalVol;
			newStatus |= Status_SetVol;
		}

		if (status & IS_QuickVol)
			newStatus |= Status_QuickVol;

		if (status & IS_Pan)
		{
			WCI.SPan = ch->finalPan;
			newStatus |= Status_SetPan;
		}

		if (status & IS_Period)
		{
			WCI.SFrq = getFrequenceValue(ch->finalPeriod);
			newStatus |= Status_SetFrq;
		}

		WCI.Status = newStatus;
		mix_UpdateChannel(i, &WCI);
	}
}

void mix_ClearChannels(void) // 8bb: rewritten to handle all voices instead of song.antChn
{
	memset(CI, 0, sizeof (CI));

	CIType *v = CI;
	for (int16_t i = 0; i < 32*2; i++, v++)
	{
		v->SPan = 128;
		v->SType = SType_Off;
	}

	for (int16_t i = 0; i < 32; i++)
		chnReloc[i] = i+i;
}

static void mix_SaveIPVolumes(void)
{
	CIType *v = CI;
	for (int32_t i = 0; i < song.antChn*2; i++, v++)
	{
		// 8bb: this cuts any active fade-out voices (volume ramping)
		if (v->SType & SType_Fadeout)
			v->SType = SType_Off;

		v->SLVol2 = v->SLVol1;
		v->SRVol2 = v->SRVol1;
		v->SVolIPLen = 0;
	}
}

void mix_UpdateBuffer(int16_t *buffer, int32_t numSamples)
{
	if (numSamples <= 0)
		return;

	if (musicPaused) // silence output
	{
		memset(buffer, 0, numSamples * (2 * sizeof (int16_t)));
		return;
	}

	memset(CDA_MixBuffer, 0, numSamples * (2 * sizeof (int32_t)));

	int32_t c = 0;
	int32_t a = numSamples;

	while (a > 0)
	{
		if (PMPLeft == 0)
		{
			mix_SaveIPVolumes();
			mainPlayer();
			mix_UpdateChannelVolPanFrq();
			PMPLeft = speedVal;
		}

		int32_t b = a;
		if (b > PMPLeft)
			b = PMPLeft;

		CIType *v = CI;
		for (int32_t i = 0; i < song.antChn*2; i++, v++)
			PMPMix32Proc(v, b, c);

		c += b;
		a -= b;
		PMPLeft -= b;
	}

	numSamples *= 2; // 8bb: stereo

	/* 8bb: Done a bit differently since we don't use a
	** Sound Blaster with its master volume setting.
	** Instead we change the amplitude here.
	*/

	if (masterVol == 256) // 8bb: max master volume, no need to change amp
	{
		for (int32_t i = 0; i < numSamples; i++)
		{
			int32_t out32 = CDA_MixBuffer[i] >> 8;
			CLAMP16(out32);
			buffer[i] = (int16_t)out32;
		}
	}
	else
	{
		for (int32_t i = 0; i < numSamples; i++)
		{
			int32_t out32 = CDA_MixBuffer[i] >> 8;
			CLAMP16(out32);
			out32 = (out32 * masterVol) >> 8;
			buffer[i] = (int16_t)out32;
		}
	}
}

void mix_UpdateBufferFloat(float *buffer, int32_t numSamples)
{
	if (numSamples <= 0)
		return;

	if (musicPaused) // silence output
	{
		memset(buffer, 0, numSamples * (2 * sizeof (int16_t)));
		return;
	}

	memset(CDA_MixBuffer, 0, numSamples * (2 * sizeof (int32_t)));

	int32_t c = 0;
	int32_t a = numSamples;

	while (a > 0)
	{
		if (PMPLeft == 0)
		{
			mix_SaveIPVolumes();
			mainPlayer();
			mix_UpdateChannelVolPanFrq();
			PMPLeft = speedVal;
		}

		int32_t b = a;
		if (b > PMPLeft)
			b = PMPLeft;

		CIType *v = CI;
		for (int32_t i = 0; i < song.antChn*2; i++, v++)
			PMPMix32Proc(v, b, c);

		c += b;
		a -= b;
		PMPLeft -= b;
	}

	numSamples *= 2; // 8bb: stereo

	/* 8bb: Done a bit differently since we don't use a
	** Sound Blaster with its master volume setting.
	** Instead we change the amplitude here.
	*/

#if defined _MSC_VER || (defined __SIZEOF_FLOAT__ && __SIZEOF_FLOAT__ == 4)
	if (masterVol == 256) // 8bb: max master volume, no need to change amp
	{
		for (int32_t i = 0; i < numSamples; i++)
		{
			int32_t out32 = CDA_MixBuffer[i] >> 8;
			CLAMP16(out32);
			*(uint32_t *)buffer = 0x43818000^((uint16_t)out32);
			*buffer++ -= 259.0f;
		}
	}
	else
	{
		for (int32_t i = 0; i < numSamples; i++)
		{
			int32_t out32 = CDA_MixBuffer[i] >> 8;
			CLAMP16(out32);
			out32 = (out32 * masterVol) >> 8;
			*(uint32_t *)buffer = 0x43818000^((uint16_t)out32);
			*buffer++ -= 259.0f;
		}
	}
#else
	if (masterVol == 256) // 8bb: max master volume, no need to change amp
	{
		for (int32_t i = 0; i < numSamples; i++)
		{
			int32_t out32 = CDA_MixBuffer[i] >> 8;
			CLAMP16(out32);
			*buffer++ = (float)out32 * 0.000030517578125f;
		}
	}
	else
	{
		for (int32_t i = 0; i < numSamples; i++)
		{
			int32_t out32 = CDA_MixBuffer[i] >> 8;
			CLAMP16(out32);
			out32 = (out32 * masterVol) >> 8;
			*buffer++ = (float)out32 * 0.000030517578125f;
		}
	}
#endif
}

bool dump_Init(int32_t frq, int32_t amp, int16_t songPos)
{
	setPos(songPos, 0);

	oldReplayRate = realReplayRate;

	realReplayRate = frq;
	updateReplayRate();
	CDA_Amp = 8 * amp;

	mix_ClearChannels();
	stopVoices();
	song.globVol = 64;
	speedVal = (frq*5 / 2) / song.speed;
	quickVolSizeVal = frq / 200;

	dump_Flag = false;
	return true;
}

void dump_Close(void)
{
	stopVoices();
	realReplayRate = oldReplayRate;
	updateReplayRate();
}

bool dump_EndOfTune(int32_t endSongPos)
{
	bool returnValue = (dump_Flag && song.pattPos == 0 && song.timer == 1) || (song.tempo == 0);

	// 8bb: FT2 bugfix for EEx (pattern delay) on first row of a pattern
	if (song.pattDelTime2 > 0)
		returnValue = false;

	if (song.songPos == endSongPos && song.pattPos == 0 && song.timer == 1)
		dump_Flag = true;

	return returnValue;
}

int32_t dump_GetFrame(int16_t *p) // 8bb: returns bytes mixed to 16-bit stereo buffer
{
	mix_SaveIPVolumes();
	mainPlayer();
	mix_UpdateChannelVolPanFrq();

	memset(CDA_MixBuffer, 0, speedVal * (2 * sizeof (int32_t)));

	CIType *v = CI;
	for (int32_t i = 0; i < song.antChn*2; i++, v++)
		PMPMix32Proc(v, speedVal, 0);

	const int32_t numSamples = speedVal * 2; // 8bb: *2 for stereo
	for (int32_t i = 0; i < numSamples; i++)
	{
		int32_t out32 = CDA_MixBuffer[i] >> 8;
		CLAMP16(out32);
		p[i] = (int16_t)out32;
	}

	return speedVal * (2 * sizeof (int16_t));
}
