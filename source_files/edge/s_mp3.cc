//----------------------------------------------------------------------------
//  EDGE MP3 Music Player
//----------------------------------------------------------------------------
//
//  Copyright (c) 2004-2009  The EDGE Team.
//  Adapted from the EDGE Ogg Player in 2021 - Dashodanger
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
#define MINIMP3_IMPLEMENTATION
#include "minimp3_ex.h"

extern bool dev_stereo;  // FIXME: encapsulation

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
	bool is_stereo;

	mp3dec_ex_t mp3_track;
	mp3dec_io_t io;

	s16_t *mono_buffer;

public:
	bool OpenFile(epi::file_c *file);

	virtual void Close(void);

	virtual void Play(bool loop);
	virtual void Stop(void);

	virtual void Pause(void);
	virtual void Resume(void);

	virtual void Ticker(void);
	virtual void Volume(float gain);

private:
	void PostOpenInit(void);

	bool StreamIntoBuffer(epi::sound_data_c *buf);
};

//----------------------------------------------------------------------------

mp3player_c::mp3player_c() : status(NOT_LOADED)
{
	mono_buffer = new s16_t[MINIMP3_IO_SIZE * 2];
}

mp3player_c::~mp3player_c()
{
	Close();

	if (mono_buffer)
		delete[] mono_buffer;
}

void mp3player_c::PostOpenInit()
{
    if (mp3_track.info.channels == 1) {
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
	s16_t *data_buf;

	if (is_stereo && !dev_stereo)
		data_buf = mono_buffer;
	else
		data_buf = buf->data_L;

	int got_size = mp3dec_ex_read(&mp3_track, data_buf, MINIMP3_IO_SIZE * 2);

	if (got_size == 0)  /* EOF */
	{
		if (! looping)
			return false;
		mp3dec_ex_seek(&mp3_track, 0);
		buf->Free();
		return true;
	}

	if (got_size < 0)  /* ERROR */
	{
		I_Debugf("[mp3player_c::StreamIntoBuffer] Failed\n");
		return false;
	}

	got_size /= (is_stereo ? 2 : 1);

	buf->length = got_size;

	if (is_stereo && !dev_stereo)
		ConvertToMono(buf->data_L, mono_buffer, got_size);

    return (true);
}


void mp3player_c::Volume(float gain)
{
	// not needed, music volume is handled in s_blit.cc
	// (see mix_channel_c::ComputeMusicVolume).
}


static size_t mp3player_epi_read(void *buf, size_t size, void *user_data)
{
	epi::file_c *file = (epi::file_c *)user_data;

	return file->Read(buf, (unsigned int)size);
}


static int mp3player_epi_seek(uint64_t position, void *user_data)
{
	epi::file_c *file = (epi::file_c *)user_data;

	return file->Seek((int)position, epi::file_c::SEEKPOINT_START) ? 0 : -1;
}


bool mp3player_c::OpenFile(epi::file_c *file)
{
	SYS_ASSERT(file);

	if (status != NOT_LOADED)
		Close();

	io.read = &mp3player_epi_read;
	io.seek = &mp3player_epi_seek;
	io.read_data = (void *) file;
	io.seek_data = (void *) file;

    if (mp3dec_ex_open_cb(&mp3_track, &io, MP3D_SEEK_TO_SAMPLE) != 0)
    {
		I_Warning("mp3player_c: Could not open MP3 file.\n");
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

	mp3dec_ex_close(&mp3_track);

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
		epi::sound_data_c *buf = S_QueueGetFreeBuffer(MINIMP3_IO_SIZE, 
				(is_stereo && dev_stereo) ? epi::SBUF_Interleaved : epi::SBUF_Mono);

		if (! buf)
			break;

		if (StreamIntoBuffer(buf))
		{
			if (buf->length > 0)
				S_QueueAddBuffer(buf, mp3_track.info.hz);
			else
				S_QueueReturnBuffer(buf);
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

abstract_music_c * S_PlayMP3Music(epi::file_c *file, float volume, bool looping)
{
	mp3player_c *player = new mp3player_c();

	if (! player->OpenFile(file))
	{
		delete player;
		return NULL;
	}

	player->Volume(volume);
	player->Play(looping);

	return player;
}


bool S_LoadMP3Sound(epi::sound_data_c *buf, const byte *data, int length)
{
	mp3dec_t mp3_sound;
	mp3dec_file_info_t sound_info;

    if (mp3dec_load_buf(&mp3_sound, data, length, &sound_info, NULL, NULL) != 0)
    {
		I_Warning("Failed to load MP3 sound (corrupt mp3?)\n");

		return false;
    }

	I_Debugf("MP3 SFX Loader: freq %d Hz, %d channels\n",
			 sound_info.hz, sound_info.channels);

	if (sound_info.channels > 2)
	{
		I_Warning("MP3 SFX Loader: too many channels: %d\n", sound_info.channels);

		free(sound_info.buffer);

		return false;
	}

	if (sound_info.samples <= 0) // I think the initial loading would fail if this were the case, but just as a sanity check - Dasho
	{
		I_Error("MP3 SFX Loader: no samples!\n");
		return false;
	}

	bool is_stereo = (sound_info.channels > 1);

	buf->freq = sound_info.hz;

	epi::sound_gather_c gather;

	s16_t *buffer = gather.MakeChunk(sound_info.samples, is_stereo);

	memcpy(buffer, sound_info.buffer, sound_info.samples * (is_stereo ? 2 : 1) * sizeof(s16_t));

	gather.CommitChunk(sound_info.samples);

	if (! gather.Finalise(buf, is_stereo))
		I_Error("MP3 SFX Loader: no samples!\n");

	free(sound_info.buffer);

	return true;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
