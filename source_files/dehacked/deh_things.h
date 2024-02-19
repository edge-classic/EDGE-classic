//------------------------------------------------------------------------
//  THING conversion
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

#include "deh_mobj.h"

namespace dehacked
{

namespace things
{
void Init();
void Shutdown();

void UseThing(int mt_num);
void MarkThing(int mt_num);  // attacks too
void MarkAllMonsters();

DehackedMapObjectDefinition   *GetModifiedMobj(int mt_num);
const char *GetMobjName(int mt_num);
int         GetMobjMBF21Flags(int mt_num);

bool IsSpawnable(int mt_num);

void        SetPlayerHealth(int new_value);
const char *AddScratchAttack(int damage, const char *sfx);

void ConvertTHING();
void ConvertATK();

void HandleFlags(const DehackedMapObjectDefinition *info, int mt_num, int player);
void HandleAttacks(const DehackedMapObjectDefinition *info, int mt_num);

const char *GetSpeed(int speed);

void AlterThing(int new_val);
void AlterBexBits(char *bit_str);
void AlterMBF21Bits(char *bit_str);
}  // namespace things

}  // namespace dehacked