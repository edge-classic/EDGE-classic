//------------------------------------------------------------------------
//  PATCH Loading
//------------------------------------------------------------------------
//
//  DEH_EDGE  Copyright (C) 2004-2023  The EDGE Team
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

#ifndef __DEH_PATCH_HDR__
#define __DEH_PATCH_HDR__

namespace Deh_Edge
{

namespace Patch
{
extern char line_buf[];
extern int  line_num;

extern int active_obj;
extern int patch_fmt;
extern int doom_ver;

dehret_e Load(input_buffer_c *buf);
} // namespace Patch

} // namespace Deh_Edge

#endif /* __DEH_PATCH_HDR__ */
