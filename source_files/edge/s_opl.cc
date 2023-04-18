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
#include "w_files.h"
#include "w_wad.h"

#include "dm_state.h"

#define BW_MidiSequencer OPLSequencer
typedef struct BW_MidiRtInterface OPLInterface;
#include "midi_sequencer_impl.hpp"

#include "ymf_player.h"

OPLPlayer *edge_opl = nullptr;

#define MID_BUFFER  4096

extern bool dev_stereo;
extern int  dev_freq;

bool opl_disabled = false;

DEF_CVAR(s_genmidi, "GENMIDI", CVAR_ARCHIVE)

extern std::vector<std::filesystem::path> available_genmidis;

bool S_StartupOPL(void)
{
	I_Printf("Initializing OPL player...\n");

	if (edge_opl) delete edge_opl;

	edge_opl = new OPLPlayer(dev_freq);

	if (!edge_opl) return false;

	// Check if CVAR value is still good
	bool cvar_good = false;
	if (s_genmidi.s == "GENMIDI")
		cvar_good = true;
	else
	{
		for (size_t i=0; i < available_genmidis.size(); i++)
		{
			if(epi::case_cmp(s_genmidi.s, available_genmidis.at(i).generic_u8string()) == 0)
				cvar_good = true;
		}
	}

	if (!cvar_good)
	{
		I_Warning("Cannot find previously used GENMIDI %s, falling back to default!\n", s_genmidi.c_str());
		s_genmidi = "GENMIDI";
	}

	int length;
	byte *data = nullptr;
	epi::file_c *F = nullptr;

	if (s_genmidi.s == "GENMIDI")
	{
		data = W_OpenPackOrLumpInMemory("GENMIDI", {".op2", ".wopl"}, &length);
		if (!data)
		{
			I_Debugf("no GENMIDI lump !\n");
			return false;
		}
	}
	else
	{
		F = epi::FS_Open(UTFSTR(s_genmidi.s), epi::file_c::ACCESS_READ | epi::file_c::ACCESS_BINARY);
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

	if (!edge_opl->loadPatches((const byte *)data, (size_t)length))
	{
		I_Warning("S_StartupOPL: Error loading instruments!\n");
		delete F;
		delete[] data;
		return false;
	}

	delete F;
	delete[] data;

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

static void ConvertToMono(s16_t *dest, const s16_t *src, int len)
{
	const s16_t *s_end = src + len*2;

	for (; src < s_end; src += 2)
	{
		// compute average of samples
		*dest++ = ( (int)src[0] + (int)src[1] ) >> 1;
	}
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

	s16_t *mono_buffer;

	OPLInterface *opl_iface;

public:
	opl_player_c(bool _looping) : status(NOT_LOADED), looping(_looping)
	{
		mono_buffer = new s16_t[2 * MID_BUFFER];
		SequencerInit();
	}

	~opl_player_c()
	{
		Close();

		if (mono_buffer)
			delete[] mono_buffer;
	}

public:

	OPLSequencer *opl_seq;

	static void rtNoteOn(void *userdata, uint8_t channel, uint8_t note, uint8_t velocity)
	{
		edge_opl->midiNoteOn(channel, note, velocity);
	}

	static void rtNoteOff(void *userdata, uint8_t channel, uint8_t note)
	{
		edge_opl->midiNoteOff(channel, note);
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
		edge_opl->midiControlChange(channel, type, value);
	}

	static void rtPatchChange(void *userdata, uint8_t channel, uint8_t patch)
	{
		edge_opl->midiProgramChange(channel, patch);
	}

	static void rtPitchBend(void *userdata, uint8_t channel, uint8_t msb, uint8_t lsb)
	{
		edge_opl->midiPitchControl(channel, (msb - 64) / 127.0);
	}

	static void rtSysEx(void *userdata, const uint8_t *msg, size_t size)
	{
		edge_opl->midiSysEx(msg, size);
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
		edge_opl->midiRawOPL(reg, value);
	}

	static void playSynth(void *userdata, uint8_t *stream, size_t length)
	{
		(void)userdata;
		edge_opl->generate(reinterpret_cast<int16_t *>(stream), length / (2 * sizeof(int16_t)));
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
		opl_iface->pcmFrameSize = 2 /*channels*/ * 2 /*size of one sample*/; // OPL3 is 2 'channels' regardless of the dev_stereo setting

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

		edge_opl->reset();

		S_QueueStop();

		status = STOPPED;
	}

	void Pause(void)
	{
		if (status != PLAYING)
			return;

		edge_opl->reset();

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
			epi::sound_data_c *buf = S_QueueGetFreeBuffer(MID_BUFFER,
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

		int played = opl_seq->playStream(reinterpret_cast<u8_t *>(data_buf), MID_BUFFER);

		if (opl_seq->positionAtEnd())
			song_done = true;

		buf->length = played / (2 * sizeof(s16_t));

		if (!dev_stereo)
			ConvertToMono(buf->data_L, mono_buffer, buf->length);

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

	// Ensure OPL register is set correctly for song type
	if (rate > 0)
		edge_opl->disableOPL3();
	else
		edge_opl->enableOPL3();

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
