//----------------------------------------------------------------------------
//  EDGE IMF to VGM conversion
//----------------------------------------------------------------------------
//
//  Copyright (c) 2015-2020 ValleyBell
//  Copyright (c) 2022  The EDGE Team.
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//----------------------------------------------------------------------------

#include "epi.h"

typedef struct _vgm_file_header
{
	u32_t fccVGM;
	u32_t lngEOFOffset;
	u32_t lngVersion;
	u32_t lngHzPSG;
	u32_t lngHzYM2413;
	u32_t lngGD3Offset;
	u32_t lngTotalSamples;
	u32_t lngLoopOffset;
	u32_t lngLoopSamples;
	u32_t lngRate;
	u16_t shtPSG_Feedback;
	u8_t bytPSG_SRWidth;
	u8_t bytPSG_Flags;
	u32_t lngHzYM2612;
	u32_t lngHzYM2151;
	u32_t lngDataOffset;
	u32_t lngHzSPCM;
	u32_t lngSPCMIntf;
	u32_t lngHzRF5C68;
	u32_t lngHzYM2203;
	u32_t lngHzYM2608;
	u32_t lngHzYM2610;
	u32_t lngHzYM3812;
	u32_t lngHzYM3526;
	u32_t lngHzY8950;
	u32_t lngHzYMF262;
	u32_t lngHzYMF278B;
	u32_t lngHzYMF271;
	u32_t lngHzYMZ280B;
	u32_t lngHzRF5C164;
	u32_t lngHzPWM;
	u32_t lngHzAY8910;
	u8_t bytAYType;
	u8_t bytAYFlag;
	u8_t bytAYFlagYM2203;
	u8_t bytAYFlagYM2608;
	u8_t bytVolumeModifier;
	u8_t bytReserved2;
	s8_t bytLoopBase;
	u8_t bytLoopModifier;
	u32_t lngHzGBDMG;
	u32_t lngHzNESAPU;
	u32_t lngHzMultiPCM;
	u32_t lngHzUPD7759;
	u32_t lngHzOKIM6258;
	u8_t bytOKI6258Flags;
	u8_t bytK054539Flags;
	u8_t bytC140Type;
	u8_t bytReservedFlags;
	u32_t lngHzOKIM6295;
	u32_t lngHzK051649;
	u32_t lngHzK054539;
	u32_t lngHzHuC6280;
	u32_t lngHzC140;
	u32_t lngHzK053260;
	u32_t lngHzPokey;
	u32_t lngHzQSound;
	u32_t lngHzSCSP;
//	u32_t lngHzOKIM6376;
	//u8_t bytReserved[0x04];
	u32_t lngExtraOffset;
	u32_t lngHzWSwan;
	u32_t lngHzVSU;
	u32_t lngHzSAA1099;
	u32_t lngHzES5503;
	u32_t lngHzES5506;
	u8_t bytES5503Chns;
	u8_t bytES5506Chns;
	u8_t bytC352ClkDiv;
	u8_t bytESReserved;
	u32_t lngHzX1_010;
	u32_t lngHzC352;
	u32_t lngHzGA20;
} VGM_HEADER;
typedef struct _vgm_header_extra
{
	u32_t DataSize;
	u32_t Chp2ClkOffset;
	u32_t ChpVolOffset;
} VGM_HDR_EXTRA;
typedef struct _vgm_extra_chip_data32
{
	u8_t Type;
	u32_t Data;
} VGMX_CHIP_DATA32;
typedef struct _vgm_extra_chip_data16
{
	u8_t Type;
	u8_t Flags;
	u16_t Data;
} VGMX_CHIP_DATA16;
typedef struct _vgm_extra_chip_extra32
{
	u8_t ChipCnt;
	VGMX_CHIP_DATA32* CCData;
} VGMX_CHP_EXTRA32;
typedef struct _vgm_extra_chip_extra16
{
	u8_t ChipCnt;
	VGMX_CHIP_DATA16* CCData;
} VGMX_CHP_EXTRA16;
typedef struct _vgm_header_extra_data
{
	VGMX_CHP_EXTRA32 Clocks;
	VGMX_CHP_EXTRA16 Volumes;
} VGM_EXTRA;

#define VOLUME_MODIF_WRAP	0xC0
typedef struct _vgm_gd3_tag
{
	u32_t fccGD3;
	u32_t lngVersion;
	u32_t lngTagLength;
	wchar_t* strTrackNameE;
	wchar_t* strTrackNameJ;
	wchar_t* strGameNameE;
	wchar_t* strGameNameJ;
	wchar_t* strSystemNameE;
	wchar_t* strSystemNameJ;
	wchar_t* strAuthorNameE;
	wchar_t* strAuthorNameJ;
	wchar_t* strReleaseDate;
	wchar_t* strCreator;
	wchar_t* strNotes;
} GD3_TAG;
typedef struct _vgm_pcm_bank_data
{
	u32_t DataSize;
	u8_t* Data;
	u32_t DataStart;
} VGM_PCM_DATA;
typedef struct _vgm_pcm_bank
{
	u32_t BankCount;
	VGM_PCM_DATA* Bank;
	u32_t DataSize;
	u8_t* Data;
	u32_t DataPos;
	u32_t BnkPos;
} VGM_PCM_BANK;

#define FCC_VGM	0x206D6756	// 'Vgm '
#define FCC_GD3	0x20336447	// 'Gd3 '

u8_t IMFType;
u32_t PITPeriod; // Counter value of 8254 Programmable Interval Timer
VGM_HEADER VGMHead;
u32_t IMFPos;
u32_t IMFDataStart;
u32_t IMFDataEnd;
u32_t VGMPos;
u8_t LoopOn = false;
u32_t vgm_header_size = sizeof(VGM_HEADER);

void ConvertIMF2VGM(u8_t *IMFBuffer, u32_t IMFBufferLen, u8_t *VGMBuffer, u32_t VGMBufferLen, s32_t IMFFreq, s32_t DevFreq)
{
	u16_t CurDelay;
	u32_t VGMSmplL;
	float  VGMSmplFraction;
	u32_t SmplVal;

	u16_t TempSht;
	memcpy(&TempSht, &IMFBuffer[0x00], 0x02);
	if (! TempSht)
		IMFType = 0x00;
	else
		IMFType = 0x01;

	switch (IMFFreq) {
		case 560: PITPeriod = 0x0850; break;
		case 280: PITPeriod = 0x10A1; break;
		case 700:
		case 701: PITPeriod = 0x06A6; break;
		default:  PITPeriod = 13125000 / (IMFFreq * 11);
	}
	I_Printf("IMF Type: %u, IMF Playback Rate: %u Hz (8254 PIT period 0x%04X)\n", IMFType, IMFFreq, PITPeriod);

	memcpy(&CurDelay, &IMFBuffer[0x00], 0x02);
	if (IMFType == 0x00)
	{
		IMFDataStart = 0x0000;
		IMFDataEnd = IMFBufferLen;
	}
	else //if (IMFType == 0x01)
	{
		IMFDataStart = 0x0002;
		IMFDataEnd = IMFDataStart + CurDelay;
	}

	// Generate VGM Header
	memset(&VGMHead, 0x00, sizeof(VGM_HEADER));
	VGMHead.fccVGM = FCC_VGM;
	VGMHead.lngVersion = 0x00000151;
	VGMHead.lngRate = IMFFreq;
	VGMHead.lngDataOffset = 0x80;
	VGMHead.lngHzYM3812 = 3579545;

	// Convert data
	IMFPos = IMFDataStart;
	VGMPos = VGMHead.lngDataOffset;
	VGMSmplL = 0;
	VGMSmplFraction =0;

	/* Add Waveform Select Enable write to beginning, as some files (e.g. from Commander Keen Episode 4) do not include it by themselves. */
	VGMBuffer[VGMPos++] = 0x5A;
	VGMBuffer[VGMPos++] = 0x01;
	VGMBuffer[VGMPos++] = 0x20;
	while(IMFPos < IMFDataEnd)
	{
		u16_t IMFDelayInIMFTicks;
		u32_t IMFDelayInPITTicks;
		float IMFDelayInMilliseconds;
		float IMFDelayInVGMSamplesFloat;
		u32_t IMFDelayInVGMSamplesInt;

		if (VGMPos >= VGMBufferLen - 0x08)
		{
			VGMBufferLen += 0x8000;
			VGMBuffer = (u8_t*)realloc(VGMBuffer, VGMBufferLen);
		}
		VGMBuffer[VGMPos + 0x00] = 0x5A;
		VGMBuffer[VGMPos + 0x01] = IMFBuffer[IMFPos + 0x00];	// register
		VGMBuffer[VGMPos + 0x02] = IMFBuffer[IMFPos + 0x01];	// data
		VGMPos += 0x03;

		IMFDelayInIMFTicks = (IMFBuffer[IMFPos + 2] | (IMFBuffer[IMFPos + 3] << 8));
		IMFPos += 0x04;

		/* Convert the delay time, specified relative to the IMF's rate (IMFDelayInIMFTicks)...
		   - first to the number of ticks of the 8254 Programmable Interval Timer's master clock (IMFDelayInPitTicks);
		   - from that to the number of milliseconds (IMFDelayInMilliseconds);
		   - from that to the number of samples.
		   Store the fractional component in VGMSmplFraction so that it is not lost between successive delays. */
		IMFDelayInPITTicks = IMFDelayInIMFTicks * PITPeriod;
		IMFDelayInMilliseconds = (float) IMFDelayInPITTicks * 11 / 13125;
		IMFDelayInVGMSamplesFloat = IMFDelayInMilliseconds * DevFreq / 1000 + VGMSmplFraction;
		IMFDelayInVGMSamplesInt = IMFDelayInVGMSamplesFloat;
		VGMSmplFraction = IMFDelayInVGMSamplesFloat - IMFDelayInVGMSamplesInt;

		VGMSmplL += IMFDelayInVGMSamplesInt; // Add the delay in samples to the total VGM file length in samples

		/* Store the delay, specified as the number of samples. */
		while (IMFDelayInVGMSamplesInt) {
			u32_t ThisCommandDelay = (IMFDelayInVGMSamplesInt > 65535) ? 65535 : IMFDelayInVGMSamplesInt;
			VGMBuffer[VGMPos++] = 0x61; // Wait n samples
			VGMBuffer[VGMPos++] = ThisCommandDelay & 0xFF;
			VGMBuffer[VGMPos++] = ThisCommandDelay >> 8;
			/* Enlarge output buffer, if necessary */
			if (VGMPos >= VGMBufferLen - 0x10) {
				VGMBufferLen += 0x8000;
				VGMBuffer = (u8_t*)realloc(VGMBuffer, VGMBufferLen);
			}
			IMFDelayInVGMSamplesInt -= ThisCommandDelay;
		}
	}
	VGMBuffer[VGMPos] = 0x66;
	VGMPos += 0x01;

	VGMBufferLen = VGMPos;
	VGMHead.lngEOFOffset = VGMBufferLen;
	VGMHead.lngTotalSamples = VGMSmplL;
	if (LoopOn)
	{
		VGMHead.lngLoopOffset = VGMHead.lngDataOffset;
		VGMHead.lngLoopSamples = VGMHead.lngTotalSamples;
	}

	SmplVal = VGMHead.lngDataOffset;
	if (SmplVal > sizeof(VGM_HEADER))
		SmplVal = sizeof(VGM_HEADER);
	VGMHead.lngEOFOffset -= 0x04;
	if (VGMHead.lngLoopOffset)
		VGMHead.lngLoopOffset -= 0x1C;
	VGMHead.lngDataOffset -= 0x34;
	memcpy(&VGMBuffer[0x00], &VGMHead, SmplVal);

	return;
}
