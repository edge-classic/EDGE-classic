//----------------------------------------------------------------------------
//  Sound Blitter
//----------------------------------------------------------------------------
//
//  Copyright (c) 1999-2024 The EDGE Team.
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
//  Based on the DOOM source code, released by Id Software under the
//  following copyright:
//
//    Copyright (C) 1993-1996 by id Software, Inc.
//
//----------------------------------------------------------------------------

#include "s_blit.h"

#include <list>

#include "dm_state.h"
#include "epi_sdl.h"
#include "i_system.h"
#include "m_misc.h"
#include "p_blockmap.h"
#include "p_local.h"  // ApproximateDistance
#include "r_misc.h"   // RendererPointToAngle
#include "s_cache.h"
#include "s_music.h"
#include "s_sound.h"

// Sound must be clipped to prevent distortion (clipping is
// a kind of distortion of course, but it's much better than
// the "white noise" you get when values overflow).
//
// The more safe bits there are, the less likely the final
// output sum will overflow into white noise, but the less
// precision you have left for the volume multiplier.
static constexpr uint8_t kSafeClippingBits = 4;
static constexpr int32_t kSoundClipThreshold =
    ((1 << (31 - kSafeClippingBits)) - 1);

static constexpr uint8_t  kMinimumSoundChannels = 32;
static constexpr uint16_t kMaximumSoundChannels = 256;

SoundChannel *mix_channels[kMaximumSoundChannels];
int           total_channels;

bool  vacuum_sound_effects    = false;
bool  submerged_sound_effects = false;
bool  outdoor_reverb          = false;
bool  dynamic_reverb          = false;
bool  ddf_reverb              = false;
int   ddf_reverb_type         = 0;
int   ddf_reverb_ratio        = 0;
int   ddf_reverb_delay        = 0;
float music_player_gain       = 1.0f;

static int *mix_buffer;
static int  mix_buffer_length;

static constexpr uint8_t kMaximumQueueBuffers = 16;

static std::list<SoundData *> free_queue_buffers;
static std::list<SoundData *> playing_queue_buffers;

static SoundChannel *queue_channel;

EDGE_DEFINE_CONSOLE_VARIABLE(sound_effect_volume, "0.15",
                             kConsoleVariableFlagArchive)

static bool sound_effects_paused = false;

// these are analogous to view_x/y/z/angle
float    listen_x;
float    listen_y;
float    listen_z;
BAMAngle listen_angle;

extern int sound_device_frequency;
extern int sound_device_bytes_per_sample;
extern int sound_device_samples_per_buffer;

extern bool sound_device_stereo;

SoundChannel::SoundChannel() : state_(kChannelEmpty), data_(nullptr) {}

SoundChannel::~SoundChannel() {}

void SoundChannel::ComputeDelta()
{
    // frequency close enough ?
    if (data_->frequency_ >
            (sound_device_frequency - sound_device_frequency / 100) &&
        data_->frequency_ <
            (sound_device_frequency + sound_device_frequency / 100))
    {
        delta_ = (1 << 10);
    }
    else
    {
        delta_ = (uint32_t)floor((float)data_->frequency_ * 1024.0f /
                                 sound_device_frequency);
    }
}

void SoundChannel::ComputeVolume()
{
    float sep  = 0.5f;
    float dist = 1.25f;

    if (position_ && category_ >= kCategoryOpponent)
    {
        if (sound_device_stereo)
        {
            BAMAngle angle = RendererPointToAngle(listen_x, listen_y,
                                                  position_->x, position_->y);

            // same equation from original DOOM
            sep = 0.5f - 0.38f * epi::BAMSin(angle - listen_angle);
        }

        if (!boss_)
        {
            dist = ApproximateDistance(listen_x - position_->x,
                                       listen_y - position_->y,
                                       listen_z - position_->z);

            if (players[console_player] && players[console_player]->map_object_)
            {
                if (CheckSightToPoint(players[console_player]->map_object_, position_->x,
                                      position_->y, position_->z))
                    dist = HMM_MAX(1.25f, dist / 100.0f);
                else
                    dist = HMM_MAX(1.25f, dist / 75.0f);
            }
        }
    }

    float MAX_VOL = (1 << (16 - kSafeClippingBits)) - 3;

    MAX_VOL = (boss_ ? MAX_VOL : MAX_VOL / dist) * sound_effect_volume.f_;

    if (definition_) MAX_VOL *= definition_->volume_;

    // strictly linear equations
    volume_left_  = (int)(MAX_VOL * (1.0 - sep));
    volume_right_ = (int)(MAX_VOL * (0.0 + sep));

    if (var_sound_stereo == 2) /* SWAP ! */
    {
        int tmp       = volume_left_;
        volume_left_  = volume_right_;
        volume_right_ = tmp;
    }
}

void SoundChannel::ComputeMusicVolume()
{
    float MAX_VOL = (1 << (16 - kSafeClippingBits)) - 3;

    MAX_VOL =
        MAX_VOL * music_volume.f_ *
        music_player_gain;  // This last one is an internal value and won't
                            // exceed 1.0, so MAX_VOL should be consistent

    volume_left_  = (int)MAX_VOL;
    volume_right_ = (int)MAX_VOL;
}

//----------------------------------------------------------------------------

static void BlitToS16(const int *src, int16_t *dest, int length)
{
    const int *s_end = src + length;

    while (src < s_end)
    {
        int val = *src++;

        if (val > kSoundClipThreshold)
            val = kSoundClipThreshold;
        else if (val < -kSoundClipThreshold)
            val = -kSoundClipThreshold;

        *dest++ = (int16_t)(val >> (16 - kSafeClippingBits));
    }
}

static void MixMono(SoundChannel *chan, int *dest, int pairs)
{
    SYS_ASSERT(pairs > 0);

    int16_t *src_L;

    if (paused || menu_active)
        src_L = chan->data_->data_left_;
    else
    {
        if (!chan->data_->is_sound_effect_ || chan->category_ == kCategoryUi ||
            chan->data_->current_filter_ == kFilterNone)
            src_L = chan->data_->data_left_;
        else
            src_L = chan->data_->filter_data_left_;
    }

    int *d_pos = dest;
    int *d_end = d_pos + pairs;

    uint32_t offset = chan->offset_;

    while (d_pos < d_end)
    {
        *d_pos++ += src_L[offset >> 10] * chan->volume_left_;

        offset += chan->delta_;
    }

    chan->offset_ = offset;

    SYS_ASSERT(offset - chan->delta_ < chan->length_);
}

static void MixStereo(SoundChannel *chan, int *dest, int pairs)
{
    SYS_ASSERT(pairs > 0);

    int16_t *src_L;
    int16_t *src_R;

    if (paused || menu_active)
    {
        src_L = chan->data_->data_left_;
        src_R = chan->data_->data_right_;
    }
    else
    {
        if (!chan->data_->is_sound_effect_ || chan->category_ == kCategoryUi ||
            chan->data_->current_filter_ == kFilterNone)
        {
            src_L = chan->data_->data_left_;
            src_R = chan->data_->data_right_;
        }
        else
        {
            src_L = chan->data_->filter_data_left_;
            src_R = chan->data_->filter_data_right_;
        }
    }

    int *d_pos = dest;
    int *d_end = d_pos + pairs * 2;

    uint32_t offset = chan->offset_;

    while (d_pos < d_end)
    {
        *d_pos++ += src_L[offset >> 10] * chan->volume_left_;
        *d_pos++ += src_R[offset >> 10] * chan->volume_right_;

        offset += chan->delta_;
    }

    chan->offset_ = offset;

    SYS_ASSERT(offset - chan->delta_ < chan->length_);
}

static void MixInterleaved(SoundChannel *chan, int *dest, int pairs)
{
    if (!sound_device_stereo)
        FatalError(
            "INTERNAL ERROR: tried to mix an interleaved buffer in MONO "
            "mode.\n");

    SYS_ASSERT(pairs > 0);

    int16_t *src_L;

    if (paused || menu_active)
        src_L = chan->data_->data_left_;
    else
    {
        if (!chan->data_->is_sound_effect_ || chan->category_ == kCategoryUi ||
            chan->data_->current_filter_ == kFilterNone)
            src_L = chan->data_->data_left_;
        else
            src_L = chan->data_->filter_data_left_;
    }

    int *d_pos = dest;
    int *d_end = d_pos + pairs * 2;

    uint32_t offset = chan->offset_;

    while (d_pos < d_end)
    {
        uint32_t pos = (offset >> 9) & ~1;

        *d_pos++ += src_L[pos] * chan->volume_left_;
        *d_pos++ += src_L[pos | 1] * chan->volume_right_;

        offset += chan->delta_;
    }

    chan->offset_ = offset;

    SYS_ASSERT(offset - chan->delta_ < chan->length_);
}

static void MixOneChannel(SoundChannel *chan, int pairs)
{
    if (sound_effects_paused && chan->category_ >= kCategoryPlayer) return;

    if (chan->volume_left_ == 0 && chan->volume_right_ == 0) return;

    SYS_ASSERT(chan->offset_ < chan->length_);

    int *dest = mix_buffer;

    while (pairs > 0)
    {
        int count = pairs;

        // check if enough sound data is left
        if (chan->offset_ + count * chan->delta_ >= chan->length_)
        {
            // find minimum number of samples we can play
            double avail = (chan->length_ - chan->offset_ + chan->delta_ - 1) /
                           (double)chan->delta_;

            count = (int)floor(avail);

            SYS_ASSERT(count > 0);
            SYS_ASSERT(count <= pairs);

            SYS_ASSERT(chan->offset_ + count * chan->delta_ >= chan->length_);
        }

        if (chan->data_->mode_ == kMixInterleaved)
            MixInterleaved(chan, dest, count);
        else if (sound_device_stereo)
            MixStereo(chan, dest, count);
        else
            MixMono(chan, dest, count);

        if (chan->offset_ >= chan->length_)
        {
            if (!chan->loop_)
            {
                chan->state_ = kChannelFinished;
                break;
            }

            // we are looping, so clear flag.  The sound needs to
            // be "pumped" (played again) to continue looping.
            chan->loop_ = false;

            chan->offset_ = 0;
        }

        dest += count * (sound_device_stereo ? 2 : 1);
        pairs -= count;
    }
}

static bool QueueNextBuffer(void)
{
    if (playing_queue_buffers.empty())
    {
        queue_channel->state_ = kChannelFinished;
        queue_channel->data_  = nullptr;
        return false;
    }

    SoundData *buf = playing_queue_buffers.front();

    queue_channel->data_ = buf;

    queue_channel->offset_ = 0;
    queue_channel->length_ = buf->length_ << 10;

    queue_channel->ComputeDelta();

    queue_channel->state_ = kChannelPlaying;
    return true;
}

static void MixQueues(int pairs)
{
    SoundChannel *chan = queue_channel;

    if (!chan || !chan->data_ || chan->state_ != kChannelPlaying) return;

    if (chan->volume_left_ == 0 && chan->volume_right_ == 0) return;

    SYS_ASSERT(chan->offset_ < chan->length_);

    int *dest = mix_buffer;

    while (pairs > 0)
    {
        int count = pairs;

        // check if enough sound data is left
        if (chan->offset_ + count * chan->delta_ >= chan->length_)
        {
            // find minimum number of samples we can play
            double avail = (chan->length_ - chan->offset_ + chan->delta_ - 1) /
                           (double)chan->delta_;

            count = (int)floor(avail);

            SYS_ASSERT(count > 0);
            SYS_ASSERT(count <= pairs);

            SYS_ASSERT(chan->offset_ + count * chan->delta_ >= chan->length_);
        }

        if (chan->data_->mode_ == kMixInterleaved)
            MixInterleaved(chan, dest, count);
        else if (sound_device_stereo)
            MixStereo(chan, dest, count);
        else
            MixMono(chan, dest, count);

        if (chan->offset_ >= chan->length_)
        {
            // reached end of current queued buffer.
            // Place current buffer onto free list,
            // and enqueue the next buffer to play.

            SYS_ASSERT(!playing_queue_buffers.empty());

            SoundData *buf = playing_queue_buffers.front();
            playing_queue_buffers.pop_front();

            free_queue_buffers.push_back(buf);

            if (!QueueNextBuffer()) break;
        }

        dest += count * (sound_device_stereo ? 2 : 1);
        pairs -= count;
    }
}

void SoundMixAllChannels(void *stream, int len)
{
    if (no_sound || len <= 0) return;

    int pairs = len / sound_device_bytes_per_sample;

    int samples = pairs;
    if (sound_device_stereo) samples *= 2;

    // check that we're not getting too much data
    SYS_ASSERT(pairs <= sound_device_samples_per_buffer);

    SYS_ASSERT(mix_buffer && samples <= mix_buffer_length);

    // clear mixer buffer
    memset(mix_buffer, 0, mix_buffer_length * sizeof(int));

    // add each channel
    for (int i = 0; i < total_channels; i++)
    {
        if (mix_channels[i]->state_ == kChannelPlaying)
        {
            MixOneChannel(mix_channels[i], pairs);
        }
    }

    MixQueues(pairs);

    // blit to the SDL stream
    BlitToS16(mix_buffer, (int16_t *)stream, samples);
}

//----------------------------------------------------------------------------

void SoundInitializeChannels(int total)
{
    // NOTE: assumes audio is locked!

    SYS_ASSERT(total >= kMinimumSoundChannels);
    SYS_ASSERT(total <= kMaximumSoundChannels);

    total_channels = total;

    for (int i = 0; i < total_channels; i++)
        mix_channels[i] = new SoundChannel();

    // allocate mixer buffer
    mix_buffer_length =
        sound_device_samples_per_buffer * (sound_device_stereo ? 2 : 1);
    mix_buffer = new int[mix_buffer_length];
}

void SoundFreeChannels(void)
{
    // NOTE: assumes audio is locked!

    for (int i = 0; i < total_channels; i++)
    {
        SoundChannel *chan = mix_channels[i];

        if (chan && chan->data_) { chan->data_ = nullptr; }

        delete chan;
    }

    memset(mix_channels, 0, sizeof(mix_channels));
}

void SoundKillChannel(int k)
{
    SoundChannel *chan = mix_channels[k];

    if (chan->state_ != kChannelEmpty)
    {
        chan->data_  = nullptr;
        chan->state_ = kChannelEmpty;
    }
}

void SoundReallocateChannels(int total)
{
    // NOTE: assumes audio is locked!

    SYS_ASSERT(total >= kMinimumSoundChannels);
    SYS_ASSERT(total <= kMaximumSoundChannels);

    if (total > total_channels)
    {
        for (int i = total_channels; i < total; i++)
            mix_channels[i] = new SoundChannel();
    }

    if (total < total_channels)
    {
        // kill all non-UI sounds, pack the UI sounds into the
        // remaining slots (normally there will be enough), and
        // delete the unused channels
        int i, j;

        for (i = 0; i < total_channels; i++)
        {
            SoundChannel *chan = mix_channels[i];

            if (chan->state_ == kChannelPlaying)
            {
                if (chan->category_ != kCategoryUi) SoundKillChannel(i);
            }
        }

        for (i = j = 0; i < total_channels; i++)
        {
            if (mix_channels[i])
            {
                /* SWAP ! */
                SoundChannel *tmp = mix_channels[j];

                mix_channels[j] = mix_channels[i];
                mix_channels[i] = tmp;
            }
        }

        for (i = total; i < total_channels; i++)
        {
            if (mix_channels[i]->state_ == kChannelPlaying) SoundKillChannel(i);

            delete mix_channels[i];
            mix_channels[i] = nullptr;
        }
    }

    total_channels = total;
}

void UpdateSounds(Position *listener, BAMAngle angle)
{
    // NOTE: assume SDL_LockAudio has been called

    listen_x = listener ? listener->x : 0;
    listen_y = listener ? listener->y : 0;
    listen_z = listener ? listener->z : 0;

    listen_angle = angle;

    for (int i = 0; i < total_channels; i++)
    {
        SoundChannel *chan = mix_channels[i];

        if (chan->state_ == kChannelPlaying)
            chan->ComputeVolume();

        else if (chan->state_ == kChannelFinished)
            SoundKillChannel(i);
    }

    if (queue_channel) queue_channel->ComputeMusicVolume();
}

void PauseSound(void) { sound_effects_paused = true; }

void ResumeSound(void) { sound_effects_paused = false; }

//----------------------------------------------------------------------------

void SoundQueueInitialize(void)
{
    if (no_sound) return;

    LockAudio();
    {
        if (free_queue_buffers.empty())
        {
            for (int i = 0; i < kMaximumQueueBuffers; i++)
            {
                free_queue_buffers.push_back(new SoundData());
            }
        }

        if (!queue_channel) queue_channel = new SoundChannel();

        queue_channel->state_ = kChannelEmpty;
        queue_channel->data_  = nullptr;

        queue_channel->ComputeMusicVolume();
    }
    UnlockAudio();
}

void SoundQueueShutdown(void)
{
    if (no_sound) return;

    LockAudio();
    {
        if (queue_channel)
        {
            // free all data on the playing / free lists.
            // The SoundData destructor takes care of data_left_/R.

            for (; !playing_queue_buffers.empty();
                 playing_queue_buffers.pop_front())
            {
                delete playing_queue_buffers.front();
            }
            for (; !free_queue_buffers.empty(); free_queue_buffers.pop_front())
            {
                delete free_queue_buffers.front();
            }

            queue_channel->data_ = nullptr;

            delete queue_channel;
            queue_channel = nullptr;
        }
    }
    UnlockAudio();
}

void SoundQueueStop(void)
{
    if (no_sound) return;

    SYS_ASSERT(queue_channel);

    LockAudio();
    {
        for (; !playing_queue_buffers.empty();
             playing_queue_buffers.pop_front())
        {
            free_queue_buffers.push_back(playing_queue_buffers.front());
        }

        queue_channel->state_ = kChannelFinished;
        queue_channel->data_  = nullptr;
    }
    UnlockAudio();
}

SoundData *SoundQueueGetFreeBuffer(int samples, int buf_mode)
{
    if (no_sound) return nullptr;

    SoundData *buf = nullptr;

    LockAudio();
    {
        if (!free_queue_buffers.empty())
        {
            buf = free_queue_buffers.front();
            free_queue_buffers.pop_front();

            buf->Allocate(samples, buf_mode);
        }
    }
    UnlockAudio();

    return buf;
}

void SoundQueueAddBuffer(SoundData *buf, int freq)
{
    SYS_ASSERT(!no_sound);
    SYS_ASSERT(buf);

    LockAudio();
    {
        buf->frequency_ = freq;

        playing_queue_buffers.push_back(buf);

        if (queue_channel->state_ != kChannelPlaying) { QueueNextBuffer(); }
    }
    UnlockAudio();
}

void SoundQueueReturnBuffer(SoundData *buf)
{
    SYS_ASSERT(!no_sound);
    SYS_ASSERT(buf);

    LockAudio();
    {
        free_queue_buffers.push_back(buf);
    }
    UnlockAudio();
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
