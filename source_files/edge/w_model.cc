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

// EPI
#include "str_util.h"

#include "e_main.h"
#include "r_image.h"
#include "r_md2.h"
#include "r_mdl.h"
#include "r_voxel.h"
#include "r_things.h"
#include "w_files.h"
#include "w_model.h"
#include "w_wad.h"

#include "p_local.h"  // mobjlisthead


// Model storage
static modeldef_c **models;
static int nummodels = 0;


modeldef_c::modeldef_c(const char *_prefix) : md2_model(nullptr), mdl_model(nullptr)
{
	strcpy(name, _prefix);

	for (int i=0; i < MAX_MODEL_SKINS; i++)
		skins[i] = nullptr;
}

modeldef_c::~modeldef_c()
{
	// FIXME: delete model;

	// TODO: free the skins
}

static void FindModelFrameNames(md2_model_c *md, int model_num)
{
	int missing = 0;

	I_Debugf("Finding frame names for model '%s'...\n",
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
		I_Error("Failed to find %d frames for model '%s' (see EDGE.LOG)\n",
				missing, ddf_model_names[model_num].c_str());
}

static void FindModelFrameNames(mdl_model_c *md, int model_num)
{
	int missing = 0;

	I_Debugf("Finding frame names for model '%s'...\n",
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

		st->frame = MDL_FindFrame(md, st->model_frame);

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
		I_Error("Failed to find %d frames for model '%s' (see EDGE.LOG)\n",
				missing, ddf_model_names[model_num].c_str());
}

modeldef_c *LoadModelFromLump(int model_num)
{
	std::string basename = ddf_model_names[model_num];

	modeldef_c *def = new modeldef_c(basename.c_str());

	std::string lumpname; // will also be used as a pack file test
	std::string skinname; // same

	epi::file_c *f = nullptr;

	bool pack_file = false;

	// try MD3 first, then MD2, then MDL, then voxels
	lumpname = epi::STR_Format("models/%s.md3", basename.c_str());
	f = W_OpenPackFile(lumpname);
	if (f)
	{
		I_Debugf("Loading MD3 model from pack file : %s\n", lumpname.c_str());
		def->md2_model = MD3_LoadModel(f);
		pack_file = true;
	}
	else
	{
		lumpname = epi::STR_Format("%sMD3", basename.c_str());
		if (W_CheckNumForName(lumpname.c_str()) >= 0)
		{
			I_Debugf("Loading MD3 model from lump : %s\n", lumpname.c_str());
			f = W_OpenLump(lumpname.c_str());
			SYS_ASSERT(f);
			def->md2_model = MD3_LoadModel(f);
		}
	}
	if (!f)
	{
		lumpname = epi::STR_Format("models/%s.md2", basename.c_str());
		f = W_OpenPackFile(lumpname);
		if (f)
		{
			I_Debugf("Loading MD2 model from pack file : %s\n", lumpname.c_str());
			def->md2_model = MD2_LoadModel(f);
			pack_file = true;
		}
		else
		{
			lumpname = epi::STR_Format("%sMD2", basename.c_str());
			if (W_CheckNumForName(lumpname.c_str()) >= 0)
			{
				I_Debugf("Loading MD2 model from lump : %s\n", lumpname.c_str());

				f = W_OpenLump(lumpname.c_str());
				SYS_ASSERT(f);
				def->md2_model = MD2_LoadModel(f);
			}
		}
	}
	if (!f)
	{
		lumpname = epi::STR_Format("models/%s.mdl", basename.c_str());
		f = W_OpenPackFile(lumpname);
		if (f)
		{
			I_Debugf("Loading MDL model from pack file : %s\n", lumpname.c_str());
			def->mdl_model = MDL_LoadModel(f);
			pack_file = true;
		}
		else
		{
			lumpname = epi::STR_Format("%sMDL", basename.c_str());
			if (W_CheckNumForName(lumpname.c_str()) >= 0)
			{
				I_Debugf("Loading MDL model from lump : %s\n", lumpname.c_str());

				f = W_OpenLump(lumpname.c_str());
				SYS_ASSERT(f);
				def->mdl_model = MDL_LoadModel(f);
			}
		}
	}
	if (!f)
	{
		lumpname = epi::STR_Format("voxels/%s.vxl", basename.c_str());
		f = W_OpenPackFile(lumpname);
		if (f)
		{
			I_Debugf("Loading VXL model from pack file : %s\n", lumpname.c_str());
			def->vxl_model = VXL_LoadModel(f, basename.c_str());
			pack_file = true;
		}
		if (!f)
		{
			lumpname = epi::STR_Format("voxels/%s.kv6", basename.c_str());
			f = W_OpenPackFile(lumpname);
			if (f)
			{
				I_Debugf("Loading KV6 model from pack file : %s\n", lumpname.c_str());
				def->vxl_model = VXL_LoadModel(f, basename.c_str());
				pack_file = true;
			}
		}
		if (!f)
		{
			lumpname = epi::STR_Format("voxels/%s.kvx", basename.c_str());
			f = W_OpenPackFile(lumpname);
			if (f)
			{
				I_Debugf("Loading KVX model from pack file : %s\n", lumpname.c_str());
				def->vxl_model = VXL_LoadModel(f, basename.c_str());
				pack_file = true;
			}
		}
		if (!f)
		{
			lumpname = epi::STR_Format("%sVXL", basename.c_str());
			if (W_CheckNumForName(lumpname.c_str()) >= 0)
			{
				I_Debugf("Loading voxel model from lump : %s\n", lumpname.c_str());

				f = W_OpenLump(lumpname.c_str());
				SYS_ASSERT(f);
				def->vxl_model = VXL_LoadModel(f, basename.c_str());
			}
		}
	}

	if (! f)
		I_Error("Missing model lump for: %s\n!", basename.c_str());

	SYS_ASSERT(def->md2_model || def->mdl_model || def->vxl_model);

	// close the lump
	delete f;

	if (def->md2_model)
	{
		for (int i=0; i < 10; i++)
		{
			if (pack_file)
			{
				skinname = epi::STR_Format("%s%d",basename.c_str(), i);
				def->skins[i] = W_ImageLookup(skinname.c_str(), INS_Sprite, ILF_Null);
			}
			else
			{
				skinname = epi::STR_Format("%sSKIN%d", basename.c_str(), i);
				def->skins[i] = W_ImageLookup(skinname.c_str(), INS_Sprite, ILF_Null);		
			}
		}
	}

	// need at least one skin (MD2/MD3 only; MDLs and VXLs should have them baked in already)
	if (def->md2_model)
	{
		if (! def->skins[1])
		{
			if (pack_file)
				I_Error("Missing model skin: %s1\n", basename.c_str());
			else
				I_Error("Missing model skin: %sSKN1\n", basename.c_str());
		}
	}

	if (def->md2_model)
		FindModelFrameNames(def->md2_model, model_num);

	if (def->mdl_model)
		FindModelFrameNames(def->mdl_model, model_num);

	return def;
}


void W_InitModels(void)
{
	nummodels = (int)ddf_model_names.size();

	SYS_ASSERT(nummodels >= 1);  // at least SPR_NULL

	E_ProgressMessage("Setting up models...");

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

		const char *model_name = nullptr;

		if (model >= 1 && model < nummodels)
			model_name = ddf_model_names[model].c_str();

		if (model_name)
		{
			for (int i = 1 ; i < nummodels ; i++)
			{
				if (epi::strncmp(model_name, ddf_model_names[i].c_str(), 4) == 0)
					model_present[i] = 1;
			}
		}
	}

	// mark all weapons
	for (int k = 1 ; k < num_states ; k++)
	{
		if ((states[k].flags & (SFF_Weapon | SFF_Model)) != (SFF_Weapon | SFF_Model))
			continue;

		int model = states[k].sprite;

		const char *model_name = nullptr;

		if (model >= 1 && model < nummodels)
			model_name = ddf_model_names[model].c_str();

		if (model_name)
		{
			for (int i = 1 ; i < nummodels ; i++)
			{
				if (epi::strncmp(model_name, ddf_model_names[i].c_str(), 4) == 0)
					model_present[i] = 1;
			}
		}
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
