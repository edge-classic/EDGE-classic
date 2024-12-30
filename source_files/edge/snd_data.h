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

class SoundData
{
  public:
    int length_;    // number of samples
    int frequency_; // frequency

    // floating point samples
    float *data_;

    // values for the engine to use
    void *definition_data_;

  public:
    SoundData();
    ~SoundData();

    void Allocate(int samples);
    void Free();
};

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
