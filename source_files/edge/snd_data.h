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
    kMixStereo      = 1,
    kMixInterleaved = 2
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
    int length_;     // number of samples
    int frequency_;  // frequency
    int mode_;       // one of the kMixxxx values

    // signed 16-bit samples.
    // For kMixMono, both pointers refer to the same memory.
    // For kMixInterleaved, only data_left_ is used and contains
    // both channels, left samples before right samples.
    int16_t *data_left_;
    int16_t *data_right_;

    // Temp buffer for mixed SFX. Will be overwritten as needed.
    int16_t *filter_data_left_;
    int16_t *filter_data_right_;

    // values for the engine to use
    void *definition_data_;

    bool is_sound_effect_;

    SoundFilter current_filter_;

    ReverbRoomSize reverbed_room_size_;

    int current_ddf_reverb_ratio_;
    int current_ddf_reverb_delay_;
    int current_ddf_reverb_type_;

    bool reverb_is_outdoors_;

   public:
    SoundData();
    ~SoundData();

    void Allocate(int samples, int buf_mode);
    void Free();
    void FreeFilter();
    void MixVacuum();
    void MixSubmerged();
    void MixReverb(bool dynamic_reverb, float room_area, bool outdoor_reverb,
                   int ddf_reverb_type, int ddf_reverb_ratio,
                   int ddf_reverb_delay);
};

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
