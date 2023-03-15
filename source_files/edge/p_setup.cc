//----------------------------------------------------------------------------
//  EDGE Level Loading/Setup Code
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2022  The EDGE Team.
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

#include "i_defs.h"

#include <vector>
#include <map>

#include "endianess.h"
#include "math_crc.h"

#include "main.h"
#include "colormap.h"

#include "dm_defs.h"
#include "dm_state.h"
#include "dm_structs.h"
#include "e_main.h"
#include "g_game.h"
#include "l_ajbsp.h"
#include "m_argv.h"
#include "m_bbox.h"
#include "m_math.h"
#include "m_misc.h"
#include "m_random.h"
#include "p_local.h"
#include "p_setup.h"
#include "am_map.h"
#include "r_gldefs.h"
#include "r_sky.h"
#include "s_sound.h"
#include "s_music.h"
#include "sv_main.h"
#include "r_image.h"
#include "w_texture.h"
#include "w_wad.h"

#ifdef __arm__
  #include "str_util.h"
#endif

#include "miniz.h" // ZGL3 nodes

// debugging aide:
#define FORCE_LOCATION  0
#define FORCE_LOC_X     12766
#define FORCE_LOC_Y     4600
#define FORCE_LOC_ANG   0


#define SEG_INVALID  ((seg_t *) -3)
#define SUB_INVALID  ((subsector_t *) -3)


static bool level_active = false;

DEF_CVAR(udmf_strict, "0", CVAR_ARCHIVE)

//
// MAP related Lookup tables.
// Store VERTEXES, LINEDEFS, SIDEDEFS, etc.
//
int numvertexes;
vec2_t *vertexes = nullptr;
vec2_t *zvertexes = nullptr;

int num_gl_vertexes;
vec2_t *gl_vertexes;

int numsegs;
seg_t *segs;

int numsectors;
sector_t *sectors;

int numsubsectors;
subsector_t *subsectors;

int numextrafloors;
extrafloor_t *extrafloors;

int numnodes;
node_t *nodes;

int numlines;
line_t *lines;

int numsides;
side_t *sides;

int numvertgaps;
vgap_t *vertgaps;

vertex_seclist_t *v_seclists;

static line_t **linebuffer = NULL;

// bbox used 
static float dummy_bbox[4];

epi::crc32_c mapsector_CRC;
epi::crc32_c mapline_CRC;
epi::crc32_c mapthing_CRC;

int mapthing_NUM;

static bool hexen_level;

static bool udmf_level;
static int udmf_lumpnum;
static char *udmf_lump;

// a place to store sidedef numbers of the loaded linedefs.
// There is two values for every line: side0 and side1.
static int *temp_line_sides;

DEF_CVAR(m_goobers, "0", 0)


// UDMF parser and loading routines backported from EDGE 2.x codebase
// with non-Vanilla namespace items removed for the time being
typedef struct {
	uint8_t *buffer;
	uint8_t line[512];
	int length;
	int next;
	int prev;
} parser_t;

static parser_t udmf_psr;

static bool GetNextLine(parser_t *psr)
{
	if (psr->next >= psr->length)
		return false; // no more lines

	int i;
	// get next line
	psr->prev = psr->next;
	uint8_t *lp = &psr->buffer[psr->next];
	for (i=0; i<(psr->length - psr->next); i++, lp++)
		if (*lp == 0x0A || *lp == 0x0D)
			break;
	if (i == (psr->length - psr->next))
		lp = &psr->buffer[psr->length - 1]; // last line
	psr->next = (int)(lp - psr->buffer) + 1;
	memcpy(psr->line, &psr->buffer[psr->prev], MIN(511, psr->next - psr->prev - 1));
	psr->line[MIN(511, psr->next - psr->prev) - 1] = 0;
	// skip any more CR/LF
	while (psr->buffer[psr->next] == 0x0A || psr->buffer[psr->next] == 0x0D)
		psr->next++;

	// check for comments
	lp = psr->line;
	while (lp[0] != 0 && lp[0] != 0x2F && lp[1] != 0x2F)
		lp++; // find full line comment start (if present)
	if (lp[0] != 0)
	{
		*lp = 0; // terminate at full line comment start
	}
	else
	{
		lp = psr->line;
		while (lp[0] != 0 && lp[0] != 0x2F && lp[1] != 0x2A)
			lp++; // find multi-line comment start (if present)
		if (lp[0] != 0)
		{
			*lp = 0; // terminate at multi-line comment start
			uint8_t *ep = &lp[2];
			while (ep[0] != 0 && ep[0] != 0x2A && ep[1] != 0x2F)
				ep++; // find multi-line comment end (if present)
			if (ep[0] == 0)
			{
				ep = &psr->buffer[psr->next];
				for (i=0; i<(psr->length - psr->next); i++, ep++)
					if (ep[0] == 0x2A && ep[1] == 0x2F)
						break;
				if (i == (psr->length - psr->next))
					ep = &psr->buffer[psr->length - 2];
			}
			psr->next = (int)(ep - psr->buffer) + 2; // skip comment for next line
		}
	}

	//I_Debugf(" parser next line: %s\n", (char *)psr->line);
	return true;
}

static bool GetNextAssign(parser_t *psr, uint8_t *ident, uint8_t *val)
{
	if (!GetNextLine(psr))
		return false; // no more lines

	int len = 0;
	while (psr->line[len] != 0 && psr->line[len] != 0x7D)
		len++;
	if (psr->line[len] == 0x7D)
	{
		//I_Debugf(" parser: block end\n");
		return false;
	}
	else if (len < 4)
		return false; //line too short for assignment

	uint8_t *lp = psr->line;
	while (*lp != 0x3D && *lp != 0)
		lp++; // find '='
	if (lp[0] != 0x3D)
		return false; // not an assignment line

	*lp++ = 0; // split at assignment operator
	int i = 0;
	while (psr->line[i] == 0x20 || psr->line[i] == 0x09)
		i++; // skip whitespace before indentifier
	memcpy(ident, &psr->line[i], lp - &psr->line[i] - 1);
	i = lp - &psr->line[i] - 2;
	while (ident[i] == 0x20 || ident[i] == 0x09)
		i--; // skip whitespace after identifier
	ident[i+1] = 0;

	i = 0;
	while (lp[i] == 0x20 || lp[i] == 0x09 || lp[i] == 0x22)
		i++; // skip whitespace and quotes before value
	memcpy(val, &lp[i], &psr->line[len] - &lp[i]);
	i = (int)(&psr->line[len] - &lp[i]) - 2;
	while (val[i] == 0x20 || val[i] == 0x09 || val[i] == 0x22  || val[i] == 0x3B)
		i--; // skip whitespace, quote, and semi-colon after value
	val[i+1] = 0;

	//I_Debugf(" parser: ident = %s, val = %s\n", (char *)ident, (char *)val);
	return true;
}

static bool GetNextBlock(parser_t *psr, uint8_t *ident)
{
	if (!GetNextLine(psr))
		return false; // no more lines

	int len, i = 0;
	while (psr->line[i] == 0x20 || psr->line[i] == 0x09)
		i++; // skip whitespace
blk_loop:
	len = 0;
	while (psr->line[len] != 0)
		len++;

	memcpy(ident, &psr->line[i], len - i);
	i = len - i - 1;
	while (ident[i] == 0x20 || ident[i] == 0x09)
		i--; // skip whitespace from end of line
	ident[i+1] = 0;

	if (!GetNextLine(psr))
		return false; // no more lines

	i = 0;
	while (psr->line[i] == 0x20 || psr->line[i] == 0x09)
		i++; // skip whitespace before indentifier or block start
	if (psr->line[i] != 0x7B)
		goto blk_loop; // not a block start

	//I_Debugf(" parser: block start = %s\n", ident);
	return true;
}

static bool str2bool(char *val)
{
	if (epi::case_cmp(val, "true") == 0)
		return true;

	// default is always false
	return false;
}

static int str2int(char *val, int def)
{
	int ret;

	if (sscanf(val, "%d", &ret) == 1)
		return ret;

	// error - return default
	return def;
}

static float str2float(char *val, float def)
{
	float ret;

	if (sscanf(val, "%f", &ret) == 1)
		return ret;

	// error - return default
	return def;
}

static void CheckEvilutionBug(byte *data, int length)
{
	// The IWAD for TNT Evilution has a bug in MAP31 which prevents
	// the yellow keycard from appearing (the "Multiplayer Only" flag
	// is set), and the level cannot be completed.  This fixes it.

	static const byte Y_key_data[] =
	{
		0x59,0xf5, 0x48,0xf8, 0,0, 6,0, 0x17,0
	};

	static const int Y_key_offset = 0x125C;

	if (length < Y_key_offset + 10)
		return;

	data += Y_key_offset;

	if (memcmp(data, Y_key_data, 10) != 0)
		return;

	I_Printf("Detected TNT MAP31 bug, adding fix.\n");

	data[8] &= ~MTF_NOT_SINGLE;
}


static void CheckDoom2Map05Bug(byte *data, int length)
{
	// The IWAD for Doom2 has a bug in MAP05 where 2 sectors 
	// are incorrectly tagged 9.  This fixes it.

	static const byte sector_4_data[] =
	{
		0x60,0, 0xc8,0, 0x46,0x4c, 0x41,0x54, 0x31,0,0,0,0x46,0x4c,0x41,0x54,0x31,0x30,0,0,0x70,0,0,0,9,0
	};
	
	static const byte sector_153_data[] =
	{
		0x98,0, 0xe8,0, 0x46,0x4c, 0x41,0x54, 0x31,0,0,0,0x46,0x4c,0x41,0x54,0x31,0x30,0,0,0x70,0,9,0,9,0
	};

	static const int sector_4_offset = 0x68; //104
	static const int sector_153_offset = 3978; //0xf8a; //3978

	if (length < sector_4_offset + 26)
		return;

	if (length < sector_153_offset + 26)
		return;

	//Sector 4 first
	data += sector_4_offset;

	if (memcmp(data, sector_4_data, 26) != 0)
		return;
	
	if(data[24] == 9) //check just in case
		data[24] = 0; //set tag to 0 instead of 9
	
	//now sector 153
	data += (sector_153_offset - sector_4_offset);

	if (memcmp(data, sector_153_data, 26) != 0)
		return;

	if(data[24] == 9) //check just in case
		data[24] = 0; //set tag to 0 instead of 9
	
	I_Printf("Detected Doom2 MAP05 bug, adding fix.\n");
}


static void LoadVertexes(int lump)
{
	const void *data;
	int i;
	const raw_vertex_t *ml;
	vec2_t *li;

	if (! W_VerifyLumpName(lump, "VERTEXES"))
		I_Error("Bad WAD: level %s missing VERTEXES.\n", 
				currmap->lump.c_str());

	// Determine number of lumps:
	//  total lump length / vertex record length.
	numvertexes = W_LumpLength(lump) / sizeof(raw_vertex_t);

	if (numvertexes == 0)
		I_Error("Bad WAD: level %s contains 0 vertexes.\n", 
				currmap->lump.c_str());

	vertexes = new vec2_t[numvertexes];

	// Load data into cache.
	data = W_CacheLumpNum(lump);

	ml = (const raw_vertex_t *) data;
	li = vertexes;

	// Copy and convert vertex coordinates,
	// internal representation as fixed.
	for (i = 0; i < numvertexes; i++, li++, ml++)
	{
		li->x = EPI_LE_S16(ml->x);
		li->y = EPI_LE_S16(ml->y);
	}

	// Free buffer memory.
	W_DoneWithLump(data);
}

static void SegCommonStuff(seg_t *seg, int linedef)
{
	seg->frontsector = seg->backsector = NULL;

	if (linedef == -1)
	{
		seg->miniseg = true;
	}
	else
	{
		if (linedef >= numlines)  // sanity check
			I_Error("Bad GWA file: seg #%d has invalid linedef.\n", (int)(seg - segs));

		seg->miniseg = false;
		seg->linedef = &lines[linedef];

		float sx = seg->side ? seg->linedef->v2->x : seg->linedef->v1->x;
		float sy = seg->side ? seg->linedef->v2->y : seg->linedef->v1->y;

		seg->offset = R_PointToDist(sx, sy, seg->v1->x, seg->v1->y);

		seg->sidedef = seg->linedef->side[seg->side];

		if (! seg->sidedef)
			I_Error("Bad GWA file: missing side for seg #%d\n", (int)(seg - segs));

		seg->frontsector = seg->sidedef->sector;

		if (seg->linedef->flags & MLF_TwoSided)
		{
			side_t *other = seg->linedef->side[seg->side^1];

			if (other)
				seg->backsector = other->sector;
		}
	}
}

//
// GroupSectorTags
//
// Called during P_LoadSectors to set the tag_next & tag_prev fields of
// each sector_t, which keep all sectors with the same tag in a linked
// list for faster handling.
//
// -AJA- 1999/07/29: written.
//
static void GroupSectorTags(sector_t * dest, sector_t * seclist, int numsecs)
{
	// NOTE: `numsecs' does not include the current sector.

	dest->tag_next = dest->tag_prev = NULL;

	for (; numsecs > 0; numsecs--)
	{
		sector_t *src = &seclist[numsecs - 1];

		if (src->tag == dest->tag)
		{
			src->tag_next = dest;
			dest->tag_prev = src;
			return;
		}
	}
}


static void LoadSectors(int lump)
{
	const void *data;
	int i;
	const raw_sector_t *ms;
	sector_t *ss;

	if (! W_VerifyLumpName(lump, "SECTORS"))
	{
		// Check if SECTORS is immediately after THINGS/LINEDEFS/SIDEDEFS/VERTEXES
		lump -= 3;
		if (! W_VerifyLumpName(lump, "SECTORS"))
			I_Error("Bad WAD: level %s missing SECTORS.\n", 
					currmap->lump.c_str());
	}

	numsectors = W_LumpLength(lump) / sizeof(raw_sector_t);

	if (numsectors == 0)
		I_Error("Bad WAD: level %s contains 0 sectors.\n", 
				currmap->lump.c_str());

	sectors = new sector_t[numsectors];
	Z_Clear(sectors, sector_t, numsectors);

	data = W_CacheLumpNum(lump);
	mapsector_CRC.AddBlock((const byte*)data, W_LumpLength(lump));

	CheckDoom2Map05Bug((byte *)data, W_LumpLength(lump)); //Lobo: 2023

	ms = (const raw_sector_t *) data;
	ss = sectors;
	for (i = 0; i < numsectors; i++, ss++, ms++)
	{
		char buffer[10];

		ss->f_h = EPI_LE_S16(ms->floor_h);
		ss->c_h = EPI_LE_S16(ms->ceil_h);

        // return to wolfenstein?
        if (m_goobers.d)
        {
            ss->f_h = 0;
            ss->c_h = (ms->floor_h == ms->ceil_h) ? 0 : 128.0f;
        }

		ss->floor.translucency = VISIBLE;
		ss->floor.x_mat.x = 1;  ss->floor.x_mat.y = 0;
		ss->floor.y_mat.x = 0;  ss->floor.y_mat.y = 1;

		ss->ceil = ss->floor;

		Z_StrNCpy(buffer, ms->floor_tex, 8);
		ss->floor.image = W_ImageLookup(buffer, INS_Flat);

		Z_StrNCpy(buffer, ms->ceil_tex, 8);
		ss->ceil.image = W_ImageLookup(buffer, INS_Flat);

		if (! ss->floor.image)
		{
			I_Warning("Bad Level: sector #%d has missing floor texture.\n", i);
			ss->floor.image = W_ImageLookup("FLAT1", INS_Flat);
		}
		if (! ss->ceil.image)
		{
			I_Warning("Bad Level: sector #%d has missing ceiling texture.\n", i);
			ss->ceil.image = ss->floor.image;
		}

		// convert negative tags to zero
		ss->tag = MAX(0, EPI_LE_S16(ms->tag));

		ss->props.lightlevel = EPI_LE_S16(ms->light);

		int type = EPI_LE_S16(ms->special);

		ss->props.type = MAX(0, type);
		ss->props.special = P_LookupSectorType(ss->props.type);

		ss->exfloor_max = 0;

		ss->props.colourmap = NULL;

		ss->props.gravity   = GRAVITY;
		ss->props.friction  = FRICTION;
		ss->props.viscosity = VISCOSITY;
		ss->props.drag      = DRAG;

		ss->p = &ss->props;

		ss->sound_player = -1;

		// -AJA- 1999/07/29: Keep sectors with same tag in a list.
		GroupSectorTags(ss, sectors, i);
	}

	W_DoneWithLump(data);
}

static void SetupRootNode(void)
{
	if (numnodes > 0)
	{
		root_node = numnodes - 1;
	}
	else
	{
		root_node = NF_V5_SUBSECTOR | 0;

		// compute bbox for the single subsector
		M_ClearBox(dummy_bbox);

		int i;
		seg_t *seg;

		for (i=0, seg=segs; i < numsegs; i++, seg++)
		{
			M_AddToBox(dummy_bbox, seg->v1->x, seg->v1->y);
			M_AddToBox(dummy_bbox, seg->v2->x, seg->v2->y);
		}
	}
}

static std::map<int, int> unknown_thing_map;

static void UnknownThingWarning(int type, float x, float y)
{
	int count = 0;

	if (unknown_thing_map.find(type) != unknown_thing_map.end())
		count = unknown_thing_map[type];

	if (count < 2)
		I_Warning("Unknown thing type %i at (%1.0f, %1.0f)\n", type, x, y);
	else if (count == 2)
		I_Warning("More unknown things of type %i found...\n", type);

	unknown_thing_map[type] = count+1;
}


static void SpawnMapThing(const mobjtype_c *info,
						  float x, float y, float z,
						  sector_t *sec, angle_t angle,
						  int options, int tag)
{
	spawnpoint_t point;

	point.x = x;
	point.y = y;
	point.z = z;
	point.angle = angle;
	point.vertangle = 0;
	point.info = info;
	point.flags = 0;
	point.tag = tag;

	// -KM- 1999/01/31 Use playernum property.
	// count deathmatch start positions
	if (info->playernum < 0)
	{
		G_AddDeathmatchStart(point);
		return;
	}

	// check for players specially -jc-
	if (info->playernum > 0)
	{
		// -AJA- 2009/10/07: Hub support
		if (sec->props.special && sec->props.special->hub)
		{
			if (sec->tag <= 0)
				I_Warning("HUB_START in sector without tag @ (%1.0f %1.0f)\n", x, y);

			point.tag = sec->tag;

			G_AddHubStart(point);
			return;
		}

		// -AJA- 2004/12/30: for duplicate players, the LAST one must
		//       be used (so levels with Voodoo dolls work properly).
		spawnpoint_t *prev = G_FindCoopPlayer(info->playernum);

		if (! prev)
			G_AddCoopStart(point);
		else
		{
			G_AddVoodooDoll(*prev);

			// overwrite one in the Coop list with new location
			memcpy(prev, &point, sizeof(point));
		}
		return;
	}

	// check for apropriate skill level
	// -ES- 1999/04/13 Implemented Kester's Bugfix.
	// -AJA- 1999/10/21: Reworked again.
	if (SP_MATCH() && (options & MTF_NOT_SINGLE))
		return;

	// Disable deathmatch weapons for vanilla coop...should probably be in the Gameplay Options menu - Dasho
	if (COOP_MATCH() && (options & MTF_NOT_SINGLE))
		return;

	// -AJA- 1999/09/22: Boom compatibility flags.
	if (COOP_MATCH() && (options & MTF_NOT_COOP))
		return;

	if (DEATHMATCH() && (options & MTF_NOT_DM))
		return;

	int bit;

	if (gameskill == sk_baby)
		bit = 1;
	else if (gameskill == sk_nightmare)
		bit = 4;
	else
		bit = 1 << (gameskill - 1);

	if ((options & bit) == 0)
		return;

	// don't spawn keycards in deathmatch
	if (DEATHMATCH() && (info->flags & MF_NOTDMATCH))
		return;

	// don't spawn any monsters if -nomonsters
	if (level_flags.nomonsters && (info->extendedflags & EF_MONSTER))
		return;

	// -AJA- 1999/10/07: don't spawn extra things if -noextra.
	if (!level_flags.have_extra && (info->extendedflags & EF_EXTRA))
		return;

	// spawn it now !
	// Use MobjCreateObject -ACB- 1998/08/06
	mobj_t * mo = P_MobjCreateObject(x, y, z, info);

	mo->angle = angle;
	mo->spawnpoint = point;

	if (mo->state && mo->state->tics > 1)
		mo->tics = 1 + (P_Random() % mo->state->tics);

	if (options & MTF_AMBUSH)
	{
		mo->flags |= MF_AMBUSH;
		mo->spawnpoint.flags |= MF_AMBUSH;
	}

	// -AJA- 2000/09/22: MBF compatibility flag
	if (options & MTF_FRIEND)
	{
		mo->side = 1; //~0;
		mo->hyperflags |=HF_ULTRALOYAL;
		/*
		player_t *player;
		player = players[0];
		mo->SetSupportObj(player->mo);
		P_LookForPlayers(mo, mo->info->sight_angle);
		*/
	}
	//Lobo 2022: added tagged mobj support ;)
	if (tag > 0)
		mo->tag = tag;
		
}

static void LoadThings(int lump)
{
	float x, y, z;
	angle_t angle;
	int options, typenum;
	int i;

	const void *data;
	const raw_thing_t *mt;
	const mobjtype_c *objtype;
	int numthings;

	unknown_thing_map.clear();

	if (!W_VerifyLumpName(lump, "THINGS"))
		I_Error("Bad WAD: level %s missing THINGS.\n", 
				currmap->lump.c_str());

	numthings = W_LumpLength(lump) / sizeof(raw_thing_t);

	if (numthings == 0)
		I_Error("Bad WAD: level %s contains 0 things.\n", 
				currmap->lump.c_str());

	data = W_CacheLumpNum(lump);
	mapthing_CRC.AddBlock((const byte*)data, W_LumpLength(lump));
	mapthing_NUM = numthings;

	CheckEvilutionBug((byte *)data, W_LumpLength(lump));

	// -AJA- 2004/11/04: check the options in all things to see whether
	// we can use new option flags or not.  Same old wads put 1 bits in
	// unused locations (unusued for original Doom anyway).  The logic
	// here is based on PrBoom, but PrBoom checks each thing separately.

	bool limit_options = false;

	mt = (const raw_thing_t *) data;

	for (i = 0; i < numthings; i++)
	{
		options = EPI_LE_U16(mt[i].options);

		if (options & MTF_RESERVED)
			limit_options = true;
	}

	for (i = 0; i < numthings; i++, mt++)
	{
		x = (float) EPI_LE_S16(mt->x);
		y = (float) EPI_LE_S16(mt->y);
		angle = FLOAT_2_ANG((float) EPI_LE_S16(mt->angle));
		typenum = EPI_LE_U16(mt->type);
		options = EPI_LE_U16(mt->options);

		if (limit_options)
			options &= 0x001F;

#if (FORCE_LOCATION)
		if (typenum == 1)
		{
			x = FORCE_LOC_X;
			y = FORCE_LOC_Y;
			angle = FORCE_LOC_ANG;
		}
#endif

		objtype = mobjtypes.Lookup(typenum);

		// MOBJTYPE not found, don't crash out: JDS Compliance.
		// -ACB- 1998/07/21
		if (objtype == NULL)
		{
			UnknownThingWarning(typenum, x, y);
			continue;
		}

		sector_t *sec = R_PointInSubsector(x, y)->sector;
		
		z = sec->f_h;
		
		if (objtype->flags & MF_SPAWNCEILING)
			z = sec->c_h - objtype->height;

		if ((options & MTF_RESERVED) == 0 && (options & MTF_EXFLOOR_MASK))
		{
			int floor_num = (options & MTF_EXFLOOR_MASK) >> MTF_EXFLOOR_SHIFT;

			for (extrafloor_t *ef = sec->bottom_ef; ef; ef = ef->higher)
			{
				z = ef->top_h;

				floor_num--;
				if (floor_num == 0)
					break;
			}
		}

		SpawnMapThing(objtype, x, y, z, sec, angle, options, 0);
	}

	W_DoneWithLump(data);
}


static void LoadHexenThings(int lump)
{
	// -AJA- 2001/08/04: wrote this, based on the Hexen specs.

	float x, y, z;
	angle_t angle;
	int options, typenum;
	int tag;
	int i;

	const void *data;
	const raw_hexen_thing_t *mt;
	const mobjtype_c *objtype;
	int numthings;

	unknown_thing_map.clear();

	if (!W_VerifyLumpName(lump, "THINGS"))
		I_Error("Bad WAD: level %s missing THINGS.\n", 
				currmap->lump.c_str());

	numthings = W_LumpLength(lump) / sizeof(raw_hexen_thing_t);

	if (numthings == 0)
		I_Error("Bad WAD: level %s contains 0 things.\n", 
				currmap->lump.c_str());

	data = W_CacheLumpNum(lump);
	mapthing_CRC.AddBlock((const byte*)data, W_LumpLength(lump));
	mapthing_NUM = numthings;

	mt = (const raw_hexen_thing_t *) data;
	for (i = 0; i < numthings; i++, mt++)
	{
		x = (float) EPI_LE_S16(mt->x);
		y = (float) EPI_LE_S16(mt->y);
		z = (float) EPI_LE_S16(mt->height);
		angle = FLOAT_2_ANG((float) EPI_LE_S16(mt->angle));

		tag = EPI_LE_S16(mt->tid);
		typenum = EPI_LE_U16(mt->type);
		options = EPI_LE_U16(mt->options) & 0x000F;

		objtype = mobjtypes.Lookup(typenum);

		// MOBJTYPE not found, don't crash out: JDS Compliance.
		// -ACB- 1998/07/21
		if (objtype == NULL)
		{
			UnknownThingWarning(typenum, x, y);
			continue;
		}

		sector_t *sec = R_PointInSubsector(x, y)->sector;
		
		z += sec->f_h;

		if (objtype->flags & MF_SPAWNCEILING)
			z = sec->c_h - objtype->height;

		SpawnMapThing(objtype, x, y, z, sec, angle, options, tag);
	}

	W_DoneWithLump(data);
}


static inline void ComputeLinedefData(line_t *ld, int side0, int side1)
{
	vec2_t *v1 = ld->v1;
	vec2_t *v2 = ld->v2;

	ld->dx = v2->x - v1->x;
	ld->dy = v2->y - v1->y;

	if (ld->dx == 0)
		ld->slopetype = ST_VERTICAL;
	else if (ld->dy == 0)
		ld->slopetype = ST_HORIZONTAL;
	else if (ld->dy / ld->dx > 0)
		ld->slopetype = ST_POSITIVE;
	else
		ld->slopetype = ST_NEGATIVE;

	ld->length = R_PointToDist(0, 0, ld->dx, ld->dy);

	if (v1->x < v2->x)
	{
		ld->bbox[BOXLEFT] = v1->x;
		ld->bbox[BOXRIGHT] = v2->x;
	}
	else
	{
		ld->bbox[BOXLEFT] = v2->x;
		ld->bbox[BOXRIGHT] = v1->x;
	}

	if (v1->y < v2->y)
	{
		ld->bbox[BOXBOTTOM] = v1->y;
		ld->bbox[BOXTOP] = v2->y;
	}
	else
	{
		ld->bbox[BOXBOTTOM] = v2->y;
		ld->bbox[BOXTOP] = v1->y;
	}

	if (!udmf_level && side0 == 0xFFFF) side0 = -1;
	if (!udmf_level && side1 == 0xFFFF) side1 = -1;

	// handle missing RIGHT sidedef (idea taken from MBF)
	if (side0 == -1)
	{
		I_Warning("Bad WAD: level %s linedef #%d is missing RIGHT side\n",
			currmap->lump.c_str(), (int)(ld - lines));
		side0 = 0;
	}

	if ((ld->flags & MLF_TwoSided) && ((side0 == -1) || (side1 == -1)))
	{
		I_Warning("Bad WAD: level %s has linedef #%d marked TWOSIDED, "
			"but it has only one side.\n", 
			currmap->lump.c_str(), (int)(ld - lines));

		ld->flags &= ~MLF_TwoSided;
	}

	temp_line_sides[(ld-lines)*2 + 0] = side0;
	temp_line_sides[(ld-lines)*2 + 1] = side1;

	numsides += (side1 == -1) ? 1 : 2;
}

static void LoadLineDefs(int lump)
{
	// -AJA- New handling for sidedefs.  Since sidedefs can be "packed" in
	//       a wad (i.e. shared by several linedefs) we need to unpack
	//       them.  This is to prevent potential problems with scrollers,
	//       the CHANGE_TEX command in RTS, etc, and also to implement
	//       "wall tiles" properly.

	if (! W_VerifyLumpName(lump, "LINEDEFS"))
		I_Error("Bad WAD: level %s missing LINEDEFS.\n", 
				currmap->lump.c_str());

	numlines = W_LumpLength(lump) / sizeof(raw_linedef_t);

	if (numlines == 0)
		I_Error("Bad WAD: level %s contains 0 linedefs.\n", 
				currmap->lump.c_str());

	lines = new line_t[numlines];
		
	Z_Clear(lines, line_t, numlines);

	temp_line_sides = new int[numlines * 2];

	const void *data = W_CacheLumpNum(lump);
	mapline_CRC.AddBlock((const byte*)data, W_LumpLength(lump));

	line_t *ld = lines;
	const raw_linedef_t *mld = (const raw_linedef_t *) data;

	for (int i = 0; i < numlines; i++, mld++, ld++)
	{
		ld->flags = EPI_LE_U16(mld->flags);
		ld->tag = MAX(0, EPI_LE_S16(mld->tag));
		ld->v1 = &vertexes[EPI_LE_U16(mld->start)];
		ld->v2 = &vertexes[EPI_LE_U16(mld->end)];

		ld->special = P_LookupLineType(MAX(0, EPI_LE_S16(mld->special)));

		if (ld->special && ld->special->type == line_walkable)
			ld->flags |= MLF_PassThru;

		if (ld->special && ld->special->type == line_none && 
			(ld->special->s_xspeed || ld->special->s_yspeed || ld->special->scroll_type > ScrollType_None ||
			ld->special->line_effect == LINEFX_VectorScroll || ld->special->line_effect == LINEFX_OffsetScroll ||
			ld->special->line_effect == LINEFX_TaggedOffsetScroll))
			ld->flags |= MLF_PassThru;

		if(ld->special && ld->special->slope_type & SLP_DetailFloor)
			ld->flags |= MLF_PassThru;
			
		if(ld->special && ld->special->slope_type & SLP_DetailCeiling)
			ld->flags |= MLF_PassThru;

		if (ld->special && ld->special == linetypes.Lookup(0)) // Add passthru to unknown/templated
			ld->flags |= MLF_PassThru;

		int side0 = EPI_LE_U16(mld->side_R);
		int side1 = EPI_LE_U16(mld->side_L);

		ComputeLinedefData(ld, side0, side1);

		// check for possible extrafloors, updating the exfloor_max count
		// for the sectors in question.

		if (ld->tag && ld->special && ld->special->ef.type)
		{
			for (int j=0; j < numsectors; j++)
			{
				if (sectors[j].tag != ld->tag)
					continue;

				sectors[j].exfloor_max++;
				numextrafloors++;
			}
		}
	}

	W_DoneWithLump(data);
}

static void LoadHexenLineDefs(int lump)
{
	// -AJA- 2001/08/04: wrote this, based on the Hexen specs.

	if (! W_VerifyLumpName(lump, "LINEDEFS"))
		I_Error("Bad WAD: level %s missing LINEDEFS.\n", 
				currmap->lump.c_str());

	numlines = W_LumpLength(lump) / sizeof(raw_hexen_linedef_t);

	if (numlines == 0)
		I_Error("Bad WAD: level %s contains 0 linedefs.\n", 
				currmap->lump.c_str());

	lines = new line_t[numlines];
		
	Z_Clear(lines, line_t, numlines);

	temp_line_sides = new int[numlines * 2];

	const void *data = W_CacheLumpNum(lump);
	mapline_CRC.AddBlock((const byte*)data, W_LumpLength(lump));

	line_t *ld = lines;
	const raw_hexen_linedef_t *mld = (const raw_hexen_linedef_t *) data;

	for (int i = 0; i < numlines; i++, mld++, ld++)
	{
		ld->flags = EPI_LE_U16(mld->flags) & 0x00FF;
		ld->tag = 0;
		ld->v1 = &vertexes[EPI_LE_U16(mld->start)];
		ld->v2 = &vertexes[EPI_LE_U16(mld->end)];

		// this ignores the activation bits -- oh well
		ld->special = (mld->args[0] == 0) ? NULL :
			linetypes.Lookup(1000 + mld->args[0]);

		int side0 = EPI_LE_U16(mld->side_R);
		int side1 = EPI_LE_U16(mld->side_L);

		ComputeLinedefData(ld, side0, side1);
	}

	W_DoneWithLump(data);
}

static sector_t *DetermineSubsectorSector(subsector_t *ss, int pass)
{
	const seg_t *seg;

	for (seg = ss->segs ; seg != NULL ; seg = seg->sub_next)
	{
		if (seg->miniseg)
			continue;

		// ignore self-referencing linedefs
		if (seg->frontsector == seg->backsector)
			continue;

		return seg->frontsector;
	}

	for (seg = ss->segs ; seg != NULL ; seg = seg->sub_next)
	{
		if (seg->partner == NULL)
			continue;

		// only do this for self-referencing linedefs if the original sector isn't tagged, otherwise
		// save it for the next pass
		if (!(seg->frontsector == seg->backsector && seg->frontsector && seg->frontsector->tag != 0) && 
			seg->partner->front_sub->sector != NULL)
			return seg->partner->front_sub->sector;
	}

	if (pass == 1)
	{
		for (seg = ss->segs ; seg != NULL ; seg = seg->sub_next)
		{
			if (! seg->miniseg)
				return seg->frontsector;
		}
	}

	if (pass == 2)
		return &sectors[0];

	return NULL;
}

static bool AssignSubsectorsPass(int pass)
{
	// pass 0 : ignore self-ref lines.
	// pass 1 : use them.
	// pass 2 : handle extreme brokenness.
	//
	// returns true if progress was made.

	int null_count = 0;
	bool progress  = false;

	for (int i = 0 ; i < numsubsectors ; i++)
	{
		subsector_t *ss = &subsectors[i];

		if (ss->sector == NULL)
		{
			null_count += 1;

			ss->sector = DetermineSubsectorSector(ss, pass);

			if (ss->sector != NULL)
			{
				progress = true;

				// link subsector into parent sector's list.
				// order is not important, so add it to the head of the list.
				ss->sec_next = ss->sector->subsectors;
				ss->sector->subsectors = ss;
			}
		}
	}

	/* DEBUG
	fprintf(stderr, "** pass %d : %d : %d\n", pass, null_count, progress ? 1 : 0);
	*/

	return progress;
}

static void AssignSubsectorsToSectors()
{
	// AJA 2022: this attempts to improve handling of self-referencing lines
	//           (i.e. ones with the same sector on both sides).  Subsectors
	//           touching such lines should NOT be assigned to that line's
	//           sector, but rather to the "outer" sector.

	while (AssignSubsectorsPass(0))
	{ }

	while (AssignSubsectorsPass(1))
	{ }

	// the above *should* handle everything, so this pass is only needed
	// for extremely broken nodes or maps.
	AssignSubsectorsPass(2);
}

// Adapted from EDGE 2.x's ZNode loading routine; only handles XGL3/ZGL3 as that is all
// our built-in AJBSP produces now
static void LoadXGL3Nodes(int lumpnum)
{
	int i, xglen = 0;
	byte *xgldata = nullptr;
	std::vector<byte> zgldata;
	byte *td = nullptr;

	I_Debugf("LoadXGL3Nodes:\n");

	xglen = W_LumpLength(lumpnum);
	xgldata = (byte *)W_CacheLumpNum(lumpnum);
	if (!xgldata)
		I_Error("LoadXGL3Nodes: Couldn't load lump\n");

	if (xglen < 12)
	{
		W_DoneWithLump(xgldata);
		I_Error("LoadXGL3Nodes: Lump too short\n");
	}

	if(!memcmp(xgldata, "XGL3", 4))
		I_Debugf(" AJBSP uncompressed GL nodes v3\n");
	else if(!memcmp(xgldata, "ZGL3", 4))
	{
		I_Debugf(" AJBSP compressed GL nodes v3\n");
		zgldata.resize(xglen);
		z_stream zgl_stream;
		memset(&zgl_stream, 0, sizeof(z_stream));
		zgl_stream.next_in = &xgldata[4];
		zgl_stream.avail_in = xglen - 4;
		zgl_stream.next_out = zgldata.data();
		zgl_stream.avail_out = zgldata.size();
		inflateInit2(&zgl_stream, MZ_DEFAULT_WINDOW_BITS);
		int inflate_status;
		for (;;)
		{	
			inflate_status = inflate(&zgl_stream, Z_NO_FLUSH);
			if (inflate_status == MZ_OK || inflate_status == MZ_BUF_ERROR) // Need to resize output buffer
			{
				zgldata.resize(zgldata.size() * 2);
				zgl_stream.next_out = &zgldata[zgl_stream.total_out];
				zgl_stream.avail_out = zgldata.size() - zgl_stream.total_out;
			}
			else if (inflate_status == Z_STREAM_END)
			{
				inflateEnd(&zgl_stream);
				zgldata.resize(zgl_stream.total_out);
				zgldata.shrink_to_fit();
				break;
			}
			else
				I_Error("LoadXGL3Nodes: Failed to decompress ZGL3 nodes!\n");
		}	
	}
	else
	{
		static char xgltemp[6];
		Z_StrNCpy(xgltemp, (char *)xgldata, 4);
		W_DoneWithLump(xgldata);
		I_Error("LoadXGL3Nodes: Unrecognized node type %s\n", xgltemp);
	}

	if (!zgldata.empty())
		td = zgldata.data();
	else
		td = &xgldata[4];

	// after signature, 1st u32 is number of original vertexes - should be <= numvertexes
	int oVerts = EPI_LE_U32(*(uint32_t*)td);
	td += 4;
	if (oVerts > numvertexes)
	{
		W_DoneWithLump(xgldata);
		I_Error("LoadXGL3Nodes: Vertex/Node mismatch\n");
	}

	// 2nd u32 is the number of extra vertexes added by ajbsp
	int nVerts = EPI_LE_U32(*(uint32_t*)td);
	td += 4;
	I_Debugf("LoadXGL3Nodes: Orig Verts = %d, New Verts = %d, Map Verts = %d\n", oVerts, nVerts, numvertexes);

	gl_vertexes = new vec2_t[nVerts];
	num_gl_vertexes = nVerts;

	// fill in new vertexes
	vec2_t *vv = gl_vertexes;
	for (i=0; i<nVerts; i++, vv++)
	{
		// convert signed 16.16 fixed point to float
		vv->x = (float)EPI_LE_S32(*(int *)td) / 65536.0f;
		td += 4;
		vv->y = (float)EPI_LE_S32(*(int *)td) / 65536.0f;
		td += 4;
	}

	// new vertexes is followed by the subsectors
	numsubsectors = EPI_LE_S32(*(int *)td);
	td += 4;
	if (numsubsectors <= 0)
	{
		W_DoneWithLump(xgldata);
		I_Error("LoadXGL3Nodes: No subsectors\n");
	}
	I_Debugf("LoadXGL3Nodes: Num SSECTORS = %d\n", numsubsectors);

	subsectors = new subsector_t[numsubsectors];
	Z_Clear(subsectors, subsector_t, numsubsectors);

	int *ss_temp = new int[numsubsectors];
	int xglSegs = 0;
	for (i=0; i<numsubsectors; i++)
	{
		int countsegs = EPI_LE_S32(*(int*)td);
		td += 4;
		ss_temp[i] = countsegs;
		xglSegs += countsegs;
	}

	// subsectors are followed by the segs
	numsegs = EPI_LE_S32(*(int *)td);
	td += 4;
	if (numsegs != xglSegs)
	{
		W_DoneWithLump(xgldata);
		I_Error("LoadXGL3Nodes: Incorrect number of segs in nodes\n");
	}
	I_Debugf("LoadXGL3Nodes: Num SEGS = %d\n", numsegs);

	segs = new seg_t[numsegs];
	Z_Clear(segs, seg_t, numsegs);
	seg_t *seg = segs;

	for (i = 0; i < numsegs; i++, seg++)
	{
		unsigned int v1num;
		int linedef, partner, side;

		v1num = EPI_LE_U32(*(uint32_t*)td);
		td += 4;
		partner = EPI_LE_S32(*(int32_t*)td);
		td += 4;
		linedef = EPI_LE_S32(*(int32_t*)td);
		td += 4;
		side = (int)(*td);
		td += 1;

		if (v1num < (uint32_t)oVerts)
			seg->v1 = &vertexes[v1num];
		else
			seg->v1 = &gl_vertexes[v1num - oVerts];

		seg->side = side ? 1 : 0;

		if (partner == -1)
			seg->partner = NULL;
		else
		{
			SYS_ASSERT(partner < numsegs);  // sanity check
			seg->partner = &segs[partner];
		}

		SegCommonStuff(seg, linedef);

		// The following fields are filled out elsewhere:
		//     sub_next, front_sub, back_sub, frontsector, backsector.

		seg->sub_next = SEG_INVALID;
		seg->front_sub = seg->back_sub = SUB_INVALID;
	}

	I_Debugf("LoadXGL3Nodes: Post-process subsectors\n");
	// go back and fill in subsectors
	subsector_t *ss = subsectors;
	xglSegs = 0;
	for (i=0; i<numsubsectors; i++, ss++)
	{
		int countsegs = ss_temp[i];
		int firstseg  = xglSegs;
		xglSegs += countsegs;

		// go back and fill in v2 from v1 of next seg and do calcs that needed both
		seg = &segs[firstseg];
		for (int j = 0; j < countsegs; j++, seg++)
		{
			seg->v2 = j == (countsegs - 1) ? segs[firstseg].v1 : segs[firstseg + j + 1].v1;

			seg->angle  = R_PointToAngle(seg->v1->x, seg->v1->y,
				seg->v2->x, seg->v2->y);

			seg->length = R_PointToDist(seg->v1->x, seg->v1->y,
				seg->v2->x, seg->v2->y);
		}

		// -AJA- 1999/09/23: New linked list for the segs of a subsector
		//       (part of true bsp rendering).
		seg_t **prevptr = &ss->segs;

		if (countsegs == 0)
			I_Error("LoadXGL3Nodes: level %s has invalid SSECTORS.\n", currmap->lump.c_str());

		ss->sector = NULL;
		ss->thinglist = NULL;

		// this is updated when the nodes are loaded
		ss->bbox = dummy_bbox;

		for (int j = 0; j < countsegs; j++)
		{
			seg_t *cur = &segs[firstseg + j];

			*prevptr = cur;
			prevptr = &cur->sub_next;

			cur->front_sub = ss;
			cur->back_sub = NULL;

			//I_Debugf("  ssec = %d, seg = %d\n", i, firstseg + j);
		}
		//I_Debugf("LoadZNodes: ssec = %d, fseg = %d, cseg = %d\n", i, firstseg, countsegs);

		*prevptr = NULL;
	}
	delete [] ss_temp; //CA 9.30.18: allocated with new but released using delete, added [] between delete and ss_temp

	I_Debugf("LoadXGL3Nodes: Read GL nodes\n");
	// finally, read the nodes
	// NOTE: no nodes is okay (a basic single sector map). -AJA-
	numnodes = EPI_LE_U32(*(uint32_t*)td);
	td += 4;
	I_Debugf("LoadXGL3Nodes: Num nodes = %d\n", numnodes);

	nodes = new node_t[numnodes+1];
	Z_Clear(nodes, node_t, numnodes);
	node_t *nd = nodes;

	for (i=0; i<numnodes; i++, nd++)
	{
	// To imitate Andrew's feelings about stuff like this, this is a filthy HACK for ARM32 and is 
	// in no way a decent solution, but it works - Dasho
#ifdef __arm__
		int td_int = EPI_LE_S32(*(int*)td);
		std::string arm_nonsense = epi::STR_Format("%d", td_int);
		nd->div.x  = (float)td_int / 65536.0f;
		td += 4;
		td_int = EPI_LE_S32(*(int*)td);
		arm_nonsense = epi::STR_Format("%d", td_int);
		nd->div.y  = (float)td_int / 65536.0f;
		td += 4;
		td_int = EPI_LE_S32(*(int*)td);
		arm_nonsense = epi::STR_Format("%d", td_int);
		nd->div.dx = (float)td_int / 65536.0f;
		td += 4;
		td_int = EPI_LE_S32(*(int*)td);
		arm_nonsense = epi::STR_Format("%d", td_int);
		nd->div.dy = (float)td_int / 65536.0f;
		td += 4;
#else
		nd->div.x  = (float)EPI_LE_S32(*(int*)td) / 65536.0f;
		td += 4;
		nd->div.y  = (float)EPI_LE_S32(*(int*)td) / 65536.0f;
		td += 4;
		nd->div.dx = (float)EPI_LE_S32(*(int*)td) / 65536.0f;
		td += 4;
		nd->div.dy = (float)EPI_LE_S32(*(int*)td) / 65536.0f;
		td += 4;
#endif

		nd->div_len = R_PointToDist(0, 0, nd->div.dx, nd->div.dy);

		for (int j=0; j<2; j++)
			for (int k=0; k<4; k++)
			{
				nd->bbox[j][k] = (float)EPI_LE_S16(*(int16_t*)td);
				td += 2;
			}

		for (int j=0; j<2; j++)
		{
			nd->children[j] = EPI_LE_U32(*(uint32_t*)td);
			td += 4;

			// update bbox pointers in subsector
			if (nd->children[j] & NF_V5_SUBSECTOR)
			{
				subsector_t *sss = subsectors + (nd->children[j] & ~NF_V5_SUBSECTOR);
				sss->bbox = &nd->bbox[j][0];
			}
		}
	}

	AssignSubsectorsToSectors();

	I_Debugf("LoadXGL3Nodes: Setup root node\n");
	SetupRootNode();

	I_Debugf("LoadXGL3Nodes: Finished\n");
	W_DoneWithLump(xgldata);
	zgldata.clear();
}

static void LoadUDMFVertexes(parser_t *psr)
{
	char ident[128];
	char val[128];
	int i = 0;

	I_Debugf("LoadUDMFVertexes: parsing TEXTMAP\n");
	numvertexes = 0;

	psr->next = 0; // restart from start of lump
	while (1)
	{
		if (!GetNextBlock(psr, (uint8_t*)ident))
			break;

		if (epi::case_cmp(ident, "vertex") == 0)
		{
			// count vertex blocks
			while (1)
			{
				if (!GetNextAssign(psr, (uint8_t*)ident, (uint8_t*)val))
				{
					uint8_t *lp = psr->line;
					while (*lp != 0 && *lp != 0x7D)
						lp++; // find end of line or '}'
					if (*lp == 0x7D)
					{
						numvertexes++;
						break; // end of block
					}
					if (psr->next >= psr->length)
					{
						numvertexes++;
						break; // end of lump
					}

					continue; // skip line
				}
			}
		}
	}

	vertexes = new vec2_t[numvertexes];
	zvertexes = new vec2_t[numvertexes];

	psr->next = 0; // restart from start of lump
	while (1)
	{
		if (!GetNextBlock(psr, (uint8_t*)ident))
			break;

		if (epi::case_cmp(ident, "vertex") == 0)
		{
			float x = 0.0f, y = 0.0f;
			float zf = -40000.0f, zc = 40000.0f;

			// process vertex block
			while (1)
			{
				if (!GetNextAssign(psr, (uint8_t*)ident, (uint8_t*)val))
				{
					uint8_t *lp = psr->line;
					while (*lp != 0 && *lp != 0x7D)
						lp++; // find end of line or '}'
					if (*lp == 0x7D)
					{
						break; // end of block
					}
					if (psr->next >= psr->length)
					{
						break; // end of lump
					}

					continue; // skip line
				}
				// process assignment
				if (epi::case_cmp(ident, "x") == 0)
				{
					x = str2float(val, 0.0f);
				}
				else if (epi::case_cmp(ident, "y") == 0)
				{
					y = str2float(val, 0.0f);
				}
				else if (epi::case_cmp(ident, "zfloor") == 0)
				{
					zf = str2float(val, -40000.0f);
				}
				else if (epi::case_cmp(ident, "zceiling") == 0)
				{
					zc = str2float(val, 40000.0f);
				}
			}

			vec2_t *vv = vertexes + i;
			vv->x = x;
			vv->y = y;
			vv = zvertexes + i;
			vv->x = zf;
			vv->y = zc;

			i++;
		}
	}

	I_Debugf("LoadUDMFVertexes: finished parsing TEXTMAP\n");
}

static void LoadUDMFSectors(parser_t *psr)
{
	char ident[128];
	char val[128];
	int i = 0;

	I_Debugf("LoadUDMFSectors: parsing TEXTMAP\n");
	numsectors = 0;

	psr->next = 0; // restart from start of lump
	while (1)
	{
		if (!GetNextBlock(psr, (uint8_t*)ident))
			break;

		if (epi::case_cmp(ident, "sector") == 0)
		{
			// count sector blocks
			while (1)
			{
				if (!GetNextAssign(psr, (uint8_t*)ident, (uint8_t*)val))
				{
					uint8_t *lp = psr->line;
					while (*lp != 0 && *lp != 0x7D)
						lp++; // find end of line or '}'
					if (*lp == 0x7D)
					{
						numsectors++;
						break; // end of block
					}
					if (psr->next >= psr->length)
					{
						numsectors++;
						break; // end of lump
					}

					continue; // skip line
				}
			}
		}
	}

	sectors = new sector_t[numsectors];
	Z_Clear(sectors, sector_t, numsectors);

	psr->next = 0; // restart from start of lump
	while (1)
	{
		if (!GetNextBlock(psr, (uint8_t*)ident))
			break;

		if (epi::case_cmp(ident, "sector") == 0)
		{
			float cz = 0.0f, fz = 0.0f;
			int light = 160, type = 0, tag = 0;
			char floor_tex[10];
			char ceil_tex[10];
			strcpy(floor_tex, "-");
			strcpy(ceil_tex, "-");

			// process sector block
			while (1)
			{
				if (!GetNextAssign(psr, (uint8_t*)ident, (uint8_t*)val))
				{
					uint8_t *lp = psr->line;
					while (*lp != 0 && *lp != 0x7D)
						lp++; // find end of line or '}'
					if (*lp == 0x7D)
					{
						break; // end of block
					}
					if (psr->next >= psr->length)
					{
						break; // end of lump
					}

					continue; // skip line
				}
				// process assignment
				if (epi::case_cmp(ident, "heightfloor") == 0)
				{
					fz = str2float(val, 0.0f);
				}
				else if (epi::case_cmp(ident, "heightceiling") == 0)
				{
					cz = str2float(val, 0.0f);
				}
				else if (epi::case_cmp(ident, "texturefloor") == 0)
				{
					Z_StrNCpy(floor_tex, val, 8);
				}
				else if (epi::case_cmp(ident, "textureceiling") == 0)
				{
					Z_StrNCpy(ceil_tex, val, 8);
				}
				else if (epi::case_cmp(ident, "lightlevel") == 0)
				{
					light = str2int(val, 160);
				}
				else if (epi::case_cmp(ident, "special") == 0)
				{
					type = str2int(val, 0);
				}
				else if (epi::case_cmp(ident, "id") == 0)
				{
					tag = str2int(val, 0);
				}
			}

			sector_t *ss = sectors + i;

			ss->f_h = fz;
			ss->c_h = cz;

			// return to wolfenstein?
			if (m_goobers.d)
			{
				ss->f_h = 0;
				ss->c_h = (fz == cz) ? 0 : 128.0f;
			}

			ss->floor.translucency = VISIBLE;
			ss->floor.x_mat.x = 1;  ss->floor.x_mat.y = 0;
			ss->floor.y_mat.x = 0;  ss->floor.y_mat.y = 1;

			ss->ceil = ss->floor;

			ss->floor.image = W_ImageLookup(floor_tex, INS_Flat);
			ss->ceil.image = W_ImageLookup(ceil_tex, INS_Flat);

			if (! ss->floor.image)
			{
				I_Warning("Bad Level: sector #%d has missing floor texture.\n", i);
				ss->floor.image = W_ImageLookup("FLAT1", INS_Flat);
			}
			if (! ss->ceil.image)
			{
				I_Warning("Bad Level: sector #%d has missing ceiling texture.\n", i);
				ss->ceil.image = ss->floor.image;
			}

			// convert negative tags to zero
			ss->tag = MAX(0, tag);

			ss->props.lightlevel = light;

			// convert negative types to zero
			ss->props.type = MAX(0, type);
			ss->props.special = P_LookupSectorType(ss->props.type);

			ss->exfloor_max = 0;

			ss->props.colourmap = NULL;

			ss->props.gravity   = GRAVITY;
			ss->props.friction  = FRICTION;
			ss->props.viscosity = VISCOSITY;
			ss->props.drag      = DRAG;

			ss->p = &ss->props;

			ss->sound_player = -1;

			// -AJA- 1999/07/29: Keep sectors with same tag in a list.
			GroupSectorTags(ss, sectors, i);

			i++;
		}
	}

	I_Debugf("LoadUDMFSectors: finished parsing TEXTMAP\n");
}

static void LoadUDMFSideDefs(parser_t *psr)
{
	char ident[128];
	char val[128];

	int nummapsides = 0;

	sides = new side_t[numsides];
	Z_Clear(sides, side_t, numsides);

	I_Debugf("LoadUDMFSideDefs: parsing TEXTMAP\n");
	nummapsides = 0;

	psr->next = 0; // restart from start of lump
	while (1)
	{
		if (!GetNextBlock(psr, (uint8_t*)ident))
			break;

		if (epi::case_cmp(ident, "sidedef") == 0)
		{
			float x = 0, y = 0;
			int sec_num = 0;
			char top_tex[10];
			char bottom_tex[10];
			char middle_tex[10];
			strcpy(top_tex, "-");
			strcpy(bottom_tex, "-");
			strcpy(middle_tex, "-");

			// process sidedef block
			while (1)
			{
				if (!GetNextAssign(psr, (uint8_t*)ident, (uint8_t*)val))
				{
					uint8_t *lp = psr->line;
					while (*lp != 0 && *lp != 0x7D)
						lp++; // find end of line or '}'
					if (*lp == 0x7D)
					{
						nummapsides++;
						break; // end of block
					}
					if (psr->next >= psr->length)
					{
						nummapsides++;
						break; // end of lump
					}

					continue; // skip line
				}
				// process assignment
				if (epi::case_cmp(ident, "offsetx") == 0)
				{
					x = str2float(val, 0);
				}
				else if (epi::case_cmp(ident, "offsety") == 0)
				{
					y = str2float(val, 0);
				}
				else if (epi::case_cmp(ident, "texturetop") == 0)
				{
					Z_StrNCpy(top_tex, val, 8);
				}
				else if (epi::case_cmp(ident, "texturebottom") == 0)
				{
					Z_StrNCpy(bottom_tex, val, 8);
				}
				else if (epi::case_cmp(ident, "texturemiddle") == 0)
				{
					Z_StrNCpy(middle_tex, val, 8);
				}
				else if (epi::case_cmp(ident, "sector") == 0)
				{
					sec_num = str2int(val, 0);
				}
			}

			SYS_ASSERT(nummapsides <= numsides);  // sanity check

			side_t *sd = sides + nummapsides - 1;

			sd->top.translucency = VISIBLE;
			sd->top.offset.x = x;
			sd->top.offset.y = y;
			sd->top.x_mat.x = 1;  sd->top.x_mat.y = 0;
			sd->top.y_mat.x = 0;  sd->top.y_mat.y = 1;

			sd->middle = sd->top;
			sd->bottom = sd->top;

			sd->sector = &sectors[sec_num];

			sd->top.image = W_ImageLookup(top_tex, INS_Texture, ILF_Null);

			if (sd->top.image == NULL)
			{
				if (m_goobers.d)
					sd->top.image = W_ImageLookup(bottom_tex, INS_Texture);
				else
					sd->top.image = W_ImageLookup(top_tex, INS_Texture);
			}

			sd->middle.image = W_ImageLookup(middle_tex, INS_Texture);
			sd->bottom.image = W_ImageLookup(bottom_tex, INS_Texture);

			// handle BOOM colourmaps with [242] linetype
			sd->top   .boom_colmap = colourmaps.Lookup(top_tex);
			sd->middle.boom_colmap = colourmaps.Lookup(middle_tex);
			sd->bottom.boom_colmap = colourmaps.Lookup(bottom_tex);
		}
	}

	I_Debugf("LoadUDMFSideDefs: post-processing linedefs & sidedefs\n");

	// post-process linedefs & sidedefs

	SYS_ASSERT(temp_line_sides);

	side_t *sd = sides;

	for (int i=0; i<numlines; i++)
	{
		line_t *ld = lines + i;

		int side0 = temp_line_sides[i*2 + 0];
		int side1 = temp_line_sides[i*2 + 1];

		SYS_ASSERT(side0 != -1);

		if (side0 >= nummapsides)
		{
			I_Warning("Bad WAD: level %s linedef #%d has bad RIGHT side.\n",
				currmap->lump.c_str(), i);
			side0 = nummapsides-1;
		}

		if (side1 != -1 && side1 >= nummapsides)
		{
			I_Warning("Bad WAD: level %s linedef #%d has bad LEFT side.\n",
				currmap->lump.c_str(), i);
			side1 = nummapsides-1;
		}

		ld->side[0] = sd;
		if (sd->middle.image && (side1 != -1))
		{
			sd->midmask_offset = sd->middle.offset.y;
			sd->middle.offset.y = 0;
		}
		ld->frontsector = sd->sector;
		sd++;

		if (side1 != -1)
		{
			ld->side[1] = sd;
			if (sd->middle.image)
			{
				sd->midmask_offset = sd->middle.offset.y;
				sd->middle.offset.y = 0;
			}
			ld->backsector = sd->sector;
			sd++;
		}

		SYS_ASSERT(sd <= sides + numsides);
	}

	SYS_ASSERT(sd == sides + numsides);

	I_Debugf("LoadUDMFSideDefs: finished parsing TEXTMAP\n");
}

static void LoadUDMFLineDefs(parser_t *psr)
{
	char ident[128];
	char val[128];
	int i = 0;

	I_Debugf("LoadUDMFLineDefs: parsing TEXTMAP\n");
	numlines = 0;

	psr->next = 0; // restart from start of lump
	while (1)
	{
		if (!GetNextBlock(psr, (uint8_t*)ident))
			break;

		if (epi::case_cmp(ident, "linedef") == 0)
		{
			// count linedef blocks
			while (1)
			{
				if (!GetNextAssign(psr, (uint8_t*)ident, (uint8_t*)val))
				{
					uint8_t *lp = psr->line;
					while (*lp != 0 && *lp != 0x7D)
						lp++; // find end of line or '}'
					if (*lp == 0x7D)
					{
						numlines++;
						break; // end of block
					}
					if (psr->next >= psr->length)
					{
						numlines++;
						break; // end of lump
					}

					continue; // skip line
				}
			}
		}
	}

	lines = new line_t[numlines];
	Z_Clear(lines, line_t, numlines);
	temp_line_sides = new int[numlines * 2];

	psr->next = 0; // restart from start of lump
	while (1)
	{
		if (!GetNextBlock(psr, (uint8_t*)ident))
			break;

		if (epi::case_cmp(ident, "linedef") == 0)
		{
			int flags = 0, v1 = 0, v2 = 0;
			int side0 = -1, side1 = -1, tag = -1;
			int special = 0;

			// process lindef block
			while (1)
			{
				if (!GetNextAssign(psr, (uint8_t*)ident, (uint8_t*)val))
				{
					uint8_t *lp = psr->line;
					while (*lp != 0 && *lp != 0x7D)
						lp++; // find end of line or '}'
					if (*lp == 0x7D)
					{
						break; // end of block
					}
					if (psr->next >= psr->length)
					{
						break; // end of lump
					}

					continue; // skip line
				}
				// process assignment
				if (epi::case_cmp(ident, "id") == 0)
				{
					tag = str2int(val, -1);
				}
				else if (epi::case_cmp(ident, "v1") == 0)
				{
					v1 = str2int(val, 0);
				}
				else if (epi::case_cmp(ident, "v2") == 0)
				{
					v2 = str2int(val, 0);
				}
				else if (epi::case_cmp(ident, "special") == 0)
				{
					special = str2int(val, 0);
				}
				else if (epi::case_cmp(ident, "sidefront") == 0)
				{
					side0 = str2int(val, -1);
				}
				else if (epi::case_cmp(ident, "sideback") == 0)
				{
					side1 = str2int(val, -1);
				}
				else if (epi::case_cmp(ident, "blocking") == 0)
				{
					if (str2bool(val))
						flags |= 0x0001;
				}
				else if (epi::case_cmp(ident, "blockmonsters") == 0)
				{
					if (str2bool(val))
						flags |= 0x0002;
				}
				else if (epi::case_cmp(ident, "twosided") == 0)
				{
					if (str2bool(val))
						flags |= 0x0004;
				}
				else if (epi::case_cmp(ident, "dontpegtop") == 0)
				{
					if (str2bool(val))
						flags |= 0x0008;
				}
				else if (epi::case_cmp(ident, "dontpegbottom") == 0)
				{
					if (str2bool(val))
						flags |= 0x0010;
				}
				else if (epi::case_cmp(ident, "secret") == 0)
				{
					if (str2bool(val))
						flags |= 0x0020;
				}
				else if (epi::case_cmp(ident, "blocksound") == 0)
				{
					if (str2bool(val))
						flags |= 0x0040;
				}
				else if (epi::case_cmp(ident, "dontdraw") == 0)
				{
					if (str2bool(val))
						flags |= 0x0080;
				}
				else if (epi::case_cmp(ident, "mapped") == 0)
				{
					if (str2bool(val))
						flags |= 0x0100;
				}
				else if (epi::case_cmp(ident, "passuse") == 0)
				{
					if (str2bool(val))
						flags |= 0x0200; // BOOM flag
				}
			}

			line_t *ld = lines + i;

			ld->flags = flags;
			ld->tag = MAX(0, tag);
			ld->v1 = &vertexes[v1];
			ld->v2 = &vertexes[v2];

			ld->special = P_LookupLineType(MAX(0, special));

			if (ld->special && ld->special->type == line_walkable)
				ld->flags |= MLF_PassThru;

			if (ld->special && ld->special->type == line_none && 
				(ld->special->s_xspeed || ld->special->s_yspeed || ld->special->scroll_type > ScrollType_None ||
				ld->special->line_effect == LINEFX_VectorScroll || ld->special->line_effect == LINEFX_OffsetScroll ||
				ld->special->line_effect == LINEFX_TaggedOffsetScroll))
				ld->flags |= MLF_PassThru;

			if(ld->special && ld->special->slope_type & SLP_DetailFloor)
				ld->flags |= MLF_PassThru;
			
			if(ld->special && ld->special->slope_type & SLP_DetailCeiling)
				ld->flags |= MLF_PassThru;

			if (ld->special && ld->special == linetypes.Lookup(0)) // Add passthru to unknown/templated
				ld->flags |= MLF_PassThru;

			ComputeLinedefData(ld, side0, side1);

			if (ld->tag && ld->special && ld->special->ef.type)
			{
				for (int j=0; j < numsectors; j++)
				{
					if (sectors[j].tag != ld->tag)
						continue;

					sectors[j].exfloor_max++;
					numextrafloors++;
				}
			}

			i++;
		}
	}

	I_Debugf("LoadUDMFLineDefs: finished parsing TEXTMAP\n");
}

static void LoadUDMFThings(parser_t *psr)
{
	char ident[128];
	char val[128];

	int numthings = 0;

	I_Debugf("LoadUDMFThings: parsing TEXTMAP\n");

	unknown_thing_map.clear();

	psr->next = 0; // restart from start of lump
	while (1)
	{
		if (!GetNextBlock(psr, (uint8_t*)ident))
			break;

		if (epi::case_cmp(ident, "thing") == 0)
		{
			float x = 0.0f, y = 0.0f, z = 0.0f;
			angle_t angle = ANG0;
			int options = MTF_NOT_SINGLE | MTF_NOT_DM | MTF_NOT_COOP;
			int typenum = -1;
			int tag = 0;
			const mobjtype_c *objtype;

			// process thing block
			while (1)
			{
				if (!GetNextAssign(psr, (uint8_t*)ident, (uint8_t*)val))
				{
					uint8_t *lp = psr->line;
					while (*lp != 0 && *lp != 0x7D)
						lp++; // find end of line or '}'
					if (*lp == 0x7D)
						break; // end of block
					if (psr->next >= psr->length)
						break; // end of lump

					continue; // skip line
				}
				// process assignment
				if (epi::case_cmp(ident, "id") == 0)
				{
					tag = str2int(val, 0);
				}
				else if (epi::case_cmp(ident, "x") == 0)
				{
					x = str2float(val, 0.0f);
				}
				else if (epi::case_cmp(ident, "y") == 0)
				{
					y = str2float(val, 0.0f);
				}
				else if (epi::case_cmp(ident, "height") == 0)
				{
					z = str2float(val, 0.0f);
				}
				else if (epi::case_cmp(ident, "angle") == 0)
				{
					int ta = str2int(val, 0);
					angle = FLOAT_2_ANG((float)ta);
				}
				else if (epi::case_cmp(ident, "type") == 0)
				{
					typenum = str2int(val, 0);
				}
				else if (epi::case_cmp(ident, "skill1") == 0)
				{
					options |= MTF_EASY;
				}
				else if (epi::case_cmp(ident, "skill2") == 0)
				{
					if (str2bool(val))
						options |= MTF_EASY;
				}
				else if (epi::case_cmp(ident, "skill3") == 0)
				{
					if (str2bool(val))
						options |= MTF_NORMAL;
				}
				else if (epi::case_cmp(ident, "skill4") == 0)
				{
					if (str2bool(val))
						options |= MTF_HARD;
				}
				else if (epi::case_cmp(ident, "skill5") == 0)
				{
					if (str2bool(val))
						options |= MTF_HARD;
				}
				else if (epi::case_cmp(ident, "ambush") == 0)
				{
					if (str2bool(val))
						options |= MTF_AMBUSH;
				}
				else if (epi::case_cmp(ident, "single") == 0)
				{
					if (str2bool(val))
						options &= ~MTF_NOT_SINGLE;
				}
				else if (epi::case_cmp(ident, "dm") == 0)
				{
					if (str2bool(val))
						options &= ~MTF_NOT_DM;
				}
				else if (epi::case_cmp(ident, "coop") == 0)
				{
					if (str2bool(val))
						options &= ~MTF_NOT_COOP;
				}
				// MBF flag
				else if (epi::case_cmp(ident, "friend") == 0)
				{
					if (str2bool(val))
						options |= MTF_FRIEND;
				}
			}

			objtype = mobjtypes.Lookup(typenum);

			// MOBJTYPE not found, don't crash out: JDS Compliance.
			// -ACB- 1998/07/21
			if (objtype == NULL)
			{
				UnknownThingWarning(typenum, x, y);
				continue;
			}

			sector_t *sec = R_PointInSubsector(x, y)->sector;

			if (objtype->flags & MF_SPAWNCEILING)
				z += sec->c_h - objtype->height;
			else
				z += sec->f_h;

			SpawnMapThing(objtype, x, y, z, sec, angle, options, tag);

			numthings++;
		}
	}

	mapthing_NUM = numthings;

	I_Debugf("LoadUDMFThings: finished parsing TEXTMAP\n");
}

static void TransferMapSideDef(const raw_sidedef_t *msd, side_t *sd,
							   bool two_sided)
{
	char upper_tex [10];
	char middle_tex[10];
	char lower_tex [10];

	int sec_num = EPI_LE_S16(msd->sector);

	sd->top.translucency = VISIBLE;
	sd->top.offset.x = EPI_LE_S16(msd->x_offset);
	sd->top.offset.y = EPI_LE_S16(msd->y_offset);
	sd->top.x_mat.x = 1;  sd->top.x_mat.y = 0;
	sd->top.y_mat.x = 0;  sd->top.y_mat.y = 1;

	sd->middle = sd->top;
	sd->bottom = sd->top;

	if (sec_num < 0)
	{
		I_Warning("Level %s has sidedef with bad sector ref (%d)\n",
			currmap->lump.c_str(), sec_num);
		sec_num = 0;
	}
	sd->sector = &sectors[sec_num];

	Z_StrNCpy(upper_tex,  msd->upper_tex, 8);
	Z_StrNCpy(middle_tex, msd->mid_tex,   8);
	Z_StrNCpy(lower_tex,  msd->lower_tex, 8);

	sd->top.image = W_ImageLookup(upper_tex, INS_Texture, ILF_Null);

	if (sd->top.image == NULL)
	{
		if (m_goobers.d)
			sd->top.image = W_ImageLookup(upper_tex, INS_Texture);
		else
			sd->top.image = W_ImageLookup(upper_tex, INS_Texture);
	}

	sd->middle.image = W_ImageLookup(middle_tex, INS_Texture);
	sd->bottom.image = W_ImageLookup(lower_tex,  INS_Texture);

	// handle BOOM colourmaps with [242] linetype
	sd->top   .boom_colmap = colourmaps.Lookup(upper_tex);
	sd->middle.boom_colmap = colourmaps.Lookup(middle_tex);
	sd->bottom.boom_colmap = colourmaps.Lookup(lower_tex);

	if (sd->middle.image && two_sided)
	{
		sd->midmask_offset = sd->middle.offset.y;
		sd->middle.offset.y = 0;
	}

	if (sd->top.image && fabs(sd->top.offset.y) > IM_HEIGHT(sd->top.image))
		sd->top.offset.y = fmodf(sd->top.offset.y, IM_HEIGHT(sd->top.image));

	if (sd->middle.image && fabs(sd->middle.offset.y) > IM_HEIGHT(sd->middle.image))
		sd->middle.offset.y = fmodf(sd->middle.offset.y, IM_HEIGHT(sd->middle.image));

	if (sd->bottom.image && fabs(sd->bottom.offset.y) > IM_HEIGHT(sd->bottom.image))
		sd->bottom.offset.y = fmodf(sd->bottom.offset.y, IM_HEIGHT(sd->bottom.image));

#if 0  // -AJA- 2005/01/13: DISABLED (see my log for explanation) 
	{
		// -AJA- 2004/09/20: fix texture alignment for some rare cases
		//       where the texture height is non-POW2 (e.g. 64x72) and
		//       a negative Y offset was used.

		if (sd->top.offset.y < 0 && sd->top.image)
			sd->top.offset.y += IM_HEIGHT(sd->top.image);

		if (sd->middle.offset.y < 0 && sd->middle.image)
			sd->middle.offset.y += IM_HEIGHT(sd->middle.image);

		if (sd->bottom.offset.y < 0 && sd->bottom.image)
			sd->bottom.offset.y += IM_HEIGHT(sd->bottom.image);
	}
#endif
}

static void LoadSideDefs(int lump)
{
	int i;
	const void *data;
	const raw_sidedef_t *msd;
	side_t *sd;

	int nummapsides;

	if (! W_VerifyLumpName(lump, "SIDEDEFS"))
		I_Error("Bad WAD: level %s missing SIDEDEFS.\n", 
				currmap->lump.c_str());

	nummapsides = W_LumpLength(lump) / sizeof(raw_sidedef_t);

	if (nummapsides == 0)
		I_Error("Bad WAD: level %s contains 0 sidedefs.\n", 
				currmap->lump.c_str());

	sides = new side_t[numsides];

	Z_Clear(sides, side_t, numsides);

	data = W_CacheLumpNum(lump);
	msd = (const raw_sidedef_t *) data;

	sd = sides;

	SYS_ASSERT(temp_line_sides);

	for (i = 0; i < numlines; i++)
	{
		line_t *ld = lines + i;

		int side0 = temp_line_sides[i*2 + 0];
		int side1 = temp_line_sides[i*2 + 1];

		SYS_ASSERT(side0 != -1);

		if (side0 >= nummapsides)
		{
			I_Warning("Bad WAD: level %s linedef #%d has bad RIGHT side.\n",
				currmap->lump.c_str(), i);
			side0 = nummapsides-1;
		}

		if (side1 != -1 && side1 >= nummapsides)
		{
			I_Warning("Bad WAD: level %s linedef #%d has bad LEFT side.\n",
				currmap->lump.c_str(), i);
			side1 = nummapsides-1;
		}

		ld->side[0] = sd;
		TransferMapSideDef(msd + side0, sd, (side1 != -1));
		ld->frontsector = sd->sector;
		sd++;

		if (side1 != -1)
		{
			ld->side[1] = sd;
			TransferMapSideDef(msd + side1, sd, true);
			ld->backsector = sd->sector;
			sd++;
		}

		SYS_ASSERT(sd <= sides + numsides);

	}

	SYS_ASSERT(sd == sides + numsides);

	W_DoneWithLump(data);

}

//
// SetupExtrafloors
// 
// This is done after loading sectors (which sets exfloor_max to 0)
// and after loading linedefs (which increases it for each new
// extrafloor).  So now we know the maximum number of extrafloors
// that can ever be needed.
//
// Note: this routine doesn't create any extrafloors (this is done
// later when their linetypes are activated).
//
static void SetupExtrafloors(void)
{
	int i, ef_index = 0;
	sector_t *ss;

	if (numextrafloors == 0)
		return;

	extrafloors = new extrafloor_t[numextrafloors];
		
	Z_Clear(extrafloors, extrafloor_t, numextrafloors);

	for (i=0, ss=sectors; i < numsectors; i++, ss++)
	{
		ss->exfloor_first = extrafloors + ef_index;

		ef_index += ss->exfloor_max;

		SYS_ASSERT(ef_index <= numextrafloors);
	}

	SYS_ASSERT(ef_index == numextrafloors);
}


static void SetupSlidingDoors(void)
{
	for (int i=0; i < numlines; i++)
	{
		line_t *ld = lines + i;

		if (! ld->special || ld->special->s.type == SLIDE_None)
			continue;

		if (ld->tag == 0 || ld->special->type == line_manual)
			ld->slide_door = ld->special;
		else
		{
			for (int k=0; k < numlines; k++)
			{
				line_t *other = lines + k;

				if (other->tag != ld->tag || other == ld)
					continue;

				other->slide_door = ld->special;
			}
		}
	}
}


//
// SetupWallTiles
//
// Computes how many wall tiles we'll need.  The tiles themselves are 
// created elsewhere.
//
#if 0  // NO LONGER USED
static void SetupWallTiles(void)
{
	int i, wt_index;
	int num_0, num_1;

	for (i=0; i < numlines; i++)
	{
		line_t *ld = lines + i;

		if (! ld->backsector)
		{
			num_0 = 1;
			num_1 = 0;
		}
		else if (ld->frontsector == ld->backsector)
		{
			num_0 = 1;
			num_1 = 1;
		}
		else if (ld->frontsector->tag == ld->backsector->tag)
		{
			SYS_ASSERT(ld->frontsector->exfloor_max ==
				ld->backsector->exfloor_max);

			num_0 = 3;  /* lower + middle + upper */
			num_1 = 3;
		}
		else
		{
			// FIXME: only count THICK sides for extrafloors.

			num_0 = 3 + ld->backsector->exfloor_max;
			num_1 = 3 + ld->frontsector->exfloor_max;
		}

		ld->side[0]->tile_max = num_0;

		if (ld->side[1])
			ld->side[1]->tile_max = num_1;

		numwalltiles += num_0 + num_1;
	}

	// I_Printf("%dK used for wall tiles.\n", (numwalltiles *
	//    sizeof(wall_tile_t) + 1023) / 1024);

	SYS_ASSERT(numwalltiles > 0);

	walltiles = new wall_tile_t[numwalltiles];
		
	Z_Clear(walltiles, wall_tile_t, numwalltiles);

	for (i=0, wt_index=0; i < numlines; i++)
	{
		line_t *ld = lines + i;

		ld->side[0]->tiles = walltiles + wt_index;
		wt_index += ld->side[0]->tile_max;

		if (ld->side[1])
		{
			ld->side[1]->tiles = walltiles + wt_index;
			wt_index += ld->side[1]->tile_max;
		}

		SYS_ASSERT(wt_index <= numwalltiles);
	}

	SYS_ASSERT(wt_index == numwalltiles);
}
#endif

//
// SetupVertGaps
//
// Computes how many vertical gaps we'll need.
//
static void SetupVertGaps(void)
{
	int i;
	int line_gaps = 0;
	int sect_sight_gaps = 0;

	vgap_t *cur_gap;

	for (i=0; i < numlines; i++)
	{
		line_t *ld = lines + i;

		ld->max_gaps = ld->backsector ? 1 : 0;

		// factor in extrafloors
		ld->max_gaps += ld->frontsector->exfloor_max;

		if (ld->backsector)
			ld->max_gaps += ld->backsector->exfloor_max;

		line_gaps += ld->max_gaps;
	}

	for (i=0; i < numsectors; i++)
	{
		sector_t *sec = sectors + i;

		sec->max_gaps = sec->exfloor_max + 1;

		sect_sight_gaps += sec->max_gaps;
	}

	numvertgaps = line_gaps + sect_sight_gaps;

	// I_Printf("%dK used for vert gaps.\n", (numvertgaps *
	//    sizeof(vgap_t) + 1023) / 1024);

	// zero is now impossible
	SYS_ASSERT(numvertgaps > 0);

	vertgaps = new vgap_t[numvertgaps];
	
	Z_Clear(vertgaps, vgap_t, numvertgaps);

	for (i=0, cur_gap=vertgaps; i < numlines; i++)
	{
		line_t *ld = lines + i;

		if (ld->max_gaps == 0)
			continue;

		ld->gaps = cur_gap;
		cur_gap += ld->max_gaps;
	}

	SYS_ASSERT(cur_gap == (vertgaps + line_gaps));

	for (i=0; i < numsectors; i++)
	{
		sector_t *sec = sectors + i;

		if (sec->max_gaps == 0)
			continue;

		sec->sight_gaps = cur_gap;
		cur_gap += sec->max_gaps;
	}

	SYS_ASSERT(cur_gap == (vertgaps + numvertgaps));
}

static void DetectDeepWaterTrick(void)
{
	byte *self_subs = new byte[numsubsectors];

	memset(self_subs, 0, numsubsectors);

	for (int i = 0; i < numsegs; i++)
	{
		const seg_t *seg = segs + i;

		if (seg->miniseg)
			continue;

		SYS_ASSERT(seg->front_sub);

		if (seg->linedef->backsector &&
		    seg->linedef->frontsector == seg->linedef->backsector)
		{
			self_subs[seg->front_sub - subsectors] |= 1;
		}
		else
		{
			self_subs[seg->front_sub - subsectors] |= 2;
		}
	}

	int count;
	int pass = 0;

	do
	{
		pass++;

		count = 0;

		for (int j = 0; j < numsubsectors; j++)
		{
			subsector_t *sub = subsectors + j;
			const seg_t *seg;

			if (self_subs[j] != 1)
				continue;
#if 0
			L_WriteDebug("Subsector [%d] @ (%1.0f,%1.0f) sec %d --> %d\n", j,
				(sub->bbox[BOXLEFT] + sub->bbox[BOXRIGHT]) / 2.0,
				(sub->bbox[BOXBOTTOM] + sub->bbox[BOXTOP]) / 2.0,
				sub->sector - sectors, self_subs[j]);
#endif
			const seg_t *Xseg = 0;

			for (seg = sub->segs; seg; seg = seg->sub_next)
			{
				SYS_ASSERT(seg->back_sub);

				int k = seg->back_sub - subsectors;
#if 0
				L_WriteDebug("  Seg [%d] back_sub %d (back_sect %d)\n", seg - segs, k,
					seg->back_sub->sector - sectors);
#endif
				if (self_subs[k] & 2)
				{
					if (! Xseg)
						Xseg = seg;
				}
			}

			if (Xseg)
			{
				sub->deep_ref = Xseg->back_sub->deep_ref ?
					Xseg->back_sub->deep_ref : Xseg->back_sub->sector;
#if 0
				L_WriteDebug("  Updating (from seg %d) --> SEC %d\n", Xseg - segs,
					sub->deep_ref - sectors);
#endif
				self_subs[j] = 3;

				count++;
			}
		}
	}
	while (count > 0 && pass < 100);

	delete[] self_subs;
}


static void DoBlockMap()
{
	int min_x = (int)vertexes[0].x;
	int min_y = (int)vertexes[0].y;

	int max_x = (int)vertexes[0].x;
	int max_y = (int)vertexes[0].y;

	for (int i=1; i < numvertexes; i++)
	{
		vec2_t *v = vertexes + i;

		min_x = MIN((int)v->x, min_x);
		min_y = MIN((int)v->y, min_y);
		max_x = MAX((int)v->x, max_x);
		max_y = MAX((int)v->y, max_y);
	}

	P_GenerateBlockMap(min_x, min_y, max_x, max_y);

	P_CreateThingBlockMap();
}


//
// GroupLines
//
// Builds sector line lists and subsector sector numbers.
// Finds block bounding boxes for sectors.
//
void GroupLines(void)
{
	int i;
	int j;
	int total;
	line_t *li;
	sector_t *sector;
	seg_t *seg;
	float bbox[4];
	line_t **line_p;

	// setup remaining seg information
	for (i=0, seg=segs; i < numsegs; i++, seg++)
	{
		if (seg->partner)
			seg->back_sub = seg->partner->front_sub;

		if (!seg->frontsector)
			seg->frontsector = seg->front_sub->sector;

		if (!seg->backsector && seg->back_sub)
			seg->backsector = seg->back_sub->sector;
	}

	// count number of lines in each sector
	li = lines;
	total = 0;
	for (i = 0; i < numlines; i++, li++)
	{
		total++;
		li->frontsector->linecount++;

		if (li->backsector && li->backsector != li->frontsector)
		{
			total++;
			li->backsector->linecount++;
		}
	}

	// build line tables for each sector 
	linebuffer = new line_t* [total];

	line_p = linebuffer;
	sector = sectors;

	for (i = 0; i < numsectors; i++, sector++)
	{
		M_ClearBox(bbox);
		sector->lines = line_p;
		li = lines;
		for (j = 0; j < numlines; j++, li++)
		{
			if (li->frontsector == sector || li->backsector == sector)
			{
				*line_p++ = li;
				M_AddToBox(bbox, li->v1->x, li->v1->y);
				M_AddToBox(bbox, li->v2->x, li->v2->y);
			}
		}
		if (line_p - sector->lines != sector->linecount)
			I_Error("GroupLines: miscounted");

		// Allow vertex slope if a triangular sector or a rectangular
		// sector in which two adjacent verts have an identical z-height
		// and the other two have it unset
		if (sector->linecount == 3 && zvertexes)
		{
			for (j=0; j < 3; j++)
			{
				vec2_t *vert = sector->lines[j]->v1;
				bool add_it = true;
				for (auto v : sector->floor_z_verts)
					if (v.x == vert->x && v.y == vert->y) add_it = false;
				if (add_it)
				{
					int vi = vert - vertexes;
					if (vi >= 0 && vi < numvertexes && zvertexes)
					{
						if (zvertexes[vi].x < 32767.0f && zvertexes[vi].x > -32768.0f)
						{
							sector->floor_vertex_slope = true;
							sector->floor_z_verts.push_back({vert->x,vert->y,zvertexes[vi].x});
							if (zvertexes[vi].x > sector->floor_vs_hilo.x)
								sector->floor_vs_hilo.x = zvertexes[vi].x;
							if (zvertexes[vi].x < sector->floor_vs_hilo.y)
								sector->floor_vs_hilo.y = zvertexes[vi].x;
						}
						else
							sector->floor_z_verts.push_back({vert->x,vert->y,sector->f_h});
						if (zvertexes[vi].y < 32767.0f && zvertexes[vi].y > -32768.0f)
						{
							sector->ceil_vertex_slope = true;
							sector->ceil_z_verts.push_back({vert->x,vert->y,zvertexes[vi].y});
						}
						else
							sector->ceil_z_verts.push_back({vert->x,vert->y,sector->c_h});
					}
				}
				vert = sector->lines[j]->v2;
				add_it = true;
				for (auto v : sector->floor_z_verts)
					if (v.x == vert->x && v.y == vert->y) add_it = false;
				if (add_it)
				{
					int vi = vert - vertexes;
					if (vi >= 0 && vi < numvertexes && zvertexes)
					{
						if (zvertexes[vi].x < 32767.0f && zvertexes[vi].x > -32768.0f)
						{
							sector->floor_vertex_slope = true;
							sector->floor_z_verts.push_back({vert->x,vert->y,zvertexes[vi].x});
							if (zvertexes[vi].x > sector->floor_vs_hilo.x)
								sector->floor_vs_hilo.x = zvertexes[vi].x;
							if (zvertexes[vi].x < sector->floor_vs_hilo.y)
								sector->floor_vs_hilo.y = zvertexes[vi].x;
						}
						else
							sector->floor_z_verts.push_back({vert->x,vert->y,sector->f_h});
						if (zvertexes[vi].y < 32767.0f && zvertexes[vi].y > -32768.0f)
						{
							sector->ceil_vertex_slope = true;
							sector->ceil_z_verts.push_back({vert->x,vert->y,zvertexes[vi].y});
						}
						else
							sector->ceil_z_verts.push_back({vert->x,vert->y,sector->c_h});
					}
				}
			}
			if (!sector->floor_vertex_slope)
				sector->floor_z_verts.clear();
			else
			{
				sector->floor_vs_normal = 
					M_CrossProduct(sector->floor_z_verts[0], sector->floor_z_verts[1], sector->floor_z_verts[2]);
				if (sector->f_h > sector->floor_vs_hilo.x)
					sector->floor_vs_hilo.x = sector->f_h;
				if (sector->f_h < sector->floor_vs_hilo.y)
					sector->floor_vs_hilo.y = sector->f_h;
			}
			if (!sector->ceil_vertex_slope)
				sector->ceil_z_verts.clear();
			else
				sector->ceil_vs_normal = 
					M_CrossProduct(sector->ceil_z_verts[0], sector->ceil_z_verts[1], sector->ceil_z_verts[2]);
		}
		if (sector->linecount == 4 && zvertexes)
		{
			int floor_z_lines = 0;
			int ceil_z_lines = 0;
			for (j=0; j < 4; j++)
			{
				vec2_t *vert = sector->lines[j]->v1;
				vec2_t *vert2 = sector->lines[j]->v2;
				bool add_it_v1 = true;
				bool add_it_v2 = true;
				for (auto v : sector->floor_z_verts)
					if (v.x == vert->x && v.y == vert->y) add_it_v1 = false;
				for (auto v : sector->floor_z_verts)
					if (v.x == vert2->x && v.y == vert2->y) add_it_v2 = false;
				int vi = vert - vertexes;
				int vi2 = vert2 - vertexes;
				if (add_it_v1)
				{
					if (zvertexes[vi].x < 32767.0f && zvertexes[vi].x > -32768.0f)
					{
						sector->floor_z_verts.push_back({vert->x,vert->y,zvertexes[vi].x});
						if (zvertexes[vi].x > sector->floor_vs_hilo.x)
							sector->floor_vs_hilo.x = zvertexes[vi].x;
						if (zvertexes[vi].x < sector->floor_vs_hilo.y)
							sector->floor_vs_hilo.y = zvertexes[vi].x;
					}
					else
						sector->floor_z_verts.push_back({vert->x,vert->y,sector->f_h});
					if (zvertexes[vi].y < 32767.0f && zvertexes[vi].y > -32768.0f)
					{
						sector->ceil_z_verts.push_back({vert->x,vert->y,zvertexes[vi].y});
					}
					else
						sector->ceil_z_verts.push_back({vert->x,vert->y,sector->c_h});
				}
				if (add_it_v2)
				{
					if (zvertexes[vi2].x < 32767.0f && zvertexes[vi2].x > -32768.0f)
					{
						sector->floor_z_verts.push_back({vert2->x,vert2->y,zvertexes[vi2].x});
						if (zvertexes[vi].x > sector->floor_vs_hilo.x)
							sector->floor_vs_hilo.x = zvertexes[vi].x;
						if (zvertexes[vi].x < sector->floor_vs_hilo.y)
							sector->floor_vs_hilo.y = zvertexes[vi].x;
					}
					else
						sector->floor_z_verts.push_back({vert2->x,vert2->y,sector->f_h});
					if (zvertexes[vi2].y < 32767.0f && zvertexes[vi2].y > -32768.0f)
					{
						sector->ceil_z_verts.push_back({vert2->x,vert2->y,zvertexes[vi2].y});
					}
					else
						sector->ceil_z_verts.push_back({vert2->x,vert2->y,sector->c_h});
				}
				if ((zvertexes[vi].x < 32767.0f && zvertexes[vi].x > -32768.0f) && 
					(zvertexes[vi2].x < 32767.0f && zvertexes[vi2].x > -32768.0f) &&
					zvertexes[vi].x == zvertexes[vi2].x)
				{
					floor_z_lines++;
				}
				if ((zvertexes[vi].y < 32767.0f && zvertexes[vi].y > -32768.0f) && 
					(zvertexes[vi2].y < 32767.0f && zvertexes[vi2].y > -32768.0f) &&
					zvertexes[vi].y == zvertexes[vi2].y)
				{
					ceil_z_lines++;
				}
			}
			if (floor_z_lines == 1 && sector->floor_z_verts.size() == 4)
			{
				sector->floor_vertex_slope = true;
				sector->floor_vs_normal = 
					M_CrossProduct(sector->floor_z_verts[0], sector->floor_z_verts[1], sector->floor_z_verts[2]);
				if (sector->f_h > sector->floor_vs_hilo.x)
					sector->floor_vs_hilo.x = sector->f_h;
				if (sector->f_h < sector->floor_vs_hilo.y)
					sector->floor_vs_hilo.y = sector->f_h;
			}
			else
				sector->floor_z_verts.clear();
			if (ceil_z_lines == 1 && sector->ceil_z_verts.size() == 4)
			{
				sector->ceil_vertex_slope = true;
				sector->ceil_vs_normal = 
					M_CrossProduct(sector->ceil_z_verts[0], sector->ceil_z_verts[1], sector->ceil_z_verts[2]);
			}
			else
				sector->ceil_z_verts.clear();
		}

		// set the degenmobj_t to the middle of the bounding box
		sector->sfx_origin.x = (bbox[BOXRIGHT] + bbox[BOXLEFT]) / 2;
		sector->sfx_origin.y = (bbox[BOXTOP] + bbox[BOXBOTTOM]) / 2;
		sector->sfx_origin.z = (sector->f_h + sector->c_h) / 2;
	}
}


static inline void AddSectorToVertices(int *branches, line_t *ld, sector_t *sec)
{
	if (! sec)
		return;

	unsigned short sec_idx = sec - sectors;

	for (int vert = 0; vert < 2; vert++)
	{
		int v_idx = (vert ? ld->v2 : ld->v1) - vertexes;

		SYS_ASSERT(0 <= v_idx && v_idx < numvertexes);

		if (branches[v_idx] < 0)
			continue;

		vertex_seclist_t *L = v_seclists + branches[v_idx];

		if (L->num >= SECLIST_MAX)
			continue;

		int pos;

		for (pos = 0; pos < L->num; pos++)
			if (L->sec[pos] == sec_idx)
				break;

		if (pos < L->num)
			continue;  // already in there

		L->sec[L->num++] = sec_idx;
	}
}


static void CreateVertexSeclists(void)
{
	// step 1: determine number of linedef branches at each vertex
	int *branches = new int[numvertexes];

	Z_Clear(branches, int, numvertexes);

	int i;

	for (i=0; i < numlines; i++)
	{
		int v1_idx = lines[i].v1 - vertexes;
		int v2_idx = lines[i].v2 - vertexes;

		SYS_ASSERT(0 <= v1_idx && v1_idx < numvertexes);
		SYS_ASSERT(0 <= v2_idx && v2_idx < numvertexes);

		branches[v1_idx] += 1;
		branches[v2_idx] += 1;
	}

	// step 2: count how many vertices have 3 or more branches,
	//         and simultaneously give them index numbers.
	int num_triples = 0;

	for (i=0; i < numvertexes; i++)
	{
		if (branches[i] < 3)
			branches[i] = -1;
		else
			branches[i] = num_triples++;
	}

	if (num_triples == 0)
	{
		delete[] branches;

		v_seclists = NULL;
		return;
	}

	// step 3: create a vertex_seclist for those multi-branches
	v_seclists = new vertex_seclist_t[num_triples];

	Z_Clear(v_seclists, vertex_seclist_t, num_triples);

	L_WriteDebug("Created %d seclists from %d vertices (%1.1f%%)\n",
			num_triples, numvertexes,
			num_triples * 100 / (float)numvertexes);

	// multiple passes for each linedef:
	//   pass #1 takes care of normal sectors
	//   pass #2 handles any extrafloors
	//
	// Rationale: normal sectors are more important, hence they
	//            should be allocated to the limited slots first.

	for (i=0; i < numlines; i++)
	{
		line_t *ld = lines + i;

		for (int side = 0; side < 2; side++)
		{
			sector_t *sec = side ? ld->backsector : ld->frontsector;

			AddSectorToVertices(branches, ld, sec);
		}
	}

	for (i=0; i < numlines; i++)
	{
		line_t *ld = lines + i;

		for (int side = 0; side < 2; side++)
		{
			sector_t *sec = side ? ld->backsector : ld->frontsector;

			if (! sec)
				continue;
			
			extrafloor_t *ef;

			for (ef = sec->bottom_ef; ef; ef = ef->higher)
				AddSectorToVertices(branches, ld, ef->ef_line->frontsector);

			for (ef = sec->bottom_liq; ef; ef = ef->higher)
				AddSectorToVertices(branches, ld, ef->ef_line->frontsector);
		}
	}

	// step 4: finally, update the segs that touch those vertices
	for (i=0; i < numsegs; i++)
	{
		seg_t *sg = segs + i;

		for (int vert=0; vert < 2; vert++)
		{
			int v_idx = (vert ? sg->v2 : sg->v1) - vertexes;

			// skip GL vertices
			if (v_idx < 0 || v_idx >= numvertexes)
				continue;

			if (branches[v_idx] < 0)
				continue;

			sg->nb_sec[vert] = v_seclists + branches[v_idx];
		}
	}

	delete[] branches;
}


static void P_RemoveSectorStuff(void)
{
	int i;

	for (i = 0; i < numsectors; i++)
	{
		P_FreeSectorTouchNodes(sectors + i);

		// Might still be playing a sound.
		S_StopFX(&sectors[i].sfx_origin);
	}
}


void ShutdownLevel(void)
{
	// Destroys everything on the level.

#ifdef DEVELOPERS
	if (!level_active)
		I_Error("ShutdownLevel: no level to shut down!");
#endif

	level_active = false;

	P_RemoveItemsInQue();

	P_RemoveSectorStuff();

	S_StopLevelFX();

	P_DestroyAllForces();
	P_DestroyAllLights();
	P_DestroyAllPlanes();
	P_DestroyAllSliders();
	P_DestroyAllAmbientSFX();

	DDF_BoomClearGenTypes();

	delete[] segs;         segs = NULL;
	delete[] nodes;        nodes = NULL;
	delete[] vertexes;     vertexes = NULL;
	if (zvertexes)
	{
		delete[] zvertexes;    
		zvertexes = NULL;
	}
	delete[] sides;        sides = NULL;
	delete[] lines;        lines = NULL;
	delete[] sectors;      sectors = NULL;
	delete[] subsectors;   subsectors = NULL;

	delete[] gl_vertexes;  gl_vertexes = NULL;
	delete[] extrafloors;  extrafloors = NULL;
	delete[] vertgaps;     vertgaps = NULL;
	delete[] linebuffer;   linebuffer = NULL;
	delete[] v_seclists;   v_seclists = NULL;

	P_DestroyBlockMap();
}


void P_SetupLevel(void)
{
	// Sets up the current level using the skill passed and the
	// information in currmap.
	//
	// -ACB- 1998/08/09 Use currmap to ref lump and par time

	if (level_active)
		ShutdownLevel();

	// -ACB- 1998/08/27 NULL the head pointers for the linked lists....
	itemquehead = NULL;
	mobjlisthead = NULL;
	seen_monsters.clear();

	// get lump for map header e.g. MAP01
	int lumpnum = W_CheckNumForName_MAP(currmap->lump.c_str());
	if (lumpnum < 0)
		I_Error("No such level: %s\n", currmap->lump.c_str());

	// get lump for XGL3 nodes from an XWA file
	int xgl_lump = W_CheckNumForName_XGL(currmap->lump.c_str());

	// ignore XGL nodes if it occurs _before_ the normal level marker.
	// [ something has gone horribly wrong if this happens! ]
	if (xgl_lump < lumpnum)
		xgl_lump = -1;

	// shouldn't happen (as during startup we checked for XWA files)
	if (xgl_lump < 0)
		I_Error("Internal error: missing XGL nodes.\n");

	// -CW- 2017/01/29: check for UDMF map lump
	if (W_VerifyLumpName(lumpnum + 1, "TEXTMAP"))
	{
		udmf_level = true;
		udmf_lumpnum = lumpnum + 1;
		udmf_lump = (char *)W_CacheLumpNum(udmf_lumpnum);
		if (!udmf_lump)
			I_Error("Internal error: can't load UDMF lump.\n");
		// initialize the parser
		udmf_psr.buffer = (uint8_t *)udmf_lump;
		udmf_psr.length = W_LumpLength(udmf_lumpnum);
		udmf_psr.next = 0; // start at first line
	}
	else
	{
		udmf_level = false;
		udmf_lumpnum = -1;
	}

	// clear CRC values
	mapsector_CRC.Reset();
	mapline_CRC.Reset();
	mapthing_CRC.Reset();

	// note: most of this ordering is important
	// 23-6-98 KM, eg, Sectors must be loaded before sidedefs,
	// Vertexes must be loaded before LineDefs,
	// LineDefs + Vertexes must be loaded before BlockMap,
	// Sectors must be loaded before Segs

	numsides = 0;
	numextrafloors = 0;
	numvertgaps = 0;

	if (!udmf_level)
	{
		// check if the level is for Hexen
		hexen_level = false;

		if (W_VerifyLump(lumpnum + ML_BEHAVIOR) &&
			W_VerifyLumpName(lumpnum + ML_BEHAVIOR, "BEHAVIOR"))
		{
			L_WriteDebug("Detected Hexen level.\n");
			hexen_level = true;
		}

		LoadVertexes(lumpnum + ML_VERTEXES);
		LoadSectors(lumpnum + ML_SECTORS);

		if (hexen_level)
			LoadHexenLineDefs(lumpnum + ML_LINEDEFS);
		else
			LoadLineDefs(lumpnum + ML_LINEDEFS);

		LoadSideDefs(lumpnum + ML_SIDEDEFS);
	}
	else
	{
		char ident[128];
		char value[128];

		if (!GetNextAssign(&udmf_psr, (uint8_t*)ident, (uint8_t*)value) || epi::case_cmp(ident, "namespace"))
			I_Error("UDMF: TEXTMAP must start with namespace assignment.\n");

		if (udmf_strict.d)
		{
			if(!(epi::case_cmp(value, "doom") == 0 || epi::case_cmp(value, "heretic") == 0) || epi::case_cmp(value, "edge") == 0
				|| epi::case_cmp(value, "zdoomtranslated") == 0)
			{
				I_Warning("UDMF: %s uses unsupported namespace \"%s\"!\nSupported namespaces are \"doom\", \"heretic\", \"edge\", or \"zdoomtranslated\"!\n",
					currmap->lump.c_str(), value);
			}
		}

		LoadUDMFVertexes(&udmf_psr);
		LoadUDMFSectors(&udmf_psr);
		LoadUDMFLineDefs(&udmf_psr);
		LoadUDMFSideDefs(&udmf_psr);
	}

	SetupExtrafloors();
	SetupSlidingDoors();
	SetupVertGaps();

	delete[] temp_line_sides;

	LoadXGL3Nodes(xgl_lump);

	// REJECT is ignored, and we generate our own BLOCKMAP

	DoBlockMap();

	GroupLines();

	DetectDeepWaterTrick();

	R_ComputeSkyHeights();

	// compute sector and line gaps
	for (int j=0; j < numsectors; j++)
		P_RecomputeGapsAroundSector(sectors + j);

	G_ClearBodyQueue();

	// set up world state
	// (must be before loading things to create Extrafloors)
	P_SpawnSpecials1();

	// -AJA- 1999/10/21: Clear out player starts (ready to load).
	G_ClearPlayerStarts();

	if (!udmf_level)
	{
		if (hexen_level)
			LoadHexenThings(lumpnum + ML_THINGS);
		else
			LoadThings(lumpnum + ML_THINGS);
	}
	else
	{
		LoadUDMFThings(&udmf_psr);

		W_DoneWithLump(udmf_lump);
	}

	// OK, CRC values have now been computed
#ifdef DEVELOPERS
	L_WriteDebug("MAP CRCS: S=%08x L=%08x T=%08x\n",
		mapsector_CRC.crc, mapline_CRC.crc, mapthing_CRC.crc);
#endif

	CreateVertexSeclists();

	P_SpawnSpecials2(currmap->autotag);

	AM_InitLevel();

	RGL_UpdateSkyBoxTextures();

	// preload graphics
	if (precache)
		W_PrecacheLevel();

	// setup categories based on game mode (SP/COOP/DM)
	S_ChangeChannelNum();

	// FIXME: cache sounds (esp. for player)

	S_ChangeMusic(currmap->music, true); // start level music

	level_active = true;
}


void P_Init(void)
{
	E_ProgressMessage(language["PlayState"]);
	
	// There should not yet exist a player
	SYS_ASSERT(numplayers == 0);

	G_ClearPlayerStarts();
}


linetype_c *P_LookupLineType(int num)
{
	if (num <= 0)
		return NULL;

	linetype_c* def = linetypes.Lookup(num);

	// DDF types always override
	if (def)
		return def;

	if (DDF_IsBoomLineType(num))
		return DDF_BoomGetGenLine(num);

	I_Warning("P_LookupLineType(): Unknown linedef type %d\n", num);

	return linetypes.Lookup(0);  // template line
}	


sectortype_c *P_LookupSectorType(int num)
{
	if (num <= 0)
		return NULL;

	sectortype_c* def = sectortypes.Lookup(num);

	// DDF types always override
	if (def)
		return def;

	if (DDF_IsBoomSectorType(num))
		return DDF_BoomGetGenSector(num);

	I_Warning("P_LookupSectorType(): Unknown sector type %d\n", num);

	return sectortypes.Lookup(0);	// template sector
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
