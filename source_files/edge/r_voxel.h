//----------------------------------------------------------------------------
//  KVX/KV6 Voxels
//----------------------------------------------------------------------------
//
//  Copyright (c) 2023  The EDGE Team.
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

#ifndef __R_VOXEL_H__
#define __R_VOXEL_H__

#include "file.h"

#include "r_defs.h"
#include "p_mobj.h"


// opaque handle for rest of the engine
class vxl_model_c;


vxl_model_c *VXL_LoadModel(epi::file_c *f);

short VXL_FindFrame(vxl_model_c *md, const char *name);

void VXL_RenderModel(vxl_model_c *md, const image_c * skin_img, bool is_weapon,
		             int frame, float x, float y, float z, mobj_t *mo,
					 region_properties_t *props,
					 float scale, float aspect, float bias);

void VXL_RenderModel_2D(vxl_model_c *md, const image_c * skin_img, int frame,
		                float x, float y, float xscale, float yscale,
		                const mobjtype_c *info);

#endif /* __R_VOXEL_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
