//----------------------------------------------------------------------------
//  EDGE MP3 Music Player
//----------------------------------------------------------------------------
// 
//  Copyright (c) 2004-2009  The EDGE Team.
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
// -ACB- 2004/08/18 Written: 
//
// Based on a tutorial at DevMaster.net:
// http://www.devmaster.net/articles/openal-tutorials/lesson8.php
//

#include "i_defs.h"

#include "endianess.h"
#include "file.h"
#include "filesystem.h"
#include "sound_gather.h"

#include "playlist.h"

#include "s_cache.h"
#include "s_blit.h"
#include "s_music.h"
#include "s_mp3.h"
#include "w_wad.h"

#define MINIMP3_ONLY_MP3
#define MINIMP3_ONLY_SIMD
#define MINIMP3_FLOAT_OUTPUT
#define MINIMP3_IMPLEMENTATION
#include "minimp3_ex.h"

#define MP3V_NUM_SAMPLES  8192

extern bool dev_stereo;  // FIXME: encapsulation

// Function for s_music.cc to check if a song lump is in .mp3 format
bool S_CheckMP3 (byte *data, int length) {
	return mp3dec_detect_buf(data, length);
}

struct datalump_s
{
	const byte *data;

	size_t pos;
	size_t size;
};

class mp3player_c : public abstract_music_c
{
public:
	 mp3player_c();
	~mp3player_c();

private:

	enum status_e
	{
		NOT_LOADED, PLAYING, PAUSED, STOPPED
	};
	
	int status;
	bool looping;

	mp3dec_ex_t mp3_file;
	datalump_s mp3_lump;

	mp3dec_ex_t mp3_stream;
	bool is_stereo;

	s16_t *mono_buffer;

public:
	bool OpenLump(const char *lumpname);
	bool OpenFile(const char *filename);

	virtual void Close(void);

	virtual void Play(bool loop);
	virtual void Stop(void);

	virtual void Pause(void);
	virtual void Resume(void);

	virtual void Ticker(void);
	virtual void Volume(float gain);

private:
	const char *GetError(int code);

	void PostOpenInit(void);

	bool StreamIntoBuffer(epi::sound_data_c *buf);

};


//----------------------------------------------------------------------------


//
// mp3player datalump operation functions
//
size_t mp3player_memread(void *ptr, size_t size, size_t nmemb, void *datasource)
{
	datalump_s *d = (datalump_s *)datasource;
	size_t rb = size*nmemb;

	if (d->pos >= d->size)
		return 0;

	if (d->pos + rb > d->size)
		rb = d->size - d->pos;

	memcpy(ptr, d->data + d->pos, rb);
	d->pos += rb;		
	
	return rb / size;
}

int mp3player_memseek(void *datasource, mp3_int64_t offset, int whence)
{
	datalump_s *d = (datalump_s *)datasource;
	size_t newpos;

	switch(whence)
	{
		case 0: { newpos = (int) offset; break; }				// Offset
		case 1: { newpos = d->pos + (int)offset; break; }		// Pos + Offset
        case 2: { newpos = d->size + (int)offset; break; }	    // End + Offset
		default: { return -1; }	// WTF?
	}
	
	if (newpos > d->size)
		return -1;

	d->pos = newpos;
	return 0;
}

int mp3player_memclose(void *datasource)
{
	datalump_s *d = (datalump_s *)datasource;

	if (d->size > 0)
	{
		delete[] d->data;
		d->data = NULL;

        d->pos  = 0;
		d->size = 0;
	}

	return 0;
}

long mp3player_memtell(void *datasource)
{
	datalump_s *d = (datalump_s *)datasource;

	if (d->pos > d->size)
		return -1;

	return d->pos;
}

//
// mp3player file operation functions
//
size_t mp3player_fread(void *ptr, size_t size, size_t nmemb, void *datasource)
{
	return fread(ptr, size, nmemb, (FILE*)datasource);
}

int mp3player_fseek(void *datasource, mp3_int64_t offset, int whence)
{
	return fseek((FILE*)datasource, (int)offset, whence);
}

int mp3player_fclose(void *datasource)
{
	return fclose((FILE*)datasource);
}

long mp3player_ftell(void *datasource)
{
	return ftell((FILE*)datasource);
}


//----------------------------------------------------------------------------


mp3player_c::mp3player_c() : status(NOT_LOADED)
{
	mp3_file = NULL;

	mp3_lump.data = NULL;

	mono_buffer = new s16_t[MP3V_NUM_SAMPLES * 2];
}

mp3player_c::~mp3player_c()
{
	Close();

	if (mono_buffer)
		delete[] mono_buffer;
}

void mp3player_c::PostOpenInit()
{
    if (mp3_file.info.channels == 1) {
        is_stereo = false;
	} else {
        is_stereo = true;
	}
    
	// Loaded, but not playing
	status = STOPPED;
}


static void ConvertToMono(s16_t *dest, const s16_t *src, int len)
{
	const s16_t *s_end = src + len*2;

	for (; src < s_end; src += 2)
	{
		// compute average of samples
		*dest++ = ( (int)src[0] + (int)src[1] ) >> 1;
	}
}


bool mp3player_c::StreamIntoBuffer(epi::sound_data_c *buf)
{
	int mp3_endian = (EPI_BYTEORDER == EPI_LIL_ENDIAN) ? 0 : 1;

    int samples = 0;

    while (samples < MP3V_NUM_SAMPLES)
    {
		s16_t *data_buf;

		if (is_stereo and !dev_stereo)
			data_buf = mono_buffer;
		else
			data_buf = buf->data_L + samples * (is_stereo ? 2 : 1);

		int section;
        int got_size = ov_read(&mp3_stream, (char *)data_buf,
				(MP3V_NUM_SAMPLES - samples) * (is_stereo ? 2 : 1) * sizeof(s16_t),
				mp3_endian, sizeof(s16_t), 1 /* signed data */,
				&section);

		if (got_size == OV_HOLE)  // ignore corruption
			continue;

		if (got_size == 0)  /* EOF */
		{
			if (! looping)
				break;

			ov_raw_seek(&mp3_stream, 0);
			continue; // try again
		}

		if (got_size < 0)  /* ERROR */
		{
			// Construct an error message
			std::string err_msg("[mp3player_c::StreamIntoBuffer] Failed: ");

			err_msg += GetError(got_size);

			// FIXME: using I_Error is too harsh
			I_Error("%s", err_msg.c_str());
			return false; /* NOT REACHED */
		}

		got_size /= (is_stereo ? 2 : 1) * sizeof(s16_t);

		if (is_stereo and !dev_stereo)
			ConvertToMono(buf->data_L + samples, mono_buffer, got_size);

		samples += got_size;
    }

    return (samples > 0);
}


void mp3player_c::Volume(float gain)
{
	// not needed, music volume is handled in s_blit.cc
	// (see mix_channel_c::ComputeMusicVolume).
}


bool mp3player_c::OpenLump(const char *lumpname)
{
	SYS_ASSERT(lumpname);

	if (status != NOT_LOADED)
		Close();

	int lump = W_CheckNumForName(lumpname);
	if (lump < 0)
	{
		I_Warning("mp3player_c: LUMP '%s' not found.\n", lumpname);
		return false;
	}

	epi::file_c *F = W_OpenLump(lump);

	int length = F->GetLength();

	byte *data = F->LoadIntoMemory();

	if (! data)
	{
		delete F;
		I_Warning("mp3player_c: Error loading data.\n");
		return false;
	}
	if (length < 4)
	{
		delete F;
		I_Debugf("mp3player_c: ignored short data (%d bytes)\n", length);
		return false;
	}

	mp3_lump.data = data;
	mp3_lump.size = length;
	mp3_lump.pos  = 0;

    int result = ov_open_callbacks((void*)&mp3_lump, &mp3_stream, NULL, 0, CB);

    if (result < 0)
    {
		// Only time we have to kill this since MP3 will deal with
		// the handle when ov_open_callbacks() succeeds
        mp3player_memclose((void*)&mp3_lump);

		I_Error("[mp3player_c::Open](DataLump) Failed!\n");
		return false; /* NOT REACHED */
    }

	PostOpenInit();
	return true;
}

bool mp3player_c::OpenFile(const char *filename)
{
	SYS_ASSERT(filename);

	if (status != NOT_LOADED)
		Close();

    if (mp3dec_ex_open(&mp3_file, filename, MP3D_SEEK_TO_SAMPLE))
    {
		I_Warning("mp3player_c: Could not open file: '%s'\n", filename);
		return false;
    }

	PostOpenInit();
	return true;
}


void mp3player_c::Close()
{
	if (status == NOT_LOADED)
		return;

	// Stop playback
	if (status != STOPPED)
		Stop();

	mp3dec_ex_close(&mp3_stream);
	
	mp3_file = NULL;

	if (mp3_lump.data)
	{
		delete[] mp3_lump.data;
		mp3_lump.data = NULL;
	}

	status = NOT_LOADED;
}


void mp3player_c::Pause()
{
	if (status != PLAYING)
		return;

	status = PAUSED;
}


void mp3player_c::Resume()
{
	if (status != PAUSED)
		return;

	status = PLAYING;
}


void mp3player_c::Play(bool loop)
{
    if (status != NOT_LOADED && status != STOPPED) return;

	status = PLAYING;
	looping = loop;

	// Load up initial buffer data
	Ticker();
}


void mp3player_c::Stop()
{
	if (status != PLAYING && status != PAUSED)
		return;

	S_QueueStop();

	status = STOPPED;
}


void mp3player_c::Ticker()
{
	while (status == PLAYING)
	{
		epi::sound_data_c *buf = S_QueueGetFreeBuffer(MP3V_NUM_SAMPLES, 
				(is_stereo && dev_stereo) ? epi::SBUF_Interleaved : epi::SBUF_Mono);

		if (! buf)
			break;

		if (StreamIntoBuffer(buf))
		{
			S_QueueAddBuffer(buf, vorbis_inf->rate);
		}
		else
		{
			// finished playing
			S_QueueReturnBuffer(buf);
			Stop();
		}
	}
}


//----------------------------------------------------------------------------

abstract_music_c * S_PlayMP3Music(const pl_entry_c *musdat, float volume, bool looping)
{
	mp3player_c *player = new mp3player_c();

	if (musdat->infotype == MUSINF_LUMP)
	{
		if (! player->OpenLump(musdat->info.c_str()))
		{
			delete player;
			return NULL;
		}
	}
	else if (musdat->infotype == MUSINF_FILE)
	{
		if (! player->OpenFile(musdat->info.c_str()))
		{
			delete player;
			return NULL;
		}
	}
	else
		I_Error("S_PlayMP3Music: bad format value %d\n", musdat->infotype);

	player->Volume(volume);
	player->Play(looping);

	return player;
}


bool S_LoadMP3Sound(epi::sound_data_c *buf, const byte *data, int length)
{
	datalump_s mp3_lump;

	mp3_lump.data = data;
	mp3_lump.size = length;
	mp3_lump.pos  = 0;

	mp3dec_ex_t mp3_stream;

    if (mp3dec_ex_open_buf(&mp3_stream, mp3_lump.data, mp3_lump.size, MP3D_SEEK_TO_SAMPLE) != 0)
    {
		I_Warning("Failed to load MP3 sound (corrupt mp3?)\n");

        mp3player_memclose((void*)&mp3_lump);
  
		return false;
    }

    I_Debugf("MP3 SFX Loader: freq %d Hz, %d channels\n",
			 mp3_stream.info.hz, mp3_stream.info.channels);

	if (mp3_stream.info.channels > 2)
	{
		I_Warning("MP3 Sfx Loader: too many channels: %d\n", mp3_stream.info.channels);

		mp3_lump.size = 0;
		mp3dec_ex_close(&mp3_stream);

		return false;
	}

	bool is_stereo = (mp3_stream.info.channels > 1);
	int mp3_endian = (EPI_BYTEORDER == EPI_LIL_ENDIAN) ? 0 : 1;

	buf->freq = mp3_stream.info.hz;

	epi::sound_gather_c gather;

	while (true)
	{
		int want = 2048;

		s16_t *buffer = gather.MakeChunk(want, is_stereo);

		int section;
		int got_size = ov_read(&mp3_stream, (char *)buffer,
				want * (is_stereo ? 2 : 1) * sizeof(s16_t),
				mp3_endian, sizeof(s16_t), 1 /* signed data */,
				&section);

		if (got_size == OV_HOLE)  // ignore corruption
		{
			gather.DiscardChunk();
			continue;
		}

		if (got_size == 0)  /* EOF */
		{
			gather.DiscardChunk();
			break;
		}
		else if (got_size < 0)  /* ERROR */
		{
			gather.DiscardChunk();

			I_Warning("Problem occurred while loading MP3 (%d)\n", got_size);
			break;
		}

		got_size /= (is_stereo ? 2 : 1) * sizeof(s16_t);

		gather.CommitChunk(got_size);
	}

	if (! gather.Finalise(buf, false /* want_stereo */))
		I_Error("MP3 SFX Loader: no samples!\n");

	// HACK: we must not free the data (in mp3player_memclose)
	mp3_lump.size = 0;

	mp3dec_ex_close(&mp3_stream);

	return true;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
