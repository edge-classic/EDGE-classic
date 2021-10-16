//----------------------------------------------------------------------------
//  EDGE Sprite Management
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
//  Based on the DOOM source code, released by Id Software under the
//  following copyright:
//
//    Copyright (C) 1993-1996 by id Software, Inc.
//
//----------------------------------------------------------------------------
//
// -KM- 1998/07/26 Replaced #ifdef RANGECHECK with #ifdef DEVELOPERS
// -KM- 1998/09/27 Dynamic colourmaps
// -AJA- 1999/07/12: Now uses colmap.ddf.
//

#include "i_defs.h"

#include "e_main.h"
#include "e_search.h"
#include "r_image.h"
#include "r_things.h"
#include "w_sprite.h"
#include "w_wad.h"

#include "p_local.h"  // mobjlisthead


//
// A sprite definition: a number of animation frames.
//
class spritedef_c
{
public:
	// four letter sprite name (e.g. "TROO").
	char name[6];
  
    // total number of frames.  Zero for missing sprites.
	int numframes;

	// sprite frames.
	spriteframe_c *frames;

public:
	spritedef_c(const char *_name) : numframes(0), frames(NULL)
	{
		strcpy(name, _name);
	}

	~spritedef_c()
	{
		// TODO: free the frames
	}

	bool HasWeapon() const
	{
		for (int i = 0 ; i < numframes ; i++)
			if (frames[i].is_weapon)
				return true;

		return false;
	}
};


//----------------------------------------------------------------------------

//
// Sprite rotation 0 is facing the viewer,
//  rotation 1 is one angle turn CLOCKWISE around the axis.
// This is not the same as the angle,
//  which increases counter clockwise (protractor).
// There was a lot of stuff grabbed wrong, so I changed it...
//

// Sprite definitions
static spritedef_c **sprites;
static int numsprites = 0;

// Sorted map of sprite defs.  Only used during initialisation.
static spritedef_c ** sprite_map = NULL;
static int sprite_map_len;


//
// SPRITE LOADING FUNCTIONS
//

static spriteframe_c *WhatFrame(spritedef_c *def, const char *name, int pos)
{
	char frame_ch = name[pos];

	int index;

	if ('A' <= frame_ch && frame_ch <= 'Z')
	{
		index = (frame_ch - 'A');
	}
	else switch (frame_ch)
	{
		case '[':  index = 26; break;
		case '\\': index = 27; break;
		case ']':  index = 28; break;
		case '^':  index = 29; break;
		case '_':  index = 30; break;

		default:
		   I_Warning("Sprite lump %s has illegal frame.\n", name);
		   return NULL;
	}

	SYS_ASSERT(index >= 0);

	// ignore frames larger than what is used in DDF
	if (index >= def->numframes)
		return NULL;

	return & def->frames[index];
}

static void SetExtendedRots(spriteframe_c *frame)
{
	frame->rots = 16;

	for (int i = 7; i >= 1; i--)
	{
		frame->images[2*i] = frame->images[i];
		frame->flip[2*i]   = frame->flip[i];
	}

	for (int k = 1; k <= 15; k += 2)
	{
		frame->images[k] = NULL;
		frame->flip[k]   = 0;
	}
}

static int WhatRot(spriteframe_c *frame, const char *name, int pos )
{
	char rot_ch = name[pos];

	int rot;

	// NOTE: rotations 9 and A-G are EDGE specific.

	if ('0' <= rot_ch && rot_ch <= '9')
		rot = (rot_ch - '0');
	else if ('A' <= rot_ch && rot_ch <= 'G')
		rot = (rot_ch - 'A') + 10;
	else
	{
		I_Warning("Sprite lump %s has illegal rotation.\n", name);
		return -1;
	}

	if (frame->rots == 0)
		frame->rots = 1;

	if (rot >= 1 && frame->rots == 1)
		frame->rots = 8;

	if (rot >= 9 && frame->rots != 16)
		SetExtendedRots(frame);

	switch (frame->rots)
	{
		case 1:
			return 0;

		case 8:
			return rot-1;

		case 16:
			if (rot >= 9)
				return 1 + (rot-9) * 2;
			else
				return 0 + (rot-1) * 2;

		default:
			I_Error("INTERNAL ERROR: frame->rots = %d\n", frame->rots);
			return -1; /* NOT REACHED */
	}
}

static void InstallSpriteLump(spritedef_c *def, int lump,
    const char *lumpname, int pos, byte flip)
{
	spriteframe_c *frame = WhatFrame(def, lumpname, pos);
	if (! frame)
		return;

	// don't disturb any frames already loaded
	if (frame->finished)
		return;

	int rot = WhatRot(frame, lumpname, pos+1);
	if (rot < 0)
		return;

	SYS_ASSERT(0 <= rot && rot < 16);

	if (frame->images[rot])
	{
		I_Warning("Sprite %s has two lumps mapped to it (frame %c).\n", 
				lumpname, lumpname[pos]);
		return;
	}

	frame->images[rot] = W_ImageCreateSprite(lumpname, lump,
		frame->is_weapon);

	frame->flip[rot] = flip;
}

static void InstallSpriteImage(spritedef_c *def, const image_c *img,
    const char *img_name, int pos, byte flip)
{
	spriteframe_c *frame = WhatFrame(def, img_name, pos);
	if (! frame)
		return;

	// don't disturb any frames already loaded
	if (frame->finished)
		return;

	int rot = WhatRot(frame, img_name, pos+1);
	if (rot < 0)
		return;

	if (frame->images[rot])
	{
		I_Warning("Sprite %s has two images mapped to it (frame %c)\n", 
				img_name, img_name[pos]);
		return;
	}

	frame->images[rot] = img;
	frame->flip[rot] = flip;
}

//
// FillSpriteFrames
//
static void FillSpriteFrames(int file, int prog_base, int prog_total)
{
	epi::u32array_c& lumps = W_GetListLumps(file, LMPLST_Sprites);
	int lumpnum = lumps.GetSize();

	if (lumpnum == 0)
		return;

	// check all lumps for prefixes matching the ones in the sprite
	// list.  Both lists have already been sorted to make this as fast
	// as possible.

	int S = 0, L = 0;

	while (S < sprite_map_len && L < lumpnum)
	{
		const char *sprname  = sprite_map[S]->name;
		const char *lumpname = W_GetLumpName(lumps[L]);

		// ignore model skins
		if (lumpname[4] == 'S' && lumpname[5] == 'K' && lumpname[6] == 'N')
		{
			L++; continue;
		}

		if (strlen(lumpname) != 6 && strlen(lumpname) != 8)
		{
			I_Warning("Sprite name %s has illegal length.\n", lumpname);
			L++; continue;
		}

		int comp = strncmp(sprname, lumpname, 4);

		if (comp < 0)  // S < L
		{
			S++; continue;
		}
		else if (comp > 0)  // S > L
		{
			L++; continue;
		}

		// we have a match
		InstallSpriteLump(sprite_map[S], lumps[L], lumpname, 4, 0);

		if (lumpname[6])
			InstallSpriteLump(sprite_map[S], lumps[L], lumpname, 6, 1);

		L++;

		// update startup progress bar
		E_LocalProgress(prog_base + L * 100 / lumpnum, prog_total);
	}
}

//
// FillSpriteFramesUser
//
// Like the above, but made especially for IMAGES.DDF.
//
static void FillSpriteFramesUser(int prog_base, int prog_total)
{
	int img_num;
	const image_c ** images = W_ImageGetUserSprites(&img_num);

	if (img_num == 0)
		return;

	SYS_ASSERT(images);

	int S = 0, L = 0;

	while (S < sprite_map_len && L < img_num)
	{
		const char *sprname  = sprite_map[S]->name;
		const char *img_name = W_ImageGetName(images[L]);

		// ignore model skins
		if (img_name[4] == 'S' && img_name[5] == 'K' && img_name[6] == 'N')
		{
			L++; continue;
		}

		if (strlen(img_name) != 6 && strlen(img_name) != 8)
		{
			I_Warning("Sprite name %s (IMAGES.DDF) has illegal length.\n", img_name);
			L++; continue;
		}

		int comp = strncmp(sprname, img_name, 4);

		if (comp < 0)  // S < L
		{
			S++; continue;
		}
		else if (comp > 0)  // S > L
		{
			L++; continue;
		}

		// we have a match
		InstallSpriteImage(sprite_map[S], images[L], img_name, 4, 0);

		if (img_name[6])
			InstallSpriteImage(sprite_map[S], images[L], img_name, 6, 1);

		L++;

		// update startup progress bar
		E_LocalProgress(prog_base + L * 100 / img_num, prog_total);
	}

	delete[] images;
}

static void MarkCompletedFrames(void)
{
	int src, dst;

	for (src = dst = 0; src < sprite_map_len; src++)
	{
		spritedef_c *def = sprite_map[src];

		int finish_num = 0;

		for (int f = 0; f < def->numframes; f++)
		{
			char frame_ch = 'A' + f;
			spriteframe_c *frame = def->frames + f;

			if (frame->finished)
			{
				finish_num++;
				continue;
			}

			int rot_count = 0;

			// check if all image pointers are NULL
			for (int i=0; i < frame->rots; i++)
				rot_count += frame->images[i] ? 1 : 0;

			if (rot_count == 0)
				continue;

			frame->finished = 1;
			finish_num++;

			if (rot_count < frame->rots)
				I_Warning("Sprite %s:%c is missing rotations (%d of %d).\n",
					def->name, frame_ch, frame->rots - rot_count, frame->rots);
		}

		// remove complete sprites from sprite_map
		if (finish_num == def->numframes)
			continue;

		sprite_map[dst++] = def;
	}

	sprite_map_len = dst;
}

// show warnings for missing patches
static void CheckSpriteFrames(spritedef_c *def)
{
	int missing = 0;

	for (int i = 0; i < def->numframes; i++)
		if (! def->frames[i].finished)
		{
			I_Debugf("Frame %d/%d is not finished\n", 1+i, def->numframes);
			missing++;
		}

	if (missing > 0 && missing < def->numframes)
		I_Warning("Missing %d frames in sprite: %s\n", missing, def->name);

	// free some memory for completely missing sprites
	if (missing == def->numframes)
	{
		delete[] def->frames;

		def->numframes = 0;
		def->frames = NULL;
	}
}

//
// W_InitSprites
//
// Use the sprite lists in the WAD (S_START..S_END) to flesh out the
// known sprite definitions (global `sprites' array, created while
// parsing DDF) with images.
//
// Checking for missing frames still done at run time.
// 
// -AJA- 2001/02/01: rewrote this stuff.
// 
void W_InitSprites(void)
{
	numsprites = (int)ddf_sprite_names.size();

	if (numsprites <= 1)
		I_Error("Missing sprite definitions !!\n");

	I_Printf("W_InitSprites: Finding sprite patches\n");

	// 1. Allocate sprite definitions (ignore NULL sprite, #0)
	
	sprites = new spritedef_c* [numsprites];
	sprites[SPR_NULL] = NULL;

	for (int i=1; i < numsprites; i++)
	{
		const char *name = ddf_sprite_names[i].c_str();

		SYS_ASSERT(strlen(name) == 4);

		sprites[i] = new spritedef_c(name);
	}

	// 2. Scan the state table, count frames 

	for (int stnum = 1; stnum < num_states; stnum++)
	{
		state_t *st = &states[stnum];

		if (st->flags & SFF_Model)
			continue;

		if (st->sprite == SPR_NULL)
			continue;

		spritedef_c *def = sprites[st->sprite];

		if (def->numframes < st->frame+1)
			def->numframes = st->frame+1;
	}

	// 3. Allocate frames

	for (int k=1; k < numsprites; k++)
	{
		spritedef_c *def = sprites[k];

		SYS_ASSERT(def->numframes > 0);

		def->frames = new spriteframe_c[def->numframes];
	}

	// 4. Mark weapon frames

	for (int st_kk = 1; st_kk < num_states; st_kk++)
	{
		state_t *st = &states[st_kk];

		if (st->flags & SFF_Model)
			continue;

		if (st->sprite == SPR_NULL)
			continue;

		spritedef_c *def = sprites[st->sprite];

		if (st->flags & SFF_Weapon)
			def->frames[st->frame].is_weapon = true;
	}

	// 5. Fill in frames using wad lumps + images.ddf
	
	// create a sorted list (ignore NULL entry, #0)
	sprite_map_len = numsprites - 1;

	sprite_map = new spritedef_c* [sprite_map_len];

	for (int n=0; n < sprite_map_len; n++)
		sprite_map[n] = sprites[n + 1];

#define CMP(a, b)  (strcmp(a->name, b->name) < 0)
	QSORT(spritedef_c *, sprite_map, sprite_map_len, CUTOFF);
#undef CMP

	// iterate over each file.  Order is important, we must go from
	// newest wad to oldest, so that new sprites override the old ones.
	// Completely finished sprites get removed from the list as we go.  
	//
	// NOTE WELL: override granularity is single frames.

	int numfiles = W_GetNumFiles();

	FillSpriteFramesUser(0, (numfiles+1) * 100);

	for (int file = numfiles - 1; file >= 0; file--)
	{
		FillSpriteFrames(file, (numfiles - file) * 100, (numfiles+1) * 100);
		MarkCompletedFrames();
	}

	// 6. Perform checks and free stuff

	for (int j=1; j < numsprites; j++)
		CheckSpriteFrames(sprites[j]);

	delete[] sprite_map;
	sprite_map = NULL;
}


bool W_CheckSpritesExist(const state_group_t& group)
{
	for (int g = 0; g < (int)group.size(); g++)
	{
		const state_range_t& range = group[g];

		for (int i = range.first; i <= range.last; i++)
		{
			if (states[i].sprite == SPR_NULL)
				continue;

			if (sprites[states[i].sprite]->frames > 0)
				return true;	

			// -AJA- only check one per group.  It _should_ check them all,
			//       however this maintains compatibility.
			break;
		}
	}

	return false;
}


spriteframe_c *W_GetSpriteFrame(int spr_num, int framenum)
{
	// spr_num comes from the 'sprite' field of state_t, and
	// is also an index into ddf_sprite_names vector.
	
	SYS_ASSERT(spr_num > 0);
	SYS_ASSERT(spr_num < numsprites);
	SYS_ASSERT(framenum >= 0);

	spritedef_c *def = sprites[spr_num];

	if (framenum >= def->numframes)
		return NULL;

	spriteframe_c *frame = &def->frames[framenum];

	if (!frame || !frame->finished)
		return NULL;

	return frame;
}


void W_PrecacheSprites(void)
{
	SYS_ASSERT(numsprites > 1);

	byte *sprite_present = new byte[numsprites];
	memset(sprite_present, 0, numsprites);

	for (mobj_t * mo = mobjlisthead ; mo ; mo = mo->next)
	{
		SYS_ASSERT(mo->state);

		if (mo->state->sprite < 1 || mo->state->sprite >= numsprites)
			continue;

		sprite_present[mo->state->sprite] = 1;
	}

	for (int i = 1 ; i < numsprites ; i++)  // ignore SPR_NULL
	{
		spritedef_c *def = sprites[i];

		const image_c *cur_image;
		const image_c *last_image = NULL;  // an optimisation

		if (def->numframes == 0)
			continue;

		// Note: all weapon sprites are pre-cached

		if (! (sprite_present[i] || def->HasWeapon()))
			continue;

		I_Debugf("Precaching sprite: %s\n", def->name);

		SYS_ASSERT(def->frames);

		for (int fr = 0 ; fr < def->numframes ; fr++)
		{
			if (! def->frames[fr].finished)
				continue;

			for (int rot = 0 ; rot < 16 ; rot++)
			{
				cur_image = def->frames[fr].images[rot];

				if (cur_image == NULL || cur_image == last_image)
					continue;

				W_ImagePreCache(cur_image);

				last_image = cur_image;
			}
		}
	}

	delete[] sprite_present;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
