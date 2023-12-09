//----------------------------------------------------------------------------
//  EDGE MOD4PLAY Music Player
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

#include "m4p.h"

#define M4P_BUFFER 512

extern bool dev_stereo; // FIXME: encapsulation
extern int  dev_freq;

class m4pplayer_c : public abstract_music_c
{
  public:
    m4pplayer_c();
    ~m4pplayer_c();

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

m4pplayer_c::m4pplayer_c() : status(NOT_LOADED)
{
    mono_buffer = new s16_t[M4P_BUFFER * 2];
}

m4pplayer_c::~m4pplayer_c()
{
    Close();

    if (mono_buffer)
        delete[] mono_buffer;
}

void m4pplayer_c::PostOpenInit()
{
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

bool m4pplayer_c::StreamIntoBuffer(epi::sound_data_c *buf)
{
    s16_t *data_buf;

    bool song_done = false;

    if (!dev_stereo)
        data_buf = mono_buffer;
    else
        data_buf = buf->data_L;

    m4p_GenerateSamples(data_buf, M4P_BUFFER / sizeof(s16_t));

    buf->length = M4P_BUFFER / 2;

    if (!dev_stereo)
        ConvertToMono(buf->data_L, mono_buffer, buf->length);

    if (song_done) /* EOF */
    {
        if (!looping)
            return false;
        m4p_Stop();
        m4p_PlaySong();
        return true;
    }

    return true;
}

bool m4pplayer_c::OpenMemory(byte *data, int length)
{
    SYS_ASSERT(data);

    if (!m4p_LoadFromData(data, length, dev_freq, M4P_BUFFER))
    {
        I_Warning("M4P: failure to load song!\n");
        return false;
    }

    PostOpenInit();
    return true;
}

void m4pplayer_c::Close()
{
    if (status == NOT_LOADED)
        return;

    // Stop playback
    if (status != STOPPED)
        Stop();

    m4p_Close();
    m4p_FreeSong();

    status = NOT_LOADED;
}

void m4pplayer_c::Pause()
{
    if (status != PLAYING)
        return;

    status = PAUSED;
}

void m4pplayer_c::Resume()
{
    if (status != PAUSED)
        return;

    status = PLAYING;
}

void m4pplayer_c::Play(bool loop)
{
    if (status != NOT_LOADED && status != STOPPED)
        return;

    status  = PLAYING;
    looping = loop;

    m4p_PlaySong();

    // Load up initial buffer data
    Ticker();
}

void m4pplayer_c::Stop()
{
    if (status != PLAYING && status != PAUSED)
        return;

    S_QueueStop();

    m4p_Stop();

    status = STOPPED;
}

void m4pplayer_c::Ticker()
{
    while (status == PLAYING && !var_pc_speaker_mode)
    {
        epi::sound_data_c *buf =
            S_QueueGetFreeBuffer(M4P_BUFFER, (dev_stereo) ? epi::SBUF_Interleaved : epi::SBUF_Mono);

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

abstract_music_c *S_PlayM4PMusic(byte *data, int length, bool looping)
{
    m4pplayer_c *player = new m4pplayer_c();

    if (!player->OpenMemory(data, length))
    {
        delete[] data;
        delete player;
        return NULL;
    }

    delete[] data;

    player->Play(looping);

    return player;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
