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

#include "AlmostEquals.h"
#include "dm_state.h"
#include "epi.h"
#include "epi_sdl.h"
#include "i_sound.h"
#include "i_system.h"
#include "m_misc.h"
#include "p_blockmap.h"
#include "p_local.h" // ApproximateDistance
#include "r_misc.h"  // PointToAngle
#include "s_cache.h"
#include "s_music.h"
#include "s_sound.h"

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

static constexpr uint8_t kMaximumQueueBuffers = 16;

static std::list<SoundData *> free_queue_buffers;
static std::list<SoundData *> playing_queue_buffers;

static SoundChannel *queue_channel;

EDGE_DEFINE_CONSOLE_VARIABLE(sound_effect_volume, "0.15", kConsoleVariableFlagArchive)

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

SoundChannel::SoundChannel() : state_(kChannelEmpty), data_(nullptr)
{
    EPI_CLEAR_MEMORY(&channel_sound_, ma_sound, 1);
}

SoundChannel::~SoundChannel()
{
}

//----------------------------------------------------------------------------

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

    queue_channel->state_ = kChannelPlaying;
    return true;
}

//----------------------------------------------------------------------------

void InitializeSoundChannels(int total)
{
    total_channels = total;

    for (int i = 0; i < total_channels; i++)
        mix_channels[i] = new SoundChannel();
}

void FreeSoundChannels(void)
{
    for (int i = 0; i < total_channels; i++)
    {
        SoundChannel *chan = mix_channels[i];

        if (chan && chan->data_)
        {
            chan->data_ = nullptr;
            ma_sound_uninit(&chan->channel_sound_);
        }

        delete chan;
    }

    EPI_CLEAR_MEMORY(mix_channels, SoundChannel*, 256);
}

void KillSoundChannel(int k)
{
    SoundChannel *chan = mix_channels[k];

    if (chan->state_ != kChannelEmpty)
    {
        chan->data_  = nullptr;
        chan->state_ = kChannelEmpty;
        ma_sound_stop(&chan->channel_sound_);
        ma_sound_uninit(&chan->channel_sound_);
    }
}

void ReallocateSoundChannels(int total)
{
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
                if (chan->category_ != kCategoryUi)
                    KillSoundChannel(i);
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
            if (mix_channels[i]->state_ == kChannelPlaying)
                KillSoundChannel(i);

            delete mix_channels[i];
            mix_channels[i] = nullptr;
        }
    }

    total_channels = total;
}

void UpdateSounds(Position *listener, BAMAngle angle)
{
    ma_engine_listener_set_position(&sound_engine, 0, listener ? listener->x : 0, listener ? listener->y : 0,
        listener ? listener->z : 0);

    for (int i = 0; i < total_channels; i++)
    {
        SoundChannel *chan = mix_channels[i];

        if (chan->state_ == kChannelPlaying && ma_sound_at_end(&chan->channel_sound_))
            chan->state_ = kChannelFinished;

        if (chan->state_ == kChannelFinished)
            KillSoundChannel(i);

        if (chan->state_ == kChannelPlaying)
        {
            if (chan->position_)
                ma_sound_set_position(&chan->channel_sound_, chan->position_->x, chan->position_->y, chan->position_->z);
            else
                ma_sound_set_position(&chan->channel_sound_, 0, 0, 0);
        }
    }
}

void PauseSound(void)
{
    sound_effects_paused = true;
}

void ResumeSound(void)
{
    sound_effects_paused = false;
}

//----------------------------------------------------------------------------

void SoundQueueInitialize(void)
{
    if (no_sound)
        return;

    if (free_queue_buffers.empty())
    {
        for (int i = 0; i < kMaximumQueueBuffers; i++)
        {
            free_queue_buffers.push_back(new SoundData());
        }
    }

    if (!queue_channel)
        queue_channel = new SoundChannel();
    else
        ma_sound_uninit(&queue_channel->channel_sound_);

    queue_channel->state_ = kChannelEmpty;
    queue_channel->data_  = nullptr;
}

void SoundQueueShutdown(void)
{
    if (no_sound)
        return;

    if (queue_channel)
    {
        // free all data on the playing / free lists.
        // The SoundData destructor takes care of data_left_/R.

        for (; !playing_queue_buffers.empty(); playing_queue_buffers.pop_front())
        {
            delete playing_queue_buffers.front();
        }
        for (; !free_queue_buffers.empty(); free_queue_buffers.pop_front())
        {
            delete free_queue_buffers.front();
        }

        queue_channel->data_ = nullptr;

        ma_sound_uninit(&queue_channel->channel_sound_);

        delete queue_channel;
        queue_channel = nullptr;
    }
}

void SoundQueueStop(void)
{
    if (no_sound)
        return;

    EPI_ASSERT(queue_channel);

    for (; !playing_queue_buffers.empty(); playing_queue_buffers.pop_front())
    {
        free_queue_buffers.push_back(playing_queue_buffers.front());
    }

    queue_channel->state_ = kChannelFinished;
    queue_channel->data_  = nullptr;
    if (queue_channel->channel_sound_.pDataSource)
        ma_sound_uninit(&queue_channel->channel_sound_);
}

SoundData *SoundQueueGetFreeBuffer(int samples)
{
    if (no_sound)
        return nullptr;

    SoundData *buf = nullptr;

    if (!free_queue_buffers.empty())
    {
        buf = free_queue_buffers.front();
        free_queue_buffers.pop_front();

        buf->Allocate(samples);
    }

    return buf;
}

void SoundQueueAddBuffer(SoundData *buf, int freq)
{
    EPI_ASSERT(!no_sound);
    EPI_ASSERT(buf);

    buf->frequency_ = freq;

    playing_queue_buffers.push_back(buf);

    if (queue_channel->state_ != kChannelPlaying)
    {
        QueueNextBuffer();
    }
}

void SoundQueueReturnBuffer(SoundData *buf)
{
    EPI_ASSERT(!no_sound);
    EPI_ASSERT(buf);

    free_queue_buffers.push_back(buf);
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
