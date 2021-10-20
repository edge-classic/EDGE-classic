//----------------------------------------------------------------------------
//  EDGE Level Loading/Setup Code
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
#include "l_glbsp.h"
#include "m_argv.h"
#include "m_bbox.h"
#include "m_misc.h"
#include "m_random.h"
#include "p_local.h"
#include "p_bot.h"
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
#include "z_zone.h"

// debugging aide:
#define FORCE_LOCATION  0
#define FORCE_LOC_X     12766
#define FORCE_LOC_Y     4600
#define FORCE_LOC_ANG   0


#define SEG_INVALID  ((seg_t *) -3)
#define SUB_INVALID  ((subsector_t *) -3)


static bool level_active = false;


//
// MAP related Lookup tables.
// Store VERTEXES, LINEDEFS, SIDEDEFS, etc.
//
int numvertexes;
vec2_t *vertexes;

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
static bool v5_nodes;

// a place to store sidedef numbers of the loaded linedefs.
// There is two values for every line: side0 and side1.
static int *temp_line_sides;

cvar_c m_goobers;


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

static void LoadV2Vertexes(const byte *data, int length)
{
	int i;
	const raw_v2_vertex_t *ml2;
	vec2_t *vert;

	num_gl_vertexes = length / sizeof(raw_v2_vertex_t);

	gl_vertexes = new vec2_t[num_gl_vertexes];

	ml2 = (const raw_v2_vertex_t *) data;
	vert = gl_vertexes;

	// Copy and convert vertex coordinates,
	for (i = 0; i < num_gl_vertexes; i++, vert++, ml2++)
	{
		vert->x = (float)EPI_LE_S32(ml2->x) / 65536.0f;
		vert->y = (float)EPI_LE_S32(ml2->y) / 65536.0f;
	}
}


static void LoadGLVertexes(int lump)
{
	const byte *data;
	int i, length;
	const raw_vertex_t *ml;
	vec2_t *vert;

	if (!W_VerifyLumpName(lump, "GL_VERT"))
		I_Error("Bad WAD: level %s missing GL_VERT.\n", currmap->lump.c_str());

	// Load data into cache.
	data = (byte *) W_CacheLumpNum(lump);

	length = W_LumpLength(lump);

	// Handle v2.0 of "GL Node" specs (fixed point vertices)
	if (length >= 4 &&
		data[0] == 'g' && data[1] == 'N' &&
		data[2] == 'd' && (data[3] == '2' || data[3] == '5'))
	{
		if (data[3] == '5')
			v5_nodes = true;

		LoadV2Vertexes(data + 4, length - 4);
		W_DoneWithLump(data);
		return;
	}

	// check for non-compliant format
	if (length >= 4 &&
		data[0] == 'g' && data[1] == 'N' &&
		data[2] == 'd' && data[3] == '4')
	{
		I_Error("V4 Nodes not supported, please rebuild nodes with glBSP.\n");
	}

	// Determine number of vertices:
	//  total lump length / vertex record length.
	num_gl_vertexes = length / sizeof(raw_vertex_t);

	gl_vertexes = new vec2_t[num_gl_vertexes];

	ml = (const raw_vertex_t *) data;
	vert = gl_vertexes;

	// Copy and convert vertex coordinates,
	// Internal representation is float.
	for (i = 0; i < num_gl_vertexes; i++, vert++, ml++)
	{
		vert->x = EPI_LE_S16(ml->x);
		vert->y = EPI_LE_S16(ml->y);
	}

	// Free buffer memory.
	W_DoneWithLump(data);
}


static void SegCommonStuff(seg_t *seg, int linedef  )
{
	seg->frontsector = seg->backsector = NULL;

	if (linedef == 0xFFFF)
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

static void LoadV3Segs(const byte *data, int length)
{
	numsegs = length / sizeof(raw_v3_seg_t);

	if (numsegs == 0)
		I_Error("Bad WAD: level %s contains 0 gl-segs (v3).\n",
			currmap->lump.c_str());

	segs = new seg_t[numsegs];

	Z_Clear(segs, seg_t, numsegs);

	// check both V3 and V5 bits
	unsigned int VERTEX_V3_OR_V5 = SF_GL_VERTEX_V3 | SF_GL_VERTEX_V5;

	seg_t *seg = segs;
	const raw_v3_seg_t *ml = (const raw_v3_seg_t *) data;

	for (int i = 0; i < numsegs; i++, seg++, ml++)
	{
		unsigned int v1num = EPI_LE_U32(ml->start);
		unsigned int v2num = EPI_LE_U32(ml->end);

		// FIXME: check if indices are valid
		if (v1num & VERTEX_V3_OR_V5)
			seg->v1 = &gl_vertexes[v1num & ~VERTEX_V3_OR_V5];
		else
			seg->v1 = &vertexes[v1num];

		if (v2num & VERTEX_V3_OR_V5)
			seg->v2 = &gl_vertexes[v2num & ~VERTEX_V3_OR_V5];
		else
			seg->v2 = &vertexes[v2num];

		seg->angle  = R_PointToAngle(seg->v1->x, seg->v1->y,
			seg->v2->x, seg->v2->y);

		seg->length = R_PointToDist(seg->v1->x, seg->v1->y,
			seg->v2->x, seg->v2->y);

		seg->side = ml->side ? 1 : 0;

		int linedef = EPI_LE_U16(ml->linedef);

		SegCommonStuff(seg, linedef  );

		int partner = EPI_LE_S32(ml->partner);

		if (partner == -1)
			seg->partner = NULL;
		else
		{
			SYS_ASSERT(partner < numsegs);  // sanity check
			seg->partner = &segs[partner];
		}

		// The following fields are filled out elsewhere:
		//     sub_next, front_sub, back_sub, frontsector, backsector.

		seg->sub_next = SEG_INVALID;
		seg->front_sub = seg->back_sub = SUB_INVALID;
	}
}

static void LoadGLSegs(int lump)
{
	SYS_ASSERT(lump < 0x10000);  // sanity check

	if (! W_VerifyLumpName(lump, "GL_SEGS"))
		I_Error("Bad WAD: level %s missing GL_SEGS.\n", currmap->lump.c_str());

	const byte *data = (byte *) W_CacheLumpNum(lump);

	int length = W_LumpLength(lump);

	// Handle v3.0 of "GL Node" specs (new GL_SEGS format)
	if (length >= 4 &&
		data[0] == 'g' && data[1] == 'N' &&
		data[2] == 'd' && data[3] == '3')
	{
		LoadV3Segs(data + 4, length - 4);
		W_DoneWithLump(data);
		return;
	}

	if (v5_nodes)
	{
		LoadV3Segs(data, length);
		W_DoneWithLump(data);
		return;
	}

	numsegs = length / sizeof(raw_gl_seg_t);

	if (numsegs == 0)
		I_Error("Bad WAD: level %s contains 0 gl-segs.\n", currmap->lump.c_str());

	segs = new seg_t[numsegs];

	Z_Clear(segs, seg_t, numsegs);

	seg_t *seg = segs;
	const raw_gl_seg_t *ml = (const raw_gl_seg_t *) data;

	for (int i = 0; i < numsegs; i++, seg++, ml++)
	{
		int v1num = EPI_LE_U16(ml->start);
		int v2num = EPI_LE_U16(ml->end);

		// FIXME: check if indices are valid, abort loading

		if (v1num & SF_GL_VERTEX)
		{
			SYS_ASSERT((v1num & ~SF_GL_VERTEX) < num_gl_vertexes);  // sanity check
			seg->v1 = &gl_vertexes[v1num & ~SF_GL_VERTEX];
		}
		else
		{
			SYS_ASSERT(v1num < numvertexes);  // sanity check
			seg->v1 = &vertexes[v1num];
		}

		if (v2num & SF_GL_VERTEX)
		{
			SYS_ASSERT((v2num & ~SF_GL_VERTEX) < num_gl_vertexes);  // sanity check
			seg->v2 = &gl_vertexes[v2num & ~SF_GL_VERTEX];
		}
		else
		{
			SYS_ASSERT(v2num < numvertexes);  // sanity check
			seg->v2 = &vertexes[v2num];
		}

		seg->angle  = R_PointToAngle(seg->v1->x, seg->v1->y,
			seg->v2->x, seg->v2->y);

		seg->length = R_PointToDist(seg->v1->x, seg->v1->y,
			seg->v2->x, seg->v2->y);

		seg->side = EPI_LE_U16(ml->side);

		int linedef = EPI_LE_U16(ml->linedef);

		SegCommonStuff(seg, linedef  );

		int partner = EPI_LE_U16(ml->partner);

		if (partner == 0xFFFF)
			seg->partner = NULL;
		else
		{
			SYS_ASSERT(partner < numsegs);  // sanity check
			seg->partner = &segs[partner];
		}

		// The following fields are filled out elsewhere:
		//     sub_next, front_sub, back_sub, frontsector, backsector.

		seg->sub_next = SEG_INVALID;
		seg->front_sub = seg->back_sub = SUB_INVALID;
	}

	W_DoneWithLump(data);
}


static void LoadV3Subsectors(const byte *data, int length)
{
	int i, j;
	const raw_v3_subsec_t *ms;
	subsector_t *ss;

	numsubsectors = length / sizeof(raw_v3_subsec_t);

	if (numsubsectors == 0)
		I_Error("Bad WAD: level %s contains 0 ssectors (v3).\n",
			currmap->lump.c_str());

	subsectors = new subsector_t[numsubsectors];

	Z_Clear(subsectors, subsector_t, numsubsectors);

	ms = (const raw_v3_subsec_t *) data;
	ss = subsectors;

	for (i = 0; i < numsubsectors; i++, ss++, ms++)
	{
		int countsegs = EPI_LE_S32(ms->num);
		int firstseg  = EPI_LE_S32(ms->first);

		// -AJA- 1999/09/23: New linked list for the segs of a subsector
		//       (part of true bsp rendering).
		seg_t **prevptr = &ss->segs;

		if (countsegs == 0 || firstseg == -1 || firstseg+countsegs > numsegs)
			I_Error("Bad WAD: level %s has invalid SSECTORS (V3).\n",
				currmap->lump.c_str());

		ss->sector = NULL;
		ss->thinglist = NULL;

		// this is updated when the nodes are loaded
		ss->bbox = dummy_bbox;

		for (j = 0; j < countsegs; j++)
		{
			seg_t *cur = &segs[firstseg + j];

			*prevptr = cur;
			prevptr = &cur->sub_next;

			cur->front_sub = ss;
			cur->back_sub = NULL;

			if (!ss->sector && !cur->miniseg)
				ss->sector = cur->sidedef->sector;
		}

		if (ss->sector == NULL)
			I_Error("Bad WAD: level %s has crazy SSECTORS (V3).\n",
				currmap->lump.c_str());

		*prevptr = NULL;

		// link subsector into parent sector's list.
		// order is not important, so add it to the head of the list.

		ss->sec_next = ss->sector->subsectors;
		ss->sector->subsectors = ss;
	}
}

static void LoadSubsectors(int lump, const char *name)
{
	int i, j;
	int length;
	const byte *data;
	const raw_subsec_t *ms;
	subsector_t *ss;

	if (! W_VerifyLumpName(lump, name))
		I_Error("Bad WAD: level %s missing %s.\n", currmap->lump.c_str(), name);

	// Load data into cache.
	data = (byte *) W_CacheLumpNum(lump);

	length = W_LumpLength(lump);

	// Handle v3.0 of "GL Node" specs (new GL_SSECT format)
	if (W_LumpLength(lump) >= 4 &&
		data[0] == 'g' && data[1] == 'N' &&
		data[2] == 'd' && data[3] == '3')
	{
		LoadV3Subsectors(data + 4, length - 4);
		W_DoneWithLump(data);
		return;
	}

	if (v5_nodes)
	{
		LoadV3Subsectors(data, length);
		W_DoneWithLump(data);
		return;
	}

	numsubsectors = length / sizeof(raw_subsec_t);

	if (numsubsectors == 0)
		I_Error("Bad WAD: level %s contains 0 ssectors.\n", currmap->lump.c_str());

	subsectors = new subsector_t[numsubsectors];

	Z_Clear(subsectors, subsector_t, numsubsectors);

	ms = (const raw_subsec_t *) data;
	ss = subsectors;

	for (i = 0; i < numsubsectors; i++, ss++, ms++)
	{
		int countsegs = EPI_LE_U16(ms->num);
		int firstseg  = EPI_LE_U16(ms->first);

		// -AJA- 1999/09/23: New linked list for the segs of a subsector
		//       (part of true bsp rendering).
		seg_t **prevptr = &ss->segs;

		if (countsegs == 0 || firstseg == 0xFFFF || firstseg+countsegs > numsegs)
			I_Error("Bad WAD: level %s has invalid SSECTORS.\n", currmap->lump.c_str());

		ss->sector = NULL;
		ss->thinglist = NULL;

		// this is updated when the nodes are loaded
		ss->bbox = dummy_bbox;

		for (j = 0; j < countsegs; j++)
		{
			seg_t *cur = &segs[firstseg + j];

			*prevptr = cur;
			prevptr = &cur->sub_next;

			cur->front_sub = ss;
			cur->back_sub = NULL;

			if (!ss->sector && !cur->miniseg)
				ss->sector = cur->sidedef->sector;
		}

		if (ss->sector == NULL)
			I_Error("Bad WAD: level %s has crazy SSECTORS.\n",
				currmap->lump.c_str());

		*prevptr = NULL;

		// link subsector into parent sector's list.
		// order is not important, so add it to the head of the list.

		ss->sec_next = ss->sector->subsectors;
		ss->sector->subsectors = ss;
	}

	W_DoneWithLump(data);
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
		I_Error("Bad WAD: level %s missing SECTORS.\n", 
				currmap->lump.c_str());

	numsectors = W_LumpLength(lump) / sizeof(raw_sector_t);

	if (numsectors == 0)
		I_Error("Bad WAD: level %s contains 0 sectors.\n", 
				currmap->lump.c_str());

	sectors = new sector_t[numsectors];
		
	Z_Clear(sectors, sector_t, numsectors);

	data = W_CacheLumpNum(lump);
	mapsector_CRC.AddBlock((const byte*)data, W_LumpLength(lump));

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


static void LoadV5Nodes(const void *data, int length)
{
	int i, j;
	const raw_v5_node_t *mn;
	node_t *nd;

	numnodes = length / sizeof(raw_v5_node_t);

	nodes = new node_t[numnodes+1];
		
	Z_Clear(nodes, node_t, numnodes);

	mn = (const raw_v5_node_t *) data;
	nd = nodes;

	for (i = 0; i < numnodes; i++, nd++, mn++)
	{
		nd->div.x  = EPI_LE_S16(mn->x);
		nd->div.y  = EPI_LE_S16(mn->y);
		nd->div.dx = EPI_LE_S16(mn->dx);
		nd->div.dy = EPI_LE_S16(mn->dy);

		nd->div_len = R_PointToDist(0, 0, nd->div.dx, nd->div.dy);

		for (j = 0; j < 2; j++)
		{
			nd->children[j] = EPI_LE_U32(mn->children[j]);

			nd->bbox[j][BOXTOP]    = (float) EPI_LE_S16(mn->bbox[j].maxy);
			nd->bbox[j][BOXBOTTOM] = (float) EPI_LE_S16(mn->bbox[j].miny);
			nd->bbox[j][BOXLEFT]   = (float) EPI_LE_S16(mn->bbox[j].minx);
			nd->bbox[j][BOXRIGHT]  = (float) EPI_LE_S16(mn->bbox[j].maxx);

			// update bbox pointers in subsector
			if (nd->children[j] & NF_V5_SUBSECTOR)
			{
				subsector_t *ss = subsectors + (nd->children[j] & ~NF_V5_SUBSECTOR);
				ss->bbox = &nd->bbox[j][0];
			}
		}
	}

	SetupRootNode();
}


static void LoadNodes(int lump, const char *name)
{
	int i, j;
	const raw_node_t *mn;
	node_t *nd;

	if (! W_VerifyLumpName(lump, name))
		I_Error("Bad WAD: level %s missing %s.\n", 
				currmap->lump.c_str(), name);

	// Note: zero numnodes is valid.
	int length = W_LumpLength(lump);
	const void *data = W_CacheLumpNum(lump);

	if (v5_nodes)
	{
		LoadV5Nodes(data, length);
		W_DoneWithLump(data);
		return;
	}

	numnodes = length / sizeof(raw_node_t);

	nodes = new node_t[numnodes+1];
		
	Z_Clear(nodes, node_t, numnodes);

	mn = (const raw_node_t *) data;
	nd = nodes;

	for (i = 0; i < numnodes; i++, nd++, mn++)
	{
		nd->div.x  = EPI_LE_S16(mn->x);
		nd->div.y  = EPI_LE_S16(mn->y);
		nd->div.dx = EPI_LE_S16(mn->dx);
		nd->div.dy = EPI_LE_S16(mn->dy);

		nd->div_len = R_PointToDist(0, 0, nd->div.dx, nd->div.dy);

		for (j = 0; j < 2; j++)
		{
			nd->children[j] = EPI_LE_U16(mn->children[j]);

			nd->bbox[j][BOXTOP]    = (float) EPI_LE_S16(mn->bbox[j].maxy);
			nd->bbox[j][BOXBOTTOM] = (float) EPI_LE_S16(mn->bbox[j].miny);
			nd->bbox[j][BOXLEFT]   = (float) EPI_LE_S16(mn->bbox[j].minx);
			nd->bbox[j][BOXRIGHT]  = (float) EPI_LE_S16(mn->bbox[j].maxx);

			// change to correct bit, and update bbox pointers
			if (nd->children[j] & NF_SUBSECTOR)
			{
				nd->children[j] = NF_V5_SUBSECTOR | (nd->children[j] & ~NF_SUBSECTOR);

				subsector_t *ss = subsectors + (nd->children[j] & ~NF_V5_SUBSECTOR);
				ss->bbox = &nd->bbox[j][0];
			}
		}
	}

	W_DoneWithLump(data);

	SetupRootNode();
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
		mo->side = ~0;
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

	if (side0 == 0xFFFF) side0 = -1;
	if (side1 == 0xFFFF) side1 = -1;

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


static void TransferMapSideDef(const raw_sidedef_t *msd, side_t *sd,
							   bool two_sided)
{
	char buffer[10];

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

	Z_StrNCpy(buffer, msd->upper_tex, 8);
	sd->top.image = W_ImageLookup(buffer, INS_Texture, ILF_Null);

	if (m_goobers.d && ! sd->top.image)
	{
		Z_StrNCpy(buffer, msd->lower_tex, 8);
		sd->top.image = W_ImageLookup(buffer, INS_Texture);
	}

	// handle air colourmaps with BOOM's [242] linetype
	if (! sd->top.image)
	{
		sd->top.image = W_ImageLookup(buffer, INS_Texture);
		colourmap_c *cmap = colourmaps.Lookup(buffer);
		if (cmap) sd->sector->props.colourmap = cmap;
	}

	Z_StrNCpy(buffer, msd->lower_tex, 8);
	sd->bottom.image = W_ImageLookup(buffer, INS_Texture, ILF_Null);

	// handle water colourmaps with BOOM's [242] linetype
	if (! sd->bottom.image)
	{
		sd->bottom.image = W_ImageLookup(buffer, INS_Texture);
		colourmap_c *cmap = colourmaps.Lookup(buffer);
		if (cmap) sd->sector->props.colourmap = cmap;
	}

	Z_StrNCpy(buffer, msd->mid_tex, 8);
	sd->middle.image = W_ImageLookup(buffer, INS_Texture);

	if (sd->middle.image && two_sided)
	{
		sd->midmask_offset = sd->middle.offset.y;
		sd->middle.offset.y = 0;
	}

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


static void DoBlockMap(int lump)
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

	// int map_width  = max_x - min_x;
	// int map_height = max_y - min_y;

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
	P_FreeShootSpots();
	P_DestroyAllAmbientSFX();

	DDF_BoomClearGenTypes();

	delete[] segs;         segs = NULL;
	delete[] nodes;        nodes = NULL;
	delete[] vertexes;     vertexes = NULL;
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

	int j;
	int lumpnum;
	int gl_lumpnum;
	char gl_lumpname[16];

	if (level_active)
		ShutdownLevel();

	// -ACB- 1998/08/27 NULL the head pointers for the linked lists....
	itemquehead = NULL;
	mobjlisthead = NULL;

	lumpnum = W_GetNumForName(currmap->lump);

	// -AJA- 1999/12/20: Support for "GL-Friendly Nodes".
	sprintf(gl_lumpname, "GL_%s", currmap->lump.c_str());
	gl_lumpnum = W_CheckNumForName(gl_lumpname);

	// ignore GL info if the level marker occurs _before_ the normal
	// level marker.
	if (gl_lumpnum >= 0 && gl_lumpnum < lumpnum)
		gl_lumpnum = -1;

	if (gl_lumpnum < 0)  // shouldn't happen
		I_Error("Internal error: missing GL-Nodes.\n");

	// clear CRC values
	mapsector_CRC.Reset();
	mapline_CRC.Reset();
	mapthing_CRC.Reset();

#ifdef DEVELOPERS

#define SHOWLUMPNAME(outstr, ln) \
	L_WriteDebug(outstr, W_GetLumpName(ln));

	SHOWLUMPNAME("MAP            : %s\n", lumpnum);
	SHOWLUMPNAME("MAP VERTEXES   : %s\n", lumpnum + ML_VERTEXES);
	SHOWLUMPNAME("MAP SECTORS    : %s\n", lumpnum + ML_SECTORS);
	SHOWLUMPNAME("MAP SIDEDEFS   : %s\n", lumpnum + ML_SIDEDEFS);
	SHOWLUMPNAME("MAP LINEDEFS   : %s\n", lumpnum + ML_LINEDEFS);
	SHOWLUMPNAME("MAP BLOCKMAP   : %s\n", lumpnum + ML_BLOCKMAP);

	if (gl_lumpnum >= 0)
	{
		SHOWLUMPNAME("MAP GL         : %s\n", gl_lumpnum);
		SHOWLUMPNAME("MAP GL VERTEXES: %s\n", gl_lumpnum + ML_GL_VERT);
		SHOWLUMPNAME("MAP GL SEGS    : %s\n", gl_lumpnum + ML_GL_SEGS);
		SHOWLUMPNAME("MAP GL SSECTORS: %s\n", gl_lumpnum + ML_GL_SSECT);
		SHOWLUMPNAME("MAP GL NODES   : %s\n", gl_lumpnum + ML_GL_NODES);
	}
	else
	{
		SHOWLUMPNAME("MAP SEGS       : %s\n", lumpnum + ML_SEGS);
		SHOWLUMPNAME("MAP SSECTORS   : %s\n", lumpnum + ML_SSECTORS);
		SHOWLUMPNAME("MAP NODES      : %s\n", lumpnum + ML_NODES);
	}

	SHOWLUMPNAME("MAP REJECT     : %s\n", lumpnum + ML_REJECT);

#undef SHOWLUMPNAME
#endif

	v5_nodes = false;

	// check if the level is for Hexen
	hexen_level = false;

	if (lumpnum + ML_BEHAVIOR < numlumps &&
		W_VerifyLumpName(lumpnum + ML_BEHAVIOR, "BEHAVIOR"))
	{
		L_WriteDebug("Detected Hexen level.\n");
		hexen_level = true;
	}

	// note: most of this ordering is important
	// 23-6-98 KM, eg, Sectors must be loaded before sidedefs,
	// Vertexes must be loaded before LineDefs,
	// LineDefs + Vertexes must be loaded before BlockMap,
	// Sectors must be loaded before Segs

	numsides = 0;
	numextrafloors = 0;
	numvertgaps = 0;

	LoadVertexes(lumpnum + ML_VERTEXES);
	LoadSectors(lumpnum + ML_SECTORS);
	
	if (hexen_level)
		LoadHexenLineDefs(lumpnum + ML_LINEDEFS);
	else
		LoadLineDefs(lumpnum + ML_LINEDEFS);

	LoadSideDefs(lumpnum + ML_SIDEDEFS);

	SetupExtrafloors();
	SetupSlidingDoors();
	SetupVertGaps();

	delete[] temp_line_sides;

	SYS_ASSERT(gl_lumpnum >= 0);

	LoadGLVertexes(gl_lumpnum + ML_GL_VERT);
	LoadGLSegs(gl_lumpnum + ML_GL_SEGS);
	LoadSubsectors(gl_lumpnum + ML_GL_SSECT, "GL_SSECT");
	LoadNodes(gl_lumpnum + ML_GL_NODES, "GL_NODES");

	// REJECT is ignored

	DoBlockMap(lumpnum + ML_BLOCKMAP);

	GroupLines();

	DetectDeepWaterTrick();

	R_ComputeSkyHeights();

	// compute sector and line gaps
	for (j=0; j < numsectors; j++)
		P_RecomputeGapsAroundSector(sectors + j);

	G_ClearBodyQueue();

	// set up world state
	// (must be before loading things to create Extrafloors)
	P_SpawnSpecials1();

	// -AJA- 1999/10/21: Clear out player starts (ready to load).
	G_ClearPlayerStarts();

	if (hexen_level)
		LoadHexenThings(lumpnum + ML_THINGS);
	else
		LoadThings(lumpnum + ML_THINGS);

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

	if (level_flags.edge_compat && (num > 0) && (num < 100))
	{
		sectortype_c* def = sectortypes.Lookup(4400 + num);
		if (def)
			return def;
	}

	if (DDF_IsBoomSectorType(num))
		return DDF_BoomGetGenSector(num);

	I_Warning("P_LookupSectorType(): Unknown sector type %d\n", num);

	return sectortypes.Lookup(0);	// template sector
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
