//----------------------------------------------------------------------------
//  EDGE OGG Music Player (HEADER)
//----------------------------------------------------------------------------
// 
//  Copyright (c) 2004-2009  The EDGE Team.
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
//
// -ACB- 2004/08/18 Written
//
#ifndef __OGGPLAYER_H__
#define __OGGPLAYER_H__

#include "i_defs.h"

#include "sound_data.h"

/* FUNCTIONS */

abstract_music_c * S_PlayOGGMusic(epi::file_c *file, float volume, bool looping);

bool S_LoadOGGSound(epi::sound_data_c *buf, const byte *data, int length);

#endif  /* __OGGPLAYER_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
