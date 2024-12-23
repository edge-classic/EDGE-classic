//----------------------------------------------------------------------------
//  MDL Models
//----------------------------------------------------------------------------
//
//  Copyright (c) 2023-2024 The EDGE Team.
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

#include "epi_file.h"
#include "p_mobj.h"
#include "r_defs.h"

// opaque handle for rest of the engine
class MDLModel;

MDLModel *MDLLoad(epi::File *f);

short MDLFindFrame(MDLModel *md, const char *name);

void MDLRenderModel(MDLModel *md, bool is_weapon, int frame1, int frame2, float lerp, float x,
                    float y, float z, MapObject *mo, RegionProperties *props, float scale, float aspect, float bias,
                    int rotation);

void MDLRenderModel2D(MDLModel *md, int frame, float x, float y, float xscale, float yscale,
                      const MapObjectDefinition *info);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
