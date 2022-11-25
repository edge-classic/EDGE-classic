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

#include "VGMFile.h"

UINT8 IMFType;
UINT32 PITPeriod; // Counter value of 8254 Programmable Interval Timer
VGM_HEADER VGMHead;
UINT32 IMFPos;
UINT32 IMFDataStart;
UINT32 IMFDataEnd;
UINT32 VGMPos;
UINT8 LoopOn = false;
UINT32 vgm_header_size = sizeof(VGM_HEADER);

void ConvertIMF2VGM(UINT8 *IMFBuffer, UINT32 IMFBufferLen, UINT8 *VGMBuffer, UINT32 VGMBufferLen, INT32 IMFFreq, INT32 DevFreq)
{
	UINT16 CurDelay;
	UINT32 VGMSmplL;
	float  VGMSmplFraction;
	UINT32 SmplVal;

	UINT16 TempSht;
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
		UINT16 IMFDelayInIMFTicks;
		UINT32 IMFDelayInPITTicks;
		float IMFDelayInMilliseconds;
		float IMFDelayInVGMSamplesFloat;
		UINT32 IMFDelayInVGMSamplesInt;

		if (VGMPos >= VGMBufferLen - 0x08)
		{
			VGMBufferLen += 0x8000;
			VGMBuffer = (UINT8*)realloc(VGMBuffer, VGMBufferLen);
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
			UINT32 ThisCommandDelay = (IMFDelayInVGMSamplesInt > 65535) ? 65535 : IMFDelayInVGMSamplesInt;
			VGMBuffer[VGMPos++] = 0x61; // Wait n samples
			VGMBuffer[VGMPos++] = ThisCommandDelay & 0xFF;
			VGMBuffer[VGMPos++] = ThisCommandDelay >> 8;
			/* Enlarge output buffer, if necessary */
			if (VGMPos >= VGMBufferLen - 0x10) {
				VGMBufferLen += 0x8000;
				VGMBuffer = (UINT8*)realloc(VGMBuffer, VGMBufferLen);
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
