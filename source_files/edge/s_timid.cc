//----------------------------------------------------------------------------
//  EDGE Timidity Music Player
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

#include "i_defs.h"

#include "file.h"
#include "filesystem.h"
#include "mus_2_midi.h"
#include "path.h"
#include "str_format.h"

#include "timidity.h"

#include "s_blit.h"
#include "s_music.h"
#include "s_timid.h"

#include "dm_state.h"
#include "m_misc.h"  // var_timid_factor


#define TIMV_NUM_SAMPLES  4096

extern bool dev_stereo;  // FIXME: encapsulation
extern int  dev_freq;    //

static bool timidity_inited;


class tim_player_c : public abstract_music_c
{
private:
	enum status_e
	{
		NOT_LOADED, PLAYING, PAUSED, STOPPED
	};
	
	int status;
	bool looping;

	struct MidiSong *song;

public:
	tim_player_c(struct MidiSong *_song) : status(NOT_LOADED), song(_song)
	{ }

	~tim_player_c()
	{
		Close();
	}

public:

	void Close(void)
	{
		if (status == NOT_LOADED)
			return;

		// Stop playback
		Stop();

		if (song)
		{
			Timidity_FreeSong(song);
			song = NULL;
		}
		
		status = NOT_LOADED;
	}

	void Play(bool loop)
	{
		if (! (status == NOT_LOADED || status == STOPPED))
			return;

		status = PLAYING;
		looping = loop;

		SYS_ASSERT(song);

		Timidity_Start(song);

		// Load up initial buffer data
		Ticker();
	}

	void Stop(void)
	{
		if (! (status == PLAYING || status == PAUSED))
			return;

		S_QueueStop();

		status = STOPPED;
	}

	void Pause(void)
	{
		if (status != PLAYING)
			return;

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
			epi::sound_data_c *buf = S_QueueGetFreeBuffer(TIMV_NUM_SAMPLES, 
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

		while (samples < TIMV_NUM_SAMPLES)
		{
			s16_t *data_buf = buf->data_L + samples * (dev_stereo ? 2 : 1);

			int got_num = Timidity_PlaySome(data_buf, TIMV_NUM_SAMPLES - samples);

			if (got_num <= 0)  /* EOF */
			{
				if (looping)
				{
					Timidity_Stop();
					Timidity_Start(song);

					continue; // try again
				}

				return (samples == TIMV_NUM_SAMPLES);
			}

			samples += got_num;
		}

		return true;
	}
};



//----------------------------------------------------------------------------

static const char *config_base_dirs[] =
{
	"$g",   // game_dir
	"$h",   // home_dir

	".",    // current directory
	
#ifdef WIN32
	"\\",
#endif

#ifdef __linux__
	"/etc",
	"/usr/local/lib",
	"/usr/local/share",
	"/usr/lib",
	"/usr/share",
	"/opt",
#endif

	NULL // the end
};

static const char *config_sub_dirs[] =
{
	".",
	
	"freepats",
#ifndef WIN32
	"Freepats", "FREEPATS",
#endif

	"timidity",
#ifndef WIN32
	"Timidity", "TIMIDITY",
#endif

	"8mbgmpat",
#ifndef WIN32
	"8MBGMPAT",
#endif

#ifdef WIN32
	"timidity\\patches",
#else
	"timidity/patches",
#endif

	NULL // end of list
};

static const char *config_names[] =
{
	"timidity.cfg",
#ifndef WIN32
	"Timidity.cfg", "TIMIDITY.cfg", "TIMIDITY.CFG",
#endif

	"crude.cfg",
#ifndef WIN32
	"Crude.cfg", "CRUDE.cfg", "CRUDE.CFG",
#endif	

	"freepats.cfg",
#ifndef WIN32
	"Freepats.cfg", "FREEPATS.cfg", "FREEPATS.CFG",
#endif

	NULL // the end
};


static const char * FindTimidityConfig(void)
{
	I_Debugf("** FindTimidityConfig:\n");

	for (int i = 0; config_base_dirs[i]; i++)
	{
		for (int j = 0; config_sub_dirs[j]; j++)
		{
			const char *base = config_base_dirs[i];
			const char *sub  = config_sub_dirs[j];
				
			std::string dir;

			if (base[0] == '$' && base[1] == 'g')
				dir = game_dir;
			else if (base[0] == '$' && base[1] == 'h')
				dir = home_dir;
			else if (base[0] == '.')
			{ /* leave empty */ } 
			else
				dir = std::string(base);
			
			if (sub[0] == '.')
			{ /* no change */ }
			else if (dir.length() == 0)
				dir = std::string(sub);
			else
				dir = epi::PATH_Join(dir.c_str(), sub);
			
			// if the directory does not exist, then there is no
			// need to proceed (hence saving a LOT of time).

			I_Debugf("TIMID: checking directory '%s'\n", dir.c_str());

			if (dir.length() > 0 && ! epi::FS_IsDir(dir.c_str()))
				continue;

			for (int k = 0; config_names[k]; k++)
			{
				std::string fn;

				if (dir.length() == 0)
					fn = std::string(config_names[k]);
				else
					fn = epi::PATH_Join(dir.c_str(), config_names[k]);
				
				I_Debugf("  trying '%s'\n", fn.c_str());
				
				if (epi::FS_Access(fn.c_str(), epi::file_c::ACCESS_READ))
				{
					I_Debugf("  ^___ EXISTS\n");

					return strdup(fn.c_str());
				}
			}
		}
	}

	I_Printf("tim_player_c: Cannot find Timidity config file!\n");
	return NULL;
}


bool S_StartupTimidity(void)
{
	const char *config_fn = FindTimidityConfig();

	if (! config_fn)
		return false;

	I_Printf("Loading Timidity config: %s\n", config_fn);

	if (! Timidity_Init(config_fn, dev_freq, dev_stereo ? 2 : 1))
		return false;

	timidity_inited = true;

	return true; // OK!
}


void S_ChangeTimidQuiet(void)
{
	Timidity_QuietFactor(var_timid_factor);
}


abstract_music_c * S_PlayTimidity(byte *data, int length, bool is_mus,
			float volume, bool loop)
{
	if (!timidity_inited)
		return NULL;

	if (is_mus)
	{
		I_Debugf("tim_player_c: Converting MUS format to MIDI...\n");

		byte *midi_data;
		int midi_len;

		if (! Mus2Midi::Convert(data, length, &midi_data, &midi_len,
					Mus2Midi::DOOM_DIVIS, true))
		{
			delete [] data;

			I_Warning("Unable to convert MUS to MIDI !\n");
			return NULL;
		}

		delete[] data;

		data   = midi_data;
		length = midi_len;

		I_Debugf("Conversion done: new length is %d\n", length);
	}

	struct MidiSong *song = Timidity_LoadSong(data, length);

	delete[] data;

	if (! song) //Lobo: quietly log it instead of completely exiting EDGE
	{
		I_Debugf("Timidity player: failed to load MIDI file!\n");
		return NULL;
	}

	S_ChangeTimidQuiet();

	tim_player_c *player = new tim_player_c(song);

	player->Volume(volume);
	player->Play(loop);

	return player;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
