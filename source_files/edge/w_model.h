//----------------------------------------------------------------------------
//  EDGE Model Management
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2008  The EDGE Team.
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

#ifndef __W_MODEL_H__
#define __W_MODEL_H__

#include "r_defs.h"

class md2_model_c;


#define MAX_MODEL_SKINS  10

class modeldef_c
{
public:
	// four letter model name (e.g. "TROO").
	char name[6];

	md2_model_c *model;

	const image_c *skins[MAX_MODEL_SKINS];

public:
	 modeldef_c(const char *_prefix);
	~modeldef_c();
};


/* Functions */

void W_InitModels(void);

void W_PrecacheModels(void);

modeldef_c *W_GetModel(int model_num);

// XXX W_GetModelSkin(int model_num, int skin_num);

#endif // __W_MODEL_H__

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
