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

void ScriptNoOperation(TriggerScriptTrigger *R, void *param);
void ScriptShowTip(TriggerScriptTrigger *R, void *param);
void ScriptUpdateTipProperties(TriggerScriptTrigger *R, void *param);
void ScriptSpawnThing(TriggerScriptTrigger *R, void *param);
void ScriptPlaySound(TriggerScriptTrigger *R, void *param);
void ScriptKillSound(TriggerScriptTrigger *R, void *param);
void ScriptChangeMusic(TriggerScriptTrigger *R, void *param);
void ScriptPlayMovie(TriggerScriptTrigger *R, void *param);
void ScriptChangeTexture(TriggerScriptTrigger *R, void *param);

void ScriptMoveSector(TriggerScriptTrigger *R, void *param);
void ScriptLightSector(TriggerScriptTrigger *R, void *param);
void ScriptFogSector(TriggerScriptTrigger *R, void *param);
void ScriptEnableScript(TriggerScriptTrigger *R, void *param);
void ScriptActivateLinetype(TriggerScriptTrigger *R, void *param);
void ScriptUnblockLines(TriggerScriptTrigger *R, void *param);
void ScriptBlockLines(TriggerScriptTrigger *R, void *param);
void ScriptJump(TriggerScriptTrigger *R, void *param);
void ScriptSleep(TriggerScriptTrigger *R, void *param);
void ScriptRetrigger(TriggerScriptTrigger *R, void *param);

void ScriptDamagePlayers(TriggerScriptTrigger *R, void *param);
void ScriptHealPlayers(TriggerScriptTrigger *R, void *param);
void ScriptArmourPlayers(TriggerScriptTrigger *R, void *param);
void ScriptBenefitPlayers(TriggerScriptTrigger *R, void *param);
void ScriptDamageMonsters(TriggerScriptTrigger *R, void *param);
void ScriptThingEvent(TriggerScriptTrigger *R, void *param);
void ScriptSkill(TriggerScriptTrigger *R, void *param);
void ScriptGotoMap(TriggerScriptTrigger *R, void *param);
void ScriptExitLevel(TriggerScriptTrigger *R, void *param);
void ScriptExitGame(TriggerScriptTrigger *R, void *param);
void ScriptShowMenu(TriggerScriptTrigger *R, void *param);
void ScriptUpdateMenuStyle(TriggerScriptTrigger *R, void *param);
void ScriptJumpOn(TriggerScriptTrigger *R, void *param);
void ScriptWaitUntilDead(TriggerScriptTrigger *R, void *param);

void ScriptSwitchWeapon(TriggerScriptTrigger *R, void *param);
void ScriptTeleportToStart(TriggerScriptTrigger *R, void *param);
void ScriptReplaceWeapon(TriggerScriptTrigger *R, void *param);
void ScriptWeaponEvent(TriggerScriptTrigger *R, void *param);
void ScriptReplaceThing(TriggerScriptTrigger *R, void *param);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
