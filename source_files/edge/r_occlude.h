//----------------------------------------------------------------------------
//  EDGE OpenGL Rendering (Occlusion testing)
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2009  The EDGE Team.
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

#ifndef __RGL_OCCLUDE_H__
#define __RGL_OCCLUDE_H__

void RGL_1DOcclusionClear(void);
void RGL_1DOcclusionSet(angle_t low, angle_t high);
bool RGL_1DOcclusionTest(angle_t low, angle_t high);

#endif /* __RGL_OCCLUDE_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
