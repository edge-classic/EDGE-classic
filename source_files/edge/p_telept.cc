//----------------------------------------------------------------------------
//  EDGE Teleport Code
//----------------------------------------------------------------------------
//
//  Copyright (c) 1999-2024 The EDGE Team.
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
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

#include "con_main.h"
#include "dm_defs.h"
#include "dm_state.h"
#include "m_random.h"
#include "p_local.h"
#include "r_misc.h"
#include "r_sky.h"
#include "r_state.h"
#include "s_sound.h"

static constexpr float kTeleportFudge = 0.1f;

MapObject *FindTeleportMan(int tag, const MapObjectDefinition *info)
{
    for (int i = 0; i < total_level_sectors; i++)
    {
        if (level_sectors[i].tag != tag)
            continue;

        for (Subsector *sub = level_sectors[i].subsectors; sub; sub = sub->sector_next)
        {
            for (MapObject *mo = sub->thing_list; mo; mo = mo->subsector_next_)
                if (mo->info_ == info && !(mo->extended_flags_ & kExtendedFlagNeverTarget))
                    return mo;
        }
    }

    return nullptr; // not found
}

Line *FindTeleportLine(int tag, Line *original)
{
    for (int i = 0; i < total_level_lines; i++)
    {
        if (level_lines[i].tag != tag)
            continue;

        if (level_lines + i == original)
            continue;

        return level_lines + i;
    }

    return nullptr; // not found
}

//
// TeleportMapObject
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
// -ACB- 1998/09/13 used effect objects: the objects themselves make any sound
// and
//                  the in effect object can be different to the out object.
//
// -ACB- 1998/09/13 Removed the missile checks: no need since this would have
// been
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
// -AJA- 1999/10/21: Allow line to be nullptr, and added `tag' param.
// -AJA- 2004/10/08: Reworked for Silent and Line-to-Line teleporters
//                   (based on the logic in prBoom's p_telept.c code).
//
bool TeleportMapObject(Line *line, int tag, MapObject *thing, const TeleportDefinition *def)
{
    if (!thing)
        return false;

    float oldx = thing->x;
    float oldy = thing->y;
    float oldz = thing->z;

    float new_x;
    float new_y;
    float new_z;

    BAMAngle new_ang;

    BAMAngle dest_ang;
    BAMAngle source_ang = kBAMAngle90 + (line ? PointToAngle(0, 0, line->delta_x, line->delta_y) : 0);

    MapObject *currmobj = nullptr;
    Line      *currline = nullptr;

    bool flipped = (def->special_ & kTeleportSpecialFlipped) ? true : false;

    Player *player = thing->player_;
    if (player && player->map_object_ != thing) // exclude voodoo dolls
        player = nullptr;

    if (def->special_ & kTeleportSpecialLine)
    {
        if (!line || tag <= 0)
            return false;

        currline = FindTeleportLine(tag, line);

        if (!currline)
            return false;

        new_x = currline->vertex_1->X + currline->delta_x / 2.0f;
        new_y = currline->vertex_1->Y + currline->delta_y / 2.0f;

        new_z = currline->front_sector ? currline->front_sector->floor_height : -32000;

        if (currline->back_sector)
            new_z = HMM_MAX(new_z, currline->back_sector->floor_height);

        dest_ang = PointToAngle(0, 0, currline->delta_x, currline->delta_y) + kBAMAngle90;

        flipped = !flipped; // match Boom's logic
    }
    else                    /* thing-based teleport */
    {
        if (!def->outspawnobj_)
            return false;

        currmobj = FindTeleportMan(tag, def->outspawnobj_);

        if (!currmobj)
            return false;

        new_x = currmobj->x;
        new_y = currmobj->y;
        new_z = currmobj->z;

        dest_ang = currmobj->angle_;
    }

    /* --- Angle handling --- */

    if (flipped)
        dest_ang += kBAMAngle180;

    if (def->special_ & kTeleportSpecialRelative && currline)
        new_ang = thing->angle_ + (dest_ang - source_ang);
    else if (def->special_ & kTeleportSpecialSameAbsDir)
        new_ang = thing->angle_;
    else if (def->special_ & kTeleportSpecialRotate)
        new_ang = thing->angle_ + dest_ang;
    else
        new_ang = dest_ang;

    /* --- Offset handling --- */

    if (line && def->special_ & kTeleportSpecialSameOffset)
    {
        float dx = 0;
        float dy = 0;

        float pos = 0;

        if (fabs(line->delta_x) > fabs(line->delta_y))
            pos = (oldx - line->vertex_1->X) / line->delta_x;
        else
            pos = (oldy - line->vertex_1->Y) / line->delta_y;

        if (currline)
        {
            dx = currline->delta_x * (pos - 0.5f);
            dy = currline->delta_y * (pos - 0.5f);

            if (flipped)
            {
                dx = -dx;
                dy = -dy;
            }

            new_x += dx;
            new_y += dy;

            // move a little distance away from the line, in case that line
            // is special (e.g. another teleporter), in order to prevent it
            // from being triggered.

            new_x += kTeleportFudge * epi::BAMCos(dest_ang);
            new_y += kTeleportFudge * epi::BAMSin(dest_ang);
        }
        else if (currmobj)
        {
            dx = line->delta_x * (pos - 0.5f);
            dy = line->delta_y * (pos - 0.5f);

            // we need to rotate the offset vector
            BAMAngle offset_ang = dest_ang - source_ang;

            float s = epi::BAMSin(offset_ang);
            float c = epi::BAMCos(offset_ang);

            new_x += dx * c - dy * s;
            new_y += dy * c + dx * s;
        }
    }

    if (def->special_ & kTeleportSpecialSameHeight)
    {
        new_z += (thing->z - thing->floor_z_);
    }
    else if (thing->flags_ & kMapObjectFlagMissile)
    {
        new_z += thing->original_height_;
    }

    if (!TeleportMove(thing, new_x, new_y, new_z))
        return false;

    if (player)
    {
        player->view_height_       = player->standard_view_height_;
        player->view_z_            = player->standard_view_height_;
        player->delta_view_height_ = 0;
    }
    else
    {        
        thing->old_x_ = thing->old_y_ = thing->old_z_ = kInvalidPosition;
    }

    /* --- Momentum handling --- */

    if (thing->flags_ & kMapObjectFlagMissile)
    {
        thing->momentum_.X = thing->speed_ * epi::BAMCos(new_ang);
        thing->momentum_.Y = thing->speed_ * epi::BAMSin(new_ang);
    }
    else if (def->special_ & kTeleportSpecialSameSpeed)
    {
        // we need to rotate the momentum vector
        BAMAngle mom_ang = new_ang - thing->angle_;

        float s = epi::BAMSin(mom_ang);
        float c = epi::BAMCos(mom_ang);

        float mx = thing->momentum_.X;
        float my = thing->momentum_.Y;

        thing->momentum_.X = mx * c - my * s;
        thing->momentum_.Y = my * c + mx * s;
    }
    else if (player)
    {
        // don't move for a bit
        thing->reaction_time_ = def->delay_;

        thing->momentum_.X = thing->momentum_.Y = thing->momentum_.Z = 0;

        player->actual_speed_ = 0;
    }

    thing->angle_ = new_ang;

    if (currmobj &&
        0 == (def->special_ & (kTeleportSpecialRelative | kTeleportSpecialSameAbsDir | kTeleportSpecialRotate)))
    {
        thing->vertical_angle_ = currmobj->vertical_angle_;
    }

    /* --- Spawning teleport fog (source and/or dest) --- */

    if (!(def->special_ & kTeleportSpecialSilent))
    {
        MapObject *fog;

        if (def->inspawnobj_)
        {
            fog = CreateMapObject(oldx, oldy, oldz, def->inspawnobj_);

            // never use this object as a teleport destination
            fog->extended_flags_ |= kExtendedFlagNeverTarget;

            if (fog->info_->chase_state_)
                MapObjectSetStateDeferred(fog, fog->info_->chase_state_, 0);
        }

        if (def->outspawnobj_)
        {
            //
            // -ACB- 1998/09/06 Switched 40 to 20. This by my records is
            //                  the original setting.
            //
            // -ES- 1998/10/29 When fading, we don't want to see the fog.
            //
            fog = CreateMapObject(new_x + 20.0f * epi::BAMCos(thing->angle_),
                                  new_y + 20.0f * epi::BAMSin(thing->angle_), new_z, def->outspawnobj_);

            // never use this object as a teleport destination
            fog->extended_flags_ |= kExtendedFlagNeverTarget;

            if (fog->info_->chase_state_)
                MapObjectSetStateDeferred(fog, fog->info_->chase_state_, 0);

            if (player == players[display_player] && reduce_flash)
            {
                fog->target_visibility_ = fog->visibility_ = 0.0f;
                ImportantConsoleMessageLDF("Teleporting...");
            }
        }
    }

    return true;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
