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

#include <algorithm>

#include "AlmostEquals.h"
#include "dm_defs.h"
#include "dm_state.h"
#include "epi.h"
#include "epi_doomdefs.h"
#include "epi_str_compare.h"
#include "m_random.h"
#include "n_network.h"
#include "p_local.h"
#include "r_misc.h"
#include "r_sky.h"
#include "r_state.h"
#include "s_sound.h"

typedef enum
{
    RES_Ok,
    RES_Crushed,
    RES_PastDest,
    RES_Impossible
} move_result_e;

std::vector<PlaneMover *>       active_planes;
std::vector<SlidingDoorMover *> active_sliders;

extern std::vector<SectorAnimation> sector_animations;
extern std::vector<LineAnimation>   line_animations;

LineType   donut[2];
static int donut_setup = 0;

static bool P_ActivateInStasis(int tag);
static bool P_StasifySector(int tag);

// -AJA- Perhaps using a pointer to `plane_info_t' would be better
//       than f**king about with the floorOrCeiling stuff all the
//       time.

static float HEIGHT(Sector *sec, bool is_ceiling)
{
    if (is_ceiling)
        return sec->ceiling_height;

    return sec->floor_height;
}

static const Image *SECPIC(Sector *sec, bool is_ceiling, const Image *new_image)
{
    if (new_image)
    {
        if (is_ceiling)
            sec->ceiling.image = new_image;
        else
        {
            sec->floor.image = new_image;
            if (new_image)
            {
                FlatDefinition *current_flatdef = flatdefs.Find(new_image->name_.c_str());
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

        if (new_image == sky_flat_image)
            ComputeSkyHeights();
    }

    return is_ceiling ? sec->ceiling.image : sec->floor.image;
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
static float GetSecHeightReference(const PlaneMoverDefinition *def, Sector *sec, Sector *model)
{
    const TriggerHeightReference ref = def->destref_;

    switch (ref & kTriggerHeightReferenceMask)
    {
    case kTriggerHeightReferenceAbsolute:
        return 0;

    case kTriggerHeightReferenceTriggeringLinedef:
        if (model)
            return (ref & kTriggerHeightReferenceCeiling) ? model->ceiling_height : model->floor_height;

        return 0; // ick!

    case kTriggerHeightReferenceCurrent:
        return (ref & kTriggerHeightReferenceCeiling) ? sec->ceiling_height : sec->floor_height;

    case kTriggerHeightReferenceSurrounding:
        return FindSurroundingHeight(ref, sec);

    case kTriggerHeightReferenceLowestLowTexture:
        return FindRaiseToTexture(def, sec);

    default:
        FatalError("GetSecHeightReference: undefined reference %d\n", ref);
    }
}

static constexpr uint8_t kReloopTics = 6;

static void MakeMovingSound(bool *started_var, SoundEffect *sfx, Position *pos)
{
    if (!sfx || sfx->num < 1)
        return;

    SoundEffectDefinition *def = sfxdefs[sfx->sounds[0]];

    // looping sounds need to be "pumped" to keep looping.
    // The main one is STNMOV, which lasts a little over 0.25 seconds,
    // hence we need to pump it every 6 tics or so.

    if (!*started_var || (def->looping_ && (level_time_elapsed % kReloopTics) == 0))
    {
        StartSoundEffect(sfx, kCategoryLevel, pos);

        *started_var = true;
    }
}

void AddActivePlane(PlaneMover *pmov)
{
    active_planes.push_back(pmov);
}

void AddActiveSlider(SlidingDoorMover *smov)
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
void DestroyAllPlanes(void)
{
    std::vector<PlaneMover *>::iterator PMI;

    for (PMI = active_planes.begin(); PMI != active_planes.end(); PMI++)
    {
        delete (*PMI);
    }

    active_planes.clear();
}

void DestroyAllSliders(void)
{
    std::vector<SlidingDoorMover *>::iterator SMI;

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
static move_result_e AttemptMovePlane(Sector *sector, float speed, float dest, int crush, bool is_ceiling,
                                      int direction)
{
    bool past = false;
    bool nofit;

    //
    // check whether we have gone past the destination height
    //
    if (direction == kPlaneDirectionUp && HEIGHT(sector, is_ceiling) + speed > dest)
    {
        past  = true;
        speed = dest - HEIGHT(sector, is_ceiling);
    }
    else if (direction == kPlaneDirectionDown && HEIGHT(sector, is_ceiling) - speed < dest)
    {
        past  = true;
        speed = HEIGHT(sector, is_ceiling) - dest;
    }

    if (speed <= 0)
        return RES_PastDest;

    if (direction == kPlaneDirectionDown)
        speed = -speed;

    // check if even possible
    if (!CheckSolidSectorMove(sector, is_ceiling, speed))
    {
        return RES_Impossible;
    }

    //
    // move the actual sector, including all things in it
    //
    nofit = SolidSectorMove(sector, is_ceiling, speed, crush, false);

    if (!nofit)
        return past ? RES_PastDest : RES_Ok;

    // bugger, something got in our way !

    if (crush == 0)
    {
        // undo the change
        SolidSectorMove(sector, is_ceiling, -speed, false, false);
    }

    return past ? RES_PastDest : RES_Crushed;
}

static move_result_e AttemptMoveSector(Sector *sector, PlaneMover *pmov, float dest, int crush)
{
    move_result_e res;

    if (!pmov->is_elevator)
    {
        return AttemptMovePlane(sector, pmov->speed, dest, crush, pmov->is_ceiling, pmov->direction);
    }

    //-------------------//
    //   ELEVATOR MOVE   //
    //-------------------//

    if (pmov->direction == kPlaneDirectionUp)
    {
        AttemptMovePlane(sector, 32768.0, HMM_MIN(sector->floor_height + pmov->speed, dest) + pmov->elevator_height,
                         false, true, kPlaneDirectionUp);
    }

    res = AttemptMovePlane(sector, pmov->speed, dest, crush, false, pmov->direction);

    if (pmov->direction == kPlaneDirectionDown)
    {
        AttemptMovePlane(sector, 32768.0, sector->floor_height + pmov->elevator_height, false, true,
                         kPlaneDirectionDown);
    }

    return res;
}

static bool MovePlane(PlaneMover *plane)
{
    // Move a floor to it's destination (up or down).
    //
    // RETURNS true if PlaneMover should be removed.

    move_result_e res;

    Sector *sec = plane->sector;

    // track if move went from start to dest in one move; if so
    // do not interpolate the sector height
    bool maybe_instant = false;

    if (plane->is_ceiling || plane->is_elevator)
        sec->old_ceiling_height = sec->ceiling_height;
    if (!plane->is_ceiling)
        sec->old_floor_height = sec->floor_height;

    switch (plane->direction)
    {
    case kPlaneDirectionStasis:
        plane->sound_effect_started = false;
        break;

    case kPlaneDirectionDown:
        if (plane->is_ceiling || plane->is_elevator)
        {
            if (AlmostEquals(plane->start_height, sec->ceiling_height))
                maybe_instant = true;
        }
        if (!plane->is_ceiling)
        {
            if (AlmostEquals(plane->start_height, sec->floor_height))
                maybe_instant = true;
        }

        res = AttemptMoveSector(sec, plane, HMM_MIN(plane->start_height, plane->destination_height),
                                plane->is_ceiling ? plane->crush : 0);

        if (maybe_instant)
        {
            if (plane->is_ceiling || plane->is_elevator)
            {
                if (AlmostEquals(plane->destination_height, sec->ceiling_height))
                    sec->old_ceiling_height = sec->ceiling_height;
            }
            if (!plane->is_ceiling)
            {
                if (AlmostEquals(plane->destination_height, sec->floor_height))
                    sec->old_floor_height = sec->floor_height;
            }
        }

        if (!AlmostEquals(plane->destination_height, plane->start_height))
        {
            MakeMovingSound(&plane->sound_effect_started, plane->type->sfxdown_, &sec->sound_effects_origin);
        }

        if (res == RES_PastDest)
        {
            if (!AlmostEquals(plane->destination_height, plane->start_height))
            {
                StartSoundEffect(plane->type->sfxstop_, kCategoryLevel, &sec->sound_effects_origin);
            }

            plane->speed = plane->type->speed_up_;

            if (plane->new_special != -1)
            {
                SectorChangeSpecial(sec, plane->new_special);
            }

            SECPIC(sec, plane->is_ceiling, plane->new_image);

            switch (plane->type->type_)
            {
            case kPlaneMoverPlatform:
            case kPlaneMoverContinuous:
                plane->direction = kPlaneDirectionWait;
                plane->waited    = plane->type->wait_;
                plane->speed     = plane->type->speed_up_;
                break;

            case kPlaneMoverMoveWaitReturn:
                if (AlmostEquals(HEIGHT(sec, plane->is_ceiling), plane->start_height))
                {
                    return true; // REMOVE ME
                }
                else             // assume we reached the destination
                {
                    plane->direction = kPlaneDirectionWait;
                    plane->waited    = plane->type->wait_;
                    plane->speed     = plane->type->speed_up_;
                }
                break;

            case kPlaneMoverToggle:
                plane->direction     = kPlaneDirectionStasis;
                plane->old_direction = kPlaneDirectionUp;
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
                plane->direction            = kPlaneDirectionUp;
                plane->sound_effect_started = false;
                plane->waited               = 0;
                plane->speed                = plane->type->speed_up_;
            }
        }

        break;

    case kPlaneDirectionWait:
        plane->waited--;
        if (plane->waited <= 0)
        {
            int   dir;
            float dest;

            if (AlmostEquals(HEIGHT(sec, plane->is_ceiling), plane->destination_height))
                dest = plane->start_height;
            else
                dest = plane->destination_height;

            if (HEIGHT(sec, plane->is_ceiling) > dest)
            {
                dir          = kPlaneDirectionDown;
                plane->speed = plane->type->speed_down_;
            }
            else
            {
                dir          = kPlaneDirectionUp;
                plane->speed = plane->type->speed_up_;
            }

            if (dir)
            {
                StartSoundEffect(plane->type->sfxstart_, kCategoryLevel, &sec->sound_effects_origin);
            }

            plane->direction            = dir; // time to go back
            plane->sound_effect_started = false;
        }
        break;

    case kPlaneDirectionUp:
        if (plane->is_ceiling || plane->is_elevator)
        {
            if (AlmostEquals(plane->start_height, sec->ceiling_height))
                maybe_instant = true;
        }
        if (!plane->is_ceiling)
        {
            if (AlmostEquals(plane->start_height, sec->floor_height))
                maybe_instant = true;
        }

        res = AttemptMoveSector(sec, plane, HMM_MAX(plane->start_height, plane->destination_height),
                                plane->is_ceiling ? 0 : plane->crush);

        if (maybe_instant)
        {
            if (plane->is_ceiling || plane->is_elevator)
            {
                if (AlmostEquals(plane->destination_height, sec->ceiling_height))
                    sec->old_ceiling_height = sec->ceiling_height;
            }
            if (!plane->is_ceiling)
            {
                if (AlmostEquals(plane->destination_height, sec->floor_height))
                    sec->old_floor_height = sec->floor_height;
            }
        }

        if (!AlmostEquals(plane->destination_height, plane->start_height))
        {
            MakeMovingSound(&plane->sound_effect_started, plane->type->sfxup_, &sec->sound_effects_origin);
        }

        if (res == RES_PastDest)
        {
            if (!AlmostEquals(plane->destination_height, plane->start_height))
            {
                StartSoundEffect(plane->type->sfxstop_, kCategoryLevel, &sec->sound_effects_origin);
            }

            if (plane->new_special != -1)
            {
                SectorChangeSpecial(sec, plane->new_special);
            }

            SECPIC(sec, plane->is_ceiling, plane->new_image);

            switch (plane->type->type_)
            {
            case kPlaneMoverPlatform:
            case kPlaneMoverContinuous:
                plane->direction = kPlaneDirectionWait;
                plane->waited    = plane->type->wait_;
                plane->speed     = plane->type->speed_down_;
                break;

            case kPlaneMoverMoveWaitReturn:
                if (AlmostEquals(HEIGHT(sec, plane->is_ceiling), plane->start_height))
                {
                    return true; // REMOVE ME
                }
                else             // assume we reached the destination
                {
                    plane->direction = kPlaneDirectionWait;
                    plane->speed     = plane->type->speed_down_;
                    plane->waited    = plane->type->wait_;
                }
                break;

            case kPlaneMoverToggle:
                plane->direction     = kPlaneDirectionStasis;
                plane->old_direction = kPlaneDirectionDown;
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
                plane->direction            = kPlaneDirectionDown;
                plane->sound_effect_started = false;
                plane->waited               = 0;
                plane->speed                = plane->type->speed_down_;
            }
        }
        break;

    default:
        FatalError("MovePlane: Unknown direction %d", plane->direction);
    }

    return false;
}

static Sector *P_GSS(Sector *sec, float dest, bool forc)
{
    int     i;
    int     secnum = sec - level_sectors;
    Sector *sector;

    // 2023.06.10 - Reversed the order of iteration because it was returning
    // the greatest numbered linedef for applicable surrounding sectors instead
    // of the least.

    for (i = 0; i < sec->line_count - 1; i++)
    {
        if (LineIsTwoSided(secnum, i))
        {
            if (GetLineSidedef(secnum, i, 0)->sector - level_sectors == secnum)
            {
                sector = GetLineSector(secnum, i, 1);

                if (SECPIC(sector, forc, nullptr) != SECPIC(sec, forc, nullptr) &&
                    AlmostEquals(HEIGHT(sector, forc), dest))
                {
                    return sector;
                }
            }
            else
            {
                sector = GetLineSector(secnum, i, 0);

                if (SECPIC(sector, forc, nullptr) != SECPIC(sec, forc, nullptr) &&
                    AlmostEquals(HEIGHT(sector, forc), dest))
                {
                    return sector;
                }
            }
        }
    }

    for (i = 0; i < sec->line_count - 1; i++)
    {
        if (LineIsTwoSided(secnum, i))
        {
            if (GetLineSidedef(secnum, i, 0)->sector - level_sectors == secnum)
            {
                sector = GetLineSector(secnum, i, 1);
            }
            else
            {
                sector = GetLineSector(secnum, i, 0);
            }
            if (sector->valid_count != valid_count)
            {
                sector->valid_count = valid_count;
                sector              = P_GSS(sector, dest, forc);
                if (sector)
                    return sector;
            }
        }
    }

    return nullptr;
}

static Sector *GetLineSectorSurrounding(Sector *sec, float dest, bool forc)
{
    valid_count++;
    sec->valid_count = valid_count;
    return P_GSS(sec, dest, forc);
}

void P_SetupPlaneDirection(PlaneMover *plane, const PlaneMoverDefinition *def, float start, float dest)
{
    plane->start_height       = start;
    plane->destination_height = dest;

    if (dest > start)
    {
        plane->direction = kPlaneDirectionUp;

        if (def->speed_up_ >= 0)
            plane->speed = def->speed_up_;
        else
            plane->speed = dest - start;
    }
    else if (start > dest)
    {
        plane->direction = kPlaneDirectionDown;

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
static PlaneMover *P_SetupSectorAction(Sector *sector, const PlaneMoverDefinition *def, Sector *model)
{
    // new door thinker
    PlaneMover *plane = new PlaneMover;

    if (def->is_ceiling_)
        sector->ceiling_move = plane;
    else
        sector->floor_move = plane;

    plane->sector               = sector;
    plane->model                = model;
    plane->crush                = def->crush_damage_;
    plane->sound_effect_started = false;

    float start = HEIGHT(sector, def->is_ceiling_);

    float dest = GetSecHeightReference(def, sector, model);
    dest += def->dest_;

    if (def->type_ == kPlaneMoverPlatform || def->type_ == kPlaneMoverContinuous || def->type_ == kPlaneMoverToggle)
    {
        start = GetSecHeightReference(def, sector, model);
        start += def->other_;
    }

    if (def->prewait_)
    {
        plane->direction          = kPlaneDirectionWait;
        plane->waited             = def->prewait_;
        plane->destination_height = dest;
        plane->start_height       = start;
    }
    else if (def->type_ == kPlaneMoverContinuous)
    {
        plane->direction = (RandomByteDeterministic() & 1) ? kPlaneDirectionUp : kPlaneDirectionDown;

        if (plane->direction == kPlaneDirectionUp)
            plane->speed = def->speed_up_;
        else
            plane->speed = def->speed_down_;

        plane->destination_height = dest;
        plane->start_height       = start;
    }
    else if (!AlmostEquals(start, dest))
    {
        P_SetupPlaneDirection(plane, def, start, dest);
    }
    else
    {
        // 2023.08.01 - If already at dest height, run the
        // texture/type changes that were intended

        plane->destination_height = dest;
        plane->new_special        = -1;

        // change to surrounding
        if (def->tex_ != "" && def->tex_[0] == '-')
        {
            model = GetLineSectorSurrounding(sector, plane->destination_height, def->is_ceiling_);
            if (model)
            {
                if (def->tex_.size() == 1) // Only '-'; do both (default)
                {
                    plane->new_image   = SECPIC(model, def->is_ceiling_, nullptr);
                    plane->new_special = model->properties.special ? model->properties.special->number_ : 0;
                }
                else if (epi::StringCaseCompareASCII(def->tex_.substr(1), "changezero") == 0)
                {
                    plane->new_image   = SECPIC(model, def->is_ceiling_, nullptr);
                    plane->new_special = 0;
                }
                else if (epi::StringCaseCompareASCII(def->tex_.substr(1), "changetexonly") == 0)
                {
                    plane->new_image = SECPIC(model, def->is_ceiling_, nullptr);
                }
                else // Unknown directive after '-'; just do default
                {
                    plane->new_image   = SECPIC(model, def->is_ceiling_, nullptr);
                    plane->new_special = model->properties.special ? model->properties.special->number_ : 0;
                }
                SECPIC(sector, def->is_ceiling_, plane->new_image);
                if (plane->new_special != -1)
                {
                    SectorChangeSpecial(sector, plane->new_special);
                }
            }
        }
        else if (def->tex_ != "" && def->tex_[0] == '+')
        {
            if (model)
            {
                if (SECPIC(model, def->is_ceiling_, nullptr) == SECPIC(sector, def->is_ceiling_, nullptr))
                {
                    model = GetLineSectorSurrounding(model, plane->destination_height, def->is_ceiling_);
                }
            }

            if (model)
            {
                if (def->tex_.size() == 1) // Only '+'; do both (default)
                {
                    plane->new_image   = SECPIC(model, def->is_ceiling_, nullptr);
                    plane->new_special = model->properties.special ? model->properties.special->number_ : 0;
                }
                else if (epi::StringCaseCompareASCII(def->tex_.substr(1), "changezero") == 0)
                {
                    plane->new_image   = SECPIC(model, def->is_ceiling_, nullptr);
                    plane->new_special = 0;
                }
                else if (epi::StringCaseCompareASCII(def->tex_.substr(1), "changetexonly") == 0)
                {
                    plane->new_image = SECPIC(model, def->is_ceiling_, nullptr);
                }
                else // Unknown directive after '+'; just do default
                {
                    plane->new_image   = SECPIC(model, def->is_ceiling_, nullptr);
                    plane->new_special = model->properties.special ? model->properties.special->number_ : 0;
                }

                SECPIC(sector, def->is_ceiling_, plane->new_image);

                if (plane->new_special != -1)
                {
                    SectorChangeSpecial(sector, plane->new_special);
                }
            }
        }
        else if (def->tex_ != "")
        {
            plane->new_image = ImageLookup(def->tex_.c_str(), kImageNamespaceFlat);
            SECPIC(sector, def->is_ceiling_, plane->new_image);
        }

        if (def->is_ceiling_)
            sector->ceiling_move = nullptr;
        else
            sector->floor_move = nullptr;

        plane->nuke_me = true;

        return plane;
    }

    plane->tag             = sector->tag;
    plane->type            = def;
    plane->new_image       = SECPIC(sector, def->is_ceiling_, nullptr);
    plane->new_special     = -1;
    plane->is_ceiling      = def->is_ceiling_;
    plane->is_elevator     = (def->type_ == kPlaneMoverElevator);
    plane->elevator_height = sector->ceiling_height - sector->floor_height;

    // -ACB- 10/01/2001 Trigger starting sfx
    // UNNEEDED    sound::StopLoopingFX(&sector->sound_effects_origin);

    if (def->sfxstart_ && !AlmostEquals(plane->destination_height, plane->start_height))
    {
        StartSoundEffect(def->sfxstart_, kCategoryLevel, &sector->sound_effects_origin);
    }

    // change to surrounding
    if (def->tex_ != "" && def->tex_[0] == '-')
    {
        model = GetLineSectorSurrounding(sector, plane->destination_height, def->is_ceiling_);
        if (model)
        {
            if (def->tex_.size() == 1) // Only '-'; do both (default)
            {
                plane->new_image   = SECPIC(model, def->is_ceiling_, nullptr);
                plane->new_special = model->properties.special ? model->properties.special->number_ : 0;
            }
            else if (epi::StringCaseCompareASCII(def->tex_.substr(1), "changezero") == 0)
            {
                plane->new_image   = SECPIC(model, def->is_ceiling_, nullptr);
                plane->new_special = 0;
            }
            else if (epi::StringCaseCompareASCII(def->tex_.substr(1), "changetexonly") == 0)
            {
                plane->new_image = SECPIC(model, def->is_ceiling_, nullptr);
            }
            else // Unknown directive after '-'; just do default
            {
                plane->new_image   = SECPIC(model, def->is_ceiling_, nullptr);
                plane->new_special = model->properties.special ? model->properties.special->number_ : 0;
            }
        }

        if (model && plane->direction == (def->is_ceiling_ ? kPlaneDirectionDown : kPlaneDirectionUp))
        {
            SECPIC(sector, def->is_ceiling_, plane->new_image);
            if (plane->new_special != -1)
            {
                SectorChangeSpecial(sector, plane->new_special);
            }
        }
    }
    else if (def->tex_ != "" && def->tex_[0] == '+')
    {
        if (model)
        {
            if (SECPIC(model, def->is_ceiling_, nullptr) == SECPIC(sector, def->is_ceiling_, nullptr))
            {
                model = GetLineSectorSurrounding(model, plane->destination_height, def->is_ceiling_);
            }
        }

        if (model)
        {
            if (def->tex_.size() == 1) // Only '+'; do both (default)
            {
                plane->new_image   = SECPIC(model, def->is_ceiling_, nullptr);
                plane->new_special = model->properties.special ? model->properties.special->number_ : 0;
            }
            else if (epi::StringCaseCompareASCII(def->tex_.substr(1), "changezero") == 0)
            {
                plane->new_image   = SECPIC(model, def->is_ceiling_, nullptr);
                plane->new_special = 0;
            }
            else if (epi::StringCaseCompareASCII(def->tex_.substr(1), "changetexonly") == 0)
            {
                plane->new_image = SECPIC(model, def->is_ceiling_, nullptr);
            }
            else // Unknown directive after '+'; just do default
            {
                plane->new_image   = SECPIC(model, def->is_ceiling_, nullptr);
                plane->new_special = model->properties.special ? model->properties.special->number_ : 0;
            }

            if (plane->direction == (def->is_ceiling_ ? kPlaneDirectionDown : kPlaneDirectionUp))
            {
                SECPIC(sector, def->is_ceiling_, plane->new_image);

                if (plane->new_special != -1)
                {
                    SectorChangeSpecial(sector, plane->new_special);
                }
            }
        }
    }
    else if (def->tex_ != "")
    {
        plane->new_image = ImageLookup(def->tex_.c_str(), kImageNamespaceFlat);
    }

    AddActivePlane(plane);

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
static bool EV_BuildOneStair(Sector *sec, const PlaneMoverDefinition *def)
{
    int   i;
    float next_height;
    bool  more;

    PlaneMover *step;
    Sector     *tsec;
    float       stairsize = def->dest_;

    const Image *image = sec->floor.image;

    // new floor thinker

    step = P_SetupSectorAction(sec, def, sec);
    if (!step)
        return false;

    next_height = step->destination_height + stairsize;

    do
    {
        more = false;

        // Find next sector to raise
        //
        // 1. Find 2-sided line with same sector side[0]
        // 2. Other side is the next sector to raise
        //
        for (i = 0; i < sec->line_count; i++)
        {
            if (!(sec->lines[i]->flags & kLineFlagTwoSided))
                continue;

            if (sec != sec->lines[i]->front_sector)
                continue;

            if (sec == sec->lines[i]->back_sector)
                continue;

            tsec = sec->lines[i]->back_sector;

            if (tsec->floor.image != image && !def->ignore_texture_)
                continue;

            if (def->is_ceiling_ && tsec->ceiling_move)
                continue;
            if (!def->is_ceiling_ && tsec->floor_move)
                continue;

            step = P_SetupSectorAction(tsec, def, tsec);
            if (step)
            {
                // Override the destination height
                P_SetupPlaneDirection(step, def, step->start_height, next_height);

                next_height += stairsize;
                sec  = tsec;
                more = true;
            }

            break;
        }
    } while (more);

    return true;
}

static bool EV_BuildStairs(Sector *sec, const PlaneMoverDefinition *def)
{
    bool rtn = false;

    while (sec->tag_previous)
        sec = sec->tag_previous;

    for (; sec; sec = sec->tag_next)
    {
        // Already moving?  If so, keep going...
        if (def->is_ceiling_ && sec->ceiling_move)
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
bool RunPlaneMover(Sector *sec, const PlaneMoverDefinition *def, Sector *model)
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
        if (sec->ceiling_move)
            return false;
    }

    if (!def->is_ceiling_)
    {
        if (sec->floor_move)
            return false;
    }

    // Do sector action
    if (sec->floor_vertex_slope || sec->ceiling_vertex_slope)
    {
        LogWarning("Plane movers are not supported for vertex slopes! (Sector %u)\n", int(sec - level_sectors));
        return false;
    }
    PlaneMover *secaction = P_SetupSectorAction(sec, def, model);
    if (secaction && secaction->nuke_me)
    {
        delete secaction;
        return true;
    }
    else
        return secaction ? true : false;
}

bool RunManualPlaneMover(Line *line, MapObject *thing, const PlaneMoverDefinition *def)
{
    int side = 0; // only front sides can be used

    // if the sector has an active thinker, use it
    Sector *sec = side ? line->front_sector : line->back_sector;
    if (!sec)
        return false;

    PlaneMover *pmov = def->is_ceiling_ ? sec->ceiling_move : sec->floor_move;

    if (pmov && thing)
    {
        if (def->type_ == kPlaneMoverMoveWaitReturn)
        {
            int newdir;
            int olddir = pmov->direction;

            // only players close doors
            if ((pmov->direction != kPlaneDirectionDown) && thing->player_)
                newdir = pmov->direction = kPlaneDirectionDown;
            else
                newdir = pmov->direction = kPlaneDirectionUp;

            if (newdir != olddir)
            {
                StartSoundEffect(def->sfxstart_, kCategoryLevel, &sec->sound_effects_origin);

                pmov->sound_effect_started = !thing->player_;
                return true;
            }
        }

        return false;
    }

    return RunPlaneMover(sec, def, sec);
}

static bool P_ActivateInStasis(int tag)
{
    bool result = false;

    std::vector<PlaneMover *>::iterator PMI;

    for (PMI = active_planes.begin(); PMI != active_planes.end(); PMI++)
    {
        PlaneMover *pmov = *PMI;

        if (pmov->direction == kPlaneDirectionStasis && pmov->tag == tag)
        {
            pmov->direction = pmov->old_direction;
            result          = true;
        }
    }

    return result;
}

static bool P_StasifySector(int tag)
{
    bool result = false;

    std::vector<PlaneMover *>::iterator PMI;

    for (PMI = active_planes.begin(); PMI != active_planes.end(); PMI++)
    {
        PlaneMover *pmov = *PMI;

        if (pmov->direction != kPlaneDirectionStasis && pmov->tag == tag)
        {
            pmov->old_direction = pmov->direction;
            pmov->direction     = kPlaneDirectionStasis;

            result = true;
        }
    }

    return result;
}

bool SectorIsLowering(Sector *sec)
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
bool RunDonutSpecial(Sector *s1, SoundEffect *sfx[4])
{
    Sector     *s2;
    Sector     *s3;
    bool        result = false;
    int         i;
    PlaneMover *sec;

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

    s2 = GetLineSectorAdjacent(s1->lines[0], s1);

    for (i = 0; i < s2->line_count; i++)
    {
        if (!(s2->lines[i]->flags & kLineFlagTwoSided) || (s2->lines[i]->back_sector == s1))
            continue;

        s3 = s2->lines[i]->back_sector;

        result = true;

        // Spawn rising slime
        donut[0].f_.sfxup_   = sfx[0];
        donut[0].f_.sfxstop_ = sfx[1];

        sec = P_SetupSectorAction(s2, &donut[0].f_, s3);

        if (sec)
        {
            sec->destination_height = s3->floor_height;
            s2->floor.image = sec->new_image = s3->floor.image;

            if (s2->floor.image)
            {
                FlatDefinition *current_flatdef = flatdefs.Find(s2->floor.image->name_.c_str());
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

            /// s2->properties.special = s3->properties.special;
            SectorChangeSpecial(s2, s3->properties.type);
        }

        // Spawn lowering donut-hole
        donut[1].f_.sfxup_   = sfx[2];
        donut[1].f_.sfxstop_ = sfx[3];

        sec = P_SetupSectorAction(s1, &donut[1].f_, s1);

        if (sec)
            sec->destination_height = s3->floor_height;
        break;
    }

    return result;
}

static inline bool SliderCanClose(Line *line)
{
    return !CheckSliderPathForThings(line);
}

static bool MoveSlider(SlidingDoorMover *smov)
{
    // RETURNS true if SlidingDoorMover should be removed.

    smov->old_opening = smov->opening;

    Sector *sec = smov->line->front_sector;

    float factor = 1.0f;

    switch (smov->direction)
    {
    // WAITING
    case 0:
        smov->waited--;
        if (smov->waited <= 0)
        {
            if (SliderCanClose(smov->line))
            {
                StartSoundEffect(smov->info->sfx_start_, kCategoryLevel, &sec->sound_effects_origin);

                smov->sound_effect_started = false;
                smov->direction            = kPlaneDirectionDown;
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
        MakeMovingSound(&smov->sound_effect_started, smov->info->sfx_open_, &sec->sound_effects_origin);

        smov->opening += (smov->info->speed_ * factor);

        // mark line as non-blocking (at some point)
        ComputeGaps(smov->line);

        if (smov->opening >= smov->target)
        {
            StartSoundEffect(smov->info->sfx_stop_, kCategoryLevel, &sec->sound_effects_origin);

            smov->opening   = smov->target;
            smov->direction = kPlaneDirectionWait;
            smov->waited    = smov->info->wait_;

            if (smov->final_open)
            {
                Line *ld = smov->line;

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
            MakeMovingSound(&smov->sound_effect_started, smov->info->sfx_close_, &sec->sound_effects_origin);

            smov->opening -= (smov->info->speed_ * factor);

            // mark line as blocking (at some point)
            ComputeGaps(smov->line);

            if (smov->opening <= 0.0f)
            {
                StartSoundEffect(smov->info->sfx_stop_, kCategoryLevel, &sec->sound_effects_origin);

                return true; // REMOVE ME
            }
        }
        else
        {
            MakeMovingSound(&smov->sound_effect_started, smov->info->sfx_open_, &sec->sound_effects_origin);

            smov->opening += (smov->info->speed_ * factor);

            // mark line as non-blocking (at some point)
            ComputeGaps(smov->line);

            if (smov->opening >= smov->target)
            {
                StartSoundEffect(smov->info->sfx_stop_, kCategoryLevel, &sec->sound_effects_origin);

                smov->opening   = smov->target;
                smov->direction = kPlaneDirectionWait;
                smov->waited    = smov->info->wait_;

                if (smov->final_open)
                {
                    Line *ld = smov->line;

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
bool RunSlidingDoor(Line *door, Line *act_line, MapObject *thing, const LineType *special)
{
    EPI_ASSERT(door);

    Sector *sec = door->front_sector;

    if (!sec || !door->side[0] || !door->side[1])
        return false;

    SlidingDoorMover *smov;

    // if the line has an active thinker, use it
    if (door->slider_move)
    {
        smov = door->slider_move;

        // only players close doors
        if (smov->direction == kPlaneDirectionWait && thing && thing->player_)
        {
            smov->waited = 0;
            return true;
        }

        return false; // nothing happened
    }

    // new sliding door thinker
    smov = new SlidingDoorMover;

    smov->info        = &special->s_;
    smov->line        = door;
    smov->opening     = 0.0f;
    smov->old_opening = 0.0f;
    smov->line_length = PointToDistance(0, 0, door->delta_x, door->delta_y);
    smov->target      = smov->line_length * smov->info->distance_;

    smov->direction            = kPlaneDirectionUp;
    smov->sound_effect_started = !(thing && thing->player_);
    smov->final_open           = (act_line && act_line->count == 1);

    door->slide_door  = special;
    door->slider_move = smov;

    // work-around for RTS-triggered doors, which cannot setup
    // the 'slide_door' field at level load and hence the code
    // which normally blocks the door does not kick in.
    door->flags &= ~kLineFlagBlocking;

    AddActiveSlider(smov);

    // Lobo: SFX_OPEN would not play for monsters.
    // Going forward, I think it's better just to use SFX_OPEN and SFX_CLOSE
    // and quietly forget about SFX_START.

    // StartSoundEffect(special->s.sfx_start, kCategoryLevel,
    // &sec->sound_effects_origin);
    StartSoundEffect(special->s_.sfx_open_, kCategoryLevel, &sec->sound_effects_origin);

    return true;
}

//
// Executes one tic's PlaneMover thinking.
// Active sectors can destroy themselves, but not each other.
//
void RunActivePlanes(void)
{
    if (time_stop_active)
        return;

    std::vector<PlaneMover *>::iterator PMI;

    bool removed_plane = false;

    for (PMI = active_planes.begin(); PMI != active_planes.end(); PMI++)
    {
        PlaneMover *pmov = *PMI;

        if (MovePlane(pmov))
        {
            // Make BOOM scroller effects permanent as this pmov will never be
            // recreated
            if (pmov->type->type_ == kPlaneMoverOnce || pmov->type->type_ == kPlaneMoverStairs ||
                pmov->type->type_ == kPlaneMoverToggle)
            {
                for (const SectorAnimation &anim : sector_animations)
                {
                    if (anim.scroll_sector_reference &&
                        (anim.scroll_sector_reference->ceiling_move == pmov ||
                         anim.scroll_sector_reference->floor_move == pmov) &&
                        (anim.permanent || anim.scroll_special_reference->scroll_type_ & BoomScrollerTypeAccel))
                    {
                        struct Sector  *sec_ref     = anim.scroll_sector_reference;
                        Sector         *sec         = anim.target;
                        const LineType *special_ref = anim.scroll_special_reference;
                        Line           *line_ref    = anim.scroll_line_reference;
                        if (!sec || !special_ref || !line_ref ||
                            !(special_ref->scroll_type_ & BoomScrollerTypeDisplace ||
                              special_ref->scroll_type_ & BoomScrollerTypeAccel))
                            continue;
                        float heightref =
                            (special_ref->scroll_type_ & BoomScrollerTypeDisplace ? anim.last_height
                                                                                  : sec_ref->original_height);
                        float sy = line_ref->length / 32.0f * line_ref->delta_y / line_ref->length *
                                   ((sec_ref->floor_height + sec_ref->ceiling_height) - heightref);
                        float sx = line_ref->length / 32.0f * line_ref->delta_x / line_ref->length *
                                   ((sec_ref->floor_height + sec_ref->ceiling_height) - heightref);
                        if (special_ref->sector_effect_ & kSectorEffectTypePushThings)
                        {
                            sec->properties.old_push.Y += kBoomCarryFactor * sy;
                            sec->properties.push.Y += kBoomCarryFactor * sy;
                            sec->properties.old_push.X += kBoomCarryFactor * sx;
                            sec->properties.push.X += kBoomCarryFactor * sx;
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
                            sec->ceiling.old_scroll.Y -= sy;
                            sec->ceiling.old_scroll.X -= sx;
                            sec->ceiling.scroll.Y -= sy;
                            sec->ceiling.scroll.X -= sx;
                        }
                    }
                }
                for (const LineAnimation &anim : line_animations)
                {
                    if (anim.scroll_sector_reference &&
                        (anim.scroll_sector_reference->ceiling_move == pmov ||
                         anim.scroll_sector_reference->floor_move == pmov) &&
                        (anim.permanent || anim.scroll_special_reference->scroll_type_ & BoomScrollerTypeAccel))
                    {
                        struct Sector  *sec_ref     = anim.scroll_sector_reference;
                        Line           *ld          = anim.target;
                        const LineType *special_ref = anim.scroll_special_reference;
                        Line           *line_ref    = anim.scroll_line_reference;

                        if (!ld || !special_ref || !line_ref)
                            continue;

                        if (special_ref->line_effect_ & kLineEffectTypeVectorScroll)
                        {
                            float tdx       = anim.dynamic_delta_x;
                            float tdy       = anim.dynamic_delta_y;
                            float heightref = special_ref->scroll_type_ & BoomScrollerTypeDisplace
                                                  ? anim.last_height
                                                  : sec_ref->original_height;
                            float sy        = tdy * ((sec_ref->floor_height + sec_ref->ceiling_height) - heightref);
                            float sx        = tdx * ((sec_ref->floor_height + sec_ref->ceiling_height) - heightref);
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
                            float x_speed   = anim.side_0_x_offset_speed;
                            float y_speed   = anim.side_0_y_offset_speed;
                            float heightref = special_ref->scroll_type_ & BoomScrollerTypeDisplace
                                                  ? anim.last_height
                                                  : sec_ref->original_height;
                            float sy        = x_speed * ((sec_ref->floor_height + sec_ref->ceiling_height) - heightref);
                            float sx        = y_speed * ((sec_ref->floor_height + sec_ref->ceiling_height) - heightref);
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
            {
                pmov->sector->ceiling_move                = nullptr;
                pmov->sector->old_ceiling_height          = pmov->sector->ceiling_height;
                pmov->sector->interpolated_ceiling_height = pmov->sector->ceiling_height;
            }

            if (!pmov->is_ceiling)
            {
                pmov->sector->floor_move                = nullptr;
                pmov->sector->old_floor_height          = pmov->sector->floor_height;
                pmov->sector->interpolated_floor_height = pmov->sector->floor_height;
            }

            *PMI = nullptr;
            delete pmov;

            removed_plane = true;
        }
    }

    if (removed_plane)
    {
        std::vector<PlaneMover *>::iterator ENDP;

        ENDP = std::remove(active_planes.begin(), active_planes.end(), (PlaneMover *)nullptr);

        active_planes.erase(ENDP, active_planes.end());
    }
}

void RunActiveSliders(void)
{
    if (time_stop_active)
        return;

    std::vector<SlidingDoorMover *>::iterator SMI;

    bool removed_slider = false;

    for (SMI = active_sliders.begin(); SMI != active_sliders.end(); SMI++)
    {
        SlidingDoorMover *smov = *SMI;

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
        std::vector<SlidingDoorMover *>::iterator ENDP;

        ENDP = std::remove(active_sliders.begin(), active_sliders.end(), (SlidingDoorMover *)nullptr);

        active_sliders.erase(ENDP, active_sliders.end());
    }
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
