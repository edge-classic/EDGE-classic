//----------------------------------------------------------------------------
//  EDGE Timidity Music Player
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

#ifndef __S_TIMID_H__
#define __S_TIMID_H__

#include "i_defs.h"

bool S_StartupTimidity(void);
void S_ChangeTimidQuiet(void);

abstract_music_c * S_PlayTimidity(byte *data, int length, bool is_mus,
			float volume, bool loop);
// this function will delete[] the 'data' parameter

#endif /* __S_TIMID_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
