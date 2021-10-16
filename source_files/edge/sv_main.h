//----------------------------------------------------------------------------
//  EDGE New SaveGame Handling (Main defs)
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
// TERMINOLOGY:
//
//   - "known" here means an array/structure that is currently built
//     into EDGE.
//
//   - "loaded" here means an array/structure definition that has been
//     loaded from the savegame file.
//

#ifndef __SV_MAIN_H__
#define __SV_MAIN_H__

#include "i_defs.h"

#include "dm_defs.h"
#include "e_player.h"
#include "p_local.h"

class image_c;
class mapdef_c;

//
// STRUCTURE TABLE STUFF
//

typedef enum
{
	SFKIND_Invalid = 0,  // invalid values can be helpful
	SFKIND_Numeric,
	SFKIND_Index,
	SFKIND_String,
	SFKIND_Struct
}
savefieldkind_e;

typedef struct
{
	// basic kind of field (for SDEF chunk)
	savefieldkind_e kind;

	// number of bytes for SFKIND_Numeric (1, 2, 4 or 8)
	int size;

	// name of structure for SFKIND_Struct, or name of array
	// for SFKIND_Index.
	const char *name;
}
savefieldtype_t;

#define SVT_INVALID        { SFKIND_Invalid, 0, NULL }

#define SVT_INT            { SFKIND_Numeric, 4, NULL }
#define SVT_SHORT          { SFKIND_Numeric, 2, NULL }
#define SVT_BYTE           { SFKIND_Numeric, 1, NULL }
#define SVT_FLOAT          { SFKIND_Numeric, 4, NULL }
#define SVT_VEC2           { SFKIND_Numeric, 8, NULL }
#define SVT_VEC3           { SFKIND_Numeric, 12,NULL }
#define SVT_INDEX(name)    { SFKIND_Index,   4, name }
#define SVT_STRING         { SFKIND_String,  0, NULL }
#define SVT_STRUCT(name)   { SFKIND_Struct,  0, name }

#define SVT_ANGLE          SVT_INT
#define SVT_BOOLEAN        SVT_INT
#define SVT_ENUM           SVT_INT
#define SVT_RGBCOL         SVT_INT
#define SVT_PERCENT        SVT_FLOAT


// This describes a single field
typedef struct savefieldtype_s
{
	// offset of field in structure (actually a ptr into dummy struct)
	const char *offset_p;

	// name of field in savegame system
	const char *field_name;

	// number of sequential elements
	int count;

	// field type information
	savefieldtype_t type;

	// get & put routines.  The extra parameter depends on the type, for
	// SFKIND_Struct it is the name of the structure, for SFKIND_Index
	// it is the name of the array.  When `field_put' is NULL, then this
	// field is not saved into the output SDEF chunk.
	bool (* field_get)(void *storage, int index, void *extra);
	void (* field_put)(void *storage, int index, void *extra);

	// for loaded info, this points to the known version of the field,
	// otherwise NULL if the loaded field is unknown.
	struct savefieldtype_s *known_field;
}
savefield_t;

// NOTE: requires SV_F_BASE to be defined as the dummy struct

#define SVFIELD(field,fname,fnum,ftype,getter,putter)  \
    { (const char *) & SV_F_BASE.field,  \
      fname, fnum, ftype, getter, putter, NULL }

#define SVFIELD_END  { 0, NULL, 0, SVT_INVALID, NULL, NULL, NULL }

#define SVDUMMY  ((const char *) & SV_F_BASE)


// This describes a single structure
typedef struct savestruct_s
{
	// link in list of structure definitions
	struct savestruct_s *next;

	// structure name (for SDEF/ADEF chunks)
	const char *struct_name;

	// four letter marker
	const char *marker;

	// array of field definitions
	savefield_t *fields;

	// address of dummy struct (used to compute field offsets)
	const char *dummy_base;

	// this must be true to put the definition into the savegame file.
	// Allows compatibility structures that are read-only.
	bool define_me;

	// only used when loading.  For loaded info, this refers to the
	// known struct of the same name (or NULL if none).  For known info,
	// this points to the loaded info (or NULL if absent).
	struct savestruct_s *counterpart;
}
savestruct_t;


// This describes a single array
typedef struct savearray_s
{
	// link in list of array definitions
	struct savearray_s *next;

	// array name (for ADEF and STOR chunks)
	const char *array_name;

	// array type.  For loaded info, this points to the loaded
	// structure.  Never NULL.
	savestruct_t *sdef;

	// this must be true to put the definition into the savegame file.
	// Allows compatibility arrays that are read-only.
	bool define_me;

	// load this array even when loading in HUB mode.  There are
	// some things we _don't_ want to load when going back to a
	// visited level: players and active_hubs in particular.
	bool allow_hub;

	// array routines.  Not used for loaded info.
	int (* count_elems)(void);
	void * (* get_elem)(int index);
	void (* create_elems)(int num_elems);
	void (* finalise_elems)(void);

	// only used when loading.  For loaded info, this refers to the
	// known array (or NULL if none).  For known info, this points to
	// the loaded info (or NULL if absent).
	struct savearray_s *counterpart;

	// number of elements to be loaded.
	int loaded_size;
}
savearray_t;


//
//  COMMON GET ROUTINES
//
//  Note the `SR_' prefix.
//
bool SR_GetByte(void *storage, int index, void *extra);
bool SR_GetShort(void *storage, int index, void *extra);
bool SR_GetInt(void *storage, int index, void *extra);

bool SR_GetAngle(void *storage, int index, void *extra);
bool SR_GetFloat(void *storage, int index, void *extra);
bool SR_GetBoolean(void *storage, int index, void *extra);

bool SR_GetVec2(void *storage, int index, void *extra);
bool SR_GetVec3(void *storage, int index, void *extra);
bool SR_GetFloatFromInt(void *storage, int index, void *extra);
bool SR_GetAngleFromSlope(void *storage, int index, void *extra);

#define SR_GetEnum     SR_GetInt
#define SR_GetRGB      SR_GetInt
#define SR_GetPercent  SR_GetFloat


//
//  COMMON PUT ROUTINES
//
//  Note the `SR_' prefix.
//
void SR_PutByte(void *storage, int index, void *extra);
void SR_PutShort(void *storage, int index, void *extra);
void SR_PutInt(void *storage, int index, void *extra);

void SR_PutAngle(void *storage, int index, void *extra);
void SR_PutFloat(void *storage, int index, void *extra);
void SR_PutBoolean(void *storage, int index, void *extra);

void SR_PutVec2(void *storage, int index, void *extra);
void SR_PutVec3(void *storage, int index, void *extra);
void SR_PutAngleToSlope(void *storage, int index, void *extra);

#define SR_PutEnum     SR_PutInt
#define SR_PutRGB      SR_PutInt
#define SR_PutPercent  SR_PutFloat


//
//  GLOBAL STUFF
//

typedef struct crc_check_s
{
	// number of items
	int count;

	// CRC computed over all the items
	u32_t crc;
}
crc_check_t;

// this structure contains everything for the top-level [GLOB] chunk.
// Strings are copies and need to be freed.
typedef struct
{
	// [IVAR] stuff:

	const char *game;
	const char *level;
	gameflags_t flags;
	int hub_tag;
	const char *hub_first;

	int level_time;
	int exit_time;
	int p_random;
	int total_kills;
	int total_items;
	int total_secrets;

	int console_player;
	int skill;
	int netgame;

	const image_c *sky_image;  // -AJA- added 2003/12/19

	const char *description;
	const char *desc_date;

	crc_check_t mapsector;
	crc_check_t mapline;
	crc_check_t mapthing;

	crc_check_t rscript;
	crc_check_t ddfatk;
	crc_check_t ddfgame;
	crc_check_t ddflevl;
	crc_check_t ddfline;
	crc_check_t ddfsect;
	crc_check_t ddfmobj;
	crc_check_t ddfweap;

	// [VIEW] info.  Unused if view_pixels is NULL.
	unsigned short *view_pixels;
	int view_width;
	int view_height;

	// [WADS] info
	int wad_num;
	const char ** wad_names;

	// [PLYR] info, for DEMO FILES only!
	void *players[MAXPLAYERS];
}
saveglobals_t;

saveglobals_t *SV_NewGLOB(void);
saveglobals_t *SV_LoadGLOB(void);
void SV_SaveGLOB(saveglobals_t *globs);
void SV_FreeGLOB(saveglobals_t *globs);


//
//  ADMININISTRATION
//

void SV_MainInit(void);

savestruct_t *SV_MainLookupStruct(const char *name);
savearray_t  *SV_MainLookupArray(const char *name);

void SV_BeginLoad(bool is_hub);
void SV_FinishLoad(void);

bool SV_LoadStruct(void *base, savestruct_t *info);
bool SV_LoadEverything(void);

void SV_BeginSave(void);
void SV_FinishSave(void);

void SV_SaveStruct(void *base, savestruct_t *info);
void SV_SaveEverything(void);

const char *SV_SlotName(int slot);
const char *SV_MapName(const mapdef_c *map);

std::string SV_FileName(const char *slot_name, const char *map_name);

void SV_ClearSlot(const char *slot_name);
void SV_CopySlot(const char *src_name, const char *dest_name);


//
//  DEBUGGING
//

void SV_DumpSaveGame(int slot);


//
//  EXTERNAL DEFS
//

extern void *sv_current_elem;

extern savestruct_t *sv_known_structs;
extern savearray_t  *sv_known_arrays;

bool SR_MobjGetMobj(void *storage, int index, void *extra);
void SR_MobjPutMobj(void *storage, int index, void *extra);

int SV_MobjFindElem(mobj_t *elem);
void * SV_MobjGetElem(int index);

int SV_PlayerFindElem(player_t *elem);
void * SV_PlayerGetElem(int index);

bool SR_LevelGetImage(void *storage, int index, void *extra);
void SR_LevelPutImage(void *storage, int index, void *extra);

bool SR_LevelGetColmap(void *storage, int index, void *extra);
void SR_LevelPutColmap(void *storage, int index, void *extra);

bool SR_LineGetLine(void *storage, int index, void *extra);
void SR_LinePutLine(void *storage, int index, void *extra);

bool SR_SectorGetSector(void *storage, int index, void *extra);
void SR_SectorPutSector(void *storage, int index, void *extra);

bool SR_SectorGetEF(void *storage, int index, void *extra);
void SR_SectorPutEF(void *storage, int index, void *extra);

bool SR_TriggerGetScript(void *storage, int index, void *extra);
void SR_TriggerPutScript(void *storage, int index, void *extra);


#endif  /*__SV_MAIN_H__*/

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
