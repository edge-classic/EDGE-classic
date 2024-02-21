//----------------------------------------------------------------------------
//  EDGE Sound System Header for SDL
//----------------------------------------------------------------------------
//
//  Copyright (c) 1999-2024 The EDGE Team.
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

#include "epi_sdl.h"

#include <string>
#include <vector>

extern std::vector<std::string> available_soundfonts;

extern std::vector<std::string> available_genmidis;

extern SDL_AudioDeviceID mydev_id;