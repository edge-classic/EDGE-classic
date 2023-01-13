//----------------------------------------------------------------------------
//  EDGE FMMIDI Music Player
//----------------------------------------------------------------------------
// 
//  Copyright (c) 2023  The EDGE Team.
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
#include "s_fmm.h"

#include "dm_state.h"

#define BW_MidiSequencer FMMSequencer
typedef struct BW_MidiRtInterface FMMInterface;
#include "midi_sequencer_impl.hpp"

#include "midisynth.hpp"

#define FMM_NUM_SAMPLES  4096

extern bool dev_stereo;
extern int  dev_freq; 

// Should only be invoked when switching MIDI players
void S_RestartFMM(void)
{
	int old_entry = entry_playing;

	S_StopMusic();

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

class fmm_player_c : public abstract_music_c
{
private:
	enum status_e
	{
		NOT_LOADED, PLAYING, PAUSED, STOPPED
	};
	
	int status;
	bool looping;

	FMMInterface *fmm_iface;

	s16_t *mono_buffer;

public:
	fmm_player_c(byte *_data, int _length, bool _looping) : status(NOT_LOADED), looping(_looping)
	{ 
		mono_buffer = new s16_t[FMM_NUM_SAMPLES * 2];
		SequencerInit(); 
	}

	~fmm_player_c()
	{
		Close();

		if (mono_buffer)
			delete[] mono_buffer;
	}

public:

	FMMSequencer *fmm_seq;
	midisynth::synthesizer *fmm;
	midisynth::fm_note_factory *fmm_note_factory;

	static void rtNoteOn(void *userdata, uint8_t channel, uint8_t note, uint8_t velocity)
	{
		fmm_player_c *player = (fmm_player_c *)userdata;
		player->fmm->note_on(channel, note, velocity);
	}

	static void rtNoteOff(void *userdata, uint8_t channel, uint8_t note)
	{
		fmm_player_c *player = (fmm_player_c *)userdata;
		player->fmm->note_off(channel, note, 0);
	}

	static void rtNoteAfterTouch(void *userdata, uint8_t channel, uint8_t note, uint8_t atVal)
	{
		fmm_player_c *player = (fmm_player_c *)userdata;
		player->fmm->polyphonic_key_pressure(channel, note, atVal);
	}

	static void rtChannelAfterTouch(void *userdata, uint8_t channel, uint8_t atVal)
	{
		fmm_player_c *player = (fmm_player_c *)userdata;
		player->fmm->channel_pressure(channel, atVal);
	}

	static void rtControllerChange(void *userdata, uint8_t channel, uint8_t type, uint8_t value)
	{
		fmm_player_c *player = (fmm_player_c *)userdata;
		player->fmm->control_change(channel, type, value);
	}

	static void rtPatchChange(void *userdata, uint8_t channel, uint8_t patch)
	{
		fmm_player_c *player = (fmm_player_c *)userdata;
		player->fmm->program_change(channel, patch);
	}

	static void rtPitchBend(void *userdata, uint8_t channel, uint8_t msb, uint8_t lsb)
	{
		fmm_player_c *player = (fmm_player_c *)userdata;
		player->fmm->pitch_bend_change(channel, (msb << 7) | lsb);
	}

	static void rtSysEx(void *userdata, const uint8_t *msg, size_t size)
	{
		fmm_player_c *player = (fmm_player_c *)userdata;
		player->fmm->sysex_message(msg, size);
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
		fmm_player_c *player = (fmm_player_c *)userdata;
		player->fmm->synthesize(reinterpret_cast<int_least16_t*>(stream), length / 4,
			dev_freq);
	}

	void SequencerInit()
	{
		fmm_seq = new FMMSequencer;
		fmm_iface = new FMMInterface;
		std::memset(fmm_iface, 0, sizeof(BW_MidiRtInterface));

		fmm_iface->rtUserData = this;
		fmm_iface->rt_noteOn  = rtNoteOn;
		fmm_iface->rt_noteOff = rtNoteOff;
		fmm_iface->rt_noteAfterTouch = rtNoteAfterTouch;
		fmm_iface->rt_channelAfterTouch = rtChannelAfterTouch;
		fmm_iface->rt_controllerChange = rtControllerChange;
		fmm_iface->rt_patchChange = rtPatchChange;
		fmm_iface->rt_pitchBend = rtPitchBend;
		fmm_iface->rt_systemExclusive = rtSysEx;

		fmm_iface->onPcmRender = playSynth;
		fmm_iface->onPcmRender_userData = this;

		fmm_iface->pcmSampleRate = dev_freq;
		fmm_iface->pcmFrameSize = 2 /*channels*/ * 2 /*size of one sample*/;

		fmm_iface->rt_deviceSwitch = rtDeviceSwitch;
		fmm_iface->rt_currentDevice = rtCurrentDevice;

		fmm_seq->setInterface(fmm_iface);
	}

	bool LoadTrack(const byte *data, int length)
	{
		return fmm_seq->loadMIDI(data, length);
	}

	void Close(void)
	{
		if (status == NOT_LOADED)
			return;

		// Stop playback
		if (status != STOPPED)
		  Stop();
	
		if (fmm_seq)
		{
			delete fmm_seq;
			fmm_seq = nullptr;
		}
		if (fmm_iface)
		{
			delete fmm_iface;
			fmm_iface = nullptr;
		}
		if (fmm_note_factory)
		{
			delete fmm_note_factory;
			fmm_note_factory = nullptr;
		}
		if (fmm)
		{
			delete fmm;
			fmm = nullptr;
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

		fmm->all_sound_off_immediately();

		S_QueueStop();

		status = STOPPED;
	}

	void Pause(void)
	{
		if (status != PLAYING)
			return;

		fmm->all_sound_off();

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
			epi::sound_data_c *buf = S_QueueGetFreeBuffer(FMM_NUM_SAMPLES, 
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

		int played = fmm_seq->playStream(reinterpret_cast<u8_t *>(data_buf), FMM_NUM_SAMPLES);

		if (fmm_seq->positionAtEnd())
			song_done = true;

		buf->length = played / 4;

		if (!dev_stereo)
			ConvertToMono(buf->data_L, mono_buffer, buf->length);

		if (song_done)  /* EOF */
		{
			if (! looping)
				return false;
			fmm_seq->rewind();
			return true;
		}

    	return true;
	}
};

abstract_music_c * S_PlayFMM(byte *data, int length, float volume, bool loop)
{

	fmm_player_c *player = new fmm_player_c(data, length, loop);

	if (!player)
	{
		I_Debugf("FMMIDI player: error initializing!\n");
		delete[] data;
		return nullptr;
	}

	player->fmm_note_factory = new midisynth::fm_note_factory;
	if (!player->fmm_note_factory)
	{
		I_Debugf("FMMIDI player: error initializing!\n");
		delete[] data;
		delete player;
		return nullptr;
	}

	player->fmm = new midisynth::synthesizer(player->fmm_note_factory);
	if (!player->fmm)
	{
		I_Debugf("FMMIDI player: error initializing!\n");
		delete[] data;
		delete player;
		return nullptr;
	}

	if (!player->LoadTrack(data, length)) //Lobo: quietly log it instead of completely exiting EDGE
	{
		I_Debugf("FMMIDI player: failed to load MIDI file!\n");
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
