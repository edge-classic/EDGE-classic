// all of the comments in this file are written by me (8bitbubsy)

// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../it_music.h"
#include "../it_structs.h"
#include "../it_d_rm.h"

static void Decompress16BitData(int16_t *Dst, const uint8_t *Src, uint32_t BlockLen);
static void Decompress8BitData(int8_t *Dst, const uint8_t *Src, uint32_t BlockLen);
static bool LoadCompressed16BitSample(MEMFILE *m, sample_t *s, bool Stereo, bool DeltaEncoded);
static bool LoadCompressed8BitSample(MEMFILE *m, sample_t *s, bool Stereo, bool DeltaEncoded);

bool LoadIT(MEMFILE *m)
{
	/*
	** ===================================
	** =========== LOAD HEADER ===========
	** ===================================
	*/

	mseek(m, 4, SEEK_CUR);
	if (!ReadBytes(m, Song.Header.SongName, 25)) return false;
	mseek(m, 1+2, SEEK_CUR);
	if (!ReadBytes(m, &Song.Header.OrdNum, 2)) return false;
	if (!ReadBytes(m, &Song.Header.InsNum, 2)) return false;
	if (!ReadBytes(m, &Song.Header.SmpNum, 2)) return false;
	if (!ReadBytes(m, &Song.Header.PatNum, 2)) return false;
	if (!ReadBytes(m, &Song.Header.Cwtv, 2)) return false;
	if (!ReadBytes(m, &Song.Header.Cmwt, 2)) return false;
	if (!ReadBytes(m, &Song.Header.Flags, 2)) return false;
	if (!ReadBytes(m, &Song.Header.Special, 2)) return false;
	if (!ReadBytes(m, &Song.Header.GlobalVol, 1)) return false;
	if (!ReadBytes(m, &Song.Header.MixVolume, 1)) return false;
	if (!ReadBytes(m, &Song.Header.InitialSpeed, 1)) return false;
	if (!ReadBytes(m, &Song.Header.InitialTempo, 1)) return false;
	if (!ReadBytes(m, &Song.Header.PanSep, 1)) return false;
	mseek(m, 1, SEEK_CUR);
	if (!ReadBytes(m, &Song.Header.MessageLength, 2)) return false;
	if (!ReadBytes(m, &Song.Header.MessageOffset, 4)) return false;
	mseek(m, 4, SEEK_CUR); // skip unwanted stuff
	if (!ReadBytes(m, Song.Header.ChnlPan, MAX_HOST_CHANNELS)) return false;
	if (!ReadBytes(m, Song.Header.ChnlVol, MAX_HOST_CHANNELS)) return false;

	// IT2 doesn't do this test, but I do it for safety.
	if (Song.Header.OrdNum > MAX_ORDERS+1 || Song.Header.InsNum > MAX_INSTRUMENTS ||
		Song.Header.SmpNum > MAX_SAMPLES  || Song.Header.PatNum > MAX_PATTERNS)
	{
		return false;
	}

	// IT2 doesn't do this, but let's do it for safety
	if (Song.Header.MessageLength > MAX_SONGMSG_LENGTH)
		Song.Header.MessageLength = MAX_SONGMSG_LENGTH;

	Song.Header.SongName[25] = '\0'; // just in case...

	/* *absolute* lowest possible initial tempo is 31, we need to clamp
	** it for safety reasons (yes, IT2 can do 31 as initial tempo!).
	*/
	if (Song.Header.InitialTempo < LOWEST_BPM_POSSIBLE)
		Song.Header.InitialTempo = LOWEST_BPM_POSSIBLE;

	int32_t PtrListOffset = 192 + Song.Header.OrdNum;

	int16_t OrdersToLoad = Song.Header.OrdNum - 1; // IT2 does this (removes the count for the last 255 terminator)
	if (OrdersToLoad > 0)
	{
		if (!ReadBytes(m, Song.Orders, OrdersToLoad))
			return false;

		// fill rest of order list with 255
		if (OrdersToLoad < MAX_ORDERS)
			memset(&Song.Orders[OrdersToLoad], 255, MAX_ORDERS-OrdersToLoad);
	}
	else
	{
		memset(Song.Orders, 255, MAX_ORDERS);
	}

	mseek(m, 192 + Song.Header.OrdNum + ((Song.Header.InsNum + Song.Header.SmpNum + Song.Header.PatNum) * 4), SEEK_SET);

	// skip time data, if present
	if (Song.Header.Special & 2)
	{
		uint16_t NumTimerData;
		ReadBytes(m, &NumTimerData, 2);
		mseek(m, NumTimerData * 8, SEEK_CUR);
	}

	// read embedded MIDI configuration, if preset (needed for Zxx macros)
	char *MIDIDataArea = Music_GetMIDIDataArea();
	if (Song.Header.Special & 8)
		ReadBytes(m, MIDIDataArea, (9+16+128)*32);

	// load song message, if present
	if ((Song.Header.Special & 1) && Song.Header.MessageLength > 0 && Song.Header.MessageOffset > 0)
	{
		mseek(m, Song.Header.MessageOffset, SEEK_SET);
		mread(Song.Message, 1, Song.Header.MessageLength, m);
		Song.Message[MAX_SONGMSG_LENGTH] = '\0'; // just in case
	}

	/*
	** ===================================
	** ======== LOAD INSTRUMENTS =========
	** ===================================
	*/

	mseek(m, PtrListOffset, SEEK_SET);
	size_t InsPtrOffset = mtell(m);

	instrument_t *ins = Song.Ins;
	for (uint32_t i = 0; i < Song.Header.InsNum; i++, ins++)
	{
		mseek(m, InsPtrOffset + (i * 4), SEEK_SET);
		if (meof(m)) return false;

		uint32_t InsOffset;
		if (!ReadBytes(m, &InsOffset, 4)) return false;

		if (InsOffset == 0)
			continue;

		mseek(m, InsOffset, SEEK_SET);
		if (meof(m)) return false;

		if (Song.Header.Cmwt >= 0x200)
		{
			mseek(m, 4, SEEK_CUR); // skip unwanted stuff
			if (!ReadBytes(m, ins->DOSFilename, 13)) return false;
			if (!ReadBytes(m, &ins->NNA, 1)) return false;
			if (!ReadBytes(m, &ins->DCT, 1)) return false;
			if (!ReadBytes(m, &ins->DCA, 1)) return false;
			if (!ReadBytes(m, &ins->FadeOut, 2)) return false;
			if (!ReadBytes(m, &ins->PitchPanSep, 1)) return false;
			if (!ReadBytes(m, &ins->PitchPanCenter, 1)) return false;
			if (!ReadBytes(m, &ins->GlobVol, 1)) return false;
			if (!ReadBytes(m, &ins->DefPan, 1)) return false;
			if (!ReadBytes(m, &ins->RandVol, 1)) return false;
			if (!ReadBytes(m, &ins->RandPan, 1)) return false;
			mseek(m, 4, SEEK_CUR); // skip unwanted stuff
			if (!ReadBytes(m, ins->InstrumentName, 26)) return false;
			if (!ReadBytes(m, &ins->FilterCutoff, 1)) return false;
			if (!ReadBytes(m, &ins->FilterResonance, 1)) return false;
			if (!ReadBytes(m, &ins->MIDIChn, 1)) return false;
			if (!ReadBytes(m, &ins->MIDIProg, 1)) return false;
			if (!ReadBytes(m, &ins->MIDIBank, 2)) return false;
			if (!ReadBytes(m, &ins->SmpNoteTable, 2*120)) return false;

			// just in case
			ins->DOSFilename[12] = '\0';
			ins->InstrumentName[25] = '\0';

			// read envelopes
			for (uint32_t j = 0; j < 3; j++)
			{
				env_t *env;

				     if (j == 0) env = &ins->VolEnv;
				else if (j == 1) env = &ins->PanEnv;
				else             env = &ins->PitchEnv;

				if (!ReadBytes(m, &env->Flags, 1)) return false;
				if (!ReadBytes(m, &env->Num, 1)) return false;
				if (!ReadBytes(m, &env->LoopBegin, 1)) return false;
				if (!ReadBytes(m, &env->LoopEnd, 1)) return false;
				if (!ReadBytes(m, &env->SustainLoopBegin, 1)) return false;
				if (!ReadBytes(m, &env->SustainLoopEnd, 1)) return false;

				envNode_t *node = env->NodePoints;
				for (uint32_t k = 0; k < 25; k++, node++)
				{
					if (!ReadBytes(m, &node->Magnitude, 1)) return false;
					if (!ReadBytes(m, &node->Tick, 2)) return false;
				}

				mseek(m, 1, SEEK_CUR); // skip unwanted stuff
			}
		}
		else // old instruments (v1.xx)
		{
			mseek(m, 4, SEEK_CUR); // skip unwanted stuff
			if (!ReadBytes(m, ins->DOSFilename, 13)) return false;
			if (!ReadBytes(m, &ins->VolEnv.Flags, 1)) return false;
			if (!ReadBytes(m, &ins->VolEnv.LoopBegin, 1)) return false;
			if (!ReadBytes(m, &ins->VolEnv.LoopEnd, 1)) return false;
			if (!ReadBytes(m, &ins->VolEnv.SustainLoopBegin, 1)) return false;
			if (!ReadBytes(m, &ins->VolEnv.SustainLoopEnd, 1)) return false;
			mseek(m, 2, SEEK_CUR); // skip unwanted stuff
			if (!ReadBytes(m, &ins->FadeOut, 2)) return false;
			if (!ReadBytes(m, &ins->NNA, 1)) return false;
			if (!ReadBytes(m, &ins->DCT, 1)) return false;
			mseek(m, 4, SEEK_CUR); // skip unwanted stuff
			if (!ReadBytes(m, ins->InstrumentName, 26)) return false;
			mseek(m, 6, SEEK_CUR); // skip unwanted stuff
			if (!ReadBytes(m, &ins->SmpNoteTable, 2*120)) return false;

			ins->FadeOut *= 2;

			// just in case
			ins->DOSFilename[12] = '\0';
			ins->InstrumentName[25] = '\0';

			// set default values not present in old instrument
			ins->PitchPanCenter = 60;
			ins->GlobVol = 128;
			ins->DefPan = 32 + 128; // center + pan disabled

			mseek(m, 200, SEEK_CUR);

			// read volume envelope
			uint8_t j;
			for (j = 0; j < 25; j++)
			{
				uint16_t word;
				envNode_t *node = &ins->VolEnv.NodePoints[j];

				if (!ReadBytes(m, &word, 2)) return false;
				if (word == 0xFFFF)
					break; // end of envelope

				node->Tick = word & 0xFF;
				node->Magnitude = word >> 8;
			}

			ins->VolEnv.Num = j;

			ins->PanEnv.Num = 2;
			ins->PanEnv.NodePoints[1].Tick = 99;

			ins->PitchEnv.Num = 2;
			ins->PitchEnv.NodePoints[1].Tick = 99;
		}
	}

	/*
	** ===================================
	** ======= LOAD SAMPLE HEADERS =======
	** ===================================
	*/

	mseek(m, PtrListOffset + (Song.Header.InsNum * 4), SEEK_SET);
	size_t SmpPtrOffset = mtell(m);

	sample_t *s = Song.Smp;
	for (uint32_t i = 0; i < Song.Header.SmpNum; i++, s++)
	{
		mseek(m, SmpPtrOffset + (i * 4), SEEK_SET);
		if (meof(m)) return false;

		uint32_t SmpOffset;
		if (!ReadBytes(m, &SmpOffset, 4)) return false;

		if (SmpOffset == 0)
			continue;

		mseek(m, SmpOffset, SEEK_SET);
		if (meof(m)) return false;

		mseek(m, 4, SEEK_CUR); // skip unwanted stuff
		if (!ReadBytes(m, s->DOSFilename, 13)) return false;
		if (!ReadBytes(m, &s->GlobVol, 1)) return false;
		if (!ReadBytes(m, &s->Flags, 1)) return false;
		if (!ReadBytes(m, &s->Vol, 1)) return false;
		if (!ReadBytes(m, s->SampleName, 26)) return false;
		if (!ReadBytes(m, &s->Cvt, 1)) return false;
		if (!ReadBytes(m, &s->DefPan, 1)) return false;
		if (!ReadBytes(m, &s->Length, 4)) return false;
		if (!ReadBytes(m, &s->LoopBegin, 4)) return false;
		if (!ReadBytes(m, &s->LoopEnd, 4)) return false;
		if (!ReadBytes(m, &s->C5Speed, 4)) return false;
		if (!ReadBytes(m, &s->SustainLoopBegin, 4)) return false;
		if (!ReadBytes(m, &s->SustainLoopEnd, 4)) return false;
		if (!ReadBytes(m, &s->OffsetInFile, 4)) return false;
		if (!ReadBytes(m, &s->AutoVibratoSpeed, 1)) return false;
		if (!ReadBytes(m, &s->AutoVibratoDepth, 1)) return false;
		if (!ReadBytes(m, &s->AutoVibratoRate, 1)) return false;
		if (!ReadBytes(m, &s->AutoVibratoWaveform, 1)) return false;

		// just in case
		s->DOSFilename[12] = '\0';
		s->SampleName[25] = '\0';
	}

	/* ===================================
	** ======== LOAD SAMPLE DATA =========
	** ===================================
	*/

	s = Song.Smp;
	for (uint32_t i = 0; i < Song.Header.SmpNum; i++, s++)
	{
		if (s->OffsetInFile == 0 || !(s->Flags & SMPF_ASSOCIATED_WITH_HEADER))
			continue;

		mseek(m, s->OffsetInFile, SEEK_SET);
		if (meof(m))
			continue; // This WAS a return false...will I regret this? - Dasho
		
		bool Stereo = !!(s->Flags & SMPF_STEREO); // added stereo support
		bool Compressed = !!(s->Flags & SMPF_COMPRESSED);
		bool Sample16Bit = !!(s->Flags & SMPF_16BIT);
		bool SignedSamples = !!(s->Cvt & 1);
		bool DeltaEncoded = !!(s->Cvt & 4);

		if (DeltaEncoded && !Compressed)
			continue;

		if (s->Length == 0 || !(s->Flags & SMPF_ASSOCIATED_WITH_HEADER))
			continue; // safely skip this sample

		if (s->Cvt & 0b11111010)
			continue; // not supported

		if (!Music_AllocateSample(i, s->Length << Sample16Bit))
			return false;

		// added stereo support
		if (Stereo)
		{
			if (!Music_AllocateRightSample(i, s->Length << Sample16Bit))
				return false;
		}

		if (Compressed)
		{
			if (Sample16Bit)
			{
				if (!LoadCompressed16BitSample(m, s, Stereo, DeltaEncoded))
					return false;
			}
			else
			{
				if (!LoadCompressed8BitSample(m, s, Stereo, DeltaEncoded))
					return false;
			}
		}
		else
		{
			mread(s->Data, 1, s->Length, m);

			// added stereo support for custom HQ driver
			if (Stereo)
				mread(s->DataR, 1, s->Length, m);
		}

		// convert unsigned sample to signed
		if (!SignedSamples)
		{
			if (Sample16Bit)
			{
				int16_t *Ptr16 = (int16_t *)s->Data;
				for (uint32_t j = 0; j < s->Length; j++)
					Ptr16[j] ^= 0x8000;
			}
			else
			{
				int8_t *Ptr8 = (int8_t *)s->Data;
				for (uint32_t j = 0; j < s->Length; j++)
					Ptr8[j] ^= 0x80;
			}
		}

		if (Sample16Bit) // Music_AllocateSample() also set s->Length, divide by two if 16-bit
			s->Length >>= 1;
	}

	/*
	** ===================================
	** ========== LOAD PATTERNS ==========
	** ===================================
	*/

	mseek(m, PtrListOffset + (Song.Header.InsNum * 4) + (Song.Header.SmpNum * 4), SEEK_SET);
	size_t PatPtrOffset = mtell(m);

	pattern_t *p = Song.Patt;
	for (uint32_t i = 0; i < Song.Header.PatNum; i++, p++)
	{
		mseek(m, PatPtrOffset + (i * 4), SEEK_SET);
		if (meof(m))
			return false;

		uint32_t PatOffset;
		if (!ReadBytes(m, &PatOffset, 4)) return false;

		if (PatOffset == 0)
			continue;

		mseek(m, PatOffset, SEEK_SET);
		if (meof(m))
			return false;

		uint16_t PatLength;
		if (!ReadBytes(m, &PatLength, 2)) return false;
		if (!ReadBytes(m, &p->Rows, 2)) return false;

		if (PatLength == 0 || p->Rows == 0)
			continue;

		mseek(m, 4, SEEK_CUR);

		if (!Music_AllocatePattern(i, PatLength)) return false;
		if (!ReadBytes(m, p->PackedData, PatLength)) return false;
	}

	return true;
}

static void Decompress16BitData(int16_t *Dst, const uint8_t *Src, uint32_t BlockLength)
{
	uint8_t Byte8, BitDepth, BitDepthInv, BitsRead;
	uint16_t Bytes16, LastVal;
	uint32_t Bytes32;

	LastVal = 0;
	BitDepth = 17;
	BitDepthInv = BitsRead = 0;

	BlockLength >>= 1;
	while (BlockLength != 0)
	{
		Bytes32 = (*(uint32_t *)Src) >> BitsRead;

		BitsRead += BitDepth;
		Src += BitsRead >> 3;
		BitsRead &= 7;

		if (BitDepth <= 6)
		{
			Bytes32 <<= BitDepthInv & 0x1F;

			Bytes16 = (uint16_t)Bytes32;
			if (Bytes16 != 0x8000)
			{
				LastVal += (int16_t)Bytes16 >> (BitDepthInv & 0x1F); // arithmetic shift
				*Dst++ = LastVal;
				BlockLength--;
			}
			else
			{
				Byte8 = ((Bytes32 >> 16) & 0xF) + 1;
				if (Byte8 >= BitDepth)
					Byte8++;
				BitDepth = Byte8;

				BitDepthInv = 16;
				if (BitDepthInv < BitDepth)
					BitDepthInv++;
				BitDepthInv -= BitDepth;

				BitsRead += 4;
			}

			continue;
		}

		Bytes16 = (uint16_t)Bytes32;

		if (BitDepth <= 16)
		{
			uint16_t DX = 0xFFFF >> (BitDepthInv & 0x1F);
			Bytes16 &= DX;
			DX = (DX >> 1) - 8;

			if (Bytes16 > DX+16 || Bytes16 <= DX)
			{
				Bytes16 <<= BitDepthInv & 0x1F;
				Bytes16 = (int16_t)Bytes16 >> (BitDepthInv & 0x1F); // arithmetic shift
				LastVal += Bytes16;
				*Dst++ = LastVal;
				BlockLength--;
				continue;
			}

			Byte8 = (uint8_t)(Bytes16 - DX);
			if (Byte8 >= BitDepth)
				Byte8++;
			BitDepth = Byte8;

			BitDepthInv = 16;
			if (BitDepthInv < BitDepth)
				BitDepthInv++;
			BitDepthInv -= BitDepth;
			continue;
		}

		if (Bytes32 & 0x10000)
		{
			BitDepth = (uint8_t)(Bytes16 + 1);
			BitDepthInv = 16 - BitDepth;
		}
		else
		{
			LastVal += Bytes16;
			*Dst++ = LastVal;
			BlockLength--;
		}
	}
}

static void Decompress8BitData(int8_t *Dst, const uint8_t *Src, uint32_t BlockLength)
{
	uint8_t LastVal, Byte8, BitDepth, BitDepthInv, BitsRead;
	uint16_t Bytes16;

	LastVal = 0;
	BitDepth = 9;
	BitDepthInv = BitsRead = 0;

	while (BlockLength != 0)
	{
		Bytes16 = (*(uint16_t *)Src) >> BitsRead;

		BitsRead += BitDepth;
		Src += (BitsRead >> 3);
		BitsRead &= 7;

		Byte8 = Bytes16 & 0xFF;

		if (BitDepth <= 6)
		{
			Bytes16 <<= (BitDepthInv & 0x1F);
			Byte8 = Bytes16 & 0xFF;

			if (Byte8 != 0x80)
			{
				LastVal += (int8_t)Byte8 >> (BitDepthInv & 0x1F); // arithmetic shift
				*Dst++ = LastVal;
				BlockLength--;
				continue;
			}

			Byte8 = (Bytes16 >> 8) & 7;
			BitsRead += 3;
			Src += (BitsRead >> 3);
			BitsRead &= 7;
		}
		else
		{
			if (BitDepth == 8)
			{
				if (Byte8 < 0x7C || Byte8 > 0x83)
				{
					LastVal += Byte8;
					*Dst++ = LastVal;
					BlockLength--;
					continue;
				}
				Byte8 -= 0x7C;
			}
			else if (BitDepth < 8)
			{
				Byte8 <<= 1;
				if (Byte8 < 0x78 || Byte8 > 0x86)
				{
					LastVal += (int8_t)Byte8 >> (BitDepthInv & 0x1F); // arithmetic shift
					*Dst++ = LastVal;
					BlockLength--;
					continue;
				}
				Byte8 = (Byte8 >> 1) - 0x3C;
			}
			else
			{
				Bytes16 &= 0x1FF;
				if ((Bytes16 & 0x100) == 0)
				{
					LastVal += Byte8;
					*Dst++ = LastVal;
					BlockLength--;
					continue;
				}
			}
		}

		Byte8++;
		if (Byte8 >= BitDepth)
			Byte8++;
		BitDepth = Byte8;

		BitDepthInv = 8;
		if (BitDepthInv < BitDepth)
			BitDepthInv++;
		BitDepthInv -= BitDepth;
	}
}

static bool LoadCompressed16BitSample(MEMFILE *m, sample_t *s, bool Stereo, bool DeltaEncoded)
{
	int8_t *DstPtr = (int8_t *)s->Data;

	uint8_t *DecompBuffer = (uint8_t *)malloc(65536);
	if (DecompBuffer == NULL)
		return false;

	uint32_t i = s->Length;
	while (i > 0)
	{
		uint32_t BytesToUnpack = 32768;
		if (BytesToUnpack > i)
			BytesToUnpack = i;

		uint16_t PackedLen;
		mread(&PackedLen, sizeof (uint16_t), 1, m);
		mread(DecompBuffer, 1, PackedLen, m);

		Decompress16BitData((int16_t *)DstPtr, DecompBuffer, BytesToUnpack);

		if (DeltaEncoded) // convert from delta values to PCM
		{
			int16_t *Ptr16 = (int16_t *)DstPtr;
			int16_t LastSmp16 = 0; // yes, reset this every block!

			const uint32_t Length = BytesToUnpack >> 1;
			for (uint32_t j = 0; j < Length; j++)
			{
				LastSmp16 += Ptr16[j];
				Ptr16[j] = LastSmp16;
			}
		}

		DstPtr += BytesToUnpack;
		i -= BytesToUnpack;
	}

	if (Stereo) // added stereo support for custom HQ driver
	{
		DstPtr = (int8_t *)s->DataR;

		i = s->Length;
		while (i > 0)
		{
			uint32_t BytesToUnpack = 32768;
			if (BytesToUnpack > i)
				BytesToUnpack = i;

			uint16_t PackedLen;
			mread(&PackedLen, sizeof (uint16_t), 1, m);
			mread(DecompBuffer, 1, PackedLen, m);

			Decompress16BitData((int16_t *)DstPtr, DecompBuffer, BytesToUnpack);

			if (DeltaEncoded) // convert from delta values to PCM
			{
				int16_t *Ptr16 = (int16_t *)DstPtr;
				int16_t LastSmp16 = 0; // yes, reset this every block!

				const uint32_t Length = BytesToUnpack >> 1;
				for (uint32_t j = 0; j < Length; j++)
				{
					LastSmp16 += Ptr16[j];
					Ptr16[j] = LastSmp16;
				}
			}

			DstPtr += BytesToUnpack;
			i -= BytesToUnpack;
		}
	}

	free(DecompBuffer);
	return true;
}

static bool LoadCompressed8BitSample(MEMFILE *m, sample_t *s, bool Stereo, bool DeltaEncoded)
{
	int8_t *DstPtr = (int8_t *)s->Data;

	uint8_t *DecompBuffer = (uint8_t *)malloc(65536);
	if (DecompBuffer == NULL)
		return false;

	uint32_t i = s->Length;
	while (i > 0)
	{
		uint32_t BytesToUnpack = 32768;
		if (BytesToUnpack > i)
			BytesToUnpack = i;

		uint16_t PackedLen;
		mread(&PackedLen, sizeof (uint16_t), 1, m);
		mread(DecompBuffer, 1, PackedLen, m);

		Decompress8BitData(DstPtr, DecompBuffer, BytesToUnpack);

		if (DeltaEncoded) // convert from delta values to PCM
		{
			int8_t LastSmp8 = 0; // yes, reset this every block!
			for (uint32_t j = 0; j < BytesToUnpack; j++)
			{
				LastSmp8 += DstPtr[j];
				DstPtr[j] = LastSmp8;
			}
		}

		DstPtr += BytesToUnpack;
		i -= BytesToUnpack;
	}

	if (Stereo) // added stereo support for custom HQ driver
	{
		DstPtr = (int8_t *)s->DataR;

		i = s->Length;
		while (i > 0)
		{
			uint32_t BytesToUnpack = 32768;
			if (BytesToUnpack > i)
				BytesToUnpack = i;

			uint16_t PackedLen;
			mread(&PackedLen, sizeof (uint16_t), 1, m);
			mread(DecompBuffer, 1, PackedLen, m);

			Decompress8BitData(DstPtr, DecompBuffer, BytesToUnpack);

			if (DeltaEncoded) // convert from delta values to PCM
			{
				int8_t LastSmp8 = 0; // yes, reset this every block!
				for (uint32_t j = 0; j < BytesToUnpack; j++)
				{
					LastSmp8 += DstPtr[j];
					DstPtr[j] = LastSmp8;
				}
			}

			DstPtr += BytesToUnpack;
			i -= BytesToUnpack;
		}
	}

	free(DecompBuffer);
	return true;
}
