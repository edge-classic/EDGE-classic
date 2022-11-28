//----------------------------------------------------------------------------
//  EDGE TinySoundfont Music Player
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
#include "path.h"
#include "str_util.h"

#include "m_misc.h"
#include "s_blit.h"
#include "s_music.h"
#include "s_tsf.h"

#include "dm_state.h"

#define BW_MidiSequencer TSFSequencer
typedef struct BW_MidiRtInterface TSFInterface;
#include "midi_sequencer_impl.hpp"

#define TSF_IMPLEMENTATION
#include "tsf.h"

#define TSF_NUM_SAMPLES  4096

extern bool dev_stereo;
extern int  dev_freq; 

bool tsf_disabled = false;

tsf *edge_tsf;

DEF_CVAR(s_soundfont, "default.sf2", CVAR_ARCHIVE)

extern std::vector<std::string> available_soundfonts;

static void ConvertToMono(s16_t *dest, const s16_t *src, int len)
{
	const s16_t *s_end = src + len*2;

	for (; src < s_end; src += 2)
	{
		// compute average of samples
		*dest++ = ( (int)src[0] + (int)src[1] ) >> 1;
	}
}

bool S_StartupTSF(void)
{
	I_Printf("Initializing TinySoundFont...\n");

	// Check for presence of previous CVAR value's file
	bool cvar_good = false;
	for (int i=0; i < available_soundfonts.size(); i++)
	{
		if(epi::case_cmp(s_soundfont.s, available_soundfonts.at(i)) == 0)
			cvar_good = true;
	}

	if (!cvar_good)
	{
		I_Warning("Cannot find previously used soundfont %s, falling back to default!\n", s_soundfont.c_str());
		s_soundfont = "default.sf2";
	}

 	std::string soundfont_dir = epi::PATH_Join(game_dir.c_str(), "soundfont");

	edge_tsf = tsf_load_filename(epi::PATH_Join(soundfont_dir.c_str(), s_soundfont.c_str()).c_str());

	if (!edge_tsf)
	{
		I_Warning("TinySoundFont: Could not load requested soundfont %s! Falling back to default soundfont!\n", s_soundfont.c_str());
		edge_tsf = tsf_load_filename(epi::PATH_Join(soundfont_dir.c_str(), "default.sf2").c_str());
	}

	if (!edge_tsf) 
	{
		I_Warning("Could not load any soundfonts! Ensure that default.sf2 is present in the soundfont directory!\n");
		return false;
	}

	tsf_channel_set_bank_preset(edge_tsf, 9, 128, 0);

	// reduce the overall gain by 6dB, to minimize the chance of clipping in
	// songs with a lot of simultaneous notes.
	tsf_set_output(edge_tsf, TSF_STEREO_INTERLEAVED, dev_freq, -6.0);

	return true; // OK!
}

// Should only be invoked when switching soundfonts
void S_RestartTSF(void)
{
	if (tsf_disabled)
		return;

	I_Printf("Restarting TinySoundFont...\n");

	int old_entry = entry_playing;

	S_StopMusic();

	tsf_close(edge_tsf);

	if (!S_StartupTSF())
	{
		tsf_disabled = true;
		return;
	}

	S_ChangeMusic(old_entry, true); // Restart track that was playing when switched

	return; // OK!
}

class tsf_player_c : public abstract_music_c
{
private:
	enum status_e
	{
		NOT_LOADED, PLAYING, PAUSED, STOPPED
	};
	
	int status;
	bool looping;

	TSFInterface *tsf_iface;

	s16_t *mono_buffer;

public:
	tsf_player_c(byte *_data, int _length, bool _looping) : status(NOT_LOADED), looping(_looping)
	{ 
		mono_buffer = new s16_t[TSF_NUM_SAMPLES * 2];
		SequencerInit(); 
	}

	~tsf_player_c()
	{
		Close();

		if (mono_buffer)
			delete[] mono_buffer;
	}

public:

	TSFSequencer *tsf_seq;

	static void rtNoteOn(void *userdata, uint8_t channel, uint8_t note, uint8_t velocity)
	{
		tsf_channel_note_on(edge_tsf, channel, note, static_cast<float>(velocity) / 127.0f);
	}

	static void rtNoteOff(void *userdata, uint8_t channel, uint8_t note)
	{
		tsf_channel_note_off(edge_tsf, channel, note);
	}

	static void rtNoteAfterTouch(void *userdata, uint8_t channel, uint8_t note, uint8_t atVal)
	{
		(void)userdata; (void)channel; (void)note; (void)atVal;
	}

	static void rtChannelAfterTouch(void *userdata, uint8_t channel, uint8_t atVal)
	{
		(void)userdata; (void)channel; (void)atVal;
	}

	static void rtControllerChange(void *userdata, uint8_t channel, uint8_t type, uint8_t value)
	{
		tsf_channel_midi_control(edge_tsf, channel, type, value);
	}

	static void rtPatchChange(void *userdata, uint8_t channel, uint8_t patch)
	{
		tsf_channel_set_presetnumber(edge_tsf, channel, patch, channel == 9);
	}

	static void rtPitchBend(void *userdata, uint8_t channel, uint8_t msb, uint8_t lsb)
	{
		tsf_channel_set_pitchwheel(edge_tsf, channel, (msb << 7) | lsb);
	}

	static void rtSysEx(void *userdata, const uint8_t *msg, size_t size)
	{
		(void)userdata; (void)msg; (void)size;
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
		tsf_render_short(edge_tsf,
						reinterpret_cast<short*>(stream),
						static_cast<int>(length) / 4, 0);
	}

	void SequencerInit()
	{
		tsf_seq = new TSFSequencer;
		tsf_iface = new TSFInterface;
		std::memset(tsf_iface, 0, sizeof(BW_MidiRtInterface));

		tsf_iface->rtUserData = this;
		tsf_iface->rt_noteOn  = rtNoteOn;
		tsf_iface->rt_noteOff = rtNoteOff;
		tsf_iface->rt_noteAfterTouch = rtNoteAfterTouch;
		tsf_iface->rt_channelAfterTouch = rtChannelAfterTouch;
		tsf_iface->rt_controllerChange = rtControllerChange;
		tsf_iface->rt_patchChange = rtPatchChange;
		tsf_iface->rt_pitchBend = rtPitchBend;
		tsf_iface->rt_systemExclusive = rtSysEx;

		tsf_iface->onPcmRender = playSynth;
		tsf_iface->onPcmRender_userData = this;

		tsf_iface->pcmSampleRate = dev_freq;
		tsf_iface->pcmFrameSize = 2 /*channels*/ * 2 /*size of one sample*/;

		tsf_iface->rt_deviceSwitch = rtDeviceSwitch;
		tsf_iface->rt_currentDevice = rtCurrentDevice;

		tsf_seq->setInterface(tsf_iface);
	}

	bool LoadTrack(const byte *data, int length)
	{
		return tsf_seq->loadMIDI(data, length);
	}

	void Close(void)
	{
		if (status == NOT_LOADED)
			return;

		// Stop playback
		if (status != STOPPED)
		  Stop();
	
		tsf_reset(edge_tsf);

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

		tsf_note_off_all(edge_tsf);

		S_QueueStop();

		status = STOPPED;
	}

	void Pause(void)
	{
		if (status != PLAYING)
			return;

		tsf_note_off_all(edge_tsf);

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
			epi::sound_data_c *buf = S_QueueGetFreeBuffer(TSF_NUM_SAMPLES, 
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

		int played = tsf_seq->playStream(reinterpret_cast<u8_t *>(data_buf), TSF_NUM_SAMPLES);

		if (tsf_seq->positionAtEnd())
			song_done = true;

		buf->length = played / 4;

		if (!dev_stereo)
			ConvertToMono(buf->data_L, mono_buffer, buf->length);

		if (song_done)  /* EOF */
		{
			if (! looping)
				return false;
			tsf_seq->rewind();
			return true;
		}

    	return true;
	}
};

abstract_music_c * S_PlayTSF(byte *data, int length, int fmt,
			float volume, bool loop)
{
	if (tsf_disabled)
	{
		delete[] data;
		return nullptr;
	}

	tsf_player_c *player = new tsf_player_c(data, length, loop);

	if (!player)
	{
		I_Debugf("TinySoundfont player: error initializing!\n");
		delete[] data;
		return nullptr;
	}

	if (!player->LoadTrack(data, length)) //Lobo: quietly log it instead of completely exiting EDGE
	{
		I_Debugf("TinySoundfont player: failed to load MIDI file!\n");
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
