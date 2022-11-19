//------------------------------------------------------------------------
//  Sound Format Detection
//------------------------------------------------------------------------
// 
//  Copyright (c) 2022 - The EDGE Team.
// 
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//------------------------------------------------------------------------

#ifndef __EPI_SOUND_TYPES_H__
#define __EPI_SOUND_TYPES_H__

#include "file.h"

namespace epi
{

typedef enum
{
	FMT_Unknown = 0,
	FMT_WAV,
	FMT_FLAC,
	FMT_OGG,
	FMT_MP3,
	FMT_XMP,
	FMT_GME,
	FMT_SID,
	FMT_VGM,
	FMT_MUS,
	FMT_MIDI,
	FMT_DOOM,
	FMT_SPK
}
sound_format_e;

// determine sound format from the first 32 bytes (or so) of the file.
sound_format_e Sound_DetectFormat(byte *data, int header_len);

// determine sound format from the filename (by its extension).
sound_format_e Sound_FilenameToFormat(const std::string& filename);

}  // namespace epi

#endif  /* __EPI_SOUND_TYPES_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
