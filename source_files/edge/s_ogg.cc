//----------------------------------------------------------------------------
//  EDGE OGG Music Player
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
#include "s_ogg.h"
#include "w_wad.h"

#define OGGV_NUM_SAMPLES  8192

#define OV_EXCLUDE_STATIC_CALLBACKS
#define OGG_IMPL
#define VORBIS_IMPL
#include "minivorbis.h"

extern bool dev_stereo;  // FIXME: encapsulation


struct datalump_s
{
	const byte *data;

	size_t pos;
	size_t size;
};


class oggplayer_c : public abstract_music_c
{
public:
	 oggplayer_c();
	~oggplayer_c();

private:

	enum status_e
	{
		NOT_LOADED, PLAYING, PAUSED, STOPPED
	};
	
	int status;
	bool looping;

	FILE *ogg_file;
	datalump_s ogg_lump;

	OggVorbis_File ogg_stream;
	vorbis_info *vorbis_inf;
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
// oggplayer datalump operation functions
//
size_t oggplayer_memread(void *ptr, size_t size, size_t nmemb, void *datasource)
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

int oggplayer_memseek(void *datasource, ogg_int64_t offset, int whence)
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

int oggplayer_memclose(void *datasource)
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

long oggplayer_memtell(void *datasource)
{
	datalump_s *d = (datalump_s *)datasource;

	if (d->pos > d->size)
		return -1;

	return d->pos;
}

//
// oggplayer file operation functions
//
size_t oggplayer_fread(void *ptr, size_t size, size_t nmemb, void *datasource)
{
	return fread(ptr, size, nmemb, (FILE*)datasource);
}

int oggplayer_fseek(void *datasource, ogg_int64_t offset, int whence)
{
	return fseek((FILE*)datasource, (int)offset, whence);
}

int oggplayer_fclose(void *datasource)
{
	return fclose((FILE*)datasource);
}

long oggplayer_ftell(void *datasource)
{
	return ftell((FILE*)datasource);
}


//----------------------------------------------------------------------------


oggplayer_c::oggplayer_c() : status(NOT_LOADED), vorbis_inf(NULL)
{
	ogg_file = NULL;

	ogg_lump.data = NULL;

	mono_buffer = new s16_t[OGGV_NUM_SAMPLES * 2];
}

oggplayer_c::~oggplayer_c()
{
	Close();

	if (mono_buffer)
		delete[] mono_buffer;
}

const char * oggplayer_c::GetError(int code)
{
    switch (code)
    {
        case OV_EREAD:
            return ("Read from media error.");

		case OV_ENOTVORBIS:
            return ("Not Vorbis data.");

		case OV_EVERSION:
            return ("Vorbis version mismatch.");

		case OV_EBADHEADER:
            return ("Invalid Vorbis header.");

		case OV_EFAULT:
            return ("Internal error.");

		default:
			break;
    }

	return ("Unknown Ogg error.");
}


void oggplayer_c::PostOpenInit()
{
    vorbis_inf = ov_info(&ogg_stream, -1);
	SYS_ASSERT(vorbis_inf);

    if (vorbis_inf->channels == 1) {
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


bool oggplayer_c::StreamIntoBuffer(epi::sound_data_c *buf)
{
	int ogg_endian = (EPI_BYTEORDER == EPI_LIL_ENDIAN) ? 0 : 1;

    int samples = 0;

    while (samples < OGGV_NUM_SAMPLES)
    {
		s16_t *data_buf;

		if (is_stereo && !dev_stereo)
			data_buf = mono_buffer;
		else
			data_buf = buf->data_L + samples * (is_stereo ? 2 : 1);

		int section;
        int got_size = ov_read(&ogg_stream, (char *)data_buf,
				(OGGV_NUM_SAMPLES - samples) * (is_stereo ? 2 : 1) * sizeof(s16_t),
				ogg_endian, sizeof(s16_t), 1 /* signed data */,
				&section);

		if (got_size == OV_HOLE)  // ignore corruption
			continue;

		if (got_size == 0)  /* EOF */
		{
			if (! looping)
				break;

			ov_raw_seek(&ogg_stream, 0);
			continue; // try again
		}

		if (got_size < 0)  /* ERROR */
		{
			// Construct an error message
			std::string err_msg("[oggplayer_c::StreamIntoBuffer] Failed: ");

			err_msg += GetError(got_size);

			// FIXME: using I_Error is too harsh
			I_Error("%s", err_msg.c_str());
			return false; /* NOT REACHED */
		}

		got_size /= (is_stereo ? 2 : 1) * sizeof(s16_t);

		if (is_stereo && !dev_stereo)
			ConvertToMono(buf->data_L + samples, mono_buffer, got_size);

		samples += got_size;
    }

    return (samples > 0);
}


void oggplayer_c::Volume(float gain)
{
	// not needed, music volume is handled in s_blit.cc
	// (see mix_channel_c::ComputeMusicVolume).
}


bool oggplayer_c::OpenLump(const char *lumpname)
{
	SYS_ASSERT(lumpname);

	if (status != NOT_LOADED)
		Close();

	int lump = W_CheckNumForName(lumpname);
	if (lump < 0)
	{
		I_Warning("oggplayer_c: LUMP '%s' not found.\n", lumpname);
		return false;
	}

	epi::file_c *F = W_OpenLump(lump);

	int length = F->GetLength();

	byte *data = F->LoadIntoMemory();

	if (! data)
	{
		delete F;
		I_Warning("oggplayer_c: Error loading data.\n");
		return false;
	}
	if (length < 4)
	{
		delete F;
		I_Debugf("oggplayer_c: ignored short data (%d bytes)\n", length);
		return false;
	}

	ogg_lump.data = data;
	ogg_lump.size = length;
	ogg_lump.pos  = 0;

	ov_callbacks CB;

	CB.read_func  = oggplayer_memread;
	CB.seek_func  = oggplayer_memseek;
	CB.close_func = oggplayer_memclose;
	CB.tell_func  = oggplayer_memtell; 

    int result = ov_open_callbacks((void*)&ogg_lump, &ogg_stream, NULL, 0, CB);

    if (result < 0)
    {
		// Only time we have to kill this since OGG will deal with
		// the handle when ov_open_callbacks() succeeds
        oggplayer_memclose((void*)&ogg_lump);
  
		// Construct an error message
		std::string err_msg("[oggplayer_c::Open](DataLump) Failed: ");

		err_msg += GetError(result);

		I_Error("%s\n", err_msg.c_str());
		return false; /* NOT REACHED */
    }

	PostOpenInit();
	return true;
}

bool oggplayer_c::OpenFile(const char *filename)
{
	SYS_ASSERT(filename);

	if (status != NOT_LOADED)
		Close();

	ogg_file = fopen(filename, "rb");
    if (!ogg_file)
    {
		I_Warning("oggplayer_c: Could not open file: '%s'\n", filename);
		return false;
    }

	ov_callbacks CB;

	CB.read_func  = oggplayer_fread;
	CB.seek_func  = oggplayer_fseek;
	CB.close_func = oggplayer_fclose;
	CB.tell_func  = oggplayer_ftell; 

    int result = ov_open_callbacks((void*)ogg_file, &ogg_stream, NULL, 0, CB);

    if (result < 0)
    {
        oggplayer_fclose(ogg_file);	
  
		std::string err_msg("[oggplayer_c::OpenFile] Failed: ");

		err_msg += GetError(result);
		
		I_Error("%s\n", err_msg.c_str());
		return false; /* NOT REACHED */
    }

	PostOpenInit();
	return true;
}


void oggplayer_c::Close()
{
	if (status == NOT_LOADED)
		return;

	// Stop playback
	Stop();

	ov_clear(&ogg_stream);
	
	// NOTE: the ov_clear has called our fclose() callback
	ogg_file = NULL;

	if (ogg_lump.data)
	{
		delete[] ogg_lump.data;
		ogg_lump.data = NULL;
	}

	status = NOT_LOADED;
}


void oggplayer_c::Pause()
{
	if (status != PLAYING)
		return;

	status = PAUSED;
}


void oggplayer_c::Resume()
{
	if (status != PAUSED)
		return;

	status = PLAYING;
}


void oggplayer_c::Play(bool loop)
{
    if (status != NOT_LOADED && status != STOPPED) return;

	status = PLAYING;
	looping = loop;

	// Load up initial buffer data
	Ticker();
}


void oggplayer_c::Stop()
{
	if (status != PLAYING && status != PAUSED)
		return;

	S_QueueStop();

	status = STOPPED;
}


void oggplayer_c::Ticker()
{
	while (status == PLAYING)
	{
		epi::sound_data_c *buf = S_QueueGetFreeBuffer(OGGV_NUM_SAMPLES, 
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

abstract_music_c * S_PlayOGGMusic(const pl_entry_c *musdat, float volume, bool looping)
{
	oggplayer_c *player = new oggplayer_c();

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
		I_Error("S_PlayOGGMusic: bad format value %d\n", musdat->infotype);

	player->Volume(volume);
	player->Play(looping);

	return player;
}


bool S_LoadOGGSound(epi::sound_data_c *buf, const byte *data, int length)
{
	datalump_s ogg_lump;

	ogg_lump.data = data;
	ogg_lump.size = length;
	ogg_lump.pos  = 0;

	ov_callbacks CB;

	CB.read_func  = oggplayer_memread;
	CB.seek_func  = oggplayer_memseek;
	CB.close_func = oggplayer_memclose;
	CB.tell_func  = oggplayer_memtell; 

	OggVorbis_File ogg_stream;

    int result = ov_open_callbacks((void*)&ogg_lump, &ogg_stream, NULL, 0, CB);

    if (result < 0)
    {
		I_Warning("Failed to load OGG sound (corrupt ogg?) error=%d\n", result);

		// Only time we have to kill this since OGG will deal with
		// the handle when ov_open_callbacks() succeeds
        oggplayer_memclose((void*)&ogg_lump);
  
		return false;
    }

	vorbis_info *vorbis_inf = ov_info(&ogg_stream, -1);
	SYS_ASSERT(vorbis_inf);

	I_Debugf("OGG SFX Loader: freq %d Hz, %d channels\n",
			 (int)vorbis_inf->rate, (int)vorbis_inf->channels);

	if (vorbis_inf->channels > 2)
	{
		I_Warning("OGG Sfx Loader: too many channels: %d\n", vorbis_inf->channels);

		ogg_lump.size = 0;
		ov_clear(&ogg_stream);

		return false;
	}

	bool is_stereo = (vorbis_inf->channels > 1);
	int ogg_endian = (EPI_BYTEORDER == EPI_LIL_ENDIAN) ? 0 : 1;

	buf->freq = vorbis_inf->rate;

	epi::sound_gather_c gather;

	while (true)
	{
		int want = 2048;

		s16_t *buffer = gather.MakeChunk(want, is_stereo);

		int section;
		int got_size = ov_read(&ogg_stream, (char *)buffer,
				want * (is_stereo ? 2 : 1) * sizeof(s16_t),
				ogg_endian, sizeof(s16_t), 1 /* signed data */,
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

			I_Warning("Problem occurred while loading OGG (%d)\n", got_size);
			break;
		}

		got_size /= (is_stereo ? 2 : 1) * sizeof(s16_t);

		gather.CommitChunk(got_size);
	}

	if (! gather.Finalise(buf, false /* want_stereo */))
		I_Error("OGG SFX Loader: no samples!\n");

	// HACK: we must not free the data (in oggplayer_memclose)
	ogg_lump.size = 0;

	ov_clear(&ogg_stream);

	return true;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
