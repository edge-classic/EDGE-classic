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

#ifndef __S_BLIT__
#define __S_BLIT__

#include "con_var.h"

#include "sound_data.h"

#include "types.h"

// Forward declarations
class SoundEffectDefinition;
struct position_c;

// We use a 22.10 fixed point for sound offsets.  It's a reasonable
// compromise between longest sound and accumulated round-off error.
typedef unsigned long fixed22_t;

typedef enum
{
    CHAN_Empty    = 0,
    CHAN_Playing  = 1,
    CHAN_Finished = 2
} chan_state_e;

// channel info
class mix_channel_c
{
  public:
    int state; // CHAN_xxx

    sound_data_c *data;

    int         category;
    SoundEffectDefinition   *def;
    position_c *pos;

    fixed22_t offset;
    fixed22_t length;
    fixed22_t delta;

    int volume_L; // mixing volume
    int volume_R;

    bool loop; // will loop *one* more time
    bool boss;

  public:
    mix_channel_c();
    ~mix_channel_c();

    void ComputeDelta();
    void ComputeVolume();
    void ComputeMusicVolume();
};

extern cvar_c sfx_volume;

extern mix_channel_c *mix_chan[];
extern int            num_chan;

extern bool  vacuum_sfx;
extern bool  submerged_sfx;
extern bool  outdoor_reverb;
extern bool  dynamic_reverb;
extern bool  ddf_reverb;
extern int   ddf_reverb_type; // 0 = None, 1 = Reverb, 2 = Echo
extern int   ddf_reverb_ratio;
extern int   ddf_reverb_delay;
extern float mus_player_gain;

void S_InitChannels(int total);
void S_FreeChannels(void);

void S_KillChannel(int k);
void S_ReallocChannels(int total);

void S_MixAllChannels(void *stream, int len);
// mix all active channels into the output stream.
// 'len' is the number of samples (for stereo: pairs)
// to mix into the stream.

void S_UpdateSounds(position_c *listener, BAMAngle angle);

//-------- API for Synthesised MUSIC --------------------

void S_QueueInit(void);
// initialise the queueing system.

void S_QueueShutdown(void);
// finalise the queuing system, stopping all playback.
// The data from all the buffers will be freed.

void S_QueueStop(void);
// stop the currently playing queue.  All playing buffers
// are moved into the free list.

sound_data_c *S_QueueGetFreeBuffer(int samples, int buf_mode);
// returns the next unused (or finished) buffer, or nullptr
// if there are none.  The data_L/data_R fields will be
// updated to ensure they hold the requested number of
// samples and conform to the wanted buffer mode.

void S_QueueAddBuffer(sound_data_c *buf, int freq);
// add a new buffer to be end of the queue.

void S_QueueReturnBuffer(sound_data_c *buf);
// if something goes wrong and you cannot add the buffer,
// then this call will return the buffer to the free list.

#endif // __S_BLIT__

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
