//----------------------------------------------------------------------------
//  EDGE Play Simulation Action routines
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

#pragma once

class MapObject;

// Weapon Action Routine pointers
void A_Light0(MapObject *mo);
void A_Light1(MapObject *mo);
void A_Light2(MapObject *mo);

void A_WeaponReady(MapObject *mo);
void A_WeaponEmpty(MapObject *mo);
void A_WeaponShoot(MapObject *mo);
void A_WeaponEject(MapObject *mo);
void A_WeaponJump(MapObject *mo);
void A_WeaponDJNE(MapObject *mo);
void A_Lower(MapObject *mo);
void A_Raise(MapObject *mo);
void A_ReFire(MapObject *mo);
void A_ReFireTo(MapObject *mo);
void A_NoFire(MapObject *mo);
void A_NoFireReturn(MapObject *mo);
void A_CheckReload(MapObject *mo);
void A_SFXWeapon1(MapObject *mo);
void A_SFXWeapon2(MapObject *mo);
void A_SFXWeapon3(MapObject *mo);
void A_WeaponPlaySound(MapObject *mo);
void A_WeaponKillSound(MapObject *mo);
void A_WeaponTransSet(MapObject *mo);
void A_WeaponTransFade(MapObject *mo);
void A_WeaponEnableRadTrig(MapObject *mo);
void A_WeaponDisableRadTrig(MapObject *mo);

void A_SetCrosshair(MapObject *mo);
void A_TargetJump(MapObject *mo);
void A_FriendJump(MapObject *mo);
void A_GunFlash(MapObject *mo);
void A_WeaponKick(MapObject *mo);
void A_WeaponSetSkin(MapObject *mo);
void A_WeaponUnzoom(MapObject *mo);
void A_WeaponBecome(MapObject *mo);

void A_WeaponShootSA(MapObject *mo);
void A_ReFireSA(MapObject *mo);
void A_ReFireToSA(MapObject *mo);
void A_NoFireSA(MapObject *mo);
void A_NoFireReturnSA(MapObject *mo);
void A_CheckReloadSA(MapObject *mo);
void A_GunFlashSA(MapObject *mo);

void A_WeaponShootTA(MapObject *mo);
void A_ReFireTA(MapObject *mo);
void A_ReFireToTA(MapObject *mo);
void A_NoFireTA(MapObject *mo);
void A_NoFireReturnTA(MapObject *mo);
void A_CheckReloadTA(MapObject *mo);
void A_GunFlashTA(MapObject *mo);

void A_WeaponShootFA(MapObject *mo);
void A_ReFireFA(MapObject *mo);
void A_ReFireToFA(MapObject *mo);
void A_NoFireFA(MapObject *mo);
void A_NoFireReturnFA(MapObject *mo);
void A_CheckReloadFA(MapObject *mo);
void A_GunFlashFA(MapObject *mo);

void A_WeaponZoom(MapObject *mo);
void A_SetInvuln(MapObject *mo);
void A_ClearInvuln(MapObject *mo);
void A_MoveFwd(MapObject *mo);
void A_MoveRight(MapObject *mo);
void A_MoveUp(MapObject *mo);
void A_StopMoving(MapObject *mo);
void A_TurnDir(MapObject *mo);
void A_TurnRandom(MapObject *mo);
void A_MlookTurn(MapObject *mo);

// Needed for the bossbrain.
void P_ActBrainScream(MapObject *mo);
void P_ActBrainDie(MapObject *mo);
void P_ActBrainSpit(MapObject *mo);
void P_ActCubeSpawn(MapObject *mo);
void P_ActBrainMissileExplode(MapObject *mo);

// Visibility Actions
void P_ActTransSet(MapObject *mo);
void P_ActTransFade(MapObject *mo);
void P_ActTransMore(MapObject *mo);
void P_ActTransLess(MapObject *mo);
void P_ActTransAlternate(MapObject *mo);

// Sound Actions
void P_ActPlaySound(MapObject *mo);
void P_ActPlaySoundBoss(MapObject *mo);
void P_ActKillSound(MapObject *mo);
void P_ActMakeAmbientSound(MapObject *mo);
void P_ActMakeAmbientSoundRandom(MapObject *mo);
void P_ActMakeCloseAttemptSound(MapObject *mo);
void P_ActMakeDyingSound(MapObject *mo);
void P_ActMakeOverKillSound(MapObject *mo);
void P_ActMakePainSound(MapObject *mo);
void P_ActMakeRangeAttemptSound(MapObject *mo);
void P_ActMakeActiveSound(MapObject *mo);
void P_ActPlayerScream(MapObject *mo);

// Explosion Damage Actions
void P_ActDamageExplosion(MapObject *mo);
void P_ActThrust(MapObject *mo);

// Stand-by / Looking Actions
void P_ActStandardLook(MapObject *mo);
void P_ActPlayerSupportLook(MapObject *mo);

// Meander, aimless movement actions.
void P_ActStandardMeander(MapObject *mo);
void P_ActPlayerSupportMeander(MapObject *mo);

// Chasing Actions
void P_ActResurrectChase(MapObject *mo);
void P_ActStandardChase(MapObject *mo);
void P_ActWalkSoundChase(MapObject *mo);

// Attacking Actions
void P_ActComboAttack(MapObject *mo);
void P_ActMeleeAttack(MapObject *mo);
void P_ActRangeAttack(MapObject *mo);
void P_ActSpareAttack(MapObject *mo);
void P_ActRefireCheck(MapObject *mo);
void P_ActReloadCheck(MapObject *mo);
void P_ActReloadReset(MapObject *mo);

// Miscellanous
void P_ActFaceTarget(MapObject *mo);
void P_ActMakeIntoCorpse(MapObject *mo);
void P_ActResetSpreadCount(MapObject *mo);
void P_ActExplode(MapObject *mo);
void P_ActActivateLineType(MapObject *mo);
void P_ActEnableRadTrig(MapObject *mo);
void P_ActDisableRadTrig(MapObject *mo);
void P_ActTouchyRearm(MapObject *mo);
void P_ActTouchyDisarm(MapObject *mo);
void P_ActBounceRearm(MapObject *mo);
void P_ActBounceDisarm(MapObject *mo);
void P_ActPathCheck(MapObject *mo);
void P_ActPathFollow(MapObject *mo);

void P_ActDropItem(MapObject *mo);
void P_ActSpawn(MapObject *mo);
void P_ActDLightSet(MapObject *mo);
void P_ActDLightSet2(MapObject *mo);
void P_ActDLightFade(MapObject *mo);
void P_ActDLightRandom(MapObject *mo);
void P_ActDLightColour(MapObject *mo);
void P_ActSetSkin(MapObject *mo);
void P_ActDie(MapObject *mo);
void P_ActKeenDie(MapObject *mo);
void P_ActCheckBlood(MapObject *mo);
void P_ActJump(MapObject *mo);
void P_ActJumpLiquid(MapObject *mo);
void P_ActJumpSky(MapObject *mo);
// void P_ActJumpStuck(MapObject *mo);
void P_ActBecome(MapObject *mo);
void P_ActUnBecome(MapObject *mo);

void P_ActMorph(MapObject *mo);
void P_ActUnMorph(MapObject *mo);

void P_ActSetInvuln(MapObject *mo);
void P_ActClearInvuln(MapObject *mo);

void P_ActPainChanceSet(MapObject *mo);

// Movement actions
void P_ActFaceDir(MapObject *mo);
void P_ActTurnDir(MapObject *mo);
void P_ActTurnRandom(MapObject *mo);
void P_ActMlookFace(MapObject *mo);
void P_ActMlookTurn(MapObject *mo);
void P_ActMoveFwd(MapObject *mo);
void P_ActMoveRight(MapObject *mo);
void P_ActMoveUp(MapObject *mo);
void P_ActStopMoving(MapObject *mo);
void P_ActCheckMoving(MapObject *mo);
void P_ActCheckActivity(MapObject *mo);

// Projectiles
void P_ActHomingProjectile(MapObject *mo);
void P_ActLaunchOrderedSpread(MapObject *mo);
void P_ActLaunchRandomSpread(MapObject *mo);
void P_ActCreateSmokeTrail(MapObject *mo);
void P_ActHomeToSpot(MapObject *mo);
bool P_ActLookForTargets(MapObject *mo);

// Trackers
void P_ActEffectTracker(MapObject *mo);
void P_ActTrackerActive(MapObject *mo);
void P_ActTrackerFollow(MapObject *mo);
void P_ActTrackerStart(MapObject *mo);

// MBF / MBF21
void P_ActMushroom(MapObject *mo);
void P_ActNoiseAlert(MapObject *mo);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab