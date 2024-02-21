//----------------------------------------------------------------------------
//  EDGE WAV Sound Loader (HEADER)
//----------------------------------------------------------------------------
//
//  Copyright (c) 2022-2024 The EDGE Team.
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

#ifndef __WAVLOADER_H__
#define __WAVLOADER_H__



#include "sound_data.h"

/* FUNCTIONS */

bool S_LoadWAVSound(sound_data_c *buf, uint8_t *data, int length, bool pc_speaker);

#endif /* __WAVLOADER_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
