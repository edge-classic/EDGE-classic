//----------------------------------------------------------------------------
//  EDGE OGG Music Player
//----------------------------------------------------------------------------
//
//  Copyright (c) 2004-2024 The EDGE Team.
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

#include "s_ogg.h"

#include "endianess.h"
#include "epi.h"
#include "file.h"
#include "filesystem.h"
// clang-format off
#define OV_EXCLUDE_STATIC_CALLBACKS
#define OGG_IMPL
#define VORBIS_IMPL
#include "minivorbis.h"
// clang-format on
#include "playlist.h"
#include "s_blit.h"
#include "s_cache.h"
#include "s_music.h"
#include "snd_gather.h"
#include "w_wad.h"

#define OGGV_NUM_SAMPLES 1024

extern bool sound_device_stereo;  // FIXME: encapsulation

struct OggDataLump
{
    const uint8_t *data;
    size_t         position;
    size_t         size;
};

class OggPlayer : public AbstractMusicPlayer
{
   public:
    OggPlayer();
    ~OggPlayer();

   private:
    enum Status
    {
        kNotLoaded,
        kPlaying,
        kPaused,
        kStopped
    };

    int status_;

    bool looping_;
    bool is_stereo_;

    OggDataLump   *ogg_lump_ = nullptr;
    OggVorbis_File ogg_stream_;
    vorbis_info   *vorbis_info_ = nullptr;

    int16_t *mono_buffer_;

   public:
    bool OpenMemory(uint8_t *data, int length);

    virtual void Close(void);

    virtual void Play(bool loop);
    virtual void Stop(void);

    virtual void Pause(void);
    virtual void Resume(void);

    virtual void Ticker(void);

   private:
    const char *GetError(int code);

    void PostOpen(void);

    bool StreamIntoBuffer(SoundData *buf);
};

//----------------------------------------------------------------------------
//
// oggplayer memory operations
//

size_t oggplayer_memread(void *ptr, size_t size, size_t nmemb, void *datasource)
{
    OggDataLump *d  = (OggDataLump *)datasource;
    size_t       rb = size * nmemb;

    if (d->position >= d->size) return 0;

    if (d->position + rb > d->size) rb = d->size - d->position;

    memcpy(ptr, d->data + d->position, rb);
    d->position += rb;

    return rb / size;
}

int oggplayer_memseek(void *datasource, ogg_int64_t offset, int whence)
{
    OggDataLump *d = (OggDataLump *)datasource;
    size_t       newpos;

    switch (whence)
    {
        case SEEK_SET:
        {
            newpos = (int)offset;
            break;
        }
        case SEEK_CUR:
        {
            newpos = d->position + (int)offset;
            break;
        }
        case SEEK_END:
        {
            newpos = d->size + (int)offset;
            break;
        }
        default:
        {
            return -1;
        }  // WTF?
    }

    if (newpos > d->size) return -1;

    d->position = newpos;
    return 0;
}

int oggplayer_memclose(void *datasource)
{
    // we don't free the data here

    return 0;
}

long oggplayer_memtell(void *datasource)
{
    OggDataLump *d = (OggDataLump *)datasource;

    if (d->position > d->size) return -1;

    return d->position;
}

//----------------------------------------------------------------------------

OggPlayer::OggPlayer() : status_(kNotLoaded), vorbis_info_(nullptr)
{
    mono_buffer_ = new int16_t[OGGV_NUM_SAMPLES * 2];
}

OggPlayer::~OggPlayer()
{
    Close();

    if (mono_buffer_) delete[] mono_buffer_;
}

const char *OggPlayer::GetError(int code)
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

void OggPlayer::PostOpen()
{
    vorbis_info_ = ov_info(&ogg_stream_, -1);
    EPI_ASSERT(vorbis_info_);

    if (vorbis_info_->channels == 1) { is_stereo_ = false; }
    else { is_stereo_ = true; }

    // Loaded, but not playing
    status_ = kStopped;
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

bool OggPlayer::StreamIntoBuffer(SoundData *buf)
{
    int ogg_endian = (kByteOrder == kLittleEndian) ? 0 : 1;

    int samples = 0;

    while (samples < OGGV_NUM_SAMPLES)
    {
        int16_t *data_buf;

        if (is_stereo_ && !sound_device_stereo)
            data_buf = mono_buffer_;
        else
            data_buf = buf->data_left_ + samples * (is_stereo_ ? 2 : 1);

        int section;
        int got_size =
            ov_read(&ogg_stream_, (char *)data_buf,
                    (OGGV_NUM_SAMPLES - samples) * (is_stereo_ ? 2 : 1) *
                        sizeof(int16_t),
                    ogg_endian, sizeof(int16_t), 1 /* signed data */, &section);

        if (got_size == OV_HOLE)  // ignore corruption
            continue;

        if (got_size == 0) /* EOF */
        {
            if (!looping_) break;

            ov_raw_seek(&ogg_stream_, 0);
            continue;  // try again
        }

        if (got_size < 0) /* ERROR */
        {
            // Construct an error message
            std::string err_msg("[oggplayer_c::StreamIntoBuffer] Failed: ");

            err_msg += GetError(got_size);

            // FIXME: using FatalError is too harsh
            FatalError("%s", err_msg.c_str());
            return false; /* NOT REACHED */
        }

        got_size /= (is_stereo_ ? 2 : 1) * sizeof(int16_t);

        if (is_stereo_ && !sound_device_stereo)
            ConvertToMono(buf->data_left_ + samples, mono_buffer_, got_size);

        samples += got_size;
    }

    return (samples > 0);
}

bool OggPlayer::OpenMemory(uint8_t *data, int length)
{
    if (status_ != kNotLoaded) Close();

    ogg_lump_ = new OggDataLump;

    ogg_lump_->data     = data;
    ogg_lump_->size     = length;
    ogg_lump_->position = 0;

    ov_callbacks CB;

    CB.read_func  = oggplayer_memread;
    CB.seek_func  = oggplayer_memseek;
    CB.close_func = oggplayer_memclose;
    CB.tell_func  = oggplayer_memtell;

    int result =
        ov_open_callbacks((void *)ogg_lump_, &ogg_stream_, nullptr, 0, CB);

    if (result < 0)
    {
        std::string err_msg("[oggplayer_c::OpenMemory] Failed: ");

        err_msg += GetError(result);
        LogWarning("%s\n", err_msg.c_str());
        ov_clear(&ogg_stream_);
        ogg_lump_->data =
            nullptr;  // this is deleted after the function returns false
        delete ogg_lump_;
        return false;
    }

    PostOpen();
    return true;
}

void OggPlayer::Close()
{
    if (status_ == kNotLoaded) return;

    // Stop playback
    Stop();

    ov_clear(&ogg_stream_);

    delete[] ogg_lump_->data;
    delete ogg_lump_;
    ogg_lump_ = nullptr;

    // Reset player gain
    music_player_gain = 1.0f;

    status_ = kNotLoaded;
}

void OggPlayer::Pause()
{
    if (status_ != kPlaying) return;

    status_ = kPaused;
}

void OggPlayer::Resume()
{
    if (status_ != kPaused) return;

    status_ = kPlaying;
}

void OggPlayer::Play(bool loop)
{
    if (status_ != kNotLoaded && status_ != kStopped) return;

    status_  = kPlaying;
    looping_ = loop;

    // Set individual player gain
    music_player_gain = 0.6f;

    // Load up initial buffer data
    Ticker();
}

void OggPlayer::Stop()
{
    if (status_ != kPlaying && status_ != kPaused) return;

    SoundQueueStop();

    status_ = kStopped;
}

void OggPlayer::Ticker()
{
    while (status_ == kPlaying && !pc_speaker_mode)
    {
        SoundData *buf = SoundQueueGetFreeBuffer(
            OGGV_NUM_SAMPLES,
            (is_stereo_ && sound_device_stereo) ? kMixInterleaved : kMixMono);

        if (!buf) break;

        if (StreamIntoBuffer(buf))
        {
            SoundQueueAddBuffer(buf, vorbis_info_->rate);
        }
        else
        {
            // finished playing
            SoundQueueReturnBuffer(buf);
            Stop();
        }
    }
}

//----------------------------------------------------------------------------

AbstractMusicPlayer *PlayOggMusic(uint8_t *data, int length, bool looping)
{
    OggPlayer *player = new OggPlayer();

    if (!player->OpenMemory(data, length))
    {
        delete[] data;
        delete player;
        return nullptr;
    }

    player->Play(looping);

    return player;
}

bool LoadOggSound(SoundData *buf, const uint8_t *data, int length)
{
    OggDataLump ogg_lump;

    ogg_lump.data     = data;
    ogg_lump.size     = length;
    ogg_lump.position = 0;

    ov_callbacks CB;

    CB.read_func  = oggplayer_memread;
    CB.seek_func  = oggplayer_memseek;
    CB.close_func = oggplayer_memclose;
    CB.tell_func  = oggplayer_memtell;

    OggVorbis_File ogg_stream;

    int result =
        ov_open_callbacks((void *)&ogg_lump, &ogg_stream, nullptr, 0, CB);

    if (result < 0)
    {
        LogWarning("Failed to load OGG sound (corrupt ogg?) error=%d\n",
                   result);

        return false;
    }

    vorbis_info *vorbis_inf = ov_info(&ogg_stream, -1);
    EPI_ASSERT(vorbis_inf);

    LogDebug("OGG SFX Loader: freq %d Hz, %d channels\n", (int)vorbis_inf->rate,
             (int)vorbis_inf->channels);

    if (vorbis_inf->channels > 2)
    {
        LogWarning("OGG Sfx Loader: too many channels: %d\n",
                   vorbis_inf->channels);

        ogg_lump.size = 0;
        ov_clear(&ogg_stream);

        return false;
    }

    bool is_stereo  = (vorbis_inf->channels > 1);
    int  ogg_endian = (kByteOrder == kLittleEndian) ? 0 : 1;

    buf->frequency_ = vorbis_inf->rate;

    SoundGatherer gather;

    for (;;)
    {
        int want = 2048;

        int16_t *buffer = gather.MakeChunk(want, is_stereo);

        int section;
        int got_size =
            ov_read(&ogg_stream, (char *)buffer,
                    want * (is_stereo ? 2 : 1) * sizeof(int16_t), ogg_endian,
                    sizeof(int16_t), 1 /* signed data */, &section);

        if (got_size == OV_HOLE)  // ignore corruption
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

            LogWarning("Problem occurred while loading OGG (%d)\n", got_size);
            break;
        }

        got_size /= (is_stereo ? 2 : 1) * sizeof(int16_t);

        gather.CommitChunk(got_size);
    }

    if (!gather.Finalise(buf, is_stereo))
        FatalError("OGG SFX Loader: no samples!\n");

    ov_clear(&ogg_stream);

    // free the data
    delete[] data;

    return true;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
