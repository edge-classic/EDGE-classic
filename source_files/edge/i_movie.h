//----------------------------------------------------------------------------
//  EDGE Movie Playback (MPEG) (HEADER)
//----------------------------------------------------------------------------
//
//  Copyright (c) 2018-2024 The EDGE Team
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

#include <string>

#include "e_event.h"

extern bool playing_movie;

void PlayMovie(const std::string &name);
void MovieTicker(void);
void MovieDrawer(void);
bool MovieResponder(InputEvent *ev);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
