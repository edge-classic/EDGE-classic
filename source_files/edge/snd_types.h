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
#if EDGE_WAV_SUPPORT
    kSoundWAV,
#endif
#if EDGE_FLAC_SUPPORT
    kSoundFLAC,
#endif
#if EDGE_OGG_SUPPORT
    kSoundOGG,
#endif
#if EDGE_MP3_SUPPORT
    kSoundMP3,
#endif
#if EDGE_TRACKER_SUPPORT
    kSoundIBXM,
#endif
#if EDGE_SID_SUPPORT
    kSoundSID,
#endif
#if EDGE_RAD_SUPPORT
    kSoundRAD,
#endif
#if EDGE_MUS_SUPPORT
    kSoundMUS,
#endif
    kSoundMIDI,
#if EDGE_IMF_SUPPORT
    kSoundIMF, // Used with DDFPLAY; not in auto-detection
#endif
#if EDGE_DOOM_SFX_SUPPORT
    kSoundDoom,
    kSoundPCSpeaker
#endif
};

// determine sound format from the file.
SoundFormat DetectSoundFormat(uint8_t *data, int song_len);

// determine sound format from the filename (by its extension).
SoundFormat SoundFilenameToFormat(std::string_view filename);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
