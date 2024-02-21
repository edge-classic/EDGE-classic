//----------------------------------------------------------------------------
//  EDGE FluidLite Music Player
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

#ifndef __S_FLUID_H__
#define __S_FLUID_H__

#include "s_music.h"

extern bool fluid_disabled;

bool S_StartupFluid(void);

void S_RestartFluid(void);

abstract_music_c *S_PlayFluid(uint8_t *data, int length, bool loop);

#endif /* __S_FLUID_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
