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

#include "epi_bam.h"

class MapObject;

// Function names in this file deviate from the style guide in order to reflect
// historical code pointer names and make the Dehacked->DDF->EDGE pipeline
// easier to track - Dasho

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
void A_WeaponRunLuaScript(MapObject *mo);

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

// These are weapon actions; the WA_ prefix
// is to avoid collision with the A_ variants - Dasho
void WA_MoveFwd(MapObject *mo);
void WA_MoveRight(MapObject *mo);
void WA_MoveUp(MapObject *mo);
void WA_TurnDir(MapObject *mo);
void WA_TurnRandom(MapObject *mo);
void WA_FaceDir(MapObject *mo);

// Needed for the bossbrain.
void A_BrainScream(MapObject *mo);
void A_BrainDie(MapObject *mo);
void A_BrainSpit(MapObject *mo);
void A_CubeSpawn(MapObject *mo);
void A_BrainMissileExplode(MapObject *mo);

// Visibility Actions
void A_TransSet(MapObject *mo);
void A_TransFade(MapObject *mo);
void A_TransMore(MapObject *mo);
void A_TransLess(MapObject *mo);
void A_TransAlternate(MapObject *mo);

// Sound Actions
void A_PlaySound(MapObject *mo);
void A_PlaySoundBoss(MapObject *mo);
void A_KillSound(MapObject *mo);
void A_MakeAmbientSound(MapObject *mo);
void A_MakeAmbientSoundRandom(MapObject *mo);
void A_MakeCloseAttemptSound(MapObject *mo);
void A_MakeDyingSound(MapObject *mo);
void A_MakeOverKillSound(MapObject *mo);
void A_MakePainSound(MapObject *mo);
void A_MakeRangeAttemptSound(MapObject *mo);
void A_MakeActiveSound(MapObject *mo);
void A_PlayerScream(MapObject *mo);

// Explosion Damage Actions
void A_DamageExplosion(MapObject *mo);
void A_Thrust(MapObject *mo);

// Stand-by / Looking Actions
void A_StandardLook(MapObject *mo);
void A_PlayerSupportLook(MapObject *mo);

// Meander, aimless movement actions.
void A_StandardMeander(MapObject *mo);
void A_PlayerSupportMeander(MapObject *mo);

// Chasing Actions
void A_ResurrectChase(MapObject *mo);
void A_StandardChase(MapObject *mo);
void A_WalkSoundChase(MapObject *mo);

// Attacking Actions
void A_ComboAttack(MapObject *mo);
void A_MeleeAttack(MapObject *mo);
void A_RangeAttack(MapObject *mo);
void A_SpareAttack(MapObject *mo);
void A_RefireCheck(MapObject *mo);
void A_ReloadCheck(MapObject *mo);
void A_ReloadReset(MapObject *mo);

// Miscellanous
void A_FaceTarget(MapObject *mo);
void A_MakeIntoCorpse(MapObject *mo);
void A_ResetSpreadCount(MapObject *mo);
void A_Explode(MapObject *mo);
void A_ActivateLineType(MapObject *mo);
void A_EnableRadTrig(MapObject *mo);
void A_DisableRadTrig(MapObject *mo);
void A_RunLuaScript(MapObject *mo);
void A_TouchyRearm(MapObject *mo);
void A_TouchyDisarm(MapObject *mo);
void A_BounceRearm(MapObject *mo);
void A_BounceDisarm(MapObject *mo);
void A_PathCheck(MapObject *mo);
void A_PathFollow(MapObject *mo);

void A_DropItem(MapObject *mo);
void A_Spawn(MapObject *mo);
void A_DLightSet(MapObject *mo);
void A_DLightFade(MapObject *mo);
void A_DLightRandom(MapObject *mo);
void A_DLightColour(MapObject *mo);
void A_SetSkin(MapObject *mo);
void A_Die(MapObject *mo);
void A_KeenDie(MapObject *mo);
void A_CheckBlood(MapObject *mo);
void A_Jump(MapObject *mo);
void A_JumpLiquid(MapObject *mo);
void A_JumpSky(MapObject *mo);
// void A_JumpStuck(MapObject *mo);
void A_Become(MapObject *mo);
void A_UnBecome(MapObject *mo);

void A_Morph(MapObject *mo);
void A_UnMorph(MapObject *mo);

void A_SetInvuln(MapObject *mo);
void A_ClearInvuln(MapObject *mo);

void A_PainChanceSet(MapObject *mo);

void A_ScaleSet(MapObject *mo);

void A_Gravity(MapObject *mo);
void A_NoGravity(MapObject *mo);

void A_ClearTarget(MapObject *mo);
void A_FriendLook(MapObject *mo);
bool FindPlayerToSupport(MapObject *mo);

// Movement actions
void A_FaceDir(MapObject *mo);
void A_TurnDir(MapObject *mo);
void A_TurnRandom(MapObject *mo);
void A_MlookFace(MapObject *mo);
void A_MlookTurn(MapObject *mo);
void A_MoveFwd(MapObject *mo);
void A_MoveRight(MapObject *mo);
void A_MoveUp(MapObject *mo);
void A_StopMoving(MapObject *mo);
void A_CheckMoving(MapObject *mo);
void A_CheckActivity(MapObject *mo);

// Projectiles
void A_HomingProjectile(MapObject *mo);
void A_CreateSmokeTrail(MapObject *mo);
void A_HomeToSpot(MapObject *mo);
bool A_LookForTargets(MapObject *mo);
MapObject *A_LookForBlockmapTarget(MapObject *mo, uint32_t rangeblocks, BAMAngle fov = 0);

// Trackers
void A_EffectTracker(MapObject *mo);
void A_TrackerActive(MapObject *mo);
void A_TrackerFollow(MapObject *mo);
void A_TrackerStart(MapObject *mo);

// Dehacked
void A_CloseShotgun2(MapObject *mo);

// MBF / MBF21
void A_Mushroom(MapObject *mo);
void A_NoiseAlert(MapObject *mo);
void A_WeaponMeleeAttack(MapObject *mo);
void A_WeaponSound(MapObject *mo);
void A_WeaponBulletAttack(MapObject *mo);
void A_WeaponProjectile(MapObject *mo);
void A_ConsumeAmmo(MapObject *mo);
void A_CheckAmmo(MapObject *mo);
void A_RadiusDamage(MapObject *mo);
void A_GunFlashTo(MapObject *mo);
void WA_NoiseAlert(MapObject *mo);
void A_HealChase(MapObject *mo);
void A_SpawnObject(MapObject *mo);
void A_MonsterProjectile(MapObject *mo);
void A_MonsterBulletAttack(MapObject *mo);
void A_MonsterMeleeAttack(MapObject *mo);
void A_ClearTracer(MapObject *mo);
void A_JumpIfHealthBelow(MapObject *mo);
void A_SeekTracer(MapObject *mo);
void A_FindTracer(MapObject *mo);
void A_JumpIfTargetInSight(MapObject *mo);
void A_JumpIfTargetCloser(MapObject *mo);
void A_JumpIfTracerInSight(MapObject *mo);
void A_JumpIfTracerCloser(MapObject *mo);
void A_JumpIfFlagsSet(MapObject *mo);
void A_AddFlags(MapObject *mo);
void A_RemoveFlags(MapObject *mo);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab