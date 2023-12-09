//----------------------------------------------------------------------------
//  EDGE OGG Music Player
//----------------------------------------------------------------------------
//
//  Copyright (c) 2004-2023  The EDGE Team.
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
//
// -ACB- 2004/08/18 Written:
//
// Based on a tutorial at DevMaster.net:
// http://www.devmaster.net/articles/openal-tutorials/lesson8.php
//

#include "i_defs.h"

#include "endianess.h"
#include "file.h"
#include "filesystem.h"
#include "sound_gather.h"

#include "playlist.h"

#include "s_cache.h"
#include "s_blit.h"
#include "s_music.h"
#include "s_ogg.h"
#include "w_wad.h"

#define OGGV_NUM_SAMPLES 512

#define OV_EXCLUDE_STATIC_CALLBACKS
#define OGG_IMPL
#define VORBIS_IMPL
#include "minivorbis.h"

extern bool dev_stereo; // FIXME: encapsulation

struct datalump_t
{
    const byte *data;

    size_t pos;
    size_t size;
};

class oggplayer_c : public abstract_music_c
{
  public:
    oggplayer_c();
    ~oggplayer_c();

  private:
    enum status_e
    {
        NOT_LOADED,
        PLAYING,
        PAUSED,
        STOPPED
    };

    int status;

    bool looping;
    bool is_stereo;

    datalump_t    *ogg_lump = nullptr;
    OggVorbis_File ogg_stream;
    vorbis_info   *vorbis_inf = nullptr;

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
    const char *GetError(int code);

    void PostOpenInit(void);

    bool StreamIntoBuffer(epi::sound_data_c *buf);
};

//----------------------------------------------------------------------------
//
// oggplayer memory operations
//

size_t oggplayer_memread(void *ptr, size_t size, size_t nmemb, void *datasource)
{
    datalump_t *d  = (datalump_t *)datasource;
    size_t      rb = size * nmemb;

    if (d->pos >= d->size)
        return 0;

    if (d->pos + rb > d->size)
        rb = d->size - d->pos;

    memcpy(ptr, d->data + d->pos, rb);
    d->pos += rb;

    return rb / size;
}

int oggplayer_memseek(void *datasource, ogg_int64_t offset, int whence)
{
    datalump_t *d = (datalump_t *)datasource;
    size_t      newpos;

    switch (whence)
    {
    case SEEK_SET: {
        newpos = (int)offset;
        break;
    }
    case SEEK_CUR: {
        newpos = d->pos + (int)offset;
        break;
    }
    case SEEK_END: {
        newpos = d->size + (int)offset;
        break;
    }
    default: {
        return -1;
    } // WTF?
    }

    if (newpos > d->size)
        return -1;

    d->pos = newpos;
    return 0;
}

int oggplayer_memclose(void *datasource)
{
    // we don't free the data here

    return 0;
}

long oggplayer_memtell(void *datasource)
{
    datalump_t *d = (datalump_t *)datasource;

    if (d->pos > d->size)
        return -1;

    return d->pos;
}

//----------------------------------------------------------------------------

oggplayer_c::oggplayer_c() : status(NOT_LOADED), vorbis_inf(NULL)
{
    mono_buffer = new s16_t[OGGV_NUM_SAMPLES * 2];
}

oggplayer_c::~oggplayer_c()
{
    Close();

    if (mono_buffer)
        delete[] mono_buffer;
}

const char *oggplayer_c::GetError(int code)
{
    switch (code)
    {
    case OV_EREAD:
        return ("Read from media error.");

    case OV_ENOTVORBIS:
        return ("Not Vorbis data.");

    case OV_EVERSION:
        return ("Vorbis version mismatch.");

    case OV_EBADHEADER:
        return ("Invalid Vorbis header.");

    case OV_EFAULT:
        return ("Internal error.");

    default:
        break;
    }

    return ("Unknown Ogg error.");
}

void oggplayer_c::PostOpenInit()
{
    vorbis_inf = ov_info(&ogg_stream, -1);
    SYS_ASSERT(vorbis_inf);

    if (vorbis_inf->channels == 1)
    {
        is_stereo = false;
    }
    else
    {
        is_stereo = true;
    }

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

bool oggplayer_c::StreamIntoBuffer(epi::sound_data_c *buf)
{
    int ogg_endian = (EPI_BYTEORDER == EPI_LIL_ENDIAN) ? 0 : 1;

    int samples = 0;

    while (samples < OGGV_NUM_SAMPLES)
    {
        s16_t *data_buf;

        if (is_stereo && !dev_stereo)
            data_buf = mono_buffer;
        else
            data_buf = buf->data_L + samples * (is_stereo ? 2 : 1);

        int section;
        int got_size =
            ov_read(&ogg_stream, (char *)data_buf, (OGGV_NUM_SAMPLES - samples) * (is_stereo ? 2 : 1) * sizeof(s16_t),
                    ogg_endian, sizeof(s16_t), 1 /* signed data */, &section);

        if (got_size == OV_HOLE) // ignore corruption
            continue;

        if (got_size == 0) /* EOF */
        {
            if (!looping)
                break;

            ov_raw_seek(&ogg_stream, 0);
            continue; // try again
        }

        if (got_size < 0) /* ERROR */
        {
            // Construct an error message
            std::string err_msg("[oggplayer_c::StreamIntoBuffer] Failed: ");

            err_msg += GetError(got_size);

            // FIXME: using I_Error is too harsh
            I_Error("%s", err_msg.c_str());
            return false; /* NOT REACHED */
        }

        got_size /= (is_stereo ? 2 : 1) * sizeof(s16_t);

        if (is_stereo && !dev_stereo)
            ConvertToMono(buf->data_L + samples, mono_buffer, got_size);

        samples += got_size;
    }

    return (samples > 0);
}

bool oggplayer_c::OpenMemory(byte *data, int length)
{
    if (status != NOT_LOADED)
        Close();

    ogg_lump = new datalump_t;

    ogg_lump->data = data;
    ogg_lump->size = length;
    ogg_lump->pos  = 0;

    ov_callbacks CB;

    CB.read_func  = oggplayer_memread;
    CB.seek_func  = oggplayer_memseek;
    CB.close_func = oggplayer_memclose;
    CB.tell_func  = oggplayer_memtell;

    int result = ov_open_callbacks((void *)ogg_lump, &ogg_stream, NULL, 0, CB);

    if (result < 0)
    {
        std::string err_msg("[oggplayer_c::OpenMemory] Failed: ");

        err_msg += GetError(result);
        I_Warning("%s\n", err_msg.c_str());
        ov_clear(&ogg_stream);
        ogg_lump->data = nullptr; // this is deleted after the function returns false
        delete ogg_lump;
        return false;
    }

    PostOpenInit();
    return true;
}

void oggplayer_c::Close()
{
    if (status == NOT_LOADED)
        return;

    // Stop playback
    Stop();

    ov_clear(&ogg_stream);

    delete[] ogg_lump->data;
    delete ogg_lump;
    ogg_lump = nullptr;

    // Reset player gain
    mus_player_gain = 1.0f;

    status = NOT_LOADED;
}

void oggplayer_c::Pause()
{
    if (status != PLAYING)
        return;

    status = PAUSED;
}

void oggplayer_c::Resume()
{
    if (status != PAUSED)
        return;

    status = PLAYING;
}

void oggplayer_c::Play(bool loop)
{
    if (status != NOT_LOADED && status != STOPPED)
        return;

    status  = PLAYING;
    looping = loop;

    // Set individual player gain
    mus_player_gain = 0.6f;

    // Load up initial buffer data
    Ticker();
}

void oggplayer_c::Stop()
{
    if (status != PLAYING && status != PAUSED)
        return;

    S_QueueStop();

    status = STOPPED;
}

void oggplayer_c::Ticker()
{
    while (status == PLAYING && !var_pc_speaker_mode)
    {
        epi::sound_data_c *buf =
            S_QueueGetFreeBuffer(OGGV_NUM_SAMPLES, (is_stereo && dev_stereo) ? epi::SBUF_Interleaved : epi::SBUF_Mono);

        if (!buf)
            break;

        if (StreamIntoBuffer(buf))
        {
            S_QueueAddBuffer(buf, vorbis_inf->rate);
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

abstract_music_c *S_PlayOGGMusic(byte *data, int length, bool looping)
{
    oggplayer_c *player = new oggplayer_c();

    if (!player->OpenMemory(data, length))
    {
        delete[] data;
        delete player;
        return NULL;
    }

    player->Play(looping);

    return player;
}

bool S_LoadOGGSound(epi::sound_data_c *buf, const byte *data, int length)
{
    datalump_t ogg_lump;

    ogg_lump.data = data;
    ogg_lump.size = length;
    ogg_lump.pos  = 0;

    ov_callbacks CB;

    CB.read_func  = oggplayer_memread;
    CB.seek_func  = oggplayer_memseek;
    CB.close_func = oggplayer_memclose;
    CB.tell_func  = oggplayer_memtell;

    OggVorbis_File ogg_stream;

    int result = ov_open_callbacks((void *)&ogg_lump, &ogg_stream, NULL, 0, CB);

    if (result < 0)
    {
        I_Warning("Failed to load OGG sound (corrupt ogg?) error=%d\n", result);

        return false;
    }

    vorbis_info *vorbis_inf = ov_info(&ogg_stream, -1);
    SYS_ASSERT(vorbis_inf);

    I_Debugf("OGG SFX Loader: freq %d Hz, %d channels\n", (int)vorbis_inf->rate, (int)vorbis_inf->channels);

    if (vorbis_inf->channels > 2)
    {
        I_Warning("OGG Sfx Loader: too many channels: %d\n", vorbis_inf->channels);

        ogg_lump.size = 0;
        ov_clear(&ogg_stream);

        return false;
    }

    bool is_stereo  = (vorbis_inf->channels > 1);
    int  ogg_endian = (EPI_BYTEORDER == EPI_LIL_ENDIAN) ? 0 : 1;

    buf->freq = vorbis_inf->rate;

    epi::sound_gather_c gather;

    for (;;)
    {
        int want = 2048;

        s16_t *buffer = gather.MakeChunk(want, is_stereo);

        int section;
        int got_size = ov_read(&ogg_stream, (char *)buffer, want * (is_stereo ? 2 : 1) * sizeof(s16_t), ogg_endian,
                               sizeof(s16_t), 1 /* signed data */, &section);

        if (got_size == OV_HOLE) // ignore corruption
        {
            gather.DiscardChunk();
            continue;
        }

        if (got_size == 0) /* EOF */
        {
            gather.DiscardChunk();
            break;
        }
        else if (got_size < 0) /* ERROR */
        {
            gather.DiscardChunk();

            I_Warning("Problem occurred while loading OGG (%d)\n", got_size);
            break;
        }

        got_size /= (is_stereo ? 2 : 1) * sizeof(s16_t);

        gather.CommitChunk(got_size);
    }

    if (!gather.Finalise(buf, is_stereo))
        I_Error("OGG SFX Loader: no samples!\n");

    ov_clear(&ogg_stream);

    // free the data
    delete[] data;

    return true;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
