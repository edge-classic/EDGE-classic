//----------------------------------------------------------------------------
//  EDGE FLAC Music Player
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



#include "endianess.h"
#include "file.h"
#include "filesystem.h"
#include "sound_gather.h"

#include "playlist.h"

#include "s_cache.h"
#include "s_blit.h"
#include "s_music.h"
#include "s_flac.h"
#include "w_wad.h"

#define DR_FLAC_NO_CRC      1
#define DR_FLAC_NO_WIN32_IO 1
#define DR_FLAC_IMPLEMENTATION
#include "dr_flac.h"

#define FLAC_FRAMES 1024

extern bool sound_device_stereo; // FIXME: encapsulation
extern int  sound_device_frequency;

class flacplayer_c : public AbstractMusicPlayer
{
  public:
    flacplayer_c();
    ~flacplayer_c();

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

    drflac *flac_track; // I had to make it rhyme

    uint8_t *flac_data; // Passed in from s_music; must be deleted on close

    int16_t *mono_buffer;

  public:
    bool OpenMemory(uint8_t *data, int length);

    virtual void Close(void);

    virtual void Play(bool loop);
    virtual void Stop(void);

    virtual void Pause(void);
    virtual void Resume(void);

    virtual void Ticker(void);

    void PostOpenInit(void);

  private:
    bool StreamIntoBuffer(sound_data_c *buf);
};

//----------------------------------------------------------------------------

flacplayer_c::flacplayer_c() : status(NOT_LOADED)
{
    mono_buffer = new int16_t[FLAC_FRAMES * 2];
}

flacplayer_c::~flacplayer_c()
{
    Close();

    if (mono_buffer)
        delete[] mono_buffer;
}

void flacplayer_c::PostOpenInit()
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

bool flacplayer_c::StreamIntoBuffer(sound_data_c *buf)
{
    int16_t *data_buf;

    bool song_done = false;

    if (!sound_device_stereo)
        data_buf = mono_buffer;
    else
        data_buf = buf->data_L;

    drflac_uint64 frames = drflac_read_pcm_frames_s16(flac_track, FLAC_FRAMES, data_buf);

    if (frames < FLAC_FRAMES)
        song_done = true;

    buf->length = frames;

    buf->freq = flac_track->sampleRate;

    if (!sound_device_stereo)
        ConvertToMono(buf->data_L, mono_buffer, buf->length);

    if (song_done) /* EOF */
    {
        if (!looping)
            return false;
        drflac_seek_to_pcm_frame(flac_track, 0);
        return true;
    }

    return (true);
}

bool flacplayer_c::OpenMemory(uint8_t *data, int length)
{
    SYS_ASSERT(data);

    flac_track = drflac_open_memory(data, length, nullptr);

    if (!flac_track)
    {
        EDGEWarning("S_PlayFLACMusic: Error opening song!\n");
        return false;
    }

    // data is only released when the player is closed
    flac_data = data;

    PostOpenInit();
    return true;
}

void flacplayer_c::Close()
{
    if (status == NOT_LOADED)
        return;

    // Stop playback
    if (status != STOPPED)
        Stop();

    drflac_close(flac_track);
    delete[] flac_data;

    // reset player gain
    mus_player_gain = 1.0f;

    status = NOT_LOADED;
}

void flacplayer_c::Pause()
{
    if (status != PLAYING)
        return;

    status = PAUSED;
}

void flacplayer_c::Resume()
{
    if (status != PAUSED)
        return;

    status = PLAYING;
}

void flacplayer_c::Play(bool loop)
{
    if (status != NOT_LOADED && status != STOPPED)
        return;

    status  = PLAYING;
    looping = loop;

    // Set individual player type gain
    mus_player_gain = 0.6f;

    // Load up initial buffer data
    Ticker();
}

void flacplayer_c::Stop()
{
    if (status != PLAYING && status != PAUSED)
        return;

    S_QueueStop();

    status = STOPPED;
}

void flacplayer_c::Ticker()
{
    while (status == PLAYING && !var_pc_speaker_mode)
    {
        sound_data_c *buf =
            S_QueueGetFreeBuffer(FLAC_FRAMES, (sound_device_stereo) ? SBUF_Interleaved : SBUF_Mono);

        if (!buf)
            break;

        if (StreamIntoBuffer(buf))
        {
            if (buf->length > 0)
            {
                S_QueueAddBuffer(buf, buf->freq);
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

AbstractMusicPlayer *S_PlayFLACMusic(uint8_t *data, int length, bool looping)
{
    flacplayer_c *player = new flacplayer_c();

    if (!player->OpenMemory(data, length))
    {
        delete[] data;
        delete player;
        return nullptr;
    }

    // data is freed when Close() is called on the player; must be retained until then

    player->Play(looping);

    return player;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
