//----------------------------------------------------------------------------
//  Radius Trigger action defs
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2009  The EDGE Team.
// 
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//----------------------------------------------------------------------------

#ifndef __RAD_ACT_H__
#define __RAD_ACT_H__

//
//  ACTIONS
//

void RAD_ActNOP(rad_trigger_t *R, void *param);
void RAD_ActTip(rad_trigger_t *R, void *param);
void RAD_ActTipProps(rad_trigger_t *R, void *param);
void RAD_ActSpawnThing(rad_trigger_t *R, void *param);
void RAD_ActPlaySound(rad_trigger_t *R, void *param);
void RAD_ActKillSound(rad_trigger_t *R, void *param);
void RAD_ActChangeMusic(rad_trigger_t *R, void *param);
void RAD_ActChangeTex(rad_trigger_t *R, void *param);

void RAD_ActMoveSector(rad_trigger_t *R, void *param);
void RAD_ActLightSector(rad_trigger_t *R, void *param);
void RAD_ActEnableScript(rad_trigger_t *R, void *param);
void RAD_ActActivateLinetype(rad_trigger_t *R, void *param);
void RAD_ActUnblockLines(rad_trigger_t *R, void *param);
void RAD_ActBlockLines(rad_trigger_t *R, void *param);
void RAD_ActJump(rad_trigger_t *R, void *param);
void RAD_ActSleep(rad_trigger_t *R, void *param);
void RAD_ActRetrigger(rad_trigger_t *R, void *param);

void RAD_ActDamagePlayers(rad_trigger_t *R, void *param);
void RAD_ActHealPlayers(rad_trigger_t *R, void *param);
void RAD_ActArmourPlayers(rad_trigger_t *R, void *param);
void RAD_ActBenefitPlayers(rad_trigger_t *R, void *param);
void RAD_ActDamageMonsters(rad_trigger_t *R, void *param);
void RAD_ActThingEvent(rad_trigger_t *R, void *param);
void RAD_ActSkill(rad_trigger_t *R, void *param);
void RAD_ActGotoMap(rad_trigger_t *R, void *param);
void RAD_ActExitLevel(rad_trigger_t *R, void *param);
void RAD_ActShowMenu(rad_trigger_t *R, void *param);
void RAD_ActMenuStyle(rad_trigger_t *R, void *param);
void RAD_ActJumpOn(rad_trigger_t *R, void *param);
void RAD_ActWaitUntilDead(rad_trigger_t *R, void *param);

#endif  /*__RAD_ACT_H__*/

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
