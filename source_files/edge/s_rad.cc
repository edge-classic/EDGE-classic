//----------------------------------------------------------------------------
//  EDGE RAD Music Player
//----------------------------------------------------------------------------
//
//  Copyright (c) 2023 - The EDGE Team.
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

#include "playlist.h"

#include "s_cache.h"
#include "s_blit.h"
#include "s_music.h"
#include "w_wad.h"

#include "opal.h"
#include "radplay.h"

extern bool dev_stereo; // FIXME: encapsulation
extern int  dev_freq;

#define RAD_BLOCK_SIZE 1024

// Works better with the RAD code if these are 'global'
Opal      *edge_opal = nullptr;
RADPlayer *edge_rad  = nullptr;

class radplayer_c : public abstract_music_c
{
  public:
    radplayer_c();
    ~radplayer_c();

  private:
    enum status_e
    {
        NOT_LOADED,
        PLAYING,
        PAUSED,
        STOPPED
    };

    int  status;
    bool looping;

    s16_t *mono_buffer;

    int   samp_count;
    int   samp_update;
    int   samp_rate;
    byte *tune;

  public:
    bool OpenMemory(byte *data, int length);

    virtual void Close(void);

    virtual void Play(bool loop);
    virtual void Stop(void);

    virtual void Pause(void);
    virtual void Resume(void);

    virtual void Ticker(void);

  private:
    void PostOpenInit(void);

    bool StreamIntoBuffer(epi::sound_data_c *buf);
};

//----------------------------------------------------------------------------

radplayer_c::radplayer_c() : status(NOT_LOADED), tune(nullptr)
{
    mono_buffer = new s16_t[RAD_BLOCK_SIZE * 2];
}

radplayer_c::~radplayer_c()
{
    Close();

    if (mono_buffer)
        delete[] mono_buffer;
}

void radplayer_c::PostOpenInit()
{
    samp_count  = 0;
    samp_update = dev_freq / samp_rate;
    // Loaded, but not playing
    status = STOPPED;
}

static void ConvertToMono(s16_t *dest, const s16_t *src, int len)
{
    const s16_t *s_end = src + len * 2;

    for (; src < s_end; src += 2)
    {
        // compute average of samples
        *dest++ = ((int)src[0] + (int)src[1]) >> 1;
    }
}

bool radplayer_c::StreamIntoBuffer(epi::sound_data_c *buf)
{
    s16_t *data_buf;

    bool song_done = false;
    int  samples   = 0;

    if (!dev_stereo)
        data_buf = mono_buffer;
    else
        data_buf = buf->data_L;

    for (int i = 0; i < RAD_BLOCK_SIZE; i += 2)
    {
        edge_opal->Sample(data_buf + i, data_buf + i + 1);
        samples++;
        samp_count++;
        if (samp_count >= samp_update)
        {
            samp_count = 0;
            song_done  = edge_rad->Update();
        }
    }

    buf->length = samples;

    if (!dev_stereo)
        ConvertToMono(buf->data_L, mono_buffer, buf->length);

    if (song_done) /* EOF */
    {
        if (!looping)
            return false;
        return true;
    }

    return true;
}

bool radplayer_c::OpenMemory(byte *data, int length)
{
    SYS_ASSERT(data);

    const char *err = RADValidate(data, length);

    if (err)
    {
        I_Warning("RAD: Cannot play tune: %s\n", err);
        return false;
    }

    edge_opal = new Opal(dev_freq);
    edge_rad  = new RADPlayer;
    edge_rad->Init(
        data, [](void *arg, uint16_t reg_num, uint8_t val) { edge_opal->Port(reg_num, val); }, 0);

    samp_rate = edge_rad->GetHertz();

    if (samp_rate <= 0)
    {
        I_Warning("RAD: failure to load song!\n");
        delete edge_rad;
        edge_rad = nullptr;
        delete edge_opal;
        edge_opal = nullptr;
        return false;
    }

    // The player needs to free this afterwards
    tune = data;

    PostOpenInit();
    return true;
}

void radplayer_c::Close()
{
    if (status == NOT_LOADED)
        return;

    // Stop playback
    if (status != STOPPED)
        Stop();

    delete edge_rad;
    edge_rad = nullptr;
    delete edge_opal;
    edge_opal = nullptr;
    delete[] tune;

    status = NOT_LOADED;
}

void radplayer_c::Pause()
{
    if (status != PLAYING)
        return;

    status = PAUSED;
}

void radplayer_c::Resume()
{
    if (status != PAUSED)
        return;

    status = PLAYING;
}

void radplayer_c::Play(bool loop)
{
    if (status != NOT_LOADED && status != STOPPED)
        return;

    status  = PLAYING;
    looping = loop;

    // Load up initial buffer data
    Ticker();
}

void radplayer_c::Stop()
{
    if (status != PLAYING && status != PAUSED)
        return;

    S_QueueStop();

    edge_rad->Stop();

    status = STOPPED;
}

void radplayer_c::Ticker()
{
    while (status == PLAYING && !var_pc_speaker_mode)
    {
        epi::sound_data_c *buf =
            S_QueueGetFreeBuffer(RAD_BLOCK_SIZE, (dev_stereo) ? epi::SBUF_Interleaved : epi::SBUF_Mono);

        if (!buf)
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

abstract_music_c *S_PlayRADMusic(byte *data, int length, bool looping)
{
    radplayer_c *player = new radplayer_c();

    if (!player->OpenMemory(data, length))
    {
        delete[] data;
        delete player;
        return NULL;
    }

    player->Play(looping);

    return player;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
