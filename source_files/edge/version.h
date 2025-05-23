//----------------------------------------------------------------------------
//  EDGE Version Header
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

#include "con_var.h"

extern ConsoleVariable window_title;
extern ConsoleVariable edge_version;
extern ConsoleVariable application_name;

// This is used for configuration file and save game compatibility reasons,
// and should not be changeable by the user unlike the displayed version
// in the branding.cfg file - Dasho
constexpr uint8_t kInternalConfigVersion = 5;
extern bool       show_old_config_warning;

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
