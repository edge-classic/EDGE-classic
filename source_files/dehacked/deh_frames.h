//------------------------------------------------------------------------
//  FRAME Handling
//------------------------------------------------------------------------
//
//  DEH_EDGE  Copyright (C) 2004-2023  The EDGE Team
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

#ifndef __DEH_FRAMES_HDR__
#define __DEH_FRAMES_HDR__

namespace Deh_Edge
{

struct state_t;

typedef enum
{
    AF_EXPLODE   = (1 << 0), // uses A_Explode
    AF_BOSSDEATH = (1 << 1), // uses A_BossDeath
    AF_KEENDIE   = (1 << 2), // uses A_KeenDie
    AF_LOOK      = (1 << 3), // uses A_Look
    AF_DETONATE  = (1 << 4), // uses A_Detonate

    AF_SPREAD = (1 << 6), // uses A_FatAttack1/2/3
    AF_CHASER = (1 << 7), // uses A_Chase
    AF_FALLER = (1 << 8), // uses A_Fall
    AF_RAISER = (1 << 9), // uses A_ResChase

    AF_FLASH    = (1 << 14), // weapon will go into flash state
    AF_MAKEDEAD = (1 << 15), // action needs an extra MAKEDEAD state
    AF_FACE     = (1 << 16), // action needs FACE_TARGET state
    AF_SPECIAL  = (1 << 17), // special action (uses misc1/2)
    AF_UNIMPL   = (1 << 18), // not yet supported

    AF_WEAPON_ST = (1 << 20), // uses a weapon state
    AF_THING_ST  = (1 << 21)  // uses a thing state
} actflags_e;

namespace Frames
{
typedef enum
{
    RANGE  = 0,
    COMBAT = 1,
    SPARE  = 2
} atkmethod_e;

extern const char *attack_slot[3];
extern int         act_flags;
extern bool        force_fullbright; // DEHEXTRA compatibility

void Init();
void Shutdown();

void MarkState(int st_num);
void MarkStatesWithSprite(int spr_num);
void StateDependencies();

state_t *GetModifiedState(int st_num);
int      GetStateSprite(int st_num);

void AlterFrame(int new_val);
void AlterPointer(int new_val);
void AlterBexCodePtr(const char *new_action);

void ResetGroups(); // also resets the slots and flags
int  BeginGroup(char group, int first);
void SpreadGroups();

bool CheckWeaponFlash(int first);
bool CheckMissileState(int first);
void OutputGroup(char group);

// debugging stuff
void DebugRange(const char *kind, const char *entry);
} // namespace Frames

} // namespace Deh_Edge

#endif /* __DEH_FRAMES_HDR__ */
