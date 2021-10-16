//----------------------------------------------------------------------------
//  EDGE Specials Lines & Floor Code
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
// -KM- 1998/09/01 Lines.ddf
//
//

#include "i_defs.h"

#include <limits.h>

#include "con_main.h"
#include "dm_data.h"
#include "dm_defs.h"
#include "dm_state.h"
#include "g_game.h"
#include "f_interm.h"
#include "m_argv.h"
#include "m_random.h"
#include "p_local.h"
#include "p_spec.h"
#include "rad_trig.h"
#include "s_sound.h"
#include "s_music.h"
#include "z_zone.h"

// Level exit timer
bool levelTimer;
int levelTimeCount;

//
// Animating line and sector specials
//
std::list<line_t *>   active_line_anims;
std::list<sector_t *> active_sector_anims;


static bool P_DoSectorsFromTag(int tag, const void *p1, void *p2,
		bool(*func) (sector_t *, const void *, void *));


static bool DoPlane_wrapper(sector_t *s, const void *p1, void *p2)
{
	return EV_DoPlane(s, (const movplanedef_c*)p1, (sector_t*)p2);
}


static bool DoLights_wrapper(sector_t *s, const void *p1, void *p2)
{
	return EV_Lights(s, (const lightdef_c*)p1);
}


static bool DoDonut_wrapper(sector_t *s, const void *p1, void *p2)
{
	return EV_DoDonut(s, (sfx_t**)p2);
}



//
// UTILITIES
//

//
// Will return a side_t * given the number of the current sector,
// the line number, and the side (0/1) that you want.
//
side_t *P_GetSide(int currentSector, int line, int side)
{
	line_t *linedef = sectors[currentSector].lines[line];

	return linedef->side[side];
}

//
// Will return a sector_t*
//  given the number of the current sector,
//  the line number and the side (0/1) that you want.
//
sector_t *P_GetSector(int currentSector, int line, int side)
{
	line_t *linedef = sectors[currentSector].lines[line];

	return side ? linedef->backsector : linedef->frontsector;
}

//
// Given the sector number and the line number, it will tell you whether the
// line is two-sided or not.
//
int P_TwoSided(int sector, int line)
{
	return (sectors[sector].lines[line])->flags & MLF_TwoSided;
}

//
// Return sector_t * of sector next to current; NULL if not two-sided line
//
sector_t *P_GetNextSector(const line_t * line, const sector_t * sec, bool ignore_selfref)
{
	if (!(line->flags & MLF_TwoSided))
		return NULL;

	// -AJA- 2011/03/31: follow BOOM's logic for self-ref linedefs, which
	//                   fixes the red door of MAP01 of 1024CLAU.wad
	if (ignore_selfref && (line->frontsector == line->backsector))
		return NULL;

	if (line->frontsector == sec)
		return line->backsector;


	return line->frontsector;
}

//
// -AJA- 2001/05/29: this is an amalgamation of the previous bunch of
//       routines, using the new REF_* flag names.  Now there's just
//       this one big routine -- the compiler's optimiser had better
//       kick in !
//
#define F_C_HEIGHT(sector)  \
((ref & REF_CEILING) ? (sector)->c_h : (sector)->f_h)

float P_FindSurroundingHeight(const heightref_e ref, const sector_t *sec)
{
	int i, count;
	float height;
	float base = F_C_HEIGHT(sec);

	if (ref & REF_INCLUDE)
		height = base;
	else if (ref & REF_HIGHEST)
		height = -32000.0f;  // BOOM compatible value
	else
		height = +32000.0f;

	for (i = count = 0; i < sec->linecount; i++)
	{
		sector_t *other = P_GetNextSector(sec->lines[i], sec, true);

		if (!other)
			continue;

		float other_h = F_C_HEIGHT(other);

		if (ref & REF_NEXT)
		{
			bool satisfy;

			// Note that REF_HIGHEST is used for the NextLowest types, and
			// vice versa, which may seem strange.  It's because the next
			// lowest sector is actually the highest of all adjacent sectors
			// that are lower than the current sector.

			if (ref & REF_HIGHEST)
				satisfy = (other_h < base);  // next lowest
			else
				satisfy = (other_h > base);  // next highest

			if (! satisfy)
				continue;
		}

		count++;

		if (ref & REF_HIGHEST)
			height = MAX(height, other_h);
		else
			height = MIN(height, other_h);
	}

	if ((ref & REF_NEXT) && count == 0)
		return base;

	return height;
}

//
// FIND THE SHORTEST BOTTOM TEXTURE SURROUNDING sec
// AND RETURN IT'S TOP HEIGHT
//
// -KM- 1998/09/01 Lines.ddf; used to be inlined in p_floors
//
float P_FindRaiseToTexture(sector_t * sec)
{
	int i;
	side_t *side;
	float minsize = INT_MAX;
	int secnum = sec - sectors;

	for (i = 0; i < sec->linecount; i++)
	{
		if (P_TwoSided(secnum, i))
		{
			side = P_GetSide(secnum, i, 0);

			if (side->bottom.image)
			{
				if (IM_HEIGHT(side->bottom.image) < minsize)
					minsize = IM_HEIGHT(side->bottom.image);
			}

			side = P_GetSide(secnum, i, 1);

			if (side->bottom.image)
			{
				if (IM_HEIGHT(side->bottom.image) < minsize)
					minsize = IM_HEIGHT(side->bottom.image);
			}
		}
	}

	return sec->f_h + minsize;
}

//
// Returns the FIRST sector that tag refers to.
//
// -KM- 1998/09/27 Doesn't need a line.
// -AJA- 1999/09/29: Now returns a sector_t, and has no start.
//
sector_t *P_FindSectorFromTag(int tag)
{
	int i;

	for (i = 0; i < numsectors; i++)
	{
		if (sectors[i].tag == tag)
			return sectors + i;
	}

	return NULL;
}

//
// Find minimum light from an adjacent sector
//
int P_FindMinSurroundingLight(sector_t * sector, int max)
{
	int i;
	int min;
	line_t *line;
	sector_t *check;

	min = max;
	for (i = 0; i < sector->linecount; i++)
	{
		line = sector->lines[i];
		check = P_GetNextSector(line, sector);

		if (!check)
			continue;

		if (check->props.lightlevel < min)
			min = check->props.lightlevel;
	}
	return min;
}


void P_AddSpecialLine(line_t *ld)
{
	// check if already linked
	std::list<line_t *>::iterator LI;

	for (LI  = active_line_anims.begin();
		 LI != active_line_anims.end();
		 LI++)
	{
		if (*LI == ld)
			return;
	}

	active_line_anims.push_back(ld);
}

void P_AddSpecialSector(sector_t *sec)
{
	// check if already linked
	std::list<sector_t *>::iterator SI;

	for (SI  = active_sector_anims.begin();
		 SI != active_sector_anims.end();
		 SI++)
	{
		if (*SI == sec)
			return;
	}

	active_sector_anims.push_back(sec);
}


static void AdjustScrollParts(side_t *side, bool left,
		scroll_part_e parts, float x_speed, float y_speed)
{
	float xmul = (left && (parts & SCPT_LeftRevX)) ? -1 : 1;
	float ymul = (left && (parts & SCPT_LeftRevY)) ? -1 : 1;

	if (! side)
		return;

	// -AJA- this is an inconsistency, needed for compatibility with
	//       original DOOM and Boom.  (Should be SCPT_RIGHT | SCPT_LEFT).
	if (parts == SCPT_None)
		parts = SCPT_RIGHT;

	if (parts & (left ? SCPT_LeftUpper : SCPT_RightUpper))
	{
		side->top.scroll.x += x_speed * xmul;
		side->top.scroll.y += y_speed * ymul;
	}
	if (parts & (left ? SCPT_LeftMiddle : SCPT_RightMiddle))
	{
		side->middle.scroll.x += x_speed * xmul;
		side->middle.scroll.y += y_speed * ymul;
	}
	if (parts & (left ? SCPT_LeftLower : SCPT_RightLower))
	{
		side->bottom.scroll.x += x_speed * xmul;
		side->bottom.scroll.y += y_speed * ymul;
	}
}

static void AdjustScaleParts(side_t *side, bool left,
		scroll_part_e parts, float factor)
{
	if (! side)
		return;

	if (parts == SCPT_None)
		parts = (scroll_part_e)(SCPT_LEFT | SCPT_RIGHT);

	if (parts & (left ? SCPT_LeftUpper : SCPT_RightUpper))
	{
		side->top.x_mat.x *= factor;
		side->top.y_mat.y *= factor;
	}
	if (parts & (left ? SCPT_LeftMiddle : SCPT_RightMiddle))
	{
		side->middle.x_mat.x *= factor;
		side->middle.y_mat.y *= factor;
	}
	if (parts & (left ? SCPT_LeftLower : SCPT_RightLower))
	{
		side->bottom.x_mat.x *= factor;
		side->bottom.y_mat.y *= factor;
	}
}

static void AdjustSkewParts(side_t *side, bool left,
		scroll_part_e parts, float skew)
{
	if (! side)
		return;

	if (parts == SCPT_None)
		parts = (scroll_part_e)(SCPT_LEFT | SCPT_RIGHT);

	if (parts & (left ? SCPT_LeftUpper : SCPT_RightUpper))
		side->top.y_mat.x = skew * side->top.y_mat.y;

	if (parts & (left ? SCPT_LeftMiddle : SCPT_RightMiddle))
		side->middle.y_mat.x = skew * side->middle.y_mat.y;

	if (parts & (left ? SCPT_LeftLower : SCPT_RightLower))
		side->bottom.y_mat.x = skew * side->bottom.y_mat.y;
}

static void AdjustLightParts(side_t *side, bool left,
		scroll_part_e parts, region_properties_t *p)
{
	if (! side)
		return;

	if (parts == SCPT_None)
		parts = (scroll_part_e)(SCPT_LEFT | SCPT_RIGHT);

	if (parts & (left ? SCPT_LeftUpper : SCPT_RightUpper))
		side->top.override_p = p;

	if (parts & (left ? SCPT_LeftMiddle : SCPT_RightMiddle))
		side->middle.override_p = p;

	if (parts & (left ? SCPT_LeftLower : SCPT_RightLower))
		side->bottom.override_p = p;
}


static void P_EFTransferTrans(sector_t *ctrl, sector_t *sec, line_t *line, 
		const extrafloordef_c *ef, float trans)
{
	int i;

	// floor and ceiling

	if (ctrl->floor.translucency > trans)
		ctrl->floor.translucency = trans;

	if (ctrl->ceil.translucency > trans)
		ctrl->ceil.translucency = trans;

	// sides

	if (! (ef->type & EXFL_Thick))
		return;

	if (ef->type & (EXFL_SideUpper | EXFL_SideLower))
	{
		for (i=0; i < sec->linecount; i++)
		{
			line_t *L = sec->lines[i];
			side_t *S = NULL;

			if (L->frontsector == sec)
				S = L->side[1];
			else if (L->backsector == sec)
				S = L->side[0];

			if (! S)
				continue;

			if (ef->type & EXFL_SideUpper)
				S->top.translucency = trans;
			else  // EXFL_SideLower
				S->bottom.translucency = trans;
		}

		return;
	}

	line->side[0]->middle.translucency = trans;
}

//
// Handles BOOM's line -> tagged line transfers.
//
static void P_LineEffect(line_t *target, line_t *source,
		const linetype_c *special)
{
	float length = R_PointToDist(0, 0, source->dx, source->dy);
   	float factor = 64.0 / length;

	if ((special->line_effect & LINEFX_Translucency) && (target->flags & MLF_TwoSided))
	{
		target->side[0]->middle.translucency = 0.5f;
		target->side[1]->middle.translucency = 0.5f;
	}


	if (special->line_effect & LINEFX_VectorScroll)
	{
		// -AJA- Note: these values are the same as in BOOM, which doesn't
		//       exactly match the description given in boomref.txt, which
		//       suggests that the horizontal speed is proportional to the
		//       tagging line's length.

		float xspeed = source->dx / 32.0f;
		float yspeed = source->dy / 32.0f;

		AdjustScrollParts(target->side[0], 0, special->line_parts,
				xspeed, yspeed);

		AdjustScrollParts(target->side[1], 1, special->line_parts,
				xspeed, yspeed);

		P_AddSpecialLine(target);
	}

	if ((special->line_effect & LINEFX_OffsetScroll) && target->side[0])
	{
		float xspeed = -target->side[0]->middle.offset.x;
		float yspeed =  target->side[0]->middle.offset.y;

		AdjustScrollParts(target->side[0], 0, special->line_parts,
				xspeed, yspeed);

		P_AddSpecialLine(target);
	}

	// experimental: unblock line(s)
	if (special->line_effect & LINEFX_UnblockThings)
	{
		if (target->side[0] && target->side[1] && (target != source))
			target->flags &= ~(MLF_Blocking | MLF_BlockMonsters);
	}

	// experimental: block bullets/missiles
	if (special->line_effect & LINEFX_BlockShots)
	{
		if (target->side[0] && target->side[1])
			target->flags |= MLF_ShootBlock;
	}

	// experimental: block monster sight
	if (special->line_effect & LINEFX_BlockSight)
	{
		if (target->side[0] && target->side[1])
			target->flags |= MLF_SightBlock;
	}

	// experimental: scale wall texture(s) by line length
	if (special->line_effect & LINEFX_Scale)
	{
		AdjustScaleParts(target->side[0], 0, special->line_parts, factor);
		AdjustScaleParts(target->side[1], 1, special->line_parts, factor);
	}

	// experimental: skew wall texture(s) by sidedef Y offset
	if ((special->line_effect & LINEFX_Skew) && source->side[0])
	{
		float skew = source->side[0]->top.offset.x / 128.0f;

		AdjustSkewParts(target->side[0], 0, special->line_parts, skew);
		AdjustSkewParts(target->side[1], 1, special->line_parts, skew);

		if (target == source)
		{
			source->side[0]->middle.offset.x = 0;
			source->side[0]->bottom.offset.x = 0;
		}
	}

	// experimental: transfer lighting to wall parts
	if (special->line_effect & LINEFX_LightWall)
	{
		AdjustLightParts(target->side[0], 0, special->line_parts,
				&source->frontsector->props);
		AdjustLightParts(target->side[1], 1, special->line_parts,
				&source->frontsector->props);
	}
}

//
// Handles BOOM's line -> tagged sector transfers.
//
static void P_SectorEffect(sector_t *target, line_t *source,
		const linetype_c *special)
{
	if (! target)
		return;

	float length  = R_PointToDist( 0, 0,  source->dx,  source->dy);
	angle_t angle = R_PointToAngle(0, 0, -source->dx, -source->dy);

	float factor  = 64.0 / length;

	if (special->sector_effect & SECTFX_LightFloor)
		target->floor.override_p = &source->frontsector->props;

	if (special->sector_effect & SECTFX_LightCeiling)
		target->ceil.override_p = &source->frontsector->props;

	if (special->sector_effect & SECTFX_ScrollFloor)
	{
		target->floor.scroll.x -= source->dx / 32.0f;
		target->floor.scroll.y -= source->dy / 32.0f;

		P_AddSpecialSector(target);
	}
	if (special->sector_effect & SECTFX_ScrollCeiling)
	{
		target->ceil.scroll.x -= source->dx / 32.0f;
		target->ceil.scroll.y -= source->dy / 32.0f;

		P_AddSpecialSector(target);
	}

	if (special->sector_effect & SECTFX_PushThings)
	{
		target->props.push.x += source->dx / 320.0f;
		target->props.push.y += source->dy / 320.0f;
	}

	if (special->sector_effect & SECTFX_SetFriction)
	{
		// TODO: this is not 100% correct, because the MSF_Friction flag is
		//       supposed to turn the custom friction on/off, but with this
		//       code, the custom value is either permanent or forgotten.
		if (target->props.type & MSF_Friction)
		{
			if (length > 100)
				target->props.friction  = MIN(1.0f, 0.8125f + length / 1066.7f);
			else
				target->props.viscosity = MAX(0.2f, length / 100.0f);
		}
	}

	if (special->sector_effect & SECTFX_PointForce)
	{
		P_AddPointForce(target, length);
	}
	if (special->sector_effect & SECTFX_WindForce)
	{
		P_AddSectorForce(target, true /* is_wind */, source->dx, source->dy);
	}
	if (special->sector_effect & SECTFX_CurrentForce)
	{
		P_AddSectorForce(target, false /* is_wind */, source->dx, source->dy);
	}

	if (special->sector_effect & SECTFX_ResetFloor)
	{
		target->floor.override_p = NULL;
		target->floor.scroll.x = target->floor.scroll.y = 0;
		target->props.push.x = target->props.push.y = target->props.push.z = 0;
	}
	if (special->sector_effect & SECTFX_ResetCeiling)
	{
		target->ceil.override_p = NULL;
		target->ceil.scroll.x = target->ceil.scroll.y = 0;
	}

	// set texture alignment
	if (special->sector_effect & SECTFX_AlignFloor)
	{
		target->floor.offset.x = -source->v1->x;
		target->floor.offset.y = -source->v1->y;

		M_Angle2Matrix(angle, &target->floor.x_mat, &target->floor.y_mat);
	}
	if (special->sector_effect & SECTFX_AlignCeiling)
	{
		target->ceil.offset.x = -source->v1->x;
		target->ceil.offset.y = -source->v1->y;

		M_Angle2Matrix(angle, &target->ceil.x_mat, &target->ceil.y_mat);
	}

	// set texture scale
	if (special->sector_effect & SECTFX_ScaleFloor)
	{
		target->floor.x_mat.x *= factor;
	    target->floor.x_mat.y *= factor;
		target->floor.y_mat.x *= factor;
	    target->floor.y_mat.y *= factor;

		target->floor.offset.x *= factor;
		target->floor.offset.y *= factor;
	}
	if (special->sector_effect & SECTFX_ScaleCeiling)
	{
		target->ceil.x_mat.x *= factor;
		target->ceil.x_mat.y *= factor;
		target->ceil.y_mat.x *= factor;
		target->ceil.y_mat.y *= factor;

		target->ceil.offset.x *= factor;
		target->ceil.offset.y *= factor;
	}
}

static void P_PortalEffect(line_t *ld)
{
	// already linked?
	if (ld->portal_pair)
		return;

	if (ld->side[1])
	{
		I_Warning("Portal on line #%d disabled: Not one-sided!\n", ld - lines);
		return;
	}

	if (ld->special->portal_effect & PORTFX_Mirror)
	{
		ld->flags |= MLF_Mirror;
		return;
	}

	if (ld->tag <= 0)
	{
		I_Warning("Portal on line #%d disabled: Missing tag.\n", ld - lines);
		return;
	}

	bool is_camera = (ld->special->portal_effect & PORTFX_Camera) ? true : false;

	for (int i=0; i < numlines; i++)
	{
		line_t *other = lines + i;

		if (other == ld)
			continue;

		if (other->tag != ld->tag)
			continue;

		float h1 = ld->frontsector->c_h - ld->frontsector->f_h;
		float h2 = other->frontsector->c_h - other->frontsector->f_h;

		if (h1 < 1 || h2 < 1)
		{
			I_Warning("Portal on line #%d disabled: sector is closed.\n", ld - lines);
			return;
		}

		if (is_camera)
		{
			// camera are much less restrictive than pass-able portals
			// (they are also one-way).

			ld->portal_pair = other;
			return;
		}

		if (other->portal_pair)
		{
			I_Warning("Portal on line #%d disabled: Partner already a portal.\n", ld - lines);
			return;
		}

		if (other->side[1])
		{
			I_Warning("Portal on line #%d disabled: Partner not one-sided.\n", ld - lines);
			return;
		}

		float h_ratio = h1 / h2;

		if (h_ratio < 0.95f || h_ratio > 1.05f)
		{
			I_Warning("Portal on line #%d disabled: Partner is different height.\n", ld - lines);
			return;
		}

		float len_ratio = ld->length / other->length;

		if (len_ratio < 0.95f || len_ratio > 1.05f)
		{
			I_Warning("Portal on line #%d disabled: Partner is different length.\n", ld - lines);
			return;
		}

		ld->portal_pair = other;
		other->portal_pair = ld;

		// let renderer (etc) know the portal information
		other->special = ld->special;

		return; // Success !!
	}

	I_Warning("Portal on line #%d disabled: Cannot find partner!\n", ld - lines);
}


static slope_plane_t * DetailSlope_BoundIt(line_t *ld, sector_t *sec, float dz1, float dz2)
{
	// determine slope's 2D coordinates
	float d_close = 0;
	float d_far   = 0;

	float nx =  ld->dy / ld->length;
	float ny = -ld->dx / ld->length;

	if (sec == ld->backsector)
	{
		nx = -nx;
		ny = -ny;
	}

	for (int k = 0; k < sec->linecount; k++)
	{
		for (int vert = 0; vert < 2; vert++)
		{
			vec2_t *V = (vert == 0) ? sec->lines[k]->v1 : sec->lines[k]->v2;

			float dist = nx * (V->x - ld->v1->x) +
			             ny * (V->y - ld->v1->y);

			d_close = MIN(d_close, dist);
			d_far   = MAX(d_far,   dist);
		}
	}

L_WriteDebug("DETAIL SLOPE in #%d: dists %1.3f -> %1.3f\n", sec - sectors, d_close, d_far);

	if (d_far - d_close < 0.5)
	{
		I_Warning("Detail slope in sector #%d disabled: no area?!?\n", sec - sectors);
		return NULL;
	}

	slope_plane_t *result = new slope_plane_t;

	result->x1  = ld->v1->x + nx * d_close;
	result->y1  = ld->v1->y + ny * d_close;
	result->dz1 = dz1;

	result->x2  = ld->v1->x + nx * d_far;
	result->y2  = ld->v1->y + ny * d_far;
	result->dz2 = dz2;

	return result;
}

static void DetailSlope_Floor(line_t *ld)
{
	if (! ld->side[1])
	{
		I_Warning("Detail slope on line #%d disabled: Not two-sided!\n", ld - lines);
		return;
	}

	sector_t *sec = ld->frontsector;

	float z1 = ld->backsector ->f_h;
	float z2 = ld->frontsector->f_h;

	if (fabs(z1 - z2) < 0.5)
	{
		I_Warning("Detail slope on line #%d disabled: floors are same height\n", ld - lines);
		return;
	}

	if (z1 > z2)
	{
		sec = ld->backsector;

		z1 = ld->frontsector->f_h;
		z2 = ld->backsector ->f_h;
	}

	if (sec->f_slope)
	{
		I_Warning("Detail slope in sector #%d disabled: floor already sloped!\n", sec - sectors);
		return;
	}

	// limit height difference to no more than player step
	z1 = MAX(z1, z2 - 24.0);

	sec->f_slope = DetailSlope_BoundIt(ld, sec, z1 - sec->f_h, z2 - sec->f_h);
}

static void DetailSlope_Ceiling(line_t *ld)
{
	if (! ld->side[1])
		return;

	sector_t *sec = ld->frontsector;

	float z1 = ld->frontsector->c_h;
	float z2 = ld->backsector ->c_h;

	if (fabs(z1 - z2) < 0.5)
	{
		I_Warning("Detail slope on line #%d disabled: ceilings are same height\n", ld - lines);
		return;
	}

	if (z1 > z2)
	{
		sec = ld->backsector;

		z1 = ld->backsector ->c_h;
		z2 = ld->frontsector->c_h;
	}

	if (sec->c_slope)
	{
		I_Warning("Detail slope in sector #%d disabled: ceiling already sloped!\n", sec - sectors);
		return;
	}

#if 0
	// limit height difference to no more than this
	z2 = MIN(z2, z1 + 16.0);
#endif

	sec->c_slope = DetailSlope_BoundIt(ld, sec, z2 - sec->c_h, z1 - sec->c_h);
}

//
// EVENTS
//
// Events are operations triggered by using, crossing,
// or shooting special lines, or by timed thinkers.
//

//
// P_ActivateSpecialLine
//
// Called when a special line is activated.
//
// line is the line to be activated, side is the side activated from,
// (as lines can only be activated from the right), thing is the thing
// activating, to check for player/monster only lines trig is how it
// was activated, ie shot/crossed/pushed.  `line' can be NULL for
// non-line activations.
//
// -KM- 1998/09/01 Procedure Written.  
//
// -ACB- 1998/09/11 Return Success or Failure.
//
// -AJA- 1999/09/29: Updated for new tagged sector links.
//
// -AJA- 1999/10/21: Allow non-line activation (line == NULL), and
//                   added `typenum' and `tag' parameter.
//
// -AJA- 2000/01/02: New trigger method `line_Any'.
//
// -ACB- 2001/01/14: Added Elevator Sector Type
//
static bool P_ActivateSpecialLine(line_t * line,
		const linetype_c * special, int tag, int side, mobj_t * thing,
		trigger_e trig, int can_reach, int no_care_who)
{
	bool texSwitch = false;
	bool playedSound = false;

	sfx_t *sfx[4];
	sector_t *tsec;

	int i;

#ifdef DEVELOPERS
	if (!special)
	{
		if (line == NULL)
			I_Error("P_ActivateSpecialLine: Special type is 0\n");
		else
			I_Error("P_ActivateSpecialLine: Line %d is not Special\n", 
					(int)(line - lines));
	}
#endif

	if (!G_CheckWhenAppear(special->appear))
	{
		if (line)
			line->special = NULL;

		return true;
	}

	if (trig != line_Any && special->type != trig &&
			!(special->type == line_manual && trig == line_pushable))
		return false;

	// Check for use once.
	if (line && line->count == 0)
		return false;

	// Single sided line
	if (trig != line_Any && special->singlesided && side == 1)
		return false;

	// -AJA- 1999/12/07: Height checking.
	if (line && thing && thing->player && 
        (special->special_flags & LINSP_MustReach) && !can_reach)
	{
        S_StartFX(thing->info->noway_sound, 
                  P_MobjGetSfxCategory(thing), thing);

		return false;
	}

	// Check this type of thing can trigger
	if (!no_care_who)
	{
		if (thing && thing->player)
		{
			// Players can only trigger if the trig_player is set
			if (!(special->obj & trig_player))
				return false;

			if (thing->player->isBot() && (special->obj & trig_nobot))
				return false;
		}
		else if (thing && (thing->info->extendedflags & EF_MONSTER))
		{
			// Monsters can only trigger if the trig_monster flag is set
			if (!(special->obj & trig_monster))
				return false;

			// Monsters don't trigger secrets
			if (line && (line->flags & MLF_Secret))
				return false;
		}
		else
		{
			// Other stuff can only trigger if trig_other is set
			if (!(special->obj & trig_other))
				return false;

			// Other stuff doesn't trigger secrets
			if (line && (line->flags & MLF_Secret))
				return false;
		}
	}

	if (thing && !no_care_who)
	{
		// Check for keys
		// -ACB- 1998/09/11 Key possibilites extended
		if (special->keys != KF_NONE)
		{
			keys_e req = (keys_e)(special->keys & KF_MASK);
			keys_e cards;

			// Monsters/Missiles have no keys
			if (!thing->player)
				return false;

			//
			// New Security Checks, allows for any combination of keys in
			// an AND or OR function. Therefore it extends the possibilities
			// of security above 3 possible combinations..
			//
			// -AJA- Reworked this for the 10 new keys.
			//
			cards = thing->player->cards;

			bool failedsecurity = false;

			if (special->keys & KF_BOOM_SKCK)
			{
				// Boom compatibility: treat card and skull types the same
				cards = (keys_e)(EXPAND_KEYS(cards));
			}

			if (special->keys & KF_STRICTLY_ALL)
			{
				if ((cards & req) != req)
					failedsecurity = true;
			}
			else
			{
				if ((cards & req) == 0)
					failedsecurity = true;
			}

			if (failedsecurity)
			{
				if (special->failedmessage)
					CON_PlayerMessageLDF(thing->player->pnum, special->failedmessage);

				if (special->failed_sfx)
					S_StartFX(special->failed_sfx, SNCAT_Level, thing);

				return false;
			}
		}
	}

	// Check if button already pressed
	if (line && P_ButtonIsPressed(line))
		return false;

	// Do lights
	// -KM- 1998/09/27 Generalised light types.
	switch (special->l.type)
	{
		case LITE_Set:
			EV_LightTurnOn(tag, special->l.level);
			texSwitch = true;
			break;

		case LITE_None:
			break;

		default:
			texSwitch = P_DoSectorsFromTag(tag, &special->l, NULL, DoLights_wrapper);
			break;
	}

	// -ACB- 1998/09/13 Use teleport define..
	if (special->t.teleport)
	{
		texSwitch = EV_Teleport(line, tag, thing, &special->t);
	}

	if (special->e_exit == EXIT_Normal)
	{
		G_ExitLevel(5);
		texSwitch = true;
	}
	else if (special->e_exit == EXIT_Secret)
	{
		G_SecretExitLevel(5);
		texSwitch = true;
	}
	else if (special->e_exit == EXIT_Hub)
	{
		G_ExitToHub(special->hub_exit, line ? line->tag : tag);
		texSwitch = true;
	}

	if (special->d.dodonut)
	{
		// Proper ANSI C++ Init
		sfx[0] = special->d.d_sfxout;
		sfx[1] = special->d.d_sfxoutstop;
		sfx[2] = special->d.d_sfxin;
		sfx[3] = special->d.d_sfxinstop;

		texSwitch = P_DoSectorsFromTag(tag, NULL, sfx, DoDonut_wrapper);
	}

	//
	// - Plats/Floors -
	//
	if (special->f.type != mov_undefined)
	{
		if (!tag || special->type == line_manual)
		{
			if (line)
				texSwitch = EV_ManualPlane(line, thing, &special->f);
		}
		else
		{
			texSwitch = P_DoSectorsFromTag(tag, &special->f, 
					line ? line->frontsector : NULL, DoPlane_wrapper);
		}
	}

	//
	// - Doors/Ceilings -
	//
	if (special->c.type != mov_undefined)
	{
		if (!tag || special->type == line_manual)
		{
			if (line)
				texSwitch = EV_ManualPlane(line, thing, &special->c);
		}
		else
		{
			texSwitch = P_DoSectorsFromTag(tag, &special->c,
					line ? line->frontsector : NULL, DoPlane_wrapper);
		}
	}

	//
	// - Thin Sliding Doors -
	//
	if (special->s.type != SLIDE_None)
	{
		if (line && (!tag || special->type == line_manual))
		{
			EV_DoSlider(line, line, thing, special);
			texSwitch = false;

			// Must handle line count here, since the normal code in p_spec.c
			// will clear the line->special pointer, confusing various bits of
			// code that deal with sliding doors (--> crash).
			if (line->count > 0)
				line->count--;

			return true;
		}
		else if (tag)
		{
			for (i = 0; i < numlines; i++)
			{
				line_t *other = lines + i;

				if (other->tag == tag && other != line)
					if (EV_DoSlider(other, line, thing, special))
						texSwitch = true;
			}
		}
	}

	if (special->use_colourmap && tag > 0)
	{
		for (tsec = P_FindSectorFromTag(tag); tsec; tsec = tsec->tag_next)
		{
			tsec->props.colourmap = special->use_colourmap;
			texSwitch = true;
		}
	}

	if (special->gravity != FLO_UNUSED && tag > 0)
	{
		for (tsec = P_FindSectorFromTag(tag); tsec; tsec = tsec->tag_next)
		{
			tsec->props.gravity = special->gravity;
			texSwitch = true;
		}
	}

	if (special->friction != FLO_UNUSED && tag > 0)
	{
		for (tsec = P_FindSectorFromTag(tag); tsec; tsec = tsec->tag_next)
		{
			tsec->props.friction = special->friction;
			texSwitch = true;
		}
	}

	if (special->viscosity != FLO_UNUSED && tag > 0)
	{
		for (tsec = P_FindSectorFromTag(tag); tsec; tsec = tsec->tag_next)
		{
			tsec->props.viscosity = special->viscosity;
			texSwitch = true;
		}
	}

	if (special->drag != FLO_UNUSED && tag > 0)
	{
		for (tsec = P_FindSectorFromTag(tag); tsec; tsec = tsec->tag_next)
		{
			tsec->props.drag = special->drag;
			texSwitch = true;
		}
	}

	// Tagged line effects
	if (line && special->line_effect)
	{
		if (!tag)
		{
			P_LineEffect(line, line, special);
			texSwitch = true;
		}
		else
		{
			for (i=0; i < numlines; i++)
			{
				if (lines[i].tag == tag)
				{
					P_LineEffect(lines + i, line, special);
					texSwitch = true;
				}
			}
		}
	}

	// Tagged sector effects
	if (line && special->sector_effect)
	{
		if (!tag)
		{
			if (special->special_flags & LINSP_BackSector)
				P_SectorEffect(line->backsector, line, special);
			else
				P_SectorEffect(line->frontsector, line, special);

			texSwitch = true;
		}
		else
		{
			for (tsec = P_FindSectorFromTag(tag); tsec; tsec = tsec->tag_next)
			{
				P_SectorEffect(tsec, line, special);
				texSwitch = true;
			}
		}
	}

	if (special->trigger_effect && tag > 0)
	{
		RAD_EnableByTag(thing, tag, special->trigger_effect < 0);
		texSwitch = true;
	}

	if (special->ambient_sfx && tag > 0)
	{
		for (tsec = P_FindSectorFromTag(tag); tsec; tsec = tsec->tag_next)
		{
			P_AddAmbientSFX(tsec, special->ambient_sfx);
			texSwitch = true;
		}
	}

	if (special->music)
	{
		S_ChangeMusic(special->music, true);
		texSwitch = true;
	}

	if (special->activate_sfx)
	{
		if (line)
        {
			S_StartFX(special->activate_sfx, SNCAT_Level,
                           &line->frontsector->sfx_origin);
		}
        else if (thing)
        {
            S_StartFX(special->activate_sfx, 
                           P_MobjGetSfxCategory(thing), thing);
        }

		playedSound = true;
	}

	// reduce count & clear special if necessary
	if (line && texSwitch)
	{
		if (line->count != -1)
		{
			line->count--;

			if (!line->count)
				line->special = NULL;
		}
		// -KM- 1998/09/27 Reversable linedefs.
		if (line->special && special->newtrignum)
		{
			line->special = (special->newtrignum <= 0) ? NULL :
				P_LookupLineType(special->newtrignum);
		}

		P_ChangeSwitchTexture(line, line->special && (special->newtrignum == 0),
				special->special_flags, playedSound);
	}

	return true;
}

//
// Called every time a thing origin is about
// to cross a line with a non-zero special.
//
// -KM- 1998/09/01 Now much simpler
// -ACB- 1998/09/12 Return success/failure
//
bool P_CrossSpecialLine(line_t *ld, int side, mobj_t * thing)
{
	return P_ActivateSpecialLine(ld, ld->special, ld->tag, 
			side, thing, line_walkable, 1, 0);
}

//
// Called when a thing shoots a special line.
//
void P_ShootSpecialLine(line_t * ld, int side, mobj_t * thing)
{
	P_ActivateSpecialLine(ld, ld->special, ld->tag, 
			side, thing, line_shootable, 1, 0);
}

//
// Called when a thing uses a special line.
// Only the front sides of lines are usable.
//
// -KM- 1998/09/01 Uses new lines.ddf code in p_spec.c
//
// -ACB- 1998/09/07 Uses the return value to discern if a move if possible.
//
// -AJA- 1999/12/07: New parameters `open_bottom' and `open_top',
//       which give a vertical range through which the linedef is
//       accessible.  Could be used for smarter switches, like one on
//       a lower wall-part which is out of reach (e.g. MAP02).
//
bool P_UseSpecialLine(mobj_t * thing, line_t * line, int side,
		float open_bottom, float open_top)
{
	int can_reach = (thing->z < open_top) &&
		(thing->z + thing->height + USE_Z_RANGE >= open_bottom);

	return P_ActivateSpecialLine(line, line->special, line->tag, side,
			thing, line_pushable, can_reach, 0);
}

//
// Called by the RTS `ACTIVATE_LINETYPE' primitive, and also the code
// pointer in things.ddf of the same name.  Thing can be NULL.
//
// -AJA- 1999/10/21: written.
//
void P_RemoteActivation(mobj_t * thing, int typenum, int tag, 
		int side, trigger_e method)
{
	const linetype_c *spec = P_LookupLineType(typenum);

	P_ActivateSpecialLine(NULL, spec, tag, side, thing, method, 1,
			(thing == NULL));
}


static inline void PlayerInProperties(player_t *player,
		float bz, float tz, float f_h, float c_h,
		region_properties_t *props,
		const sectortype_c ** swim_special)
{
	const sectortype_c *special = props->special;
	float damage, factor;

	if (!special || c_h < f_h)
		return;

	if (!G_CheckWhenAppear(special->appear))
		return;

	// breathing support
	// (Mouth is where the eye is !)
	//
	float mouth_z = player->mo->z + player->viewz;

	if ((special->special_flags & SECSP_AirLess) &&
		mouth_z >= f_h && mouth_z <= c_h &&
		player->powers[PW_Scuba] <= 0)
	{
		player->air_in_lungs--;
		player->underwater = true;

		if (player->air_in_lungs <= 0 &&
				(leveltime % (1 + player->mo->info->choke_damage.delay)) == 0)
		{
			DAMAGE_COMPUTE(damage, &player->mo->info->choke_damage);

			if (damage)
				P_DamageMobj(player->mo, NULL, NULL, damage,
						&player->mo->info->choke_damage);
		}
	}

	if ((special->special_flags & SECSP_Swimming) &&
		mouth_z >= f_h && mouth_z <= c_h)
	{
		player->swimming = true;
		*swim_special = special;
	}

	if ((special->special_flags & SECSP_Swimming) &&
		player->mo->z >= f_h && player->mo->z <= c_h)
	{
		player->wet_feet = true;
	}

	factor = 1.0f;

	if (special->special_flags & SECSP_WholeRegion)
	{
		if (special->special_flags & SECSP_Proportional)
		{
			// only partially in region -- mitigate damage

			if (tz > c_h)
				factor -= factor * (tz - c_h) / (tz-bz);

			if (bz < f_h)
				factor -= factor * (f_h - bz) / (tz-bz);
		}
		else
		{
			if (bz > c_h || tz < f_h)
				factor = 0;
		}
	}
	else
	{

		// Not touching the floor ?
		if (player->mo->z > f_h + 2.0f)
			return;
	}

	if (player->powers[PW_AcidSuit])
		factor = 0;

	if (factor > 0 &&
			(leveltime % (1 + special->damage.delay)) == 0)
	{
		DAMAGE_COMPUTE(damage, &special->damage);

		if (damage)
			P_DamageMobj(player->mo, NULL, NULL, damage * factor,
					&special->damage);
	}

	if (special->secret)
	{
		player->secretcount++;

		props->type = 0;
		props->special = NULL;
	}

	if (special->e_exit != EXIT_None)
	{
		player->cheats &= ~CF_GODMODE;

		if (player->health <= special->damage.nominal)
		{
            S_StartFX(player->mo->info->deathsound,
                           P_MobjGetSfxCategory(player->mo),
                           player->mo);

			// -KM- 1998/12/16 We don't want to alter the special type,
			//   modify the sector's attributes instead.
			props->special = NULL;

			if (special->e_exit == EXIT_Secret)
				G_SecretExitLevel(1);
			else
				G_ExitLevel(1);
		}
	}
}


//
// Called every tic frame that the player origin is in a special sector
//
// -KM- 1998/09/27 Generalised for sectors.ddf
// -AJA- 1999/10/09: Updated for new sector handling.
//
void P_PlayerInSpecialSector(player_t * player, sector_t * sec)
{
	extrafloor_t *S, *L, *C;
	float floor_h;

	float bz = player->mo->z;
	float tz = player->mo->z + player->mo->height;

	bool was_underwater = player->underwater;
	bool was_swimming   = player->swimming;

	const sectortype_c *swim_special = NULL;

	player->swimming = false;
	player->underwater = false;
	player->wet_feet = false;

	// traverse extrafloor list
	floor_h = sec->f_h;

	S = sec->bottom_ef;
	L = sec->bottom_liq;

	while (S || L)
	{
		if (!L || (S && S->bottom_h < L->bottom_h))
		{
			C = S;  S = S->higher;
		}
		else
		{
			C = L;  L = L->higher;
		}

		SYS_ASSERT(C);

		// ignore "hidden" liquids
		if (C->bottom_h < floor_h || C->bottom_h > sec->c_h)
			continue;

		PlayerInProperties(player, bz, tz, floor_h, C->top_h, C->p, &swim_special);

		floor_h = C->top_h;
	}

	PlayerInProperties(player, bz, tz, floor_h, sec->c_h, sec->p, &swim_special);

	// breathing support: handle gasping when leaving the water
	if (was_underwater && !player->underwater)
	{
		if (player->air_in_lungs <=
				(player->mo->info->lung_capacity - player->mo->info->gasp_start))
		{
			if (player->mo->info->gasp_sound)
            {
                S_StartFX(player->mo->info->gasp_sound,
                          P_MobjGetSfxCategory(player->mo),
                          player->mo);
            }
		}

		player->air_in_lungs = player->mo->info->lung_capacity;
	} 

	// -AJA- 2008/01/20: water splash sounds for players
	if (!was_swimming && player->swimming)
	{
		SYS_ASSERT(swim_special);

		if (player->splashwait == 0 && swim_special->splash_sfx)
		{
			S_StartFX(swim_special->splash_sfx, SNCAT_Level, player->mo);
		}
	}
	else if (was_swimming && !player->swimming)
	{
		player->splashwait = TICRATE;
	}
}

static inline void ApplyScroll(vec2_t& offset, const vec2_t& delta)
{
	// prevent loss of precision (eventually stops the scrolling)

	offset.x = fmod(offset.x + delta.x, 4096.0f);
	offset.y = fmod(offset.y + delta.y, 4096.0f);
}

//
// Animate planes, scroll walls, etc.
//
void P_UpdateSpecials(void)
{
	// LEVEL TIMER
	if (levelTimer == true)
	{
		levelTimeCount--;

		if (!levelTimeCount)
			G_ExitLevel(1);
	}

	// ANIMATE LINE SPECIALS
	// -KM- 1998/09/01 Lines.ddf
	std::list<line_t *>::iterator LI;

	for (LI  = active_line_anims.begin();
		 LI != active_line_anims.end();
		 LI++)
	{
		line_t *ld = *LI;

		// -KM- 1999/01/31 Use new method.
		// -AJA- 1999/07/01: Handle both sidedefs.
		if (ld->side[0])
		{
			ApplyScroll(ld->side[0]->top.offset,    ld->side[0]->top.scroll);
			ApplyScroll(ld->side[0]->middle.offset, ld->side[0]->middle.scroll);
			ApplyScroll(ld->side[0]->bottom.offset, ld->side[0]->bottom.scroll);
		}

		if (ld->side[1])
		{
			ApplyScroll(ld->side[1]->top.offset,    ld->side[1]->top.scroll);
			ApplyScroll(ld->side[1]->middle.offset, ld->side[1]->middle.scroll);
			ApplyScroll(ld->side[1]->bottom.offset, ld->side[1]->bottom.scroll);
		}
	}

	// ANIMATE SECTOR SPECIALS
	std::list<sector_t *>::iterator SI;

	for (SI  = active_sector_anims.begin();
		 SI != active_sector_anims.end();
		 SI++)
	{
		sector_t *sec = *SI;

		ApplyScroll(sec->floor.offset, sec->floor.scroll);
		ApplyScroll(sec->ceil.offset,  sec->ceil.scroll);
	}

	// DO BUTTONS
	P_UpdateButtons();
}

//
//  SPECIAL SPAWNING
//


//
// This function is called at the start of every level.  It parses command line
// parameters for level timer, spawns passive special sectors, (ie sectors that
// act even when a player is not in them, and counts total secrets) spawns
// passive lines, (ie scrollers) and resets floor/ceiling movement.
//
// -KM- 1998/09/27 Generalised for sectors.ddf
// -KM- 1998/11/25 Lines with auto tag are automatically triggered.
//
// -AJA- split into two, the first is called _before_ things are
//       loaded, and the second one _afterwards_.
//
void P_SpawnSpecials1(void)
{
	const char *s;
	int i;

	active_sector_anims.clear();
	active_line_anims.clear();

	P_ClearButtons();

	// See if -TIMER needs to be used.
	levelTimer = false;

	i = M_CheckParm("-avg");
	if (i && DEATHMATCH())
	{
		levelTimer = true;
		levelTimeCount = 20 * 60 * TICRATE;
	}

	s = M_GetParm("-timer");
	if (s && DEATHMATCH())
	{
		int time;

		time = atoi(s) * 60 * TICRATE;
		levelTimer = true;
		levelTimeCount = time;
	}

	for (i = 0; i < numlines; i++)
	{
		const linetype_c *special = lines[i].special;

		if (! special)
		{
			lines[i].count = 0;
			continue;
		}

		// -AJA- 1999/10/23: weed out non-appearing lines.
		if (! G_CheckWhenAppear(special->appear))
		{
			lines[i].special = NULL;
			continue;
		}

		lines[i].count = special->count;

		// -AJA- 2007/12/29: Portal effects
		if (special->portal_effect != PORTFX_None)
		{
			P_PortalEffect(&lines[i]);
		}

		// Extrafloor creation
		if (special->ef.type != EXFL_None && lines[i].tag > 0)
		{
			sector_t *ctrl = lines[i].frontsector;

			for (sector_t * tsec = P_FindSectorFromTag(lines[i].tag); tsec; tsec = tsec->tag_next)
			{
				if (special->ef.type & EXFL_BoomTex)
				{
					if (ctrl->f_h <= tsec->f_h)
					{
						tsec->props.colourmap = ctrl->props.colourmap;

						// FIXME: BOOM's invisible floor feature
						continue;
					}
				}

				P_AddExtraFloor(tsec, &lines[i]);

				// transfer any translucency
				if (PERCENT_2_FLOAT(special->translucency) <= 0.99f)
				{
					P_EFTransferTrans(ctrl, tsec, &lines[i], &special->ef,
							PERCENT_2_FLOAT(special->translucency));
				}

				// update the line gaps & things:
				P_RecomputeGapsAroundSector(tsec);

				P_FloodExtraFloors(tsec);
			}
		}

		// Detail slopes
		if (special->slope_type & SLP_DetailFloor)
		{
			DetailSlope_Floor(&lines[i]);
		}
		if (special->slope_type & SLP_DetailCeiling)
		{
			DetailSlope_Ceiling(&lines[i]);
		}
	}
}

void P_SpawnSpecials2(int autotag)
{
	sector_t *sector;
	const sectortype_c *secSpecial;
	const linetype_c *special;

	int i;

	//
	// Init special SECTORs.
	//

	sector = sectors;
	for (i = 0; i < numsectors; i++, sector++)
	{
		if (!sector->props.special)
			continue;

		secSpecial = sector->props.special;

		if (! G_CheckWhenAppear(secSpecial->appear))
		{
			P_SectorChangeSpecial(sector, 0);
			continue;
		}

		if (secSpecial->l.type != LITE_None)
			EV_Lights(sector, &secSpecial->l);

		if (secSpecial->secret)
			wi_stats.secret++;

		if (secSpecial->use_colourmap)
			sector->props.colourmap = secSpecial->use_colourmap;

		if (secSpecial->ambient_sfx)
			P_AddAmbientSFX(sector, secSpecial->ambient_sfx);

		// - Plats/Floors -
		if (secSpecial->f.type != mov_undefined)
			EV_DoPlane(sector, &secSpecial->f, sector);

		// - Doors/Ceilings -
		if (secSpecial->c.type != mov_undefined)
			EV_DoPlane(sector, &secSpecial->c, sector);

		sector->props.gravity   = secSpecial->gravity;
		sector->props.friction  = secSpecial->friction;
		sector->props.viscosity = secSpecial->viscosity;
		sector->props.drag      = secSpecial->drag;

		// compute pushing force
		if (secSpecial->push_speed > 0 || secSpecial->push_zspeed > 0)
		{
			float mul = secSpecial->push_speed / 100.0f;

			sector->props.push.x += M_Cos(secSpecial->push_angle) * mul;
			sector->props.push.y += M_Sin(secSpecial->push_angle) * mul;
			sector->props.push.z += secSpecial->push_zspeed / 100.0f;
		}

		// Scrollers
		if (secSpecial->f.scroll_speed > 0)
		{
			float dx = M_Cos(secSpecial->f.scroll_angle);
			float dy = M_Sin(secSpecial->f.scroll_angle);

			sector->floor.scroll.x -= dx * secSpecial->f.scroll_speed / 32.0f;
			sector->floor.scroll.y -= dy * secSpecial->f.scroll_speed / 32.0f;

			P_AddSpecialSector(sector);
		}
		if (secSpecial->c.scroll_speed > 0)
		{
			float dx = M_Cos(secSpecial->c.scroll_angle);
			float dy = M_Sin(secSpecial->c.scroll_angle);

			sector->ceil.scroll.x -= dx * secSpecial->c.scroll_speed / 32.0f;
			sector->ceil.scroll.y -= dy * secSpecial->c.scroll_speed / 32.0f;

			P_AddSpecialSector(sector);
		}
	}

	//
	// Init special LINEs.
	//
	// -ACB- & -JC- 1998/06/10 Implemented additional scroll effects
	//
	// -ACB- Added the code
	// -JC-  Designed and contributed code
	// -KM-  Removed Limit
	// -KM- 1998/09/01 Added lines.ddf support
	//
	for (i = 0; i < numlines; i++)
	{
		special = lines[i].special;

		if (! special)
			continue;

		if (special->s_xspeed || special->s_yspeed)
		{
			AdjustScrollParts(lines[i].side[0], 0, special->scroll_parts,
					special->s_xspeed, special->s_yspeed);

			AdjustScrollParts(lines[i].side[1], 1, special->scroll_parts,
					special->s_xspeed, special->s_yspeed);

			P_AddSpecialLine(lines + i);
		}

		// -AJA- 1999/06/30: Translucency effect.
		if (PERCENT_2_FLOAT(special->translucency) <= 0.99f && lines[i].side[0])
			lines[i].side[0]->middle.translucency = PERCENT_2_FLOAT(special->translucency);

		if (PERCENT_2_FLOAT(special->translucency) <= 0.99f && lines[i].side[1])
			lines[i].side[1]->middle.translucency = PERCENT_2_FLOAT(special->translucency);

		if (special->autoline)
		{
			P_ActivateSpecialLine(&lines[i], lines[i].special,
					lines[i].tag, 0, NULL, line_Any, 1, 1);
		}

		// -KM- 1998/11/25 This line should be pushed automatically
		if (autotag && lines[i].special && lines[i].tag == autotag)
		{
			P_ActivateSpecialLine(&lines[i], lines[i].special,
					lines[i].tag, 0, NULL, line_pushable, 1, 1);
		}
	}
}

//
// -KM- 1998/09/27 This helper function is used to do stuff to all the
//                 sectors with the specified line's tag.
//
// -AJA- 1999/09/29: Updated for new tagged sector links.
//
static bool P_DoSectorsFromTag(int tag, const void *p1, void *p2,
		bool(*func) (sector_t *, const void *, void *))
{
	sector_t *tsec;
	bool rtn = false;

	for (tsec = P_FindSectorFromTag(tag); tsec; tsec = tsec->tag_next)
	{
		if ((*func) (tsec, p1, p2))
			rtn = true;
	}

	return rtn;
}


void P_SectorChangeSpecial(sector_t *sec, int new_type)
{
	sec->props.type = MAX(0, new_type);

	sec->props.special = P_LookupSectorType(sec->props.type);
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
