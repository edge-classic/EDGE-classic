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

#pragma once

#include "s_music.h"

extern bool fluid_disabled;

bool StartupFluid(void);

void RestartFluid(void);

AbstractMusicPlayer *PlayFluidMusic(uint8_t *data, int length, bool loop);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
