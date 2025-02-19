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

#include <set>
#include <string>

#include "con_var.h"
#include "miniaudio.h"
#include "miniaudio_freeverb.h"

extern std::set<std::string> available_soundfonts;

extern ma_engine        sound_engine;
extern ma_engine        music_engine;
extern ma_freeverb_node reverb_node;
extern ma_delay_node    underwater_node;
extern ma_delay_node    reverb_delay_node;
extern ma_lpf_node      vacuum_node;
extern bool             sector_reverb;  // true if we are in a sector with DDF reverb
extern bool             outdoor_reverb; // governs node attachment for dynamic reverb
extern ConsoleVariable  dynamic_reverb;