/*
** 8bb: IT2 module loading routines
**
** NOTE: This file is not directly ported from the IT2 code,
**       so routines have non-original names. All comments in
**       this file are by me (8bitbubsy).
*/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "loaders/mmcmp/mmcmp.h"
#include "it_structs.h"
#include "it_music.h"
#include "it_d_rm.h"
#include "loaders/it.h"
#include "loaders/s3m.h"

static bool FirstTimeLoading = true;

static int8_t GetModuleType(MEMFILE *m) // 8bb: added this
{
	static uint8_t Header[44+4];

	size_t OldOffset = mtell(m);
	
	mseek(m, 0, SEEK_END);
	size_t DataLen = mtell(m);
	mseek(m, 0, SEEK_SET);

	mread(Header, 1, sizeof (Header), m);

	int8_t Format = FORMAT_UNKNOWN;
	if (DataLen >= 4 && !memcmp(&Header[0], "IMPM", 4))
		Format = FORMAT_IT;
	else if (DataLen >= 44+4 && !memcmp(&Header[44], "SCRM", 4))
		Format = FORMAT_S3M;

	mseek(m, OldOffset, SEEK_SET);
	return Format;
}

bool Music_LoadFromData(uint8_t *Data, uint32_t DataLen)
{
	bool WasCompressed = false;
	if (DataLen >= 4+4) // find out if module is MMCMP compressed
	{
		uint32_t Sig1 = *(uint32_t *)&Data[0];
		uint32_t Sig2 = *(uint32_t *)&Data[4];
		if (Sig1 == 0x4352697A && Sig2 == 0x61694E4F) // Sig1 = "ziRCONia"
		{
			if (unpackMMCMP(&Data, &DataLen))
				WasCompressed = true;
			else
				return false;
		}
	}

	MEMFILE *m = mopen(Data, DataLen);
	if (m == NULL)
		return false;

	if (FirstTimeLoading)
	{
		memset(&Song, 0, sizeof (Song));
		FirstTimeLoading = false;
	}
	else
	{
		Music_FreeSong();
	}

	bool WasLoaded = false;

	uint8_t Format = GetModuleType(m);
	if (Format != FORMAT_UNKNOWN)
	{
		Music_SetDefaultMIDIDataArea();
		switch (Format)
		{
			default: break;
			case FORMAT_IT:  WasLoaded = LoadIT(m);  break;
			case FORMAT_S3M: WasLoaded = LoadS3M(m); break;
		}
	}

	mclose(&m);
	if (WasCompressed)
		free(Data);

	if (WasLoaded)
	{
		DriverSetMixVolume(Song.Header.MixVolume);
		DriverFixSamples();

		Song.Loaded = true;
		return true;
	}
	else
	{
		Music_FreeSong();

		Song.Loaded = false;
		return false;
	}
}

// routines for handling data in RAM as a "FILE" type

MEMFILE *mopen(const uint8_t *src, uint32_t length)
{
	MEMFILE *b;

	if (src == NULL || length == 0)
		return NULL;

	b = (MEMFILE *)malloc(sizeof (MEMFILE));
	if (b == NULL)
		return NULL;

	b->_base = (uint8_t *)src;
	b->_ptr = (uint8_t *)src;
	b->_cnt = length;
	b->_bufsiz = length;
	b->_eof = false;

	return b;
}

void mclose(MEMFILE **buf)
{
	if (*buf != NULL)
	{
		free(*buf);
		*buf = NULL;
	}
}

size_t mread(void *buffer, size_t size, size_t count, MEMFILE *buf)
{
	int32_t pcnt;
	size_t wrcnt;

	if (buf == NULL || buf->_ptr == NULL)
		return 0;

	wrcnt = size * count;
	if (size == 0 || buf->_eof)
		return 0;

	pcnt = (buf->_cnt > (uint32_t)wrcnt) ? (uint32_t)wrcnt : buf->_cnt;
	memcpy(buffer, buf->_ptr, pcnt);

	buf->_cnt -= pcnt;
	buf->_ptr += pcnt;

	if (buf->_cnt <= 0)
	{
		buf->_ptr = buf->_base + buf->_bufsiz;
		buf->_cnt = 0;
		buf->_eof = true;
	}

	return pcnt / size;
}

size_t mtell(MEMFILE *buf)
{
	return (buf->_ptr - buf->_base);
}

int32_t meof(MEMFILE *buf)
{
	if (buf == NULL)
		return true;

	return buf->_eof;
}

void mseek(MEMFILE *buf, size_t offset, int32_t whence)
{
	if (buf == NULL)
		return;

	if (buf->_base)
	{
		switch (whence)
		{
			case SEEK_SET: buf->_ptr = buf->_base + offset; break;
			case SEEK_CUR: buf->_ptr += offset; break;
			case SEEK_END: buf->_ptr = buf->_base + buf->_bufsiz + offset; break;
			default: break;
		}

		buf->_eof = false;
		if (buf->_ptr >= buf->_base+buf->_bufsiz)
		{
			buf->_ptr = buf->_base + buf->_bufsiz;
			buf->_eof = true;
		}

		buf->_cnt = (uint32_t)((buf->_base + buf->_bufsiz) - buf->_ptr);
	}
}

bool ReadBytes(MEMFILE *m, void *dst, uint32_t num)
{
	if ((m == NULL) || meof(m))
		return false;

	if (mread(dst, 1, num, m) != num)
		return false;

	return true;
}
