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

#include "snd_data.h"

SoundData::SoundData()
    : length_(0), frequency_(0), data_(nullptr),
      definition_data_(nullptr), is_sound_effect_(false)
{
}

SoundData::~SoundData()
{
    Free();
}

void SoundData::Free()
{
    length_ = 0;

    if (data_)
        delete[] data_;

    data_ = nullptr;
}

void SoundData::Allocate(int samples)
{
    // early out when requirements are already met
    if (data_ && length_ >= samples)
    {
        length_ = samples; // FIXME: perhaps keep allocated count
        return;
    }

    if (data_)
    {
        Free();
    }

    length_ = samples;

    data_ = new float[samples * 2];
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
