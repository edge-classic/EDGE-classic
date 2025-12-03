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
#include "con_var.h"
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

extern ConsoleVariable fliplevels;

SoundChannel *mix_channels[kMaximumSoundChannels];
int           total_channels;

bool vacuum_sound_effects    = false;
bool submerged_sound_effects = false;

EDGE_DEFINE_CONSOLE_VARIABLE(sound_effect_volume, "0.15", kConsoleVariableFlagArchive)

static bool sound_effects_paused = false;

// these are analogous to view_x/y/z/angle
float    listen_x;
float    listen_y;
float    listen_z;
BAMAngle listen_angle;

extern int sound_device_frequency;

SoundChannel::SoundChannel() : state_(kChannelEmpty), data_(nullptr), definition_(nullptr), position_(nullptr)
{
    EPI_CLEAR_MEMORY(&channel_sound_, ma_sound, 1);
    EPI_CLEAR_MEMORY(&ref_config_, ma_audio_buffer_config, 1);
    EPI_CLEAR_MEMORY(&ref_, ma_audio_buffer, 1);
}

SoundChannel::~SoundChannel()
{
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
            chan->data_       = nullptr;
            chan->definition_ = nullptr;
            chan->position_   = nullptr;
            ma_sound_uninit(&chan->channel_sound_);
            ma_audio_buffer_uninit(&chan->ref_);
        }

        delete chan;
    }

    EPI_CLEAR_MEMORY(mix_channels, SoundChannel *, total_channels);
}

void KillSoundChannel(int k)
{
    SoundChannel *chan = mix_channels[k];

    if (chan->state_ != kChannelEmpty)
    {
        chan->data_       = nullptr;
        chan->definition_ = nullptr;
        chan->position_   = nullptr;
        chan->state_      = kChannelEmpty;
        ma_sound_stop(&chan->channel_sound_);
        ma_sound_uninit(&chan->channel_sound_);
        ma_audio_buffer_uninit(&chan->ref_);
    }
}

void UpdateSounds(MapObject *listener, BAMAngle angle)
{
    EDGE_ZoneScoped;

    ma_sound_group_set_volume(&sfx_node, sound_effect_volume.f_ * 0.5f);

    listen_x = listener ? listener->x : 0;
    listen_y = listener ? listener->y : 0;
    listen_z = listener ? listener->z : 0;

    ma_engine_listener_set_position(&sound_engine, 0, listen_x, listen_z, -listen_y);

    if (listener)
    {
        if (fliplevels.d_)
            ma_engine_listener_set_direction(&sound_engine, 0, epi::BAMCos(angle - kBAMAngle180), epi::BAMTan(listener->vertical_angle_),
                                            -epi::BAMSin(angle));
        else
            ma_engine_listener_set_direction(&sound_engine, 0, epi::BAMCos(angle), epi::BAMTan(listener->vertical_angle_),
                                            -epi::BAMSin(angle));
    }
    else
        ma_engine_listener_set_direction(&sound_engine, 0, 0, 0, 0);

    for (int i = 0; i < total_channels; i++)
    {
        SoundChannel *chan = mix_channels[i];

        if (chan->state_ == kChannelPlaying)
        {
            if (ma_sound_at_end(&chan->channel_sound_))
                chan->state_ = kChannelFinished;
            else if (chan->position_)
            {
                ma_sound_set_position(&chan->channel_sound_, chan->position_->x, chan->position_->z,
                                      -chan->position_->y);
                if (listener &&
                    ma_sound_get_attenuation_model(&chan->channel_sound_) == ma_attenuation_model_exponential)
                {
                    if (CheckSightToPoint(listener, chan->position_->x, chan->position_->y, chan->position_->z))
                        ma_sound_set_min_distance(&chan->channel_sound_, kMinimumSoundClipDistance);
                    else
                        ma_sound_set_min_distance(&chan->channel_sound_, kMinimumOccludedSoundClipDistance);
                }
            }
        }

        if (chan->state_ == kChannelFinished)
            KillSoundChannel(i);
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

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
