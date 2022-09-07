//----------------------------------------------------------------------------
//  EDGE OPL-Emulation Music Player
//----------------------------------------------------------------------------
//
//  Copyright (c) 2004-2022  The EDGE Team.
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

#include "file.h"
#include "filesystem.h"
#include "mus_2_midi.h"
#include "path.h"
#include "str_format.h"

#include "m_misc.h"
#include "s_blit.h"
#include "s_music.h"
#include "s_opl.h"
#include "w_wad.h"

#include "dm_state.h"

// these are in the opl/ directory
#include "genmidi.h"
#include "midifile.h"
#include "opl_player.h"

#define NUM_SAMPLES  1024

extern bool dev_stereo;
extern int  dev_freq;

static bool opl_inited;
static bool opl_disabled;

static bool S_StartupOPL(void)
{
	I_Debugf("Initializing OPL player...\n");

	if (! OPLAY_Init(dev_freq, dev_stereo))
	{
		return false;
	}

	int p = W_CheckNumForName("GENMIDI");
	if (p < 0)
	{
		I_Debugf("no GENMIDI lump !\n");
		return false;
	}

	int length;
	const byte *data = (const byte*)W_ReadLumpAlloc(p, &length);

	GM_LoadInstruments(data, (size_t)length);

	delete[] data;

	// OK
	return true;
}

class opl_player_c : public abstract_music_c
{
private:
	enum status_e
	{
		NOT_LOADED, PLAYING, PAUSED, STOPPED
	};

	int status;
	bool looping;

	midi_file_t *song;

public:
	opl_player_c(midi_file_t *_song) : status(NOT_LOADED), looping(false), song(_song)
	{ }

	~opl_player_c()
	{
		Close();
	}

public:
	void Close(void)
	{
		if (status == NOT_LOADED)
			return;

		// Stop playback
		if (status != STOPPED)
		  Stop();

		OPLAY_FinishSong();

		MIDI_FreeFile(song);

		status = NOT_LOADED;
	}

	void Play(bool loop)
	{
		if (! (status == NOT_LOADED || status == STOPPED))
			return;

		status  = PLAYING;
		looping = loop;

		OPLAY_StartSong(song);

		// Load up initial buffer data
		Ticker();
	}

	void Stop(void)
	{
		if (! (status == PLAYING || status == PAUSED))
			return;

		OPLAY_NotesOff();

		S_QueueStop();

		status = STOPPED;
	}

	void Pause(void)
	{
		if (status != PLAYING)
			return;

		OPLAY_NotesOff();

		status = PAUSED;
	}

	void Resume(void)
	{
		if (status != PAUSED)
			return;

		status = PLAYING;
	}

	void Ticker(void)
	{
		while (status == PLAYING)
		{
			epi::sound_data_c *buf = S_QueueGetFreeBuffer(NUM_SAMPLES,
					dev_stereo ? epi::SBUF_Interleaved : epi::SBUF_Mono);

			if (! buf)
				break;

			if (StreamIntoBuffer(buf))
			{
				S_QueueAddBuffer(buf, dev_freq);
			}
			else
			{
				// finished playing
				S_QueueReturnBuffer(buf);

				Stop();
			}
		}
	}

	void Volume(float gain)
	{
		// not needed, music volume is handled in s_blit.cc
		// (see mix_channel_c::ComputeMusicVolume).
	}

private:
	bool StreamIntoBuffer(epi::sound_data_c *buf)
	{
		int samples = 0;

		while (samples < NUM_SAMPLES)
		{
			s16_t *data_buf = buf->data_L + samples * (dev_stereo ? 2 : 1);

			int got = OPLAY_Stream(data_buf, NUM_SAMPLES - samples, dev_stereo);

			samples += got;

			if (got > 0)
				continue;

			/* EOF */

			if (looping)
			{
				OPLAY_StartSong(song);
				continue;
			}

			if (samples == 0)
			{
				// buffer not used at all, so free it
				return false;
			}

			// buffer partially used, fill rest with zeroes
			data_buf += got * (dev_stereo ? 2 : 1);
			int total = (NUM_SAMPLES - samples) * (dev_stereo ? 2 : 1);

			for (; total > 0 ; total--)
			{
				*data_buf++ = 0;
			}
			break;
		}

		return true;
	}
};

abstract_music_c * S_PlayOPL(byte *data, int length, bool is_mus, float volume, bool loop)
{
	if (! opl_inited)
	{
		if (! S_StartupOPL())
		{
			opl_disabled = true;
		}
		opl_inited = true;
	}

	if (opl_disabled)
		return NULL;

	if (is_mus)
	{
		I_Debugf("opl_player_c: Converting MUS format to MIDI...\n");

		byte *midi_data;
		int   midi_len;

		if (! Mus2Midi::Convert(data, length, &midi_data, &midi_len, Mus2Midi::DOOM_DIVIS, true))
		{
			delete[] data;

			I_Warning("Unable to convert MUS to MIDI !\n");
			return NULL;
		}

		delete[] data;

		data   = midi_data;
		length = midi_len;

		I_Debugf("Conversion done: new length is %d\n", length);
	}

	midi_file_t *song = MIDI_LoadFile(data, length);

	if (! song)
	{
		I_Debugf("OPL player: failed to load MIDI file!\n");
		return NULL;
	}

	delete[] data;

	opl_player_c *player = new opl_player_c(song);

	player->Volume(volume);
	player->Play(loop);

	return player;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
