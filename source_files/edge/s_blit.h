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

    SoundData *data_;

    int                    category_;
    SoundEffectDefinition *definition_;
    Position              *position_;

    bool loop_;       // will loop *one* more time
    bool boss_;

    ma_sound channel_sound_;

  public:
    SoundChannel();
    ~SoundChannel();
};

extern ConsoleVariable sound_effect_volume;

extern SoundChannel *mix_channels[];
extern int           total_channels;

extern bool  vacuum_sound_effects;
extern bool  submerged_sound_effects;
extern bool  outdoor_reverb;
extern bool  dynamic_reverb;
extern bool  ddf_reverb;
extern int   ddf_reverb_type; // 0 = None, 1 = Reverb, 2 = Echo
extern int   ddf_reverb_ratio;
extern int   ddf_reverb_delay;
extern float music_player_gain;

void InitializeSoundChannels(int total);
void FreeSoundChannels(void);

void KillSoundChannel(int k);
void ReallocateSoundChannels(int total);

void UpdateSounds(Position *listener, BAMAngle angle);

//-------- API for Synthesised MUSIC --------------------

void SoundQueueInitialize(void);
// initialise the queueing system.

void SoundQueueShutdown(void);
// finalise the queuing system, stopping all playback.
// The data from all the buffers will be freed.

void SoundQueueStop(void);
// stop the currently playing queue.  All playing buffers
// are moved into the free list.

SoundData *SoundQueueGetFreeBuffer(int samples);
// returns the next unused (or finished) buffer, or nullptr
// if there are none.  The data_ field will be
// updated to ensure they hold the requested number of
// samples.

void SoundQueueAddBuffer(SoundData *buf, int freq);
// add a new buffer to be end of the queue.

void SoundQueueReturnBuffer(SoundData *buf);
// if something goes wrong and you cannot add the buffer,
// then this call will return the buffer to the free list.

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
