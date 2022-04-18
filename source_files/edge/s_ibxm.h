//----------------------------------------------------------------------------
//  EDGE IBXM Music Player (HEADER)
//----------------------------------------------------------------------------
// 
//  Copyright (c) 2022 - The EDGE-Classic Community
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

#ifndef __IBXMPLAYER_H__
#define __IBXMPLAYER_H__

#include "i_defs.h"

#include "sound_data.h"

class pl_entry_c;

/* FUNCTIONS */

abstract_music_c * S_PlayIBXMMusic(const pl_entry_c *musdat, float volume, bool looping);

bool S_CheckIBXM(byte *data, int length);

#endif  /* __XMPPLAYER_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
