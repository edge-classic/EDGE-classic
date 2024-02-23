//----------------------------------------------------------------------------
//  EDGE FLAC Music Player (HEADER)
//----------------------------------------------------------------------------
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
//----------------------------------------------------------------------------

#ifndef __FLACPLAYER_H__
#define __FLACPLAYER_H__



#include "sound_data.h"

/* FUNCTIONS */

AbstractMusicPlayer *S_PlayFLACMusic(uint8_t *data, int length, bool looping);

#endif /* __FLACPLAYER_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
