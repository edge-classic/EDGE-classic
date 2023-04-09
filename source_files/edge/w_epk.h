//----------------------------------------------------------------------------
//  EDGE EPK Support Code
//----------------------------------------------------------------------------
//
//  Copyright (c) 2022-2023  The EDGE Team.
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

#ifndef __W_EPK__
#define __W_EPK__

#include "dm_defs.h"

// EPI
#include "file.h"

class pack_file_c;

epi::file_c * Pack_OpenFile(pack_file_c *pack, const std::string& name);

void Pack_ProcessImages(pack_file_c *pack, const std::string& dir_name, const std::string& prefix);

#endif /* __W_PK3__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
