//----------------------------------------------------------------------------
//  EDGE Teleport Code
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


#define TELE_FUDGE  0.1f

static mobj_t *FindTeleportMan(int tag, const mobjtype_c *info)
{
    for (int i = 0; i < numsectors; i++)
    {
        if (sectors[i].tag != tag)
            continue;

        for (subsector_t *sub = sectors[i].subsectors; sub; sub = sub->sec_next)
        {
            for (mobj_t *mo = sub->thinglist; mo; mo = mo->snext)
                if (mo->info == info &&
 				    ! (mo->extendedflags & EF_NEVERTARGET))
                    return mo;
        }
    }

    return NULL;  // not found
}

static line_t *FindTeleportLine(int tag, line_t *original)
{
    for (int i = 0; i < numlines; i++)
    {
        if (lines[i].tag != tag)
            continue;

        if (lines + i == original)
            continue;

        return lines + i;
    }

    return NULL;  // not found
}

//
// EV_Teleport
//
// Teleportation is an effect which is simulated by searching for the first
// special[MOBJ_TELEPOS] in a sector with the same tag as the activation line,
// moving an object from one sector to another upon the MOBJ_TELEPOS found, and
// possibly spawning an effect object (i.e teleport flash) at either the entry &
// exit points or both.
//
// -KM- 1998/09/01 Added stuff for lines.ddf (mostly sounds)
//
// -ACB- 1998/09/11 Reformatted and cleaned up.
//
// -ACB- 1998/09/12 Teleport delay setting from linedef.
//
// -ACB- 1998/09/13 used effect objects: the objects themselves make any sound and
//                  the in effect object can be different to the out object.
//
// -ACB- 1998/09/13 Removed the missile checks: no need since this would have been
//                  Checked at the linedef stage.
//
// -KM- 1998/11/25 Changed Erik's code a bit, Teleport flash still appears.
//  if def faded_teleportation == 1, doesn't if faded_teleportation == 2
//
// -ES- 1998/11/28 Changed Kester's code a bit :-) Teleport method can now be
//  toggled in the menu. (That is the way it should be. -KM)
//
// -KM- 1999/01/31 Search only the target sector, not the entire map.
//
// -AJA- 1999/07/12: Support for TELEPORT_SPECIAL in lines.ddf.
// -AJA- 1999/07/30: Updated for extra floor support.
// -AJA- 1999/10/21: Allow line to be NULL, and added `tag' param.
// -AJA- 2004/10/08: Reworked for Silent and Line-to-Line teleporters
//                   (based on the logic in prBoom's p_telept.c code).
//
bool EV_Teleport(line_t* line, int tag, mobj_t* thing,
                 const teleportdef_c *def)
{
    if (!thing)
        return false;

    float oldx = thing->x;
    float oldy = thing->y;
    float oldz = thing->z;

    float new_x;
    float new_y;
    float new_z;

    angle_t new_ang;

    angle_t dest_ang;
    angle_t source_ang = ANG90 + (line ? R_PointToAngle(0, 0, line->dx, line->dy) : 0);

    mobj_t *currmobj = NULL;
    line_t *currline = NULL;

	bool flipped = (def->special & TELSP_Flipped) ? true : false;

	player_t *player = thing->player;
	if (player && player->mo != thing)  // exclude voodoo dolls
		player = NULL;

    if (def->special & TELSP_Line)
    {
        if (!line || tag <= 0)
            return false;

        currline = FindTeleportLine(tag, line);

        if (! currline)
            return false;

        new_x = currline->v1->x + currline->dx / 2.0f;
        new_y = currline->v1->y + currline->dy / 2.0f;

        new_z = currline->frontsector ? currline->frontsector->f_h : -32000;

        if (currline->backsector)
            new_z = MAX(new_z, currline->backsector->f_h);

        dest_ang = R_PointToAngle(0, 0, currline->dx, currline->dy) + ANG90;

		flipped = ! flipped;  // match Boom's logic
    }
    else  /* thing-based teleport */
    {
        if (! def->outspawnobj)
            return false;

        currmobj = FindTeleportMan(tag, def->outspawnobj);

        if (! currmobj)
            return false;

        new_x = currmobj->x;
        new_y = currmobj->y;
        new_z = currmobj->z;

        dest_ang = currmobj->angle;
    }

    /* --- Angle handling --- */

    if (flipped)
        dest_ang += ANG180;

    if (def->special & TELSP_Relative)
        new_ang = thing->angle + (dest_ang - source_ang);
    else if (def->special & TELSP_SameAbsDir)
        new_ang = thing->angle;
    else if (def->special & TELSP_Rotate)
        new_ang = thing->angle + dest_ang;
    else
        new_ang = dest_ang;

    /* --- Offset handling --- */

    if (line && def->special & TELSP_SameOffset)
    {
        float dx = 0;
        float dy = 0;

        float pos = 0;

        if (fabs(line->dx) > fabs(line->dy))
            pos = (oldx - line->v1->x) / line->dx;
        else
            pos = (oldy - line->v1->y) / line->dy;

        if (currline)
        {
            dx = currline->dx * (pos - 0.5f);
            dy = currline->dy * (pos - 0.5f);

            if (flipped)
            {
                dx = -dx; dy = -dy;
            }

            new_x += dx;
            new_y += dy;

            // move a little distance away from the line, in case that line
            // is special (e.g. another teleporter), in order to prevent it
            // from being triggered.

            new_x += TELE_FUDGE * M_Cos(dest_ang);
            new_y += TELE_FUDGE * M_Sin(dest_ang);
        }
        else if (currmobj)
        {
            dx = line->dx * (pos - 0.5f);
            dy = line->dy * (pos - 0.5f);

            // we need to rotate the offset vector
            angle_t offset_ang = dest_ang - source_ang;

            float s = M_Sin(offset_ang);
            float c = M_Cos(offset_ang);

            new_x += dx * c - dy * s;
            new_y += dy * c + dx * s;
        }
    }

    if (def->special & TELSP_SameHeight)
    {
        new_z += (thing->z - thing->floorz);
    }
    else if (thing->flags & MF_MISSILE)
    {
        new_z += thing->origheight;
    }

    if (!P_TeleportMove(thing, new_x, new_y, new_z))
        return false;
    
    if (player)
	{
		player->viewheight = player->std_viewheight;
        player->viewz      = player->std_viewheight;
		player->deltaviewheight = 0;
	}

    /* --- Momentum handling --- */

    if (thing->flags & MF_MISSILE)
    {
        thing->mom.x = thing->speed * M_Cos(new_ang);
        thing->mom.y = thing->speed * M_Sin(new_ang);
    }
    else if (def->special & TELSP_SameSpeed)
    {
        // we need to rotate the momentum vector
        angle_t mom_ang = new_ang - thing->angle;

        float s = M_Sin(mom_ang);
        float c = M_Cos(mom_ang);

        float mx = thing->mom.x;
        float my = thing->mom.y;

        thing->mom.x = mx * c - my * s;
        thing->mom.y = my * c + mx * s;
    }
	else if (player)
    {
        // don't move for a bit
        thing->reactiontime = def->delay;

        // -ES- 1998/10/29 Start the fading
        if (telept_effect && player == players[displayplayer])
            R_StartFading(0, (def->delay * 5) / 2);

        thing->mom.x = thing->mom.y = thing->mom.z = 0;

		player->actual_speed = 0;
    }

    thing->angle = new_ang;

    if (currmobj && 0 == (def->special & (TELSP_Relative | TELSP_SameAbsDir |
                                          TELSP_Rotate)))
    {
        thing->vertangle = currmobj->vertangle;
    }

    /* --- Spawning teleport fog (source and/or dest) --- */

    if (! (def->special & TELSP_Silent))
    {
        mobj_t *fog;

        if (def->inspawnobj)
        {
            fog = P_MobjCreateObject(oldx, oldy, oldz, def->inspawnobj);

			// never use this object as a teleport destination
			fog->extendedflags |= EF_NEVERTARGET;

            if (fog->info->chase_state)
                P_SetMobjStateDeferred(fog, fog->info->chase_state, 0);
        }

        if (def->outspawnobj)
        {
            //
            // -ACB- 1998/09/06 Switched 40 to 20. This by my records is
            //                  the original setting.
            //
            // -ES- 1998/10/29 When fading, we don't want to see the fog.
            //
            fog = P_MobjCreateObject(new_x + 20.0f * M_Cos(thing->angle),
                                     new_y + 20.0f * M_Sin(thing->angle),
                                     new_z, def->outspawnobj);

			// never use this object as a teleport destination
			fog->extendedflags |= EF_NEVERTARGET;

            if (fog->info->chase_state)
                P_SetMobjStateDeferred(fog, fog->info->chase_state, 0);

            if (player == players[displayplayer] && !telept_flash)
                fog->vis_target = fog->visibility = INVISIBLE;
        }
    }

    return true;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
