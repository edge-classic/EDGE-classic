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

#include "dm_state.h"

// these are in the opl/ directory
#include "genmidi.h"
#include "midifile.h"
#include "opl_player.h"

#define NUM_SAMPLES  4096

extern bool dev_stereo;
extern int  dev_freq;

static bool opl_inited;

class opl_player_c : public abstract_music_c
{
private:
	enum status_e
	{
		NOT_LOADED, PLAYING, PAUSED, STOPPED
	};

	int status;
	bool looping;
	double current_time;

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

		//??  OPLAY_Close()

		MIDI_FreeFile(song);

		status = NOT_LOADED;
	}

	void Play(bool loop)
	{
		if (! (status == NOT_LOADED || status == STOPPED))
			return;

		status  = PLAYING;
		looping = loop;

		SYS_ASSERT(song);
		current_time = 0;

		// Load up initial buffer data
		Ticker();
	}

	void Stop(void)
	{
		if (! (status == PLAYING || status == PAUSED))
			return;

		// OPLAY_note_off_all();

		S_QueueStop();

		status = STOPPED;
	}

	void Pause(void)
	{
		if (status != PLAYING)
			return;

		// OPLAY_note_off_all();

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
	bool PlaySome(s16_t *data_buf, int samples)
	{
		// OPLAY_Stream(data_buf, samples)

		return false;
	}

	bool StreamIntoBuffer(epi::sound_data_c *buf)
	{
		int samples = 0;

		bool song_done = false;

		while (samples < NUM_SAMPLES)
		{
			s16_t *data_buf = buf->data_L + samples * (dev_stereo ? 2 : 1);

			song_done = PlaySome(data_buf, NUM_SAMPLES - samples);

			if (song_done)  /* EOF */
			{
				if (looping)
				{
					// OPLAY_Restart()
					current_time = 0;
					continue;
				}

				// FIXME fill remaining buffer with zeroes

				return (false);
			}

			samples += NUM_SAMPLES;
		}

		return true;
	}
};

bool S_StartupOPL(void)
{
	I_Printf("Initializing OPL player...\n");

	// TODO find GENMIDI lump !!

	opl_inited = true;

	return true; // OK
}

abstract_music_c * S_PlayOPL(byte *data, int length, bool is_mus, float volume, bool loop)
{
	if (! opl_inited)
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
