//----------------------------------------------------------------------------
//  EDGE Floor/Ceiling/Stair/Elevator Action Code
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



#include "dm_defs.h"
#include "dm_state.h"
#include "m_random.h"
#include "n_network.h"
#include "p_local.h"
#include "r_sky.h"
#include "r_misc.h"
#include "r_state.h"
#include "s_sound.h"

#include "AlmostEquals.h"
#include "str_compare.h"
#include <algorithm>

typedef enum
{
    RES_Ok,
    RES_Crushed,
    RES_PastDest,
    RES_Impossible
} move_result_e;

std::vector<plane_move_t *>  active_planes;
std::vector<slider_move_t *> active_sliders;

extern std::vector<secanim_t>  secanims;
extern std::vector<lineanim_t> lineanims;

LineType donut[2];
static int donut_setup = 0;

extern ConsoleVariable double_framerate;

static bool P_ActivateInStasis(int tag);
static bool P_StasifySector(int tag);

// -AJA- Perhaps using a pointer to `plane_info_t' would be better
//       than f**king about with the floorOrCeiling stuff all the
//       time.

static float HEIGHT(sector_t *sec, bool is_ceiling)
{
    if (is_ceiling)
        return sec->c_h;

    return sec->f_h;
}

static const image_c *SECPIC(sector_t *sec, bool is_ceiling, const image_c *new_image)
{
    if (new_image)
    {
        if (is_ceiling)
            sec->ceil.image = new_image;
        else
        {
            sec->floor.image = new_image;
            if (new_image)
            {
                FlatDefinition *current_flatdef = flatdefs.Find(new_image->name.c_str());
                if (current_flatdef)
                {
                    sec->bob_depth  = current_flatdef->bob_depth_;
                    sec->sink_depth = current_flatdef->sink_depth_;
                }
                else
                    sec->bob_depth = 0;
                sec->sink_depth = 0;
            }
            else
            {
                sec->bob_depth  = 0;
                sec->sink_depth = 0;
            }
        }

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
static float GetSecHeightReference(TriggerHeightReference ref, sector_t *sec, sector_t *model)
{
    switch (ref & kTriggerHeightReferenceMask)
    {
    case kTriggerHeightReferenceAbsolute:
        return 0;

    case kTriggerHeightReferenceTriggeringLinedef:
        if (model)
            return (ref & kTriggerHeightReferenceCeiling) ? model->c_h : model->f_h;

        return 0; // ick!

    case kTriggerHeightReferenceCurrent:
        return (ref & kTriggerHeightReferenceCeiling) ? sec->c_h : sec->f_h;

    case kTriggerHeightReferenceSurrounding:
        return P_FindSurroundingHeight(ref, sec);

    case kTriggerHeightReferenceLowestLowTexture:
        return P_FindRaiseToTexture(sec);

    default:
        FatalError("GetSecHeightReference: undefined reference %d\n", ref);
    }

    return 0;
}

#define RELOOP_TICKS 6

static void MakeMovingSound(bool *started_var, SoundEffect *sfx, position_c *pos)
{
    if (!sfx || sfx->num < 1)
        return;

    SoundEffectDefinition *def = sfxdefs[sfx->sounds[0]];

    // looping sounds need to be "pumped" to keep looping.
    // The main one is STNMOV, which lasts a little over 0.25 seconds,
    // hence we need to pump it every 6 tics or so.

    if (!*started_var || (def->looping_ && (leveltime % RELOOP_TICKS) == 0))
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
    std::vector<plane_move_t *>::iterator PMI;

    for (PMI = active_planes.begin(); PMI != active_planes.end(); PMI++)
    {
        delete (*PMI);
    }

    active_planes.clear();
}

void P_DestroyAllSliders(void)
{
    std::vector<slider_move_t *>::iterator SMI;

    for (SMI = active_sliders.begin(); SMI != active_sliders.end(); SMI++)
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
static move_result_e AttemptMovePlane(sector_t *sector, float speed, float dest, int crush, bool is_ceiling,
                                      int direction)
{
    bool past = false;
    bool nofit;

    if (double_framerate.d_)
        speed *= 0.5f;

    //
    // check whether we have gone past the destination height
    //
    if (direction == DIRECTION_UP && HEIGHT(sector, is_ceiling) + speed > dest)
    {
        past  = true;
        speed = dest - HEIGHT(sector, is_ceiling);
    }
    else if (direction == DIRECTION_DOWN && HEIGHT(sector, is_ceiling) - speed < dest)
    {
        past  = true;
        speed = HEIGHT(sector, is_ceiling) - dest;
    }

    if (speed <= 0)
        return RES_PastDest;

    if (direction == DIRECTION_DOWN)
        speed = -speed;

    // check if even possible
    if (!P_CheckSolidSectorMove(sector, is_ceiling, speed))
    {
        return RES_Impossible;
    }

    //
    // move the actual sector, including all things in it
    //
    nofit = P_SolidSectorMove(sector, is_ceiling, speed, crush, false);

    if (!nofit)
        return past ? RES_PastDest : RES_Ok;

    // bugger, something got in our way !

    if (crush == 0)
    {
        // undo the change
        P_SolidSectorMove(sector, is_ceiling, -speed, false, false);
    }

    return past ? RES_PastDest : RES_Crushed;
}

static move_result_e AttemptMoveSector(sector_t *sector, plane_move_t *pmov, float dest, int crush)
{
    move_result_e res;

    if (!pmov->is_elevator)
    {
        return AttemptMovePlane(sector, pmov->speed, dest, crush, pmov->is_ceiling, pmov->direction);
    }

    //-------------------//
    //   ELEVATOR MOVE   //
    //-------------------//

    if (pmov->direction == DIRECTION_UP)
    {
        AttemptMovePlane(sector, 32768.0, HMM_MIN(sector->f_h + pmov->speed, dest) + pmov->elev_height, false, true,
                         DIRECTION_UP);
    }

    res = AttemptMovePlane(sector, pmov->speed, dest, crush, false, pmov->direction);

    if (pmov->direction == DIRECTION_DOWN)
    {
        AttemptMovePlane(sector, 32768.0, sector->f_h + pmov->elev_height, false, true, DIRECTION_DOWN);
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
        res = AttemptMoveSector(plane->sector, plane, HMM_MIN(plane->startheight, plane->destheight),
                                plane->is_ceiling ? plane->crush : 0);

        if (!AlmostEquals(plane->destheight, plane->startheight))
        {
            MakeMovingSound(&plane->sfxstarted, plane->type->sfxdown_, &plane->sector->sfx_origin);
        }

        if (res == RES_PastDest)
        {
            if (!AlmostEquals(plane->destheight, plane->startheight))
            {
                S_StartFX(plane->type->sfxstop_, SNCAT_Level, &plane->sector->sfx_origin);
            }

            plane->speed = plane->type->speed_up_;

            if (plane->newspecial != -1)
            {
                P_SectorChangeSpecial(plane->sector, plane->newspecial);
            }

            SECPIC(plane->sector, plane->is_ceiling, plane->new_image);

            switch (plane->type->type_)
            {
            case kPlaneMoverPlatform:
            case kPlaneMoverContinuous:
                plane->direction = DIRECTION_WAIT;
                plane->waited    = plane->type->wait_;
                plane->speed     = plane->type->speed_up_;
                break;

            case kPlaneMoverMoveWaitReturn:
                if (AlmostEquals(HEIGHT(plane->sector, plane->is_ceiling), plane->startheight))
                {
                    return true; // REMOVE ME
                }
                else // assume we reached the destination
                {
                    plane->direction = DIRECTION_WAIT;
                    plane->waited    = plane->type->wait_;
                    plane->speed     = plane->type->speed_up_;
                }
                break;

            case kPlaneMoverToggle:
                plane->direction    = DIRECTION_STASIS;
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
                plane->speed = plane->type->speed_down_;

                if (plane->speed < 1.5f)
                    plane->speed = plane->speed / 8.0f;
            }
            else if (plane->type->type_ == kPlaneMoverMoveWaitReturn) // Go back up
            {
                plane->direction  = DIRECTION_UP;
                plane->sfxstarted = false;
                plane->waited     = 0;
                plane->speed      = plane->type->speed_up_;
            }
        }

        break;

    case DIRECTION_WAIT:
        plane->waited -= (!double_framerate.d_|| !(game_tic & 1)) ? 1 : 0;
        if (plane->waited <= 0)
        {
            int   dir;
            float dest;

            if (AlmostEquals(HEIGHT(plane->sector, plane->is_ceiling), plane->destheight))
                dest = plane->startheight;
            else
                dest = plane->destheight;

            if (HEIGHT(plane->sector, plane->is_ceiling) > dest)
            {
                dir          = DIRECTION_DOWN;
                plane->speed = plane->type->speed_down_;
            }
            else
            {
                dir          = DIRECTION_UP;
                plane->speed = plane->type->speed_up_;
            }

            if (dir)
            {
                S_StartFX(plane->type->sfxstart_, SNCAT_Level, &plane->sector->sfx_origin);
            }

            plane->direction  = dir; // time to go back
            plane->sfxstarted = false;
        }
        break;

    case DIRECTION_UP:
        res = AttemptMoveSector(plane->sector, plane, HMM_MAX(plane->startheight, plane->destheight),
                                plane->is_ceiling ? 0 : plane->crush);

        if (!AlmostEquals(plane->destheight, plane->startheight))
        {
            MakeMovingSound(&plane->sfxstarted, plane->type->sfxup_, &plane->sector->sfx_origin);
        }

        if (res == RES_PastDest)
        {
            if (!AlmostEquals(plane->destheight, plane->startheight))
            {
                S_StartFX(plane->type->sfxstop_, SNCAT_Level, &plane->sector->sfx_origin);
            }

            if (plane->newspecial != -1)
            {
                P_SectorChangeSpecial(plane->sector, plane->newspecial);
            }

            SECPIC(plane->sector, plane->is_ceiling, plane->new_image);

            switch (plane->type->type_)
            {
            case kPlaneMoverPlatform:
            case kPlaneMoverContinuous:
                plane->direction = DIRECTION_WAIT;
                plane->waited    = plane->type->wait_;
                plane->speed     = plane->type->speed_down_;
                break;

            case kPlaneMoverMoveWaitReturn:
                if (AlmostEquals(HEIGHT(plane->sector, plane->is_ceiling), plane->startheight))
                {
                    return true; // REMOVE ME
                }
                else // assume we reached the destination
                {
                    plane->direction = DIRECTION_WAIT;
                    plane->speed     = plane->type->speed_down_;
                    plane->waited    = plane->type->wait_;
                }
                break;

            case kPlaneMoverToggle:
                plane->direction    = DIRECTION_STASIS;
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
                plane->speed = plane->type->speed_up_;

                if (plane->speed < 1.5f)
                    plane->speed = plane->speed / 8.0f;
            }
            else if (plane->type->type_ == kPlaneMoverMoveWaitReturn) // Go back down
            {
                plane->direction  = DIRECTION_DOWN;
                plane->sfxstarted = false;
                plane->waited     = 0;
                plane->speed      = plane->type->speed_down_;
            }
        }
        break;

    default:
        FatalError("MovePlane: Unknown direction %d", plane->direction);
    }

    return false;
}

static sector_t *P_GSS(sector_t *sec, float dest, bool forc)
{
    int       i;
    int       secnum = sec - sectors;
    sector_t *sector;

    // 2023.06.10 - Reversed the order of iteration because it was returning
    // the greatest numbered linedef for applicable surrounding sectors instead
    // of the least.

    for (i = 0; i < sec->linecount - 1; i++)
    {
        if (P_TwoSided(secnum, i))
        {
            if (P_GetSide(secnum, i, 0)->sector - sectors == secnum)
            {
                sector = P_GetSector(secnum, i, 1);

                if (SECPIC(sector, forc, nullptr) != SECPIC(sec, forc, nullptr) && AlmostEquals(HEIGHT(sector, forc), dest))
                {
                    return sector;
                }
            }
            else
            {
                sector = P_GetSector(secnum, i, 0);

                if (SECPIC(sector, forc, nullptr) != SECPIC(sec, forc, nullptr) && AlmostEquals(HEIGHT(sector, forc), dest))
                {
                    return sector;
                }
            }
        }
    }

    for (i = 0; i < sec->linecount - 1; i++)
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
                sector             = P_GSS(sector, dest, forc);
                if (sector)
                    return sector;
            }
        }
    }

    return nullptr;
}

static sector_t *P_GetSectorSurrounding(sector_t *sec, float dest, bool forc)
{
    validcount++;
    sec->validcount = validcount;
    return P_GSS(sec, dest, forc);
}

void P_SetupPlaneDirection(plane_move_t *plane, const PlaneMoverDefinition *def, float start, float dest)
{
    plane->startheight = start;
    plane->destheight  = dest;

    if (dest > start)
    {
        plane->direction = DIRECTION_UP;

        if (def->speed_up_ >= 0)
            plane->speed = def->speed_up_;
        else
            plane->speed = dest - start;
    }
    else if (start > dest)
    {
        plane->direction = DIRECTION_DOWN;

        if (def->speed_down_ >= 0)
            plane->speed = def->speed_down_;
        else
            plane->speed = start - dest;
    }

    return;
}

//
// Setup the Floor Action, depending on the linedeftype trigger and the
// sector info.
//
static plane_move_t *P_SetupSectorAction(sector_t *sector, const PlaneMoverDefinition *def, sector_t *model)
{
    // new door thinker
    plane_move_t *plane = new plane_move_t;

    if (def->is_ceiling_)
        sector->ceil_move = plane;
    else
        sector->floor_move = plane;

    plane->sector     = sector;
    plane->crush      = def->crush_damage_;
    plane->sfxstarted = false;

    float start = HEIGHT(sector, def->is_ceiling_);

    float dest = GetSecHeightReference(def->destref_, sector, model);
    dest += def->dest_;

    if (def->type_ == kPlaneMoverPlatform || def->type_ == kPlaneMoverContinuous || def->type_ == kPlaneMoverToggle)
    {
        start = GetSecHeightReference(def->otherref_, sector, model);
        start += def->other_;
    }

#if 0 // DEBUG
    LogDebug("SEC_ACT: %d type %d %s start %1.0f dest %1.0f\n",
                 sector - sectors, def->type, 
                 def->is_ceiling_ ? "CEIL" : "FLOOR", 
                 start, dest);
#endif

    if (def->prewait_)
    {
        plane->direction   = DIRECTION_WAIT;
        plane->waited      = def->prewait_;
        plane->destheight  = dest;
        plane->startheight = start;
    }
    else if (def->type_ == kPlaneMoverContinuous)
    {
        plane->direction = (RandomByteDeterministic() & 1) ? DIRECTION_UP : DIRECTION_DOWN;

        if (plane->direction == DIRECTION_UP)
            plane->speed = def->speed_up_;
        else
            plane->speed = def->speed_down_;

        plane->destheight  = dest;
        plane->startheight = start;
    }
    else if (!AlmostEquals(start, dest))
    {
        P_SetupPlaneDirection(plane, def, start, dest);
    }
    else
    {
        // 2023.08.01 - If already at dest height, run the
        // texture/type changes that were intended

        // change to surrounding
        if (def->tex_ != "" && def->tex_[0] == '-')
        {
            model = P_GetSectorSurrounding(sector, plane->destheight, def->is_ceiling_);
            if (model)
            {
                if (def->tex_.size() == 1) // Only '-'; do both (default)
                {
                    plane->new_image  = SECPIC(model, def->is_ceiling_, nullptr);
                    plane->newspecial = model->props.special ? model->props.special->number_ : 0;
                }
                else if (epi::StringCaseCompareASCII(def->tex_.substr(1), "changezero") == 0)
                {
                    plane->new_image  = SECPIC(model, def->is_ceiling_, nullptr);
                    plane->newspecial = 0;
                }
                else if (epi::StringCaseCompareASCII(def->tex_.substr(1), "changetexonly") == 0)
                {
                    plane->new_image = SECPIC(model, def->is_ceiling_, nullptr);
                }
                else // Unknown directive after '-'; just do default
                {
                    plane->new_image  = SECPIC(model, def->is_ceiling_, nullptr);
                    plane->newspecial = model->props.special ? model->props.special->number_ : 0;
                }
                SECPIC(sector, def->is_ceiling_, plane->new_image);
                if (plane->newspecial != -1)
                {
                    P_SectorChangeSpecial(sector, plane->newspecial);
                }
            }
        }
        else if (def->tex_ != "" && def->tex_[0] == '+')
        {
            if (model)
            {
                if (SECPIC(model, def->is_ceiling_, nullptr) == SECPIC(sector, def->is_ceiling_, nullptr))
                {
                    model = P_GetSectorSurrounding(model, plane->destheight, def->is_ceiling_);
                }
            }

            if (model)
            {
                if (def->tex_.size() == 1) // Only '+'; do both (default)
                {
                    plane->new_image  = SECPIC(model, def->is_ceiling_, nullptr);
                    plane->newspecial = model->props.special ? model->props.special->number_ : 0;
                }
                else if (epi::StringCaseCompareASCII(def->tex_.substr(1), "changezero") == 0)
                {
                    plane->new_image  = SECPIC(model, def->is_ceiling_, nullptr);
                    plane->newspecial = 0;
                }
                else if (epi::StringCaseCompareASCII(def->tex_.substr(1), "changetexonly") == 0)
                {
                    plane->new_image = SECPIC(model, def->is_ceiling_, nullptr);
                }
                else // Unknown directive after '+'; just do default
                {
                    plane->new_image  = SECPIC(model, def->is_ceiling_, nullptr);
                    plane->newspecial = model->props.special ? model->props.special->number_ : 0;
                }

                SECPIC(sector, def->is_ceiling_, plane->new_image);

                if (plane->newspecial != -1)
                {
                    P_SectorChangeSpecial(sector, plane->newspecial);
                }
            }
        }
        else if (def->tex_ != "")
        {
            plane->new_image = W_ImageLookup(def->tex_.c_str(), kImageNamespaceFlat);
            SECPIC(sector, def->is_ceiling_, plane->new_image);
        }

        if (def->is_ceiling_)
            sector->ceil_move = nullptr;
        else
            sector->floor_move = nullptr;

        plane->nukeme = true;

        return plane;
    }

    plane->tag         = sector->tag;
    plane->type        = def;
    plane->new_image   = SECPIC(sector, def->is_ceiling_, nullptr);
    plane->newspecial  = -1;
    plane->is_ceiling  = def->is_ceiling_;
    plane->is_elevator = (def->type_ == kPlaneMoverElevator);
    plane->elev_height = sector->c_h - sector->f_h;

    // -ACB- 10/01/2001 Trigger starting sfx
    // UNNEEDED    sound::StopLoopingFX(&sector->sfx_origin);

    if (def->sfxstart_ && !AlmostEquals(plane->destheight, plane->startheight))
    {
        S_StartFX(def->sfxstart_, SNCAT_Level, &sector->sfx_origin);
    }

    // change to surrounding
    if (def->tex_ != "" && def->tex_[0] == '-')
    {
        model = P_GetSectorSurrounding(sector, plane->destheight, def->is_ceiling_);
        if (model)
        {
            if (def->tex_.size() == 1) // Only '-'; do both (default)
            {
                plane->new_image  = SECPIC(model, def->is_ceiling_, nullptr);
                plane->newspecial = model->props.special ? model->props.special->number_ : 0;
            }
            else if (epi::StringCaseCompareASCII(def->tex_.substr(1), "changezero") == 0)
            {
                plane->new_image  = SECPIC(model, def->is_ceiling_, nullptr);
                plane->newspecial = 0;
            }
            else if (epi::StringCaseCompareASCII(def->tex_.substr(1), "changetexonly") == 0)
            {
                plane->new_image = SECPIC(model, def->is_ceiling_, nullptr);
            }
            else // Unknown directive after '-'; just do default
            {
                plane->new_image  = SECPIC(model, def->is_ceiling_, nullptr);
                plane->newspecial = model->props.special ? model->props.special->number_ : 0;
            }
        }

        if (model && plane->direction == (def->is_ceiling_ ? DIRECTION_DOWN : DIRECTION_UP))
        {
            SECPIC(sector, def->is_ceiling_, plane->new_image);
            if (plane->newspecial != -1)
            {
                P_SectorChangeSpecial(sector, plane->newspecial);
            }
        }
    }
    else if (def->tex_ != "" && def->tex_[0] == '+')
    {
        if (model)
        {
            if (SECPIC(model, def->is_ceiling_, nullptr) == SECPIC(sector, def->is_ceiling_, nullptr))
            {
                model = P_GetSectorSurrounding(model, plane->destheight, def->is_ceiling_);
            }
        }

        if (model)
        {
            if (def->tex_.size() == 1) // Only '+'; do both (default)
            {
                plane->new_image  = SECPIC(model, def->is_ceiling_, nullptr);
                plane->newspecial = model->props.special ? model->props.special->number_ : 0;
            }
            else if (epi::StringCaseCompareASCII(def->tex_.substr(1), "changezero") == 0)
            {
                plane->new_image  = SECPIC(model, def->is_ceiling_, nullptr);
                plane->newspecial = 0;
            }
            else if (epi::StringCaseCompareASCII(def->tex_.substr(1), "changetexonly") == 0)
            {
                plane->new_image = SECPIC(model, def->is_ceiling_, nullptr);
            }
            else // Unknown directive after '+'; just do default
            {
                plane->new_image  = SECPIC(model, def->is_ceiling_, nullptr);
                plane->newspecial = model->props.special ? model->props.special->number_ : 0;
            }

            if (plane->direction == (def->is_ceiling_ ? DIRECTION_DOWN : DIRECTION_UP))
            {
                SECPIC(sector, def->is_ceiling_, plane->new_image);

                if (plane->newspecial != -1)
                {
                    P_SectorChangeSpecial(sector, plane->newspecial);
                }
            }
        }
    }
    else if (def->tex_ != "")
    {
        plane->new_image = W_ImageLookup(def->tex_.c_str(), kImageNamespaceFlat);
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
static bool EV_BuildOneStair(sector_t *sec, const PlaneMoverDefinition *def)
{
    int   i;
    float next_height;
    bool  more;

    plane_move_t *step;
    sector_t     *tsec;
    float         stairsize = def->dest_;

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
            if (!(sec->lines[i]->flags & MLF_TwoSided))
                continue;

            if (sec != sec->lines[i]->frontsector)
                continue;

            if (sec == sec->lines[i]->backsector)
                continue;

            tsec = sec->lines[i]->backsector;

            if (tsec->floor.image != image && !def->ignore_texture_)
                continue;

            if (def->is_ceiling_ && tsec->ceil_move)
                continue;
            if (!def->is_ceiling_ && tsec->floor_move)
                continue;

            step = P_SetupSectorAction(tsec, def, tsec);
            if (step)
            {
                // Override the destination height
                P_SetupPlaneDirection(step, def, step->startheight, next_height);

                next_height += stairsize;
                sec  = tsec;
                more = true;
            }

            break;
        }
    } while (more);

    return true;
}

static bool EV_BuildStairs(sector_t *sec, const PlaneMoverDefinition *def)
{
    bool rtn = false;

    while (sec->tag_prev)
        sec = sec->tag_prev;

    for (; sec; sec = sec->tag_next)
    {
        // Already moving?  If so, keep going...
        if (def->is_ceiling_ && sec->ceil_move)
            continue;
        if (!def->is_ceiling_ && sec->floor_move)
            continue;

        if (EV_BuildOneStair(sec, def))
            rtn = true;
    }

    return rtn;
}

//
// Do Platforms/Floors/Stairs/Ceilings/Doors/Elevators
//
bool EV_DoPlane(sector_t *sec, const PlaneMoverDefinition *def, sector_t *model)
{
    // Activate all <type> plats that are in_stasis
    switch (def->type_)
    {
    case kPlaneMoverPlatform:
    case kPlaneMoverContinuous:
    case kPlaneMoverToggle:
        if (P_ActivateInStasis(sec->tag))
            return true;
        break;

    case kPlaneMoverStairs:
        return EV_BuildStairs(sec, def);

    case kPlaneMoverStop:
        return P_StasifySector(sec->tag);

    default:
        break;
    }

    if (def->is_ceiling_ || def->type_ == kPlaneMoverElevator)
    {
        if (sec->ceil_move)
            return false;
    }

    if (!def->is_ceiling_)
    {
        if (sec->floor_move)
            return false;
    }

    // Do sector action
    if (sec->floor_vertex_slope || sec->ceil_vertex_slope)
    {
        LogWarning("Plane movers are not supported for vertex slopes! (Sector %u)\n", int(sec - sectors));
        return false;
    }
    plane_move_t *secaction = P_SetupSectorAction(sec, def, model);
    if (secaction && secaction->nukeme)
    {
        delete secaction;
        return true;
    }
    else
        return secaction ? true : false;
}

bool EV_ManualPlane(line_t *line, mobj_t *thing, const PlaneMoverDefinition *def)
{
    int side = 0; // only front sides can be used

    // if the sector has an active thinker, use it
    sector_t *sec = side ? line->frontsector : line->backsector;
    if (!sec)
        return false;

    plane_move_t *pmov = def->is_ceiling_ ? sec->ceil_move : sec->floor_move;

    if (pmov && thing)
    {
        if (def->type_ == kPlaneMoverMoveWaitReturn)
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
                S_StartFX(def->sfxstart_, SNCAT_Level, &sec->sfx_origin);

                pmov->sfxstarted = !thing->player;
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

    for (PMI = active_planes.begin(); PMI != active_planes.end(); PMI++)
    {
        plane_move_t *pmov = *PMI;

        if (pmov->direction == DIRECTION_STASIS && pmov->tag == tag)
        {
            pmov->direction = pmov->olddirection;
            result          = true;
        }
    }

    return result;
}

static bool P_StasifySector(int tag)
{
    bool result = false;

    std::vector<plane_move_t *>::iterator PMI;

    for (PMI = active_planes.begin(); PMI != active_planes.end(); PMI++)
    {
        plane_move_t *pmov = *PMI;

        if (pmov->direction != DIRECTION_STASIS && pmov->tag == tag)
        {
            pmov->olddirection = pmov->direction;
            pmov->direction    = DIRECTION_STASIS;

            result = true;
        }
    }

    return result;
}

bool P_SectorIsLowering(sector_t *sec)
{
    if (!sec->floor_move)
        return false;

    return sec->floor_move->direction < 0;
}

// -AJA- 1999/12/07: cleaned up this donut stuff

//
// Special Stuff that can not be categorized
// Mmmmmmm....  Donuts....
//
bool EV_DoDonut(sector_t *s1, SoundEffect *sfx[4])
{
    sector_t     *s2;
    sector_t     *s3;
    bool          result = false;
    int           i;
    plane_move_t *sec;

    if (!donut_setup)
    {
        donut[0].Default();
        donut[0].count_ = 1;
        donut[0].f_.Default(PlaneMoverDefinition::kPlaneMoverDefaultDonutFloor);
        donut[0].f_.tex_ = "-";

        donut[1].Default();
        donut[1].count_ = 1;
        donut[1].f_.Default(PlaneMoverDefinition::kPlaneMoverDefaultDonutFloor);
        donut[1].f_.dest_ = -32000.0f;

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
        donut[0].f_.sfxup_   = sfx[0];
        donut[0].f_.sfxstop_ = sfx[1];

        sec = P_SetupSectorAction(s2, &donut[0].f_, s3);

        if (sec)
        {
            sec->destheight = s3->f_h;
            s2->floor.image = sec->new_image = s3->floor.image;

            if (s2->floor.image)
            {
                FlatDefinition *current_flatdef = flatdefs.Find(s2->floor.image->name.c_str());
                if (current_flatdef)
                {
                    s2->bob_depth  = current_flatdef->bob_depth_;
                    s2->sink_depth = current_flatdef->sink_depth_;
                }
                else
                    s2->bob_depth = 0;
                s2->sink_depth = 0;
            }
            else
            {
                s2->bob_depth  = 0;
                s2->sink_depth = 0;
            }

            /// s2->props.special = s3->props.special;
            P_SectorChangeSpecial(s2, s3->props.type);
        }

        // Spawn lowering donut-hole
        donut[1].f_.sfxup_   = sfx[2];
        donut[1].f_.sfxstop_ = sfx[3];

        sec = P_SetupSectorAction(s1, &donut[1].f_, s1);

        if (sec)
            sec->destheight = s3->f_h;
        break;
    }

    return result;
}

static inline bool SliderCanClose(line_t *line)
{
    return !P_ThingsOnSliderPath(line);
}

static bool MoveSlider(slider_move_t *smov)
{
    // RETURNS true if slider_move_t should be removed.

    sector_t *sec = smov->line->frontsector;

    float factor = double_framerate.d_? 0.5f : 1.0f;

    switch (smov->direction)
    {
    // WAITING
    case 0:
        smov->waited -= (!double_framerate.d_|| !(game_tic & 1)) ? 1 : 0;
        if (smov->waited <= 0)
        {
            if (SliderCanClose(smov->line))
            {
                S_StartFX(smov->info->sfx_start_, SNCAT_Level, &sec->sfx_origin);

                smov->sfxstarted = false;
                smov->direction  = DIRECTION_DOWN;
            }
            else
            {
                // try again soon
                smov->waited = kTicRate / 3;
            }
        }
        break;

    // OPENING
    case 1:
        MakeMovingSound(&smov->sfxstarted, smov->info->sfx_open_, &sec->sfx_origin);

        smov->opening += (smov->info->speed_ * factor);

        // mark line as non-blocking (at some point)
        P_ComputeGaps(smov->line);

        if (smov->opening >= smov->target)
        {
            S_StartFX(smov->info->sfx_stop_, SNCAT_Level, &sec->sfx_origin);

            smov->opening   = smov->target;
            smov->direction = DIRECTION_WAIT;
            smov->waited    = smov->info->wait_;

            if (smov->final_open)
            {
                line_t *ld = smov->line;

                // clear line special
                ld->slide_door = nullptr;
                ld->special    = nullptr;

                // clear the side textures
                ld->side[0]->middle.image = nullptr;
                ld->side[1]->middle.image = nullptr;

                return true; // REMOVE ME
            }
        }
        break;

    // CLOSING
    case -1:

        if (SliderCanClose(smov->line))
        {
            MakeMovingSound(&smov->sfxstarted, smov->info->sfx_close_, &sec->sfx_origin);

            smov->opening -= (smov->info->speed_ * factor);

            // mark line as blocking (at some point)
            P_ComputeGaps(smov->line);

            if (smov->opening <= 0.0f)
            {
                S_StartFX(smov->info->sfx_stop_, SNCAT_Level, &sec->sfx_origin);

                return true; // REMOVE ME
            }
        }
        else
        {
            MakeMovingSound(&smov->sfxstarted, smov->info->sfx_open_, &sec->sfx_origin);

            smov->opening += (smov->info->speed_ * factor);

            // mark line as non-blocking (at some point)
            P_ComputeGaps(smov->line);

            if (smov->opening >= smov->target)
            {
                S_StartFX(smov->info->sfx_stop_, SNCAT_Level, &sec->sfx_origin);

                smov->opening   = smov->target;
                smov->direction = DIRECTION_WAIT;
                smov->waited    = smov->info->wait_;

                if (smov->final_open)
                {
                    line_t *ld = smov->line;

                    // clear line special
                    ld->slide_door = nullptr;
                    ld->special    = nullptr;

                    // clear the side textures
                    ld->side[0]->middle.image = nullptr;
                    ld->side[1]->middle.image = nullptr;

                    return true; // REMOVE ME
                }
            }
        }

        break;

    default:
        FatalError("MoveSlider: Unknown direction %d", smov->direction);
    }

    return false;
}

//
// Handle thin horizontal sliding doors.
//
bool EV_DoSlider(line_t *door, line_t *act_line, mobj_t *thing, const LineType *special)
{
    SYS_ASSERT(door);

    sector_t *sec = door->frontsector;

    if (!sec || !door->side[0] || !door->side[1])
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

        return false; // nothing happened
    }

    // new sliding door thinker
    smov = new slider_move_t;

    smov->info     = &special->s_;
    smov->line     = door;
    smov->opening  = 0.0f;
    smov->line_len = R_PointToDist(0, 0, door->dx, door->dy);
    smov->target   = smov->line_len * smov->info->distance_;

    smov->direction  = DIRECTION_UP;
    smov->sfxstarted = !(thing && thing->player);
    smov->final_open = (act_line && act_line->count == 1);

    door->slide_door  = special;
    door->slider_move = smov;

    // work-around for RTS-triggered doors, which cannot setup
    // the 'slide_door' field at level load and hence the code
    // which normally blocks the door does not kick in.
    door->flags &= ~MLF_Blocking;

    P_AddActiveSlider(smov);

    // Lobo: SFX_OPEN would not play for monsters.
    // Going forward, I think it's better just to use SFX_OPEN and SFX_CLOSE
    // and quietly forget about SFX_START.

    // S_StartFX(special->s.sfx_start, SNCAT_Level, &sec->sfx_origin);
    S_StartFX(special->s_.sfx_open_, SNCAT_Level, &sec->sfx_origin);

    return true;
}

//
// Executes one tic's plane_move_t thinking.
// Active sectors can destroy themselves, but not each other.
//
void P_RunActivePlanes(void)
{
    if (time_stop_active)
        return;

    std::vector<plane_move_t *>::iterator PMI;

    bool removed_plane = false;

    for (PMI = active_planes.begin(); PMI != active_planes.end(); PMI++)
    {
        plane_move_t *pmov = *PMI;

        if (MovePlane(pmov))
        {
            // Make BOOM scroller effects permanent as this pmov will never be recreated
            if (pmov->type->type_ == kPlaneMoverOnce || pmov->type->type_ == kPlaneMoverStairs || pmov->type->type_ == kPlaneMoverToggle)
            {
                for (auto anim : secanims)
                {
                    if (anim.scroll_sec_ref &&
                        (anim.scroll_sec_ref->ceil_move == pmov || anim.scroll_sec_ref->floor_move == pmov) &&
                        (anim.permanent || anim.scroll_special_ref->scroll_type_ & BoomScrollerTypeAccel))
                    {
                        struct sector_s  *sec_ref     = anim.scroll_sec_ref;
                        sector_t         *sec         = anim.target;
                        const LineType *special_ref = anim.scroll_special_ref;
                        line_s           *line_ref    = anim.scroll_line_ref;
                        if (!sec || !special_ref || !line_ref ||
                            !(special_ref->scroll_type_ & BoomScrollerTypeDisplace ||
                              special_ref->scroll_type_ & BoomScrollerTypeAccel))
                            continue;
                        float heightref =
                            (special_ref->scroll_type_ & BoomScrollerTypeDisplace ? anim.last_height : sec_ref->orig_height);
                        float sy = line_ref->length / 32.0f * line_ref->dy / line_ref->length *
                                   ((sec_ref->f_h + sec_ref->c_h) - heightref);
                        float sx = line_ref->length / 32.0f * line_ref->dx / line_ref->length *
                                   ((sec_ref->f_h + sec_ref->c_h) - heightref);
                        if (double_framerate.d_&& special_ref->scroll_type_ & BoomScrollerTypeDisplace)
                        {
                            sy *= 2;
                            sx *= 2;
                        }
                        if (special_ref->sector_effect_ & kSectorEffectTypePushThings)
                        {
                            sec->props.old_push.Y += BOOM_CARRY_FACTOR * sy;
                            sec->props.push.Y += BOOM_CARRY_FACTOR * sy;
                            sec->props.old_push.X += BOOM_CARRY_FACTOR * sx;
                            sec->props.push.X += BOOM_CARRY_FACTOR * sx;
                        }
                        if (special_ref->sector_effect_ & kSectorEffectTypeScrollFloor)
                        {
                            sec->floor.old_scroll.Y -= sy;
                            sec->floor.scroll.Y -= sy;
                            sec->floor.old_scroll.X -= sx;
                            sec->floor.scroll.X -= sx;
                        }
                        if (special_ref->sector_effect_ & kSectorEffectTypeScrollCeiling)
                        {
                            sec->ceil.old_scroll.Y -= sy;
                            sec->ceil.old_scroll.X -= sx;
                            sec->ceil.scroll.Y -= sy;
                            sec->ceil.scroll.X -= sx;
                        }
                    }
                }
                for (auto anim : lineanims)
                {
                    if (anim.scroll_sec_ref &&
                        (anim.scroll_sec_ref->ceil_move == pmov || anim.scroll_sec_ref->floor_move == pmov) &&
                        (anim.permanent || anim.scroll_special_ref->scroll_type_ & BoomScrollerTypeAccel))
                    {
                        struct sector_s  *sec_ref     = anim.scroll_sec_ref;
                        line_t           *ld          = anim.target;
                        const LineType *special_ref = anim.scroll_special_ref;
                        line_s           *line_ref    = anim.scroll_line_ref;

                        if (!ld || !special_ref || !line_ref)
                            continue;

                        if (special_ref->line_effect_ & kLineEffectTypeVectorScroll)
                        {
                            float tdx       = anim.dynamic_dx;
                            float tdy       = anim.dynamic_dy;
                            float heightref = special_ref->scroll_type_ & BoomScrollerTypeDisplace ? anim.last_height
                                                                                             : sec_ref->orig_height;
                            float sy        = tdy * ((sec_ref->f_h + sec_ref->c_h) - heightref);
                            float sx        = tdx * ((sec_ref->f_h + sec_ref->c_h) - heightref);
                            if (double_framerate.d_&& special_ref->scroll_type_ & BoomScrollerTypeDisplace)
                            {
                                sy *= 2;
                                sx *= 2;
                            }
                            if (ld->side[0])
                            {
                                if (ld->side[0]->top.image)
                                {
                                    ld->side[0]->top.old_scroll.X += sx;
                                    ld->side[0]->top.old_scroll.Y += sy;
                                    ld->side[0]->top.scroll.X += sx;
                                    ld->side[0]->top.scroll.Y += sy;
                                }
                                if (ld->side[0]->middle.image)
                                {
                                    ld->side[0]->middle.old_scroll.X += sx;
                                    ld->side[0]->middle.old_scroll.Y += sy;
                                    ld->side[0]->middle.scroll.X += sx;
                                    ld->side[0]->middle.scroll.Y += sy;
                                }
                                if (ld->side[0]->bottom.image)
                                {
                                    ld->side[0]->bottom.old_scroll.X += sx;
                                    ld->side[0]->bottom.old_scroll.Y += sy;
                                    ld->side[0]->bottom.scroll.X += sx;
                                    ld->side[0]->bottom.scroll.Y += sy;
                                }
                            }
                            if (ld->side[1])
                            {
                                if (ld->side[1]->top.image)
                                {
                                    ld->side[1]->top.old_scroll.X += sx;
                                    ld->side[1]->top.old_scroll.Y += sy;
                                    ld->side[1]->top.scroll.X += sx;
                                    ld->side[1]->top.scroll.Y += sy;
                                }
                                if (ld->side[1]->middle.image)
                                {
                                    ld->side[1]->middle.old_scroll.X += sx;
                                    ld->side[1]->middle.old_scroll.Y += sy;
                                    ld->side[1]->middle.scroll.X += sx;
                                    ld->side[1]->middle.scroll.Y += sy;
                                }
                                if (ld->side[1]->bottom.image)
                                {
                                    ld->side[1]->bottom.old_scroll.X += sx;
                                    ld->side[1]->bottom.old_scroll.Y += sy;
                                    ld->side[1]->bottom.scroll.X += sx;
                                    ld->side[1]->bottom.scroll.Y += sy;
                                }
                            }
                        }
                        if (special_ref->line_effect_ & kLineEffectTypeTaggedOffsetScroll)
                        {
                            float x_speed   = anim.side0_xoffspeed;
                            float y_speed   = anim.side0_yoffspeed;
                            float heightref = special_ref->scroll_type_ & BoomScrollerTypeDisplace ? anim.last_height
                                                                                             : sec_ref->orig_height;
                            float sy        = x_speed * ((sec_ref->f_h + sec_ref->c_h) - heightref);
                            float sx        = y_speed * ((sec_ref->f_h + sec_ref->c_h) - heightref);
                            if (double_framerate.d_&& special_ref->scroll_type_ & BoomScrollerTypeDisplace)
                            {
                                sy *= 2;
                                sx *= 2;
                            }
                            if (ld->side[0])
                            {
                                if (ld->side[0]->top.image)
                                {
                                    ld->side[0]->top.old_scroll.X -= sx;
                                    ld->side[0]->top.old_scroll.Y -= sy;
                                    ld->side[0]->top.scroll.X -= sx;
                                    ld->side[0]->top.scroll.Y -= sy;
                                }
                                if (ld->side[0]->middle.image)
                                {
                                    ld->side[0]->middle.old_scroll.X -= sx;
                                    ld->side[0]->middle.old_scroll.Y -= sy;
                                    ld->side[0]->middle.scroll.X -= sx;
                                    ld->side[0]->middle.scroll.Y -= sy;
                                }
                                if (ld->side[0]->bottom.image)
                                {
                                    ld->side[0]->bottom.old_scroll.X -= sx;
                                    ld->side[0]->bottom.old_scroll.Y -= sy;
                                    ld->side[0]->bottom.scroll.X -= sx;
                                    ld->side[0]->bottom.scroll.Y -= sy;
                                }
                            }
                        }
                    }
                }
            }

            if (pmov->is_ceiling || pmov->is_elevator)
                pmov->sector->ceil_move = nullptr;

            if (!pmov->is_ceiling)
                pmov->sector->floor_move = nullptr;

            *PMI = nullptr;
            delete pmov;

            removed_plane = true;
        }
    }

    if (removed_plane)
    {
        std::vector<plane_move_t *>::iterator ENDP;

        ENDP = std::remove(active_planes.begin(), active_planes.end(), (plane_move_t *)nullptr);

        active_planes.erase(ENDP, active_planes.end());
    }
}

void P_RunActiveSliders(void)
{
    if (time_stop_active)
        return;

    std::vector<slider_move_t *>::iterator SMI;

    bool removed_slider = false;

    for (SMI = active_sliders.begin(); SMI != active_sliders.end(); SMI++)
    {
        slider_move_t *smov = *SMI;

        if (MoveSlider(smov))
        {
            smov->line->slider_move = nullptr;

            *SMI = nullptr;
            delete smov;

            removed_slider = true;
        }
    }

    if (removed_slider)
    {
        std::vector<slider_move_t *>::iterator ENDP;

        ENDP = std::remove(active_sliders.begin(), active_sliders.end(), (slider_move_t *)nullptr);

        active_sliders.erase(ENDP, active_sliders.end());
    }
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
