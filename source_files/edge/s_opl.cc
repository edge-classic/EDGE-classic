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
#include "sound_types.h"
#include "path.h"
#include "str_util.h"
#include "playlist.h"

#include "m_misc.h"
#include "s_blit.h"
#include "s_music.h"
#include "s_opl.h"
#include "w_wad.h"

#include "dm_state.h"

#define BW_MidiSequencer OPLSequencer
typedef struct BW_MidiRtInterface OPLInterface;
#include "midi_sequencer_impl.hpp"

// these are in the opl/ directory
#include "genmidi.h"
#include "opl_player.h"

#define NUM_SAMPLES  4096

extern bool dev_stereo;
extern int  dev_freq;

bool opl_disabled = false;

DEF_CVAR(s_genmidi, "", CVAR_ARCHIVE)

extern std::vector<std::filesystem::path> available_genmidis;

bool S_StartupOPL(void)
{
	I_Debugf("Initializing OPL player...\n");

	if (! OPLAY_Init(dev_freq, dev_stereo, var_opl_music))
	{
		return false;
	}

	// Check if CVAR value is still good
	bool cvar_good = false;
	if (s_genmidi.s.empty())
		cvar_good = true;
	else
	{
		for (size_t i=0; i < available_genmidis.size(); i++)
		{
			if(epi::case_cmp(s_genmidi.s, available_genmidis.at(i).u8string()) == 0)
				cvar_good = true;
		}
	}

	if (!cvar_good)
	{
		I_Warning("Cannot find previously used GENMIDI %s, falling back to default!\n", s_genmidi.c_str());
		s_genmidi.s = "";
	}

	int length;
	byte *data = nullptr;
	epi::file_c *F = nullptr;

	if (s_genmidi.s.empty())
	{
		int p = W_CheckNumForName("GENMIDI");
		if (p < 0)
		{
			I_Debugf("no GENMIDI lump !\n");
			return false;
		}
		data = W_LoadLump(p, &length);
	}
	else
	{
		std::filesystem::path soundfont_dir = epi::PATH_Join(game_dir, UTFSTR("soundfont"));

		F = epi::FS_Open(epi::PATH_Join(soundfont_dir, UTFSTR(s_genmidi.s)), epi::file_c::ACCESS_READ | epi::file_c::ACCESS_BINARY);
		if (! F)
		{
			I_Warning("S_StartupOPL: Error opening GENMIDI!\n");
			return false;
		}
		length = F->GetLength();
		data = F->LoadIntoMemory();
	}

	if (!data)
	{
		I_Warning("S_StartupOPL: Error loading instruments!\n");
		if (F)
			delete F;
		return false;
	}

	if (!GM_LoadInstruments((const byte *)data, (size_t)length))
	{
		I_Warning("S_StartupOPL: Error loading instruments!\n");
		if (s_genmidi.s.empty())
			W_DoneWithLump(data);
		else
		{
			delete F;
			delete data;
		}
		return false;
	}

	if (s_genmidi.s.empty())
		W_DoneWithLump(data);
	else
	{
		delete F;
		delete data;
	}

	// OK
	return true;
}

// Should only be invoked when switching GENMIDI lumps
void S_RestartOPL(void)
{
	if (opl_disabled)
		return;

	int old_entry = entry_playing;

	S_StopMusic();

	if (!S_StartupOPL())
	{
		opl_disabled = true;
		return;
	}

	S_ChangeMusic(old_entry, true); // Restart track that was playing when switched

	return; // OK!
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

	OPLInterface *opl_iface;

public:
	opl_player_c(bool _looping) : status(NOT_LOADED), looping(_looping)
	{
		SequencerInit();
	}

	~opl_player_c()
	{
		Close();
	}

public:

	OPLSequencer *opl_seq;

	static void rtNoteOn(void *userdata, uint8_t channel, uint8_t note, uint8_t velocity)
	{
		OPLAY_KeyOn(channel, note, velocity);
	}

	static void rtNoteOff(void *userdata, uint8_t channel, uint8_t note)
	{
		OPLAY_KeyOff(channel, note);
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
		OPLAY_ControllerChange(channel, type, value);
	}

	static void rtPatchChange(void *userdata, uint8_t channel, uint8_t patch)
	{
		OPLAY_ProgramChange(channel, patch);
	}

	static void rtPitchBend(void *userdata, uint8_t channel, uint8_t msb, uint8_t lsb)
	{
		OPLAY_PitchBend(channel, msb);
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

	static void rtRawOPL(void *userdata, uint8_t reg, uint8_t value)
	{
		OPLAY_WriteReg(reg, value);
	}

	static void playSynth(void *userdata, uint8_t *stream, size_t length)
	{
		(void)userdata;
		OPLAY_Stream(reinterpret_cast<short*>(stream),
						static_cast<int>(length) / (dev_stereo ? 4 : 2), dev_stereo);
	}

	void SequencerInit()
	{
		opl_seq = new OPLSequencer;
		opl_iface = new OPLInterface;
		std::memset(opl_iface, 0, sizeof(BW_MidiRtInterface));

		opl_iface->rtUserData = this;
		opl_iface->rt_noteOn  = rtNoteOn;
		opl_iface->rt_noteOff = rtNoteOff;
		opl_iface->rt_noteAfterTouch = rtNoteAfterTouch;
		opl_iface->rt_channelAfterTouch = rtChannelAfterTouch;
		opl_iface->rt_controllerChange = rtControllerChange;
		opl_iface->rt_patchChange = rtPatchChange;
		opl_iface->rt_pitchBend = rtPitchBend;
		opl_iface->rt_systemExclusive = rtSysEx;

		opl_iface->onPcmRender = playSynth;
		opl_iface->onPcmRender_userData = this;

		opl_iface->pcmSampleRate = dev_freq;
		opl_iface->pcmFrameSize = (dev_stereo ? 2 : 1) /*channels*/ * 2 /*size of one sample*/;

		opl_iface->rt_deviceSwitch = rtDeviceSwitch;
		opl_iface->rt_currentDevice = rtCurrentDevice;
		opl_iface->rt_rawOPL = rtRawOPL;

		opl_seq->setInterface(opl_iface);
	}

	bool LoadTrack(const byte *data, int length, uint16_t rate)
	{
		return opl_seq->loadMIDI(data, length, rate);
	}		

	void Close(void)
	{
		if (status == NOT_LOADED)
			return;

		// Stop playback
		if (status != STOPPED)
		  Stop();

		if (opl_seq)
		{
			delete opl_seq;
			opl_seq = nullptr;
		}
		if (opl_iface)
		{
			delete opl_iface;
			opl_iface = nullptr;
		}

		status = NOT_LOADED;
	}

	void Play(bool loop)
	{
		if (! (status == NOT_LOADED || status == STOPPED))
			return;

		status  = PLAYING;
		looping = loop;

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
		bool song_done = false;

		int played = opl_seq->playStream(reinterpret_cast<u8_t *>(buf->data_L), NUM_SAMPLES);

		if (opl_seq->positionAtEnd())
			song_done = true;

		buf->length = played / (dev_stereo ? 4 : 2);

		if (song_done)  /* EOF */
		{
			if (! looping)
				return false;
			opl_seq->rewind();
			return true;
		}

    	return true;
	}
};

abstract_music_c * S_PlayOPL(byte *data, int length, float volume, bool loop, int type)
{

	if (opl_disabled)
	{
		delete[] data;
		return nullptr;
	}

	opl_player_c *player = new opl_player_c(loop);

	if (!player)
	{
		I_Debugf("OPL player: error initializing!\n");
		delete[] data;
		return nullptr;
	}

	uint16_t rate;

	switch (type)
	{
		case MUS_IMF280:
			rate = 280;
			break;
		case MUS_IMF560:
			rate = 560;
			break;
		case MUS_IMF700:
			rate = 700;
			break;
		default:
			rate = 0;
			break;
	}

	if (!player->LoadTrack(data, length, rate)) //Lobo: quietly log it instead of completely exiting EDGE
	{
		I_Debugf("OPL player: failed to load MIDI file!\n");
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
