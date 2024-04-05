//----------------------------------------------------------------------------
//  Radius Trigger action defs
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

#pragma once

#include "rad_defs.h"

//
//  ACTIONS
//

void ScriptNoOperation(RADScriptTrigger *R, void *param);
void ScriptShowTip(RADScriptTrigger *R, void *param);
void ScriptUpdateTipProperties(RADScriptTrigger *R, void *param);
void ScriptSpawnThing(RADScriptTrigger *R, void *param);
void ScriptPlaySound(RADScriptTrigger *R, void *param);
void ScriptKillSound(RADScriptTrigger *R, void *param);
void ScriptChangeMusic(RADScriptTrigger *R, void *param);
void ScriptPlayMovie(RADScriptTrigger *R, void *param);
void ScriptChangeTexture(RADScriptTrigger *R, void *param);

void ScriptMoveSector(RADScriptTrigger *R, void *param);
void ScriptLightSector(RADScriptTrigger *R, void *param);
void ScriptFogSector(RADScriptTrigger *R, void *param);
void ScriptEnableScript(RADScriptTrigger *R, void *param);
void ScriptActivateLinetype(RADScriptTrigger *R, void *param);
void ScriptUnblockLines(RADScriptTrigger *R, void *param);
void ScriptBlockLines(RADScriptTrigger *R, void *param);
void ScriptJump(RADScriptTrigger *R, void *param);
void ScriptSleep(RADScriptTrigger *R, void *param);
void ScriptRetrigger(RADScriptTrigger *R, void *param);

void ScriptDamagePlayers(RADScriptTrigger *R, void *param);
void ScriptHealPlayers(RADScriptTrigger *R, void *param);
void ScriptArmourPlayers(RADScriptTrigger *R, void *param);
void ScriptBenefitPlayers(RADScriptTrigger *R, void *param);
void ScriptDamageMonsters(RADScriptTrigger *R, void *param);
void ScriptThingEvent(RADScriptTrigger *R, void *param);
void ScriptSkill(RADScriptTrigger *R, void *param);
void ScriptGotoMap(RADScriptTrigger *R, void *param);
void ScriptExitLevel(RADScriptTrigger *R, void *param);
void ScriptExitGame(RADScriptTrigger *R, void *param);
void ScriptShowMenu(RADScriptTrigger *R, void *param);
void ScriptUpdateMenuStyle(RADScriptTrigger *R, void *param);
void ScriptJumpOn(RADScriptTrigger *R, void *param);
void ScriptWaitUntilDead(RADScriptTrigger *R, void *param);

void ScriptSwitchWeapon(RADScriptTrigger *R, void *param);
void ScriptTeleportToStart(RADScriptTrigger *R, void *param);
void ScriptReplaceWeapon(RADScriptTrigger *R, void *param);
void ScriptWeaponEvent(RADScriptTrigger *R, void *param);
void ScriptReplaceThing(RADScriptTrigger *R, void *param);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
