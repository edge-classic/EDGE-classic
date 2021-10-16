//----------------------------------------------------------------------------
//  WAV Format Sound Loading
//----------------------------------------------------------------------------
// 
//  Copyright (c) 2007-2008  The EDGE Team.
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
//
//  Based heavily on the WAV decoder from the "SDL_sound" library
//  Copyright (C) 2001  Ryan C. Gordon.
// 
//----------------------------------------------------------------------------

#include "epi.h"
#include "endianess.h"

#include "sound_wav.h"
#include "sound_gather.h"

#if 1
#define SND_DEBUG(args)  I_Debugf(args)
#else
#define SND_DEBUG(args)  do {} while (0)
#endif

namespace epi
{

static inline bool read_le_u16(file_c *f, u16_t *val)
{
    if (2 != f->Read(val, 2))
		return false;

    *val = EPI_LE_U16(*val);
    return true;
}

static inline bool read_le_s16(file_c *f, s16_t *val)
{
	return read_le_u16(f, (u16_t *) val);
}


static inline bool read_le_u32(file_c *f, u32_t *val)
{
    if (4 != f->Read(val, 4))
		return false;

    *val = EPI_LE_U32(*val);
    return true;
}

static inline bool read_le_s32(file_c *f, s32_t *val)
{
	return read_le_u32(f, (u32_t *) val);
}


/* This is just cleaner on the caller's end... */
static inline bool read_uint8(file_c *f, u8_t *val)
{
	return (1 == f->Read(val, 1));
}


/* Chunk management code... */

#define ID_RIFF  0x46464952U  /* "RIFF", in ascii. */
#define ID_WAVE  0x45564157U  /* "WAVE", in ascii. */
#define ID_FACT  0x74636166U  /* "fact", in ascii. */


/*********************************************************
 * The FORMAT chunk...                                   *
 *********************************************************/

#define ID_FMT  0x20746D66U  /* "fmt ", in ascii. */

#define FMT_NORMAL 0x0001    /* Uncompressed waveform data.     */
#define FMT_ADPCM  0x0002    /* ADPCM compressed waveform data. */

typedef struct
{
    s16_t iCoef1;
    s16_t iCoef2;
}
ADPCMCOEFSET;

typedef struct
{
    u8_t bPredictor;
    u16_t iDelta;
    s16_t iSamp1;
    s16_t iSamp2;
}
ADPCMBLOCKHEADER;

typedef struct S_WAV_FMT_T
{
    u32_t chunkID;
    s32_t chunkSize;

    s16_t wFormatTag;
    u16_t wChannels;
    u32_t dwSamplesPerSec;
    u32_t dwAvgBytesPerSec;
    u16_t wBlockAlign;
    u16_t wBitsPerSample;

    u32_t next_chunk_offset;
    u32_t sample_frame_size;
    u32_t total_bytes;

    union
    {
        struct
        {
            u16_t cbSize;
            u16_t wSamplesPerBlock;
            u16_t wNumCoef;
            ADPCMCOEFSET *aCoef;
            ADPCMBLOCKHEADER *blockheaders;
            u32_t samples_left_in_block;
            int nibble_state;
            s8_t nibble;
        } adpcm;

        /* put other format-specific data here... */
    } fmt;
}
fmt_t;

static int (*read_sample)(s16_t *buffer, int max_samples);


/*
 * Read in a fmt_t from disk. This makes this process safe regardless of
 *  the processor's byte order or how the fmt_t structure is packed.
 * Note that the union "fmt" is not read in here; that is handled as 
 *  needed in the read_fmt_* functions.
 */
static bool read_fmt_chunk(file_c *f, fmt_t *fmt)
{
    /* skip reading the chunk ID, since it was already read at this point... */
    fmt->chunkID = ID_FMT;

    if (! read_le_s32(f, &fmt->chunkSize))
		return false;

    if (fmt->chunkSize < 16)
	{
		I_Printf("WAV: Invalid chunk size");
		return false;
	}

    fmt->next_chunk_offset = f->GetPosition() + fmt->chunkSize;

    return ( read_le_s16(f, &fmt->wFormatTag) &&
			 read_le_u16(f, &fmt->wChannels) &&
			 read_le_u32(f, &fmt->dwSamplesPerSec) &&
			 read_le_u32(f, &fmt->dwAvgBytesPerSec) &&
			 read_le_u16(f, &fmt->wBlockAlign) &&
			 read_le_u16(f, &fmt->wBitsPerSample) );
}


/*********************************************************
 * The DATA chunk...                                     *
 *********************************************************/

#define ID_DATA  0x61746164U  /* "data", in ascii. */


/*****************************************************************************
 * this is what we store in our internal->decoder_private field...           *
 *****************************************************************************/

static fmt_t decoder_fmt;

typedef struct
{
    fmt_t *fmt;

    int bytes_left;
}
wav_t;

static wav_t decoder_wavt;

static file_c *decode_F;

static bool decode_eof;
static bool decode_error;


/*****************************************************************************
 * Normal, uncompressed waveform handler...                                  *
 *****************************************************************************/

/*
 * Sound_Decode() lands here for uncompressed WAVs...
 */
static int read_sample_fmt_normal(s16_t *buffer, int max_samples)
{
    wav_t *w = &decoder_wavt;
	fmt_t *fmt = w->fmt;

	bool is_stereo = (fmt->wChannels == 2);

	int bytes_each = (fmt->wBitsPerSample / 8) * (is_stereo ? 2 : 1);

    int want = MIN(max_samples, w->bytes_left / bytes_each);

	if (want == 0)
	{
		decode_eof = true;
		return 0;
	}

	/*
	 * We don't actually do any decoding, so we read the wav data
	 * directly into the internal buffer...
	 */
    int got_bytes = decode_F->Read(buffer, want * bytes_each);  // FIXME: DECODE U8 --> S16

	if (got_bytes < 0)
	{
		decode_error = true;
		return got_bytes;
	}

        /* Make sure the read went smoothly... */
    if (got_bytes == 0)
	{
        decode_eof = true;
		return 0;
	}

	// FIXME: handle case of F->Read() returns odd number (for S16)

	if (fmt->wBitsPerSample == 8)
	{
		// convert U8 samples to S16 
		for (int i = got_bytes-1; i >= 0; i--)
		{
			u8_t src = ((u8_t *) buffer)[i];

			buffer[i] = (src ^ 0x80) << 8;
		}
	}
	else
	{
		// endian swap 16-bit samples
		for (int i = 0; i < got_bytes / 2; i++)
			buffer[i] = EPI_LE_S16(buffer[i]);
	}

    w->bytes_left -= got_bytes;

    return got_bytes / bytes_each;
}


static bool read_fmt_normal(file_c *f, fmt_t *fmt)
{
    /* (don't need to read more from the RWops...) */
    read_sample = read_sample_fmt_normal;

    return true; //OK
}



/*********************************************************
 * ADPCM compression handler...                          *
 *********************************************************/

#define FIXED_POINT_COEF_BASE      256
#define FIXED_POINT_ADAPTION_BASE  256
#define SMALLEST_ADPCM_DELTA       16


static inline bool read_adpcm_block_headers(file_c *f)
{

    wav_t *w = &decoder_wavt;
    fmt_t *fmt = w->fmt;

    ADPCMBLOCKHEADER *headers = fmt->fmt.adpcm.blockheaders;

    if (w->bytes_left < fmt->wBlockAlign)
    {
		decode_eof = true;
        return(0);
    }

    w->bytes_left -= fmt->wBlockAlign;

	int i;
    for (i = 0; i < fmt->wChannels; i++)
        if (! read_uint8(f, &headers[i].bPredictor))
			return false;

    for (i = 0; i < fmt->wChannels; i++)
        if (! read_le_u16(f, &headers[i].iDelta))
			return false;

    for (i = 0; i < fmt->wChannels; i++)
        if (! read_le_s16(f, &headers[i].iSamp1))
			return false;

    for (i = 0; i < fmt->wChannels; i++)
        if (! read_le_s16(f, &headers[i].iSamp2))
			return false;

    fmt->fmt.adpcm.samples_left_in_block = fmt->fmt.adpcm.wSamplesPerBlock;
    fmt->fmt.adpcm.nibble_state = 0;

    return true; //OK
}


static inline void do_adpcm_nibble(u8_t nib,
                                   ADPCMBLOCKHEADER *header,
                                   s32_t lPredSamp)
{
	static const s32_t max_audioval = (1<<15) - 1;
	static const s32_t min_audioval = -max_audioval;

	static const s32_t AdaptionTable[] =
    {
		230, 230, 230, 230, 307, 409, 512, 614,
		768, 614, 512, 409, 307, 230, 230, 230
	};

    s32_t lNewSamp;
    s32_t delta;

    if (nib & 0x08)
        lNewSamp = lPredSamp + (header->iDelta * (nib - 0x10));
	else
        lNewSamp = lPredSamp + (header->iDelta * nib);

        /* clamp value... */
    if (lNewSamp < min_audioval)
        lNewSamp = min_audioval;
    else if (lNewSamp > max_audioval)
        lNewSamp = max_audioval;

    delta = ((s32_t) header->iDelta * AdaptionTable[nib]) /
              FIXED_POINT_ADAPTION_BASE;

	if (delta < SMALLEST_ADPCM_DELTA)
	    delta = SMALLEST_ADPCM_DELTA;

    header->iDelta = delta;
	header->iSamp2 = header->iSamp1;
	header->iSamp1 = lNewSamp;
}


static inline int decode_adpcm_sample_frame()
{
    wav_t *w = &decoder_wavt;
    fmt_t *fmt = w->fmt;

    ADPCMBLOCKHEADER *headers = fmt->fmt.adpcm.blockheaders;

    u8_t nib = fmt->fmt.adpcm.nibble;

    for (int i = 0; i < fmt->wChannels; i++)
    {
		s16_t iCoef1 = fmt->fmt.adpcm.aCoef[headers[i].bPredictor].iCoef1;
        s16_t iCoef2 = fmt->fmt.adpcm.aCoef[headers[i].bPredictor].iCoef2;
		
        s32_t lPredSamp = ((headers[i].iSamp1 * iCoef1) +
                            (headers[i].iSamp2 * iCoef2)) / 
                             FIXED_POINT_COEF_BASE;

        if (fmt->fmt.adpcm.nibble_state == 0)
        {
            if (!read_uint8(decode_F, &nib)) // FIXME !!!
				I_Error("WAV DECODE ERROR: adcpm read\n");
			
            fmt->fmt.adpcm.nibble_state = 1;
            do_adpcm_nibble(nib >> 4, &headers[i], lPredSamp);
        }
        else
        {
            fmt->fmt.adpcm.nibble_state = 0;
            do_adpcm_nibble(nib & 0x0F, &headers[i], lPredSamp);
        }
    }

    fmt->fmt.adpcm.nibble = nib;

    return true; //OK
}


static inline void put_adpcm_sample_frame1(void *_buf, fmt_t *fmt)
{
    u16_t *buf = (u16_t *) _buf;
    ADPCMBLOCKHEADER *headers = fmt->fmt.adpcm.blockheaders;

    for (int i = 0; i < fmt->wChannels; i++)
        *(buf++) = headers[i].iSamp1;
}


static inline void put_adpcm_sample_frame2(void *_buf, fmt_t *fmt)
{
    u16_t *buf = (u16_t *) _buf;
    ADPCMBLOCKHEADER *headers = fmt->fmt.adpcm.blockheaders;
    int i;
    for (i = 0; i < fmt->wChannels; i++)
        *(buf++) = headers[i].iSamp2;
}


/*
 * Sound_Decode() lands here for ADPCM-encoded WAVs...
 */
static int read_sample_fmt_adpcm(s16_t *buffer, int max_samples)
{
    wav_t *w = &decoder_wavt;
    fmt_t *fmt = w->fmt;
    int bw = 0;

    while (bw < max_samples)
    {
        /* write ongoing sample frame before reading more data... */
        switch (fmt->fmt.adpcm.samples_left_in_block)
        {
            case 0:  /* need to read a new block... */
                if (!read_adpcm_block_headers(decode_F))
                {
                    if (! decode_eof)
                        decode_error = true;
                    return(bw);
                } /* if */

                /* only write first sample frame for now. */
                put_adpcm_sample_frame2((u8_t *) buffer + bw, fmt);
                fmt->fmt.adpcm.samples_left_in_block--;
                bw += fmt->sample_frame_size;
                break;

            case 1:  /* output last sample frame of block... */
                put_adpcm_sample_frame1((u8_t *) buffer + bw, fmt);
                fmt->fmt.adpcm.samples_left_in_block--;
                bw += fmt->sample_frame_size;
                break;

            default: /* output latest sample frame and read a new one... */
                put_adpcm_sample_frame1((u8_t *) buffer + bw, fmt);
                fmt->fmt.adpcm.samples_left_in_block--;
                bw += fmt->sample_frame_size;

                if (!decode_adpcm_sample_frame())
                {
                    decode_error = true;
                    return(bw);
                } /* if */
        } /* switch */
    } /* while */

    return(bw);
}


/*
 * Sound_FreeSample() lands here for ADPCM-encoded WAVs...
 */
static void free_fmt_adpcm(fmt_t *fmt)
{
    delete fmt->fmt.adpcm.aCoef;
    delete fmt->fmt.adpcm.blockheaders;
}



/*
 * Read in the adpcm-specific info from disk. This makes this process
 * safe regardless of the processor's byte order or how the fmt_t 
 * structure is packed.
 */
static bool read_fmt_adpcm(file_c *rw, fmt_t *fmt)
{
    memset(&fmt->fmt.adpcm, 0, sizeof(fmt->fmt.adpcm));

    read_sample = read_sample_fmt_adpcm;

    if (! read_le_u16(rw, &fmt->fmt.adpcm.cbSize) ||
        ! read_le_u16(rw, &fmt->fmt.adpcm.wSamplesPerBlock) ||
        ! read_le_u16(rw, &fmt->fmt.adpcm.wNumCoef))
	{
		return false;
	}

    fmt->fmt.adpcm.aCoef = new ADPCMCOEFSET[fmt->fmt.adpcm.wNumCoef];

    for (int i = 0; i < fmt->fmt.adpcm.wNumCoef; i++)
    {
        if (! read_le_s16(rw, &fmt->fmt.adpcm.aCoef[i].iCoef1) ||
            ! read_le_s16(rw, &fmt->fmt.adpcm.aCoef[i].iCoef2))
		{
			return false;
		}
    }

    fmt->fmt.adpcm.blockheaders = new ADPCMBLOCKHEADER[fmt->wChannels];

    return true; //OK
}


/*****************************************************************************
 * Everything else...                                                        *
 *****************************************************************************/

static bool read_fmt(file_c *f, fmt_t *fmt)
{
    // if it's in this switch statement, we support the format
    switch (fmt->wFormatTag)
    {
        case FMT_NORMAL:
            SND_DEBUG(("WAV: Appears to be uncompressed audio.\n"));
            return read_fmt_normal(f, fmt);

        case FMT_ADPCM:
            SND_DEBUG(("WAV: Appears to be ADPCM compressed audio.\n"));
            return read_fmt_adpcm(f, fmt);

        /* add other types here. */

        default:
			break;
	}

	I_Debugf("WAV Loader: Format 0x%X is unknown.\n", (u32_t) fmt->wFormatTag);

	return false;
}


/*
 * Locate a specific chunk in the WAVE file by ID...
 */
static bool find_chunk(file_c *f, u32_t id)
{
    int pos = f->GetPosition();

    for (;;)
    {
		u32_t got_id;

        if (! read_le_u32(f, &got_id))
			return false;

        if (got_id == id)
            return true;  // found it!

		// skip ahead to the next chunk is...
		s32_t len;

        if (! read_le_s32(f, &len))
			return false;

        SYS_ASSERT(len >= 0);

        pos += sizeof(u32_t) * 2 + len;

        if (len > 0)
		{
			if (! f->Seek(pos, file_c::SEEKPOINT_START))
				return false;
		}
    }

    return false; /* NOT REACHED */
}


bool WAV_Load(sound_data_c *buf, file_c *f)
{
	decode_F = f;

    memset(&decoder_fmt, 0, sizeof(decoder_fmt));

	decoder_wavt.fmt = &decoder_fmt;


    wav_t *w = &decoder_wavt;
	fmt_t *fmt = w->fmt;


	u32_t header_id;
	u32_t header_file_len;

	if (! read_le_u32(f, &header_id) || header_id != ID_RIFF)
	{
		I_Warning("WAV Loader: Not a RIFF file.\n");
		return false;
	}

	read_le_u32(f, &header_file_len);

	if (! read_le_u32(f, &header_id) || header_id != ID_WAVE)
	{
		I_Warning("WAV Loader: Not a RIFF/WAVE file.\n");
		return false;
	}

    if (! find_chunk(f, ID_FMT))
	{
		I_Warning("WAV Loader: Missing [fmt] chunk.\n");
		return false;
	}

    if (! read_fmt_chunk(f, fmt))
	{
		I_Warning("WAV Loader: Cannot decode [fmt] chunk.\n");
		return false;
	}

	int freq     = fmt->dwSamplesPerSec;
	int bits     = fmt->wBitsPerSample;
    int channels = fmt->wChannels;

	if (channels > 2)
	{
		I_Warning("WAV Loader: too many channels: %d\n", channels);
		return false;
	}

	bool is_stereo = (channels == 2);

	I_Debugf("WAV Loader: freq %d Hz, %d channels, %d sample bits\n",
			 freq, channels, bits);

	buf->freq = freq;

	if (bits == 32)
	{
        I_Warning("WAV Loader: Floating point not supported.\n");
		return false;
	}
	// FIXME: 4 used for ADPCM
	else if (! (bits == 8 || bits == 16))
    {
        I_Warning("WAV Loader: Unsupported sample bits: %d\n", bits);
		return false;
    }

    if (! read_fmt(f, fmt))
		return false;

	f->Seek(fmt->next_chunk_offset, file_c::SEEKPOINT_START);

    if (! find_chunk(f, ID_DATA))
	{
		I_Warning("WAV Loader: Missing [data] chunk.\n");
		return false;
	}

	s32_t data_size;

    if (! read_le_s32(f, &data_size))
	{
		I_Warning("WAV Loader: Cannot get [data] chunk size.\n");
		return false;
	}

    fmt->total_bytes = w->bytes_left = data_size;

    fmt->sample_frame_size = (sizeof(s16_t) * channels); //!!!!! FIXME: made up shit

	decode_eof = decode_error = false;
	
    SND_DEBUG(("WAV: Accepting data stream.\n"));

	// load the data stream

	sound_gather_c gather;

	while (! decode_error)
	{
		int want = 2048;

		s16_t *buffer = gather.MakeChunk(want, is_stereo);

		int got_num = (*read_sample)(buffer, want);

		if (got_num < 0)
			I_Error("WAV Loader: Error reading file (%d)\n", got_num);

		if (got_num == 0)  // EOF
		{
			gather.DiscardChunk();
			break;
		}

		gather.CommitChunk(got_num);
	}

	if (! gather.Finalise(buf, false /* want_stereo */))
		I_Error("WAV Loader: no samples!\n");

    return true;
}

} // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
