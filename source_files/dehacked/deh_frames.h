//------------------------------------------------------------------------
//  FRAME Handling
//------------------------------------------------------------------------
//
//  DEH_EDGE  Copyright (C) 2004-2024 The EDGE Team
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License (in COPYING.txt) for more details.
//
//------------------------------------------------------------------------

#pragma once

namespace dehacked
{

struct State;

enum ActionFlags
{
    kActionFlagExplode    = (1 << 0),   // uses A_Explode
    kActionFlagBossDeath  = (1 << 1),   // uses A_BossDeath
    kActionFlagKeenDie    = (1 << 2),   // uses A_KeenDie
    kActionFlagLook       = (1 << 3),   // uses A_Look
    kActionFlagDetonate   = (1 << 4),   // uses A_Detonate
    kActionFlagSpread     = (1 << 6),   // uses A_FatAttack1/2/3
    kActionFlagChase      = (1 << 7),   // uses A_Chase
    kActionFlagFall       = (1 << 8),   // uses A_Fall
    kActionFlagRaise      = (1 << 9),   // uses A_ResChase
    kActionFlagFlash      = (1 << 14),  // weapon will go into flash state
    kActionFlagMakeDead   = (1 << 15),  // action needs an extra MAKEDEAD state
    kActionFlagFaceTarget = (1 << 16),  // action needs FACE_TARGET state
    kActionFlagSpecial    = (1 << 17),  // special action (uses misc1/2)
    kActionFlagUnimplemented = (1 << 18),  // not yet supported
    kActionFlagWeaponState   = (1 << 20),  // uses a weapon state
    kActionFlagThingState    = (1 << 21)   // uses a thing state
};

namespace frames
{

enum AttackMethod
{
    kAttackMethodRanged = 0,
    kAttackMethodCombat = 1,
    kAttackMethodSpare  = 2
};

extern const char *attack_slot[3];
extern int         act_flags;
extern bool        force_fullbright;  // DEHEXTRA compatibility

void Init();
void Shutdown();

void MarkState(int st_num);
void MarkStatesWithSprite(int spr_num);
void StateDependencies();

State *GetModifiedState(int st_num);
int    GetStateSprite(int st_num);

void AlterFrame(int new_val);
void AlterPointer(int new_val);
void AlterBexCodePtr(const char *new_action);

void ResetGroups();  // also resets the slots and flags
int  BeginGroup(char group, int first);
void SpreadGroups();

bool CheckWeaponFlash(int first);
bool CheckMissileState(int first);
void OutputGroup(char group);
}  // namespace frames

}  // namespace dehacked