//----------------------------------------------------------------------------
//  EDGE FluidLite Music Player
//----------------------------------------------------------------------------
// 
//  Copyright (c) 2022  The EDGE Team.
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
#include "path.h"
#include "str_util.h"

#include "m_misc.h"
#include "s_blit.h"
#include "s_music.h"
#include "s_fluid.h"

#include "dm_state.h"

#define BW_MidiSequencer FluidSequencer
typedef struct BW_MidiRtInterface FluidInterface;
#include "midi_sequencer_impl.hpp"

#include "fluidlite.h"

#define FLUID_NUM_SAMPLES  4096

extern bool dev_stereo;
extern int  dev_freq; 

bool fluid_disabled = false;

fluid_synth_t *edge_fluid;
fluid_settings_t *edge_fluid_settings;
fluid_sfloader_t *edge_fluid_sfloader;

DEF_CVAR(s_soundfont, "default.sf2", CVAR_ARCHIVE)

extern std::vector<std::filesystem::path> available_soundfonts;

// Nneeded to support loading soundfonts from memory in Fluidlite
void *edge_fluid_open(fluid_fileapi_t *fileapi, const char *filename)
{
    void *p;
 
    if(filename[0] != '&')
    {
        return NULL;
    }
 
    sscanf(filename, "&%p", &p);
    return p;
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

bool S_StartupFluid(void)
{
	I_Printf("Initializing FluidLite...\n");

	// Check for presence of previous CVAR value's file
	bool cvar_good = false;
	for (int i=0; i < available_soundfonts.size(); i++)
	{
		if(epi::case_cmp(s_soundfont.s, available_soundfonts.at(i).u8string()) == 0)
			cvar_good = true;
	}

	if (!cvar_good)
	{
		I_Warning("Cannot find previously used soundfont %s, falling back to default!\n", s_soundfont.c_str());
		s_soundfont = "default.sf2";
	}

 	std::filesystem::path soundfont_dir = epi::PATH_Join(game_dir, UTFSTR("soundfont"));

	edge_fluid_settings = new_fluid_settings();
	edge_fluid = new_fluid_synth(edge_fluid_settings);
	edge_fluid_sfloader = new_fluid_defsfloader();
	edge_fluid_sfloader->fileapi = new fluid_fileapi_t;
	fluid_init_default_fileapi(edge_fluid_sfloader->fileapi);
	edge_fluid_sfloader->fileapi->fopen = edge_fluid_open;
	fluid_synth_add_sfloader(edge_fluid, edge_fluid_sfloader);

	fluid_synth_set_sample_rate(edge_fluid, dev_freq);
	
	char *pointer_filename = new char[64];
	const void *pointer_to_sf2_in_mem;
	FILE* sf_fp = EPIFOPEN(epi::PATH_Join(soundfont_dir, UTFSTR(s_soundfont.s)), "rb");
	if (sf_fp)
	{
		pointer_to_sf2_in_mem = (void *)sf_fp;
		sprintf(pointer_filename, "&%p", pointer_to_sf2_in_mem);
	}
	if (fluid_synth_sfload(edge_fluid, pointer_filename, 1) == -1)
	{
		I_Warning("FluidLite: Could not load requested soundfont %s! Falling back to default soundfont!\n", s_soundfont.c_str());
		std::fclose(sf_fp);
		pointer_to_sf2_in_mem = nullptr;
		memset(pointer_filename, 0, 64);
		sf_fp = EPIFOPEN(epi::PATH_Join(soundfont_dir, UTFSTR("default.sf2")), "rb");
		if (sf_fp)
		{
			pointer_to_sf2_in_mem = (void *)sf_fp;
			sprintf(pointer_filename, "&%p", pointer_to_sf2_in_mem);
		}
		if (fluid_synth_sfload(edge_fluid, pointer_filename, 1) == -1) 
		{
			I_Warning("Could not load any soundfonts! Ensure that default.sf2 is present in the soundfont directory!\n");
			std::fclose(sf_fp);
			pointer_to_sf2_in_mem = nullptr;
			delete[] pointer_filename;
			delete_fluid_synth(edge_fluid);
			delete_fluid_settings(edge_fluid_settings);
			return false;
		}
	}
	pointer_to_sf2_in_mem = nullptr;
	delete[] pointer_filename;
	fluid_synth_program_reset(edge_fluid);

	return true; // OK!
}

// Should only be invoked when switching soundfonts
void S_RestartFluid(void)
{
	if (fluid_disabled)
		return;

	I_Printf("Restarting FluidLite...\n");

	int old_entry = entry_playing;

	S_StopMusic();

	delete_fluid_synth(edge_fluid);
	delete_fluid_settings(edge_fluid_settings);
	edge_fluid = nullptr;
	edge_fluid_settings = nullptr;

	if (!S_StartupFluid())
	{
		fluid_disabled = true;
		return;
	}

	S_ChangeMusic(old_entry, true); // Restart track that was playing when switched

	return; // OK!
}

class fluid_player_c : public abstract_music_c
{
private:
	enum status_e
	{
		NOT_LOADED, PLAYING, PAUSED, STOPPED
	};
	
	int status;
	bool looping;

	FluidInterface *fluid_iface;

	s16_t *mono_buffer;

public:
	fluid_player_c(byte *_data, int _length, bool _looping) : status(NOT_LOADED), looping(_looping)
	{ 
		mono_buffer = new s16_t[FLUID_NUM_SAMPLES * 2];
		SequencerInit(); 
	}

	~fluid_player_c()
	{
		Close();

		if (mono_buffer)
			delete[] mono_buffer;
	}

public:

	FluidSequencer *fluid_seq;

	static void rtNoteOn(void *userdata, uint8_t channel, uint8_t note, uint8_t velocity)
	{
		fluid_synth_noteon(edge_fluid, channel, note, velocity);
	}

	static void rtNoteOff(void *userdata, uint8_t channel, uint8_t note)
	{
		fluid_synth_noteoff(edge_fluid, channel, note);
	}

	static void rtNoteAfterTouch(void *userdata, uint8_t channel, uint8_t note, uint8_t atVal)
	{
		fluid_synth_key_pressure(edge_fluid, channel, note, atVal);
	}

	static void rtChannelAfterTouch(void *userdata, uint8_t channel, uint8_t atVal)
	{
		fluid_synth_channel_pressure(edge_fluid, channel, atVal);
	}

	static void rtControllerChange(void *userdata, uint8_t channel, uint8_t type, uint8_t value)
	{
		fluid_synth_cc(edge_fluid, channel, type, value);
	}

	static void rtPatchChange(void *userdata, uint8_t channel, uint8_t patch)
	{
		fluid_synth_program_change(edge_fluid, channel, patch);
	}

	static void rtPitchBend(void *userdata, uint8_t channel, uint8_t msb, uint8_t lsb)
	{
		fluid_synth_pitch_bend(edge_fluid, channel, (msb << 7) | lsb);
	}

	static void rtSysEx(void *userdata, const uint8_t *msg, size_t size)
	{
		fluid_synth_sysex(edge_fluid, reinterpret_cast<const char*>(msg), static_cast<int>(size), nullptr, nullptr, nullptr, 0);
	}

	static void rtDeviceSwitch(void *userdata, size_t track, const char *data, size_t length)
	{
		(void)userdata; (void)track; (void)data; (void)length;
	}

	static size_t rtCurrentDevice(void *userdata, size_t track)
	{
		(void)userdata; (void)track;
		return 0;
	}

	static void playSynth(void *userdata, uint8_t *stream, size_t length)
	{
		fluid_synth_write_s16(edge_fluid, static_cast<int>(length) / 4,
			stream, 0, 2, stream + 2, 0, 2);
	}

	void SequencerInit()
	{
		fluid_seq = new FluidSequencer;
		fluid_iface = new FluidInterface;
		std::memset(fluid_iface, 0, sizeof(BW_MidiRtInterface));

		fluid_iface->rtUserData = this;
		fluid_iface->rt_noteOn  = rtNoteOn;
		fluid_iface->rt_noteOff = rtNoteOff;
		fluid_iface->rt_noteAfterTouch = rtNoteAfterTouch;
		fluid_iface->rt_channelAfterTouch = rtChannelAfterTouch;
		fluid_iface->rt_controllerChange = rtControllerChange;
		fluid_iface->rt_patchChange = rtPatchChange;
		fluid_iface->rt_pitchBend = rtPitchBend;
		fluid_iface->rt_systemExclusive = rtSysEx;

		fluid_iface->onPcmRender = playSynth;
		fluid_iface->onPcmRender_userData = this;

		fluid_iface->pcmSampleRate = dev_freq;
		fluid_iface->pcmFrameSize = 2 /*channels*/ * 2 /*size of one sample*/;

		fluid_iface->rt_deviceSwitch = rtDeviceSwitch;
		fluid_iface->rt_currentDevice = rtCurrentDevice;

		fluid_seq->setInterface(fluid_iface);
	}

	bool LoadTrack(const byte *data, int length)
	{
		return fluid_seq->loadMIDI(data, length);
	}

	void Close(void)
	{
		if (status == NOT_LOADED)
			return;

		// Stop playback
		if (status != STOPPED)
		  Stop();
	
		if (fluid_seq)
		{
			delete fluid_seq;
			fluid_seq = nullptr;
		}
		if (fluid_iface)
		{
			delete fluid_iface;
			fluid_iface = nullptr;
		}

		status = NOT_LOADED;
	}

	void Play(bool loop)
	{
		if (! (status == NOT_LOADED || status == STOPPED))
			return;

		status = PLAYING;
		looping = loop;

		// Load up initial buffer data
		Ticker();
	}

	void Stop(void)
	{
		if (! (status == PLAYING || status == PAUSED))
			return;

		fluid_synth_all_voices_stop(edge_fluid);

		S_QueueStop();

		status = STOPPED;
	}

	void Pause(void)
	{
		if (status != PLAYING)
			return;

		fluid_synth_all_voices_pause(edge_fluid);

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
			epi::sound_data_c *buf = S_QueueGetFreeBuffer(FLUID_NUM_SAMPLES, 
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
		s16_t *data_buf;

		bool song_done = false;

		if (!dev_stereo)
			data_buf = mono_buffer;
		else
			data_buf = buf->data_L;

		int played = fluid_seq->playStream(reinterpret_cast<u8_t *>(data_buf), FLUID_NUM_SAMPLES);

		if (fluid_seq->positionAtEnd())
			song_done = true;

		buf->length = played / 4;

		if (!dev_stereo)
			ConvertToMono(buf->data_L, mono_buffer, buf->length);

		if (song_done)  /* EOF */
		{
			if (! looping)
				return false;
			fluid_seq->rewind();
			return true;
		}

    	return true;
	}
};

abstract_music_c * S_PlayFluid(byte *data, int length, float volume, bool loop)
{
	if (fluid_disabled)
	{
		delete[] data;
		return nullptr;
	}

	fluid_player_c *player = new fluid_player_c(data, length, loop);

	if (!player)
	{
		I_Debugf("FluidLite player: error initializing!\n");
		delete[] data;
		return nullptr;
	}

	if (!player->LoadTrack(data, length)) //Lobo: quietly log it instead of completely exiting EDGE
	{
		I_Debugf("FluidLite player: failed to load MIDI file!\n");
		delete[] data;
		delete player;
		return nullptr;
	}

	delete[] data;

	player->Volume(volume);
	player->Play(loop);

	return player;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
