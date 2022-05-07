//----------------------------------------------------------------------------
//  WAV Format Sound Loading
//----------------------------------------------------------------------------
// 
//  Copyright (c) 2007-2008  The EDGE Team.
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
//----------------------------------------------------------------------------

#ifndef __EPI_SOUND_WAV_H__
#define __EPI_SOUND_WAV_H__

#include "file.h"
#include "sound_data.h"

namespace epi
{

bool WAV_Load(sound_data_c *buf, file_c *f);
// Decode WAV format sound data from the given file stream,
// storing the results in the given sound_data_c object.
// Returns false if something went wrong.

} // namespace epi

#endif /* __EPI_SOUND_WAV_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
