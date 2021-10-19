//----------------------------------------------------------------------------
//  EDGE Play Simulation Action routines
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
//
//  Based on the DOOM source code, released by Id Software under the
//  following copyright:
//
//    Copyright (C) 1993-1996 by id Software, Inc.
//
//----------------------------------------------------------------------------

#ifndef __P_ACTION_H__
#define __P_ACTION_H__

struct mobj_s;

// Weapon Action Routine pointers
void A_Light0(struct mobj_s *mo);
void A_Light1(struct mobj_s *mo);
void A_Light2(struct mobj_s *mo);

void A_WeaponReady(struct mobj_s *mo);
void A_WeaponEmpty(struct mobj_s *mo);
void A_WeaponShoot(struct mobj_s *mo);
void A_WeaponEject(struct mobj_s *mo);
void A_WeaponJump(struct mobj_s *mo);
void A_WeaponDJNE(struct mobj_s *mo);
void A_Lower(struct mobj_s *mo);
void A_Raise(struct mobj_s *mo);
void A_ReFire(struct mobj_s *mo);
void A_NoFire(struct mobj_s *mo);
void A_NoFireReturn(struct mobj_s *mo);
void A_CheckReload(struct mobj_s *mo);
void A_SFXWeapon1(struct mobj_s *mo);
void A_SFXWeapon2(struct mobj_s *mo);
void A_SFXWeapon3(struct mobj_s *mo);
void A_WeaponPlaySound(struct mobj_s *mo);
void A_WeaponKillSound(struct mobj_s *mo);
void A_WeaponTransSet(struct mobj_s *mo);
void A_WeaponTransFade(struct mobj_s *mo);
void A_WeaponEnableRadTrig(struct mobj_s *mo);
void A_WeaponDisableRadTrig(struct mobj_s *mo);

void A_SetCrosshair(struct mobj_s *mo);
void A_TargetJump(struct mobj_s *mo);
void A_FriendJump(struct mobj_s *mo);
void A_GunFlash(struct mobj_s *mo);
void A_WeaponKick(struct mobj_s *mo);
void A_WeaponSetSkin(struct mobj_s *mo);
void A_WeaponUnzoom(struct mobj_s *mo);

void A_WeaponShootSA(struct mobj_s *mo);
void A_ReFireSA(struct mobj_s *mo);
void A_NoFireSA(struct mobj_s *mo);
void A_NoFireReturnSA(struct mobj_s *mo);
void A_CheckReloadSA(struct mobj_s *mo);
void A_GunFlashSA(struct mobj_s *mo);

// Needed for the bossbrain.
void P_ActBrainScream(struct mobj_s *mo);
void P_ActBrainDie(struct mobj_s *mo);
void P_ActBrainSpit(struct mobj_s *mo);
void P_ActCubeSpawn(struct mobj_s *mo);
void P_ActBrainMissileExplode(struct mobj_s *mo);

// Visibility Actions
void P_ActTransSet(struct mobj_s *mo);
void P_ActTransFade(struct mobj_s *mo);
void P_ActTransMore(struct mobj_s *mo);
void P_ActTransLess(struct mobj_s *mo);
void P_ActTransAlternate(struct mobj_s *mo);

// Sound Actions
void P_ActPlaySound(struct mobj_s *mo);
void P_ActKillSound(struct mobj_s *mo);
void P_ActMakeAmbientSound(struct mobj_s *mo);
void P_ActMakeAmbientSoundRandom(struct mobj_s *mo);
void P_ActMakeCloseAttemptSound(struct mobj_s *mo);
void P_ActMakeDyingSound(struct mobj_s *mo);
void P_ActMakeOverKillSound(struct mobj_s *mo);
void P_ActMakePainSound(struct mobj_s *mo);
void P_ActMakeRangeAttemptSound(struct mobj_s *mo);
void P_ActMakeActiveSound(struct mobj_s *mo);
void P_ActPlayerScream(struct mobj_s *mo);

// Explosion Damage Actions
void P_ActDamageExplosion(struct mobj_s *mo);
void P_ActThrust(struct mobj_s *mo);

// Stand-by / Looking Actions
void P_ActStandardLook(struct mobj_s *mo);
void P_ActPlayerSupportLook(struct mobj_s *mo);

// Meander, aimless movement actions.
void P_ActStandardMeander(struct mobj_s *mo);
void P_ActPlayerSupportMeander(struct mobj_s *mo);

// Chasing Actions
void P_ActResurrectChase(struct mobj_s *mo);
void P_ActStandardChase(struct mobj_s *mo);
void P_ActWalkSoundChase(struct mobj_s *mo);

// Attacking Actions
void P_ActComboAttack(struct mobj_s *mo);
void P_ActMeleeAttack(struct mobj_s *mo);
void P_ActRangeAttack(struct mobj_s *mo);
void P_ActSpareAttack(struct mobj_s *mo);
void P_ActRefireCheck(struct mobj_s *mo);
void P_ActReloadCheck(struct mobj_s *mo);
void P_ActReloadReset(struct mobj_s *mo);

// Miscellanous
void P_ActFaceTarget(struct mobj_s *mo);
void P_ActMakeIntoCorpse(struct mobj_s *mo);
void P_ActResetSpreadCount(struct mobj_s *mo);
void P_ActExplode(struct mobj_s *mo);
void P_ActActivateLineType(struct mobj_s *mo);
void P_ActEnableRadTrig(struct mobj_s *mo);
void P_ActDisableRadTrig(struct mobj_s *mo);
void P_ActTouchyRearm(struct mobj_s *mo);
void P_ActTouchyDisarm(struct mobj_s *mo);
void P_ActBounceRearm(struct mobj_s *mo);
void P_ActBounceDisarm(struct mobj_s *mo);
void P_ActPathCheck(struct mobj_s *mo);
void P_ActPathFollow(struct mobj_s *mo);

void P_ActDropItem(struct mobj_s *mo);
void P_ActSpawn(struct mobj_s *mo);
void P_ActDLightSet(struct mobj_s *mo);
void P_ActDLightSet2(struct mobj_s *mo);
void P_ActDLightFade(struct mobj_s *mo);
void P_ActDLightRandom(struct mobj_s *mo);
void P_ActDLightColour(struct mobj_s *mo);
void P_ActSetSkin(struct mobj_s *mo);
void P_ActDie(struct mobj_s *mo);
void P_ActKeenDie(struct mobj_s *mo);
void P_ActCheckBlood(struct mobj_s *mo);
void P_ActJump(struct mobj_s *mo);
void P_ActBecome(struct mobj_s *mo);
void P_ActSetInvuln(struct mobj_s *mo);
void P_ActClearInvuln(struct mobj_s *mo);

// Movement actions
void P_ActFaceDir(struct mobj_s *mo);
void P_ActTurnDir(struct mobj_s *mo);
void P_ActTurnRandom(struct mobj_s *mo);
void P_ActMlookFace(struct mobj_s *mo);
void P_ActMlookTurn(struct mobj_s *mo);
void P_ActMoveFwd(struct mobj_s *mo);
void P_ActMoveRight(struct mobj_s *mo);
void P_ActMoveUp(struct mobj_s *mo);
void P_ActStopMoving(struct mobj_s *mo);
void P_ActCheckMoving(struct mobj_s *mo);
void P_ActCheckActivity(struct mobj_s *mo);

// Projectiles
void P_ActHomingProjectile(struct mobj_s *mo);
void P_ActLaunchOrderedSpread(struct mobj_s *mo);
void P_ActLaunchRandomSpread(struct mobj_s *mo);
void P_ActCreateSmokeTrail(struct mobj_s *mo);
void P_ActHomeToSpot(struct mobj_s *mo);
bool P_ActLookForTargets(struct mobj_s *mo);

// Trackers
void P_ActEffectTracker(struct mobj_s *mo);
void P_ActTrackerActive(struct mobj_s *mo);
void P_ActTrackerFollow(struct mobj_s *mo);
void P_ActTrackerStart(struct mobj_s *mo);

#endif /* __P_ACTION_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
