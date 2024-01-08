//----------------------------------------------------------------------------
//  EDGE OpenGL Rendering (Occlusion testing)
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

#ifndef __RGL_OCCLUDE_H__
#define __RGL_OCCLUDE_H__

void RGL_1DOcclusionClear(void);
void RGL_1DOcclusionSet(bam_angle_t low, bam_angle_t high);
bool RGL_1DOcclusionTest(bam_angle_t low, bam_angle_t high);

#endif /* __RGL_OCCLUDE_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
