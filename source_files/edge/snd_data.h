//----------------------------------------------------------------------------
//  Sound Data
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

#pragma once

#include <stdint.h>

enum SoundBufferMix
{
    kMixMono        = 0,
    kMixInterleaved = 1
};

enum SoundFilter
{
    kFilterNone      = 0,
    kFilterVacuum    = 1,
    kFilterSubmerged = 2,
    kFilterReverb    = 3
};

enum ReverbRoomSize
{
    kRoomReverbNone   = 0,
    kRoomReverbSmall  = 1,
    kRoomReverbMedium = 2,
    kRoomReverbLarge  = 3
};

class SoundData
{
  public:
    int length_;    // number of samples
    int frequency_; // frequency
    int mode_;      // one of the kMixxxx values

    // 32-bit floating point samples.
    float *data_;

    // values for the engine to use
    void *definition_data_;

    bool is_sound_effect_;

  public:
    SoundData();
    ~SoundData();

    void Allocate(int samples, int buf_mode);
    void Free();
};

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
