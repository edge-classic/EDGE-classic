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

#pragma once

#include "con_var.h"
#include "ddf_types.h"
#include "miniaudio.h"
#include "p_mobj.h"
#include "snd_data.h"

// Forward declarations
class SoundEffectDefinition;
struct Position;

enum ChannelState
{
    kChannelEmpty    = 0,
    kChannelPlaying  = 1,
    kChannelFinished = 2
};

// channel info
class SoundChannel
{
  public:
    int state_; // CHAN_xxx

    const SoundData *data_;

    int                          category_;
    const SoundEffectDefinition *definition_;
    const Position              *position_;

    bool boss_;

    ma_audio_buffer_config ref_config_;
    ma_audio_buffer        ref_;
    ma_sound               channel_sound_;

  public:
    SoundChannel();
    ~SoundChannel();
};

constexpr uint16_t kMaximumSoundChannels = 128;

extern ConsoleVariable sound_effect_volume;

extern SoundChannel *mix_channels[];
extern int           total_channels;

extern bool            vacuum_sound_effects;
extern bool            submerged_sound_effects;
extern ConsoleVariable dynamic_reverb;

void InitializeSoundChannels(int total);
void FreeSoundChannels(void);

void KillSoundChannel(int k);

void UpdateSounds(MapObject *listener, BAMAngle angle);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
