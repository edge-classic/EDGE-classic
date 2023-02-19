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

#define S3M_ROWS 64

static void ClearEncodingInfo(void);
static bool GetPatternLength(uint16_t Rows, uint16_t *LengthOut);
static void EncodePattern(pattern_t *p, uint8_t Rows);
static bool StorePattern(uint8_t NumRows, int32_t Pattern);
static bool TranslateS3MPattern(uint8_t *Src, int32_t Pattern);

static uint8_t *PatternDataArea, EncodingInfo[MAX_HOST_CHANNELS*6];

bool LoadS3M(MEMFILE *m)
{
	uint8_t DefPan;
	uint16_t Flags, SmpPtrs[100], PatPtrs[100];

	if (!ReadBytes(m, Song.Header.SongName, 25)) return false;
	mseek(m, 0x20, SEEK_SET);
	if (!ReadBytes(m, &Song.Header.OrdNum, 2)) return false;
	if (!ReadBytes(m, &Song.Header.SmpNum, 2)) return false;
	if (!ReadBytes(m, &Song.Header.PatNum, 2)) return false;
	if (!ReadBytes(m, &Flags, 2)) return false;

	mseek(m, 0x30, SEEK_SET);
	if (!ReadBytes(m, &Song.Header.GlobalVol, 1)) return false;
	if (!ReadBytes(m, &Song.Header.InitialSpeed, 1)) return false;
	if (!ReadBytes(m, &Song.Header.InitialTempo, 1)) return false;
	if (!ReadBytes(m, &Song.Header.MixVolume, 1)) return false;
	mseek(m, 1, SEEK_CUR);
	if (!ReadBytes(m, &DefPan, 1)) return false;

	if (Song.Header.SmpNum > 100)
		Song.Header.SmpNum = 100;

	if (Song.Header.PatNum > 100)
		Song.Header.PatNum = 100;

	Song.Header.Flags = ITF_OLD_EFFECTS;
	if (Flags & 8)
		Song.Header.Flags = ITF_VOL0_OPTIMIZATION;

	Song.Header.PanSep = 128;
	Song.Header.GlobalVol *= 2;

	if (Song.Header.MixVolume & 128)
	{
		Song.Header.Flags |= ITF_STEREO;
		Song.Header.MixVolume &= 127;
	}

	// OK, panning now...
	mseek(m, 64, SEEK_SET);
	for (int32_t i = 0; i < 32; i++)
	{
		uint8_t Pan;
		if (!ReadBytes(m, &Pan, 1)) return false;

		if (Pan >= 128)
		{
			Song.Header.ChnlPan[i] = 32 | 128; // 8bb: center + channel off
		}
		else
		{
			Pan &= 127;
			if (Pan <= 7)
				Song.Header.ChnlPan[i] = 0;
			else if (Pan <= 15)
				Song.Header.ChnlPan[i] = 64;
			else
				Song.Header.ChnlPan[i] = 32;
		}
	}

	// 8bb: set rest of channels to "off"
	for (int32_t i = 32; i < MAX_HOST_CHANNELS; i++)
		Song.Header.ChnlPan[i] = 32 | 128;

	for (int32_t i = 0; i < MAX_HOST_CHANNELS; i++)
		Song.Header.ChnlVol[i] = 64;

	memset(Song.Orders, 255, MAX_ORDERS);
	if (!ReadBytes(m, Song.Orders, Song.Header.OrdNum)) return false; // Order list loaded.

	if (!ReadBytes(m, SmpPtrs, Song.Header.SmpNum * 2)) return false;
	if (!ReadBytes(m, PatPtrs, Song.Header.PatNum * 2)) return false;

	if (DefPan == 252) // 8bb: load custom channel pans, if present
	{
		for (int32_t i = 0; i < 32; i++)
		{
			uint8_t Pan;
			if (!ReadBytes(m, &Pan, 1)) return false;

			if (Pan & 32)
			{
				uint8_t ChannelOffFlag = Song.Header.ChnlPan[i] & 128;
				Song.Header.ChnlPan[i] = (((Pan & 15) << 2) + 2) | ChannelOffFlag;
			}
		}
	}

	// Load instruments (8bb: and data)
	sample_t *s = Song.Smp;
	for (int32_t i = 0; i < Song.Header.SmpNum; i++, s++)
	{
		const uint32_t HeaderOffset = SmpPtrs[i] << 4;
		if (HeaderOffset == 0) // 8bb: added this check
			continue;

		mseek(m, HeaderOffset, SEEK_SET);

		uint8_t Type;
		ReadBytes(m, &Type, 1);

		ReadBytes(m, &s->DOSFilename, 12);

		uint8_t MemSegH;
		ReadBytes(m, &MemSegH, 1);
		uint16_t MemSegL;
		ReadBytes(m, &MemSegL, 2);

		ReadBytes(m, &s->Length, 4);
		ReadBytes(m, &s->LoopBegin, 4);
		ReadBytes(m, &s->LoopEnd, 4);
		ReadBytes(m, &s->Vol, 1);

		mseek(m, 2, SEEK_CUR);

		uint8_t SmpFlags;
		ReadBytes(m, &SmpFlags, 1);

		ReadBytes(m, &s->C5Speed, 4);

		mseek(m, 12, SEEK_CUR);
		ReadBytes(m, &s->SampleName, 25);

		if (Type == 1)
		{
			if (SmpFlags & 2)
				s->Flags |= SMPF_STEREO;

			if ((s->Length & 0xFFFF) > 0)
				s->Flags |= SMPF_ASSOCIATED_WITH_HEADER;

			s->OffsetInFile = ((MemSegH << 16) | MemSegL) << 4;
		}

		if (SmpFlags & 1)
			s->Flags |= SMPF_USE_LOOP;

		if (SmpFlags & 4)
			s->Flags |= SMPF_16BIT;

		s->GlobVol = 64;
		s->DefPan = 32;

		if (s->Flags & SMPF_ASSOCIATED_WITH_HEADER)
		{
			if (s->OffsetInFile != 0) // 8bb: added this check
			{
				bool Stereo = false; // !!(s->Flags & SMPF_STEREO); // 8bb: added stereo support for custom HQ driver
				bool Sample16Bit = !!(s->Flags & SMPF_16BIT);

				uint32_t SampleBytes = s->Length << Sample16Bit;

				if (!Music_AllocateSample(i, SampleBytes))
					return false;

				if (Stereo)
				{
					if (!Music_AllocateRightSample(i, SampleBytes))
						return false;
				}

				mseek(m, s->OffsetInFile, SEEK_SET);
				if (!ReadBytes(m, s->Data, SampleBytes))
					return false;

				if (Stereo)
				{
					if (!ReadBytes(m, s->DataR, SampleBytes))
						return false;
				}

				if (!Sample16Bit)
				{
					// 8bb: convert from unsigned to signed
					int8_t *Ptr8 = (int8_t *)s->Data;
					for (uint32_t j = 0; j < s->Length; j++)
						Ptr8[j] ^= 0x80;

					if (Stereo)
					{
						Ptr8 = (int8_t *)s->DataR;
						for (uint32_t j = 0; j < s->Length; j++)
							Ptr8[j] ^= 0x80;
					}
				}
				else
				{
					// 8bb: Music_AllocateSample() also set s->Length, divide by two if 16-bit
					s->Length >>= 1;

					// 8bb: convert from unsigned to signed
					int16_t *Ptr16 = (int16_t *)s->Data;
					for (uint32_t j = 0; j < s->Length; j++)
						Ptr16[j] ^= 0x8000;

					if (Stereo)
					{
						Ptr16 = (int16_t *)s->DataR;
						for (uint32_t j = 0; j < s->Length; j++)
							Ptr16[j] ^= 0x8000;
					}
				}
			}
		}
	}

	// Load patterns....
	pattern_t *p = Song.Patt;
	for (int32_t i = 0; i < Song.Header.PatNum; i++, p++)
	{
		const uint32_t PatternOffset = PatPtrs[i] << 4;
		if (PatternOffset == 0)
			continue;

		mseek(m, PatternOffset, SEEK_SET);

		uint16_t PackedPatLength;
		if (!ReadBytes(m, &PackedPatLength, 2))
			return false;

		uint8_t *PackedData = (uint8_t *)malloc(PackedPatLength);
		if (PackedData == NULL)
			return false;

		if (!ReadBytes(m, PackedData, PackedPatLength))
			return false;

		if (!TranslateS3MPattern(PackedData, i))
		{
			free(PackedData);
			return false;
		}

		free(PackedData);
	}

	return true;
}

static bool TranslateS3MPattern(uint8_t *Src, int32_t Pattern)
{
	PatternDataArea = (uint8_t *)malloc(MAX_HOST_CHANNELS * MAX_ROWS * 5);
	if (PatternDataArea == NULL)
		return false;

	// 8bb: clear destination pattern
	uint8_t *Ptr8 = PatternDataArea;
	for (int32_t i = 0; i < 200; i++)
	{
		for (int32_t j = 0; j < MAX_HOST_CHANNELS; j++, Ptr8 += 5)
		{
			Ptr8[0] = 253; // note
			Ptr8[1] = 0;   // ins
			Ptr8[2] = 255; // vol
			Ptr8[3] = 0;   // cmd
			Ptr8[4] = 0;   // value
		}
	}

	uint8_t *OrigDst = PatternDataArea;
	for (int32_t i = 0; i < S3M_ROWS; i++)
	{
		while (true)
		{
			uint8_t Byte, Mask = *Src++;
			if (Mask == 0)
			{
				OrigDst += MAX_HOST_CHANNELS * 5; // 8bb: end of channels, go to next row
				break;
			}

			uint8_t *Dst = OrigDst + ((Mask & 31) * 5); // 8bb: aligned to current channel to write into

			// 8bb: Note and sample
			if (Mask & 32)
			{
				Byte = *Src++;
				if (Byte == 254)
					Dst[0] = 254;
				else if (Byte <= 127)
					Dst[0] = 12 + (((Byte >> 4) * 12) + (Byte & 0x0F)); // C5 is now central octave

				// Instrument
				Byte = *Src++;
				if (Byte <= 99)
					Dst[1] = Byte;
				else
					Dst[1] = 0;
			}

			// Volume
			if (Mask & 64)
			{
				Byte = *Src++;
				if (Byte != 255)
				{
					if (Byte <= 64)
						Dst[2] = Byte;
					else
						Dst[2] = 64;
				}
			}

			// 8bb: Effect + parameter
			if (Mask & 128)
			{
				Dst[3] = *Src++;
				Dst[4] = *Src++;

				if (Dst[3] == 'C'-'@')
				{
					// 8bb: IT2's broken (?) way of converting between decimal/hex
					Dst[4] = (Dst[4] & 0x0F) + ((Dst[4] & 0xF0) >> 1) + ((Dst[4] & 0xF0) >> 3);
				}
				else if (Dst[3] == 'V'-'@')
				{
					if (Dst[4] < 128)
						Dst[4] <<= 1;
					else
						Dst[4] = 255;
				}
				else if (Dst[3] == 'X'-'@')
				{
					if (Dst[4] == 0xA4) // 8bb: surround
					{
						Dst[3] = 'S'-'@';
						Dst[4] = 0x91;
					}
					else
					{
						if (Dst[4] < 128)
							Dst[4] <<= 1;
						else
							Dst[4] = 255;
					}
				}
				else if (Dst[3] == 'D'-'@')
				{
					uint8_t lo = Dst[4] & 0x0F;
					uint8_t hi = Dst[4] & 0xF0;

					if (lo != 0 && hi != 0)
					{
						if (lo != 0x0F && hi != 0xF0)
							Dst[4] &= 0x0F;
					}
				}
			}
		}
	}

	bool result = StorePattern(S3M_ROWS, Pattern);

	free(PatternDataArea);
	return result;
}

static void ClearEncodingInfo(void)
{
	uint8_t *Enc = EncodingInfo;
	for (int32_t i = 0; i < MAX_HOST_CHANNELS; i++, Enc += 6)
	{
		Enc[0] = 0;   // mask
		Enc[1] = 253; // note
		Enc[2] = 0;   // ins
		Enc[3] = 255; // vol
		Enc[4] = 0;   // cmd
		Enc[5] = 0;   // value
	}
}

static bool GetPatternLength(uint16_t Rows, uint16_t *LengthOut)
{
	ClearEncodingInfo();

	uint8_t *Src = PatternDataArea;
	uint32_t Bytes = Rows; // End of row bytes added.

	for (int32_t i = 0; i < Rows; i++)
	{
		uint8_t *Enc = EncodingInfo;
		for (int32_t j = 0; j < MAX_HOST_CHANNELS; j++, Src += 5, Enc += 6)
		{
			if (Src[0] == 253 && Src[1] == 0 && Src[2] == 255 && Src[3] == 0 && Src[4] == 0)
				continue;

			Bytes++; // 1 byte for channel indication

			uint8_t Mask = 0;

			uint8_t Note = Src[0];
			if (Note != 253)
			{
				if (Enc[1] != Note)
				{
					Enc[1] = Note;
					Bytes++;
					Mask |= 1;
				}
				else
				{
					Mask |= 16;
				}
			}

			uint8_t Instr = Src[1];
			if (Instr != 0)
			{
				if (Enc[2] != Instr)
				{
					Enc[2] = Instr;
					Bytes++;
					Mask |= 2;
				}
				else
				{
					Mask |= 32;
				}
			}

			uint8_t Vol = Src[2];
			if (Vol != 255)
			{
				if (Enc[3] != Vol)
				{
					Enc[3] = Vol;
					Bytes++;
					Mask |= 4;
				}
				else
				{
					Mask |= 64;
				}
			}

			uint16_t EfxAndParam = *(uint16_t *)&Src[3];
			if (EfxAndParam != 0)
			{
				if (*(uint16_t *)&Enc[4] != EfxAndParam)
				{
					*(uint16_t *)&Enc[4] = EfxAndParam;
					Bytes += 2;
					Mask |= 8;
				}
				else
				{
					Mask |= 128;
				}
			}

			if (Mask != Enc[0])
			{
				Enc[0] = Mask;
				Bytes++;
			}
		}
	}

	if (Bytes > 65535)
		return false;

	*LengthOut = (uint16_t)Bytes;
	return true;
}

static void EncodePattern(pattern_t *p, uint8_t Rows)
{
	ClearEncodingInfo();

	p->Rows = Rows;

	uint8_t *Src = PatternDataArea;
	uint8_t *Dst = p->PackedData;

	for (int32_t i = 0; i < Rows; i++)
	{
		uint8_t *Enc = EncodingInfo;
		for (uint8_t ch = 0; ch < MAX_HOST_CHANNELS; ch++, Src += 5, Enc += 6)
		{
			if (Src[0] == 253 && Src[1] == 0 && Src[2] == 255 && Src[3] == 0 && Src[4] == 0)
				continue;

			uint8_t Mask = 0;

			uint8_t Note = Src[0];
			if (Note != 253)
			{
				if (Enc[1] != Note)
				{
					Enc[1] = Note;
					Mask |= 1;
				}
				else
				{
					Mask |= 16;
				}
			}

			uint8_t Ins = Src[1];
			if (Src[1] != 0)
			{
				if (Enc[2] != Ins)
				{
					Enc[2] = Ins;
					Mask |= 2;
				}
				else
				{
					Mask |= 32;
				}
			}

			uint8_t Vol = Src[2];
			if (Vol != 255)
			{
				if (Enc[3] != Vol)
				{
					Enc[3] = Vol;
					Mask |= 4;
				}
				else
				{
					Mask |= 64;
				}
			}

			uint16_t EfxAndParam = *(uint16_t *)&Src[3];
			if (EfxAndParam != 0)
			{
				if (EfxAndParam != *(uint16_t *)&Enc[4])
				{
					*(uint16_t *)&Enc[4] = EfxAndParam;
					Mask |= 8;
				}
				else
				{
					Mask |= 128;
				}
			}

			if (Enc[0] != Mask)
			{
				Enc[0] = Mask;

				*Dst++ = (ch + 1) | 128; // read another mask...
				*Dst++ = Mask;
			}
			else
			{
				*Dst++ = ch + 1;
			}

			if (Mask & 1)
				*Dst++ = Note;

			if (Mask & 2)
				*Dst++ = Ins;

			if (Mask & 4)
				*Dst++ = Vol;

			if (Mask & 8)
			{
				*(uint16_t *)Dst = EfxAndParam;
				Dst += 2;
			}
		}

		*Dst++ = 0;
	}
}

static bool StorePattern(uint8_t NumRows, int32_t Pattern)
{
	uint16_t PackedLength;
	if (!GetPatternLength(NumRows, &PackedLength))
		return false;

	if (!Music_AllocatePattern(Pattern, PackedLength))
		return false;

	EncodePattern(&Song.Patt[Pattern], NumRows);
	return true;
}
