//----------------------------------------------------------------------------
//  EDGE New SaveGame Handling (Globals)
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
// See the file "docs/save_sys.txt" for a complete description of the
// new savegame system.
//
// TODO HERE:
//   + implement ReadWADS and ReadVIEW.
//

#include "i_defs.h"

#include <limits.h>

#include "sv_chunk.h"
#include "sv_main.h"
#include "z_zone.h"

// forward decls:
static void GV_GetInt(const char *info, void *storage);
static void GV_GetString(const char *info, void *storage);
static void GV_GetCheckCRC(const char *info, void *storage);
static void GV_GetLevelFlags(const char *info, void *storage);
static void GV_GetImage(const char *info, void *storage);

static const char *GV_PutInt(void *storage);
static const char *GV_PutString(void *storage);
static const char *GV_PutCheckCRC(void *storage);
static const char *GV_PutLevelFlags(void *storage);
static const char *GV_PutImage(void *storage);


static saveglobals_t dummy_glob;
static saveglobals_t *cur_globs = NULL;

typedef struct
{
	// global name
	const char *name;

	// parse function.  `storage' is where the data should go (for
	// routines that don't modify the cur_globs structure directly).
	void (* parse_func)(const char *info, void *storage);

	// stringify function.  Return string must be freed.
	const char * (* stringify_func)(void *storage);

	// field offset (given as a pointer within dummy struct)
	const char *offset_p;
}
global_command_t;


#define GLOB_OFF(field)  ((const char *) &dummy_glob.field)

static const global_command_t global_commands[] =
{
	{ "GAME",  GV_GetString, GV_PutString, GLOB_OFF(game) },
	{ "LEVEL", GV_GetString, GV_PutString, GLOB_OFF(level) },
	{ "FLAGS", GV_GetLevelFlags, GV_PutLevelFlags, GLOB_OFF(flags) },
	{ "HUB_TAG", GV_GetInt, GV_PutInt, GLOB_OFF(hub_tag) },
	{ "HUB_FIRST", GV_GetString, GV_PutString, GLOB_OFF(hub_first) },

	{ "GRAVITY", GV_GetInt, GV_PutInt, GLOB_OFF(flags.menu_grav) },
	{ "LEVEL_TIME", GV_GetInt, GV_PutInt, GLOB_OFF(level_time) },
	{ "EXIT_TIME", GV_GetInt, GV_PutInt, GLOB_OFF(exit_time) },
	{ "P_RANDOM", GV_GetInt, GV_PutInt, GLOB_OFF(p_random) },

	{ "TOTAL_KILLS",   GV_GetInt, GV_PutInt, GLOB_OFF(total_kills) },
	{ "TOTAL_ITEMS",   GV_GetInt, GV_PutInt, GLOB_OFF(total_items) },
	{ "TOTAL_SECRETS", GV_GetInt, GV_PutInt, GLOB_OFF(total_secrets) },
	{ "CONSOLE_PLAYER", GV_GetInt, GV_PutInt, GLOB_OFF(console_player) },
	{ "SKILL", GV_GetInt, GV_PutInt, GLOB_OFF(skill) },
	{ "NETGAME", GV_GetInt, GV_PutInt, GLOB_OFF(netgame) },
	{ "SKY_IMAGE", GV_GetImage, GV_PutImage, GLOB_OFF(sky_image) },

	{ "DESCRIPTION", GV_GetString, GV_PutString, GLOB_OFF(description) },
	{ "DESC_DATE", GV_GetString, GV_PutString, GLOB_OFF(desc_date) },

	{ "MAPSECTOR", GV_GetCheckCRC, GV_PutCheckCRC, GLOB_OFF(mapsector) },
	{ "MAPLINE",   GV_GetCheckCRC, GV_PutCheckCRC, GLOB_OFF(mapline) },
	{ "MAPTHING",  GV_GetCheckCRC, GV_PutCheckCRC, GLOB_OFF(mapthing) },

	{ "RSCRIPT", GV_GetCheckCRC, GV_PutCheckCRC, GLOB_OFF(rscript) },
	{ "DDFATK",  GV_GetCheckCRC, GV_PutCheckCRC, GLOB_OFF(ddfatk) },
	{ "DDFGAME", GV_GetCheckCRC, GV_PutCheckCRC, GLOB_OFF(ddfgame) },
	{ "DDFLEVL", GV_GetCheckCRC, GV_PutCheckCRC, GLOB_OFF(ddflevl) },
	{ "DDFLINE", GV_GetCheckCRC, GV_PutCheckCRC, GLOB_OFF(ddfline) },
	{ "DDFSECT", GV_GetCheckCRC, GV_PutCheckCRC, GLOB_OFF(ddfsect) },
	{ "DDFMOBJ", GV_GetCheckCRC, GV_PutCheckCRC, GLOB_OFF(ddfmobj) },
	{ "DDFWEAP", GV_GetCheckCRC, GV_PutCheckCRC, GLOB_OFF(ddfweap) },

	{ NULL, NULL, 0 }
};


//----------------------------------------------------------------------------
//
//  PARSERS
//

static void GV_GetInt(const char *info, void *storage)
{
	int *dest = (int *)storage;

	SYS_ASSERT(info && storage);

	*dest = strtol(info, NULL, 0);
}

static void GV_GetString(const char *info, void *storage)
{
	char **dest = (char **)storage;

	SYS_ASSERT(info && storage);

	// free any previous string
	SV_FreeString(*dest);

	if (info[0] == 0)
		*dest = NULL;
	else
		*dest = (char *) SV_DupString(info);
}

static void GV_GetCheckCRC(const char *info, void *storage)
{
	crc_check_t *dest = (crc_check_t *)storage;

	SYS_ASSERT(info && storage);

	sscanf(info, "%d %u", &dest->count, &dest->crc);
}

static void GV_GetLevelFlags(const char *info, void *storage)
{
	gameflags_t *dest = (gameflags_t *)storage;
	int flags;

	SYS_ASSERT(info && storage);

	flags = strtol(info, NULL, 0);

	Z_Clear(dest, gameflags_t, 1);

#define HANDLE_FLAG(var, specflag)  \
	(var) = (flags & (specflag)) ? true : false;

	HANDLE_FLAG(dest->jump, MPF_Jumping);
	HANDLE_FLAG(dest->crouch, MPF_Crouching);
	HANDLE_FLAG(dest->mlook, MPF_Mlook);
	HANDLE_FLAG(dest->itemrespawn, MPF_ItemRespawn);
	HANDLE_FLAG(dest->fastparm, MPF_FastParm);
	HANDLE_FLAG(dest->true3dgameplay, MPF_True3D);
	HANDLE_FLAG(dest->more_blood, MPF_MoreBlood);
	HANDLE_FLAG(dest->cheats, MPF_Cheats);
	HANDLE_FLAG(dest->respawn, MPF_Respawn);
	HANDLE_FLAG(dest->res_respawn, MPF_ResRespawn);
	HANDLE_FLAG(dest->have_extra, MPF_Extras);
	HANDLE_FLAG(dest->limit_zoom, MPF_LimitZoom);
	HANDLE_FLAG(dest->shadows, MPF_Shadows);
	HANDLE_FLAG(dest->halos, MPF_Halos);
	HANDLE_FLAG(dest->kicking, MPF_Kicking);
	HANDLE_FLAG(dest->weapon_switch, MPF_WeaponSwitch);
	HANDLE_FLAG(dest->pass_missile, MPF_PassMissile);
	HANDLE_FLAG(dest->team_damage, MPF_TeamDamage);

#undef HANDLE_FLAG

	dest->edge_compat = (flags & MPF_BoomCompat) ? false : true;

	dest->autoaim = (flags & MPF_AutoAim) ? 
		((flags & MPF_AutoAimMlook) ? AA_MLOOK : AA_ON) : AA_OFF;
}

static void GV_GetImage(const char *info, void *storage)
{
	// based on SR_LevelGetImage...

	const image_c ** dest = (const image_c **)storage;

	SYS_ASSERT(info && storage);

	if (info[0] == 0)
	{
		(*dest) = NULL;
		return;
	}

	if (info[1] != ':')
		I_Warning("GV_GetImage: invalid image string `%s'\n", info);

	(*dest) = W_ImageParseSaveString(info[0], info + 2);
}


//----------------------------------------------------------------------------
//
//  STRINGIFIERS
//

static const char *GV_PutInt(void *storage)
{
	int *src = (int *)storage;
	char buffer[40];

	SYS_ASSERT(storage);

	sprintf(buffer, "%d", *src);

	return SV_DupString(buffer);
}

static const char *GV_PutString(void *storage)
{
	char **src = (char **)storage;

	SYS_ASSERT(storage);

	if (*src == NULL)
		return SV_DupString("");

	return SV_DupString(*src);
}

static const char *GV_PutCheckCRC(void *storage)
{
	crc_check_t *src = (crc_check_t *)storage;
	char buffer[80];

	SYS_ASSERT(storage);

	sprintf(buffer, "%d %u", src->count, src->crc);

	return SV_DupString(buffer);
}

static const char *GV_PutLevelFlags(void *storage)
{
	gameflags_t *src = (gameflags_t *)storage;
	int flags;

	SYS_ASSERT(storage);

	flags = 0;

#define HANDLE_FLAG(var, specflag)  \
	if (var) flags |= (specflag);

	HANDLE_FLAG(src->jump, MPF_Jumping);
	HANDLE_FLAG(src->crouch, MPF_Crouching);
	HANDLE_FLAG(src->mlook, MPF_Mlook);
	HANDLE_FLAG(src->itemrespawn, MPF_ItemRespawn);
	HANDLE_FLAG(src->fastparm, MPF_FastParm);
	HANDLE_FLAG(src->true3dgameplay, MPF_True3D);
	HANDLE_FLAG(src->more_blood, MPF_MoreBlood);
	HANDLE_FLAG(src->cheats, MPF_Cheats);
	HANDLE_FLAG(src->respawn, MPF_Respawn);
	HANDLE_FLAG(src->res_respawn, MPF_ResRespawn);
	HANDLE_FLAG(src->have_extra, MPF_Extras);
	HANDLE_FLAG(src->limit_zoom, MPF_LimitZoom);
	HANDLE_FLAG(src->shadows, MPF_Shadows);
	HANDLE_FLAG(src->halos, MPF_Halos);
	HANDLE_FLAG(src->kicking, MPF_Kicking);
	HANDLE_FLAG(src->weapon_switch, MPF_WeaponSwitch);
	HANDLE_FLAG(src->pass_missile, MPF_PassMissile);
	HANDLE_FLAG(src->team_damage, MPF_TeamDamage);

#undef HANDLE_FLAG

	if (!src->edge_compat)
		flags |= MPF_BoomCompat;

	if (src->autoaim != AA_OFF)
		flags |= MPF_AutoAim;
	if (src->autoaim == AA_MLOOK)
		flags |= MPF_AutoAimMlook;

	return GV_PutInt(&flags);
}

static const char *GV_PutImage(void *storage)
{
	// based on SR_LevelPutImage...

	const image_c **src = (const image_c **)storage;
	char buffer[64];

	SYS_ASSERT(storage);

	if (*src == NULL)
		return SV_DupString("");

	W_ImageMakeSaveString(*src, buffer, buffer + 2);
	buffer[1] = ':';

	return SV_DupString(buffer);
}


//----------------------------------------------------------------------------
//
//  MISCELLANY
//

saveglobals_t *SV_NewGLOB(void)
{
	saveglobals_t *globs;

	globs = new saveglobals_t;
	
	Z_Clear(globs, saveglobals_t, 1);

	globs->exit_time = INT_MAX;

	return globs;
}

void SV_FreeGLOB(saveglobals_t *globs)
{
	SV_FreeString(globs->game);
	SV_FreeString(globs->level);
	SV_FreeString(globs->hub_first);
	SV_FreeString(globs->description);
	SV_FreeString(globs->desc_date);

	if (globs->view_pixels)
		delete[] globs->view_pixels;

	if (globs->wad_names)
		delete[] globs->wad_names;

	delete globs;
}


//----------------------------------------------------------------------------
//
//  LOADING GLOBALS
//

static bool GlobReadVARI(saveglobals_t *globs)
{
	const char *var_name;
	const char *var_data;

	int i;
	void *storage;

	if (! SV_PushReadChunk("Vari"))
		return false;

	var_name = SV_GetString();
	var_data = SV_GetString();

	if (! SV_PopReadChunk() || !var_name || !var_data)
	{
		SV_FreeString(var_name);
		SV_FreeString(var_data);

		return false;
	}

	// find variable in list 
	for (i=0; global_commands[i].name; i++)
	{
		if (strcmp(global_commands[i].name, var_name) == 0)
			break;
	}

	if (global_commands[i].name)
	{
		int offset = global_commands[i].offset_p - (char *) &dummy_glob;

		// found it, so parse it
		storage = ((char *) globs) + offset;

		(* global_commands[i].parse_func)(var_data, storage);
	}
	else
	{
		I_Debugf("GlobReadVARI: unknown global: %s\n", var_name);
	}

	SV_FreeString(var_name);
	SV_FreeString(var_data);

	return true;
}

static bool GlobReadWADS(saveglobals_t *glob)
{
	if (! SV_PushReadChunk("Wads"))
		return false;

	//!!! IMPLEMENT THIS

	SV_PopReadChunk();
	return true;
}

static bool GlobReadVIEW(saveglobals_t *glob)
{
	if (! SV_PushReadChunk("View"))
		return false;

	//!!! IMPLEMENT THIS

	SV_PopReadChunk();
	return true;
}


saveglobals_t *SV_LoadGLOB(void)
{
	char marker[6];

	saveglobals_t *globs;

	SV_GetMarker(marker);

	if (strcmp(marker, "Glob") != 0 || ! SV_PushReadChunk("Glob"))
		return NULL;

	cur_globs = globs = SV_NewGLOB();

	// read through all the chunks, picking the bits we need

	for (;;)
	{
		if (SV_GetError() != 0)
			break;  /// set error !!

		if (SV_RemainingChunkSize() == 0)
			break;

		SV_GetMarker(marker);

		if (strcmp(marker, "Vari") == 0)
		{
			GlobReadVARI(globs);
			continue;
		}
		if (strcmp(marker, "Wads") == 0)
		{
			GlobReadWADS(globs);
			continue;
		}
		if (strcmp(marker, "View") == 0)
		{
			GlobReadVIEW(globs);
			continue;
		}

		// skip chunk
		I_Warning("LOADGAME: Unknown GLOB chunk [%s]\n", marker);

		if (! SV_SkipReadChunk(marker))
			break;
	}

	SV_PopReadChunk();  /// check err

	return globs;
}


//----------------------------------------------------------------------------
//
//  SAVING GLOBALS
//

static void GlobWriteVARIs(saveglobals_t *globs)
{
	int i;

	for (i=0; global_commands[i].name; i++)
	{
		int offset = global_commands[i].offset_p - (char *) &dummy_glob;

		void *storage = ((char *) globs) + offset;

		const char *data = (* global_commands[i].stringify_func)(storage);
		SYS_ASSERT(data);

		SV_PushWriteChunk("Vari");
		SV_PutString(global_commands[i].name);
		SV_PutString(data);
		SV_PopWriteChunk();

		SV_FreeString(data);
	}
}

static void GlobWriteWADS(saveglobals_t *globs)
{
	int i;

	if (! globs->wad_names)
		return;

	SYS_ASSERT(globs->wad_num > 0);

	SV_PushWriteChunk("Wads");
	SV_PutInt(globs->wad_num);

	for (i=0; i < globs->wad_num; i++)
		SV_PutString(globs->wad_names[i]);

	SV_PopWriteChunk();
}

static void GlobWriteVIEW(saveglobals_t *globs)
{
	int x, y;

	if (! globs->view_pixels)
		return;

	SYS_ASSERT(globs->view_width  > 0);
	SYS_ASSERT(globs->view_height > 0);

	SV_PushWriteChunk("View");

	SV_PutInt(globs->view_width);
	SV_PutInt(globs->view_height);

	for (y=0; y < globs->view_height; y++)
	{
		for (x=0; x < globs->view_width; x++)
		{
			SV_PutShort(globs->view_pixels[y * globs->view_width + x]);
		}
	}

	SV_PopWriteChunk();
}


void SV_SaveGLOB(saveglobals_t *globs)
{
	cur_globs = globs;

	SV_PushWriteChunk("Glob");

	GlobWriteVARIs(globs);
	GlobWriteWADS(globs);
	GlobWriteVIEW(globs);

	// all done
	SV_PopWriteChunk();
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
