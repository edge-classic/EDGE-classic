#include "m4p.h"
#include <string.h>

// IT/S3M
extern bool Music_Init(int32_t mixingFrequency, int32_t mixingBufferSize);
extern bool Music_LoadFromData(uint8_t *Data, uint32_t DataLen);
extern void Music_PlaySong(uint16_t order);
extern void Music_FillAudioBuffer(int16_t *buffer, int32_t numSamples);
extern void Music_Close(void);
extern void Music_Stop(void);
extern void Music_FreeSong(void);

// XM/MOD/FT
extern bool initMusic(int32_t audioFrequency, int32_t audioBufferSize, bool interpolation, bool volumeRamping);
extern bool loadMusicFromData(const uint8_t *data, uint32_t dataLength);
extern void startPlaying(void);
extern void stopPlaying(void);
extern void mix_UpdateBuffer(int16_t *buffer, int32_t numSamples);
extern void stopMusic();
extern void freeMusic(void);

extern const char *MODSig[16]; // For format checking

enum
{
	FORMAT_UNKNOWN = 0,
	FORMAT_IT_S3M = 1,
	FORMAT_XM_MOD = 2
};

int current_format = FORMAT_UNKNOWN;

int m4p_TestFromData(uint8_t *Data, uint32_t DataLen)
{
	if ((DataLen >= 4 && (Data[0] == 'I' && Data[1] == 'M' &&
		Data[2] == 'P' && Data[3] == 'M')) || (DataLen >= 48 &&
		(Data[44] == 'S' && Data[45] == 'C' &&
		Data[46] == 'R' && Data[47] == 'M')))
	{
		return FORMAT_IT_S3M;
	}
	if (DataLen >= 17)
	{
		bool is_xm_mod = true;
		const char *hdrtxt = "Extended Module:";
		for (int i = 0; i < 16; i++)
		{
			if (Data[i] != *hdrtxt++)
			{
				is_xm_mod = false;
				break;
			}
		}
		if (is_xm_mod) return FORMAT_XM_MOD;
	}
	if (DataLen >= 1084)
	{
		for (uint8_t i = 0; i < 16; i++)
		{
			if (Data[1080] == MODSig[i][0] && Data[1081] == MODSig[i][1] &&
				Data[1082] == MODSig[i][2] && Data[1083] == MODSig[i][3])
				return FORMAT_XM_MOD;
		}
	}

	return FORMAT_UNKNOWN;
}

bool m4p_LoadFromData(uint8_t *Data, uint32_t DataLen, int32_t mixingFrequency, int32_t mixingBufferSize)
{
	current_format = m4p_TestFromData(Data, DataLen);

	if (current_format == FORMAT_IT_S3M)
	{
		if (Music_Init(mixingFrequency, mixingBufferSize))
			return Music_LoadFromData(Data, DataLen);
		else
			return false;
	}
	else if (current_format == FORMAT_XM_MOD)
	{
		if (initMusic(mixingFrequency, mixingBufferSize, true, true))
			return loadMusicFromData(Data, DataLen);
		else
			return false;
	}

	return false;
}

void m4p_PlaySong(void)
{
	if (current_format == FORMAT_IT_S3M)
		Music_PlaySong(0);
	else
		startPlaying();	
}

void m4p_GenerateSamples(int16_t *buffer, int32_t numSamples)
{
	if (current_format == FORMAT_IT_S3M)
		Music_FillAudioBuffer(buffer, numSamples);
	else
		mix_UpdateBuffer(buffer, numSamples);
}

void m4p_Stop(void)
{
	if (current_format == FORMAT_IT_S3M)
		Music_Stop();
	else
		stopPlaying();
}

void m4p_Close(void)
{
	if (current_format == FORMAT_IT_S3M)
		Music_Close();
	else
		stopMusic();
}

void m4p_FreeSong(void)
{
	if (current_format == FORMAT_IT_S3M)
		Music_FreeSong();
	else
		freeMusic();
}