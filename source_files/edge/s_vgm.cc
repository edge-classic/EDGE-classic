//----------------------------------------------------------------------------
//  EDGE VGM Music Player
//----------------------------------------------------------------------------
// 
//  Copyright (c) 2022-2023 - The EDGE Team.
// 
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
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
#include "mini_gzip.h"

#include "playlist.h"

#include "s_cache.h"
#include "s_blit.h"
#include "s_music.h"
#include "s_vgm.h"
#include "w_wad.h"

// libvgm
#include "player/vgmplayer.hpp"
#include "player/playera.hpp"
#include "util/MemoryLoader.h"

#define VGM_BUFFER 1024

extern bool dev_stereo;  // FIXME: encapsulation
extern int  dev_freq;

class vgmplayer_c : public abstract_music_c
{
public:
	 vgmplayer_c();
	~vgmplayer_c();
private:

	enum status_e
	{
		NOT_LOADED, PLAYING, PAUSED, STOPPED
	};
	
	int status;
	bool looping;

	s16_t *mono_buffer;

	PlayerA *vgm_player = nullptr;
	PlayerBase *vgm_engine = nullptr;
	DATA_LOADER *vgm_loader = nullptr;

public:

	bool OpenMemory(byte *data, int length);

	virtual void Close(void);

	virtual void Play(bool loop);
	virtual void Stop(void);

	virtual void Pause(void);
	virtual void Resume(void);

	virtual void Ticker(void);

	void PostOpenInit(void);

private:

	bool StreamIntoBuffer(epi::sound_data_c *buf);
	
};

//----------------------------------------------------------------------------

vgmplayer_c::vgmplayer_c() : status(NOT_LOADED)
{
	mono_buffer = new s16_t[VGM_BUFFER * 2];
}

vgmplayer_c::~vgmplayer_c()
{
	Close();

	if (mono_buffer)
		delete[] mono_buffer;
}

void vgmplayer_c::PostOpenInit()
{   
	// Despite name, does not actually start playing
	vgm_player->Start();
	VGMPlayer* vgmplay = dynamic_cast<VGMPlayer*>(vgm_engine);
    vgm_player->SetLoopCount(vgmplay->GetModifiedLoopCount(2));
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

bool vgmplayer_c::StreamIntoBuffer(epi::sound_data_c *buf)
{
	s16_t *data_buf;

	if (!dev_stereo)
		data_buf = mono_buffer;
	else
		data_buf = buf->data_L;

	uint32_t samples = vgm_player->Render(VGM_BUFFER * 4, data_buf);

	buf->length = samples / 4;

	if (!dev_stereo)
		ConvertToMono(buf->data_L, mono_buffer, buf->length);

	if (samples <= 0)  // EOF ?
	{
		if (! looping)
			return false;
		vgm_player->Reset();
		return true;
	}

    return (true);
}

bool vgmplayer_c::OpenMemory(byte *data, int length)
{
	SYS_ASSERT(data);

	std::vector<uint8_t> vgm_buffer(length);
	std::copy(data, data+length, vgm_buffer.data());

	// Decompress if VGZ
	if (vgm_buffer.size() >= 10 && vgm_buffer[0] == 0x1f && vgm_buffer[1] == 0x8b)
	{
		// copy the raw data to a new buffer
		std::vector<uint8_t> compressed = vgm_buffer;

		// determine uncompressed size and resize the buffer
		uint8_t *end = &compressed[compressed.size()-1];
		uint32_t uncompressed = end[-3] | (end[-2] << 8) | (end[-1] << 16) | (end[0] << 24);
		if (length < compressed.size() || length > 32*1024*1024)
		{
			I_Debugf("[vgmplayer_c::S_PlayVGMMusic] Failed to load VGZ file with odd size!\n");
			return false;
		}
		vgm_buffer.resize(uncompressed);

		// decompress the data
		mini_gzip *vgz = new mini_gzip;
		mini_gz_init(vgz);
		if (mini_gz_start(vgz, &compressed[0], compressed.size()) != 0)
		{
			I_Debugf("[vgmplayer_c::S_PlayVGMMusic] Error decompressing VGZ!\n");
			delete vgz;
			return false;
		}

		if (mini_gz_unpack(vgz, &vgm_buffer[0], uncompressed) < 0)
		{
			I_Debugf("[vgmplayer_c::S_PlayVGMMusic] Failed decompressing VGZ file!\n");
			delete vgz;
			return false;
		}

		delete vgz;
	}

	vgm_player = new PlayerA;

	vgm_engine = new VGMPlayer;

	vgm_player->RegisterPlayerEngine(vgm_engine);

	vgm_player->SetOutputSettings(dev_freq, 2, 16, VGM_BUFFER);

	vgm_loader = MemoryLoader_Init(vgm_buffer.data(), vgm_buffer.size());

	if (!vgm_loader)
	{
		I_Debugf("[vgmplayer_c::S_PlayVGMMusic] Failed to init loader!\n");
		delete vgm_player;
		return false;
	}

	// initial sanity check
	DataLoader_SetPreloadBytes(vgm_loader,0x100);
	if (DataLoader_Load(vgm_loader))
	{
		I_Debugf("[vgmplayer_c::S_PlayVGMMusic] Failed to init loader!\n");
		DataLoader_Deinit(vgm_loader);
		delete vgm_player;
		return false;
	}

	if (vgm_player->LoadFile(vgm_loader))
	{
		I_Debugf("[vgmplayer_c::S_PlayVGMMusic] Failed to load track!\n");
		DataLoader_Deinit(vgm_loader);
		delete vgm_player;
		return false;
	}

	PostOpenInit();
	return true;
}

void vgmplayer_c::Close()
{
	if (status == NOT_LOADED)
		return;

	// Stop playback
	if (status != STOPPED)
		Stop();
		
	DataLoader_Deinit(vgm_loader);
	delete vgm_player; //

	status = NOT_LOADED;
}


void vgmplayer_c::Pause()
{
	if (status != PLAYING)
		return;

	status = PAUSED;
}


void vgmplayer_c::Resume()
{
	if (status != PAUSED)
		return;

	status = PLAYING;
}


void vgmplayer_c::Play(bool loop)
{
    if (status != NOT_LOADED && status != STOPPED) return;

	status = PLAYING;
	looping = loop;

	// Set individual player type gain
	mus_player_gain = 0.4f;

	// Load up initial buffer data
	Ticker();
}


void vgmplayer_c::Stop()
{
	if (status != PLAYING && status != PAUSED)
		return;

	vgm_player->Stop();

	S_QueueStop();

	// Reset gain
	mus_player_gain = 1.0f;

	status = STOPPED;
}


void vgmplayer_c::Ticker()
{
	while (status == PLAYING && !var_pc_speaker_mode)
	{
		epi::sound_data_c *buf = S_QueueGetFreeBuffer(VGM_BUFFER, 
				(dev_stereo) ? epi::SBUF_Interleaved : epi::SBUF_Mono);

		if (! buf)
			break;

		if (StreamIntoBuffer(buf))
		{
			if (buf->length > 0)
			{
				S_QueueAddBuffer(buf, dev_freq);
			}
			else
			{
				S_QueueReturnBuffer(buf);
			}
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

abstract_music_c * S_PlayVGMMusic(byte *data, int length, bool looping)
{
	vgmplayer_c *player = new vgmplayer_c();

	if (! player->OpenMemory(data, length))
	{
		delete[] data;
		delete player;
		return nullptr;
	}

	// we retain a copy of the VGM data, so can free it here
	delete[] data;

	player->Play(looping);

	return player;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
