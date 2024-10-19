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

    // 16-bit signed samples.
    int16_t *data_;

    // Temp buffer for mixed SFX. Will be overwritten as needed.
    int16_t *filter_data_;

    // Circular buffer used for reverb processing, if needed
    int *reverb_buffer_;

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

    void Allocate(int samples);
    void Free();
    void MixVacuum();
    void MixSubmerged();
    void MixReverb(bool dynamic_reverb, float room_area, bool outdoor_reverb, int ddf_reverb_type, int ddf_reverb_ratio,
                   int ddf_reverb_delay);
};

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
