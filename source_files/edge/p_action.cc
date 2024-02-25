//----------------------------------------------------------------------------
//  EDGE Play Simulation Action routines
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
//
// Notes:
//  All Procedures here are never called directly, except possibly
//  by another P_Act* Routine. Otherwise the procedure is called
//  by referencing an code pointer from the states[] table. The only
//  exception to these rules are P_ActMissileContact and
//  P_SlammedIntoObject that requiring "acting" on the part
//  of an obj.
//
// This file was created for all action code by DDF.
//
// -KM- 1998/09/27 Added sounds.ddf capability
// -KM- 1998/12/21 New smooth visibility.
// -AJA- 1999/07/21: Replaced some non-critical Random8BitStatefuls with Random8BitStateless.
// -AJA- 1999/08/08: Replaced some Random8BitStateful()-Random8BitStateful() stuff.
//


#include "p_action.h"

#include "con_main.h"
#include "dm_defs.h"
#include "dm_state.h"
#include "g_game.h"
#include "i_system.h"
#include "m_misc.h"
#include "m_random.h"
#include "p_local.h"
#include "p_weapon.h"
#include "r_misc.h"
#include "r_state.h"
#include "rad_trig.h"
#include "s_sound.h"
#include "w_wad.h"
#include "f_interm.h" // intermission_stats

#include "AlmostEquals.h"

extern FlatDefinition *P_IsThingOnLiquidFloor(mobj_t *thing);

static int AttackSfxCat(const mobj_t *mo)
{
    int category = P_MobjGetSfxCategory(mo);

    if (category == SNCAT_Player)
        return SNCAT_Weapon;

    return category;
}

static int SfxFlags(const MapObjectDefinition *info)
{
    int flags = 0;

    if (info->extendedflags_ & kExtendedFlagAlwaysLoud)
        flags |= FX_Boss;

    return flags;
}

//-----------------------------------------
//--------------MISCELLANOUS---------------
//-----------------------------------------

//
// P_ActActivateLineType
//
// Allows things to also activate linetypes, bringing them into the
// fold with radius triggers, which can also do it.  There's only two
// parameters needed: linetype number & tag number, which are stored
// in the state's `action_par' field as a pointer to two integers.
//
void P_ActActivateLineType(mobj_t *mo)
{
    int *values;

    if (!mo->state || !mo->state->action_par)
        return;

    values = (int *)mo->state->action_par;

    // Note the `nullptr' here: this prevents the activation from failing
    // because the object isn't a PLAYER, for example.
    P_RemoteActivation(nullptr, values[0], values[1], 0, kLineTriggerAny);
}

//
// P_ActEnableRadTrig
// P_ActDisableRadTrig
//
// Allows things to enable or disable radius triggers (by tag number),
// like linetypes can do already.
//
void P_ActEnableRadTrig(mobj_t *mo)
{
    if (!mo->state || !mo->state->action_par)
        return;

    int *value = (int *)mo->state->action_par;

    RAD_EnableByTag(mo, value[0], false, (s_tagtype_e)mo->state->rts_tag_type);
}

void P_ActDisableRadTrig(mobj_t *mo)
{
    if (!mo->state || !mo->state->action_par)
        return;

    int *value = (int *)mo->state->action_par;

    RAD_EnableByTag(mo, value[0], true, (s_tagtype_e)mo->state->rts_tag_type);
}

//
// P_ActLookForTargets
//
// Looks for targets: used in the same way as enemy things look
// for players
//
// TODO: Write a decent procedure.
// -KM- 1999/01/31 Added sides. Still has to search every mobj on the
//  map to find a target.  There must be a better way...
// -AJA- 2004/04/28: Rewritten. Mobjs on same side are never targeted.
//
// NOTE: a better way might be: do a mini "BSP render", use a small 1D
//       occlusion buffer (e.g. 64 bits).
//
bool P_ActLookForTargets(mobj_t *we)
{
    mobj_t *them;

    // Optimisation: nobody to support when side is zero
    if (we->side == 0)
        return P_LookForPlayers(we, we->info->sight_angle_);

    for (them = mobjlisthead; them != nullptr; them = them->next)
    {
        if (them == we)
            continue;

        bool same_side = ((them->side & we->side) != 0);

        // only target monsters or players (not barrels)
        if (!(them->extendedflags & kExtendedFlagMonster) && !them->player)
            continue;

        if (!(them->flags & kMapObjectFlagShootable))
            continue;

        if (same_side && !we->supportobj && them->supportobj != we)
        {
            if (them->supportobj && P_CheckSight(we, them->supportobj))
                them = them->supportobj;
            else if (!P_CheckSight(we, them))
                continue; // OK since same side

            if (them)
            {
                we->SetSupportObj(them);
                if (we->info->meander_state_)
                    P_SetMobjStateDeferred(we, we->info->meander_state_, 0);
                return true;
            }
        }

        if (same_side)
            continue;

        /// if (them == we->supportobj || we == them->supportobj ||
        ///	(them->supportobj && them->supportobj == we->supportobj))
        ///	continue;

        if ((we->info == them->info) && !(we->extendedflags & kExtendedFlagDisloyalToOwnType))
            continue;

        /// -AJA- IDEALLY: use this to prioritize possible targets.
        /// if (! (them->target &&
        ///	  (them->target == we || them->target == we->supportobj)))
        ///	continue;

        if (P_CheckSight(we, them))
        {
            we->SetTarget(them);
            if (we->info->chase_state_)
                P_SetMobjStateDeferred(we, we->info->chase_state_, 0);
            return true;
        }
    }

    return false;
}

//
// DecideMeleeAttack
//
// This is based on P_CheckMeleeRange, except that it relys upon
// info from the objects close combat attack, the original code
// used a set value for all objects which was MELEERANGE + 20,
// this code allows different melee ranges for different objects.
//
// -ACB- 1998/08/15
// -KM- 1998/11/25 Added attack parameter.
//
static bool DecideMeleeAttack(mobj_t *object, const AttackDefinition *attack)
{
    mobj_t *target;
    float   distance;
    float   meleedist;

    target = object->target;

    if (!target)
        return false;

    distance = P_ApproxDistance(target->x - object->x, target->y - object->y);

    if (level_flags.true3dgameplay)
        distance = P_ApproxDistance(target->z - object->z, distance);

    if (attack)
        meleedist = attack->range_;
    else
    {
        meleedist = MELEERANGE;
        if (object->mbf21flags & kMBF21FlagLongMeleeRange)
            meleedist = LONGMELEERANGE;
        // I guess a specific MBF21 Thing Melee range should override the above choices?
        if (object->info->melee_range_ > -1)
            meleedist = object->info->melee_range_;
    }
    meleedist += target->radius - 20.0f; // Check the thing's actual radius

    if (distance >= meleedist)
        return false;

    return P_CheckSight(object, target);
}

//
// DecideRangeAttack
//
// This is based on P_CheckMissileRange, contrary the name it does more
// than check the missile range, it makes a decision of whether or not an
// attack should be made or not depending on the object with the option
// to attack. A return of false is mandatory if the object cannot see its
// target (makes sense, doesn't it?), after this the distance is calculated,
// it will eventually be check to see if it is greater than a number from
// the Random Number Generator; if so the procedure returns true. Essentially
// the closer the object is to its target, the more chance an attack will
// be made (another logical decision).
//
// -ACB- 1998/08/15
//
static bool DecideRangeAttack(mobj_t *object)
{
    float       chance;
    float           distance;
    const AttackDefinition *attack;

    if (!object->target)
        return false;

    if (object->info->rangeattack_)
        attack = object->info->rangeattack_;
    else
        return false; // cannot evaluate range with no attack range

    // Just been hit (and have felt pain), so in true tit-for-tat
    // style, the object - without regard to anything else - hits back.
    if (object->flags & kMapObjectFlagJustHit)
    {
        if (!P_CheckSight(object, object->target))
            return false;

        object->flags &= ~kMapObjectFlagJustHit;
        return true;
    }

    // Bit slow on the up-take: the object hasn't had time to
    // react his target.
    if (object->reactiontime)
        return false;

    // Get the distance, a basis for our decision making from now on
    distance = P_ApproxDistance(object->x - object->target->x, object->y - object->target->y);

    // If no close-combat attack, increase the chance of a missile attack
    if (!object->info->melee_state_)
        distance -= 192;
    else
        distance -= 64;

    // Object is too far away to attack?
    if (attack->range_ && distance >= attack->range_)
        return false;

    // MBF21 SHORTMRANGE flag
    if ((object->mbf21flags & kMBF21FlagShortMissileRange) && distance >= SHORTMISSILERANGE)
        return false;

    // Object is too close to target
    if (attack->tooclose_ && attack->tooclose_ >= distance)
        return false;

    // Object likes to fire? if so, double the chance of it happening
    if (object->extendedflags & kExtendedFlagTriggerHappy)
        distance /= 2;

    // The chance in the object is one given that the attack will happen, so
    // we inverse the result (since its one in 255) to get the chance that
    // the attack will not happen.
    chance = 1.0f - object->info->minatkchance_;
    chance = HMM_MIN(distance / 255.0f, chance);

    // now after modifing distance where applicable, we get the random number and
    // check if it is less than distance, if so no attack is made.
    if (Random8BitTestStateful(chance))
        return false;

    return P_CheckSight(object, object->target);
}

//
// P_ActFaceTarget
//
// Look at the prey......
//
void P_ActFaceTarget(mobj_t *object)
{
    mobj_t *target = object->target;

    if (!target)
        return;

    if (object->flags & kMapObjectFlagStealth)
        object->vis_target = VISIBLE;

    object->flags &= ~kMapObjectFlagAmbush;

    object->angle = R_PointToAngle(object->x, object->y, target->x, target->y);

    float dist = R_PointToDist(object->x, object->y, target->x, target->y);

    if (dist >= 0.1f)
    {
        float dz = MO_MIDZ(target) - MO_MIDZ(object);

        object->vertangle = epi::BAMFromATan(dz / dist);
    }

    if (target->flags & kMapObjectFlagFuzzy)
    {
        object->angle += Random8BitSkewToZeroStateful() << (kBAMAngleBits - 11);
        object->vertangle += epi::BAMFromATan(Random8BitSkewToZeroStateful() / 1024.0f);
    }

    if (target->visibility < VISIBLE)
    {
        float amount = (VISIBLE - target->visibility);

        object->angle += (BAMAngle)(Random8BitSkewToZeroStateful() * (kBAMAngleBits - 12) * amount);
        object->vertangle += epi::BAMFromATan(Random8BitSkewToZeroStateful() * amount / 2048.0f);
    }

    // don't look up/down too far...
    if (object->vertangle < kBAMAngle180 && object->vertangle > kBAMAngle45)
        object->vertangle = kBAMAngle45;

    if (object->vertangle >= kBAMAngle180 && object->vertangle < kBAMAngle315)
        object->vertangle = kBAMAngle315;
}

//
// P_ForceFaceTarget
//
// FaceTarget, but ignoring visibility modifiers
//
void P_ForceFaceTarget(mobj_t *object)
{
    mobj_t *target = object->target;

    if (!target)
        return;

    if (object->flags & kMapObjectFlagStealth)
        object->vis_target = VISIBLE;

    object->flags &= ~kMapObjectFlagAmbush;

    object->angle = R_PointToAngle(object->x, object->y, target->x, target->y);

    float dist = R_PointToDist(object->x, object->y, target->x, target->y);

    if (dist >= 0.1f)
    {
        float dz = MO_MIDZ(target) - MO_MIDZ(object);

        object->vertangle = epi::BAMFromATan(dz / dist);
    }

    // don't look up/down too far...
    if (object->vertangle < kBAMAngle180 && object->vertangle > kBAMAngle45)
        object->vertangle = kBAMAngle45;

    if (object->vertangle >= kBAMAngle180 && object->vertangle < kBAMAngle315)
        object->vertangle = kBAMAngle315;
}

void P_ActMakeIntoCorpse(mobj_t *mo)
{
    // Gives the effect of the object being a corpse....

    if (mo->flags & kMapObjectFlagStealth)
        mo->vis_target = VISIBLE; // dead and very visible

    // object is on ground, it can be walked over
    mo->flags &= ~kMapObjectFlagSolid;

    mo->tag = 0;

    P_HitLiquidFloor(mo);
}

void P_BringCorpseToLife(mobj_t *corpse)
{
    // Bring a corpse back to life (the opposite of the above routine).
    // Handles players too !

    const MapObjectDefinition *info = corpse->info;

    corpse->flags         = info->flags_;
    corpse->health        = corpse->spawnhealth;
    corpse->radius        = info->radius_;
    corpse->height        = info->height_;
    corpse->extendedflags = info->extendedflags_;
    corpse->hyperflags    = info->hyperflags_;
    corpse->vis_target    = info->translucency_;
    // UDMF check
    if (!AlmostEquals(corpse->alpha, 1.0f))
        corpse->vis_target = corpse->alpha;
    corpse->tag           = corpse->spawnpoint.tag;

    corpse->flags &= ~kMapObjectFlagCountKill; // Lobo 2023: don't add to killcount

    if (corpse->player)
    {
        corpse->player->playerstate    = PST_LIVE;
        corpse->player->health         = corpse->health;
        corpse->player->std_viewheight = corpse->height * info->viewheight_;
    }

    if (info->overkill_sound_)
        S_StartFX(info->overkill_sound_, P_MobjGetSfxCategory(corpse), corpse);

    if (info->raise_state_)
        P_SetMobjState(corpse, info->raise_state_);
    else if (info->meander_state_)
        P_SetMobjState(corpse, info->meander_state_);
    else if (info->idle_state_)
        P_SetMobjState(corpse, info->idle_state_);
    else
        FatalError("Object %s has no RESURRECT states.\n", info->name_.c_str());
}

void P_ActResetSpreadCount(mobj_t *mo)
{
    // Resets the spreader count for fixed-order spreaders, normally used
    // at the beginning of a set of missile states to ensure that an object
    // fires in the same object each time.

    mo->spreadcount = 0;
}

//-------------------------------------------------------------------
//-------------------VISIBILITY HANDLING ROUTINES--------------------
//-------------------------------------------------------------------

void P_ActTransSet(mobj_t *mo)
{
    float value = VISIBLE;

    const State *st = mo->state;

    if (st && st->action_par)
    {
        value = ((float *)st->action_par)[0];
        value = HMM_MAX(0.0f, HMM_MIN(1.0f, value));
    }

    mo->visibility = mo->vis_target = value;
}

void P_ActTransFade(mobj_t *mo)
{
    float value = INVISIBLE;

    const State *st = mo->state;

    if (st && st->action_par)
    {
        value = ((float *)st->action_par)[0];
        value = HMM_MAX(0.0f, HMM_MIN(1.0f, value));
    }

    mo->vis_target = value;
}

void P_ActTransLess(mobj_t *mo)
{
    float value = 0.05f;

    const State *st = mo->state;

    if (st && st->action_par)
    {
        value = ((float *)st->action_par)[0];
        value = HMM_MAX(0.0f, HMM_MIN(1.0f, value));
    }

    mo->vis_target -= value;

    if (mo->vis_target < INVISIBLE)
        mo->vis_target = INVISIBLE;
}

void P_ActTransMore(mobj_t *mo)
{
    float value = 0.05f;

    const State *st = mo->state;

    if (st && st->action_par)
    {
        value = ((float *)st->action_par)[0];
        value = HMM_MAX(0.0f, HMM_MIN(1.0f, value));
    }

    mo->vis_target += value;

    if (mo->vis_target > VISIBLE)
        mo->vis_target = VISIBLE;
}

//
// P_ActTransAlternate
//
// Alters the translucency of an item, kExtendedFlagLessVisible is used
// internally to tell the object if it should be getting
// more visible or less visible; kExtendedFlagLessVisible is set when an
// object is to get less visible (because it has become
// to a level of lowest translucency) and the flag is unset
// if the object has become as highly translucent as possible.
//
void P_ActTransAlternate(mobj_t *object)
{
    const State *st;
    float          value = 0.05f;

    st = object->state;

    if (st && st->action_par)
    {
        value = ((float *)st->action_par)[0];
        value = HMM_MAX(0.0f, HMM_MIN(1.0f, value));
    }

    if (object->extendedflags & kExtendedFlagLessVisible)
    {
        object->vis_target -= value;
        if (object->vis_target <= INVISIBLE)
        {
            object->vis_target = INVISIBLE;
            object->extendedflags &= ~kExtendedFlagLessVisible;
        }
    }
    else
    {
        object->vis_target += value;
        if (object->vis_target >= VISIBLE)
        {
            object->vis_target = VISIBLE;
            object->extendedflags |= kExtendedFlagLessVisible;
        }
    }
}

void P_ActDLightSet(mobj_t *mo)
{
    const State *st = mo->state;

    if (st && st->action_par)
    {
        mo->dlight.r = HMM_MAX(0.0f, ((int *)st->action_par)[0]);

        if (mo->info->hyperflags_ & kHyperFlagQuadraticDynamicLight)
            mo->dlight.r = DynamicLightCompatibilityRadius(mo->dlight.r);

        mo->dlight.target = mo->dlight.r;
    }
}

void P_ActDLightFade(mobj_t *mo)
{
    const State *st = mo->state;

    if (st && st->action_par)
    {
        mo->dlight.target = HMM_MAX(0.0f, ((int *)st->action_par)[0]);

        if (mo->info->hyperflags_ & kHyperFlagQuadraticDynamicLight)
            mo->dlight.target = DynamicLightCompatibilityRadius(mo->dlight.target);
    }
}

void P_ActDLightRandom(mobj_t *mo)
{
    const State *st = mo->state;

    if (st && st->action_par)
    {
        int low  = ((int *)st->action_par)[0];
        int high = ((int *)st->action_par)[1];

        // Note: using Random8BitStateless so that gameplay is unaffected
        float qty = low + (high - low) * Random8BitStateless() / 255.0f;

        if (mo->info->hyperflags_ & kHyperFlagQuadraticDynamicLight)
            qty = DynamicLightCompatibilityRadius(qty);

        mo->dlight.r      = HMM_MAX(0.0f, qty);
        mo->dlight.target = mo->dlight.r;
    }
}

void P_ActDLightColour(struct mobj_s *mo)
{
    const State *st = mo->state;

    if (st && st->action_par)
    {
        mo->dlight.color = ((RGBAColor *)st->action_par)[0];
    }
}

void P_ActSetSkin(mobj_t *mo)
{
    const State *st = mo->state;

    if (st && st->action_par)
    {
        int skin = ((int *)st->action_par)[0];

        if (skin < 0 || skin > 9)
            FatalError("Thing [%s]: Bad skin number %d in SET_SKIN action.\n", mo->info->name_.c_str(), skin);

        mo->model_skin = skin;
    }
}

//-------------------------------------------------------------------
//------------------- MOVEMENT ROUTINES -----------------------------
//-------------------------------------------------------------------

void P_ActFaceDir(mobj_t *mo)
{
    const State *st = mo->state;

    if (st && st->action_par)
        mo->angle = *(BAMAngle *)st->action_par;
    else
        mo->angle = 0;
}

void P_ActTurnDir(mobj_t *mo)
{
    const State *st = mo->state;

    BAMAngle turn = kBAMAngle180;

    if (st && st->action_par)
        turn = *(BAMAngle *)st->action_par;

    mo->angle += turn;
}

void P_ActTurnRandom(mobj_t *mo)
{
    const State *st   = mo->state;
    int            turn = 359;

    if (st && st->action_par)
    {
        turn = (int)epi::DegreesFromBAM(*(BAMAngle *)st->action_par);
    }

    turn = turn * Random8BitStateful() / 90; // 10 bits of angle

    if (turn < 0)
        mo->angle -= (BAMAngle)((-turn) << (kBAMAngleBits - 10));
    else
        mo->angle += (BAMAngle)(turn << (kBAMAngleBits - 10));
}

void P_ActMlookFace(mobj_t *mo)
{
    const State *st = mo->state;

    if (st && st->action_par)
        mo->vertangle = epi::BAMFromATan(*(float *)st->action_par);
    else
        mo->vertangle = 0;
}

void P_ActMlookTurn(mobj_t *mo)
{
    const State *st = mo->state;

    if (st && st->action_par)
        mo->vertangle = epi::BAMFromATan(*(float *)st->action_par);
}

void P_ActMoveFwd(mobj_t *mo)
{
    const State *st = mo->state;

    if (st && st->action_par)
    {
        float amount = *(float *)st->action_par;

        float dx = epi::BAMCos(mo->angle);
        float dy = epi::BAMSin(mo->angle);

        mo->mom.X += dx * amount;
        mo->mom.Y += dy * amount;
    }
}

void P_ActMoveRight(mobj_t *mo)
{
    const State *st = mo->state;

    if (st && st->action_par)
    {
        float amount = *(float *)st->action_par;

        float dx = epi::BAMCos(mo->angle - kBAMAngle90);
        float dy = epi::BAMSin(mo->angle - kBAMAngle90);

        mo->mom.X += dx * amount;
        mo->mom.Y += dy * amount;
    }
}

void P_ActMoveUp(mobj_t *mo)
{
    const State *st = mo->state;

    if (st && st->action_par)
        mo->mom.Z += *(float *)st->action_par;
}

void P_ActStopMoving(mobj_t *mo)
{
    mo->mom.X = mo->mom.Y = mo->mom.Z = 0;
}

//-------------------------------------------------------------------
//-------------------SOUND CAUSING ROUTINES--------------------------
//-------------------------------------------------------------------

void P_ActPlaySound(mobj_t *mo)
{
    // Generate an arbitrary sound.

    SoundEffect *sound = nullptr;

    if (mo->state && mo->state->action_par)
        sound = (SoundEffect *)mo->state->action_par;

    if (!sound)
    {
        PrintWarningOrError("P_ActPlaySound: missing sound name in %s.\n", mo->info->name_.c_str());
        return;
    }

    // S_StartFX(sound, P_MobjGetSfxCategory(mo), mo, SfxFlags(mo->info));
    S_StartFX(sound, P_MobjGetSfxCategory(mo), mo);
}

// Same as above but always loud
void P_ActPlaySoundBoss(mobj_t *mo)
{
    // Generate an arbitrary sound.

    SoundEffect *sound = nullptr;

    if (mo->state && mo->state->action_par)
        sound = (SoundEffect *)mo->state->action_par;

    if (!sound)
    {
        PrintWarningOrError("P_ActPlaySoundBoss: missing sound name in %s.\n", mo->info->name_.c_str());
        return;
    }

    int flags = 0;
    flags |= FX_Boss;

    S_StartFX(sound, P_MobjGetSfxCategory(mo), mo, flags);
}

void P_ActKillSound(mobj_t *mo)
{
    // Kill any current sounds from this thing.

    S_StopFX(mo);
}

void P_ActMakeAmbientSound(mobj_t *mo)
{
    // Just a sound generating procedure that cause the sound ref
    // in seesound_ to be generated.

    if (mo->info->seesound_)
        S_StartFX(mo->info->seesound_, P_MobjGetSfxCategory(mo), mo);
    else
        LogDebug("%s has no ambient sound\n", mo->info->name_.c_str());
}

void P_ActMakeAmbientSoundRandom(mobj_t *mo)
{
    // Give a small "random" chance that this object will make its
    // ambient sound. Currently this is a set value of 50, however
    // the code that drives this, should allow for the user to set
    // the value, note for further DDF Development.

    if (mo->info->seesound_)
    {
        if (Random8BitStateless() < 50)
            S_StartFX(mo->info->seesound_, P_MobjGetSfxCategory(mo), mo);
    }
    else
        LogDebug("%s has no ambient sound\n", mo->info->name_.c_str());
}

void P_ActMakeActiveSound(mobj_t *mo)
{
    // Just a sound generating procedure that cause the sound ref
    // in activesound_ to be generated.
    //
    // -KM- 1999/01/31

    if (mo->info->activesound_)
        S_StartFX(mo->info->activesound_, P_MobjGetSfxCategory(mo), mo);
    else
        LogDebug("%s has no ambient sound\n", mo->info->name_.c_str());
}

void P_ActMakeDyingSound(mobj_t *mo)
{
    // This procedure is like every other sound generating
    // procedure with the exception that if the object is
    // a boss (kExtendedFlagAlwaysLoud extended flag) then the sound is
    // generated at full volume (source = nullptr).

    SoundEffect *sound = mo->info->deathsound_;

    if (sound)
        S_StartFX(sound, P_MobjGetSfxCategory(mo), mo, SfxFlags(mo->info));
    else
        LogDebug("%s has no death sound\n", mo->info->name_.c_str());
}

void P_ActMakePainSound(mobj_t *mo)
{
    // Ow!! it hurts!

    if (mo->info->painsound_)
        S_StartFX(mo->info->painsound_, P_MobjGetSfxCategory(mo), mo, SfxFlags(mo->info));
    else
        LogDebug("%s has no pain sound\n", mo->info->name_.c_str());
}

void P_ActMakeOverKillSound(mobj_t *mo)
{
    if (mo->info->overkill_sound_)
        S_StartFX(mo->info->overkill_sound_, P_MobjGetSfxCategory(mo), mo, SfxFlags(mo->info));
    else
        LogDebug("%s has no overkill sound\n", mo->info->name_.c_str());
}

void P_ActMakeCloseAttemptSound(mobj_t *mo)
{
    // Attempting close combat sound

    if (!mo->info->closecombat_)
        FatalError("Object [%s] used CLOSEATTEMPTSND action, "
                "but has no CLOSE_ATTACK\n",
                mo->info->name_.c_str());

    SoundEffect *sound = mo->info->closecombat_->initsound_;

    if (sound)
        S_StartFX(sound, P_MobjGetSfxCategory(mo), mo);
    else
        LogDebug("%s has no close combat attempt sound\n", mo->info->name_.c_str());
}

void P_ActMakeRangeAttemptSound(mobj_t *mo)
{
    // Attempting range attack sound

    if (!mo->info->rangeattack_)
        FatalError("Object [%s] used RANGEATTEMPTSND action, "
                "but has no RANGE_ATTACK\n",
                mo->info->name_.c_str());

    SoundEffect *sound = mo->info->rangeattack_->initsound_;

    if (sound)
        S_StartFX(sound, P_MobjGetSfxCategory(mo), mo);
    else
        LogDebug("%s has no range attack attempt sound\n", mo->info->name_.c_str());
}

//-------------------------------------------------------------------
//-------------------EXPLOSION DAMAGE ROUTINES-----------------------
//-------------------------------------------------------------------

//
// P_ActDamageExplosion
//
// Radius Attack damage set by info->damage. Used for the original Barrels
//
void P_ActDamageExplosion(mobj_t *object)
{
    float damage;

    DAMAGE_COMPUTE(damage, &object->info->explode_damage_);

#ifdef DEVELOPERS
    if (!damage)
    {
        LogDebug("%s caused no explosion damage\n", object->info->name.c_str());
        return;
    }
#endif

    // -AJA- 2004/09/27: new EXPLODE_RADIUS command (overrides normal calc)
    float radius = object->info->explode_radius_;
    if (AlmostEquals(radius, 0.0f))
        radius = damage;

    P_RadiusAttack(object, object->source, radius, damage, &object->info->explode_damage_, false);
}

//
// P_ActThrust
//
// Thrust set by info->damage.
//
void P_ActThrust(mobj_t *object)
{
    float damage;

    DAMAGE_COMPUTE(damage, &object->info->explode_damage_);

#ifdef DEVELOPERS
    if (!damage)
    {
        LogDebug("%s caused no thrust\n", object->info->name.c_str());
        return;
    }
#endif

    float radius = object->info->explode_radius_;
    if (AlmostEquals(radius, 0.0f))
        radius = damage;

    P_RadiusAttack(object, object->source, radius, damage, &object->info->explode_damage_, true);
}

//-------------------------------------------------------------------
//-------------------MISSILE HANDLING ROUTINES-----------------------
//-------------------------------------------------------------------

//
// P_ActExplode
//
// The object blows up, like a missile.
//
// -AJA- 1999/08/21: Replaced P_ActExplodeMissile (which was identical
//       to p_mobj's P_ExplodeMissile) with this.
//
void P_ActExplode(mobj_t *object)
{
    P_MobjExplodeMissile(object);
}

//
// P_ActCheckMissileSpawn
//
// This procedure handles a newly spawned missile, it moved
// by half the amount of momentum and then checked to see
// if the move is possible, if not the projectile is
// exploded. Also the number of initial tics on its
// current state is taken away from by a random number
// between 0 and 3, although the number of tics will never
// go below 1.
//
// -ACB- 1998/08/04
//
// -AJA- 1999/08/22: Fixed a bug that occasionally caused the game to
//       go into an infinite loop.  NOTE WELL: don't fiddle with the
//       object's x & y directly, use P_TryMove instead, or
//       P_ChangeThingPosition.
//
static void CheckMissileSpawn(mobj_t *projectile)
{
    projectile->tics -= Random8BitStateful() & 3;

    if (projectile->tics < 1)
        projectile->tics = 1;

    projectile->z += projectile->mom.Z / 2;

    if (!P_TryMove(projectile, projectile->x + projectile->mom.X / 2, projectile->y + projectile->mom.Y / 2))
    {
        P_MobjExplodeMissile(projectile);
    }
}

//
// LaunchProjectile
//
// This procedure launches a project the direction of the target mobj.
// * source - the source of the projectile, required
// * target - the target of the projectile, can be nullptr
// * type   - the mobj type of the projectile
//
// For all sense and purposes it is possible for the target to be a dummy
// mobj, just to act as a carrier for a set of target co-ordinates.
//
// Missiles can be spawned at different locations on and around
// the mobj. Traditionally an mobj would fire a projectile
// at a height of 32 from the centerpoint of that
// mobj, this was true for all creatures from the Cyberdemon to
// the Imp. The currentattack holds the height and x & y
// offsets that dictates the spawning location of a projectile.
//
// Traditionally: Height   = 4*8
//                x-offset = 0
//                y-offset = 0
//
// The exception to this rule is the revenant, which did increase
// its z value by 16 before firing: This was a hack
// to launch a missile at a height of 48. The revenants
// height was reduced to normal after firing, this new code
// makes that an unnecesary procedure.
//
// projx, projy & projz are the projectiles spawn location
//
// NOTE: may return nullptr.
//
static mobj_t *DoLaunchProjectile(mobj_t *source, float tx, float ty, float tz, mobj_t *target, const MapObjectDefinition *type)
{
    const AttackDefinition *attack = source->currentattack;

    if (!attack)
        return nullptr;

    // -AJA- projz now handles crouching
    float     projx          = source->x;
    float     projy          = source->y;
    float     projz          = source->z + attack->height_ * source->height / source->info->height_;
    sector_t *cur_source_sec = source->subsector->sector;

    if (source->player)
        projz += (source->player->viewz - source->player->std_viewheight);
    else if (cur_source_sec->sink_depth > 0 && !cur_source_sec->exfloor_used && !cur_source_sec->heightsec &&
             abs(source->z - cur_source_sec->f_h) < 1)
        projz -= (source->height * 0.5 * cur_source_sec->sink_depth);

    BAMAngle angle = source->angle;

    projx += attack->xoffset_ * epi::BAMCos(angle + kBAMAngle90);
    projy += attack->xoffset_ * epi::BAMSin(angle + kBAMAngle90);

    float yoffset;

    if (attack->yoffset_)
        yoffset = attack->yoffset_;
    else
        yoffset = source->radius - 0.5f;

    projx += yoffset * epi::BAMCos(angle) * epi::BAMCos(source->vertangle);
    projy += yoffset * epi::BAMSin(angle) * epi::BAMCos(source->vertangle);
    projz += yoffset * epi::BAMSin(source->vertangle);

    mobj_t *projectile = P_MobjCreateObject(projx, projy, projz, type);

    // currentattack is held so that when a collision takes place
    // with another object, we know whether or not the object hit
    // can shake off the attack or is damaged by it.
    //
    projectile->currentattack = attack;
    projectile->SetRealSource(source);

    // check for blocking lines between source and projectile
    if (P_MapCheckBlockingLine(source, projectile))
    {
        P_MobjExplodeMissile(projectile);
        return nullptr;
    }

    // launch sound
    if (projectile->info && projectile->info->seesound_)
    {
        int category = AttackSfxCat(source);
        int flags    = SfxFlags(projectile->info);

        mobj_t *sfx_source = projectile;
        if (category == SNCAT_Player || category == SNCAT_Weapon)
            sfx_source = source;

        S_StartFX(projectile->info->seesound_, category, sfx_source, flags);
    }

    angle = R_PointToAngle(projx, projy, tx, ty);

    // Now add the fact that the target may be difficult to spot and
    // make the projectile's target the same as the sources. Only
    // do these if the object is not a dummy object, otherwise just
    // flag the missile not to trace: you cannot track a target that
    // does not exist...

    projectile->SetTarget(target);

    if (!target)
    {
        tz += attack->height_;
    }
    else
    {
        projectile->extendedflags |= kExtendedFlagFirstTracerCheck;

        if (!(attack->flags_ & kAttackFlagPlayer))
        {
            if (target->flags & kMapObjectFlagFuzzy)
                angle += Random8BitSkewToZeroStateful() << (kBAMAngleBits - 12);

            if (target->visibility < VISIBLE)
                angle += (BAMAngle)(Random8BitSkewToZeroStateful() * 64 * (VISIBLE - target->visibility));
        }

        sector_t *cur_target_sec = target->subsector->sector;

        if (cur_target_sec->sink_depth > 0 && !cur_target_sec->exfloor_used && !cur_target_sec->heightsec &&
            abs(target->z - cur_target_sec->f_h) < 1)
            tz -= (target->height * 0.5 * cur_target_sec->sink_depth);
    }

    // Calculate slope
    float slope = P_ApproxSlope(tx - projx, ty - projy, tz - projz);

    // -AJA- 1999/09/11: add in attack's angle & slope offsets.
    angle -= attack->angle_offset_;
    slope += attack->slope_offset_;

    // is the attack not accurate?
    if (!source->player || source->player->refire > 0)
    {
        if (attack->accuracy_angle_ > 0.0f)
            angle += (attack->accuracy_angle_ >> 8) * Random8BitSkewToZeroStateful();

        if (attack->accuracy_slope_ > 0.0f)
            slope += attack->accuracy_slope_ * (Random8BitSkewToZeroStateful() / 255.0f);
    }

    P_SetMobjDirAndSpeed(projectile, angle, slope, projectile->speed);
    if (projectile->flags & kMapObjectFlagPreserveMomentum)
    {
        projectile->mom.X += source->mom.X;
        projectile->mom.Y += source->mom.Y;
        projectile->mom.Z += source->mom.Z;
    }
    CheckMissileSpawn(projectile);

    return projectile;
}

static mobj_t *LaunchProjectile(mobj_t *source, mobj_t *target, const MapObjectDefinition *type)
{
    float tx, ty, tz;

    if (source->currentattack && (source->currentattack->flags_ & kAttackFlagNoTarget))
        target = nullptr;

    P_TargetTheory(source, target, &tx, &ty, &tz);

    return DoLaunchProjectile(source, tx, ty, tz, target, type);
}

//
// LaunchSmartProjectile
//
// This procedure has the same effect as
// LaunchProjectile, but it calculates a point where the target
// and missile will intersect.  This comes from the fact that to shoot
// something, you have to aim slightly ahead of it.  It will also put
// an end to circle-strafing.  :-)
//
// -KM- 1998/10/29
// -KM- 1998/12/16 Fixed it up.  Works quite well :-)
//
static void LaunchSmartProjectile(mobj_t *source, mobj_t *target, const MapObjectDefinition *type)
{
    float t  = -1;
    float mx = 0, my = 0;

    if (target)
    {
        mx = target->mom.X;
        my = target->mom.Y;

        float dx = source->x - target->x;
        float dy = source->y - target->y;

        float s = type->speed_;
        if (level_flags.fastparm && type->fast_speed_ > -1)
            s = type->fast_speed_;

        float a = mx * mx + my * my - s * s;
        float b = 2 * (dx * mx + dy * my);
        float c = dx * dx + dy * dy;

        float t1 = -1, t2 = -1;

        // find solution to the quadratic equation
        if (a && ((b * b - 4 * a * c) >= 0))
        {
            t1 = -b + (float)sqrt(b * b - 4.0f * a * c);
            t1 /= 2.0f * a;

            t2 = -b - (float)sqrt(b * b - 4.0f * a * c);
            t2 /= 2.0f * a;
        }

        if (t1 < 0)
            t = t2;
        else if (t2 < 0)
            t = t1;
        else
            t = (t1 < t2) ? t1 : t2;
    }

    if (t <= 0)
    {
        // -AJA- when no target, fall back to "dumb mode"
        LaunchProjectile(source, target, type);
    }
    else
    {
        // -AJA- 2005/02/07: assumes target doesn't move up or down

        float tx = target->x + mx * t;
        float ty = target->y + my * t;
        float tz = MO_MIDZ(target);

        DoLaunchProjectile(source, tx, ty, tz, target, type);

#if 0 // -AJA- this doesn't seem correct / consistent
		if (projectile)
			source->angle = projectile->angle;
#endif
    }
}

static inline bool Weakness_CheckHit(mobj_t *target, const AttackDefinition *attack, float x, float y, float z)
{
    const WeaknessDefinition *weak = &target->info->weak_;

    if (weak->classes_ == 0)
        return false;

    if (!attack) // Lobo: This fixes the long standing bug where EDGE crashes out sometimes.
    {
        return false;
    }

    if (0 != (attack->attack_class_ & ~weak->classes_))
        return false;

    if (target->height < 1)
        return false;

    // LogDebug("Weakness_CheckHit: target=[%s] classes=0x%08x\n", target->info->name.c_str(), weak->classes);

    // compute vertical position.  Clamping it means that a missile
    // which hits the target on the head (coming sharply down) will
    // still register as a head-shot.
    z = (z - target->z) / target->height;
    z = HMM_Clamp(0.01f, z, 0.99f);

    // LogDebug("HEIGHT CHECK: %1.2f < %1.2f < %1.2f\n",
    //		  weak->height[0], z, weak->height[1]);

    if (z < weak->height_[0] || z > weak->height_[1])
        return false;

    BAMAngle ang = R_PointToAngle(target->x, target->y, x, y);

    ang -= target->angle;

    // LogDebug("ANGLE CHECK: %1.2f < %1.2f < %1.2f\n",
    //		 ANG_2_FLOAT(weak->angle[0]), ANG_2_FLOAT(ang),
    //		 ANG_2_FLOAT(weak->angle[1]));

    if (weak->angle_[0] <= weak->angle_[1])
    {
        if (ang < weak->angle_[0] || ang > weak->angle_[1])
            return false;
    }
    else
    {
        if (ang < weak->angle_[0] && ang > weak->angle_[1])
            return false;
    }

    return true;
}

//
// P_MissileContact
//
// Called by PIT_CheckRelThing when a missile comes into
// contact with another object. Placed here with
// the other missile code for cleaner code.
//
// Returns: -1 if missile should pass through.
//           0 if hit but no damage was done.
//          +1 if hit and damage was done.
//
int P_MissileContact(mobj_t *object, mobj_t *target)
{
    mobj_t *source = object->source;

    if (source)
    {
        // check for ghosts (attack passes through)
        if (object->currentattack && 0 == (object->currentattack->attack_class_ & ~target->info->ghost_))
            return -1;

        if ((target->side & source->side) != 0)
        {
            if (target->hyperflags & kHyperFlagFriendlyFirePassesThrough)
                return -1;

            if (target->hyperflags & kHyperFlagFriendlyFireImmune)
                return 0;
        }

        if (source->info == target->info)
        {
            if (!(target->extendedflags & kExtendedFlagDisloyalToOwnType) && (source->info->proj_group_ != -1))
                return 0;
        }

        // MBF21: If in same projectile group, attack does no damage
        if (source->info->proj_group_ >= 0 && target->info->proj_group_ >= 0 &&
            (source->info->proj_group_ == target->info->proj_group_))
        {
            if (object->extendedflags & kExtendedFlagTunnel)
                return -1;
            else
                return 0;
        }

        if (object->currentattack != nullptr && !(target->extendedflags & kExtendedFlagOwnAttackHurts))
        {
            if (object->currentattack == target->info->rangeattack_)
                return 0;
            if (object->currentattack == target->info->closecombat_)
                return 0;
        }
    }

    const DamageClass *damtype;

    // transitional hack
    if (object->currentattack)
        damtype = &object->currentattack->damage_;
    else
        damtype = &object->info->explode_damage_;

    float damage;
    DAMAGE_COMPUTE(damage, damtype);

    bool weak_spot = false;

    // check for Weakness against the attack
    if (Weakness_CheckHit(target, object->currentattack, object->x, object->y, MO_MIDZ(object)))
    {
        damage *= target->info->weak_.multiply_;
        weak_spot = true;
    }

    // check for immunity against the attack
    if (object->hyperflags & kHyperFlagInvulnerable)
        return 0;

    if (!weak_spot && object->currentattack &&
        0 == (object->currentattack->attack_class_ & ~target->info->immunity_))
    {
        return 0;
    }

    // support for "tunnelling" missiles, which should only do damage at
    // the first impact.
    if (object->extendedflags & kExtendedFlagTunnel)
    {
        // this hash is very basic, but should work OK
        uint32_t hash = (uint32_t)(long long)target;

        if (object->tunnel_hash[0] == hash || object->tunnel_hash[1] == hash)
            return -1;

        object->tunnel_hash[0] = object->tunnel_hash[1];
        object->tunnel_hash[1] = hash;
        if (object->info->rip_sound_)
            S_StartFX(object->info->rip_sound_, SNCAT_Object, object, 0);
    }

    if (source)
    {
        // Berserk handling
        if (source->player && object->currentattack && !AlmostEquals(source->player->powers[kPowerTypeBerserk], 0.0f))
        {
            damage *= object->currentattack->berserk_mul_;
        }
    }

    if (!damage)
    {
#ifdef DEVELOPERS
        LogDebug("%s missile did zero damage.\n", object->info->name.c_str());
#endif
        return 0;
    }

    P_DamageMobj(target, object, object->source, damage, damtype, weak_spot);
    return 1;
}

//
// P_BulletContact
//
// Called by PTR_ShootTraverse when a bullet comes into contact with
// another object.  Needed so that the "DISLOYAL" special will behave
// in the same manner for bullets as for missiles.
//
// Note: also used for Close-Combat attacks.
//
// Returns: -1 if bullet should pass through.
//           0 if hit but no damage was done.
//          +1 if hit and damage was done.
//
int P_BulletContact(mobj_t *source, mobj_t *target, float damage, const DamageClass *damtype, float x, float y, float z)
{
    // check for ghosts (attack passes through)
    if (source->currentattack && 0 == (source->currentattack->attack_class_ & ~target->info->ghost_))
        return -1;

    if ((target->side & source->side) != 0)
    {
        if (target->hyperflags & kHyperFlagFriendlyFirePassesThrough)
            return -1;

        if (target->hyperflags & kHyperFlagFriendlyFireImmune)
            return 0;
    }

    if (source->info == target->info)
    {
        if (!(target->extendedflags & kExtendedFlagDisloyalToOwnType))
            return 0;
    }

    if (source->currentattack != nullptr && !(target->extendedflags & kExtendedFlagOwnAttackHurts))
    {
        if (source->currentattack == target->info->rangeattack_)
            return 0;
        if (source->currentattack == target->info->closecombat_)
            return 0;
    }

    // ignore damage in GOD mode, or with INVUL powerup
    if (target->player)
    {
        if ((target->player->cheats & CF_GODMODE) || target->player->powers[kPowerTypeInvulnerable] > 0)
        {
            // emulate the thrust that P_DamageMobj() would have done
            if (source && damage > 0 && !(target->flags & kMapObjectFlagNoClip))
                P_ThrustMobj(target, source, damage);

            return 0;
        }
    }

    bool weak_spot = false;

    // check for Weakness against the attack
    if (Weakness_CheckHit(target, source->currentattack, x, y, z))
    {
        damage *= target->info->weak_.multiply_;
        weak_spot = true;
    }

    // check for immunity against the attack
    if (target->hyperflags & kHyperFlagInvulnerable)
        return 0;

    if (!weak_spot && source->currentattack &&
        0 == (source->currentattack->attack_class_ & ~target->info->immunity_))
    {
        return 0;
    }

    if (!damage)
    {
#ifdef DEVELOPERS
        LogDebug("%s's shoot/combat attack did zero damage.\n", source->info->name.c_str());
#endif
        return 0;
    }

    P_DamageMobj(target, source, source, damage, damtype, weak_spot);
    return 1;
}

//
// P_ActCreateSmokeTrail
//
// Just spawns smoke behind an mobj: the smoke is
// risen by giving it z momentum, in order to
// prevent the smoke appearing uniform (which obviously
// does not happen), the number of tics that the smoke
// mobj has is "randomly" reduced, although the number
// of tics never gets to zero or below.
//
// -ACB- 1998/08/10 Written
// -ACB- 1999/10/01 Check thing's current attack has a smoke projectile
//
void P_ActCreateSmokeTrail(mobj_t *projectile)
{
    const AttackDefinition *attack = projectile->currentattack;

    if (attack == nullptr)
        return;

    if (attack->puff_ == nullptr)
    {
        PrintWarningOrError("P_ActCreateSmokeTrail: attack %s has no PUFF object\n", attack->name_.c_str());
        return;
    }

    // spawn a puff of smoke behind the rocket
    mobj_t *smoke = P_MobjCreateObject(projectile->x - projectile->mom.X / 2.0f,
                                       projectile->y - projectile->mom.Y / 2.0f, projectile->z, attack->puff_);

    smoke->mom.Z = smoke->info->float_speed_;
    smoke->tics -= Random8BitStateless() & 3;

    if (smoke->tics < 1)
        smoke->tics = 1;
}

//
// P_ActHomingProjectile
//
// This projectile will alter its course to intercept its
// target, if is possible for this procedure to be called
// and nothing results because of a chance that the
// projectile will not chase its target.
//
// As this code is based on the revenant tracer, it did use
// a bit check on the current game_tic - which was why every so
// often a revenant fires a missile straight and not one that
// homes in on its target: If the game_tic has bits 1+2 on
// (which boils down to 1 in every 4 tics), the trick in this
// is that - in conjuntion with the tic count for the
// tracing object's states - the tracing will always fail or
// pass the check: if it passes first time, it will always
// pass and vice versa. The problem is that for someone designing a new
// tracing projectile it would be more than a bit confusing to
// joe "dooming" public.
//
// The new system that affects the original gameplay slightly is
// to get a random chance of the projectile not homing in on its
// target and working this out first time round, the test result
// is recorded (by clearing the 'target' field).
//
// -ACB- 1998/08/10
//
void P_ActHomingProjectile(mobj_t *projectile)
{
    const AttackDefinition *attack = projectile->currentattack;

    if (attack == nullptr)
        return;

    if (attack->flags_ & kAttackFlagSmokingTracer)
        P_ActCreateSmokeTrail(projectile);

    if (projectile->extendedflags & kExtendedFlagFirstTracerCheck)
    {
        projectile->extendedflags &= ~kExtendedFlagFirstTracerCheck;

        if (Random8BitTestStateful(attack->notracechance_))
        {
            projectile->SetTarget(nullptr);
            return;
        }
    }

    mobj_t *destination = projectile->target;

    if (!destination || destination->health <= 0)
        return;

    // change angle
    BAMAngle exact = R_PointToAngle(projectile->x, projectile->y, destination->x, destination->y);

    if (exact != projectile->angle)
    {
        if (exact - projectile->angle > kBAMAngle180)
        {
            projectile->angle -= attack->trace_angle_;

            if (exact - projectile->angle < kBAMAngle180)
                projectile->angle = exact;
        }
        else
        {
            projectile->angle += attack->trace_angle_;

            if (exact - projectile->angle > kBAMAngle180)
                projectile->angle = exact;
        }
    }

    projectile->mom.X = projectile->speed * epi::BAMCos(projectile->angle);
    projectile->mom.Y = projectile->speed * epi::BAMSin(projectile->angle);

    // change slope
    float slope = P_ApproxSlope(destination->x - projectile->x, destination->y - projectile->y,
                                MO_MIDZ(destination) - projectile->z);

    slope *= projectile->speed;

    if (slope < projectile->mom.Z)
        projectile->mom.Z -= 0.125f;
    else
        projectile->mom.Z += 0.125f;
}

//
// P_ActHomeToSpot
//
// This projectile will alter its course to intercept its target,
// or explode if it has reached it.  Used by the bossbrain cube.
//
void P_ActHomeToSpot(mobj_t *projectile)
{
    mobj_t *target = projectile->target;

    if (target == nullptr)
    {
        P_MobjExplodeMissile(projectile);
        return;
    }

    float dx = target->x - projectile->x;
    float dy = target->y - projectile->y;
    float dz = target->z - projectile->z;

    float ck_radius = target->radius + projectile->radius + 2;
    float ck_height = target->height + projectile->height + 2;

    // reached target ?
    if (fabs(dx) <= ck_radius && fabs(dy) <= ck_radius && fabs(dz) <= ck_height)
    {
        P_MobjExplodeMissile(projectile);
        return;
    }

    // calculate new angles
    BAMAngle angle = R_PointToAngle(0, 0, dx, dy);
    float   slope = P_ApproxSlope(dx, dy, dz);

    P_SetMobjDirAndSpeed(projectile, angle, slope, projectile->speed);
}

//
// LaunchOrderedSpread
//
// Due to the unique way of handling that the mancubus fires, it is necessary
// to write a single procedure to handle the firing. In real terms it amounts
// to a glorified hack; The table holds the angle modifier and the choice of
// whether the firing object or the projectile is affected. This procedure
// should NOT be used for players as it will alter the player's mobj, bypassing
// the normal player controls; The only reason for its existance is to maintain
// the original mancubus behaviour. Although it is possible to make this generic,
// the benefits of doing so are minimal. Purist function....
//
// -ACB- 1998/08/15
//
static void LaunchOrderedSpread(mobj_t *mo)
{
    // left side = angle modifier
    // right side = object or projectile (true for object).
    static int spreadorder[] = {(int)(kBAMAngle90 / 8),  true,  (int)(kBAMAngle90 / 8),   false, -(int)(kBAMAngle90 / 8), true,
                                -(int)(kBAMAngle90 / 4), false, -(int)(kBAMAngle90 / 16), false, (int)(kBAMAngle90 / 16), false};

    const AttackDefinition *attack = mo->currentattack;

    if (attack == nullptr)
        return;

    int count = mo->spreadcount;

    if (count < 0 || count > 10)
        count = mo->spreadcount = 0;

    // object or projectile?
    // true --> the object, false --> the projectile.
    if (spreadorder[count + 1])
    {
        mo->angle += spreadorder[count];

        LaunchProjectile(mo, mo->target, attack->atk_mobj_);
    }
    else
    {
        mobj_t *projectile = LaunchProjectile(mo, mo->target, attack->atk_mobj_);
        if (projectile == nullptr)
            return;

        projectile->angle += spreadorder[count];

        projectile->mom.X = projectile->speed * epi::BAMCos(projectile->angle);
        projectile->mom.Y = projectile->speed * epi::BAMSin(projectile->angle);
    }

    mo->spreadcount += 2;
}

//
// LaunchRandomSpread
//
// This is a the generic function that should be used for a spreader like
// mancubus, although its random nature would certainly be a change to the
// ordered method used now. The random number is bit shifted to the right
// and then the kBAMAngle90 is divided by it, the first bit of the RN is checked
// to detemine if the angle is change is negative or not (approx 50% chance).
// The result is the modifier for the projectile's angle.
//
// -ACB- 1998/08/15
//
static void LaunchRandomSpread(mobj_t *mo)
{
    if (mo->currentattack == nullptr)
        return;

    mobj_t *projectile = LaunchProjectile(mo, mo->target, mo->currentattack->atk_mobj_);
    if (projectile == nullptr)
        return;

    int i = Random8BitStateful() & 127;

    if (i >> 1)
    {
        BAMAngle spreadangle = (kBAMAngle90 / (i >> 1));

        if (i & 1)
            spreadangle -= spreadangle << 1;

        projectile->angle += spreadangle;
    }

    projectile->mom.X = projectile->speed * epi::BAMCos(projectile->angle);
    projectile->mom.Y = projectile->speed * epi::BAMSin(projectile->angle);
}

//-------------------------------------------------------------------
//-------------------LINEATTACK ATTACK ROUTINES-----------------------
//-------------------------------------------------------------------

// -KM- 1998/11/25 Added uncertainty to the z component of the line.
static void ShotAttack(mobj_t *mo)
{
    const AttackDefinition *attack = mo->currentattack;

    if (!attack)
        return;

    float range = (attack->range_ > 0) ? attack->range_ : MISSILERANGE;

    // -ACB- 1998/09/05 Remember to use the object angle, fool!
    BAMAngle objangle = mo->angle;
    float   objslope;

    if ((mo->player && !mo->target) || (attack->flags_ & kAttackFlagNoTarget))
        objslope = epi::BAMTan(mo->vertangle);
    else
        P_AimLineAttack(mo, objangle, range, &objslope);

    if (attack->sound_)
        S_StartFX(attack->sound_, AttackSfxCat(mo), mo);

    // -AJA- 1999/09/10: apply the attack's angle offsets.
    objangle -= attack->angle_offset_;
    objslope += attack->slope_offset_;

    for (int i = 0; i < attack->count_; i++)
    {
        BAMAngle angle = objangle;
        float   slope = objslope;

        // is the attack not accurate?
        if (!mo->player || mo->player->refire > 0)
        {
            if (attack->accuracy_angle_ > 0)
                angle += (attack->accuracy_angle_ >> 8) * Random8BitSkewToZeroStateful();

            if (attack->accuracy_slope_ > 0)
                slope += attack->accuracy_slope_ * (Random8BitSkewToZeroStateful() / 255.0f);
        }

        float damage;
        DAMAGE_COMPUTE(damage, &attack->damage_);

        if (mo->player && !AlmostEquals(mo->player->powers[kPowerTypeBerserk], 0.0f))
            damage *= attack->berserk_mul_;

        P_LineAttack(mo, angle, range, slope, damage, &attack->damage_, attack->puff_);
    }
}

// -KM- 1998/11/25 BFG Spray attack.  Must be used from missiles.
//   Will do a BFG spray on every monster in sight.
static void SprayAttack(mobj_t *mo)
{
    const AttackDefinition *attack = mo->currentattack;

    if (!attack)
        return;

    float range = (attack->range_ > 0) ? attack->range_ : MISSILERANGE;

    // offset angles from its attack angle
    for (int i = 0; i < 40; i++)
    {
        BAMAngle an = mo->angle - kBAMAngle90 / 2 + (kBAMAngle90 / 40) * i;

        // mo->source is the originator (player) of the missile
        mobj_t *target = P_AimLineAttack(mo->source ? mo->source : mo, an, range, nullptr);

        if (!target)
            continue;

        mobj_t *ball = P_MobjCreateObject(target->x, target->y, target->z + target->height / 4, attack->atk_mobj_);

        ball->SetTarget(mo->target);

        // check for immunity against the attack
        if (target->hyperflags & kHyperFlagInvulnerable)
            continue;

        if (0 == (attack->attack_class_ & ~target->info->immunity_))
            continue;

        float damage;
        DAMAGE_COMPUTE(damage, &attack->damage_);

        if (mo->player && !AlmostEquals(mo->player->powers[kPowerTypeBerserk], 0.0f))
            damage *= attack->berserk_mul_;

        if (damage)
            P_DamageMobj(target, nullptr, mo->source, damage, &attack->damage_);
    }
}

static void DoMeleeAttack(mobj_t *mo)
{
    const AttackDefinition *attack = mo->currentattack;

    float range = (attack->range_ > 0) ? attack->range_ : MISSILERANGE;

    float damage;
    DAMAGE_COMPUTE(damage, &attack->damage_);

    // -KM- 1998/11/25 Berserk ability
    // -ACB- 2004/02/04 Only zero is off
    if (mo->player && !AlmostEquals(mo->player->powers[kPowerTypeBerserk], 0.0f))
        damage *= attack->berserk_mul_;

    // -KM- 1998/12/21 Use Line attack so bullet puffs are spawned.

    if (!DecideMeleeAttack(mo, attack))
    {
        P_LineAttack(mo, mo->angle, range, epi::BAMTan(mo->vertangle), damage, &attack->damage_, attack->puff_);
        return;
    }

    if (attack->sound_)
        S_StartFX(attack->sound_, AttackSfxCat(mo), mo);

    float slope;

    P_AimLineAttack(mo, mo->angle, range, &slope);

    P_LineAttack(mo, mo->angle, range, slope, damage, &attack->damage_, attack->puff_);
}

//-------------------------------------------------------------------
//--------------------TRACKER HANDLING ROUTINES----------------------
//-------------------------------------------------------------------

//
// A Tracker is an object that follows its target, by being on top of
// it. This is the attack style used by an Arch-Vile. The first routines
// handle the tracker itself, the last two are called by the source of
// the tracker.
//

//
// P_ActTrackerFollow
//
// Called by the tracker to follow its target.
//
// -ACB- 1998/08/22
//
void P_ActTrackerFollow(mobj_t *tracker)
{
    mobj_t *destination = tracker->target;

    if (!destination || !tracker->source)
        return;

    // Can the parent of the tracker see the target?
    if (!P_CheckSight(tracker->source, destination))
        return;

    BAMAngle angle = destination->angle;

    P_ChangeThingPosition(tracker, destination->x + 24 * epi::BAMCos(angle), destination->y + 24 * epi::BAMSin(angle),
                          destination->z);
}

//
// P_ActTrackerActive
//
// Called by the tracker to make its active sound: also tracks
//
// -ACB- 1998/08/22
//
void P_ActTrackerActive(mobj_t *tracker)
{
    if (tracker->info->activesound_)
        S_StartFX(tracker->info->activesound_, P_MobjGetSfxCategory(tracker), tracker);

    P_ActTrackerFollow(tracker);
}

//
// P_ActTrackerStart
//
// Called by the tracker to make its launch (see) sound: also tracks
//
// -ACB- 1998/08/22
//
void P_ActTrackerStart(mobj_t *tracker)
{
    if (tracker->info->seesound_)
        S_StartFX(tracker->info->seesound_, P_MobjGetSfxCategory(tracker), tracker);

    P_ActTrackerFollow(tracker);
}

//
// LaunchTracker
//
// This procedure starts a tracking object off and links
// the tracker and the monster together.
//
// -ACB- 1998/08/22
//
static void LaunchTracker(mobj_t *object)
{
    const AttackDefinition *attack = object->currentattack;
    mobj_t         *target = object->target;

    if (!attack || !target)
        return;

    mobj_t *tracker = P_MobjCreateObject(target->x, target->y, target->z, attack->atk_mobj_);

    // link the tracker to the object
    object->SetTracer(tracker);

    // tracker source is the object
    tracker->SetRealSource(object);

    // tracker's target is the object's target
    tracker->SetTarget(target);

    P_ActTrackerFollow(tracker);
}

//
// P_ActEffectTracker
//
// Called by the object that launched the tracker to
// cause damage to its target and a radius attack
// (explosion) at the location of the tracker.
//
// -ACB- 1998/08/22
//
void P_ActEffectTracker(mobj_t *object)
{
    mobj_t         *tracker;
    mobj_t         *target;
    const AttackDefinition *attack;
    BAMAngle         angle;
    float           damage;

    if (!object->target || !object->currentattack)
        return;

    attack = object->currentattack;
    target = object->target;

    if (attack->flags_ & kAttackFlagFaceTarget)
        P_ActFaceTarget(object);

    if (attack->flags_ & kAttackFlagNeedSight)
    {
        if (!P_CheckSight(object, target))
            return;
    }

    if (attack->sound_)
        S_StartFX(attack->sound_, P_MobjGetSfxCategory(object), object);

    angle   = object->angle;
    tracker = object->tracer;

    DAMAGE_COMPUTE(damage, &attack->damage_);

    if (damage)
        P_DamageMobj(target, object, object, damage, &attack->damage_);
#ifdef DEVELOPERS
    else
        LogDebug("%s + %s attack has zero damage\n", object->info->name.c_str(), tracker->info->name.c_str());
#endif

    // -ACB- 2000/03/11 Check for zero mass
    if (target->info->mass_)
        target->mom.Z = 1000 / target->info->mass_;
    else
        target->mom.Z = 2000;

    if (!tracker)
        return;

    // move the tracker between the object and the object's target

    P_ChangeThingPosition(tracker, target->x - 24 * epi::BAMCos(angle), target->y - 24 * epi::BAMSin(angle), target->z);

#ifdef DEVELOPERS
    if (!tracker->info->explode_damage_.nominal)
        LogDebug("%s + %s explosion has zero damage\n", object->info->name.c_str(), tracker->info->name.c_str());
#endif

    DAMAGE_COMPUTE(damage, &tracker->info->explode_damage_);

    float radius = object->info->explode_radius_;
    if (AlmostEquals(radius, 0.0f))
        radius = damage;

    P_RadiusAttack(tracker, object, radius, damage, &tracker->info->explode_damage_, false);
}

//
// P_ActPsychicEffect
//
// Same as above, but with a single non-explosive damage instance and no lifting of the target
//
void P_ActPsychicEffect(mobj_t *object)
{
    mobj_t         *target;
    const AttackDefinition *attack;
    float           damage;

    if (!object->target || !object->currentattack)
        return;

    attack = object->currentattack;
    target = object->target;

    if (attack->flags_ & kAttackFlagFaceTarget)
        P_ActFaceTarget(object);

    if (attack->flags_ & kAttackFlagNeedSight)
    {
        if (!P_CheckSight(object, target))
            return;
    }

    if (attack->sound_)
        S_StartFX(attack->sound_, P_MobjGetSfxCategory(object), object);

    DAMAGE_COMPUTE(damage, &attack->damage_);

    if (damage)
        P_DamageMobj(target, object, object, damage, &attack->damage_);
#ifdef DEVELOPERS
    else
        LogDebug("%s + %s attack has zero damage\n", object->info->name.c_str(), tracker->info->name.c_str());
#endif
}

//-----------------------------------------------------------------
//--------------------BOSS HANDLING PROCEDURES---------------------
//-----------------------------------------------------------------

static void ShootToSpot(mobj_t *object)
{
    if (!object->currentattack)
        return;

    const MapObjectDefinition *spot_type = object->info->spitspot_;

    if (spot_type == nullptr)
    {
        PrintWarningOrError("Thing [%s] used SHOOT_TO_SPOT attack, but has no SPIT_SPOT\n", object->info->name_.c_str());
        return;
    }

    mobj_t *spot = P_LookForShootSpot(object->info->spitspot_);

    if (spot == nullptr)
    {
        LogWarning("No [%s] objects found for BossBrain shooter.\n", spot_type->name_.c_str());
        return;
    }

    LaunchProjectile(object, spot, object->currentattack->atk_mobj_);
}

//-------------------------------------------------------------------
//-------------------OBJECT-SPAWN-OBJECT HANDLING--------------------
//-------------------------------------------------------------------

//
// P_ActObjectSpawning
//
// An Object spawns another object and is spawned in the state specificed
// by attack->objinitstate. The procedure is based on the A_PainShootSkull
// which is the routine for shooting skulls from a pain elemental. In
// this the object being created is decided in the attack. This
// procedure also used the new blocking line check to see if
// the object is spawned across a blocking line, if so the procedure
// terminates.
//
// -ACB- 1998/08/23
//
static void ObjectSpawning(mobj_t *parent, BAMAngle angle)
{
    float           slope;
    const AttackDefinition *attack;

    attack = parent->currentattack;
    if (!attack)
        return;

    const MapObjectDefinition *shoottype = attack->spawnedobj_;

    if (!shoottype)
    {
        FatalError("Object [%s] uses spawning attack [%s], but no object "
                "specified.\n",
                parent->info->name_.c_str(), attack->name_.c_str());
    }

    if (attack->spawn_limit_ > 0)
    {
        int count = 0;
        for (mobj_t *mo = mobjlisthead; mo; mo = mo->next)
            if (mo->info == shoottype)
                if (++count >= attack->spawn_limit_)
                    return;
    }

    // -AJA- 1999/09/10: apply the angle offset of the attack.
    angle -= attack->angle_offset_;
    slope = epi::BAMTan(parent->vertangle) + attack->slope_offset_;

    float spawnx = parent->x;
    float spawny = parent->y;
    float spawnz = parent->z + attack->height_;

    if (attack->flags_ & kAttackFlagPrestepSpawn)
    {
        float prestep = 4.0f + 1.5f * parent->radius + shoottype->radius_;

        spawnx += prestep * epi::BAMCos(angle);
        spawny += prestep * epi::BAMSin(angle);
    }

    mobj_t *child = P_MobjCreateObject(spawnx, spawny, spawnz, shoottype);

    // Blocking line detected between object and spawnpoint?
    if (P_MapCheckBlockingLine(parent, child))
    {
        if (child->flags & kMapObjectFlagCountKill)
            intermission_stats.kills--;
        if (child->flags & kMapObjectFlagCountItem)
            intermission_stats.items--;
        // -KM- 1999/01/31 Explode objects over remove them.
        // -AJA- 2000/02/01: Remove now the default.
        if (attack->flags_ & kAttackFlagKillFailedSpawn)
        {
            P_KillMobj(parent, child, nullptr);
            if (child->flags & kMapObjectFlagCountKill)
                players[consoleplayer]->killcount--;
        }
        else
            P_RemoveMobj(child);

        return;
    }

    if (attack->sound_)
        S_StartFX(attack->sound_, AttackSfxCat(parent), parent);

    // If the object cannot move from its position, remove it or kill it.
    if (!P_TryMove(child, child->x, child->y))
    {
        if (child->flags & kMapObjectFlagCountKill)
            intermission_stats.kills--;
        if (child->flags & kMapObjectFlagCountItem)
            intermission_stats.items--;
        if (attack->flags_ & kAttackFlagKillFailedSpawn)
        {
            P_KillMobj(parent, child, nullptr);
            if (child->flags & kMapObjectFlagCountKill)
                players[consoleplayer]->killcount--;
        }
        else
            P_RemoveMobj(child);

        return;
    }

    if (!(attack->flags_ & kAttackFlagNoTarget))
        child->SetTarget(parent->target);

    child->SetSupportObj(parent);

    child->side = parent->side;

    // -AJA- 2004/09/27: keep ambush status of parent
    child->flags |= (parent->flags & kMapObjectFlagAmbush);

    // -AJA- 1999/09/25: Set the initial direction & momentum when
    //       the ANGLED_SPAWN attack special is used.
    if (attack->flags_ & kAttackFlagAngledSpawn)
        P_SetMobjDirAndSpeed(child, angle, slope, attack->assault_speed_);

    P_SetMobjStateDeferred(child, attack->objinitstate_, 0);
}

//
// P_ActObjectTripleSpawn
//
// Spawns three objects at 90, 180 and 270 degrees. This is essentially
// another purist function to support the death sequence of the Pain
// elemental. However it could be used as in conjunction with radius
// triggers to generate a nice teleport spawn invasion.
//
// -ACB- 1998/08/23 (I think....)
//

static void ObjectTripleSpawn(mobj_t *object)
{
    ObjectSpawning(object, object->angle + kBAMAngle90);
    ObjectSpawning(object, object->angle + kBAMAngle180);
    ObjectSpawning(object, object->angle + kBAMAngle270);
}

//
// P_ActObjectDoubleSpawn
//
// Spawns two objects at 90 and 270 degrees.
// Like the death sequence of the Pain
// elemental.
//
// Lobo: 2021 to mimic the Doom64 pain elemental
//

static void ObjectDoubleSpawn(mobj_t *object)
{
    ObjectSpawning(object, object->angle + kBAMAngle90);
    // ObjectSpawning(object, object->angle + kBAMAngle180);
    ObjectSpawning(object, object->angle + kBAMAngle270);
}

//-------------------------------------------------------------------
//-------------------SKULLFLY HANDLING ROUTINES----------------------
//-------------------------------------------------------------------

//
// SkullFlyAssault
//
// This is the attack procedure for objects that launch themselves
// at their target like a missile.
//
// -ACB- 1998/08/16
//
static void SkullFlyAssault(mobj_t *object)
{
    if (!object->currentattack)
        return;

    if (!object->target && !object->player)
    {
        // -AJA- 2000/09/29: fix for the zombie lost soul bug
        // -AJA- 2000/10/22: monsters only !  Don't stuff up gibs/missiles.
        if (object->extendedflags & kExtendedFlagMonster)
            object->flags |= kMapObjectFlagSkullFly;
        return;
    }

    float speed = object->currentattack->assault_speed_;

    SoundEffect *sound = object->currentattack->initsound_;

    if (sound)
        S_StartFX(sound, P_MobjGetSfxCategory(object), object);

    object->flags |= kMapObjectFlagSkullFly;

    // determine destination
    float tx, ty, tz;

    P_TargetTheory(object, object->target, &tx, &ty, &tz);

    float slope = P_ApproxSlope(tx - object->x, ty - object->y, tz - object->z);

    P_SetMobjDirAndSpeed(object, object->angle, slope, speed);
}

//
// P_SlammedIntoObject
//
// Used when a flying object hammers into another object when on the
// attack. Replaces the code in PIT_Checkthing.
//
// -ACB- 1998/07/29: Written
//
// -AJA- 1999/09/12: Now uses P_SetMobjStateDeferred, since this
//                   routine can be called by TryMove/PIT_CheckRelThing.
//
void P_SlammedIntoObject(mobj_t *object, mobj_t *target)
{
    if (object->currentattack)
    {
        if (target != nullptr)
        {
            // -KM- 1999/01/31 Only hurt shootable objects...
            if (target->flags & kMapObjectFlagShootable)
            {
                float damage;

                DAMAGE_COMPUTE(damage, &object->currentattack->damage_);

                if (damage)
                    P_DamageMobj(target, object, object, damage, &object->currentattack->damage_);
            }
        }

        SoundEffect *sound = object->currentattack->sound_;
        if (sound)
            S_StartFX(sound, P_MobjGetSfxCategory(object), object);
    }

    object->flags &= ~kMapObjectFlagSkullFly;
    object->mom.X = object->mom.Y = object->mom.Z = 0;

    P_SetMobjStateDeferred(object, object->info->idle_state_, 0);
}

bool P_UseThing(mobj_t *user, mobj_t *thing, float open_bottom, float open_top)
{
    // Called when this thing is attempted to be used (i.e. by pressing
    // the spacebar near it) by the player.  Returns true if successfully
    // used, or false if other things should be checked.

    // item is disarmed ?
    if (!(thing->flags & kMapObjectFlagTouchy))
        return false;

    // can be reached ?
    open_top    = HMM_MIN(open_top, thing->z + thing->height);
    open_bottom = HMM_MAX(open_bottom, thing->z);

    if (user->z >= open_top || (user->z + user->height + USE_Z_RANGE < open_bottom))
        return false;

    // OK, disarm and put into touch states
    SYS_ASSERT(thing->info->touch_state_ > 0);

    thing->flags &= ~kMapObjectFlagTouchy;
    P_SetMobjStateDeferred(thing, thing->info->touch_state_, 0);

    return true;
}

void P_TouchyContact(mobj_t *touchy, mobj_t *victim)
{
    // Used whenever a thing comes into contact with a TOUCHY object.
    //
    // -AJA- 1999/09/12: Now uses P_SetMobjStateDeferred, since this
    //       routine can be called by TryMove/PIT_CheckRelThing.

    // dead thing touching. Can happen with a sliding player corpse.
    if (victim->health <= 0)
        return;

    // don't harm the grenadier...
    if (touchy->source == victim)
        return;

    touchy->SetTarget(victim);
    touchy->flags &= ~kMapObjectFlagTouchy; // disarm

    if (touchy->info->touch_state_)
        P_SetMobjStateDeferred(touchy, touchy->info->touch_state_, 0);
    else
        P_MobjExplodeMissile(touchy);
}

void P_ActTouchyRearm(mobj_t *touchy)
{
    touchy->flags |= kMapObjectFlagTouchy;
}

void P_ActTouchyDisarm(mobj_t *touchy)
{
    touchy->flags &= ~kMapObjectFlagTouchy;
}

void P_ActBounceRearm(mobj_t *mo)
{
    mo->extendedflags &= ~kExtendedFlagJustBounced;
}

void P_ActBounceDisarm(mobj_t *mo)
{
    mo->extendedflags |= kExtendedFlagJustBounced;
}

void P_ActDropItem(mobj_t *mo)
{
    const MapObjectDefinition *info = mo->info->dropitem_;

    if (mo->state && mo->state->action_par)
    {
        MobjStringReference *ref = (MobjStringReference *)mo->state->action_par;

        info = ref->GetRef();
    }

    if (!info)
    {
        PrintWarningOrError("P_ActDropItem: %s specifies no item to drop.\n", mo->info->name_.c_str());
        return;
    }

    // unlike normal drops, these ones are displaced randomly

    float dx = Random8BitSkewToZeroStateful() * mo->info->radius_ / 255.0f;
    float dy = Random8BitSkewToZeroStateful() * mo->info->radius_ / 255.0f;

    mobj_t *item = P_MobjCreateObject(mo->x + dx, mo->y + dy, mo->floorz, info);
    SYS_ASSERT(item);

    item->flags |= kMapObjectFlagDropped;
    item->flags &= ~kMapObjectFlagSolid;

    item->angle = mo->angle;

    // allow respawning
    item->spawnpoint.x         = item->x;
    item->spawnpoint.y         = item->y;
    item->spawnpoint.z         = item->z;
    item->spawnpoint.angle     = item->angle;
    item->spawnpoint.vertangle = item->vertangle;
    item->spawnpoint.info      = info;
    item->spawnpoint.flags     = 0;
}

void P_ActSpawn(mobj_t *mo)
{
    if (!mo->state || !mo->state->action_par)
        FatalError("SPAWN() action used without a object name!\n");

    MobjStringReference *ref = (MobjStringReference *)mo->state->action_par;

    const MapObjectDefinition *info = ref->GetRef();
    SYS_ASSERT(info);

    mobj_t *item = P_MobjCreateObject(mo->x, mo->y, mo->z, info);
    SYS_ASSERT(item);

    item->angle = mo->angle;
    item->side  = mo->side;

    item->SetSource(mo);
}

void P_ActPathCheck(mobj_t *mo)
{
    // Checks if the creature is a path follower, and if so enters the
    // meander states.

    if (!mo->path_trigger || !mo->info->meander_state_)
        return;

    P_SetMobjStateDeferred(mo, mo->info->meander_state_, 0);

    mo->movedir   = DI_SLOWTURN;
    mo->movecount = 0;
}

void P_ActPathFollow(mobj_t *mo)
{
    // For path-following creatures (spawned via RTS), makes the creature
    // follow the path by trying to get to the next node.

    if (!mo->path_trigger)
        return;

    if (RAD_CheckReachedTrigger(mo))
    {
        // reached the very last one ?
        if (!mo->path_trigger)
        {
            mo->movedir = DI_NODIR;
            return;
        }

        mo->movedir = DI_SLOWTURN;
        return;
    }

    float dx = mo->path_trigger->x - mo->x;
    float dy = mo->path_trigger->y - mo->y;

    BAMAngle diff = R_PointToAngle(0, 0, dx, dy) - mo->angle;

    // movedir value:
    //   0 for slow turning.
    //   1 for fast turning.
    //   2 for walking.
    //   3 for evasive maneouvres.

    // if (mo->movedir < 2)

    if (mo->movedir == DI_SLOWTURN || mo->movedir == DI_FASTTURN)
    {
        if (diff > kBAMAngle15 && diff < (kBAMAngle360 - kBAMAngle15))
        {
            BAMAngle step = kBAMAngle30;

            if (diff < kBAMAngle180)
                mo->angle += Random8BitStateful() * (step >> 8);
            else
                mo->angle -= Random8BitStateful() * (step >> 8);

            return;
        }

        // we are now facing the next node
        mo->angle += diff;
        mo->movedir = DI_WALKING;

        diff = 0;
    }

    if (mo->movedir == DI_WALKING)
    {
        if (diff < kBAMAngle30)
            mo->angle += kBAMAngle1 * 2;
        else if (diff > (kBAMAngle360 - kBAMAngle30))
            mo->angle -= kBAMAngle1 * 2;
        else
            mo->movedir = DI_SLOWTURN;

        if (!P_Move(mo, true))
        {
            mo->movedir   = DI_EVASIVE;
            mo->angle     = Random8BitStateful() << (kBAMAngleBits - 8);
            mo->movecount = 1 + (Random8BitStateful() & 7);
        }
        return;
    }

    // make evasive maneouvres
    mo->movecount--;

    if (mo->movecount <= 0)
    {
        mo->movedir = DI_FASTTURN;
        return;
    }

    P_Move(mo, true);
}

//-------------------------------------------------------------------
//--------------------ATTACK HANDLING PROCEDURES---------------------
//-------------------------------------------------------------------

//
// P_DoAttack
//
// When an object goes on the attack, it current attack is handled here;
// the attack type is discerned and the assault is launched.
//
// -ACB- 1998/08/07
//
static void P_DoAttack(mobj_t *object)
{
    const AttackDefinition *attack = object->currentattack;

    SYS_ASSERT(attack);

    switch (attack->attackstyle_)
    {
    case kAttackStyleCloseCombat: {
        DoMeleeAttack(object);
        break;
    }

    case kAttackStyleProjectile: {
        LaunchProjectile(object, object->target, attack->atk_mobj_);
        break;
    }

    case kAttackStyleSmartProjectile: {
        LaunchSmartProjectile(object, object->target, attack->atk_mobj_);
        break;
    }

    case kAttackStyleRandomSpread: {
        LaunchRandomSpread(object);
        break;
    }

    case kAttackStyleShootToSpot: {
        ShootToSpot(object);
        break;
    }

    case kAttackStyleShot: {
        ShotAttack(object);
        break;
    }

    case kAttackStyleSkullFly: {
        SkullFlyAssault(object);
        break;
    }

    case kAttackStyleSpawner: {
        ObjectSpawning(object, object->angle);
        break;
    }

    case kAttackStyleSpreader: {
        LaunchOrderedSpread(object);
        break;
    }

    case kAttackStyleTracker: {
        LaunchTracker(object);
        break;
    }

    case kAttackStylePsychic: {
        LaunchTracker(object);
        P_ActPsychicEffect(object);
        break;
    }

    // Lobo 2021: added doublespawner like the Doom64 elemental
    case kAttackStyleDoubleSpawner: {
        ObjectDoubleSpawn(object);
        break;
    }

    case kAttackStyleTripleSpawner: {
        ObjectTripleSpawn(object);
        break;
    }

    // -KM- 1998/11/25 Added spray attack
    case kAttackStyleSpray: {
        SprayAttack(object);
        break;
    }

    default: // THIS SHOULD NOT HAPPEN
    {
        if (strict_errors)
            FatalError("P_DoAttack: %s has an unknown attack type.\n", object->info->name_.c_str());
        break;
    }
    }
}

//
// P_ActComboAttack
//
// This is called at end of a set of states that can result in
// either a closecombat_ or ranged attack. The procedure checks
// to see if the target is within melee range and picks the
// approiate attack.
//
// -ACB- 1998/08/07
//
void P_ActComboAttack(mobj_t *object)
{
    const AttackDefinition *attack;

    if (!object->target)
        return;

    if (DecideMeleeAttack(object, object->info->closecombat_))
        attack = object->info->closecombat_;
    else
        attack = object->info->rangeattack_;

    if (attack)
    {
        if (attack->flags_ & kAttackFlagFaceTarget)
            P_ActFaceTarget(object);

        if (attack->flags_ & kAttackFlagNeedSight)
        {
            if (!P_CheckSight(object, object->target))
                return;
        }

        object->currentattack = attack;
        P_DoAttack(object);
    }
#ifdef DEVELOPERS
    else
    {
        if (!object->info->closecombat_)
            PrintWarningOrError("%s hasn't got a close combat attack\n", object->info->name.c_str());
        else
            PrintWarningOrError("%s hasn't got a range attack\n", object->info->name.c_str());
    }
#endif
}

//
// P_ActMeleeAttack
//
// Setup a close combat assault
//
// -ACB- 1998/08/07
//
void P_ActMeleeAttack(mobj_t *object)
{
    const AttackDefinition *attack;

    attack = object->info->closecombat_;

    // -AJA- 1999/08/10: Multiple attack support.
    if (object->state && object->state->action_par)
        attack = (const AttackDefinition *)object->state->action_par;

    if (!attack)
    {
        PrintWarningOrError("P_ActMeleeAttack: %s has no close combat attack.\n", object->info->name_.c_str());
        return;
    }

    if (attack->flags_ & kAttackFlagFaceTarget)
        P_ActFaceTarget(object);

    if (attack->flags_ & kAttackFlagNeedSight)
    {
        if (!object->target || !P_CheckSight(object, object->target))
            return;
    }

    object->currentattack = attack;
    P_DoAttack(object);
}

//
// P_ActRangeAttack
//
// Setup an attack at range
//
// -ACB- 1998/08/07
//
void P_ActRangeAttack(mobj_t *object)
{
    const AttackDefinition *attack;

    attack = object->info->rangeattack_;

    // -AJA- 1999/08/10: Multiple attack support.
    if (object->state && object->state->action_par)
        attack = (const AttackDefinition *)object->state->action_par;

    if (!attack)
    {
        PrintWarningOrError("P_ActRangeAttack: %s hasn't got a range attack.\n", object->info->name_.c_str());
        return;
    }

    if (attack->flags_ & kAttackFlagFaceTarget)
        P_ActFaceTarget(object);

    if (attack->flags_ & kAttackFlagNeedSight)
    {
        if (!object->target || !P_CheckSight(object, object->target))
            return;
    }

    object->currentattack = attack;
    P_DoAttack(object);
}

//
// P_ActSpareAttack
//
// Setup an attack that is not defined as close or range. can be
// used to act as a follow attack for close or range, if you want one to
// add to the others.
//
// -ACB- 1998/08/24
//
void P_ActSpareAttack(mobj_t *object)
{
    const AttackDefinition *attack;

    attack = object->info->spareattack_;

    // -AJA- 1999/08/10: Multiple attack support.
    if (object->state && object->state->action_par)
        attack = (const AttackDefinition *)object->state->action_par;

    if (attack)
    {
        if ((attack->flags_ & kAttackFlagFaceTarget) && object->target)
            P_ActFaceTarget(object);

        if ((attack->flags_ & kAttackFlagNeedSight) && object->target)
        {
            if (!P_CheckSight(object, object->target))
                return;
        }

        object->currentattack = attack;
        P_DoAttack(object);
    }
#ifdef DEVELOPERS
    else
    {
        PrintWarningOrError("P_ActSpareAttack: %s hasn't got a spare attack\n", object->info->name.c_str());
        return;
    }
#endif
}

//
// P_ActRefireCheck
//
// This procedure will be called inbetween firing on an object
// that will fire repeatly (Chaingunner/Arachontron etc...), the
// purpose of this is to see if the object should refire and
// performs checks to that effect, first there is a check to see
// if the object will keep firing regardless and the others
// check if the the target exists, is alive and within view. The
// only other code here is a stealth check: a object with stealth
// capabilitys will lose the ability while firing.
//
// -ACB- 1998/08/10
//
void P_ActRefireCheck(mobj_t *object)
{
    mobj_t         *target;
    const AttackDefinition *attack;

    attack = object->currentattack;

    if (!attack)
        return;

    if (attack->flags_ & kAttackFlagFaceTarget)
        P_ActFaceTarget(object);

    // Random chance that object will keep firing regardless
    if (Random8BitTestStateful(attack->keepfirechance_))
        return;

    target = object->target;

    if (!target || (target->health <= 0) || !P_CheckSight(object, target))
    {
        if (object->info->chase_state_)
            P_SetMobjStateDeferred(object, object->info->chase_state_, 0);
    }
    else if (object->flags & kMapObjectFlagStealth)
    {
        object->vis_target = VISIBLE;
    }
}

//
// P_ActReloadCheck
//
// Enter reload states if the monster has shot a certain number of
// shots (given by RELOAD_SHOTS command).
//
// -AJA- 2004/11/15: added this.
//
void P_ActReloadCheck(mobj_t *object)
{
    object->shot_count++;

    if (object->shot_count >= object->info->reload_shots_)
    {
        object->shot_count = 0;

        if (object->info->reload_state_)
            P_SetMobjStateDeferred(object, object->info->reload_state_, 0);
    }
}

void P_ActReloadReset(mobj_t *object)
{
    object->shot_count = 0;
}

//---------------------------------------------
//-----------LOOKING AND CHASING---------------
//---------------------------------------------

extern mobj_t **bmap_things;

//
// CreateAggression
//
// Sets an object up to target a previously stored object.
//
// -ACB- 2000/06/20 Re-written and Simplified
//
// -AJA- 2009/07/05 Rewritten again, using the blockmap
//
static bool CreateAggression(mobj_t *mo)
{
    if (mo->target && mo->target->health > 0)
        return false;

    // pick a block in blockmap to check
    int bdx = Random8BitSkewToZeroStateful() / 17;
    int bdy = Random8BitSkewToZeroStateful() / 17;

    int block_x = BLOCKMAP_GET_X(mo->x) + bdx;
    int block_y = BLOCKMAP_GET_X(mo->y) + bdy;

    block_x = abs(block_x + bmap_width) % bmap_width;
    block_y = abs(block_y + bmap_height) % bmap_height;

    //  LogDebug("BLOCKMAP POS: %3d %3d  (size: %d %d)\n", block_x, block_y, bmap_width, bmap_height);

    int bnum = block_y * bmap_width + block_x;

    for (mobj_t *other = bmap_things[bnum]; other; other = other->bnext)
    {
        if (!(other->info->extendedflags_ & kExtendedFlagMonster) || other->health <= 0)
            continue;

        if (other == mo)
            continue;

        if (other->info == mo->info)
        {
            if (!(other->info->extendedflags_ & kExtendedFlagDisloyalToOwnType))
                continue;

            // Type the same and it can't hurt own kind - not good.
            if (!(other->info->extendedflags_ & kExtendedFlagOwnAttackHurts))
                continue;
        }

        // don't attack a friend if we cannot hurt them.
        // -AJA- I'm assuming that even friends will 'infight'.
        if ((mo->info->side_ & other->info->side_) != 0 && (other->info->hyperflags_ & (kHyperFlagFriendlyFireImmune | kHyperFlagUltraLoyal)))
        {
            continue;
        }

        // MBF21: If in same infighting group, never target each other even if
        // hit with 'friendly fire'
        if (mo->info->infight_group_ >= 0 && other->info->infight_group_ >= 0 &&
            (mo->info->infight_group_ == other->info->infight_group_))
        {
            continue;
        }

        // POTENTIAL TARGET

        // fairly low chance of trying it, in case this block
        // contains many monsters (spread the love)
        if (Random8BitStateful() > 99)
            continue;

        // sight check is expensive, do it last
        if (!P_CheckSight(mo, other))
            continue;

        // OK, you got me
        mo->SetTarget(other);

        LogDebug("Created aggression : %s --> %s\n", mo->info->name_.c_str(), other->info->name_.c_str());

        if (mo->info->seesound_)
            S_StartFX(mo->info->seesound_, P_MobjGetSfxCategory(mo), mo, SfxFlags(mo->info));

        if (mo->info->chase_state_)
            P_SetMobjStateDeferred(mo, mo->info->chase_state_, 0);

        return true;
    }

    return false;
}

//
// P_ActStandardLook
//
// Standard Lookout procedure
//
// -ACB- 1998/08/22
//
void P_ActStandardLook(mobj_t *object)
{
    int     targ_pnum;
    mobj_t *targ = nullptr;

    object->threshold = 0; // any shot will wake up

    // FIXME: replace with cvar/Menu toggle
    bool CVAR_DOOM_TARGETTING = false;

    if (CVAR_DOOM_TARGETTING == true)
        targ_pnum = object->subsector->sector->sound_player; // old way
    else
        targ_pnum = object->lastheard; // new way

    if (targ_pnum >= 0 && targ_pnum < MAXPLAYERS && players[targ_pnum])
    {
        targ = players[targ_pnum]->mo;
    }

    // -AJA- 2004/09/02: ignore the sound of a friend
    // FIXME: maybe wake up and support that player ??
    // if (targ && (targ->side & object->side) != 0)
    if (object->side != 0)
    {
        // targ = nullptr;
        // P_ActPlayerSupportLook(object);
        P_ActPlayerSupportMeander(object);
        return;
    }

    if (object->flags & kMapObjectFlagStealth)
        object->vis_target = VISIBLE;

    if (g_aggression.d_)
        if (CreateAggression(object) || CreateAggression(object))
            return;

    if (targ && (targ->flags & kMapObjectFlagShootable))
    {
        object->SetTarget(targ);

        if (object->flags & kMapObjectFlagAmbush)
        {
            if (!P_CheckSight(object, object->target) && !P_LookForPlayers(object, object->info->sight_angle_))
                return;
        }
    }
    else
    {
        if (!P_LookForPlayers(object, object->info->sight_angle_))
            return;
    }

    if (object->info->seesound_)
    {
        S_StartFX(object->info->seesound_, P_MobjGetSfxCategory(object), object, SfxFlags(object->info));
    }

    // -AJA- this will remove objects which have no chase states.
    // For compatibility with original DOOM.
    P_SetMobjStateDeferred(object, object->info->chase_state_, 0);
}

//
// P_ActPlayerSupportLook
//
// Player Support Lookout procedure
//
// -ACB- 1998/09/05
//
void P_ActPlayerSupportLook(mobj_t *object)
{
    object->threshold = 0; // any shot will wake up

    if (object->flags & kMapObjectFlagStealth)
        object->vis_target = VISIBLE;

    if (!object->supportobj)
    {
        if (!P_ActLookForTargets(object))
            return;

        // -AJA- 2004/09/02: join the player's side
        if (object->side == 0)
            object->side = object->target->side;

        if (object->info->seesound_)
        {
            S_StartFX(object->info->seesound_, P_MobjGetSfxCategory(object), object, SfxFlags(object->info));
        }
    }

    if (object->info->meander_state_)
        P_SetMobjStateDeferred(object, object->info->meander_state_, 0);
}

//
// P_ActStandardMeander
//
void P_ActStandardMeander(mobj_t *object)
{
    int delta;

    object->threshold = 0; // any shot will wake up

    // move within supporting distance of player
    if (--object->movecount < 0 || !P_Move(object, false))
        P_NewChaseDir(object);

    // turn towards movement direction if not there yet
    if (object->movedir < 8)
    {
        object->angle &= (7 << 29);
        delta = object->angle - (object->movedir << 29);

        if (delta > 0)
            object->angle -= kBAMAngle45;
        else if (delta < 0)
            object->angle += kBAMAngle45;
    }
}

//
// P_ActPlayerSupportMeander
//
void P_ActPlayerSupportMeander(mobj_t *object)
{
    int delta;

    object->threshold = 0; // any shot will wake up

    // move within supporting distance of player
    if (--object->movecount < 0 || !P_Move(object, false))
        P_NewChaseDir(object);

    // turn towards movement direction if not there yet
    if (object->movedir < 8)
    {
        object->angle &= (7 << 29);
        delta = object->angle - (object->movedir << 29);

        if (delta > 0)
            object->angle -= kBAMAngle45;
        else if (delta < 0)
            object->angle += kBAMAngle45;
    }

    //
    // we have now meandered, now check for a support object, if we don't
    // look for one and return; else look for targets to take out, if we
    // find one, go for the chase.
    //
    /*  if (!object->supportobj)
        {
        P_ActPlayerSupportLook(object);
        return;
        } */

    P_ActLookForTargets(object);
}

//
// P_ActStandardChase
//
// Standard AI Chase Procedure
//
// -ACB- 1998/08/22 Procedure Written
// -ACB- 1998/09/05 Added Support Object Check
//
void P_ActStandardChase(mobj_t *object)
{
    int    delta;
    SoundEffect *sound;

    if (object->reactiontime)
        object->reactiontime--;

    // object has a pain threshold, while this is true, reduce it. while
    // the threshold is true, the object will remain intent on its target.
    if (object->threshold)
    {
        if (!object->target || object->target->health <= 0)
            object->threshold = 0;
        else
            object->threshold--;
    }

    // A Chasing Stealth Creature becomes less visible
    if (object->flags & kMapObjectFlagStealth)
        object->vis_target = INVISIBLE;

    // turn towards movement direction if not there yet
    if (object->movedir < 8)
    {
        object->angle &= (7 << 29);
        delta = object->angle - (object->movedir << 29);

        if (delta > 0)
            object->angle -= kBAMAngle45;
        else if (delta < 0)
            object->angle += kBAMAngle45;
    }

    if (!object->target || !(object->target->flags & kMapObjectFlagShootable))
    {
        if (P_ActLookForTargets(object))
            return;

        // -ACB- 1998/09/06 Target is not relevant: nullptrify.
        object->SetTarget(nullptr);

        P_SetMobjStateDeferred(object, object->info->idle_state_, 0);
        return;
    }

    // do not attack twice in a row
    if (object->flags & kMapObjectFlagJustAttacked)
    {
        object->flags &= ~kMapObjectFlagJustAttacked;

        // -KM- 1998/12/16 Nightmare mode set the fast parm.
        if (!level_flags.fastparm)
            P_NewChaseDir(object);

        return;
    }

    sound = object->info->attacksound_;

    // check for melee attack
    if (object->info->melee_state_ && DecideMeleeAttack(object, object->info->closecombat_))
    {
        if (sound)
            S_StartFX(sound, P_MobjGetSfxCategory(object), object);

        if (object->info->melee_state_)
            P_SetMobjStateDeferred(object, object->info->melee_state_, 0);
        return;
    }

    // check for missile attack
    if (object->info->missile_state_)
    {
        // -KM- 1998/12/16 Nightmare set the fastparm.
        if (!(!level_flags.fastparm && object->movecount))
        {
            if (DecideRangeAttack(object))
            {
                if (object->info->missile_state_)
                    P_SetMobjStateDeferred(object, object->info->missile_state_, 0);
                object->flags |= kMapObjectFlagJustAttacked;
                return;
            }
        }
    }

    // possibly choose another target
    // -ACB- 1998/09/05 Object->support->object check, go for new targets
    if (!P_CheckSight(object, object->target) && !object->threshold)
    {
        if (P_ActLookForTargets(object))
            return;
    }

    // chase towards player
    if (--object->movecount < 0 || !P_Move(object, false))
        P_NewChaseDir(object);

    // make active sound
    if (object->info->activesound_ && Random8BitStateless() < 3)
        S_StartFX(object->info->activesound_, P_MobjGetSfxCategory(object), object);
}

//
// P_ActResurrectChase
//
// Before undertaking the standard chase procedure, the object
// will check for a nearby corpse and raises one if it exists.
//
// -ACB- 1998/09/05 Support Check: Raised object supports raiser's supportobj
//
void P_ActResurrectChase(mobj_t *object)
{
    mobj_t *corpse;

    corpse = P_MapFindCorpse(object);

    if (corpse)
    {
        object->angle = R_PointToAngle(object->x, object->y, corpse->x, corpse->y);
        if (object->info->res_state_)
            P_SetMobjStateDeferred(object, object->info->res_state_, 0);

        // corpses without raise states should be skipped
        SYS_ASSERT(corpse->info->raise_state_);

        P_BringCorpseToLife(corpse);

        // -ACB- 1998/09/05 Support Check: Res creatures to support that object
        if (object->supportobj)
        {
            corpse->SetSupportObj(object->supportobj);
            corpse->SetTarget(object->target);
        }
        else
        {
            corpse->SetSupportObj(nullptr);
            corpse->SetTarget(nullptr);
        }

        // -AJA- Resurrected creatures are on Archvile's side (like MBF)
        corpse->side = object->side;
        return;
    }

    P_ActStandardChase(object);
}

//
// P_ActWalkSoundChase
//
// Make a sound and then chase...
//
void P_ActWalkSoundChase(mobj_t *object)
{
    if (!object->info->walksound_)
    {
        PrintWarningOrError("WALKSOUND_CHASE: %s hasn't got a walksound_.\n", object->info->name_.c_str());
        return;
    }

    S_StartFX(object->info->walksound_, P_MobjGetSfxCategory(object), object);
    P_ActStandardChase(object);
}

void P_ActDie(mobj_t *mo)
{
    // Boom/MBF compatibility.

    mo->health = 0;
    P_KillMobj(nullptr, mo);
}

void P_ActKeenDie(mobj_t *mo)
{
    P_ActMakeIntoCorpse(mo);

    // see if all other Keens are dead
    for (mobj_t *cur = mobjlisthead; cur != nullptr; cur = cur->next)
    {
        if (cur == mo)
            continue;

        if (cur->info != mo->info)
            continue;

        if (cur->health > 0)
            return; // other Keen not dead
    }

    LogDebug("P_ActKeenDie: ALL DEAD, activating...\n");

    P_RemoteActivation(nullptr, 2 /* door type */, 666 /* tag */, 0, kLineTriggerAny);
}

void P_ActCheckMoving(mobj_t *mo)
{
    // -KM- 1999/01/31 Returns a player to spawnstate when not moving.

    player_t *pl = mo->player;

    if (pl)
    {
        if (pl->actual_speed < PLAYER_STOPSPEED)
        {
            P_SetMobjStateDeferred(mo, mo->info->idle_state_, 0);

            // we delay a little bit, in order to prevent a loop where
            // CHECK_ACTIVITY jumps to SWIM states (for example) and
            // then CHECK_MOVING jumps right back to IDLE states.
            mo->tics = 2;
        }
        return;
    }

    if (fabs(mo->mom.X) < STOPSPEED && fabs(mo->mom.Y) < STOPSPEED)
    {
        mo->mom.X = mo->mom.Y = 0;
        P_SetMobjStateDeferred(mo, mo->info->idle_state_, 0);
    }
}

void P_ActCheckActivity(mobj_t *mo)
{
    player_t *pl = mo->player;

    if (!pl)
        return;

    if (pl->swimming)
    {
        // enter the SWIM states (if present)
        int swim_st = P_MobjFindLabel(pl->mo, "SWIM");

        if (swim_st == 0)
            swim_st = pl->mo->info->chase_state_;

        if (swim_st != 0)
            P_SetMobjStateDeferred(pl->mo, swim_st, 0);

        return;
    }

    if (pl->powers[kPowerTypeJetpack] > 0)
    {
        // enter the FLY states (if present)
        int fly_st = P_MobjFindLabel(pl->mo, "FLY");

        if (fly_st != 0)
            P_SetMobjStateDeferred(pl->mo, fly_st, 0);

        return;
    }

    if (mo->on_ladder >= 0)
    {
        // enter the CLIMB states (if present)
        int climb_st = P_MobjFindLabel(pl->mo, "CLIMB");

        if (climb_st != 0)
            P_SetMobjStateDeferred(pl->mo, climb_st, 0);

        return;
    }

    // Lobo 2022: use crouch states if we have them and we are, you know, crouching ;)
    if (pl->mo->extendedflags & kExtendedFlagCrouching)
    {
        // enter the CROUCH states (if present)
        int crouch_st = P_MobjFindLabel(pl->mo, "CROUCH");

        if (crouch_st != 0)
            P_SetMobjStateDeferred(pl->mo, crouch_st, 0);

        return;
    }

    /* Otherwise: do nothing */
}

void P_ActCheckBlood(mobj_t *mo)
{
    // -KM- 1999/01/31 Part of the extra blood option, makes blood stick around...
    // -AJA- 1999/10/02: ...but not indefinitely.

    if (level_flags.more_blood && mo->tics >= 0)
    {
        int val = Random8BitStateful();

        // exponential formula
        mo->tics = ((val * val * val) >> 18) * kTicRate + kTicRate;
    }
}

void P_ActJump(mobj_t *mo)
{
    // Jumps to the given label, possibly randomly.
    //
    // Note: nothing to do with monsters physically jumping.

    if (!mo->state || !mo->state->action_par)
    {
        PrintWarningOrError("JUMP action used in [%s] without a label !\n", mo->info->name_.c_str());
        return;
    }

    JumpActionInfo *jump = (JumpActionInfo *)mo->state->action_par;

    SYS_ASSERT(jump->chance >= 0);
    SYS_ASSERT(jump->chance <= 1);

    if (Random8BitTestStateful(jump->chance))
    {
        mo->next_state = (mo->state->jumpstate == 0) ? nullptr : (states + mo->state->jumpstate);
    }
}

void P_ActJumpLiquid(mobj_t *mo)
{
    // Jumps to the given label, possibly randomly.
    //
    // Note: nothing to do with monsters physically jumping.

    if (!P_IsThingOnLiquidFloor(mo)) // Are we touching a liquid floor?
    {
        return;
    }

    if (!mo->state || !mo->state->action_par)
    {
        PrintWarningOrError("JUMP_LIQUID action used in [%s] without a label !\n", mo->info->name_.c_str());
        return;
    }

    JumpActionInfo *jump = (JumpActionInfo *)mo->state->action_par;

    SYS_ASSERT(jump->chance >= 0);
    SYS_ASSERT(jump->chance <= 1);

    if (Random8BitTestStateful(jump->chance))
    {
        mo->next_state = (mo->state->jumpstate == 0) ? nullptr : (states + mo->state->jumpstate);
    }
}

void P_ActJumpSky(mobj_t *mo)
{
    // Jumps to the given label, possibly randomly.
    //
    // Note: nothing to do with monsters physically jumping.

    if (mo->subsector->sector->ceil.image != skyflatimage) // is it outdoors?
    {
        return;
    }
    if (!mo->state || !mo->state->action_par)
    {
        PrintWarningOrError("JUMP_SKY action used in [%s] without a label !\n", mo->info->name_.c_str());
        return;
    }

    JumpActionInfo *jump = (JumpActionInfo *)mo->state->action_par;

    SYS_ASSERT(jump->chance >= 0);
    SYS_ASSERT(jump->chance <= 1);

    if (Random8BitTestStateful(jump->chance))
    {
        mo->next_state = (mo->state->jumpstate == 0) ? nullptr : (states + mo->state->jumpstate);
    }
}

/*
void P_ActJumpStuck(mobj_t * mo)
{
    // Jumps to the given label, possibly randomly.
    //
    // Note: nothing to do with monsters physically jumping.

    if (mo->mom.X > 0.1f && mo->mom.Y > 0.1f)
    {
        return;
    }

    if (!mo->state || !mo->state->action_par)
    {
        PrintWarningOrError("JUMP_STUCK action used in [%s] without a label !\n",
                    mo->info->name_.c_str());
        return;
    }

    JumpActionInfo *jump = (JumpActionInfo *) mo->state->action_par;

    SYS_ASSERT(jump->chance >= 0);
    SYS_ASSERT(jump->chance <= 1);

    if (Random8BitTestStateful(jump->chance))
    {
        mo->next_state = (mo->state->jumpstate == 0) ?
            nullptr : (states + mo->state->jumpstate);
    }
}
*/

void P_ActSetInvuln(struct mobj_s *mo)
{
    mo->hyperflags |= kHyperFlagInvulnerable;
}

void P_ActClearInvuln(struct mobj_s *mo)
{
    mo->hyperflags &= ~kHyperFlagInvulnerable;
}

void P_ActBecome(struct mobj_s *mo)
{
    if (!mo->state || !mo->state->action_par)
    {
        FatalError("BECOME action used in [%s] without arguments!\n", mo->info->name_.c_str());
        return; /* NOT REACHED */
    }

    BecomeActionInfo *become = (BecomeActionInfo *)mo->state->action_par;

    if (!become->info_)
    {
        become->info_ = mobjtypes.Lookup(become->info_ref_.c_str());
        SYS_ASSERT(become->info_); // lookup should be OK (fatal error if not found)
    }

    // DO THE DEED !!
    mo->preBecome = mo->info; // store what we used to be

    P_UnsetThingPosition(mo);
    {
        mo->info = become->info_;

        mo->morphtimeout = mo->info->morphtimeout_;

        // Note: health is not changed
        mo->radius = mo->info->radius_;
        mo->height = mo->info->height_;
        if (mo->info->fast_speed_ > -1 && level_flags.fastparm)
            mo->speed = mo->info->fast_speed_;
        else
            mo->speed = mo->info->speed_;

        if (mo->flags & kMapObjectFlagAmbush) // preserve map editor AMBUSH flag
        {
            mo->flags = mo->info->flags_;
            mo->flags |= kMapObjectFlagAmbush;
        }
        else
            mo->flags = mo->info->flags_;

        mo->extendedflags = mo->info->extendedflags_;
        mo->hyperflags    = mo->info->hyperflags_;

        mo->vis_target       = mo->info->translucency_;
        mo->currentattack    = nullptr;
        mo->model_skin       = mo->info->model_skin_;
        mo->model_last_frame = -1;

        mo->painchance = mo->info->painchance_;

        // handle dynamic lights
        {
            const DynamicLightDefinition *dinfo = &mo->info->dlight_[0];

            if (dinfo->type_ != kDynamicLightTypeNone)
            {
                mo->dlight.target = dinfo->radius_;
                mo->dlight.color  = dinfo->colour_;

                // make renderer re-create shader info
                if (mo->dlight.shader)
                {
                    // FIXME: delete mo->dlight.shader;
                    mo->dlight.shader = nullptr;
                }
            }
        }
    }
    P_SetThingPosition(mo);

    int state = P_MobjFindLabel(mo, become->start_.label_.c_str());
    if (state == 0)
        FatalError("BECOME action: frame '%s' in [%s] not found!\n", become->start_.label_.c_str(), mo->info->name_.c_str());

    state += become->start_.offset_;

    P_SetMobjStateDeferred(mo, state, 0);
}

void P_ActUnBecome(struct mobj_s *mo)
{

    if (!mo->preBecome)
    {
        return;
    }

    const MapObjectDefinition *preBecome = nullptr;
    preBecome                   = mo->preBecome;

    // DO THE DEED !!
    mo->preBecome = nullptr; // remove old reference

    P_UnsetThingPosition(mo);
    {
        mo->info = preBecome;

        mo->morphtimeout = mo->info->morphtimeout_;

        mo->radius = mo->info->radius_;
        mo->height = mo->info->height_;
        if (mo->info->fast_speed_ > -1 && level_flags.fastparm)
            mo->speed = mo->info->fast_speed_;
        else
            mo->speed = mo->info->speed_;

        // Note: health is not changed
        if (mo->flags & kMapObjectFlagAmbush) // preserve map editor AMBUSH flag
        {
            mo->flags = mo->info->flags_;
            mo->flags |= kMapObjectFlagAmbush;
        }
        else
            mo->flags = mo->info->flags_;

        mo->extendedflags = mo->info->extendedflags_;
        mo->hyperflags    = mo->info->hyperflags_;

        mo->vis_target       = mo->info->translucency_;
        mo->currentattack    = nullptr;
        mo->model_skin       = mo->info->model_skin_;
        mo->model_last_frame = -1;

        mo->painchance = mo->info->painchance_;

        // handle dynamic lights
        {
            const DynamicLightDefinition *dinfo = &mo->info->dlight_[0];

            if (dinfo->type_ != kDynamicLightTypeNone)
            {
                mo->dlight.target = dinfo->radius_;
                mo->dlight.color  = dinfo->colour_;

                // make renderer re-create shader info
                if (mo->dlight.shader)
                {
                    // FIXME: delete mo->dlight.shader;
                    mo->dlight.shader = nullptr;
                }
            }
        }
    }
    P_SetThingPosition(mo);

    int state = P_MobjFindLabel(mo, "IDLE");
    if (state == 0)
        FatalError("UNBECOME action: frame '%s' in [%s] not found!\n", "IDLE", mo->info->name_.c_str());

    P_SetMobjStateDeferred(mo, state, 0);
}

// Same as P_ActBecome, but health is set to max
void P_ActMorph(struct mobj_s *mo)
{
    if (!mo->state || !mo->state->action_par)
    {
        FatalError("MORPH action used in [%s] without arguments!\n", mo->info->name_.c_str());
        return; /* NOT REACHED */
    }

    MorphActionInfo *morph = (MorphActionInfo *)mo->state->action_par;

    if (!morph->info_)
    {
        morph->info_ = mobjtypes.Lookup(morph->info_ref_.c_str());
        SYS_ASSERT(morph->info_); // lookup should be OK (fatal error if not found)
    }

    // DO THE DEED !!
    mo->preBecome = mo->info; // store what we used to be

    P_UnsetThingPosition(mo);
    {
        mo->info   = morph->info_;
        mo->health = mo->info->spawnhealth_; // Set health to full again

        mo->morphtimeout = mo->info->morphtimeout_;

        mo->radius = mo->info->radius_;
        mo->height = mo->info->height_;
        if (mo->info->fast_speed_ > -1 && level_flags.fastparm)
            mo->speed = mo->info->fast_speed_;
        else
            mo->speed = mo->info->speed_;

        if (mo->flags & kMapObjectFlagAmbush) // preserve map editor AMBUSH flag
        {
            mo->flags = mo->info->flags_;
            mo->flags |= kMapObjectFlagAmbush;
        }
        else
            mo->flags = mo->info->flags_;

        mo->extendedflags = mo->info->extendedflags_;
        mo->hyperflags    = mo->info->hyperflags_;

        mo->vis_target       = mo->info->translucency_;
        mo->currentattack    = nullptr;
        mo->model_skin       = mo->info->model_skin_;
        mo->model_last_frame = -1;

        mo->painchance = mo->info->painchance_;

        // handle dynamic lights
        {
            const DynamicLightDefinition *dinfo = &mo->info->dlight_[0];

            if (dinfo->type_ != kDynamicLightTypeNone)
            {
                mo->dlight.target = dinfo->radius_;
                mo->dlight.color  = dinfo->colour_;

                // make renderer re-create shader info
                if (mo->dlight.shader)
                {
                    // FIXME: delete mo->dlight.shader;
                    mo->dlight.shader = nullptr;
                }
            }
        }
    }
    P_SetThingPosition(mo);

    int state = P_MobjFindLabel(mo, morph->start_.label_.c_str());
    if (state == 0)
        FatalError("MORPH action: frame '%s' in [%s] not found!\n", morph->start_.label_.c_str(), mo->info->name_.c_str());

    state += morph->start_.offset_;

    P_SetMobjStateDeferred(mo, state, 0);
}

// Same as P_ActUnBecome, but health is set to max
void P_ActUnMorph(struct mobj_s *mo)
{

    if (!mo->preBecome)
    {
        return;
    }

    const MapObjectDefinition *preBecome = nullptr;
    preBecome                   = mo->preBecome;

    // DO THE DEED !!
    mo->preBecome = nullptr; // remove old reference

    P_UnsetThingPosition(mo);
    {
        mo->info = preBecome;

        mo->health = mo->info->spawnhealth_; // Set health to max again

        mo->morphtimeout = mo->info->morphtimeout_;

        mo->radius = mo->info->radius_;
        mo->height = mo->info->height_;
        if (mo->info->fast_speed_ > -1 && level_flags.fastparm)
            mo->speed = mo->info->fast_speed_;
        else
            mo->speed = mo->info->speed_;

        // Note: health is not changed

        if (mo->flags & kMapObjectFlagAmbush) // preserve map editor AMBUSH flag
        {
            mo->flags = mo->info->flags_;
            mo->flags |= kMapObjectFlagAmbush;
        }
        else
            mo->flags = mo->info->flags_;

        mo->extendedflags = mo->info->extendedflags_;
        mo->hyperflags    = mo->info->hyperflags_;

        mo->vis_target       = mo->info->translucency_;
        mo->currentattack    = nullptr;
        mo->model_skin       = mo->info->model_skin_;
        mo->model_last_frame = -1;

        mo->painchance = mo->info->painchance_;

        // handle dynamic lights
        {
            const DynamicLightDefinition *dinfo = &mo->info->dlight_[0];

            if (dinfo->type_ != kDynamicLightTypeNone)
            {
                mo->dlight.target = dinfo->radius_;
                mo->dlight.color  = dinfo->colour_;

                // make renderer re-create shader info
                if (mo->dlight.shader)
                {
                    // FIXME: delete mo->dlight.shader;
                    mo->dlight.shader = nullptr;
                }
            }
        }
    }
    P_SetThingPosition(mo);

    int state = P_MobjFindLabel(mo, "IDLE");
    if (state == 0)
        FatalError("UNMORPH action: frame '%s' in [%s] not found!\n", "IDLE", mo->info->name_.c_str());

    P_SetMobjStateDeferred(mo, state, 0);
}

// -AJA- 1999/08/08: New attack flag FORCEAIM, which fixes chainsaw.
//
void P_PlayerAttack(mobj_t *p_obj, const AttackDefinition *attack)
{
    SYS_ASSERT(attack);

    p_obj->currentattack = attack;

    if (attack->attackstyle_ != kAttackStyleDualAttack)
    {
        float range = (attack->range_ > 0) ? attack->range_ : MISSILERANGE;

        // see which target is to be aimed at
        mobj_t *target = P_MapTargetAutoAim(p_obj, p_obj->angle, range, (attack->flags_ & kAttackFlagForceAim) ? true : false);

        mobj_t *old_target = p_obj->target;

        p_obj->SetTarget(target);

        if (attack->flags_ & kAttackFlagFaceTarget)
        {
            if (attack->flags_ & kAttackFlagForceAim)
                P_ForceFaceTarget(p_obj);
            else
                P_ActFaceTarget(p_obj);
        }

        P_DoAttack(p_obj);
        // restore the previous target for bots
        if (p_obj->player && (p_obj->player->playerflags & PFL_Bot))
            p_obj->SetTarget(old_target);
    }
    else
    {
        SYS_ASSERT(attack->dualattack1_ && attack->dualattack2_);

        if (attack->dualattack1_->attackstyle_ == kAttackStyleDualAttack)
            P_PlayerAttack(p_obj, attack->dualattack1_);
        else
        {
            p_obj->currentattack = attack->dualattack1_;

            float range = (p_obj->currentattack->range_ > 0) ? p_obj->currentattack->range_ : MISSILERANGE;

            // see which target is to be aimed at
            mobj_t *target = P_MapTargetAutoAim(p_obj, p_obj->angle, range,
                                                (p_obj->currentattack->flags_ & kAttackFlagForceAim) ? true : false);

            mobj_t *old_target = p_obj->target;

            p_obj->SetTarget(target);

            if (p_obj->currentattack->flags_ & kAttackFlagFaceTarget)
            {
                if (p_obj->currentattack->flags_ & kAttackFlagForceAim)
                    P_ForceFaceTarget(p_obj);
                else
                    P_ActFaceTarget(p_obj);
            }

            P_DoAttack(p_obj);
            // restore the previous target for bots
            if (p_obj->player && (p_obj->player->playerflags & PFL_Bot))
                p_obj->SetTarget(old_target);
        }

        if (attack->dualattack2_->attackstyle_ == kAttackStyleDualAttack)
            P_PlayerAttack(p_obj, attack->dualattack2_);
        else
        {
            p_obj->currentattack = attack->dualattack2_;

            float range = (p_obj->currentattack->range_ > 0) ? p_obj->currentattack->range_ : MISSILERANGE;

            // see which target is to be aimed at
            mobj_t *target = P_MapTargetAutoAim(p_obj, p_obj->angle, range,
                                                (p_obj->currentattack->flags_ & kAttackFlagForceAim) ? true : false);

            mobj_t *old_target = p_obj->target;

            p_obj->SetTarget(target);

            if (p_obj->currentattack->flags_ & kAttackFlagFaceTarget)
            {
                if (p_obj->currentattack->flags_ & kAttackFlagForceAim)
                    P_ForceFaceTarget(p_obj);
                else
                    P_ActFaceTarget(p_obj);
            }

            P_DoAttack(p_obj);
            // restore the previous target for bots
            if (p_obj->player && (p_obj->player->playerflags & PFL_Bot))
                p_obj->SetTarget(old_target);
        }
    }
}

//-------------------------------------------------------------------
//----------------------   MBF / MBF21  -----------------------------
//-------------------------------------------------------------------

//
// killough 9/98: a mushroom explosion effect, sorta :)
// Original idea: Linguica
//
void P_ActMushroom(struct mobj_s *mo)
{
    float height = 4.0;
    int   spread = 32;

    // First make normal explosion damage
    P_ActDamageExplosion(mo);

    // Now launch mushroom cloud
    const AttackDefinition *atk = mo->info->spareattack_;
    if (atk == nullptr)
        atk = atkdefs.Lookup("MUSHROOM_FIREBALL");
    if (atk == nullptr)
        return;

    for (int i = -spread; i <= spread; i += 16)
    {
        for (int j = -spread; j <= spread; j += 16)
        {
            // Aim in many directions from source
            float tx = mo->x + i;
            float ty = mo->y + j;
            float tz = mo->z + P_ApproxDistance(i, j) * height;

            mo->currentattack = atk;

            mobj_t *proj = DoLaunchProjectile(mo, tx, ty, tz, nullptr, atk->atk_mobj_);
            if (proj == nullptr)
                continue;
        }
    }
}

void P_ActPainChanceSet(mobj_t *mo)
{
    float value = 0;

    const State *st = mo->state;

    if (st && st->action_par)
    {
        value = ((float *)st->action_par)[0];
        value = HMM_MAX(0.0f, HMM_MIN(1.0f, value));
    }
    mo->painchance = value;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
