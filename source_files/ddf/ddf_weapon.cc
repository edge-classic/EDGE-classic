//----------------------------------------------------------------------------
//  EDGE Data Definition File Code (Weapons)
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
// Player Weapons Setup and Parser Code
//
// -KM- 1998/11/25 File Written
//

#include "ddf_weapon.h"

#include <string.h>

#include "ddf_local.h"
#include "ddf_types.h"
#include "epi_str_compare.h"
#include "epi_str_hash.h"
#include "epi_str_util.h"
#include "p_action.h"

std::vector<std::string> flag_tests;

static WeaponDefinition *dynamic_weapon;

WeaponDefinitionContainer weapondefs;

static void DDFWGetAmmo(const char *info, void *storage);
static void DDFWGetUpgrade(const char *info, void *storage);
static void DDFWGetSpecialFlags(const char *info, void *storage);
static void DDFWStateGetRADTrigger(const char *arg, State *cur_state);
static void DDFWStateGetDEHMelee(const char *arg, State *cur_state);
static void DDFWStateGetDEHBullet(const char *arg, State *cur_state);
static void DDFWStateGetDEHProjectile(const char *arg, State *cur_state);
static void DDFWStateGetString(const char *arg, State *cur_state);

static WeaponDefinition dummy_weapon;

static const DDFCommandList weapon_commands[] = {
    DDF_FIELD("AMMOTYPE", dummy_weapon, ammo_[0], DDFWGetAmmo),
    DDF_FIELD("AMMOPERSHOT", dummy_weapon, ammopershot_[0], DDFMainGetNumeric),
    DDF_FIELD("CLIPSIZE", dummy_weapon, clip_size_[0], DDFMainGetNumeric),
    DDF_FIELD("AUTOMATIC", dummy_weapon, autofire_[0], DDFMainGetBoolean),
    DDF_FIELD("NO_CHEAT", dummy_weapon, no_cheat_, DDFMainGetBoolean),
    DDF_FIELD("ATTACK", dummy_weapon, attack_[0], DDFMainRefAttack),
    DDF_FIELD("SPECIAL", dummy_weapon, specials_[0], DDFWGetSpecialFlags),

    DDF_FIELD("SEC_AMMOTYPE", dummy_weapon, ammo_[1], DDFWGetAmmo),
    DDF_FIELD("SEC_AMMOPERSHOT", dummy_weapon, ammopershot_[1], DDFMainGetNumeric),
    DDF_FIELD("SEC_CLIPSIZE", dummy_weapon, clip_size_[1], DDFMainGetNumeric),
    DDF_FIELD("SEC_AUTOMATIC", dummy_weapon, autofire_[1], DDFMainGetBoolean),
    DDF_FIELD("SEC_ATTACK", dummy_weapon, attack_[1], DDFMainRefAttack),
    DDF_FIELD("SEC_SPECIAL", dummy_weapon, specials_[1], DDFWGetSpecialFlags),

    DDF_FIELD("2ND_AMMOTYPE", dummy_weapon, ammo_[1], DDFWGetAmmo),
    DDF_FIELD("2ND_AMMOPERSHOT", dummy_weapon, ammopershot_[1], DDFMainGetNumeric),
    DDF_FIELD("2ND_CLIPSIZE", dummy_weapon, clip_size_[1], DDFMainGetNumeric),
    DDF_FIELD("2ND_AUTOMATIC", dummy_weapon, autofire_[1], DDFMainGetBoolean),
    DDF_FIELD("2ND_ATTACK", dummy_weapon, attack_[1], DDFMainRefAttack),
    DDF_FIELD("2ND_SPECIAL", dummy_weapon, specials_[1], DDFWGetSpecialFlags),

    DDF_FIELD("3RD_AMMOTYPE", dummy_weapon, ammo_[2], DDFWGetAmmo),
    DDF_FIELD("3RD_AMMOPERSHOT", dummy_weapon, ammopershot_[2], DDFMainGetNumeric),
    DDF_FIELD("3RD_CLIPSIZE", dummy_weapon, clip_size_[2], DDFMainGetNumeric),
    DDF_FIELD("3RD_AUTOMATIC", dummy_weapon, autofire_[2], DDFMainGetBoolean),
    DDF_FIELD("3RD_ATTACK", dummy_weapon, attack_[2], DDFMainRefAttack),
    DDF_FIELD("3RD_SPECIAL", dummy_weapon, specials_[2], DDFWGetSpecialFlags),

    DDF_FIELD("4TH_AMMOTYPE", dummy_weapon, ammo_[3], DDFWGetAmmo),
    DDF_FIELD("4TH_AMMOPERSHOT", dummy_weapon, ammopershot_[3], DDFMainGetNumeric),
    DDF_FIELD("4TH_CLIPSIZE", dummy_weapon, clip_size_[3], DDFMainGetNumeric),
    DDF_FIELD("4TH_AUTOMATIC", dummy_weapon, autofire_[3], DDFMainGetBoolean),
    DDF_FIELD("4TH_ATTACK", dummy_weapon, attack_[3], DDFMainRefAttack),
    DDF_FIELD("4TH_SPECIAL", dummy_weapon, specials_[3], DDFWGetSpecialFlags),

    DDF_FIELD("EJECT_ATTACK", dummy_weapon, eject_attack_, DDFMainRefAttack),
    DDF_FIELD("FREE", dummy_weapon, autogive_, DDFMainGetBoolean),
    DDF_FIELD("BINDKEY", dummy_weapon, bind_key_, DDFMainGetNumeric),
    DDF_FIELD("PRIORITY", dummy_weapon, priority_, DDFMainGetNumeric),
    DDF_FIELD("DANGEROUS", dummy_weapon, dangerous_, DDFMainGetBoolean),
    DDF_FIELD("UPGRADES", dummy_weapon, upgrade_weap_, DDFWGetUpgrade),
    DDF_FIELD("IDLE_SOUND", dummy_weapon, idle_, DDFMainLookupSound),
    DDF_FIELD("ENGAGED_SOUND", dummy_weapon, engaged_, DDFMainLookupSound),
    DDF_FIELD("HIT_SOUND", dummy_weapon, hit_, DDFMainLookupSound),
    DDF_FIELD("START_SOUND", dummy_weapon, start_, DDFMainLookupSound),
    DDF_FIELD("NOTHRUST", dummy_weapon, nothrust_, DDFMainGetBoolean),
    DDF_FIELD("FEEDBACK", dummy_weapon, feedback_, DDFMainGetBoolean),
    DDF_FIELD("KICK", dummy_weapon, kick_, DDFMainGetFloat),
    DDF_FIELD("ZOOM_FOV", dummy_weapon, zoom_fov_, DDFMainGetNumeric),
    DDF_FIELD("ZOOM_FACTOR", dummy_weapon, zoom_factor_, DDFMainGetFloat),
    DDF_FIELD("REFIRE_INACCURATE", dummy_weapon, refire_inacc_, DDFMainGetBoolean),
    DDF_FIELD("SHOW_CLIP", dummy_weapon, show_clip_, DDFMainGetBoolean),
    DDF_FIELD("SHARED_CLIP", dummy_weapon, shared_clip_, DDFMainGetBoolean),
    DDF_FIELD("BOBBING", dummy_weapon, bobbing_, DDFMainGetPercentAny),
    DDF_FIELD("SWAYING", dummy_weapon, swaying_, DDFMainGetPercent),
    DDF_FIELD("IDLE_WAIT", dummy_weapon, idle_wait_, DDFMainGetTime),
    DDF_FIELD("IDLE_CHANCE", dummy_weapon, idle_chance_, DDFMainGetPercent),
    DDF_FIELD("MODEL_SKIN", dummy_weapon, model_skin_, DDFMainGetNumeric),
    DDF_FIELD("MODEL_ASPECT", dummy_weapon, model_aspect_, DDFMainGetFloat),
    DDF_FIELD("MODEL_BIAS", dummy_weapon, model_bias_, DDFMainGetFloat),
    DDF_FIELD("MODEL_ROTATE", dummy_weapon, model_rotate_, DDFMainGetNumeric),
    DDF_FIELD("MODEL_FORWARD", dummy_weapon, model_forward_, DDFMainGetFloat),
    DDF_FIELD("MODEL_SIDE", dummy_weapon, model_side_, DDFMainGetFloat),

    // -AJA- backwards compatibility cruft...
    DDF_FIELD("SECOND_ATTACK", dummy_weapon, attack_[1], DDFMainRefAttack),

    DDF_FIELD("SOUND1", dummy_weapon, sound1_, DDFMainLookupSound),
    DDF_FIELD("SOUND2", dummy_weapon, sound2_, DDFMainLookupSound),
    DDF_FIELD("SOUND3", dummy_weapon, sound3_, DDFMainLookupSound),

    DDF_FIELD("RENDER_INVERT", dummy_weapon, render_invert_, DDFMainGetBoolean),
    DDF_FIELD("Y_ADJUST", dummy_weapon, y_adjust_, DDFMainGetFloat),
    DDF_FIELD("IGNORE_CROSSHAIR_SCALING", dummy_weapon, ignore_crosshair_scaling_, DDFMainGetBoolean),

    {nullptr, nullptr, 0, nullptr}};

static const DDFStateStarter weapon_starters[] = {DDF_STATE("UP", "UP", dummy_weapon, up_state_),
                                                  DDF_STATE("DOWN", "DOWN", dummy_weapon, down_state_),
                                                  DDF_STATE("READY", "READY", dummy_weapon, ready_state_),
                                                  DDF_STATE("EMPTY", "EMPTY", dummy_weapon, empty_state_),
                                                  DDF_STATE("IDLE", "READY", dummy_weapon, idle_state_),
                                                  DDF_STATE("CROSSHAIR", "CROSSHAIR", dummy_weapon, crosshair_),
                                                  DDF_STATE("ZOOM", "ZOOM", dummy_weapon, zoom_state_),

                                                  DDF_STATE("ATTACK", "READY", dummy_weapon, attack_state_[0]),
                                                  DDF_STATE("RELOAD", "READY", dummy_weapon, reload_state_[0]),
                                                  DDF_STATE("DISCARD", "READY", dummy_weapon, discard_state_[0]),
                                                  DDF_STATE("WARMUP", "ATTACK", dummy_weapon, warmup_state_[0]),
                                                  DDF_STATE("FLASH", "REMOVE", dummy_weapon, flash_state_[0]),

                                                  DDF_STATE("SECATTACK", "READY", dummy_weapon, attack_state_[1]),
                                                  DDF_STATE("SECRELOAD", "READY", dummy_weapon, reload_state_[1]),
                                                  DDF_STATE("SECDISCARD", "READY", dummy_weapon, discard_state_[1]),
                                                  DDF_STATE("SECWARMUP", "SECATTACK", dummy_weapon, warmup_state_[1]),
                                                  DDF_STATE("SECFLASH", "REMOVE", dummy_weapon, flash_state_[1]),

                                                  DDF_STATE("2NDATTACK", "READY", dummy_weapon, attack_state_[1]),
                                                  DDF_STATE("2NDRELOAD", "READY", dummy_weapon, reload_state_[1]),
                                                  DDF_STATE("2NDDISCARD", "READY", dummy_weapon, discard_state_[1]),
                                                  DDF_STATE("2NDWARMUP", "2NDATTACK", dummy_weapon, warmup_state_[1]),
                                                  DDF_STATE("2NDFLASH", "REMOVE", dummy_weapon, flash_state_[1]),

                                                  DDF_STATE("3RDATTACK", "READY", dummy_weapon, attack_state_[2]),
                                                  DDF_STATE("3RDRELOAD", "READY", dummy_weapon, reload_state_[2]),
                                                  DDF_STATE("3RDDISCARD", "READY", dummy_weapon, discard_state_[2]),
                                                  DDF_STATE("3RDWARMUP", "3RDATTACK", dummy_weapon, warmup_state_[2]),
                                                  DDF_STATE("3RDFLASH", "REMOVE", dummy_weapon, flash_state_[2]),

                                                  DDF_STATE("4THATTACK", "READY", dummy_weapon, attack_state_[3]),
                                                  DDF_STATE("4THRELOAD", "READY", dummy_weapon, reload_state_[3]),
                                                  DDF_STATE("4THDISCARD", "READY", dummy_weapon, discard_state_[3]),
                                                  DDF_STATE("4THWARMUP", "4THATTACK", dummy_weapon, warmup_state_[3]),
                                                  DDF_STATE("4THFLASH", "REMOVE", dummy_weapon, flash_state_[3]),

                                                  {nullptr, nullptr, 0}};

static const DDFActionCode weapon_actions[] = {{"NOTHING", nullptr, nullptr},

                                               {"RAISE", A_Raise, nullptr},
                                               {"LOWER", A_Lower, nullptr},
                                               {"READY", A_WeaponReady, nullptr},
                                               {"EMPTY", A_WeaponEmpty, nullptr},
                                               {"SHOOT", A_WeaponShoot, DDFStateGetAttack},
                                               {"EJECT", A_WeaponEject, DDFStateGetAttack},
                                               {"REFIRE", A_ReFire, nullptr},
                                               {"REFIRE_TO", A_ReFireTo, DDFStateGetJumpInt},
                                               {"NOFIRE", A_NoFire, nullptr},
                                               {"NOFIRE_RETURN", A_NoFireReturn, nullptr},
                                               {"KICK", A_WeaponKick, DDFStateGetFloat},
                                               {"CHECKRELOAD", A_CheckReload, nullptr},
                                               {"PLAYSOUND", A_WeaponPlaySound, DDFStateGetSound},
                                               {"KILLSOUND", A_WeaponKillSound, nullptr},
                                               {"SET_SKIN", A_WeaponSetSkin, DDFStateGetInteger},
                                               {"JUMP", A_WeaponJump, DDFStateGetJump},
                                               {"UNZOOM", A_WeaponUnzoom, nullptr},

                                               {"DJNE", A_WeaponDJNE, DDFStateGetJump},

                                               {"ZOOM", A_WeaponZoom, nullptr},
                                               {"SET_INVULNERABLE", A_SetInvuln, nullptr},
                                               {"CLEAR_INVULNERABLE", A_ClearInvuln, nullptr},

                                               {"GRAVITY", A_Gravity, nullptr},
                                               {"NO_GRAVITY", A_NoGravity, nullptr},

                                               {"MOVE_FWD", WA_MoveFwd, DDFStateGetFloat},
                                               {"MOVE_RIGHT", WA_MoveRight, DDFStateGetFloat},
                                               {"MOVE_UP", WA_MoveUp, DDFStateGetFloat},
                                               {"STOP", A_StopMoving, nullptr},
                                               {"TURN", WA_TurnDir, DDFStateGetAngle},
                                               {"TURN_RANDOM", WA_TurnRandom, DDFStateGetInteger},
                                               {"FACE", WA_FaceDir, DDFStateGetSlope},

                                               {"RTS_ENABLE_TAGGED", A_WeaponEnableRadTrig, DDFWStateGetRADTrigger},
                                               {"RTS_DISABLE_TAGGED", A_WeaponDisableRadTrig, DDFWStateGetRADTrigger},
                                               {"LUA_RUN_SCRIPT", A_WeaponRunLuaScript, DDFWStateGetString},
                                               {"SEC_SHOOT", A_WeaponShootSA, DDFStateGetAttack},
                                               {"SEC_REFIRE", A_ReFireSA, nullptr},
                                               {"SEC_REFIRE_TO", A_ReFireToSA, DDFStateGetJump},
                                               {"SEC_NOFIRE", A_NoFireSA, nullptr},
                                               {"SEC_NOFIRE_RETURN", A_NoFireReturnSA, nullptr},
                                               {"SEC_CHECKRELOAD", A_CheckReloadSA, nullptr},

                                               {"2ND_SHOOT", A_WeaponShootSA, DDFStateGetAttack},
                                               {"2ND_REFIRE", A_ReFireSA, nullptr},
                                               {"2ND_REFIRE_TO", A_ReFireToSA, DDFStateGetJump},
                                               {"2ND_NOFIRE", A_NoFireSA, nullptr},
                                               {"2ND_NOFIRE_RETURN", A_NoFireReturnSA, nullptr},
                                               {"2ND_CHECKRELOAD", A_CheckReloadSA, nullptr},

                                               {"3RD_SHOOT", A_WeaponShootTA, DDFStateGetAttack},
                                               {"3RD_REFIRE", A_ReFireTA, nullptr},
                                               {"3RD_REFIRE_TO", A_ReFireToTA, DDFStateGetJump},
                                               {"3RD_NOFIRE", A_NoFireTA, nullptr},
                                               {"3RD_NOFIRE_RETURN", A_NoFireReturnTA, nullptr},
                                               {"3RD_CHECKRELOAD", A_CheckReloadTA, nullptr},

                                               {"4TH_SHOOT", A_WeaponShootFA, DDFStateGetAttack},
                                               {"4TH_REFIRE", A_ReFireFA, nullptr},
                                               {"4TH_REFIRE_TO", A_ReFireToFA, DDFStateGetJump},
                                               {"4TH_NOFIRE", A_NoFireFA, nullptr},
                                               {"4TH_NOFIRE_RETURN", A_NoFireReturnFA, nullptr},
                                               {"4TH_CHECKRELOAD", A_CheckReloadFA, nullptr},

                                               // flash-related actions
                                               {"FLASH", A_GunFlash, nullptr},
                                               {"SEC_FLASH", A_GunFlashSA, nullptr},
                                               {"2ND_FLASH", A_GunFlashSA, nullptr},
                                               {"3RD_FLASH", A_GunFlashTA, nullptr},
                                               {"4TH_FLASH", A_GunFlashFA, nullptr},
                                               {"LIGHT0", A_Light0, nullptr},
                                               {"LIGHT1", A_Light1, nullptr},
                                               {"LIGHT2", A_Light2, nullptr},
                                               {"TRANS_SET", A_WeaponTransSet, DDFStateGetPercent},
                                               {"TRANS_FADE", A_WeaponTransFade, DDFStateGetPercent},

                                               // crosshair-related actions
                                               {"SETCROSS", A_SetCrosshair, DDFStateGetFrame},
                                               {"TARGET_JUMP", A_TargetJump, DDFStateGetFrame},
                                               {"FRIEND_JUMP", A_FriendJump, DDFStateGetFrame},

                                               // -AJA- backwards compatibility cruft...
                                               {"SOUND1", A_SFXWeapon1, nullptr},
                                               {"SOUND2", A_SFXWeapon2, nullptr},
                                               {"SOUND3", A_SFXWeapon3, nullptr},

                                               {"BECOME", A_WeaponBecome, DDFStateGetBecomeWeapon},

                                               // Internal: Dehacked direct codepointer testing
                                               {"DEH_CLOSE_SHOTGUN2", A_CloseShotgun2, nullptr},
                                               {"DEH_WEAPON_MELEE", A_WeaponMeleeAttack, DDFWStateGetDEHMelee},
                                               {"DEH_WEAPON_SOUND", A_WeaponSound, DDFStateGetDEHParams},
                                               {"DEH_WEAPON_BULLET", A_WeaponBulletAttack, DDFWStateGetDEHBullet},
                                               {"DEH_WEAPON_PROJECTILE", A_WeaponProjectile, DDFWStateGetDEHProjectile},
                                               {"DEH_WEAPON_CONSUMEAMMO", A_ConsumeAmmo, DDFStateGetDEHParams},
                                               {"DEH_WEAPON_CHECKAMMO", A_CheckAmmo, DDFStateGetJumpInt},
                                               {"DEH_WEAPON_GUNFLASH_TO", A_GunFlashTo, DDFStateGetJumpInt},
                                               {"DEH_WEAPON_NOISE_ALERT", WA_NoiseAlert, nullptr},

                                               {nullptr, nullptr, nullptr}};

const DDFSpecialFlags ammo_types[] = {{"NOAMMO", kAmmunitionTypeNoAmmo, 0},

                                      {"BULLETS", kAmmunitionTypeBullet, 0},
                                      {"SHELLS", kAmmunitionTypeShell, 0},
                                      {"ROCKETS", kAmmunitionTypeRocket, 0},
                                      {"CELLS", kAmmunitionTypeCell, 0},
                                      {"PELLETS", kAmmunitionTypePellet, 0},
                                      {"NAILS", kAmmunitionTypeNail, 0},
                                      {"GRENADES", kAmmunitionTypeGrenade, 0},
                                      {"GAS", kAmmunitionTypeGas, 0},

                                      {"AMMO1", kAmmunitionTypeBullet, 0},
                                      {"AMMO2", kAmmunitionTypeShell, 0},
                                      {"AMMO3", kAmmunitionTypeRocket, 0},
                                      {"AMMO4", kAmmunitionTypeCell, 0},
                                      {"AMMO5", kAmmunitionTypePellet, 0},
                                      {"AMMO6", kAmmunitionTypeNail, 0},
                                      {"AMMO7", kAmmunitionTypeGrenade, 0},
                                      {"AMMO8", kAmmunitionTypeGas, 0},

                                      {"AMMO9", kAmmunitionType9, 0},
                                      {"AMMO10", kAmmunitionType10, 0},
                                      {"AMMO11", kAmmunitionType11, 0},
                                      {"AMMO12", kAmmunitionType12, 0},
                                      {"AMMO13", kAmmunitionType13, 0},
                                      {"AMMO14", kAmmunitionType14, 0},
                                      {"AMMO15", kAmmunitionType15, 0},
                                      {"AMMO16", kAmmunitionType16, 0},
                                      {"AMMO17", kAmmunitionType17, 0},
                                      {"AMMO18", kAmmunitionType18, 0},
                                      {"AMMO19", kAmmunitionType19, 0},
                                      {"AMMO20", kAmmunitionType20, 0},
                                      {"AMMO21", kAmmunitionType21, 0},
                                      {"AMMO22", kAmmunitionType22, 0},
                                      {"AMMO23", kAmmunitionType23, 0},
                                      {"AMMO24", kAmmunitionType24, 0},
                                      {"AMMO25", kAmmunitionType25, 0},
                                      {"AMMO26", kAmmunitionType26, 0},
                                      {"AMMO27", kAmmunitionType27, 0},
                                      {"AMMO28", kAmmunitionType28, 0},
                                      {"AMMO29", kAmmunitionType29, 0},
                                      {"AMMO30", kAmmunitionType30, 0},
                                      {"AMMO31", kAmmunitionType31, 0},
                                      {"AMMO32", kAmmunitionType32, 0},
                                      {"AMMO33", kAmmunitionType33, 0},
                                      {"AMMO34", kAmmunitionType34, 0},
                                      {"AMMO35", kAmmunitionType35, 0},
                                      {"AMMO36", kAmmunitionType36, 0},
                                      {"AMMO37", kAmmunitionType37, 0},
                                      {"AMMO38", kAmmunitionType38, 0},
                                      {"AMMO39", kAmmunitionType39, 0},
                                      {"AMMO40", kAmmunitionType40, 0},
                                      {"AMMO41", kAmmunitionType41, 0},
                                      {"AMMO42", kAmmunitionType42, 0},
                                      {"AMMO43", kAmmunitionType43, 0},
                                      {"AMMO44", kAmmunitionType44, 0},
                                      {"AMMO45", kAmmunitionType45, 0},
                                      {"AMMO46", kAmmunitionType46, 0},
                                      {"AMMO47", kAmmunitionType47, 0},
                                      {"AMMO48", kAmmunitionType48, 0},
                                      {"AMMO49", kAmmunitionType49, 0},
                                      {"AMMO50", kAmmunitionType50, 0},
                                      {"AMMO51", kAmmunitionType51, 0},
                                      {"AMMO52", kAmmunitionType52, 0},
                                      {"AMMO53", kAmmunitionType53, 0},
                                      {"AMMO54", kAmmunitionType54, 0},
                                      {"AMMO55", kAmmunitionType55, 0},
                                      {"AMMO56", kAmmunitionType56, 0},
                                      {"AMMO57", kAmmunitionType57, 0},
                                      {"AMMO58", kAmmunitionType58, 0},
                                      {"AMMO59", kAmmunitionType59, 0},
                                      {"AMMO60", kAmmunitionType60, 0},
                                      {"AMMO61", kAmmunitionType61, 0},
                                      {"AMMO62", kAmmunitionType62, 0},
                                      {"AMMO63", kAmmunitionType63, 0},
                                      {"AMMO64", kAmmunitionType64, 0},
                                      {"AMMO65", kAmmunitionType65, 0},
                                      {"AMMO66", kAmmunitionType66, 0},
                                      {"AMMO67", kAmmunitionType67, 0},
                                      {"AMMO68", kAmmunitionType68, 0},
                                      {"AMMO69", kAmmunitionType69, 0},
                                      {"AMMO70", kAmmunitionType70, 0},
                                      {"AMMO71", kAmmunitionType71, 0},
                                      {"AMMO72", kAmmunitionType72, 0},
                                      {"AMMO73", kAmmunitionType73, 0},
                                      {"AMMO74", kAmmunitionType74, 0},
                                      {"AMMO75", kAmmunitionType75, 0},
                                      {"AMMO76", kAmmunitionType76, 0},
                                      {"AMMO77", kAmmunitionType77, 0},
                                      {"AMMO78", kAmmunitionType78, 0},
                                      {"AMMO79", kAmmunitionType79, 0},
                                      {"AMMO80", kAmmunitionType80, 0},
                                      {"AMMO81", kAmmunitionType81, 0},
                                      {"AMMO82", kAmmunitionType82, 0},
                                      {"AMMO83", kAmmunitionType83, 0},
                                      {"AMMO84", kAmmunitionType84, 0},
                                      {"AMMO85", kAmmunitionType85, 0},
                                      {"AMMO86", kAmmunitionType86, 0},
                                      {"AMMO87", kAmmunitionType87, 0},
                                      {"AMMO88", kAmmunitionType88, 0},
                                      {"AMMO89", kAmmunitionType89, 0},
                                      {"AMMO90", kAmmunitionType90, 0},
                                      {"AMMO91", kAmmunitionType91, 0},
                                      {"AMMO92", kAmmunitionType92, 0},
                                      {"AMMO93", kAmmunitionType93, 0},
                                      {"AMMO94", kAmmunitionType94, 0},
                                      {"AMMO95", kAmmunitionType95, 0},
                                      {"AMMO96", kAmmunitionType96, 0},
                                      {"AMMO97", kAmmunitionType97, 0},
                                      {"AMMO98", kAmmunitionType98, 0},
                                      {"AMMO99", kAmmunitionType99, 0},

                                      {nullptr, 0, 0}};

//
//  DDF PARSE ROUTINES
//

static void WeaponStartEntry(const char *name, bool extend)
{
    flag_tests.clear();

    if (!name || !name[0])
    {
        DDFWarnError("New weapon entry is missing a name!");
        name = "WEAPON_WITH_NO_NAME";
    }

    dynamic_weapon = weapondefs.Lookup(name);

    if (extend)
    {
        if (!dynamic_weapon)
            DDFError("Unknown weapon to extend: %s\n", name);

        DDFStateBeginRange(dynamic_weapon->state_grp_);
        return;
    }

    // replaces an existing entry?
    if (dynamic_weapon)
    {
        dynamic_weapon->Default();
    }
    else
    {
        // not found, create a new one
        dynamic_weapon        = new WeaponDefinition;
        dynamic_weapon->name_ = name;

        weapondefs.push_back(dynamic_weapon);
    }

    DDFStateBeginRange(dynamic_weapon->state_grp_);
}

static void WeaponDoTemplate(const char *contents)
{
    WeaponDefinition *other = weapondefs.Lookup(contents);

    if (!other || other == dynamic_weapon)
        DDFError("Unknown weapon template: '%s'\n", contents);

    dynamic_weapon->CopyDetail(*other);

    DDFStateBeginRange(dynamic_weapon->state_grp_);
}

static void WeaponParseField(const char *field, const char *contents, int index, bool is_last)
{
#if (DDF_DEBUG)
    LogDebug("WEAPON_PARSE: %s = %s;\n", field, contents);
#endif

    if (DDFCompareName(field, "TEMPLATE") == 0)
    {
        WeaponDoTemplate(contents);
        return;
    }

    if (DDFMainParseField(weapon_commands, field, contents, (uint8_t *)dynamic_weapon))
        return;

    if (DDFMainParseState((uint8_t *)dynamic_weapon, dynamic_weapon->state_grp_, field, contents, index, is_last,
                          true /* is_weapon */, weapon_starters, weapon_actions))
        return;

    DDFWarnError("Unknown weapons.ddf command: %s\n", field);
}

static void WeaponFinishEntry(void)
{
    // Lobo December 2021: this check seems wrong and breaks DDFWEAP inheritance
    /*if (! dynamic_weapon->state_grp.back().first)
        DDFError("Weapon `%s' has missing states.\n",
            dynamic_weapon->name.c_str());
    */
    DDFStateFinishRange(dynamic_weapon->state_grp_);

    // check stuff...
    int ATK;

    for (ATK = 0; ATK < 4; ATK++)
    {
        if (dynamic_weapon->ammopershot_[ATK] < 0)
        {
            DDFWarnError("Bad %sAMMOPERSHOT value for weapon: %d\n", ATK ? "XXX_" : "",
                         dynamic_weapon->ammopershot_[ATK]);
            dynamic_weapon->ammopershot_[ATK] = 0;
        }

        // zero values for ammopershot really mean infinite ammo
        if (dynamic_weapon->ammopershot_[ATK] == 0)
            dynamic_weapon->ammo_[ATK] = kAmmunitionTypeNoAmmo;

        if (dynamic_weapon->clip_size_[ATK] < 0)
        {
            DDFWarnError("Bad %sCLIPSIZE value for weapon: %d\n", ATK ? "XXX_" : "", dynamic_weapon->clip_size_[ATK]);
            dynamic_weapon->clip_size_[ATK] = 0;
        }

        // check if clip_size + ammopershot makes sense
        if (dynamic_weapon->clip_size_[ATK] > 0 && dynamic_weapon->ammo_[ATK] != kAmmunitionTypeNoAmmo &&
            (dynamic_weapon->clip_size_[ATK] < dynamic_weapon->ammopershot_[ATK] ||
             (dynamic_weapon->clip_size_[ATK] % dynamic_weapon->ammopershot_[ATK] != 0)))
        {
            DDFWarnError("%sAMMOPERSHOT=%d incompatible with %sCLIPSIZE=%d\n", ATK ? "XXX_" : "",
                         dynamic_weapon->ammopershot_[ATK], ATK ? "XXX_" : "", dynamic_weapon->clip_size_[ATK]);
            dynamic_weapon->ammopershot_[ATK] = 1;
        }

        // DISCARD states require the PARTIAL special
        if (dynamic_weapon->discard_state_[ATK] && !(dynamic_weapon->specials_[ATK] & WeaponFlagPartialReload))
        {
            DDFError("Cannot use %sDISCARD states with NO_PARTIAL special.\n", ATK ? "XXX_" : "");
        }
    }

    if (dynamic_weapon->shared_clip_)
    {
        if (dynamic_weapon->clip_size_[0] == 0)
            DDFError("SHARED_CLIP requires a clip weapon (missing CLIPSIZE)\n");

        if (dynamic_weapon->attack_state_[1] == 0 && dynamic_weapon->attack_state_[2] == 0 &&
            dynamic_weapon->attack_state_[3] == 0)
            DDFError("SHARED_CLIP used without 2nd 3rd or 4th attack states.\n");

        if (dynamic_weapon->ammo_[1] != kAmmunitionTypeNoAmmo || dynamic_weapon->ammopershot_[1] != 0 ||
            dynamic_weapon->clip_size_[1] != 0)
        {
            DDFError("SHARED_CLIP cannot be used with SEC_AMMO or SEC_AMMOPERSHOT "
                     "or SEC_CLIPSIZE commands.\n");
        }

        if (dynamic_weapon->ammo_[2] != kAmmunitionTypeNoAmmo || dynamic_weapon->ammopershot_[2] != 0 ||
            dynamic_weapon->clip_size_[2] != 0)
        {
            DDFError("SHARED_CLIP cannot be used with 3RD_AMMO or 3RD_AMMOPERSHOT "
                     "or 3RD_CLIPSIZE commands.\n");
        }

        if (dynamic_weapon->ammo_[3] != kAmmunitionTypeNoAmmo || dynamic_weapon->ammopershot_[3] != 0 ||
            dynamic_weapon->clip_size_[3] != 0)
        {
            DDFError("SHARED_CLIP cannot be used with 4TH_AMMO or 4TH_AMMOPERSHOT "
                     "or 4TH_CLIPSIZE commands.\n");
        }
    }

    if (dynamic_weapon->model_skin_ < 0 || dynamic_weapon->model_skin_ > 9)
        DDFError("Bad MODEL_SKIN value %d in DDF (must be 0-9).\n", dynamic_weapon->model_skin_);

    // backwards compatibility
    if (dynamic_weapon->priority_ < 0)
    {
        DDFWarnError("Using PRIORITY=-1 in weapons.ddf is obsolete !\n");

        dynamic_weapon->dangerous_ = true;
        dynamic_weapon->priority_  = 10;
    }

    if (dynamic_weapon->zoom_factor_ > 0.0)
        dynamic_weapon->zoom_fov_ = RoundToInteger(90 / dynamic_weapon->zoom_factor_);

    dynamic_weapon->model_rotate_ *= kBAMAngle1;

    // Check MBF21 weapon flags that don't correlate to DDFWEAP flags
    for (std::string &flag : flag_tests)
    {
        if (epi::StringCaseCompareASCII(flag, "NOTHRUST") == 0)
            dynamic_weapon->nothrust_ = true;
        else if (epi::StringCaseCompareASCII(flag, "DANGEROUS") == 0)
            dynamic_weapon->dangerous_ = true;
        else if (epi::StringCaseCompareASCII(flag, "FLEEMELEE") == 0)
            continue; // We don't implement FLEEMELEE, but don't present the
                      // user with an error as it's a valid MBF21 flag
        else
            DDFWarnError("DDFWGetSpecialFlags: Unknown Special: %s", flag.c_str());
    }
    flag_tests.clear();
}

static void WeaponClearAll(void)
{
    // not safe to delete weapons, there are (integer) references

    // not using SetDisabledCount() since it breaks castle.wad

    for (WeaponDefinition *wd : weapondefs)
    {
        if (wd)
        {
            wd->no_cheat_ = true;
            wd->autogive_ = false;
        }
    }
}

void DDFReadWeapons(const std::string &data)
{
    DDFReadInfo weapons;

    weapons.tag      = "WEAPONS";
    weapons.lumpname = "DDFWEAP";

    weapons.start_entry  = WeaponStartEntry;
    weapons.parse_field  = WeaponParseField;
    weapons.finish_entry = WeaponFinishEntry;
    weapons.clear_all    = WeaponClearAll;

    DDFMainReadFile(&weapons, data);
}

void DDFWeaponInit(void)
{
    for (WeaponDefinition *w : weapondefs)
    {
        delete w;
        w = nullptr;
    }
    weapondefs.clear();
}

void DDFWeaponCleanUp(void)
{
    // Trim down the required to size
    weapondefs.shrink_to_fit();
}

static void DDFWGetAmmo(const char *info, void *storage)
{
    int *ammo = (int *)storage;
    int  flag_value;

    switch (DDFMainCheckSpecialFlag(info, ammo_types, &flag_value, false, false))
    {
    case kDDFCheckFlagPositive:
    case kDDFCheckFlagNegative:
        (*ammo) = flag_value;
        break;

    case kDDFCheckFlagUser:
    case kDDFCheckFlagUnknown:
        DDFWarnError("Unknown Ammo type '%s'\n", info);
        break;
    }
}

static void DDFWGetUpgrade(const char *info, void *storage)
{
    WeaponDefinition **dest = (WeaponDefinition **)storage;

    *dest = weapondefs.Lookup(info);

    if (*dest == nullptr)
        DDFWarning("Unknown weapon: %s\n", info);
}

static DDFSpecialFlags weapon_specials[] = {{"SILENT_TO_MONSTERS", WeaponFlagSilentToMonsters, 0},
                                            {"ANIMATED", WeaponFlagAnimated, 0},
                                            {"SWITCH", WeaponFlagSwitchAway, 0},
                                            {"TRIGGER", WeaponFlagReloadWhileTrigger, 0},
                                            {"FRESH", WeaponFlagFreshReload, 0},
                                            {"MANUAL", WeaponFlagManualReload, 0},
                                            {"PARTIAL", WeaponFlagPartialReload, 0},
                                            {"NOAUTOFIRE", WeaponFlagNoAutoFire, 0},
                                            {nullptr, WeaponFlagNone, 0}};

//
// DDFWStateGetDEHMelee
//
static void DDFWStateGetDEHMelee(const char *arg, State *cur_state)
{
    if (!arg || !arg[0])
        return;

    if (atkdefs.Lookup(arg))
    {
        cur_state->action_par = atkdefs.Lookup(arg);
        return;
    }

    std::vector<std::string> args = epi::SeparatedStringVector(arg, ',');

    if (args.empty())
        return;

    AttackDefinition *atk = new AttackDefinition();
    atk->name_            = arg;
    atk->attackstyle_     = kAttackStyleCloseCombat;
    atk->attack_class_    = epi::BitSetFromChar('C');
    atk->flags_           = kAttackFlagPlayer;
    atk->damage_.Default(DamageClass::kDamageClassDefaultAttack);
    atk->damage_.nominal_    = 2.0f;
    atk->damage_.linear_max_ = 20.0f;
    atk->puff_ref_           = "PUFF";
    atk->range_              = 64.0f;
    // In case player melee range has been modified, find the first
    // player mobj and use its range to calculate the default.
    // This shouldn't really fail or else it would be hard to
    // play the game
    for (int i = 0, i_end = mobjtypes.size(); i < i_end; i++)
    {
        if (mobjtypes[i]->playernum_ > 0)
        {
            if (mobjtypes[i]->melee_range_ > 0)
                atk->range_ = mobjtypes[i]->melee_range_;
            break;
        }
    }

    size_t arg_size = args.size();

    if (arg_size > 0)
    {
        int damagebase = 0;
        if (sscanf(args[0].c_str(), "%d", &damagebase) == 1 && damagebase != 0)
            atk->damage_.nominal_ = (float)damagebase;
    }
    if (arg_size > 1)
    {
        int damagedice = 0;
        if (sscanf(args[1].c_str(), "%d", &damagedice) == 1 && damagedice != 0)
            atk->damage_.linear_max_ = atk->damage_.nominal_ * damagedice;
    }
    if (arg_size > 2)
    {
        int zerkfactor = 0;
        if (sscanf(args[2].c_str(), "%d", &zerkfactor) == 1 && zerkfactor != 0)
            atk->berserk_mul_ = (float)zerkfactor / 65536.0f;
    }
    if (arg_size > 3)
    {
        int sound_id = 0;
        if (sscanf(args[3].c_str(), "%d", &sound_id) == 1 && sound_id != 0)
        {
            SoundEffectDefinition *sound = sfxdefs.DEHLookup(sound_id);
            atk->sound_                  = sfxdefs.GetEffect(sound->name_.c_str());
        }
    }
    if (arg_size > 4)
    {
        int range = 0;
        if (sscanf(args[4].c_str(), "%d", &range) == 1 && range != 0)
            atk->range_ = (float)range / 65536.0f;
    }

    atkdefs.push_back(atk);
    cur_state->action_par = atk;
}

//
// DDFWStateGetDEHBullet
//
static void DDFWStateGetDEHBullet(const char *arg, State *cur_state)
{
    if (!arg || !arg[0])
        return;

    if (atkdefs.Lookup(arg))
    {
        cur_state->action_par = atkdefs.Lookup(arg);
        return;
    }

    std::vector<std::string> args = epi::SeparatedStringVector(arg, ',');

    if (args.empty())
        return;

    AttackDefinition *atk = new AttackDefinition();
    atk->name_            = arg;
    atk->range_           = 2048.0f;
    atk->attackstyle_     = kAttackStyleShot;
    atk->attack_class_    = epi::BitSetFromChar('B');
    atk->flags_           = kAttackFlagPlayer;
    atk->damage_.Default(DamageClass::kDamageClassDefaultAttack);
    atk->count_              = 1;
    atk->damage_.nominal_    = 5.0f;
    atk->damage_.linear_max_ = 15.0f;
    atk->puff_ref_           = "PUFF";

    size_t arg_size = args.size();

    if (arg_size > 0)
    {
        int hspread = 0;
        if (sscanf(args[0].c_str(), "%d", &hspread) == 1 && hspread != 0)
            atk->accuracy_angle_ = epi::BAMFromDegrees((float)hspread / 65536.0f);
    }
    if (arg_size > 1)
    {
        int vspread = 0;
        if (sscanf(args[1].c_str(), "%d", &vspread) == 1 && vspread != 0)
            atk->accuracy_slope_ = tan((float)vspread / 65536.0f * HMM_PI / 180.0);
    }
    if (arg_size > 2)
    {
        int shots = 0;
        if (sscanf(args[2].c_str(), "%d", &shots) == 1 && shots != 0)
            atk->count_ = shots;
    }
    if (arg_size > 3)
    {
        int damagebase = 0;
        if (sscanf(args[3].c_str(), "%d", &damagebase) == 1 && damagebase != 0)
            atk->damage_.nominal_ = (float)damagebase;
    }
    if (arg_size > 4)
    {
        int damagedice = 0;
        if (sscanf(args[4].c_str(), "%d", &damagedice) == 1 && damagedice != 0)
            atk->damage_.linear_max_ = atk->damage_.nominal_ * damagedice;
    }

    atkdefs.push_back(atk);
    cur_state->action_par = atk;
}

//
// DDFWStateGetDEHProjectile
//
static void DDFWStateGetDEHProjectile(const char *arg, State *cur_state)
{
    if (!arg || !arg[0])
        return;

    if (atkdefs.Lookup(arg))
    {
        cur_state->action_par = atkdefs.Lookup(arg);
        return;
    }

    std::vector<std::string> args = epi::SeparatedStringVector(arg, ',');

    if (args.empty())
        return;

    AttackDefinition *atk = new AttackDefinition();
    atk->name_            = arg;
    atk->atk_mobj_ref_    = args[0];

    size_t arg_size = args.size();

    atk->range_        = 2048.0f;
    atk->attackstyle_  = kAttackStyleProjectile;
    atk->attack_class_ = epi::BitSetFromChar('M');
    atk->flags_        = (AttackFlags)(kAttackFlagPlayer | kAttackFlagInheritTracerFromTarget);
    atk->damage_.Default(DamageClass::kDamageClassDefaultAttack);
    atk->height_ = 32.0f;
    // In case player heights have been modified, find the first
    // player mobj and use its height to calculate the default.
    // This shouldn't really fail or else it would be hard to
    // play the game
    for (int i = 0, i_end = mobjtypes.size(); i < i_end; i++)
    {
        if (mobjtypes[i]->playernum_ > 0)
        {
            atk->height_ = mobjtypes[i]->height_ * 0.5f;
            break;
        }
    }

    if (arg_size > 1)
    {
        int angle = 0;
        if (sscanf(args[1].c_str(), "%d", &angle) == 1 && angle != 0)
            atk->angle_offset_ = epi::BAMFromDegrees((float)angle / 65536.0f);
    }
    if (arg_size > 2)
    {
        int slope = 0;
        if (sscanf(args[2].c_str(), "%d", &slope) == 1 && slope != 0)
            atk->slope_offset_ = tan((float)slope / 65536.0f * HMM_PI / 180.0);
    }
    if (arg_size > 3)
    {
        int xoffset = 0;
        if (sscanf(args[3].c_str(), "%d", &xoffset) == 1 && xoffset != 0)
            atk->xoffset_ = (float)xoffset / 65536.0f;
    }
    if (arg_size > 4)
    {
        int height = 0;
        if (sscanf(args[4].c_str(), "%d", &height) == 1 && height != 0)
            atk->height_ += (float)height / 65536.0f;
    }

    atkdefs.push_back(atk);
    cur_state->action_par = atk;
}

//
// DDFWStateGetString
//
static void DDFWStateGetString(const char *arg, State *cur_state)
{
    if (!arg || !arg[0])
        return;

    cur_state->action_par = epi::CStringDuplicate(arg);
}

//
// DDFWStateGetRADTrigger
//
static void DDFWStateGetRADTrigger(const char *arg, State *cur_state)
{
    if (!arg || !arg[0])
        return;

    // int *val_ptr = new int;
    uint64_t *val_ptr = new uint64_t;

    // Modified RAD_CheckForInt
    const char *pos    = arg;
    int         count  = 0;
    int         length = strlen(arg);

    while (epi::IsDigitASCII(*pos++))
        count++;

    // Is the value an integer?
    if (length != count)
    {
        *val_ptr                = epi::StringHash(arg).Value();
        cur_state->rts_tag_type = 1;
    }
    else
    {
        *val_ptr                = atoi(arg);
        cur_state->rts_tag_type = 0;
    }

    cur_state->action_par = val_ptr;
}

//
// DDFWGetSpecialFlags
//
static void DDFWGetSpecialFlags(const char *info, void *storage)
{
    int flag_value;

    WeaponFlag *dest = (WeaponFlag *)storage;

    switch (DDFMainCheckSpecialFlag(info, weapon_specials, &flag_value, true, false))
    {
    case kDDFCheckFlagPositive:
        *dest = (WeaponFlag)(*dest | flag_value);
        break;

    case kDDFCheckFlagNegative:
        *dest = (WeaponFlag)(*dest & ~flag_value);
        break;

    case kDDFCheckFlagUser:
    case kDDFCheckFlagUnknown: {
        // Check unknown flags in WeaponFinishEntry as some MBF21 flags
        // correlate to non-flag variables
        flag_tests.push_back(info);
        return;
    }
    }
}

//
// DDFWeaponIsUpgrade
//
// Checks whether first weapon is an upgrade of second one,
// including indirectly (e.g. an upgrade of an upgrade).
//
bool DDFWeaponIsUpgrade(WeaponDefinition *weap, WeaponDefinition *old)
{
    if (!weap || !old || weap == old)
        return false;

    for (int loop = 0; loop < 10; loop++)
    {
        if (!weap->upgrade_weap_)
            return false;

        if (weap->upgrade_weap_ == old)
            return true;

        weap = weap->upgrade_weap_;
    }

    return false;
}

// --> Weapon Definition

//
// WeaponDefinition Constructor
//
WeaponDefinition::WeaponDefinition() : name_(), state_grp_()
{
    Default();
}

//
// WeaponDefinition Destructor
//
WeaponDefinition::~WeaponDefinition()
{
}

//
// WeaponDefinition::CopyDetail()
//
void WeaponDefinition::CopyDetail(const WeaponDefinition &src)
{
    state_grp_.clear();

    for (unsigned int i = 0; i < src.state_grp_.size(); i++)
        state_grp_.push_back(src.state_grp_[i]);

    for (int ATK = 0; ATK < 4; ATK++)
    {
        attack_[ATK]      = src.attack_[ATK];
        ammo_[ATK]        = src.ammo_[ATK];
        ammopershot_[ATK] = src.ammopershot_[ATK];
        autofire_[ATK]    = src.autofire_[ATK];
        clip_size_[ATK]   = src.clip_size_[ATK];
        specials_[ATK]    = src.specials_[ATK];

        attack_state_[ATK]  = src.attack_state_[ATK];
        reload_state_[ATK]  = src.reload_state_[ATK];
        discard_state_[ATK] = src.discard_state_[ATK];
        warmup_state_[ATK]  = src.warmup_state_[ATK];
        flash_state_[ATK]   = src.flash_state_[ATK];
    }

    kick_ = src.kick_;

    up_state_    = src.up_state_;
    down_state_  = src.down_state_;
    ready_state_ = src.ready_state_;
    empty_state_ = src.empty_state_;
    idle_state_  = src.idle_state_;
    crosshair_   = src.crosshair_;
    zoom_state_  = src.zoom_state_;

    no_cheat_ = src.no_cheat_;

    autogive_     = src.autogive_;
    feedback_     = src.feedback_;
    upgrade_weap_ = src.upgrade_weap_;

    priority_  = src.priority_;
    dangerous_ = src.dangerous_;

    eject_attack_ = src.eject_attack_;

    idle_    = src.idle_;
    engaged_ = src.engaged_;
    hit_     = src.hit_;
    start_   = src.start_;

    sound1_ = src.sound1_;
    sound2_ = src.sound2_;
    sound3_ = src.sound3_;

    nothrust_ = src.nothrust_;

    bind_key_ = src.bind_key_;

    zoom_fov_     = src.zoom_fov_;
    zoom_factor_  = src.zoom_factor_;
    refire_inacc_ = src.refire_inacc_;
    show_clip_    = src.show_clip_;
    shared_clip_  = src.shared_clip_;

    bobbing_     = src.bobbing_;
    swaying_     = src.swaying_;
    idle_wait_   = src.idle_wait_;
    idle_chance_ = src.idle_chance_;

    model_skin_    = src.model_skin_;
    model_aspect_  = src.model_aspect_;
    model_bias_    = src.model_bias_;
    model_rotate_  = src.model_rotate_;
    model_forward_ = src.model_forward_;
    model_side_    = src.model_side_;

    render_invert_            = src.render_invert_;
    y_adjust_                 = src.y_adjust_;
    ignore_crosshair_scaling_ = src.ignore_crosshair_scaling_;
}

//
// WeaponDefinition::Default()
//
void WeaponDefinition::Default(void)
{
    state_grp_.clear();

    for (int ATK = 0; ATK < 4; ATK++)
    {
        attack_[ATK]      = nullptr;
        ammo_[ATK]        = kAmmunitionTypeNoAmmo;
        ammopershot_[ATK] = 0;
        clip_size_[ATK]   = 0;
        autofire_[ATK]    = false;

        attack_state_[ATK]  = 0;
        reload_state_[ATK]  = 0;
        discard_state_[ATK] = 0;
        warmup_state_[ATK]  = 0;
        flash_state_[ATK]   = 0;
    }

    specials_[0] = kDefaultWeaponFlags;
    specials_[1] = (WeaponFlag)(kDefaultWeaponFlags & ~WeaponFlagSwitchAway);
    specials_[2] = (WeaponFlag)(kDefaultWeaponFlags & ~WeaponFlagSwitchAway);
    specials_[3] = (WeaponFlag)(kDefaultWeaponFlags & ~WeaponFlagSwitchAway);

    kick_ = 0.0f;

    up_state_    = 0;
    down_state_  = 0;
    ready_state_ = 0;
    empty_state_ = 0;
    idle_state_  = 0;

    crosshair_  = 0;
    zoom_state_ = 0;

    no_cheat_ = false;

    autogive_     = false;
    feedback_     = false;
    upgrade_weap_ = nullptr;
    priority_     = 0;
    dangerous_    = false;

    eject_attack_ = nullptr;
    idle_         = nullptr;
    engaged_      = nullptr;
    hit_          = nullptr;
    start_        = nullptr;

    sound1_ = nullptr;
    sound2_ = nullptr;
    sound3_ = nullptr;

    nothrust_     = false;
    bind_key_     = -1;
    zoom_fov_     = kBAMAngle360;
    zoom_factor_  = 0.0;
    refire_inacc_ = false;
    show_clip_    = false;
    shared_clip_  = false;

    bobbing_     = 1.0f;
    swaying_     = 1.0f;
    idle_wait_   = 15 * kTicRate;
    idle_chance_ = 0.12f;

    model_skin_    = 1;
    model_aspect_  = 1.0f;
    model_bias_    = 0.0f;
    model_rotate_  = 0;
    model_forward_ = 0.0f;
    model_side_    = 0.0f;

    render_invert_            = false;
    y_adjust_                 = 0.0f;
    ignore_crosshair_scaling_ = false;
}

// --> Weapon Definition Container

//
// WeaponDefinitionContainer Constructor
//
WeaponDefinitionContainer::WeaponDefinitionContainer()
{
}

//
// WeaponDefinitionContainer Destructor
//
WeaponDefinitionContainer::~WeaponDefinitionContainer()
{
    for (std::vector<WeaponDefinition *>::iterator iter = begin(), iter_end = end(); iter != iter_end; iter++)
    {
        WeaponDefinition *w = *iter;
        delete w;
        w = nullptr;
    }
}

//
// WeaponDefinitionContainer::FindFirst()
//
int WeaponDefinitionContainer::FindFirst(const char *name, size_t startpos)
{
    for (; startpos < size(); startpos++)
    {
        WeaponDefinition *w = at(startpos);
        if (DDFCompareName(w->name_.c_str(), name) == 0)
            return startpos;
    }

    return -1;
}

//
// WeaponDefinitionContainer::Lookup()
//
WeaponDefinition *WeaponDefinitionContainer::Lookup(const char *refname)
{
    int idx = FindFirst(refname, 0);
    if (idx >= 0)
        return (*this)[idx];

    return nullptr;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
