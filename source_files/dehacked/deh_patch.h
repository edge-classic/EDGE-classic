//------------------------------------------------------------------------
//  PATCH Loading
//------------------------------------------------------------------------
//
//  DEH_EDGE  Copyright (C) 2004-2024 The EDGE Team
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License (in COPYING.txt) for more details.
//
//------------------------------------------------------------------------

#pragma once

#include "deh_buffer.h"
#include "deh_edge.h"

namespace dehacked
{

namespace patch
{
extern char line_buf[];
extern int  line_num;

extern int active_obj;
extern int patch_fmt;
extern int doom_ver;

DehackedResult Load(InputBuffer *buf);
} // namespace patch

} // namespace dehacked