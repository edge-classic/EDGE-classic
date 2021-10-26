//----------------------------------------------------------------------------
//  EDGE Floor/Ceiling/Stair/Elevator Action Code
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

#include "z_zone.h"
#include "dm_defs.h"
#include "dm_state.h"
#include "m_random.h"
#include "p_local.h"
#include "r_sky.h"
#include "r_state.h"
#include "s_sound.h"

#include <algorithm>

typedef enum
{
    RES_Ok,
    RES_Crushed,
    RES_PastDest,
    RES_Impossible
}
move_result_e;


std::vector<plane_move_t *>  active_planes;
std::vector<slider_move_t *> active_sliders;


linetype_c donut[2];
static int donut_setup = 0;


static bool P_ActivateInStasis(int tag);
static bool P_StasifySector(int tag);


// -AJA- Perhaps using a pointer to `plane_info_t' would be better
//       than f**king about with the floorOrCeiling stuff all the
//       time.

static float HEIGHT(sector_t * sec, bool is_ceiling)
{
    if (is_ceiling)
        return sec->c_h;

    return sec->f_h;
}

static const image_c * SECPIC(sector_t * sec, bool is_ceiling,
                              const image_c *new_image)
{
    if (new_image)
    {
        if (is_ceiling) 
            sec->ceil.image = new_image;
        else
            sec->floor.image = new_image;

		if (new_image == skyflatimage)
			R_ComputeSkyHeights();
    }

    return is_ceiling ? sec->ceil.image : sec->floor.image;
}

//
// GetSecHeightReference
//
// Finds a sector height, using the reference provided; will select
// the approriate method of obtaining this value, if it cannot
// get it directly.
//
// -KM-  1998/09/01 Wrote Procedure.
// -ACB- 1998/09/06 Remarked and Reformatted.
// -ACB- 2001/02/04 Move to p_plane.c
//
static float GetSecHeightReference(heightref_e ref,
	sector_t * sec, sector_t * model)
{
    switch (ref & REF_MASK)
    {
        case REF_Absolute:
            return 0;

        case REF_Trigger:
			if (model)
				return (ref & REF_CEILING) ? model->c_h : model->f_h;

			return 0; // ick!

        case REF_Current:
            return (ref & REF_CEILING) ? sec->c_h : sec->f_h;

        case REF_Surrounding:
            return P_FindSurroundingHeight(ref, sec);

        case REF_LowestLoTexture:
            return P_FindRaiseToTexture(sec);

        default:
            I_Error("GetSecHeightReference: undefined reference %d\n", ref);
    }

    return 0;
}

#define RELOOP_TICKS  6

static void MakeMovingSound(bool *started_var, sfx_t *sfx, position_c *pos)
{
	if (!sfx || sfx->num < 1)
		return;

	sfxdef_c *def = sfxdefs[sfx->sounds[0]];

	// looping sounds need to be "pumped" to keep looping.
	// The main one is STNMOV, which lasts a little over 0.25 seconds,
	// hence we need to pump it every 6 tics or so.

	if (! *started_var || (def->looping && (leveltime % RELOOP_TICKS)==0))
	{
		S_StartFX(sfx, SNCAT_Level, pos);

		*started_var = true;
	}
}

void P_AddActivePlane(plane_move_t *pmov)
{
	active_planes.push_back(pmov);
}

void P_AddActiveSlider(slider_move_t *smov)
{
	active_sliders.push_back(smov);
}


//
// -ACB- This is a clear-the-decks function: we don't care
// with tyding up the loose ends inbetween removing active
// parts - that is pointless since we are nuking the entire
// thing. Hence the lack of call to P_RemoveActivePart() for
// each individual part.
//
void P_DestroyAllPlanes(void)
{
	std::vector<plane_move_t *> ::iterator PMI;

	for (PMI  = active_planes.begin();
		 PMI != active_planes.end();
		 PMI++)
	{
		delete (*PMI);
	}

	active_planes.clear();
}

void P_DestroyAllSliders(void)
{
	std::vector<slider_move_t *>::iterator SMI;

	for (SMI  = active_sliders.begin();
		 SMI != active_sliders.end();
		 SMI++)
	{
		delete (*SMI);
	}

	active_sliders.clear();
}

//
// FLOORS
//

//
// AttemptMovePlane
//
// Move a plane (floor or ceiling) and check for crushing
//
// Returns:
//    RES_Ok - the move was completely successful.
//
//    RES_Impossible - the move was not possible due to another solid
//    surface (e.g. an extrafloor) getting in the way.  The plane will
//    remain at its current height.
//
//    RES_PastDest - the destination height has been reached.  The
//    actual height in the sector may not be the target height, which
//    means some objects got in the way (whether crushed or not).
//
//    RES_Crushed - some objects got in the way.  When `crush'
//    parameter is non-zero, those object will have been crushed (take
//    damage) and the plane height will be the new height, otherwise
//    the plane height will remain at its current height.
//
static move_result_e AttemptMovePlane(sector_t * sector, 
                                      float speed, float dest, int crush, 
                                      bool is_ceiling, int direction)
{
    bool past = false;
    bool nofit;

    //
    // check whether we have gone past the destination height
    //
    if (direction == DIRECTION_UP && 
        HEIGHT(sector, is_ceiling) + speed > dest)
    {
        past = true;
        speed = dest - HEIGHT(sector, is_ceiling);
    }
    else if (direction == DIRECTION_DOWN && 
             HEIGHT(sector, is_ceiling) - speed < dest)
    {
        past = true;
        speed = HEIGHT(sector, is_ceiling) - dest;
    }
 
    if (speed <= 0)
        return RES_PastDest;

    if (direction == DIRECTION_DOWN)
        speed = -speed;

    // check if even possible
    if (! P_CheckSolidSectorMove(sector, is_ceiling, speed))
    {
        return RES_Impossible;
    }
   
    //
    // move the actual sector, including all things in it
    //
    nofit = P_SolidSectorMove(sector, is_ceiling, speed, crush, false);
    
    if (! nofit)
        return past ? RES_PastDest : RES_Ok;

    // bugger, something got in our way !
 
    if (crush == 0)
    {
        // undo the change
        P_SolidSectorMove(sector, is_ceiling, -speed, false, false);
    }

    return past ? RES_PastDest : RES_Crushed;
}

static move_result_e AttemptMoveSector(sector_t * sector,
		plane_move_t *pmov, float dest, int crush)
{
	move_result_e res;

	if (! pmov->is_elevator)
	{
		return AttemptMovePlane(sector, pmov->speed, dest, crush,
				  pmov->is_ceiling, pmov->direction);
	}

	//-------------------//
	//   ELEVATOR MOVE   //
	//-------------------//

	if (pmov->direction == DIRECTION_UP)
	{
		AttemptMovePlane(sector, 32768.0, 
				MIN(sector->f_h + pmov->speed, dest) + pmov->elev_height,
				false, true, DIRECTION_UP);
	}

	res = AttemptMovePlane(sector, pmov->speed, dest, crush,
			  false, pmov->direction);
	
	if (pmov->direction == DIRECTION_DOWN)
	{
		AttemptMovePlane(sector, 32768.0, sector->f_h + pmov->elev_height,
				false, true, DIRECTION_DOWN);
	}

	return res;
}

static bool MovePlane(plane_move_t *plane)
{
	// Move a floor to it's destination (up or down).
	//
	// RETURNS true if plane_move_t should be removed.
	
    move_result_e res;

    switch (plane->direction)
    {
        case DIRECTION_STASIS:
			plane->sfxstarted = false;
            break;

        case DIRECTION_DOWN:
            res = AttemptMoveSector(plane->sector, plane,
                                   MIN(plane->startheight, plane->destheight),
                                   plane->is_ceiling ? plane->crush : 0);

			MakeMovingSound(&plane->sfxstarted, plane->type->sfxdown,
                            &plane->sector->sfx_origin);

            if (res == RES_PastDest)
            {
                S_StartFX(plane->type->sfxstop, 
                               SNCAT_Level,
                               &plane->sector->sfx_origin);

                plane->speed = plane->type->speed_up;

                if (plane->newspecial != -1)
                {
					P_SectorChangeSpecial(plane->sector, plane->newspecial);
                }

                SECPIC(plane->sector, plane->is_ceiling, plane->new_image);

                switch (plane->type->type)
                {
                    case mov_Plat:
                    case mov_Continuous:
                        plane->direction = DIRECTION_WAIT;
                        plane->waited = plane->type->wait;
                        plane->speed = plane->type->speed_up;
                        break;

                    case mov_MoveWaitReturn:
                        if (HEIGHT(plane->sector, plane->is_ceiling) == plane->startheight)
                        {
                            return true; // REMOVE ME
                        }
                        else  // assume we reached the destination
                        {
                            plane->direction = DIRECTION_WAIT;
                            plane->waited = plane->type->wait;
                            plane->speed = plane->type->speed_up;
                        }
                        break;

                    case mov_Toggle:
                        plane->direction = DIRECTION_STASIS;
                        plane->olddirection = DIRECTION_UP;
                        break;

                    default:
                        return true; // REMOVE ME
                }
            }
            else if (res == RES_Crushed || res == RES_Impossible)
            {
                if (plane->crush)
                {
                    plane->speed = plane->type->speed_down;

					if (plane->speed < 1.5f)
						plane->speed = plane->speed / 8.0f;
                }
                else if (plane->type->type == mov_MoveWaitReturn)  // Go back up
                {
                    plane->direction = DIRECTION_UP;
                    plane->sfxstarted = false;
                    plane->waited = 0;
                    plane->speed = plane->type->speed_up;
                }
            }

            break;

        case DIRECTION_WAIT:
            if (--plane->waited <= 0)
            {
                int dir;
                float dest;

                if (HEIGHT(plane->sector, plane->is_ceiling) == plane->destheight)
                    dest = plane->startheight;
                else
                    dest = plane->destheight;

                if (HEIGHT(plane->sector, plane->is_ceiling) > dest)
                {
                    dir = DIRECTION_DOWN;
                    plane->speed = plane->type->speed_down;
                }
                else
                {
                    dir = DIRECTION_UP;
                    plane->speed = plane->type->speed_up;
                }

                if (dir)
                {
                    S_StartFX(plane->type->sfxstart, 
                                   SNCAT_Level,
                                   &plane->sector->sfx_origin);
                }

                plane->direction = dir;  // time to go back
                plane->sfxstarted = false;
            }
            break;

        case DIRECTION_UP:
            res = AttemptMoveSector(plane->sector, plane,
                                   MAX(plane->startheight, plane->destheight),
                                   plane->is_ceiling ? 0 : plane->crush);

			MakeMovingSound(&plane->sfxstarted, plane->type->sfxup,
                            &plane->sector->sfx_origin);

            if (res == RES_PastDest)
            {
                S_StartFX(plane->type->sfxstop, 
                               SNCAT_Level,
                               &plane->sector->sfx_origin);

                if (plane->newspecial != -1)
                {
					P_SectorChangeSpecial(plane->sector, plane->newspecial);
                }

                SECPIC(plane->sector, plane->is_ceiling, plane->new_image);

                switch (plane->type->type)
                {
                    case mov_Plat:
                    case mov_Continuous:
                        plane->direction = DIRECTION_WAIT;
                        plane->waited = plane->type->wait;
                        plane->speed = plane->type->speed_down;
                        break;

                    case mov_MoveWaitReturn:
                        if (HEIGHT(plane->sector, plane->is_ceiling) == plane->startheight)
                        {
                            return true; // REMOVE ME
                        }
                        else  // assume we reached the destination
                        {
                            plane->direction = DIRECTION_WAIT;
                            plane->speed = plane->type->speed_down;
                            plane->waited = plane->type->wait;
                        }
                        break;

                    case mov_Toggle:
                        plane->direction = DIRECTION_STASIS;
                        plane->olddirection = DIRECTION_DOWN;
                        break;

                    default:
						return true; // REMOVE ME
                }

            }
            else if (res == RES_Crushed || res == RES_Impossible)
            {
                if (plane->crush)
                {
                    plane->speed = plane->type->speed_up;

					if (plane->speed < 1.5f)
						plane->speed = plane->speed / 8.0f;
                }
                else if (plane->type->type == mov_MoveWaitReturn)  // Go back down
                {
                    plane->direction = DIRECTION_DOWN;
                    plane->sfxstarted = false;
                    plane->waited = 0;
                    plane->speed = plane->type->speed_down;
                }
            }
            break;

        default:
            I_Error("MovePlane: Unknown direction %d", plane->direction);
    }

	return false;
}

static sector_t *P_GSS(sector_t * sec, float dest, bool forc)
{
    int i;
    int secnum = sec - sectors;
    sector_t *sector;

    for (i = sec->linecount-1; i; i--)
    {
        if (P_TwoSided(secnum, i))
        {
            if (P_GetSide(secnum, i, 0)->sector - sectors == secnum)
            {
                sector = P_GetSector(secnum, i, 1);

                if (SECPIC(sector, forc, NULL) != SECPIC(sec, forc, NULL)
                    && HEIGHT(sector, forc) == dest)
                {
                    return sector;
                }

            }
            else
            {
                sector = P_GetSector(secnum, i, 0);
        
                if (SECPIC(sector, forc, NULL) != SECPIC(sec, forc, NULL)
                    && HEIGHT(sector, forc) == dest)
                {
                    return sector;
                }
            }
        }
    }

    for (i = sec->linecount; i--;)
    {
        if (P_TwoSided(secnum, i))
        {
            if (P_GetSide(secnum, i, 0)->sector - sectors == secnum)
            {
                sector = P_GetSector(secnum, i, 1);
            }
            else
            {
                sector = P_GetSector(secnum, i, 0);
            }
            if (sector->validcount != validcount)
            {
                sector->validcount = validcount;
                sector = P_GSS(sector, dest, forc);
                if (sector)
                    return sector;
            }
        }
    }

    return NULL;
}

static sector_t *P_GetSectorSurrounding(sector_t * sec, float dest, bool forc)
{
    validcount++;
    sec->validcount = validcount;
    return P_GSS(sec, dest, forc);
}

void P_SetupPlaneDirection(plane_move_t *plane, const movplanedef_c *def, 
                           float start, float dest)
{
    plane->startheight = start;
    plane->destheight = dest;

    if (dest > start)
    {
        plane->direction = DIRECTION_UP;

        if (def->speed_up >= 0)
            plane->speed = def->speed_up;
        else
            plane->speed = dest - start;
    }
    else if (start > dest)
    {
        plane->direction = DIRECTION_DOWN;

        if (def->speed_down >= 0)
            plane->speed = def->speed_down;
        else
            plane->speed = start - dest;
    }

    return;
}

//
// Setup the Floor Action, depending on the linedeftype trigger and the
// sector info.
//
static plane_move_t *P_SetupSectorAction(sector_t * sector, 
                                         const movplanedef_c * def, 
                                         sector_t * model)
{
    // new door thinker
    plane_move_t *plane = new plane_move_t;

    if (def->is_ceiling)
        sector->ceil_move = plane;
    else
        sector->floor_move = plane;

    plane->sector = sector;
    plane->crush = def->crush_damage;
    plane->sfxstarted = false;

    float start = HEIGHT(sector, def->is_ceiling);

    float dest = GetSecHeightReference(def->destref, sector, model);
    dest += def->dest;

    if (def->type == mov_Plat || def->type == mov_Continuous ||
        def->type == mov_Toggle)
    {
        start = GetSecHeightReference(def->otherref, sector, model);
        start += def->other;
    }

#if 0  // DEBUG
    L_WriteDebug("SEC_ACT: %d type %d %s start %1.0f dest %1.0f\n",
                 sector - sectors, def->type, 
                 def->is_ceiling ? "CEIL" : "FLOOR", 
                 start, dest);
#endif

    if (def->prewait)
    {
        plane->direction = DIRECTION_WAIT;
        plane->waited = def->prewait;
        plane->destheight = dest;
        plane->startheight = start;
    }
    else if (def->type == mov_Continuous)
    {
        plane->direction = (P_Random() & 1) ? DIRECTION_UP : DIRECTION_DOWN;

        if (plane->direction == DIRECTION_UP)
            plane->speed = def->speed_up;
        else
            plane->speed = def->speed_down;

        plane->destheight = dest;
        plane->startheight = start;
    }
    else if (start != dest)
    {
        P_SetupPlaneDirection(plane, def, start, dest);
    }
    else
    {
        delete plane;

        if (def->is_ceiling)
            sector->ceil_move = NULL;
        else
            sector->floor_move = NULL;

        return NULL;
    }

    plane->tag = sector->tag;
    plane->type = def;
    plane->new_image = SECPIC(sector, def->is_ceiling, NULL);
    plane->newspecial = -1;
    plane->is_ceiling = def->is_ceiling;
	plane->is_elevator = (def->type == mov_Elevator);
	plane->elev_height = sector->c_h - sector->f_h;

    // -ACB- 10/01/2001 Trigger starting sfx
// UNNEEDED    sound::StopLoopingFX(&sector->sfx_origin);

    if (def->sfxstart)
    {
        S_StartFX(def->sfxstart, SNCAT_Level, &sector->sfx_origin);
    }

    // change to surrounding
    if (def->tex[0] == '-')
    {
        model = P_GetSectorSurrounding(sector, 
                                       plane->destheight, 
                                       def->is_ceiling);
        if (model)
        {
            plane->new_image = SECPIC(model, def->is_ceiling, NULL);

            plane->newspecial = model->props.special ?
                model->props.special->number : 0;
        }

        if (plane->direction == (def->is_ceiling ? DIRECTION_DOWN : DIRECTION_UP))
        {
            SECPIC(sector, def->is_ceiling, plane->new_image);
            if (plane->newspecial != -1)
            {
				P_SectorChangeSpecial(sector, plane->newspecial);
            }
        }
    }
    else if (def->tex[0] == '+')
    {
        if (model)
        {
            if (SECPIC(model,  def->is_ceiling, NULL) == 
                SECPIC(sector, def->is_ceiling, NULL))
            {
                model = P_GetSectorSurrounding(model, plane->destheight,
                                               def->is_ceiling);
            }
        }

        if (model)
        {
            plane->new_image = SECPIC(model, def->is_ceiling, NULL);
            plane->newspecial = model->props.special ?
                model->props.special->number : 0;

            if (plane->direction == (def->is_ceiling ? DIRECTION_DOWN : DIRECTION_UP))
            {
                SECPIC(sector, def->is_ceiling, plane->new_image);

                if (plane->newspecial != -1)
                {
					P_SectorChangeSpecial(sector, plane->newspecial);
                }
            }
        }
    }
    else if (def->tex[0])
    {
        plane->new_image = W_ImageLookup(def->tex, INS_Flat);
    }

    P_AddActivePlane(plane);

    return plane;
}


//
// BUILD A STAIRCASE!
//
// -AJA- 1999/07/04: Fixed the problem on MAP20. The next stair's
// dest height should be relative to the previous stair's dest height
// (and not just the current height).
//
// -AJA- 1999/07/29: Split into two functions. The old code could do bad
// things (e.g. skip a whole staircase) when 2 or more stair sectors
// were tagged.
//
static bool EV_BuildOneStair(sector_t * sec, const movplanedef_c * def)
{
    int i;
    float next_height;
    bool more;

    plane_move_t *step;
    sector_t *tsec;
    float stairsize = def->dest;

    const image_c *image = sec->floor.image;

    // new floor thinker

    step = P_SetupSectorAction(sec, def, sec);
    if (!step)
        return false;

    next_height = step->destheight + stairsize;

    do
    {
        more = false;

        // Find next sector to raise
        //
        // 1. Find 2-sided line with same sector side[0]
        // 2. Other side is the next sector to raise
        //
        for (i = 0; i < sec->linecount; i++)
        {
            if (! (sec->lines[i]->flags & MLF_TwoSided))
                continue;

            if (sec != sec->lines[i]->frontsector)
                continue;

            if (sec == sec->lines[i]->backsector)
                continue;

            tsec = sec->lines[i]->backsector;

            if (tsec->floor.image != image && !def->ignore_texture)
                continue;

            if (def->is_ceiling && tsec->ceil_move)
                continue;
            if (!def->is_ceiling && tsec->floor_move)
                continue;

            step = P_SetupSectorAction(tsec, def, tsec);
            if (step)
            {
                // Override the destination height
                P_SetupPlaneDirection(step, def, 
                                      step->startheight, 
                                      next_height);

                next_height += stairsize;
                sec = tsec;
                more = true;
            }

            break;
        }
    }
    while (more);

    return true;
}

static bool EV_BuildStairs(sector_t * sec, const movplanedef_c * def)
{
    bool rtn = false;

    while (sec->tag_prev)
        sec = sec->tag_prev;

    for (; sec; sec = sec->tag_next)
    {
        // Already moving?  If so, keep going...
        if (def->is_ceiling && sec->ceil_move)
            continue;
		if (!def->is_ceiling && sec->floor_move)
            continue;

        if (EV_BuildOneStair(sec, def))
            rtn = true;
    }

    return rtn;
}

//
// Do Platforms/Floors/Stairs/Ceilings/Doors/Elevators
//
bool EV_DoPlane(sector_t * sec, const movplanedef_c * def, sector_t * model)
{
    // Activate all <type> plats that are in_stasis
    switch (def->type)
    {
        case mov_Plat:
        case mov_Continuous:
        case mov_Toggle:
            if (P_ActivateInStasis(sec->tag))
                return true;
            break;

        case mov_Stairs:
            return EV_BuildStairs(sec, def);

        case mov_Stop:
            return P_StasifySector(sec->tag);

        default:
            break;
    }

    if (def->is_ceiling || def->type == mov_Elevator)
    {
        if (sec->ceil_move)
            return false;
    }

    if (!def->is_ceiling)
    {
        if (sec->floor_move)
            return false;
    }

    // Do Floor action
    return P_SetupSectorAction(sec, def, model) ? true : false;
}


bool EV_ManualPlane(line_t * line, mobj_t * thing, const movplanedef_c * def)
{
    int side = 0;  // only front sides can be used

    // if the sector has an active thinker, use it
    sector_t *sec = side ? line->frontsector : line->backsector;
    if (!sec) return false;

	plane_move_t *pmov = def->is_ceiling ? sec->ceil_move : sec->floor_move;

    if (pmov && thing)
    {
        if (def->type == mov_MoveWaitReturn)
        {
			int newdir;
			int olddir = pmov->direction;

			// only players close doors
			if ((pmov->direction != DIRECTION_DOWN) && thing->player)
				newdir = pmov->direction = DIRECTION_DOWN;
			else
				newdir = pmov->direction = DIRECTION_UP;

			if (newdir != olddir)
			{
				S_StartFX(def->sfxstart, SNCAT_Level, &sec->sfx_origin);

				pmov->sfxstarted = ! thing->player;
				return true;
			}
        }

        return false;
    }

    return EV_DoPlane(sec, def, sec);
}

static bool P_ActivateInStasis(int tag)
{
    bool result = false;

	std::vector<plane_move_t *>::iterator PMI;

	for (PMI  = active_planes.begin();
		 PMI != active_planes.end();
		 PMI++)
	{
		plane_move_t *pmov = *PMI;

		if(pmov->direction == DIRECTION_STASIS && pmov->tag == tag)
		{
			pmov->direction = pmov->olddirection;
			result = true;
		}
    }

    return result;
}

static bool P_StasifySector(int tag)
{
    bool result = false;

	std::vector<plane_move_t *>::iterator PMI;

	for (PMI  = active_planes.begin();
		 PMI != active_planes.end();
		 PMI++)
	{
		plane_move_t *pmov = *PMI;

		if(pmov->direction != DIRECTION_STASIS && pmov->tag == tag)
		{
			pmov->olddirection = pmov->direction;
			pmov->direction = DIRECTION_STASIS;

			result = true;
		}
    }

    return result;
}

bool P_SectorIsLowering(sector_t *sec)
{
	if (! sec->floor_move)
		return false;

	return sec->floor_move->direction < 0;
}

// -AJA- 1999/12/07: cleaned up this donut stuff

//
// Special Stuff that can not be categorized
// Mmmmmmm....  Donuts....
//
bool EV_DoDonut(sector_t * s1, sfx_t *sfx[4])
{
    sector_t *s2;
    sector_t *s3;
    bool result = false;
    int i;
    plane_move_t *sec;

    if (! donut_setup)
    {
        donut[0].Default();
        donut[0].count = 1;
        donut[0].f.Default(movplanedef_c::DEFAULT_DonutFloor);
        donut[0].f.tex.Set("-");

        donut[1].Default();
        donut[1].count = 1;
        donut[1].f.Default(movplanedef_c::DEFAULT_DonutFloor);
        donut[1].f.dest = -32000.0f;

        donut_setup++;
    }
  
    // ALREADY MOVING?  IF SO, KEEP GOING...
    if (s1->floor_move)
        return false;

    s2 = P_GetNextSector(s1->lines[0], s1);

    for (i = 0; i < s2->linecount; i++)
    {
        if (!(s2->lines[i]->flags & MLF_TwoSided) || (s2->lines[i]->backsector == s1))
            continue;

        s3 = s2->lines[i]->backsector;

        result = true;

        // Spawn rising slime
        donut[0].f.sfxup = sfx[0];
        donut[0].f.sfxstop = sfx[1];
    
        sec = P_SetupSectorAction(s2, &donut[0].f, s3);

        if (sec)
        {
            sec->destheight = s3->f_h;
            s2->floor.image = sec->new_image = s3->floor.image;

            /// s2->props.special = s3->props.special;
			P_SectorChangeSpecial(s2, s3->props.type);
        }

        // Spawn lowering donut-hole
        donut[1].f.sfxup = sfx[2];
        donut[1].f.sfxstop = sfx[3];

        sec = P_SetupSectorAction(s1, &donut[1].f, s1);

        if (sec)
            sec->destheight = s3->f_h;
        break;
    }

    return result;
}

static inline bool SliderCanClose(line_t *line)
{
    return ! P_ThingsOnLine(line);
}

static bool MoveSlider(slider_move_t *smov)
{
	// RETURNS true if slider_move_t should be removed.

    sector_t *sec = smov->line->frontsector;

    switch (smov->direction)
    {
        // WAITING
        case 0:
            if (--smov->waited <= 0)
            {
                if (SliderCanClose(smov->line))
                {
                    S_StartFX(smov->info->sfx_start, 
                                   SNCAT_Level,
                                   &sec->sfx_origin);

                    smov->sfxstarted = false;
                    smov->direction = DIRECTION_DOWN;
                }
                else
                {
                    // try again soon
                    smov->waited = TICRATE / 3;
                }
            }
            break;

		// OPENING
        case 1:
			MakeMovingSound(&smov->sfxstarted, smov->info->sfx_open,
                            &sec->sfx_origin);

            smov->opening += smov->info->speed;

            // mark line as non-blocking (at some point)
            P_ComputeGaps(smov->line);

            if (smov->opening >= smov->target)
            {
                S_StartFX(smov->info->sfx_stop, 
                               SNCAT_Level,
                               &sec->sfx_origin);

                smov->opening = smov->target;
                smov->direction = DIRECTION_WAIT;
                smov->waited = smov->info->wait;

                if (smov->final_open)
                {
                    line_t *ld = smov->line;

                    // clear line special
					ld->slide_door = NULL;
                    ld->special = NULL;

                    // clear the side textures
                    ld->side[0]->middle.image = NULL;
                    ld->side[1]->middle.image = NULL;

                    return true; // REMOVE ME
                }
            }
            break;

		// CLOSING
        case -1:
			MakeMovingSound(&smov->sfxstarted, smov->info->sfx_close,
                            &sec->sfx_origin);

            smov->opening -= smov->info->speed;

            // mark line as blocking (at some point)
            P_ComputeGaps(smov->line);

            if (smov->opening <= 0.0f)
            {
                S_StartFX(smov->info->sfx_stop, 
                               SNCAT_Level,
                               &sec->sfx_origin);

                return true; // REMOVE ME
            }
            break;

        default:
            I_Error("MoveSlider: Unknown direction %d", smov->direction);
    }

	return false;
}

//
// Handle thin horizontal sliding doors.
//
bool EV_DoSlider(line_t * door, line_t *act_line, mobj_t * thing,
		         const linetype_c * special)
{
	SYS_ASSERT(door);

    sector_t *sec = door->frontsector;

    if (! sec || ! door->side[0] || ! door->side[1])
        return false;

    slider_move_t *smov;

    // if the line has an active thinker, use it
    if (door->slider_move)
    {
        smov = door->slider_move;

        // only players close doors
        if (smov->direction == DIRECTION_WAIT && thing && thing->player)
        {
            smov->waited = 0;
			return true;
        }

        return false;  // nothing happened
    }

    // new sliding door thinker
    smov = new slider_move_t;

    smov->info = &special->s;
    smov->line = door;
    smov->opening = 0.0f;
    smov->line_len = R_PointToDist(0, 0, door->dx, door->dy);
    smov->target = smov->line_len * PERCENT_2_FLOAT(smov->info->distance);

    smov->direction = DIRECTION_UP;
    smov->sfxstarted = ! (thing && thing->player);
    smov->final_open = (act_line && act_line->count == 1);

	door->slide_door  = special;
    door->slider_move = smov;

	// work-around for RTS-triggered doors, which cannot setup
	// the 'slide_door' field at level load and hence the code
	// which normally blocks the door does not kick in.
	door->flags &= ~MLF_Blocking;

    P_AddActiveSlider(smov);

    //Lobo: SFX_OPEN would not play for monsters.
    //Going forward, I think it's better just to use SFX_OPEN and SFX_CLOSE
    //and quietly forget about SFX_START.

    //S_StartFX(special->s.sfx_start, SNCAT_Level, &sec->sfx_origin);
    S_StartFX(special->s.sfx_open, SNCAT_Level, &sec->sfx_origin);

	return true;
}

//
// Executes one tic's plane_move_t thinking.
// Active sectors can destroy themselves, but not each other.
//
void P_RunActivePlanes(void)
{
	std::vector<plane_move_t *> ::iterator PMI;

	bool removed_plane = false;

	for (PMI  = active_planes.begin();
		 PMI != active_planes.end();
		 PMI++)
	{
		plane_move_t *pmov = *PMI;

		if (MovePlane(pmov))
		{
            if (pmov->is_ceiling || pmov->is_elevator)
                pmov->sector->ceil_move = NULL;
            
            if (!pmov->is_ceiling)
                pmov->sector->floor_move = NULL;

			*PMI = NULL;
			delete pmov;

		    removed_plane = true;
		}
	}

	if (removed_plane)
	{
		std::vector<plane_move_t *>::iterator ENDP;

		ENDP = std::remove(active_planes.begin(), 
		                   active_planes.end(),
				   (plane_move_t *) NULL);

		active_planes.erase(ENDP, active_planes.end());
	}
}

void P_RunActiveSliders(void)
{
	std::vector<slider_move_t *>::iterator SMI;

	bool removed_slider = false;

	for (SMI  = active_sliders.begin();
		 SMI != active_sliders.end();
		 SMI++)
	{
		slider_move_t *smov = *SMI;

		if (MoveSlider(smov))
		{
            smov->line->slider_move = NULL;

			*SMI = NULL;
			delete smov;

		    removed_slider = true;
		}
	}

	if (removed_slider)
	{
		std::vector<slider_move_t *>::iterator ENDP;

		ENDP = std::remove(active_sliders.begin(), 
		                   active_sliders.end(),
				   (slider_move_t *) NULL);

		active_sliders.erase(ENDP, active_sliders.end());
	}
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
