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
#include "p_blockmap.h"  // HACK!
#include "r_defs.h"

#define DEATHVIEWHEIGHT 6.0f
#define CROUCH_SLOWDOWN 0.5f

#define BOOM_CARRY_FACTOR 0.09375f

#define MLOOK_LIMIT 0x53333355  // 75 degree BAM angle

#define MAXMOVE           (200.0f)
#define STEPMOVE          (16.0f)
#define USERANGE          (64.0f)
#define USE_Z_RANGE       (32.0f)
#define MELEERANGE        (64.0f)
#define LONGMELEERANGE    (128.0f)  // For kMBF21FlagLongMeleeRange
#define MISSILERANGE      (2000.0f)
#define SHORTMISSILERANGE (896.0f)  // For kMBF21FlagShortMissileRange

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
#define DAMAGE_COMPUTE(var, dam)                                              \
    {                                                                         \
        (var) = (dam)->nominal_;                                              \
                                                                              \
        if ((dam)->error_ > 0)                                                \
            (var) +=                                                          \
                (dam)->error_ * RandomByteSkewToZeroDeterministic() / 255.0f; \
        else if ((dam)->linear_max_ > 0)                                      \
            (var) += ((dam)->linear_max_ - (var)) *                           \
                     RandomByteDeterministic() / 255.0f;                      \
                                                                              \
        if ((var) < 0) (var) = 0;                                             \
    }

//
// P_ACTION
//
extern ConsoleVariable g_aggression;

void P_PlayerAttack(MapObject *playerobj, const AttackDefinition *attack);
void P_SlammedIntoObject(MapObject *object, MapObject *objecthit);
int  P_MissileContact(MapObject *object, MapObject *objecthit);
int  P_BulletContact(MapObject *object, MapObject *objecthit, float damage,
                     const DamageClass *damtype, float x, float y, float z);
void P_TouchyContact(MapObject *touchy, MapObject *victim);
bool P_UseThing(MapObject *user, MapObject *thing, float open_bottom,
                float open_top);
void BringCorpseToLife(MapObject *corpse);

//
// P_WEAPON
//
#define GRIN_TIME (kTicRate * 2)

void SetupPlayerSprites(Player *curplayer);
void MovePlayerSprites(Player *curplayer);
void DropWeapon(Player *player);
bool CheckWeaponSprite(WeaponDefinition *info);

void DesireWeaponChange(Player *p, int key);
void CycleWeapon(Player *p, int dir);
void SelectNewWeapon(Player *player, int priority, AmmunitionType ammo);
void TrySwitchNewWeapon(Player *p, int new_weap, AmmunitionType new_ammo);
bool TryFillNewWeapon(Player *p, int idx, AmmunitionType ammo, int *qty);
void FillWeapon(Player *p, int slot);
void P_FixWeaponClip(Player *p, int slot);

//
// P_USER
//
void P_CreatePlayer(int pnum, bool is_bot);
void DestroyAllPlayers(void);
void P_GiveInitialBenefits(Player *player, const MapObjectDefinition *info);

bool P_PlayerThink(Player *player, bool extra_tic);
void UpdateAvailWeapons(Player *p);
void UpdateTotalArmour(Player *p);

bool AddWeapon(Player *player, WeaponDefinition *info, int *index);
bool RemoveWeapon(Player *player, WeaponDefinition *info);
bool P_PlayerSwitchWeapon(Player *player, WeaponDefinition *choice);
void P_PlayerJump(Player *pl, float dz, int wait);

//
// P_MOBJ
//
#define ONFLOORZ   ((float)INT_MIN)
#define ONCEILINGZ ((float)INT_MAX)

// -ACB- 1998/07/30 Start Pointer the item-respawn-que.
extern RespawnQueueItem *respawn_queue_head;

// -ACB- 1998/08/27 Start Pointer in the mobj list.
extern MapObject *map_object_list_head;

void P_RemoveMobj(MapObject *th);
int  P_MobjFindLabel(MapObject *mobj, const char *label);
bool P_SetMobjState(MapObject *mobj, int state);
bool P_SetMobjStateDeferred(MapObject *mobj, int state, int tic_skip);
void P_SetMobjDirAndSpeed(MapObject *mobj, BAMAngle angle, float slope,
                          float speed);
void RunMobjThinkers(bool extra_tic);
void P_SpawnDebris(float x, float y, float z, BAMAngle angle,
                   const MapObjectDefinition *debris);
void P_SpawnPuff(float x, float y, float z, const MapObjectDefinition *puff,
                 BAMAngle angle);
void P_SpawnBlood(float x, float y, float z, float damage, BAMAngle angle,
                  const MapObjectDefinition *blood);
void P_CalcFullProperties(const MapObject *mo, RegionProperties *newregp);
bool P_HitLiquidFloor(MapObject *thing);

// -ACB- 1998/08/02 New procedures for DDF etc...
void       P_MobjItemRespawn(void);
void       P_MobjRemoveMissile(MapObject *missile);
void       P_MobjExplodeMissile(MapObject *missile);
MapObject *P_MobjCreateObject(float x, float y, float z,
                              const MapObjectDefinition *type);

// -ACB- 2005/05/06 Sound Effect Category Support
int P_MobjGetSfxCategory(const MapObject *mo);

// Needed by savegame code.
void P_RemoveAllMobjs(bool loading);
void P_RemoveItemsInQue(void);
void ClearAllStaleRefs(void);

//
// P_ENEMY
//

extern DirectionType opposite[];
extern DirectionType diagonals[];
extern float         xspeed[8];
extern float         yspeed[8];

void       NoiseAlert(Player *p);
void       NoiseAlert(MapObject *actor);
void       NewChaseDir(MapObject *actor);
bool       P_CreateAggression(MapObject *actor);
bool       P_CheckMeleeRange(MapObject *actor);
bool       P_CheckMissileRange(MapObject *actor);
bool       P_Move(MapObject *actor, bool path);
bool       P_LookForPlayers(MapObject *actor, BAMAngle range);
MapObject *P_LookForShootSpot(const MapObjectDefinition *spot_type);

//
// P_MAPUTL
//
float ApproximateDistance(float dx, float dy);
float ApproximateDistance(float dx, float dy, float dz);
float ApproximateSlope(float dx, float dy, float dz);
int   PointOnDividingLineSide(float x, float y, DividingLine *div);
int   PointOnDividingLineThick(float x, float y, DividingLine *div, float div_len,
                               float thickness);
void ComputeIntersection(DividingLine *div, float x1, float y1, float x2, float y2,
                         float *ix, float *iy);
int  BoxOnLineSide(const float *tmbox, Line *ld);
int  BoxOnDividingLineSide(const float *tmbox, DividingLine *div);
int  ThingOnLineSide(const MapObject *mo, Line *ld);

int   FindThingGap(VerticalGap *gaps, int gap_num, float z1, float z2);
void  ComputeGaps(Line *ld);
float ComputeThingGap(MapObject *thing, Sector *sec, float z, float *f,
                      float *c, float floor_slope_z = 0.0f, float ceiling_slope_z = 0.0f);
void  AddExtraFloor(Sector *sec, Line *line);
void  RecomputeGapsAroundSector(Sector *sec);
void  FloodExtraFloors(Sector *sector);

bool CheckAreaForThings(float *bbox);
bool CheckSliderPathForThings(Line *ld);

typedef enum
{
    EXFIT_Ok = 0,
    EXFIT_StuckInCeiling,
    EXFIT_StuckInFloor,
    EXFIT_StuckInExtraFloor
} exfloor_fit_e;

exfloor_fit_e CheckExtrafloorFit(Sector *sec, float z1, float z2);

//
// P_MAP
//

// --> Line list class
class linelist_c : public std::vector<Line *>
{
   public:
    linelist_c() {}
    ~linelist_c()
    {
        for (auto iter = begin(); iter != end(); iter++)
        {
            Line *l = *iter;
            delete l;
            l = nullptr;
        }
    }
};

// If "float_ok" true, move would be OK at float_destination_z height.
extern bool  float_ok;
extern float float_destination_z;

extern bool    map_object_hit_sky;
extern Line *block_line;

extern linelist_c special_lines_hit;

void       MapInitialize(void);
bool       MapCheckBlockingLine(MapObject *thing, MapObject *spawnthing);
MapObject *FindCorpseForResurrection(MapObject *thing);
MapObject *P_MapTargetAutoAim(MapObject *source, BAMAngle angle, float distance,
                              bool force_aim);
MapObject *DoMapTargetAutoAim(MapObject *source, BAMAngle angle, float distance,
                              bool force_aim);
void P_TargetTheory(MapObject *source, MapObject *target, float *x, float *y,
                    float *z);

MapObject *P_AimLineAttack(MapObject *t1, BAMAngle angle, float distance,
                           float *slope);
void       UpdateMultipleFloors(Sector *sector);
bool       CheckSolidSectorMove(Sector *sec, bool is_ceiling, float dh);
bool SolidSectorMove(Sector *sec, bool is_ceiling, float dh, int crush = 10,
                     bool nocarething = false);
bool CheckAbsolutePosition(MapObject *thing, float x, float y, float z);
bool P_CheckSight(MapObject *src, MapObject *dest);
bool CheckSightToPoint(MapObject *src, float x, float y, float z);
bool P_CheckSightApproxVert(MapObject *src, MapObject *dest);
void RadiusAttack(MapObject *spot, MapObject *source, float radius,
                  float damage, const DamageClass *damtype, bool thrust_only);

bool TeleportMove(MapObject *thing, float x, float y, float z);
bool TryMove(MapObject *thing, float x, float y);
void P_SlideMove(MapObject *mo, float x, float y);
void P_UseLines(Player *player);
void P_LineAttack(MapObject *t1, BAMAngle angle, float distance, float slope,
                  float damage, const DamageClass *damtype,
                  const MapObjectDefinition *puff);

void P_UnblockLineEffectDebris(Line *TheLine, const LineType *special);

MapObject *GetMapTargetAimInfo(MapObject *source, BAMAngle angle,
                               float distance);

bool ReplaceMidTexFromPart(Line *TheLine, ScrollingPart parts);

//
// P_SETUP
//
// 23-6-98 KM Short*s changed to int*s, for bigger, better blockmaps
// -AJA- 2000/07/31: line data changed back to shorts.
//

//
// P_INTER
//

void TouchSpecialThing(MapObject *special, MapObject *toucher);
void ThrustMapObject(MapObject *target, MapObject *inflictor, float thrust);
void PushMapObject(MapObject *target, MapObject *inflictor, float thrust);
void DamageMapObject(MapObject *target, MapObject *inflictor, MapObject *source,
                     float amount, const DamageClass *damtype = nullptr,
                     bool weak_spot = false);
void TelefragMapObject(MapObject *target, MapObject *inflictor,
                       const DamageClass *damtype = nullptr);
void KillMapObject(MapObject *source, MapObject *target,
                   const DamageClass *damtype   = nullptr,
                   bool               weak_spot = false);
bool GiveBenefitList(Player *player, MapObject *special, Benefit *list,
                     bool lose_them);
bool HasBenefitInList(Player *player, Benefit *list);

//
// P_SPEC
//
#include "p_spec.h"

LineType   *P_LookupLineType(int num);
SectorType *P_LookupSectorType(int num);

#endif  // __P_LOCAL__

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
