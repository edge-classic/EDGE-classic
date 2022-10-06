//----------------------------------------------------------------------------
//  Texture Conversion and Caching code
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
//
// This module converts image lumps on disk to usable structures, and also
// provides a caching system for these.
//
// -ES- 2000/02/12 Written.

#include "i_defs.h"

#include "endianess.h"

#include "dm_structs.h"
#include "e_search.h"
#include "e_main.h"
#include "r_image.h"
#include "w_files.h"
#include "w_texture.h"
#include "w_wad.h"
#include "z_zone.h"


class texture_set_c
{
public:
	texture_set_c(int _num) : num_tex(_num)
	{
		textures = new texturedef_t*[num_tex];
	}

	~texture_set_c()
	{
		delete[] textures;
	}

	texturedef_t ** textures;
	int num_tex;
};


static std::vector<texture_set_c *> tex_sets;


//
// InstallTextureLumps
//
// -ACB- 1998/09/09 Removed the Doom II SkyName change: unnecessary and not DDF.
//                  Reformatted and cleaned up.
//
static void InstallTextureLumps(int file, const wadtex_resource_c *WT)
{
	int i;
	int maxoff;
	int maxoff2;
	int numtextures1;
	int numtextures2;

	const int *maptex;
	const int *maptex1;
	const int *maptex2;
	const int *directory;

	// Load the patch names from pnames.lmp.
	const char *names = (const char*)W_CacheLumpNum(WT->pnames);
	int nummappatches = EPI_LE_S32(*((const int *)names));  // Eww...

	const char *name_p = names + 4;

	int *patchlookup = new int[nummappatches+1];

	for (i = 0; i < nummappatches; i++)
	{
		char name[16];

		Z_StrNCpy(name, (const char*)(name_p + i * 8), 8);

		patchlookup[i] = W_CheckNumForTexPatch(name);
	}

	W_DoneWithLump(names);

	//
	// Load the map texture definitions from textures.lmp.
	//
	// The data is contained in one or two lumps:
	//   TEXTURE1 for shareware
	//   TEXTURE2 for commercial.
	//
	maptex = maptex1 = (const int*)W_CacheLumpNum(WT->texture1);
	numtextures1 = EPI_LE_S32(*maptex);
	maxoff = W_LumpLength(WT->texture1);
	directory = maptex + 1;

	if (WT->texture2 != -1)
	{
		maptex2 = (const int*)W_CacheLumpNum(WT->texture2);
		numtextures2 = EPI_LE_S32(*maptex2);
		maxoff2 = W_LumpLength(WT->texture2);
	}
	else
	{
		maptex2 = NULL;
		numtextures2 = 0;
		maxoff2 = 0;
	}

	texture_set_c *cur_set = new texture_set_c(numtextures1 + numtextures2);

	tex_sets.push_back(cur_set);

	for (i = 0; i < cur_set->num_tex; i++, directory++)
	{
		if (i == numtextures1)
		{
			// Start looking in second texture file.
			maptex = maptex2;
			maxoff = maxoff2;
			directory = maptex + 1;
		}

		int offset = EPI_LE_S32(*directory);
		if (offset < 0 || offset > maxoff)
			I_Error("W_InitTextures: bad texture directory");

		const raw_texture_t *mtexture =
			(const raw_texture_t *) ((const byte *) maptex + offset);

		// -ES- 2000/02/10 Texture must have patches.
		int patchcount = EPI_LE_S16(mtexture->patch_count);
		
		//Lobo 2021: Changed this to a warning. Allows us to run several DBPs
		// which have this issue
		if (!patchcount)
		{
			I_Warning("W_InitTextures: Texture '%.8s' has no patches\n", mtexture->name);
			//I_Error("W_InitTextures: Texture '%.8s' has no patches", mtexture->name);
			patchcount = 0; //mark it as a dud
		} 

		int width = EPI_LE_S16(mtexture->width);
		if (width == 0)
			I_Error("W_InitTextures: Texture '%.8s' has zero width", mtexture->name);

		// -ES- Allocate texture, patches and columnlump/ofs in one big chunk
		int base_size = sizeof(texturedef_t) + sizeof(texpatch_t) * (patchcount - 1);

		texturedef_t * texture = (texturedef_t *) Z_Malloc(base_size + width * (sizeof(byte) + sizeof(short)));
		cur_set->textures[i] = texture;

		byte *base = (byte *)texture + base_size;

		texture->columnofs = (unsigned short *)base;

		texture->width = width;
		texture->height = EPI_LE_S16(mtexture->height);
		texture->scale_x = mtexture->scale_x;
		texture->scale_y = mtexture->scale_y;
		texture->file = file;
		texture->palette_lump = WT->palette;
		texture->patchcount = patchcount;

		Z_StrNCpy(texture->name, mtexture->name, 8);
		for (size_t i=0;i<strlen(texture->name);i++) {
			texture->name[i] = toupper(texture->name[i]);
		}

		const raw_patchdef_t *mpatch = &mtexture->patches[0];
		texpatch_t *patch = &texture->patches[0];

		bool is_sky = (strncmp("SKY", texture->name, 3) == 0);

		for (int k = 0; k < texture->patchcount; k++, mpatch++, patch++)
		{
			int pname = EPI_LE_S16(mpatch->pname);

			patch->originx = EPI_LE_S16(mpatch->x_origin);
			patch->originy = EPI_LE_S16(mpatch->y_origin);
			patch->patch   = patchlookup[pname];

			// work-around for strange Y offset in SKY1 of DOOM 1 
			if (is_sky && patch->originy < 0)
				patch->originy = 0;

			if (patch->patch == -1)
			{
				I_Warning("Missing patch '%.8s' in texture \'%.8s\'\n",
						  name_p + pname*8, texture->name);

				// mark texture as a dud
				texture->patchcount = 0;
				break;
			}
		}
	}

	// free stuff
	W_DoneWithLump(maptex1);

	if (maptex2)
		W_DoneWithLump(maptex2);
	
	delete[] patchlookup;
}

//
// W_InitTextures
//
// Initialises the texture list with the textures from the world map.
//
// -ACB- 1998/09/09 Fixed the Display routine from display rubbish.
//
void W_InitTextures(void)
{
	int num_files = W_GetNumFiles();
	int j, t, file;

	texturedef_t ** textures = NULL;
	texturedef_t ** cur;
	int numtextures = 0;

	I_Printf("Initializing Textures...\n");

	SYS_ASSERT(tex_sets.empty());

	// iterate over each file, creating our sets of textures
	// -ACB- 1998/09/09 Removed the Doom II SkyName change: unnecessary and not DDF.

	for (file=0; file < num_files; file++)
	{
		wadtex_resource_c WT;

		W_GetTextureLumps(file, &WT);

		if (WT.pnames < 0)
			continue;

		if (WT.texture1 < 0 && WT.texture2 >= 0)
		{
			WT.texture1 = WT.texture2;
			WT.texture2 = -1;
		}

		if (WT.texture1 < 0)
			continue;

		InstallTextureLumps(file, &WT);
	}

	if (tex_sets.empty())
		I_Error("No textures found !  Make sure the chosen IWAD is valid.\n");

	// now clump all of the texturedefs together and sort 'em, primarily
	// by increasing name, secondarily by increasing file number
	// (measure of newness).  We ignore "dud" textures (missing
	// patches).

	for (j=0; j < (int)tex_sets.size(); j++)
		numtextures += tex_sets[j]->num_tex;

	textures = cur = new texturedef_t*[numtextures];

	for (j=0; j < (int)tex_sets.size(); j++)
	{
		texture_set_c *set = tex_sets[j];

		for (t=0; t < set->num_tex; t++)
			if (set->textures[t]->patchcount > 0)
				*cur++ = set->textures[t];
	}

	numtextures = cur - textures;

#define CMP(a, b)  \
	(strcmp(a->name, b->name) < 0 || \
	 (strcmp(a->name, b->name) == 0 && a->file < b->file))
		QSORT(texturedef_t *, textures, numtextures, CUTOFF);
#undef CMP

	// remove duplicate names.  Because the QSORT took newness into
	// account, only the last entry in a run of identically named
	// textures needs to be kept.

	for (j=1; j < numtextures; j++)
	{
		texturedef_t * a = textures[j - 1];
		texturedef_t * b = textures[j];

		if (strcmp(a->name, b->name) == 0)
		{
			textures[j - 1] = NULL;
		}
	}

#if 0  // DEBUGGING
	for (j=0; j < numtextures; j++)
	{
		if (textures[j] == NULL)
		{
			L_WriteDebug("TEXTURE #%d was a dupicate\n", j);
			continue;
		}
		L_WriteDebug("TEXTURE #%d:  name=[%s]  file=%d  size=%dx%d\n", j,
				textures[j]->name, textures[j]->file,
				textures[j]->width, textures[j]->height);
	}
#endif

	W_ImageCreateTextures(textures, numtextures); 

	// free pointer array.  We need to keep the definitions in memory
	// for (a) the image system and (b) texture anims.
	delete [] textures;
}

//
// W_FindTextureSequence
//
// Returns the set number containing the texture names (with the
// offset values updated to the indexes), or -1 if none could be
// found.  Used by animation code.
//
// Note: search is from latest set to earliest set.
// 
int W_FindTextureSequence(const char *start, const char *end,
		int *s_offset, int *e_offset)
{
	int i, j;

	for (i = (int)tex_sets.size() - 1; i >= 0; i--)
	{
		// look for start name
		for (j=0; j < tex_sets[i]->num_tex; j++)
			if (stricmp(start, tex_sets[i]->textures[j]->name) == 0)
				break;

		if (j >= tex_sets[i]->num_tex)
			continue;

		(*s_offset) = j;

		// look for end name
		for (j++; j < tex_sets[i]->num_tex; j++)
		{
			if (stricmp(end, tex_sets[i]->textures[j]->name) == 0)
			{
				(*e_offset) = j;
				return i;
			}
		}
	}

	// not found
	return -1;
}

//
// W_TextureNameInSet
//
const char *W_TextureNameInSet(int set, int offset)
{
	SYS_ASSERT(0 <= set && set < (int)tex_sets.size());
	SYS_ASSERT(0 <= offset && offset < tex_sets[set]->num_tex);

	return tex_sets[set]->textures[offset]->name;
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
