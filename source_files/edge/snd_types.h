//------------------------------------------------------------------------
//  Sound Format Detection
//------------------------------------------------------------------------
//
//  Copyright (c) 2022-2023 - The EDGE Team.
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
//------------------------------------------------------------------------

#pragma once

#include <stdint.h>

#include <string>

enum SoundFormat
{
    kSoundUnknown = 0,
    kSoundWAV,
    kSoundFLAC,
    kSoundOGG,
    kSoundMP3,
    kSoundIBXM,
    kSoundSID,
    kSoundRAD,
    kSoundMUS,
    kSoundMIDI,
    kSoundIMF, // Used with DDFPLAY; not in auto-detection
    kSoundDoom,
    kSoundPCSpeaker
};

// determine sound format from the file.
SoundFormat DetectSoundFormat(uint8_t *data, int song_len);

// determine sound format from the filename (by its extension).
SoundFormat SoundFilenameToFormat(std::string_view filename);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
