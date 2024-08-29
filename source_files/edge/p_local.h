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

#pragma once

#include "con_var.h"
#include "e_player.h"
#include "p_blockmap.h" // HACK!
#include "p_spec.h"

constexpr float kBoomCarryFactor = 0.09375f;
constexpr float kUseRange        = 64.0f;
constexpr float kUseZRange       = 32.0f;
constexpr float kMeleeRange      = 64.0f;
constexpr float kMissileRange    = 2000.0f;
constexpr float kOnFloorZ        = (float)INT_MIN;
constexpr float kOnCeilingZ      = (float)INT_MAX;

// -ACB- 2004/07/22 Moved here since its playsim related
#define EDGE_DAMAGE_COMPUTE(var, dam)                                                                                  \
    {                                                                                                                  \
        (var) = (dam)->nominal_;                                                                                       \
                                                                                                                       \
        if ((dam)->error_ > 0)                                                                                         \
            (var) += (dam)->error_ * RandomByteSkewToZeroDeterministic() / 255.0f;                                     \
        else if ((dam)->linear_max_ > 0)                                                                               \
            (var) += ((dam)->linear_max_ - (var)) * RandomByteDeterministic() / 255.0f;                                \
                                                                                                                       \
        if ((var) < 0)                                                                                                 \
            (var) = 0;                                                                                                 \
    }

//
// P_ACTION
//
extern ConsoleVariable force_infighting;

void PlayerAttack(MapObject *playerobj, const AttackDefinition *attack);
void SlammedIntoObject(MapObject *object, MapObject *objecthit);
int  MissileContact(MapObject *object, MapObject *objecthit);
int  BulletContact(MapObject *object, MapObject *objecthit, float damage, const DamageClass *damtype, float x, float y,
                   float z);
void TouchyContact(MapObject *touchy, MapObject *victim);
bool UseThing(MapObject *user, MapObject *thing, float open_bottom, float open_top);
void BringCorpseToLife(MapObject *corpse);

//
// P_WEAPON
//
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
void FixWeaponClip(Player *p, int slot);

//
// P_USER
//
void CreatePlayer(int pnum, bool is_bot);
void DestroyAllPlayers(void);
void GiveInitialBenefits(Player *player, const MapObjectDefinition *info);

bool PlayerThink(Player *player);
void UpdateAvailWeapons(Player *p);
void UpdateTotalArmour(Player *p);

bool AddWeapon(Player *player, WeaponDefinition *info, int *index);
bool RemoveWeapon(Player *player, WeaponDefinition *info);
bool PlayerSwitchWeapon(Player *player, WeaponDefinition *choice);
void PlayerJump(Player *pl, float dz, int wait);

//
// P_MOBJ
//

// -ACB- 1998/07/30 Start Pointer the item-respawn-que.
extern RespawnQueueItem *respawn_queue_head;

// -ACB- 1998/08/27 Start Pointer in the mobj list.
extern MapObject *map_object_list_head;

void RemoveMapObject(MapObject *th);
int  MapObjectFindLabel(MapObject *mobj, const char *label);
bool MapObjectSetState(MapObject *mobj, int state);
bool MapObjectSetStateDeferred(MapObject *mobj, int state, int tic_skip);
void MapObjectSetDirectionAndSpeed(MapObject *mobj, BAMAngle angle, float slope, float speed);
void RunMapObjectThinkers();
void SpawnDebris(float x, float y, float z, BAMAngle angle, const MapObjectDefinition *debris);
void SpawnPuff(float x, float y, float z, const MapObjectDefinition *puff, BAMAngle angle);
void SpawnBlood(float x, float y, float z, float damage, BAMAngle angle, const MapObjectDefinition *blood);
void CalculateFullRegionProperties(const MapObject *mo, RegionProperties *newregp);
bool HitLiquidFloor(MapObject *thing);

// -ACB- 1998/08/02 New procedures for DDF etc...
void       ItemRespawn(void);
void       RemoveMissile(MapObject *missile);
void       ExplodeMissile(MapObject *missile);
MapObject *CreateMapObject(float x, float y, float z, const MapObjectDefinition *type);

// -ACB- 2005/05/06 Sound Effect Category Support
int GetSoundEffectCategory(const MapObject *mo);

// Needed by savegame code.
void RemoveAllMapObjects(bool loading);
void ClearRespawnQueue(void);
void ClearAllStaleReferences(void);

//
// P_ENEMY
//

extern DirectionType opposite[];
extern DirectionType diagonals[];
extern float         xspeed[8];
extern float         yspeed[8];

void       NoiseAlert(Player *p);
void       NewChaseDir(MapObject *actor);
bool       DoMove(MapObject *actor, bool path);
bool       LookForPlayers(MapObject *actor, BAMAngle range);
MapObject *LookForShootSpot(const MapObjectDefinition *spot_type);

//
// P_MAPUTL
//
float ApproximateDistance(float dx, float dy);
float ApproximateDistance(float dx, float dy, float dz);
float ApproximateSlope(float dx, float dy, float dz);
int   PointOnDividingLineSide(float x, float y, DividingLine *div);
int   PointOnDividingLineThick(float x, float y, DividingLine *div, float div_len, float thickness);
void  ComputeIntersection(DividingLine *div, float x1, float y1, float x2, float y2, float *ix, float *iy);
int   BoxOnLineSide(const float *tmbox, Line *ld);
int   BoxOnDividingLineSide(const float *tmbox, DividingLine *div);
int   ThingOnLineSide(const MapObject *mo, Line *ld);

int   FindThingGap(VerticalGap *gaps, int gap_num, float z1, float z2);
void  ComputeGaps(Line *ld);
float ComputeThingGap(MapObject *thing, Sector *sec, float z, float *f, float *c, float floor_slope_z = 0.0f,
                      float ceiling_slope_z = 0.0f);
void  AddExtraFloor(Sector *sec, Line *line);
void  RecomputeGapsAroundSector(Sector *sec);
void  FloodExtraFloors(Sector *sector);

bool CheckAreaForThings(float *bbox);
bool CheckSliderPathForThings(Line *ld);

enum ExtrafloorFit
{
    kFitOk = 0,
    kFitStuckInCeiling,
    kFitStuckInFloor,
    kFitStuckInExtraFloor
};

ExtrafloorFit CheckExtrafloorFit(Sector *sec, float z1, float z2);

//
// P_MAP
//

// If "float_ok" true, move would be OK at float_destination_z height.
extern bool  float_ok;
extern float float_destination_z;

extern bool  map_object_hit_sky;
extern Line *block_line;

extern std::vector<Line *> special_lines_hit;

bool       MapCheckBlockingLine(MapObject *thing, MapObject *spawnthing);
MapObject *FindCorpseForResurrection(MapObject *thing);
MapObject *MapTargetAutoAim(MapObject *source, BAMAngle angle, float distance, bool force_aim);
MapObject *DoMapTargetAutoAim(MapObject *source, BAMAngle angle, float distance, bool force_aim);
void       TargetTheory(MapObject *source, MapObject *target, float *x, float *y, float *z);

MapObject *AimLineAttack(MapObject *t1, BAMAngle angle, float distance, float *slope);
bool       CheckSolidSectorMove(Sector *sec, bool is_ceiling, float dh);
bool       SolidSectorMove(Sector *sec, bool is_ceiling, float dh, int crush = 10, bool nocarething = false);
bool       CheckAbsolutePosition(MapObject *thing, float x, float y, float z);
bool       CheckSight(MapObject *src, MapObject *dest);
bool       CheckSightToPoint(MapObject *src, float x, float y, float z);
bool       QuickVerticalSightCheck(MapObject *src, MapObject *dest);
void       RadiusAttack(MapObject *spot, MapObject *source, float radius, float damage, const DamageClass *damtype,
                        bool thrust_only);

bool TeleportMove(MapObject *thing, float x, float y, float z);
bool TryMove(MapObject *thing, float x, float y);
void SlideMove(MapObject *mo, float x, float y);
void UseLines(Player *player);
void LineAttack(MapObject *t1, BAMAngle angle, float distance, float slope, float damage, const DamageClass *damtype,
                const MapObjectDefinition *puff, const MapObjectDefinition *blood);

void UnblockLineEffectDebris(Line *TheLine, const LineType *special);

MapObject *GetMapTargetAimInfo(MapObject *source, BAMAngle angle, float distance);

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
void DamageMapObject(MapObject *target, MapObject *inflictor, MapObject *source, float amount,
                     const DamageClass *damtype = nullptr, bool weak_spot = false);
void TelefragMapObject(MapObject *target, MapObject *inflictor, const DamageClass *damtype = nullptr);
void KillMapObject(MapObject *source, MapObject *target, const DamageClass *damtype = nullptr, bool weak_spot = false);
bool GiveBenefitList(Player *player, MapObject *special, Benefit *list, bool lose_them);
bool HasBenefitInList(Player *player, Benefit *list);

//
// P_SPEC
//

LineType   *LookupLineType(int num);
SectorType *LookupSectorType(int num);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
