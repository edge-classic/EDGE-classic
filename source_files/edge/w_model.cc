//----------------------------------------------------------------------------
//  EDGE Model Management
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

#include "i_defs.h"
#include "i_defs_gl.h"

#include "e_main.h"
#include "r_image.h"
#include "r_md2.h"
#include "r_things.h"
#include "w_model.h"
#include "w_wad.h"

#include "p_local.h"  // mobjlisthead


// Model storage
static modeldef_c **models;
static int nummodels = 0;


modeldef_c::modeldef_c(const char *_prefix) : model(NULL)
{
	strcpy(name, _prefix);

	for (int i=0; i < MAX_MODEL_SKINS; i++)
		skins[i] = NULL;
}

modeldef_c::~modeldef_c()
{
	// FIXME: delete model;

	// TODO: free the skins
}


static void FindModelFrameNames(md2_model_c *md, int model_num)
{
	int missing = 0;

	I_Printf("Finding frame names for model '%sMD2'...\n",
			 ddf_model_names[model_num].c_str());

	for (int stnum = 1; stnum < num_states; stnum++)
	{
		state_t *st = &states[stnum];

		if (st->sprite != model_num)
			continue;

		if (! (st->flags & SFF_Model))
			continue;

		if (! (st->flags & SFF_Unmapped))
			continue;

		SYS_ASSERT(st->model_frame);

		st->frame = MD2_FindFrame(md, st->model_frame);

		if (st->frame >= 0)
		{
			st->flags &= ~SFF_Unmapped;
		}
		else
		{
			missing++;
			I_Printf("-- no such frame '%s'\n", st->model_frame);
		}
	}

	if (missing > 0)
		I_Error("Failed to find %d frames for model '%sMD2' (see EDGE.LOG)\n",
				missing, ddf_model_names[model_num].c_str());
}


modeldef_c *LoadModelFromLump(int model_num)
{
	const char *basename = ddf_model_names[model_num].c_str();

	modeldef_c *def = new modeldef_c(basename);

	char lumpname[16];
	char skinname[16];

	sprintf(lumpname, "%sMD2", basename);

	I_Debugf("Loading model from lump : %s\n", lumpname);

	epi::file_c *f = W_OpenLump(lumpname);
	if (! f)
		I_Error("Missing model lump: %s\n", lumpname);

	def->model = MD2_LoadModel(f);
	SYS_ASSERT(def->model);

	// close the lump
	delete f;

	for (int i=0; i < 10; i++)
	{
		sprintf(skinname, "%sSKN%d", basename, i);

		def->skins[i] = W_ImageLookup(skinname, INS_Sprite, ILF_Null);
	}

	// need at least one skin
	if (! def->skins[1])
		I_Error("Missing model skin: %sSKN1\n", basename);

	FindModelFrameNames(def->model, model_num);

	return def;
}


void W_InitModels(void)
{
	nummodels = (int)ddf_model_names.size();

	SYS_ASSERT(nummodels >= 1);  // at least SPR_NULL

	I_Printf("W_InitModels: Setting up\n");

	models = new modeldef_c * [nummodels];

	for (int i=0; i < nummodels; i++)
		models[i] = NULL;
}


modeldef_c *W_GetModel(int model_num)
{
	// model_num comes from the 'sprite' field of state_t, and
	// is also an index into ddf_model_names vector.

	SYS_ASSERT(model_num > 0);
	SYS_ASSERT(model_num < nummodels);

	if (! models[model_num])
	{
		models[model_num] = LoadModelFromLump(model_num);
	}

	return models[model_num];
}


void W_PrecacheModels(void)
{
	if (nummodels <= 0)
		return;

	byte *model_present = new byte[nummodels];
	memset(model_present, 0, nummodels);

	// mark all monsters (etc) in the level
	for (mobj_t * mo = mobjlisthead ; mo ; mo = mo->next)
	{
		SYS_ASSERT(mo->state);

		if (! (mo->state->flags & SFF_Model))
			continue;

		int model = mo->state->sprite;

		if (model >= 1 && model < nummodels)
			model_present[model] = 1;
	}

	// mark all weapons
	for (int k = 1 ; k < num_states ; k++)
	{
		if ((states[k].flags & (SFF_Weapon | SFF_Model)) != (SFF_Weapon | SFF_Model))
			continue;

		int model = states[k].sprite;

		if (model >= 1 && model < nummodels)
			model_present[model] = 1;
	}

	for (int i = 1 ; i < nummodels ; i++)  // ignore SPR_NULL
	{
		if (model_present[i])
		{
			I_Debugf("Precaching model: %s\n", ddf_model_names[i].c_str());

			modeldef_c *def = W_GetModel(i);

			// precache skins too
			for (int n = 0 ; n < 10 ; n++)
			{
				if (def && def->skins[n])
					W_ImagePreCache(def->skins[n]);
			}
		}
	}

	delete[] model_present;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
