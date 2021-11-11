//----------------------------------------------------------------------------
//  EDGE TinySoundfont Music Player
//----------------------------------------------------------------------------
// 
//  Copyright (c) 2004-2009  The EDGE Team.
//  Converted from the original Timidity-based player in 2021 - Dashodanger
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

#ifndef __S_TSF_H__
#define __S_TSF_H__

#include "i_defs.h"

bool S_StartupTSF(void);

abstract_music_c * S_PlayTSF(byte *data, int length, bool is_mus,
			float volume, bool loop);
// this function will delete[] the 'data' parameter

#endif /* __S_TSF_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
