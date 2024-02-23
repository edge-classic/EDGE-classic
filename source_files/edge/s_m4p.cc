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

#define M4P_BUFFER 1024

extern bool sound_device_stereo; // FIXME: encapsulation
extern int  sound_device_frequency;

class m4pplayer_c : public AbstractMusicPlayer
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

    int16_t *mono_buffer;

  public:
    bool OpenMemory(uint8_t *data, int length);

    virtual void Close(void);

    virtual void Play(bool loop);
    virtual void Stop(void);

    virtual void Pause(void);
    virtual void Resume(void);

    virtual void Ticker(void);

  private:
    void PostOpenInit(void);

    bool StreamIntoBuffer(sound_data_c *buf);
};

//----------------------------------------------------------------------------

m4pplayer_c::m4pplayer_c() : status(NOT_LOADED)
{
    mono_buffer = new int16_t[M4P_BUFFER * 2];
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

static void ConvertToMono(int16_t *dest, const int16_t *src, int len)
{
    const int16_t *s_end = src + len * 2;

    for (; src < s_end; src += 2)
    {
        // compute average of samples
        *dest++ = ((int)src[0] + (int)src[1]) >> 1;
    }
}

bool m4pplayer_c::StreamIntoBuffer(sound_data_c *buf)
{
    int16_t *data_buf;

    bool song_done = false;

    if (!sound_device_stereo)
        data_buf = mono_buffer;
    else
        data_buf = buf->data_L;

    m4p_GenerateSamples(data_buf, M4P_BUFFER / sizeof(int16_t));

    buf->length = M4P_BUFFER / 2;

    if (!sound_device_stereo)
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

bool m4pplayer_c::OpenMemory(uint8_t *data, int length)
{
    SYS_ASSERT(data);

    if (!m4p_LoadFromData(data, length, sound_device_frequency, M4P_BUFFER))
    {
        EDGEWarning("M4P: failure to load song!\n");
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
        sound_data_c *buf =
            S_QueueGetFreeBuffer(M4P_BUFFER, (sound_device_stereo) ? SBUF_Interleaved : SBUF_Mono);

        if (!buf)
            break;

        if (StreamIntoBuffer(buf))
        {
            if (buf->length > 0)
            {
                S_QueueAddBuffer(buf, sound_device_frequency);
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

AbstractMusicPlayer *S_PlayM4PMusic(uint8_t *data, int length, bool looping)
{
    m4pplayer_c *player = new m4pplayer_c();

    if (!player->OpenMemory(data, length))
    {
        delete[] data;
        delete player;
        return nullptr;
    }

    delete[] data;

    player->Play(looping);

    return player;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
