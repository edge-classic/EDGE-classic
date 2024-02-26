//----------------------------------------------------------------------------
//  EDGE Local Header for play sim functions
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
// -ACB- 1998/07/27 Cleaned up, Can read it now :).
//

#ifndef __P_LOCAL__
#define __P_LOCAL__

#include "con_var.h"
#include "e_player.h"
#include "p_blockmap.h" // HACK!
#include "r_defs.h"

#define DEATHVIEWHEIGHT 6.0f
#define CROUCH_SLOWDOWN 0.5f

#define BOOM_CARRY_FACTOR 0.09375f

#define MLOOK_LIMIT 0x53333355 // 75 degree BAM angle

#define MAXMOVE           (200.0f)
#define STEPMOVE          (16.0f)
#define USERANGE          (64.0f)
#define USE_Z_RANGE       (32.0f)
#define MELEERANGE        (64.0f)
#define LONGMELEERANGE    (128.0f) // For kMBF21FlagLongMeleeRange
#define MISSILERANGE      (2000.0f)
#define SHORTMISSILERANGE (896.0f) // For kMBF21FlagShortMissileRange

#define RESPAWN_DELAY (kTicRate / 2)

// Weapon sprite speeds
#define LOWERSPEED (6.0f)
#define RAISESPEED (6.0f)

#define WPNLOWERSPEED (6.0f)
#define WPNRAISESPEED (6.0f)

#define WEAPONBOTTOM 128
#define WEAPONTOP    32

// follow a player exlusively for 3 seconds
#define BASETHRESHOLD 100

// -ACB- 2004/07/22 Moved here since its playsim related
#define DAMAGE_COMPUTE(var, dam)                                                                                       \
    {                                                                                                                  \
        (var) = (dam)->nominal_;                                                                                        \
                                                                                                                       \
        if ((dam)->error_ > 0)                                                                                          \
            (var) += (dam)->error_ * RandomByteSkewToZeroDeterministic() / 255.0f;                                                         \
        else if ((dam)->linear_max_ > 0)                                                                                \
            (var) += ((dam)->linear_max_ - (var)) * RandomByteDeterministic() / 255.0f;                                                \
                                                                                                                       \
        if ((var) < 0)                                                                                                 \
            (var) = 0;                                                                                                 \
    }

//
// P_ACTION
//
extern ConsoleVariable g_aggression;

void P_PlayerAttack(mobj_t *playerobj, const AttackDefinition *attack);
void P_SlammedIntoObject(mobj_t *object, mobj_t *objecthit);
int  P_MissileContact(mobj_t *object, mobj_t *objecthit);
int  P_BulletContact(mobj_t *object, mobj_t *objecthit, float damage, const DamageClass *damtype, float x, float y,
                     float z);
void P_TouchyContact(mobj_t *touchy, mobj_t *victim);
bool P_UseThing(mobj_t *user, mobj_t *thing, float open_bottom, float open_top);
void P_BringCorpseToLife(mobj_t *corpse);

//
// P_WEAPON
//
#define GRIN_TIME (kTicRate * 2)

void P_SetupPsprites(player_t *curplayer);
void P_MovePsprites(player_t *curplayer);
void P_DropWeapon(player_t *player);
bool P_CheckWeaponSprite(WeaponDefinition *info);

void P_DesireWeaponChange(player_t *p, int key);
void P_NextPrevWeapon(player_t *p, int dir);
void P_SelectNewWeapon(player_t *player, int priority, AmmunitionType ammo);
void P_TrySwitchNewWeapon(player_t *p, int new_weap, AmmunitionType new_ammo);
bool P_TryFillNewWeapon(player_t *p, int idx, AmmunitionType ammo, int *qty);
void P_FillWeapon(player_t *p, int slot);
void P_FixWeaponClip(player_t *p, int slot);

//
// P_USER
//
void P_CreatePlayer(int pnum, bool is_bot);
void P_DestroyAllPlayers(void);
void P_GiveInitialBenefits(player_t *player, const MapObjectDefinition *info);

bool P_PlayerThink(player_t *player, bool extra_tic);
void P_UpdateAvailWeapons(player_t *p);
void P_UpdateTotalArmour(player_t *p);

bool P_AddWeapon(player_t *player, WeaponDefinition *info, int *index);
bool P_RemoveWeapon(player_t *player, WeaponDefinition *info);
bool P_PlayerSwitchWeapon(player_t *player, WeaponDefinition *choice);
void P_PlayerJump(player_t *pl, float dz, int wait);

//
// P_MOBJ
//
#define ONFLOORZ   ((float)INT_MIN)
#define ONCEILINGZ ((float)INT_MAX)

// -ACB- 1998/07/30 Start Pointer the item-respawn-que.
extern iteminque_t *itemquehead;

// -ACB- 1998/08/27 Start Pointer in the mobj list.
extern mobj_t *mobjlisthead;

void       P_RemoveMobj(mobj_t *th);
int P_MobjFindLabel(mobj_t *mobj, const char *label);
bool       P_SetMobjState(mobj_t *mobj, int state);
bool       P_SetMobjStateDeferred(mobj_t *mobj, int state, int tic_skip);
void       P_SetMobjDirAndSpeed(mobj_t *mobj, BAMAngle angle, float slope, float speed);
void       P_RunMobjThinkers(bool extra_tic);
void       P_SpawnDebris(float x, float y, float z, BAMAngle angle, const MapObjectDefinition *debris);
void       P_SpawnPuff(float x, float y, float z, const MapObjectDefinition *puff, BAMAngle angle);
void       P_SpawnBlood(float x, float y, float z, float damage, BAMAngle angle, const MapObjectDefinition *blood);
void       P_CalcFullProperties(const mobj_t *mo, region_properties_t *newregp);
bool       P_HitLiquidFloor(mobj_t *thing);

// -ACB- 1998/08/02 New procedures for DDF etc...
void    P_MobjItemRespawn(void);
void    P_MobjRemoveMissile(mobj_t *missile);
void    P_MobjExplodeMissile(mobj_t *missile);
mobj_t *P_MobjCreateObject(float x, float y, float z, const MapObjectDefinition *type);

// -ACB- 2005/05/06 Sound Effect Category Support
int P_MobjGetSfxCategory(const mobj_t *mo);

// Needed by savegame code.
void P_RemoveAllMobjs(bool loading);
void P_RemoveItemsInQue(void);
void P_ClearAllStaleRefs(void);

//
// P_ENEMY
//

extern dirtype_e opposite[];
extern dirtype_e diags[];
extern float     xspeed[8];
extern float     yspeed[8];

void    P_NoiseAlert(player_t *p);
void    P_NoiseAlert(mobj_t *actor);
void    P_NewChaseDir(mobj_t *actor);
bool    P_CreateAggression(mobj_t *actor);
bool    P_CheckMeleeRange(mobj_t *actor);
bool    P_CheckMissileRange(mobj_t *actor);
bool    P_Move(mobj_t *actor, bool path);
bool    P_LookForPlayers(mobj_t *actor, BAMAngle range);
mobj_t *P_LookForShootSpot(const MapObjectDefinition *spot_type);

//
// P_MAPUTL
//
float P_ApproxDistance(float dx, float dy);
float P_ApproxDistance(float dx, float dy, float dz);
float P_ApproxSlope(float dx, float dy, float dz);
int   P_PointOnDivlineSide(float x, float y, divline_t *div);
int   P_PointOnDivlineThick(float x, float y, divline_t *div, float div_len, float thickness);
void  P_ComputeIntersection(divline_t *div, float x1, float y1, float x2, float y2, float *ix, float *iy);
int   P_BoxOnLineSide(const float *tmbox, line_t *ld);
int   P_BoxOnDivLineSide(const float *tmbox, divline_t *div);
int   P_ThingOnLineSide(const mobj_t *mo, line_t *ld);

int   P_FindThingGap(vgap_t *gaps, int gap_num, float z1, float z2);
void  P_ComputeGaps(line_t *ld);
float P_ComputeThingGap(mobj_t *thing, sector_t *sec, float z, float *f, float *c, float f_slope_z = 0.0f,
                        float c_slope_z = 0.0f);
void  P_AddExtraFloor(sector_t *sec, line_t *line);
void  P_RecomputeGapsAroundSector(sector_t *sec);
void  P_FloodExtraFloors(sector_t *sector);

bool P_ThingsInArea(float *bbox);
bool P_ThingsOnSliderPath(line_t *ld);

typedef enum
{
    EXFIT_Ok = 0,
    EXFIT_StuckInCeiling,
    EXFIT_StuckInFloor,
    EXFIT_StuckInExtraFloor
} exfloor_fit_e;

exfloor_fit_e P_ExtraFloorFits(sector_t *sec, float z1, float z2);

//
// P_MAP
//

// --> Line list class
class linelist_c : public std::vector<line_t *>
{
  public:
    linelist_c()
    {
    }
    ~linelist_c()
    {
        for (auto iter = begin(); iter != end(); iter++)
        {
            line_t *l = *iter;
            delete l;
            l = nullptr;
        }
    }
};

// If "floatok" true, move would be OK at float_destz height.
extern bool  floatok;
extern float float_destz;

extern bool    mobj_hit_sky;
extern line_t *blockline;

extern linelist_c spechit;

void    P_MapInit(void);
bool    P_MapCheckBlockingLine(mobj_t *thing, mobj_t *spawnthing);
mobj_t *P_MapFindCorpse(mobj_t *thing);
mobj_t *P_MapTargetAutoAim(mobj_t *source, BAMAngle angle, float distance, bool force_aim);
mobj_t *DoMapTargetAutoAim(mobj_t *source, BAMAngle angle, float distance, bool force_aim);
void    P_TargetTheory(mobj_t *source, mobj_t *target, float *x, float *y, float *z);

mobj_t *P_AimLineAttack(mobj_t *t1, BAMAngle angle, float distance, float *slope);
void    P_UpdateMultipleFloors(sector_t *sector);
bool    P_CheckSolidSectorMove(sector_t *sec, bool is_ceiling, float dh);
bool    P_SolidSectorMove(sector_t *sec, bool is_ceiling, float dh, int crush = 10, bool nocarething = false);
bool    P_CheckAbsPosition(mobj_t *thing, float x, float y, float z);
bool    P_CheckSight(mobj_t *src, mobj_t *dest);
bool    P_CheckSightToPoint(mobj_t *src, float x, float y, float z);
bool    P_CheckSightApproxVert(mobj_t *src, mobj_t *dest);
void    P_RadiusAttack(mobj_t *spot, mobj_t *source, float radius, float damage, const DamageClass *damtype,
                       bool thrust_only);

bool P_TeleportMove(mobj_t *thing, float x, float y, float z);
bool P_TryMove(mobj_t *thing, float x, float y);
void P_SlideMove(mobj_t *mo, float x, float y);
void P_UseLines(player_t *player);
void P_LineAttack(mobj_t *t1, BAMAngle angle, float distance, float slope, float damage, const DamageClass *damtype,
                  const MapObjectDefinition *puff);

void P_UnblockLineEffectDebris(line_t *TheLine, const LineType *special);

mobj_t *GetMapTargetAimInfo(mobj_t *source, BAMAngle angle, float distance);

bool ReplaceMidTexFromPart(line_t *TheLine, ScrollingPart parts);

//
// P_SETUP
//
// 23-6-98 KM Short*s changed to int*s, for bigger, better blockmaps
// -AJA- 2000/07/31: line data changed back to shorts.
//

//
// P_INTER
//

void P_TouchSpecialThing(mobj_t *special, mobj_t *toucher);
void P_ThrustMobj(mobj_t *target, mobj_t *inflictor, float thrust);
void P_PushMobj(mobj_t *target, mobj_t *inflictor, float thrust);
void P_DamageMobj(mobj_t *target, mobj_t *inflictor, mobj_t *source, float amount, const DamageClass *damtype = nullptr,
                  bool weak_spot = false);
void P_TelefragMobj(mobj_t *target, mobj_t *inflictor, const DamageClass *damtype = nullptr);
void P_KillMobj(mobj_t *source, mobj_t *target, const DamageClass *damtype = nullptr, bool weak_spot = false);
bool P_GiveBenefitList(player_t *player, mobj_t *special, Benefit *list, bool lose_em);
bool P_HasBenefitInList(player_t *player, Benefit *list);

//
// P_SPEC
//
#include "p_spec.h"

LineType   *P_LookupLineType(int num);
SectorType *P_LookupSectorType(int num);

#endif // __P_LOCAL__

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
