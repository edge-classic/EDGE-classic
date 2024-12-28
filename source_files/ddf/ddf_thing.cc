//----------------------------------------------------------------------------
//  EDGE Data Definition File Code (Things - MOBJs)
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
// Moving Object Setup and Parser Code
//
// -ACB- 1998/08/04 Written.
// -ACB- 1998/09/12 Use DDFMainGetFixed for fixed number references.
// -ACB- 1998/09/13 Use DDFMainGetTime for Time count references.
// -KM- 1998/11/25 Translucency is now a fixed_t. Fixed spelling of available.
// -KM- 1998/12/16 No limit on number of ammo types.
//

#include "ddf_thing.h"

#include <string.h>

#include "ddf_local.h"
#include "ddf_types.h"
#include "epi_str_compare.h"
#include "epi_str_util.h"
#include "p_action.h"

const char *TemplateThing = nullptr; // Lobo 2022: TEMPLATE inheritance fix

MapObjectDefinitionContainer mobjtypes;

static MapObjectDefinition *default_mobjtype;

void DDFMobjGetSpecial(const char *info);
void DDFMobjGetBenefit(const char *info, void *storage);
void DDFMobjGetPickupEffect(const char *info, void *storage);
void DDFMobjGetDLight(const char *info, void *storage);

static void DDFMobjGetGlowType(const char *info, void *storage);
static void DDFMobjGetYAlign(const char *info, void *storage);
static void DDFMobjGetPercentRange(const char *info, void *storage);
static void DDFMobjGetAngleRange(const char *info, void *storage);
static void DDFMobjStateGetRADTrigger(const char *arg, State *cur_state);
static void DDFMobjStateGetDEHSpawn(const char *arg, State *cur_state);
static void DDFMobjStateGetDEHProjectile(const char *arg, State *cur_state);
static void DDFMobjStateGetDEHBullet(const char *arg, State *cur_state);
static void DDFMobjStateGetDEHMelee(const char *arg, State *cur_state);
static void DDFMobjStateGetString(const char *arg, State *cur_state);

static void AddPickupEffect(PickupEffect **list, PickupEffect *cur);

static int dlight_radius_warnings = 0;

static DynamicLightDefinition dummy_dlight;

const DDFCommandList dlight_commands[] = {DDF_FIELD("TYPE", dummy_dlight, type_, DDFMobjGetDLight),
                                          DDF_FIELD("GRAPHIC", dummy_dlight, shape_, DDFMainGetString),
                                          DDF_FIELD("RADIUS", dummy_dlight, radius_, DDFMainGetFloat),
                                          DDF_FIELD("COLOUR", dummy_dlight, colour_, DDFMainGetRGB),
                                          DDF_FIELD("HEIGHT", dummy_dlight, height_, DDFMainGetPercent),
                                          DDF_FIELD("LEAKY", dummy_dlight, leaky_, DDFMainGetBoolean),
                                          DDF_FIELD("AUTOCOLOUR", dummy_dlight, autocolour_reference_, DDFMainGetString),

                                          // backwards compatibility
                                          DDF_FIELD("INTENSITY", dummy_dlight, radius_, DDFMainGetFloat),

                                          {nullptr, nullptr, 0, nullptr}};

static WeaknessDefinition dummy_weakness;

const DDFCommandList weakness_commands[] = {DDF_FIELD("CLASS", dummy_weakness, classes_, DDFMainGetBitSet),
                                            DDF_FIELD("HEIGHTS", dummy_weakness, height_, DDFMobjGetPercentRange),
                                            DDF_FIELD("ANGLES", dummy_weakness, angle_, DDFMobjGetAngleRange),
                                            DDF_FIELD("MULTIPLY", dummy_weakness, multiply_, DDFMainGetFloat),
                                            DDF_FIELD("PAINCHANCE", dummy_weakness, painchance_, DDFMainGetPercent),

                                            {nullptr, nullptr, 0, nullptr}};

MapObjectDefinition *dynamic_mobj;

static MapObjectDefinition dummy_mobj;

const DDFCommandList thing_commands[] = {
    // sub-commands
    DDF_SUB_LIST("DLIGHT", dummy_mobj, dlight_, dlight_commands),
    DDF_SUB_LIST("WEAKNESS", dummy_mobj, weak_, weakness_commands),
    DDF_SUB_LIST("EXPLODE_DAMAGE", dummy_mobj, explode_damage_, damage_commands),
    DDF_SUB_LIST("CHOKE_DAMAGE", dummy_mobj, choke_damage_, damage_commands),

    DDF_FIELD("SPAWNHEALTH", dummy_mobj, spawn_health_, DDFMainGetFloat),
    DDF_FIELD("RADIUS", dummy_mobj, radius_, DDFMainGetFloat),
    DDF_FIELD("HEIGHT", dummy_mobj, height_, DDFMainGetFloat),
    DDF_FIELD("MASS", dummy_mobj, mass_, DDFMainGetFloat),
    DDF_FIELD("SPEED", dummy_mobj, speed_, DDFMainGetFloat),
    DDF_FIELD("FAST", dummy_mobj, fast_, DDFMainGetFloat),
    DDF_FIELD("EXTRA", dummy_mobj, extended_flags_, DDFMobjGetExtra),
    DDF_FIELD("RESPAWN_TIME", dummy_mobj, respawntime_, DDFMainGetTime),
    DDF_FIELD("FUSE", dummy_mobj, fuse_, DDFMainGetTime),
    DDF_FIELD("LIFESPAN", dummy_mobj, fuse_, DDFMainGetTime),
    DDF_FIELD("PALETTE_REMAP", dummy_mobj, palremap_, DDFMainGetColourmap),
    DDF_FIELD("TRANSLUCENCY", dummy_mobj, translucency_, DDFMainGetPercent),

    DDF_FIELD("INITIAL_BENEFIT", dummy_mobj, initial_benefits_, DDFMobjGetBenefit),
    DDF_FIELD("LOSE_BENEFIT", dummy_mobj, lose_benefits_, DDFMobjGetBenefit),
    DDF_FIELD("PICKUP_BENEFIT", dummy_mobj, pickup_benefits_, DDFMobjGetBenefit),
    DDF_FIELD("KILL_BENEFIT", dummy_mobj, kill_benefits_, DDFMobjGetBenefit),
    DDF_FIELD("PICKUP_MESSAGE", dummy_mobj, pickup_message_, DDFMainGetString),
    DDF_FIELD("PICKUP_EFFECT", dummy_mobj, pickup_effects_, DDFMobjGetPickupEffect),

    DDF_FIELD("PAINCHANCE", dummy_mobj, pain_chance_, DDFMainGetPercent),
    DDF_FIELD("MINATTACK_CHANCE", dummy_mobj, minatkchance_, DDFMainGetPercent),
    DDF_FIELD("REACTION_TIME", dummy_mobj, reaction_time_, DDFMainGetTime),
    DDF_FIELD("JUMP_DELAY", dummy_mobj, jump_delay_, DDFMainGetTime),
    DDF_FIELD("JUMP_HEIGHT", dummy_mobj, jumpheight_, DDFMainGetFloat),
    DDF_FIELD("CROUCH_HEIGHT", dummy_mobj, crouchheight_, DDFMainGetFloat),
    DDF_FIELD("VIEW_HEIGHT", dummy_mobj, viewheight_, DDFMainGetPercent),
    DDF_FIELD("SHOT_HEIGHT", dummy_mobj, shotheight_, DDFMainGetPercent),
    DDF_FIELD("MAX_FALL", dummy_mobj, maxfall_, DDFMainGetFloat),
    DDF_FIELD("CASTORDER", dummy_mobj, castorder_, DDFMainGetNumeric),
    DDF_FIELD("CAST_TITLE", dummy_mobj, cast_title_, DDFMainGetString),
    DDF_FIELD("PLAYER", dummy_mobj, playernum_, DDFMobjGetPlayer),
    DDF_FIELD("SIDE", dummy_mobj, side_, DDFMainGetBitSet),
    DDF_FIELD("CLOSE_ATTACK", dummy_mobj, closecombat_, DDFMainRefAttack),
    DDF_FIELD("RANGE_ATTACK", dummy_mobj, rangeattack_, DDFMainRefAttack),
    DDF_FIELD("SPARE_ATTACK", dummy_mobj, spareattack_, DDFMainRefAttack),
    DDF_FIELD("DROPITEM", dummy_mobj, dropitem_ref_, DDFMainGetString),
    DDF_FIELD("BLOOD", dummy_mobj, blood_ref_, DDFMainGetString),
    DDF_FIELD("RESPAWN_EFFECT", dummy_mobj, respawneffect_ref_, DDFMainGetString),
    DDF_FIELD("SPIT_SPOT", dummy_mobj, spitspot_ref_, DDFMainGetString),

    DDF_FIELD("PICKUP_SOUND", dummy_mobj, activesound_, DDFMainLookupSound),
    DDF_FIELD("ACTIVE_SOUND", dummy_mobj, activesound_, DDFMainLookupSound),
    DDF_FIELD("LAUNCH_SOUND", dummy_mobj, seesound_, DDFMainLookupSound),
    DDF_FIELD("AMBIENT_SOUND", dummy_mobj, seesound_, DDFMainLookupSound),
    DDF_FIELD("SIGHTING_SOUND", dummy_mobj, seesound_, DDFMainLookupSound),
    DDF_FIELD("DEATH_SOUND", dummy_mobj, deathsound_, DDFMainLookupSound),
    DDF_FIELD("OVERKILL_SOUND", dummy_mobj, overkill_sound_, DDFMainLookupSound),
    DDF_FIELD("PAIN_SOUND", dummy_mobj, painsound_, DDFMainLookupSound),
    DDF_FIELD("STARTCOMBAT_SOUND", dummy_mobj, attacksound_, DDFMainLookupSound),
    DDF_FIELD("WALK_SOUND", dummy_mobj, walksound_, DDFMainLookupSound),
    DDF_FIELD("JUMP_SOUND", dummy_mobj, jump_sound_, DDFMainLookupSound),
    DDF_FIELD("NOWAY_SOUND", dummy_mobj, noway_sound_, DDFMainLookupSound),
    DDF_FIELD("OOF_SOUND", dummy_mobj, oof_sound_, DDFMainLookupSound),
    DDF_FIELD("FALLPAIN_SOUND", dummy_mobj, fallpain_sound_, DDFMainLookupSound),
    DDF_FIELD("GASP_SOUND", dummy_mobj, gasp_sound_, DDFMainLookupSound),
    DDF_FIELD("SECRET_SOUND", dummy_mobj, secretsound_, DDFMainLookupSound),
    DDF_FIELD("FALLING_SOUND", dummy_mobj, falling_sound_, DDFMainLookupSound),
    DDF_FIELD("RIP_SOUND", dummy_mobj, rip_sound_, DDFMainLookupSound),

    DDF_FIELD("FLOAT_SPEED", dummy_mobj, float_speed_, DDFMainGetFloat),
    DDF_FIELD("STEP_SIZE", dummy_mobj, step_size_, DDFMainGetFloat),
    DDF_FIELD("SPRITE_SCALE", dummy_mobj, scale_, DDFMainGetFloat),
    DDF_FIELD("SPRITE_ASPECT", dummy_mobj, aspect_, DDFMainGetFloat),
    DDF_FIELD("SPRITE_YALIGN", dummy_mobj, yalign_,
              DDFMobjGetYAlign),  // -AJA- 2007/08/08
    DDF_FIELD("MODEL_SKIN", dummy_mobj, model_skin_,
              DDFMainGetNumeric), // -AJA- 2007/10/16
    DDF_FIELD("MODEL_SCALE", dummy_mobj, model_scale_, DDFMainGetFloat),
    DDF_FIELD("MODEL_ASPECT", dummy_mobj, model_aspect_, DDFMainGetFloat),
    DDF_FIELD("MODEL_BIAS", dummy_mobj, model_bias_, DDFMainGetFloat),
    DDF_FIELD("MODEL_ROTATE", dummy_mobj, model_rotate_, DDFMainGetNumeric),
    DDF_FIELD("BOUNCE_SPEED", dummy_mobj, bounce_speed_, DDFMainGetFloat),
    DDF_FIELD("BOUNCE_UP", dummy_mobj, bounce_up_, DDFMainGetFloat),
    DDF_FIELD("SIGHT_SLOPE", dummy_mobj, sight_slope_, DDFMainGetSlope),
    DDF_FIELD("SIGHT_ANGLE", dummy_mobj, sight_angle_, DDFMainGetAngle),
    DDF_FIELD("RIDE_FRICTION", dummy_mobj, ride_friction_, DDFMainGetFloat),
    DDF_FIELD("BOBBING", dummy_mobj, bobbing_, DDFMainGetPercent),
    DDF_FIELD("IMMUNITY_CLASS", dummy_mobj, immunity_, DDFMainGetBitSet),
    DDF_FIELD("RESISTANCE_CLASS", dummy_mobj, resistance_, DDFMainGetBitSet),
    DDF_FIELD("RESISTANCE_MULTIPLY", dummy_mobj, resist_multiply_, DDFMainGetFloat),
    DDF_FIELD("RESISTANCE_PAINCHANCE", dummy_mobj, resist_painchance_, DDFMainGetPercent),
    DDF_FIELD("GHOST_CLASS", dummy_mobj, ghost_,
              DDFMainGetBitSet), // -AJA- 2005/05/15
    DDF_FIELD("SHADOW_TRANSLUCENCY", dummy_mobj, shadow_trans_, DDFMainGetPercent),
    DDF_FIELD("LUNG_CAPACITY", dummy_mobj, lung_capacity_, DDFMainGetTime),
    DDF_FIELD("GASP_START", dummy_mobj, gasp_start_, DDFMainGetTime),
    DDF_FIELD("EXPLODE_RADIUS", dummy_mobj, explode_radius_, DDFMainGetFloat),
    DDF_FIELD("RELOAD_SHOTS", dummy_mobj, reload_shots_,
              DDFMainGetNumeric),    // -AJA- 2004/11/15
    DDF_FIELD("GLOW_TYPE", dummy_mobj, glow_type_,
              DDFMobjGetGlowType),   // -AJA- 2007/08/19
    DDF_FIELD("ARMOUR_PROTECTION", dummy_mobj, armour_protect_,
              DDFMainGetPercent),    // -AJA- 2007/08/22
    DDF_FIELD("ARMOUR_DEPLETION", dummy_mobj, armour_deplete_,
              DDFMainGetPercentAny), // -AJA- 2007/08/22
    DDF_FIELD("ARMOUR_CLASS", dummy_mobj, armour_class_,
              DDFMainGetBitSet),     // -AJA- 2007/08/22

    DDF_FIELD("SIGHT_DISTANCE", dummy_mobj, sight_distance_,
              DDFMainGetFloat),      // Lobo 2022
    DDF_FIELD("HEAR_DISTANCE", dummy_mobj, hear_distance_,
              DDFMainGetFloat),      // Lobo 2022

    DDF_FIELD("MORPH_TIMEOUT", dummy_mobj, morphtimeout_,
              DDFMainGetTime),       // Lobo 2023

    // MBF21/DEHEXTRA
    DDF_FIELD("INFIGHTING_GROUP", dummy_mobj, infight_group_, DDFMainGetNumeric),
    DDF_FIELD("PROJECTILE_GROUP", dummy_mobj, proj_group_, DDFMainGetNumeric),
    DDF_FIELD("SPLASH_GROUP", dummy_mobj, splash_group_, DDFMainGetNumeric),
    DDF_FIELD("FAST_SPEED", dummy_mobj, fast_speed_, DDFMainGetNumeric),
    DDF_FIELD("MELEE_RANGE", dummy_mobj, melee_range_, DDFMainGetFloat),
    DDF_FIELD("DEH_THING_ID", dummy_mobj, deh_thing_id_, DDFMainGetNumeric),

    // -AJA- backwards compatibility cruft...
    DDF_FIELD("EXPLOD_DAMAGE", dummy_mobj, explode_damage_.nominal_, DDFMainGetFloat),
    DDF_FIELD("EXPLOSION_DAMAGE", dummy_mobj, explode_damage_.nominal_, DDFMainGetFloat),
    DDF_FIELD("EXPLOD_DAMAGERANGE", dummy_mobj, explode_damage_.nominal_, DDFMainGetFloat),

    {nullptr, nullptr, 0, nullptr}};

const DDFStateStarter thing_starters[] = {DDF_STATE("SPAWN", "IDLE", dummy_mobj, spawn_state_),
                                          DDF_STATE("IDLE", "IDLE", dummy_mobj, idle_state_),
                                          DDF_STATE("CHASE", "CHASE", dummy_mobj, chase_state_),
                                          DDF_STATE("PAIN", "IDLE", dummy_mobj, pain_state_),
                                          DDF_STATE("MISSILE", "IDLE", dummy_mobj, missile_state_),
                                          DDF_STATE("MELEE", "IDLE", dummy_mobj, melee_state_),
                                          DDF_STATE("DEATH", "REMOVE", dummy_mobj, death_state_),
                                          DDF_STATE("OVERKILL", "REMOVE", dummy_mobj, overkill_state_),
                                          DDF_STATE("RESPAWN", "IDLE", dummy_mobj, raise_state_),
                                          DDF_STATE("RESURRECT", "IDLE", dummy_mobj, res_state_),
                                          DDF_STATE("MEANDER", "MEANDER", dummy_mobj, meander_state_),
                                          DDF_STATE("MORPH", "MORPH", dummy_mobj, morph_state_),
                                          DDF_STATE("BOUNCE", "IDLE", dummy_mobj, bounce_state_),
                                          DDF_STATE("TOUCH", "IDLE", dummy_mobj, touch_state_),
                                          DDF_STATE("RELOAD", "IDLE", dummy_mobj, reload_state_),
                                          DDF_STATE("GIB", "REMOVE", dummy_mobj, gib_state_),

                                          {nullptr, nullptr, 0}};

// -KM- 1998/11/25 Added weapon functions.
// -AJA- 1999/08/09: Moved this here from p_action.h, and added an extra
// field `handle_arg' for things like "WEAPON_SHOOT(FIREBALL)".

const DDFActionCode thing_actions[] = {{"NOTHING", nullptr, nullptr},

                                       {"CLOSEATTEMPTSND", A_MakeCloseAttemptSound, nullptr},
                                       {"COMBOATTACK", A_ComboAttack, nullptr},
                                       {"FACETARGET", A_FaceTarget, nullptr},
                                       {"PLAYSOUND", A_PlaySound, DDFStateGetSound},
                                       {"PLAYSOUND_BOSS", A_PlaySoundBoss, DDFStateGetSound},
                                       {"KILLSOUND", A_KillSound, nullptr},
                                       {"MAKESOUND", A_MakeAmbientSound, nullptr},
                                       {"MAKEACTIVESOUND", A_MakeActiveSound, nullptr},
                                       {"MAKESOUNDRANDOM", A_MakeAmbientSoundRandom, nullptr},
                                       {"MAKEDEATHSOUND", A_MakeDyingSound, nullptr},
                                       {"MAKEDEAD", A_MakeIntoCorpse, nullptr},
                                       {"MAKEOVERKILLSOUND", A_MakeOverKillSound, nullptr},
                                       {"MAKEPAINSOUND", A_MakePainSound, nullptr},
                                       {"PLAYER_SCREAM", A_PlayerScream, nullptr},
                                       {"CLOSE_ATTACK", A_MeleeAttack, DDFStateGetAttack},
                                       {"RANGE_ATTACK", A_RangeAttack, DDFStateGetAttack},
                                       {"SPARE_ATTACK", A_SpareAttack, DDFStateGetAttack},

                                       {"RANGEATTEMPTSND", A_MakeRangeAttemptSound, nullptr},
                                       {"REFIRE_CHECK", A_RefireCheck, nullptr},
                                       {"RELOAD_CHECK", A_ReloadCheck, nullptr},
                                       {"RELOAD_RESET", A_ReloadReset, nullptr},
                                       {"LOOKOUT", A_StandardLook, nullptr},
                                       {"SUPPORT_LOOKOUT", A_PlayerSupportLook, nullptr},
                                       {"CHASE", A_StandardChase, nullptr},
                                       {"RESCHASE", A_ResurrectChase, nullptr},
                                       {"WALKSOUND_CHASE", A_WalkSoundChase, nullptr},
                                       {"MEANDER", A_StandardMeander, nullptr},
                                       {"SUPPORT_MEANDER", A_PlayerSupportMeander, nullptr},
                                       {"EXPLOSIONDAMAGE", A_DamageExplosion, nullptr},
                                       {"THRUST", A_Thrust, nullptr},
                                       {"TRACER", A_HomingProjectile, nullptr},
                                       {"RANDOM_TRACER", A_HomingProjectile, nullptr}, // same as above
                                       {"RESET_SPREADER", A_ResetSpreadCount, nullptr},
                                       {"SMOKING", A_CreateSmokeTrail, nullptr},
                                       {"TRACKERACTIVE", A_TrackerActive, nullptr},
                                       {"TRACKERFOLLOW", A_TrackerFollow, nullptr},
                                       {"TRACKERSTART", A_TrackerStart, nullptr},
                                       {"EFFECTTRACKER", A_EffectTracker, nullptr},
                                       {"CHECKBLOOD", A_CheckBlood, nullptr},
                                       {"CHECKMOVING", A_CheckMoving, nullptr},
                                       {"CHECK_ACTIVITY", A_CheckActivity, nullptr},
                                       {"JUMP", A_Jump, DDFStateGetJump},
                                       {"JUMP_LIQUID", A_JumpLiquid, DDFStateGetJump},
                                       {"JUMP_SKY", A_JumpSky, DDFStateGetJump},
                                       //{"JUMP_STUCK",        A_JumpStuck, DDFStateGetJump},
                                       {"BECOME", A_Become, DDFStateGetBecome},
                                       {"UNBECOME", A_UnBecome, nullptr},
                                       {"MORPH", A_Morph, DDFStateGetMorph}, // same as BECOME but resets health
                                       {"UNMORPH", A_UnMorph, nullptr},      // same as UNBECOME but resets health

                                       {"EXPLODE", A_Explode, nullptr},
                                       {"ACTIVATE_LINETYPE", A_ActivateLineType, DDFStateGetIntPair},
                                       {"RTS_ENABLE_TAGGED", A_EnableRadTrig, DDFMobjStateGetRADTrigger},
                                       {"RTS_DISABLE_TAGGED", A_DisableRadTrig, DDFMobjStateGetRADTrigger},
                                       {"LUA_RUN_SCRIPT", A_RunLuaScript, DDFMobjStateGetString},
                                       {"TOUCHY_REARM", A_TouchyRearm, nullptr},
                                       {"TOUCHY_DISARM", A_TouchyDisarm, nullptr},
                                       {"BOUNCE_REARM", A_BounceRearm, nullptr},
                                       {"BOUNCE_DISARM", A_BounceDisarm, nullptr},
                                       {"PATH_CHECK", A_PathCheck, nullptr},
                                       {"PATH_FOLLOW", A_PathFollow, nullptr},
                                       {"SET_INVULNERABLE", A_SetInvuln, nullptr},
                                       {"CLEAR_INVULNERABLE", A_ClearInvuln, nullptr},
                                       {"SET_PAINCHANCE", A_PainChanceSet, DDFStateGetPercent},
									   
                                       {"GRAVITY", A_Gravity, nullptr},
                                       {"NO_GRAVITY", A_NoGravity, nullptr},

									   {"CLEAR_TARGET", A_ClearTarget, nullptr},
                                       {"FRIEND_LOOKOUT", A_FriendLook, nullptr},

                                       {"SET_SCALE", A_ScaleSet, DDFStateGetFloat},

                                       {"DROPITEM", A_DropItem, DDFStateGetMobj},
                                       {"SPAWN", A_Spawn, DDFStateGetMobj},
                                       {"TRANS_SET", A_TransSet, DDFStateGetPercent},
                                       {"TRANS_FADE", A_TransFade, DDFStateGetPercent},
                                       {"TRANS_MORE", A_TransMore, DDFStateGetPercent},
                                       {"TRANS_LESS", A_TransLess, DDFStateGetPercent},
                                       {"TRANS_ALTERNATE", A_TransAlternate, DDFStateGetPercent},
                                       {"DLIGHT_SET", A_DLightSet, DDFStateGetInteger},
                                       {"DLIGHT_FADE", A_DLightFade, DDFStateGetInteger},
                                       {"DLIGHT_RANDOM", A_DLightRandom, DDFStateGetIntPair},
                                       {"DLIGHT_COLOUR", A_DLightColour, DDFStateGetRGB},
                                       {"SET_SKIN", A_SetSkin, DDFStateGetInteger},

                                       {"FACE", A_FaceDir, DDFStateGetAngle},
                                       {"TURN", A_TurnDir, DDFStateGetAngle},
                                       {"TURN_RANDOM", A_TurnRandom, DDFStateGetAngle},
                                       {"MLOOK_FACE", A_MlookFace, DDFStateGetSlope},
                                       {"MLOOK_TURN", A_MlookTurn, DDFStateGetSlope},
                                       {"MOVE_FWD", A_MoveFwd, DDFStateGetFloat},
                                       {"MOVE_RIGHT", A_MoveRight, DDFStateGetFloat},
                                       {"MOVE_UP", A_MoveUp, DDFStateGetFloat},
                                       {"STOP", A_StopMoving, nullptr},

                                       // Boom/MBF compatibility
                                       {"DIE", A_Die, nullptr},
                                       {"KEEN_DIE", A_KeenDie, nullptr},
                                       {"MUSHROOM", A_Mushroom, nullptr},
                                       {"NOISE_ALERT", A_NoiseAlert, nullptr},
                                       {"DEH_RADIUS_DAMAGE", A_RadiusDamage, DDFStateGetDEHParams},
                                       {"DEH_HEAL_CHASE", A_HealChase, DDFStateGetJumpInt},
                                       {"DEH_SPAWN_OBJECT", A_SpawnObject, DDFMobjStateGetDEHSpawn},
                                       {"DEH_MONSTER_PROJECTILE", A_MonsterProjectile, DDFMobjStateGetDEHProjectile},
                                       {"DEH_MONSTER_BULLET", A_MonsterBulletAttack, DDFMobjStateGetDEHBullet},
                                       {"DEH_MONSTER_MELEE", A_MonsterMeleeAttack, DDFMobjStateGetDEHMelee},
                                       {"CLEAR_TRACER", A_ClearTracer, nullptr},
                                       {"DEH_HEALTH_JUMP", A_JumpIfHealthBelow, DDFStateGetJumpInt},
                                       {"DEH_SEEK_TRACER", A_SeekTracer, DDFStateGetIntPair},
                                       {"DEH_FIND_TRACER", A_FindTracer, DDFStateGetIntPair},
                                       {"DEH_TARGET_SIGHT_JUMP", A_JumpIfTargetInSight, DDFStateGetJumpInt},
                                       {"DEH_TARGET_CLOSER_JUMP", A_JumpIfTargetCloser, DDFStateGetJumpInt},
                                       {"DEH_TRACER_SIGHT_JUMP", A_JumpIfTracerInSight, DDFStateGetJumpInt},
                                       {"DEH_TRACER_CLOSER_JUMP", A_JumpIfTracerCloser, DDFStateGetJumpInt},
                                       {"DEH_FLAG_JUMP", A_JumpIfTracerCloser, DDFStateGetJumpIntPair},
                                       {"DEH_ADD_FLAGS", A_AddFlags, DDFStateGetIntPair},
                                       {"DEH_REMOVE_FLAGS", A_RemoveFlags, DDFStateGetIntPair},

                                       // bossbrain actions
                                       {"BRAINSPIT", A_BrainSpit, nullptr},
                                       {"CUBESPAWN", A_CubeSpawn, nullptr},
                                       {"CUBETRACER", A_HomeToSpot, nullptr},
                                       {"BRAINSCREAM", A_BrainScream, nullptr},
                                       {"BRAINMISSILEEXPLODE", A_BrainMissileExplode, nullptr},
                                       {"BRAINDIE", A_BrainDie, nullptr},

                                       // -AJA- backwards compatibility cruft...
                                       {"VARIEDEXPDAMAGE", A_DamageExplosion, nullptr},
                                       {"VARIED_THRUST", A_Thrust, nullptr},

                                       {nullptr, nullptr, nullptr}};

const DDFSpecialFlags keytype_names[] = {{"BLUECARD", kDoorKeyBlueCard, 0},
                                         {"YELLOWCARD", kDoorKeyYellowCard, 0},
                                         {"REDCARD", kDoorKeyRedCard, 0},
                                         {"GREENCARD", kDoorKeyGreenCard, 0},

                                         {"BLUESKULL", kDoorKeyBlueSkull, 0},
                                         {"YELLOWSKULL", kDoorKeyYellowSkull, 0},
                                         {"REDSKULL", kDoorKeyRedSkull, 0},
                                         {"GREENSKULL", kDoorKeyGreenSkull, 0},

                                         {"GOLD_KEY", kDoorKeyGoldKey, 0},
                                         {"SILVER_KEY", kDoorKeySilverKey, 0},
                                         {"BRASS_KEY", kDoorKeyBrassKey, 0},
                                         {"COPPER_KEY", kDoorKeyCopperKey, 0},
                                         {"STEEL_KEY", kDoorKeySteelKey, 0},
                                         {"WOODEN_KEY", kDoorKeyWoodenKey, 0},
                                         {"FIRE_KEY", kDoorKeyFireKey, 0},
                                         {"WATER_KEY", kDoorKeyWaterKey, 0},

                                         // -AJA- compatibility (this way is the easiest)
                                         {"KEY_BLUECARD", kDoorKeyBlueCard, 0},
                                         {"KEY_YELLOWCARD", kDoorKeyYellowCard, 0},
                                         {"KEY_REDCARD", kDoorKeyRedCard, 0},
                                         {"KEY_GREENCARD", kDoorKeyGreenCard, 0},

                                         {"KEY_BLUESKULL", kDoorKeyBlueSkull, 0},
                                         {"KEY_YELLOWSKULL", kDoorKeyYellowSkull, 0},
                                         {"KEY_REDSKULL", kDoorKeyRedSkull, 0},
                                         {"KEY_GREENSKULL", kDoorKeyGreenSkull, 0},

                                         {nullptr, 0, 0}};

const DDFSpecialFlags armourtype_names[] = {
    {"GREEN_ARMOUR", kArmourTypeGreen, 0},   {"BLUE_ARMOUR", kArmourTypeBlue, 0},
    {"PURPLE_ARMOUR", kArmourTypePurple, 0}, {"YELLOW_ARMOUR", kArmourTypeYellow, 0},
    {"RED_ARMOUR", kArmourTypeRed, 0},       {nullptr, 0, 0}};

const DDFSpecialFlags powertype_names[] = {{"POWERUP_INVULNERABLE", kPowerTypeInvulnerable, 0},
                                           {"POWERUP_BARE_BERSERK", kPowerTypeBerserk, 0},
                                           {"POWERUP_BERSERK", kPowerTypeBerserk, 0},
                                           {"POWERUP_PARTINVIS", kPowerTypePartInvis, 0},
                                           {"POWERUP_TRANSLUCENT", kPowerTypePartInvisTranslucent, 0},
                                           {"POWERUP_ACIDSUIT", kPowerTypeAcidSuit, 0},
                                           {"POWERUP_AUTOMAP", kPowerTypeAllMap, 0},
                                           {"POWERUP_LIGHTGOGGLES", kPowerTypeInfrared, 0},
                                           {"POWERUP_JETPACK", kPowerTypeJetpack, 0},
                                           {"POWERUP_NIGHTVISION", kPowerTypeNightVision, 0},
                                           {"POWERUP_SCUBA", kPowerTypeScuba, 0},
                                           {"POWERUP_TIMESTOP", kPowerTypeTimeStop, 0},
                                           {nullptr, 0, 0}};

const DDFSpecialFlags simplecond_names[] = {
    {"JUMPING", kConditionCheckTypeJumping, 0},     {"CROUCHING", kConditionCheckTypeCrouching, 0},
    {"SWIMMING", kConditionCheckTypeSwimming, 0},   {"ATTACKING", kConditionCheckTypeAttacking, 0},
    {"RAMPAGING", kConditionCheckTypeRampaging, 0}, {"USING", kConditionCheckTypeUsing, 0},
    {"ACTION1", kConditionCheckTypeAction1, 0},     {"ACTION2", kConditionCheckTypeAction2, 0},
    {"WALKING", kConditionCheckTypeWalking, 0},     {nullptr, 0, 0}};

const DDFSpecialFlags inv_types[] = {{"INVENTORY01", kInventoryType01, 0}, {"INVENTORY02", kInventoryType02, 0},
                                     {"INVENTORY03", kInventoryType03, 0}, {"INVENTORY04", kInventoryType04, 0},
                                     {"INVENTORY05", kInventoryType05, 0}, {"INVENTORY06", kInventoryType06, 0},
                                     {"INVENTORY07", kInventoryType07, 0}, {"INVENTORY08", kInventoryType08, 0},
                                     {"INVENTORY09", kInventoryType09, 0}, {"INVENTORY10", kInventoryType10, 0},
                                     {"INVENTORY11", kInventoryType11, 0}, {"INVENTORY12", kInventoryType12, 0},
                                     {"INVENTORY13", kInventoryType13, 0}, {"INVENTORY14", kInventoryType14, 0},
                                     {"INVENTORY15", kInventoryType15, 0}, {"INVENTORY16", kInventoryType16, 0},
                                     {"INVENTORY17", kInventoryType17, 0}, {"INVENTORY18", kInventoryType18, 0},
                                     {"INVENTORY19", kInventoryType19, 0}, {"INVENTORY20", kInventoryType20, 0},
                                     {"INVENTORY21", kInventoryType21, 0}, {"INVENTORY22", kInventoryType22, 0},
                                     {"INVENTORY23", kInventoryType23, 0}, {"INVENTORY24", kInventoryType24, 0},
                                     {"INVENTORY25", kInventoryType25, 0}, {"INVENTORY26", kInventoryType26, 0},
                                     {"INVENTORY27", kInventoryType27, 0}, {"INVENTORY28", kInventoryType28, 0},
                                     {"INVENTORY29", kInventoryType29, 0}, {"INVENTORY30", kInventoryType30, 0},
                                     {"INVENTORY31", kInventoryType31, 0}, {"INVENTORY32", kInventoryType32, 0},
                                     {"INVENTORY33", kInventoryType33, 0}, {"INVENTORY34", kInventoryType34, 0},
                                     {"INVENTORY35", kInventoryType35, 0}, {"INVENTORY36", kInventoryType36, 0},
                                     {"INVENTORY37", kInventoryType37, 0}, {"INVENTORY38", kInventoryType38, 0},
                                     {"INVENTORY39", kInventoryType39, 0}, {"INVENTORY40", kInventoryType40, 0},
                                     {"INVENTORY41", kInventoryType41, 0}, {"INVENTORY42", kInventoryType42, 0},
                                     {"INVENTORY43", kInventoryType43, 0}, {"INVENTORY44", kInventoryType44, 0},
                                     {"INVENTORY45", kInventoryType45, 0}, {"INVENTORY46", kInventoryType46, 0},
                                     {"INVENTORY47", kInventoryType47, 0}, {"INVENTORY48", kInventoryType48, 0},
                                     {"INVENTORY49", kInventoryType49, 0}, {"INVENTORY50", kInventoryType50, 0},
                                     {"INVENTORY51", kInventoryType51, 0}, {"INVENTORY52", kInventoryType52, 0},
                                     {"INVENTORY53", kInventoryType53, 0}, {"INVENTORY54", kInventoryType54, 0},
                                     {"INVENTORY55", kInventoryType55, 0}, {"INVENTORY56", kInventoryType56, 0},
                                     {"INVENTORY57", kInventoryType57, 0}, {"INVENTORY58", kInventoryType58, 0},
                                     {"INVENTORY59", kInventoryType59, 0}, {"INVENTORY60", kInventoryType60, 0},
                                     {"INVENTORY61", kInventoryType61, 0}, {"INVENTORY62", kInventoryType62, 0},
                                     {"INVENTORY63", kInventoryType63, 0}, {"INVENTORY64", kInventoryType64, 0},
                                     {"INVENTORY65", kInventoryType65, 0}, {"INVENTORY66", kInventoryType66, 0},
                                     {"INVENTORY67", kInventoryType67, 0}, {"INVENTORY68", kInventoryType68, 0},
                                     {"INVENTORY69", kInventoryType69, 0}, {"INVENTORY70", kInventoryType70, 0},
                                     {"INVENTORY71", kInventoryType71, 0}, {"INVENTORY72", kInventoryType72, 0},
                                     {"INVENTORY73", kInventoryType73, 0}, {"INVENTORY74", kInventoryType74, 0},
                                     {"INVENTORY75", kInventoryType75, 0}, {"INVENTORY76", kInventoryType76, 0},
                                     {"INVENTORY77", kInventoryType77, 0}, {"INVENTORY78", kInventoryType78, 0},
                                     {"INVENTORY79", kInventoryType79, 0}, {"INVENTORY80", kInventoryType80, 0},
                                     {"INVENTORY81", kInventoryType81, 0}, {"INVENTORY82", kInventoryType82, 0},
                                     {"INVENTORY83", kInventoryType83, 0}, {"INVENTORY84", kInventoryType84, 0},
                                     {"INVENTORY85", kInventoryType85, 0}, {"INVENTORY86", kInventoryType86, 0},
                                     {"INVENTORY87", kInventoryType87, 0}, {"INVENTORY88", kInventoryType88, 0},
                                     {"INVENTORY89", kInventoryType89, 0}, {"INVENTORY90", kInventoryType90, 0},
                                     {"INVENTORY91", kInventoryType91, 0}, {"INVENTORY92", kInventoryType92, 0},
                                     {"INVENTORY93", kInventoryType93, 0}, {"INVENTORY94", kInventoryType94, 0},
                                     {"INVENTORY95", kInventoryType95, 0}, {"INVENTORY96", kInventoryType96, 0},
                                     {"INVENTORY97", kInventoryType97, 0}, {"INVENTORY98", kInventoryType98, 0},
                                     {"INVENTORY99", kInventoryType99, 0}, {nullptr, 0, 0}};

const DDFSpecialFlags counter_types[] = {{"LIVES", kCounterTypeLives, 0},     {"SCORE", kCounterTypeScore, 0},
                                         {"MONEY", kCounterTypeMoney, 0},     {"EXPERIENCE", kCounterTypeExperience, 0},
                                         {"COUNTER01", kCounterTypeLives, 0}, {"COUNTER02", kCounterTypeScore, 0},
                                         {"COUNTER03", kCounterTypeMoney, 0}, {"COUNTER04", kCounterTypeExperience, 0},
                                         {"COUNTER05", kCounterType05, 0},    {"COUNTER06", kCounterType06, 0},
                                         {"COUNTER07", kCounterType07, 0},    {"COUNTER08", kCounterType08, 0},
                                         {"COUNTER09", kCounterType09, 0},    {"COUNTER10", kCounterType10, 0},
                                         {"COUNTER11", kCounterType11, 0},    {"COUNTER12", kCounterType12, 0},
                                         {"COUNTER13", kCounterType13, 0},    {"COUNTER14", kCounterType14, 0},
                                         {"COUNTER15", kCounterType15, 0},    {"COUNTER16", kCounterType16, 0},
                                         {"COUNTER17", kCounterType17, 0},    {"COUNTER18", kCounterType18, 0},
                                         {"COUNTER19", kCounterType19, 0},    {"COUNTER20", kCounterType20, 0},
                                         {"COUNTER21", kCounterType21, 0},    {"COUNTER22", kCounterType22, 0},
                                         {"COUNTER23", kCounterType23, 0},    {"COUNTER24", kCounterType24, 0},
                                         {"COUNTER25", kCounterType25, 0},    {"COUNTER26", kCounterType26, 0},
                                         {"COUNTER27", kCounterType27, 0},    {"COUNTER28", kCounterType28, 0},
                                         {"COUNTER29", kCounterType29, 0},    {"COUNTER30", kCounterType30, 0},
                                         {"COUNTER31", kCounterType31, 0},    {"COUNTER32", kCounterType32, 0},
                                         {"COUNTER33", kCounterType33, 0},    {"COUNTER34", kCounterType34, 0},
                                         {"COUNTER35", kCounterType35, 0},    {"COUNTER36", kCounterType36, 0},
                                         {"COUNTER37", kCounterType37, 0},    {"COUNTER38", kCounterType38, 0},
                                         {"COUNTER39", kCounterType39, 0},    {"COUNTER40", kCounterType40, 0},
                                         {"COUNTER41", kCounterType41, 0},    {"COUNTER42", kCounterType42, 0},
                                         {"COUNTER43", kCounterType43, 0},    {"COUNTER44", kCounterType44, 0},
                                         {"COUNTER45", kCounterType45, 0},    {"COUNTER46", kCounterType46, 0},
                                         {"COUNTER47", kCounterType47, 0},    {"COUNTER48", kCounterType48, 0},
                                         {"COUNTER49", kCounterType49, 0},    {"COUNTER50", kCounterType50, 0},
                                         {"COUNTER51", kCounterType51, 0},    {"COUNTER52", kCounterType52, 0},
                                         {"COUNTER53", kCounterType53, 0},    {"COUNTER54", kCounterType54, 0},
                                         {"COUNTER55", kCounterType55, 0},    {"COUNTER56", kCounterType56, 0},
                                         {"COUNTER57", kCounterType57, 0},    {"COUNTER58", kCounterType58, 0},
                                         {"COUNTER59", kCounterType59, 0},    {"COUNTER60", kCounterType60, 0},
                                         {"COUNTER61", kCounterType61, 0},    {"COUNTER62", kCounterType62, 0},
                                         {"COUNTER63", kCounterType63, 0},    {"COUNTER64", kCounterType64, 0},
                                         {"COUNTER65", kCounterType65, 0},    {"COUNTER66", kCounterType66, 0},
                                         {"COUNTER67", kCounterType67, 0},    {"COUNTER68", kCounterType68, 0},
                                         {"COUNTER69", kCounterType69, 0},    {"COUNTER70", kCounterType70, 0},
                                         {"COUNTER71", kCounterType71, 0},    {"COUNTER72", kCounterType72, 0},
                                         {"COUNTER73", kCounterType73, 0},    {"COUNTER74", kCounterType74, 0},
                                         {"COUNTER75", kCounterType75, 0},    {"COUNTER76", kCounterType76, 0},
                                         {"COUNTER77", kCounterType77, 0},    {"COUNTER78", kCounterType78, 0},
                                         {"COUNTER79", kCounterType79, 0},    {"COUNTER80", kCounterType80, 0},
                                         {"COUNTER81", kCounterType81, 0},    {"COUNTER82", kCounterType82, 0},
                                         {"COUNTER83", kCounterType83, 0},    {"COUNTER84", kCounterType84, 0},
                                         {"COUNTER85", kCounterType85, 0},    {"COUNTER86", kCounterType86, 0},
                                         {"COUNTER87", kCounterType87, 0},    {"COUNTER88", kCounterType88, 0},
                                         {"COUNTER89", kCounterType89, 0},    {"COUNTER90", kCounterType90, 0},
                                         {"COUNTER91", kCounterType91, 0},    {"COUNTER92", kCounterType92, 0},
                                         {"COUNTER93", kCounterType93, 0},    {"COUNTER94", kCounterType94, 0},
                                         {"COUNTER95", kCounterType95, 0},    {"COUNTER96", kCounterType96, 0},
                                         {"COUNTER97", kCounterType97, 0},    {"COUNTER98", kCounterType98, 0},
                                         {"COUNTER99", kCounterType99, 0},    {nullptr, 0, 0}};

//
// DDFCompareName
//
// Compare two names. This is like stricmp(), except that spaces
// and underscors are ignored for comparison purposes.
//
// -AJA- 1999/09/11: written.
//
int DDFCompareName(const char *A, const char *B)
{
    for (;;)
    {
        // Note: must skip stuff BEFORE checking for NUL
        while (*A == ' ' || *A == '_')
            A++;
        while (*B == ' ' || *B == '_')
            B++;

        if (*A == 0 && *B == 0)
            return 0;

        if (*A == 0)
            return -1;
        if (*B == 0)
            return +1;

        if (epi::ToUpperASCII(*A) == epi::ToUpperASCII(*B))
        {
            A++;
            B++;
            continue;
        }

        return epi::ToUpperASCII(*A) - epi::ToUpperASCII(*B);
    }
}

//
//  DDF PARSE ROUTINES
//

static void ThingStartEntry(const char *buffer, bool extend)
{
    if (!buffer || !buffer[0])
    {
        DDFWarnError("New thing entry is missing a name!");
        buffer = "THING_WITH_NO_NAME";
    }

    TemplateThing = nullptr;

    std::string name(buffer);
    int         number = 0;

    const char *pos = strchr(buffer, ':');

    if (pos)
    {
        name = std::string(buffer, pos - buffer);

        number = HMM_MAX(0, atoi(pos + 1));

        if (name.empty())
        {
            DDFWarnError("New thing entry is missing a name!");
            name = "THING_WITH_NO_NAME";
        }
    }

    dynamic_mobj = nullptr;

    int idx = mobjtypes.FindFirst(name.c_str(), 0);

    if (idx >= 0)
    {
        mobjtypes.MoveToEnd(idx);
        dynamic_mobj = mobjtypes[mobjtypes.size() - 1];
    }

    if (extend)
    {
        if (!dynamic_mobj)
            DDFError("Unknown thing to extend: %s\n", name.c_str());

        if (number > 0)
            dynamic_mobj->number_ = number;

        DDFStateBeginRange(dynamic_mobj->state_grp_);
        return;
    }

    // replaces an existing entry?
    if (dynamic_mobj)
    {
        dynamic_mobj->Default();
        dynamic_mobj->number_ = number;
    }
    else
    {
        // not found, create a new one
        dynamic_mobj          = new MapObjectDefinition;
        dynamic_mobj->name_   = name.c_str();
        dynamic_mobj->number_ = number;

        mobjtypes.push_back(dynamic_mobj);
    }

    DDFStateBeginRange(dynamic_mobj->state_grp_);
}

static void ThingDoTemplate(const char *contents)
{
    int idx = mobjtypes.FindFirst(contents, 0);
    if (idx < 0)
        DDFError("Unknown thing template: '%s'\n", contents);

    MapObjectDefinition *other = mobjtypes[idx];
    EPI_ASSERT(other);

    if (other == dynamic_mobj)
        DDFError("Bad thing template: '%s'\n", contents);

    dynamic_mobj->CopyDetail(*other);

    TemplateThing = other->name_.c_str();

    DDFStateBeginRange(dynamic_mobj->state_grp_);
}

void ThingParseField(const char *field, const char *contents, int index, bool is_last)
{
#if (DDF_DEBUG)
    LogDebug("THING_PARSE: %s = %s;\n", field, contents);
#endif

    if (DDFCompareName(field, "TEMPLATE") == 0)
    {
        ThingDoTemplate(contents);
        return;
    }

    // -AJA- this needs special handling (it touches several fields)
    if (DDFCompareName(field, "SPECIAL") == 0 || DDFCompareName(field, "PROJECTILE_SPECIAL") == 0)
    {
        DDFMobjGetSpecial(contents);
        return;
    }

    // handle the "MODEL_ROTATE" command
    if (DDFCompareName(field, "MODEL_ROTATE") == 0)
    {
        if (DDFMainParseField(thing_commands, field, contents, (uint8_t *)dynamic_mobj))
        {
            dynamic_mobj->model_rotate_ *= kBAMAngle1; // apply the rotation
            return;
        }
    }

    if (DDFMainParseField(thing_commands, field, contents, (uint8_t *)dynamic_mobj))
        return;

    if (DDFMainParseState((uint8_t *)dynamic_mobj, dynamic_mobj->state_grp_, field, contents, index, is_last,
                          false /* is_weapon */, thing_starters, thing_actions))
        return;

    DDFWarnError("Unknown thing/attack command: %s\n", field);
}

static void ThingFinishEntry(void)
{
    DDFStateFinishRange(dynamic_mobj->state_grp_);

    // count-as-kill things are automatically monsters
    if (dynamic_mobj->flags_ & kMapObjectFlagCountKill)
        dynamic_mobj->extended_flags_ |= kExtendedFlagMonster;

    // countable items are always pick-up-able
    if (dynamic_mobj->flags_ & kMapObjectFlagCountItem)
        dynamic_mobj->hyper_flags_ |= kHyperFlagForcePickup;

    // shootable things are always pushable
    if (dynamic_mobj->flags_ & kMapObjectFlagShootable)
        dynamic_mobj->hyper_flags_ |= kHyperFlagPushable;

    // check stuff...

    if (dynamic_mobj->mass_ < 1)
    {
        DDFWarnError("Bad MASS value %f in DDF.\n", dynamic_mobj->mass_);
        dynamic_mobj->mass_ = 1;
    }

    // check CAST stuff
    if (dynamic_mobj->castorder_ > 0)
    {
        if (!dynamic_mobj->chase_state_)
            DDFError("Cast object must have CHASE states !\n");

        if (!dynamic_mobj->death_state_)
            DDFError("Cast object must have DEATH states !\n");
    }

    // check DAMAGE stuff
    if (dynamic_mobj->explode_damage_.nominal_ < 0)
    {
        DDFWarnError("Bad EXPLODE_DAMAGE.VAL value %f in DDF.\n", dynamic_mobj->explode_damage_.nominal_);
    }

    if (dynamic_mobj->explode_radius_ < 0)
    {
        DDFError("Bad EXPLODE_RADIUS value %f in DDF.\n", dynamic_mobj->explode_radius_);
    }

    if (dynamic_mobj->reload_shots_ <= 0)
    {
        DDFError("Bad RELOAD_SHOTS value %d in DDF.\n", dynamic_mobj->reload_shots_);
    }

    if (dynamic_mobj->choke_damage_.nominal_ < 0)
    {
        DDFWarnError("Bad CHOKE_DAMAGE.VAL value %f in DDF.\n", dynamic_mobj->choke_damage_.nominal_);
    }

    if (dynamic_mobj->model_skin_ < 0 || dynamic_mobj->model_skin_ > 9)
        DDFError("Bad MODEL_SKIN value %d in DDF (must be 0-9).\n", dynamic_mobj->model_skin_);

    if (dynamic_mobj->dlight_.radius_ > 512)
    {
        if (dlight_radius_warnings < 3)
            DDFWarning("DLIGHT_RADIUS value %1.1f too large (over 512).\n", dynamic_mobj->dlight_.radius_);
        else if (dlight_radius_warnings == 3)
            LogWarning("More too large DLIGHT_RADIUS values found....\n");

        dlight_radius_warnings++;
    }

    // FIXME: check more stuff

    // backwards compatibility: if no idle state, re-use spawn state
    if (dynamic_mobj->idle_state_ == 0)
        dynamic_mobj->idle_state_ = dynamic_mobj->spawn_state_;

    dynamic_mobj->DLightCompatibility();

    if (TemplateThing)
    {
        int idx = mobjtypes.FindFirst(TemplateThing, 0);
        if (idx < 0)
            DDFError("Unknown thing template: \n");

        MapObjectDefinition *other = mobjtypes[idx];

        if (!dynamic_mobj->lose_benefits_)
        {
            if (other->lose_benefits_)
            {
                dynamic_mobj->lose_benefits_  = new Benefit;
                *dynamic_mobj->lose_benefits_ = *other->lose_benefits_;
            }
        }

        if (!dynamic_mobj->pickup_benefits_)
        {
            if (other->pickup_benefits_)
            {
                dynamic_mobj->pickup_benefits_  = new Benefit;
                *dynamic_mobj->pickup_benefits_ = *other->pickup_benefits_;
            }
        }

        if (!dynamic_mobj->kill_benefits_)
        {
            if (other->kill_benefits_)
            {
                dynamic_mobj->kill_benefits_  = new Benefit;
                *dynamic_mobj->kill_benefits_ = *other->kill_benefits_;
            }
        }

        if (dynamic_mobj->pickup_message_.empty())
        {
            dynamic_mobj->pickup_message_ = other->pickup_message_;
        }
    }
    TemplateThing = nullptr;
}

static void ThingClearAll(void)
{
    LogWarning("Ignoring #CLEARALL in things.ddf\n");
}

void DDFReadThings(const std::string &data)
{
    DDFReadInfo things;

    things.tag      = "THINGS";
    things.lumpname = "DDFTHING";

    things.start_entry  = ThingStartEntry;
    things.parse_field  = ThingParseField;
    things.finish_entry = ThingFinishEntry;
    things.clear_all    = ThingClearAll;

    DDFMainReadFile(&things, data);
}

void DDFMobjInit(void)
{
    for (MapObjectDefinition *m : mobjtypes)
    {
        delete m;
        m = nullptr;
    }
    mobjtypes.clear();

    default_mobjtype          = new MapObjectDefinition();
    default_mobjtype->name_   = "__DEFAULT_MOBJ";
    default_mobjtype->number_ = 0;
}

void DDFMobjCleanUp(void)
{
    // lookup references
    for (MapObjectDefinition *m : mobjtypes)
    {
        cur_ddf_entryname = epi::StringFormat("[%s]  (things.ddf)", m->name_.c_str());

        m->dropitem_ = m->dropitem_ref_ != "" ? mobjtypes.Lookup(m->dropitem_ref_.c_str()) : nullptr;
        m->blood_    = m->blood_ref_ != "" ? mobjtypes.Lookup(m->blood_ref_.c_str()) : mobjtypes.Lookup("BLOOD");

        m->respawneffect_ = m->respawneffect_ref_ != ""           ? mobjtypes.Lookup(m->respawneffect_ref_.c_str())
                            : (m->flags_ & kMapObjectFlagSpecial) ? mobjtypes.Lookup("ITEM_RESPAWN")
                                                                  : mobjtypes.Lookup("RESPAWN_FLASH");

        m->spitspot_ = m->spitspot_ref_ != "" ? mobjtypes.Lookup(m->spitspot_ref_.c_str()) : nullptr;

        cur_ddf_entryname.clear();
    }

    mobjtypes.shrink_to_fit();
}

//
// ParseBenefitString
//
// Parses a string like "HEALTH(20:100)".  Returns the number of
// number parameters (0, 1 or 2).  If the brackets are missing, an
// error occurs.  If the numbers cannot be parsed, then 0 is returned
// and the param buffer contains the stuff in brackets (normally the
// param string will be empty).   FIXME: this interface is fucked.
//
static int ParseBenefitString(const char *info, char *name, char *param, float *value, float *limit)
{
    int len = strlen(info);

    const char *pos = strchr(info, '(');

    param[0] = 0;

    // do we have matched parentheses ?
    if (pos && len >= 4 && info[len - 1] == ')')
    {
        int len2 = (pos - info);

        epi::CStringCopyMax(name, info, len2);

        len -= len2 + 2;
        epi::CStringCopyMax(param, pos + 1, len);

        switch (sscanf(param, " %f : %f ", value, limit))
        {
        case 0:
            return 0;

        case 1:
            param[0] = 0;
            return 1;
        case 2:
            param[0] = 0;
            return 2;

        default:
            DDFWarnError("Bad value in benefit string: %s\n", info);
            return -1;
        }
    }
    else if (pos)
    {
        DDFWarnError("Malformed benefit string: %s\n", info);
        return -1;
    }

    strcpy(name, info);
    return 0;
}

//
//  BENEFIT TESTERS
//
//  These return true if the name matches that particular type of
//  benefit (e.g. "ROCKET" for ammo), and then adjusts the benefit
//  according to how many number values there were.  Otherwise returns
//  false.
//

static bool BenefitTryCounter(const char *name, Benefit *be, int num_vals)
{
    if (kDDFCheckFlagPositive != DDFMainCheckSpecialFlag(name, counter_types, &be->sub.type, false, false))
    {
        return false;
    }

    be->type = kBenefitTypeCounter;

    if (num_vals < 1)
    {
        DDFWarnError("Counter benefit used, but amount is missing.\n");
        return false;
    }

    if (num_vals < 2)
    {
        be->limit = be->amount;
    }

    return true;
}

static bool BenefitTryCounterLimit(const char *name, Benefit *be, int num_vals)
{
    char   namebuf[200];
    size_t len = strlen(name);

    // check for ".LIMIT" prefix
    if (len < 7 || DDFCompareName(name + len - 6, ".LIMIT") != 0)
        return false;

    len -= 6;
    epi::CStringCopyMax(namebuf, name, len);

    if (kDDFCheckFlagPositive != DDFMainCheckSpecialFlag(namebuf, counter_types, &be->sub.type, false, false))
    {
        return false;
    }

    be->type  = kBenefitTypeCounterLimit;
    be->limit = 0;

    if (num_vals < 1)
    {
        DDFWarnError("CounterLimit benefit used, but amount is missing.\n");
        return false;
    }

    if (num_vals > 1)
    {
        DDFWarnError("CounterLimit benefit cannot have a limit value.\n");
        return false;
    }
    return true;
}

static bool BenefitTryInventory(const char *name, Benefit *be, int num_vals)
{
    if (kDDFCheckFlagPositive != DDFMainCheckSpecialFlag(name, inv_types, &be->sub.type, false, false))
    {
        return false;
    }

    be->type = kBenefitTypeInventory;

    if (num_vals < 1)
    {
        DDFWarnError("Inventory benefit used, but amount is missing.\n");
        return false;
    }

    if (num_vals < 2)
    {
        be->limit = be->amount;
    }

    return true;
}

static bool BenefitTryInventoryLimit(const char *name, Benefit *be, int num_vals)
{
    char namebuf[200];
    int  len = strlen(name);

    // check for ".LIMIT" prefix
    if (len < 7 || DDFCompareName(name + len - 6, ".LIMIT") != 0)
        return false;

    len -= 6;
    epi::CStringCopyMax(namebuf, name, len);

    if (kDDFCheckFlagPositive != DDFMainCheckSpecialFlag(namebuf, inv_types, &be->sub.type, false, false))
    {
        return false;
    }

    be->type  = kBenefitTypeInventoryLimit;
    be->limit = 0;

    if (num_vals < 1)
    {
        DDFWarnError("InventoryLimit benefit used, but amount is missing.\n");
        return false;
    }

    if (num_vals > 1)
    {
        DDFWarnError("InventoryLimit benefit cannot have a limit value.\n");
        return false;
    }
    return true;
}

static bool BenefitTryAmmo(const char *name, Benefit *be, int num_vals)
{
    if (kDDFCheckFlagPositive != DDFMainCheckSpecialFlag(name, ammo_types, &be->sub.type, false, false))
    {
        return false;
    }

    be->type = kBenefitTypeAmmo;

    if ((AmmunitionType)be->sub.type == kAmmunitionTypeNoAmmo)
    {
        DDFWarnError("Illegal ammo benefit: %s\n", name);
        return false;
    }

    if (num_vals < 1)
    {
        DDFWarnError("Ammo benefit used, but amount is missing.\n");
        return false;
    }

    if (num_vals < 2)
    {
        be->limit = be->amount;
    }

    return true;
}

static bool BenefitTryAmmoLimit(const char *name, Benefit *be, int num_vals)
{
    char   namebuf[200];
    size_t len = strlen(name);

    // check for ".LIMIT" prefix

    if (len < 7 || DDFCompareName(name + len - 6, ".LIMIT") != 0)
        return false;

    len -= 6;
    epi::CStringCopyMax(namebuf, name, len);

    if (kDDFCheckFlagPositive != DDFMainCheckSpecialFlag(namebuf, ammo_types, &be->sub.type, false, false))
    {
        return false;
    }

    be->type  = kBenefitTypeAmmoLimit;
    be->limit = 0;

    if (be->sub.type == kAmmunitionTypeNoAmmo)
    {
        DDFWarnError("Illegal ammolimit benefit: %s\n", name);
        return false;
    }

    if (num_vals < 1)
    {
        DDFWarnError("AmmoLimit benefit used, but amount is missing.\n");
        return false;
    }

    if (num_vals > 1)
    {
        DDFWarnError("AmmoLimit benefit cannot have a limit value.\n");
        return false;
    }

    return true;
}

static bool BenefitTryWeapon(const char *name, Benefit *be, int num_vals)
{
    int idx = weapondefs.FindFirst(name, 0);

    if (idx < 0)
        return false;

    be->sub.weap = weapondefs[idx];

    be->type  = kBenefitTypeWeapon;
    be->limit = 1.0f;

    if (num_vals < 1)
        be->amount = 1.0f;
    else if (be->amount != 0.0f && be->amount != 1.0f)
    {
        DDFWarnError("Weapon benefit used, bad amount value: %1.1f\n", be->amount);
        return false;
    }

    if (num_vals > 1)
    {
        DDFWarnError("Weapon benefit cannot have a limit value.\n");
        return false;
    }

    return true;
}

static bool BenefitTryKey(const char *name, Benefit *be, int num_vals)
{
    if (kDDFCheckFlagPositive != DDFMainCheckSpecialFlag(name, keytype_names, &be->sub.type, false, false))
    {
        return false;
    }

    be->type  = kBenefitTypeKey;
    be->limit = 1.0f;

    if (num_vals < 1)
        be->amount = 1.0f;
    else if (be->amount != 0.0f && be->amount != 1.0f)
    {
        DDFWarnError("Key benefit used, bad amount value: %1.1f\n", be->amount);
        return false;
    }

    if (num_vals > 1)
    {
        DDFWarnError("Key benefit cannot have a limit value.\n");
        return false;
    }

    return true;
}

static bool BenefitTryHealth(const char *name, Benefit *be, int num_vals)
{
    if (DDFCompareName(name, "HEALTH") != 0)
        return false;

    be->type     = kBenefitTypeHealth;
    be->sub.type = 0;

    if (num_vals < 1)
    {
        DDFWarnError("Health benefit used, but amount is missing.\n");
        return false;
    }

    if (num_vals < 2)
        be->limit = 100.0f;

    return true;
}

static bool BenefitTryArmour(const char *name, Benefit *be, int num_vals)
{
    if (kDDFCheckFlagPositive != DDFMainCheckSpecialFlag(name, armourtype_names, &be->sub.type, false, false))
    {
        return false;
    }

    be->type = kBenefitTypeArmour;

    if (num_vals < 1)
    {
        DDFWarnError("Armour benefit used, but amount is missing.\n");
        return false;
    }

    if (num_vals < 2)
    {
        switch (be->sub.type)
        {
        case kArmourTypeGreen:
            be->limit = 100;
            break;
        case kArmourTypeBlue:
            be->limit = 200;
            break;
        case kArmourTypePurple:
            be->limit = 200;
            break;
        case kArmourTypeYellow:
            be->limit = 200;
            break;
        case kArmourTypeRed:
            be->limit = 200;
            break;
        default:;
        }
    }

    return true;
}

static bool BenefitTryPowerup(const char *name, Benefit *be, int num_vals)
{
    if (kDDFCheckFlagPositive != DDFMainCheckSpecialFlag(name, powertype_names, &be->sub.type, false, false))
    {
        return false;
    }

    be->type = kBenefitTypePowerup;

    if (num_vals < 1)
        be->amount = 999999.0f;

    if (num_vals < 2)
        be->limit = 999999.0f;

    // -AJA- backwards compatibility (need Fist for Berserk)
    if (be->sub.type == kPowerTypeBerserk && DDFCompareName(name, "POWERUP_BERSERK") == 0)
    {
        int idx = weapondefs.FindFirst("FIST", 0);

        if (idx >= 0)
        {
            AddPickupEffect(&dynamic_mobj->pickup_effects_,
                            new PickupEffect(kPickupEffectTypeSwitchWeapon, weapondefs[idx], 0, 0));

            AddPickupEffect(&dynamic_mobj->pickup_effects_,
                            new PickupEffect(kPickupEffectTypeKeepPowerup, kPowerTypeBerserk, 0, 0));
        }
    }

    return true;
}

static void BenefitAdd(Benefit **list, Benefit *source)
{
    Benefit *cur, *tail;

    // check if this benefit overrides a previous one
    for (cur = (*list); cur; cur = cur->next)
    {
        if (cur->type == kBenefitTypeWeapon)
            continue;

        if (cur->type == source->type && cur->sub.type == source->sub.type)
        {
            cur->amount = source->amount;
            cur->limit  = source->limit;
            return;
        }
    }

    // nope, create a new one and link it onto the _TAIL_
    cur = new Benefit;

    cur[0]    = source[0];
    cur->next = nullptr;

    if ((*list) == nullptr)
    {
        (*list) = cur;
        return;
    }

    for (tail = (*list); tail && tail->next; tail = tail->next)
    {
    }

    tail->next = cur;
}

//
// DDFMobjGetBenefit
//
// Parse a single benefit and update the benefit list accordingly.  If
// the type/subtype are not in the list, add a new entry, otherwise
// just modify the existing entry.
//
void DDFMobjGetBenefit(const char *info, void *storage)
{
    char namebuf[200];
    char parambuf[200];
    int  num_vals;

    Benefit temp;

    EPI_ASSERT(storage);

    num_vals = ParseBenefitString(info, namebuf, parambuf, &temp.amount, &temp.limit);

    // an error occurred ?
    if (num_vals < 0)
        return;

    if (BenefitTryAmmo(namebuf, &temp, num_vals) || BenefitTryAmmoLimit(namebuf, &temp, num_vals) ||
        BenefitTryWeapon(namebuf, &temp, num_vals) || BenefitTryKey(namebuf, &temp, num_vals) ||
        BenefitTryHealth(namebuf, &temp, num_vals) || BenefitTryArmour(namebuf, &temp, num_vals) ||
        BenefitTryPowerup(namebuf, &temp, num_vals) || BenefitTryInventory(namebuf, &temp, num_vals) ||
        BenefitTryInventoryLimit(namebuf, &temp, num_vals) || BenefitTryCounter(namebuf, &temp, num_vals) ||
        BenefitTryCounterLimit(namebuf, &temp, num_vals))
    {
        BenefitAdd((Benefit **)storage, &temp);
        return;
    }

    DDFWarnError("Unknown/Malformed benefit type: %s\n", namebuf);
}

PickupEffect::PickupEffect(PickupEffectType type, int sub, int slot, float time)
    : next_(nullptr), type_(type), slot_(slot), time_(time)
{
    sub_.type = sub;
}

PickupEffect::PickupEffect(PickupEffectType type, WeaponDefinition *weap, int slot, float time)
    : next_(nullptr), type_(type), slot_(slot), time_(time)
{
    sub_.weap = weap;
}

static void AddPickupEffect(PickupEffect **list, PickupEffect *cur)
{
    cur->next_ = nullptr;

    if ((*list) == nullptr)
    {
        (*list) = cur;
        return;
    }

    PickupEffect *tail;

    for (tail = (*list); tail && tail->next_; tail = tail->next_)
    {
    }

    tail->next_ = cur;
}

void BA_ParsePowerupEffect(PickupEffect **list, float par1, float par2)
{
    int p_up = (int)par1;
    int slot = (int)par2;

    EPI_ASSERT(0 <= p_up && p_up < kTotalPowerTypes);

    if (slot < 0 || slot >= kTotalEffectsSlots)
        DDFError("POWERUP_EFFECT: bad FX slot #%d\n", p_up);

    AddPickupEffect(list, new PickupEffect(kPickupEffectTypePowerupEffect, p_up, slot, 0));
}

void BA_ParseScreenEffect(PickupEffect **list, int pnum, float par1, float par2, const char *word_par)
{
    EPI_UNUSED(pnum);
    EPI_UNUSED(word_par);
    int slot = (int)par1;

    if (slot < 0 || slot >= kTotalEffectsSlots)
        DDFError("SCREEN_EFFECT: bad FX slot #%d\n", slot);

    if (par2 <= 0)
        DDFError("SCREEN_EFFECT: bad time value: %1.2f\n", par2);

    AddPickupEffect(list, new PickupEffect(kPickupEffectTypeScreenEffect, 0, slot, par2));
}

void BA_ParseSwitchWeapon(PickupEffect **list, int pnum, float par1, float par2, const char *word_par)
{
    EPI_UNUSED(par1);
    EPI_UNUSED(par2);
    if (pnum != -1)
        DDFError("SWITCH_WEAPON: missing weapon name !\n");

    EPI_ASSERT(word_par && word_par[0]);

    WeaponDefinition *weap = weapondefs.Lookup(word_par);

    AddPickupEffect(list, new PickupEffect(kPickupEffectTypeSwitchWeapon, weap, 0, 0));
}

void BA_ParseKeepPowerup(PickupEffect **list, int pnum, float par1, float par2, const char *word_par)
{
    EPI_UNUSED(par1);
    EPI_UNUSED(par2);
    if (pnum != -1)
        DDFError("KEEP_POWERUP: missing powerup name !\n");

    EPI_ASSERT(word_par && word_par[0]);

    if (DDFCompareName(word_par, "BERSERK") != 0)
        DDFError("KEEP_POWERUP: %s is not supported\n", word_par);

    AddPickupEffect(list, new PickupEffect(kPickupEffectTypeKeepPowerup, kPowerTypeBerserk, 0, 0));
}

struct PickupEffectParser
{
    const char *name;
    int         num_pars; // -1 means a single word
    void (*parser)(PickupEffect **list, int pnum, float par1, float par2, const char *word_par);
};

static const PickupEffectParser pick_fx_parsers[] = {{"SCREEN_EFFECT", 2, BA_ParseScreenEffect},
                                                     {"SWITCH_WEAPON", -1, BA_ParseSwitchWeapon},
                                                     {"KEEP_POWERUP", -1, BA_ParseKeepPowerup},

                                                     // that's all, folks.
                                                     {nullptr, 0, nullptr}};

//
// DDFMobjGetPickupEffect
//
// Parse a single effect and add it to the effect list accordingly.
// No merging is done.
//
void DDFMobjGetPickupEffect(const char *info, void *storage)
{
    char namebuf[200];
    char parambuf[200];
    int  num_vals;

    EPI_ASSERT(storage);

    PickupEffect **fx_list = (PickupEffect **)storage;

    Benefit temp; // FIXME kludge (write new parser method ?)

    num_vals = ParseBenefitString(info, namebuf, parambuf, &temp.amount, &temp.limit);

    // an error occurred ?
    if (num_vals < 0)
        return;

    if (parambuf[0])
        num_vals = -1;

    for (int i = 0; pick_fx_parsers[i].name; i++)
    {
        if (DDFCompareName(pick_fx_parsers[i].name, namebuf) != 0)
            continue;

        (*pick_fx_parsers[i].parser)(fx_list, num_vals, temp.amount, temp.limit, parambuf);

        return;
    }

    // secondly, try the powerups
    for (int p = 0; powertype_names[p].name; p++)
    {
        if (DDFCompareName(powertype_names[p].name, namebuf) != 0)
            continue;

        BA_ParsePowerupEffect(fx_list, p, temp.amount);

        return;
    }

    DDFError("Unknown/Malformed benefit effect: %s\n", namebuf);
}

// -KM- 1998/11/25 Translucency to fractional.
// -KM- 1998/12/16 Added individual flags for all.
// -AJA- 2000/02/02: Split into two lists.

static const DDFSpecialFlags normal_specials[] = {
    {"AMBUSH", kMapObjectFlagAmbush, 0},
    {"FUZZY", kMapObjectFlagFuzzy, 0},
    {"SOLID", kMapObjectFlagSolid, 0},
    {"ON_CEILING", kMapObjectFlagSpawnCeiling + kMapObjectFlagNoGravity, 0},
    {"FLOATER", kMapObjectFlagFloat + kMapObjectFlagNoGravity, 0},
    {"INERT", kMapObjectFlagNoBlockmap, 0},
    {"TELEPORT_TYPE", kMapObjectFlagNoGravity, 0},
    {"LINKS", kMapObjectFlagNoBlockmap + kMapObjectFlagNoSector, 1},
    {"DAMAGESMOKE", kMapObjectFlagNoBlood, 0},
    {"SHOOTABLE", kMapObjectFlagShootable, 0},
    {"COUNT_AS_KILL", kMapObjectFlagCountKill, 0},
    {"COUNT_AS_ITEM", kMapObjectFlagCountItem, 0},
    {"SKULLFLY", kMapObjectFlagSkullFly, 0},
    {"SPECIAL", kMapObjectFlagSpecial, 0},
    {"SECTOR", kMapObjectFlagNoSector, 1},
    {"BLOCKMAP", kMapObjectFlagNoBlockmap, 1},
    {"SPAWNCEILING", kMapObjectFlagSpawnCeiling, 0},
    {"GRAVITY", kMapObjectFlagNoGravity, 1},
    {"DROPOFF", kMapObjectFlagDropOff, 0},
    {"PICKUP", kMapObjectFlagPickup, 0},
    {"CLIP", kMapObjectFlagNoClip, 1},
    {"SLIDER", kMapObjectFlagSlide, 0},
    {"FLOAT", kMapObjectFlagFloat, 0},
    {"TELEPORT", kMapObjectFlagTeleport, 0},
    {"MISSILE", kMapObjectFlagMissile, 0}, // has a special check
    {"BARE_MISSILE", kMapObjectFlagMissile, 0},
    {"DROPPED", kMapObjectFlagDropped, 0},
    {"CORPSE", kMapObjectFlagCorpse, 0},
    {"STEALTH", kMapObjectFlagStealth, 0},
    {"PRESERVE_MOMENTUM", kMapObjectFlagPreserveMomentum, 0},
    {"DEATHMATCH", kMapObjectFlagNotDeathmatch, 1},
    {"TOUCHY", kMapObjectFlagTouchy, 0},
    {nullptr, 0, 0}};

static DDFSpecialFlags extended_specials[] = {{"RESPAWN", kExtendedFlagNoRespawn, 1},
                                              {"RESURRECT", kExtendedFlagCannotResurrect, 1},
                                              {"DISLOYAL", kExtendedFlagDisloyalToOwnType, 0},
                                              {"TRIGGER_HAPPY", kExtendedFlagTriggerHappy, 0},
                                              {"ATTACK_HURTS", kExtendedFlagOwnAttackHurts, 0},
                                              {"EXPLODE_IMMUNE", kExtendedFlagExplodeImmune, 0},
                                              {"ALWAYS_LOUD", kExtendedFlagAlwaysLoud, 0},
                                              {"BOSSMAN", kExtendedFlagExplodeImmune + kExtendedFlagAlwaysLoud, 0},
                                              {"NEVERTARGETED", kExtendedFlagNeverTarget, 0},
                                              {"GRAV_KILL", kExtendedFlagNoGravityOnKill, 1},
                                              {"GRUDGE", kExtendedFlagNoGrudge, 1},
                                              {"BOUNCE", kExtendedFlagBounce, 0},
                                              {"EDGEWALKER", kExtendedFlagEdgeWalker, 0},
                                              {"GRAVFALL", kExtendedFlagGravityFall, 0},
                                              {"CLIMBABLE", kExtendedFlagClimbable, 0},
                                              {"WATERWALKER", kExtendedFlagWaterWalker, 0},
                                              {"MONSTER", kExtendedFlagMonster, 0},
                                              {"CROSSLINES", kExtendedFlagCrossBlockingLines, 0},
                                              {"FRICTION", kExtendedFlagNoFriction, 1},
                                              {"USABLE", kExtendedFlagUsable, 0},
                                              {"BLOCK_SHOTS", kExtendedFlagBlockShots, 0},
                                              {"TUNNEL", kExtendedFlagTunnel, 0},
                                              {"BORE", (kExtendedFlagTunnel|kExtendedFlagBore), 0},
                                              {"SIMPLE_ARMOUR", kExtendedFlagSimpleArmour, 0},
                                              {nullptr, 0, 0}};

static DDFSpecialFlags hyper_specials[] = {
    {"FORCE_PICKUP", kHyperFlagForcePickup, 0},
    {"SIDE_IMMUNE", kHyperFlagFriendlyFireImmune, 0},
    {"SIDE_GHOST", kHyperFlagFriendlyFirePassesThrough, 0},
    {"ULTRA_LOYAL", kHyperFlagUltraLoyal, 0},
    {"ZBUFFER", kHyperFlagNoZBufferUpdate, 1},
    {"HOVER", kHyperFlagHover, 0},
    {"PUSHABLE", kHyperFlagPushable, 0},
    {"POINT_FORCE", kHyperFlagPointForce, 0},
    {"PASS_MISSILE", kHyperFlagMissilesPassThrough, 0},
    {"INVULNERABLE", kHyperFlagInvulnerable, 0},
    {"VAMPIRE", kHyperFlagVampire, 0},
    {"AUTOAIM", kHyperFlagNoAutoaim, 1},
    {"TILT", kHyperFlagForceModelTilt, 0},
    {"IMMORTAL", kHyperFlagImmortal, 0},
    {"FLOOR_CLIP", kHyperFlagFloorClip, 0},         // Lobo: new FLOOR_CLIP flag
    {"TRIGGER_LINES", kHyperFlagNoTriggerLines, 1}, // Lobo: Cannot activate doors etc.
    {"SHOVEABLE", kHyperFlagShoveable, 0},          // Lobo: can be pushed
    {"SPLASH", kHyperFlagNoSplash, 1},              // Lobo: causes no splash on liquids
    {"DEHACKED_COMPAT", kHyperFlagDehackedCompatibility, 0},
    {"IMMOVABLE", kHyperFlagImmovable, 0},
    {"MUSIC_CHANGER", kHyperFlagMusicChanger, 0},
    {"TRIGGER_TELEPORTS", kHyperFlagTriggerTeleports, 0}, // Lobo: Can always activate teleporters.
    {nullptr, 0, 0}};

// MBF21 Boss Flags are already handled and converted to RTS in the Dehacked processor,
// so they do not appear here
static DDFSpecialFlags mbf21_specials[] = {
    {"LOGRAV", kMBF21FlagLowGravity, 0},
    {"SHORTMRANGE", kMBF21FlagShortMissileRange, 0},
    {"LONGMELEE", kMBF21FlagLongMeleeRange, 0},
    {"FORCERADIUSDMG", kMBF21FlagForceRadiusDamage, 0},
    {nullptr, 0, 0}};

//
// DDFMobjGetSpecial
//
// Compares info the the entries in special flag lists.
// If found, apply attributes for it to current mobj.
//
void DDFMobjGetSpecial(const char *info)
{
    int flag_value;

    // handle the "INVISIBLE" tag
    if (DDFCompareName(info, "INVISIBLE") == 0)
    {
        dynamic_mobj->translucency_ = 0.0f;
        return;
    }

    // handle the "NOSHADOW" tag
    if (DDFCompareName(info, "NOSHADOW") == 0)
    {
        dynamic_mobj->shadow_trans_ = 0.0f;
        return;
    }

    // the "MISSILE" tag needs special treatment, since it sets both
    // normal flags & extended flags.
    if (DDFCompareName(info, "MISSILE") == 0)
    {
        dynamic_mobj->flags_ |= kMapObjectFlagMissile;
        dynamic_mobj->extended_flags_ |= kExtendedFlagCrossBlockingLines | kExtendedFlagNoFriction;
        return;
    }

    int *flag_ptr = &dynamic_mobj->flags_;

    DDFCheckFlagResult res = DDFMainCheckSpecialFlag(info, normal_specials, &flag_value, true, false);

    if (res == kDDFCheckFlagUser || res == kDDFCheckFlagUnknown)
    {
        // wasn't a normal special.  Try the extended ones...
        flag_ptr = &dynamic_mobj->extended_flags_;

        res = DDFMainCheckSpecialFlag(info, extended_specials, &flag_value, true, false);
    }

    if (res == kDDFCheckFlagUser || res == kDDFCheckFlagUnknown)
    {
        // -AJA- 2004/08/25: Try the hyper specials...
        flag_ptr = &dynamic_mobj->hyper_flags_;

        res = DDFMainCheckSpecialFlag(info, hyper_specials, &flag_value, true, false);
    }

    if (res == kDDFCheckFlagUser || res == kDDFCheckFlagUnknown)
    {
        // Try the MBF21 specials...
        flag_ptr = &dynamic_mobj->mbf21_flags_;

        res = DDFMainCheckSpecialFlag(info, mbf21_specials, &flag_value, true, false);
    }

    switch (res)
    {
    case kDDFCheckFlagPositive:
        *flag_ptr |= flag_value;
        break;

    case kDDFCheckFlagNegative:
        *flag_ptr &= ~flag_value;
        break;

    case kDDFCheckFlagUser:
    case kDDFCheckFlagUnknown:
        DDFWarnError("DDFMobjGetSpecial: Unknown special '%s'\n", info);
        break;
    }
}

static DDFSpecialFlags dlight_type_names[] = {{"NONE", kDynamicLightTypeNone, 0},
                                              {"MODULATE", kDynamicLightTypeModulate, 0},
                                              {"ADD", kDynamicLightTypeAdd, 0},

                                              // backwards compatibility
                                              {"LINEAR", kDynamicLightTypeCompatibilityLinear, 0},
                                              {"QUADRATIC", kDynamicLightTypeCompatibilityQuadratic, 0},
                                              {"CONSTANT", kDynamicLightTypeCompatibilityLinear, 0},

                                              {nullptr, 0, 0}};

//
// DDFMobjGetDLight
//
void DDFMobjGetDLight(const char *info, void *storage)
{
    DynamicLightType *dtype = (DynamicLightType *)storage;
    int               flag_value;

    EPI_ASSERT(dtype);

    if (kDDFCheckFlagPositive != DDFMainCheckSpecialFlag(info, dlight_type_names, &flag_value, false, false))
    {
        DDFWarnError("Unknown dlight type '%s'\n", info);
        return;
    }

    (*dtype) = (DynamicLightType)flag_value;
}

//
// DDFMobjGetExtra
//
void DDFMobjGetExtra(const char *info, void *storage)
{
    int *extendedflags = (int *)storage;

    // If keyword is "NULL", then the mobj is not marked as extra.
    // Otherwise it is.

    if (DDFCompareName(info, "NULL") == 0)
    {
        *extendedflags &= ~kExtendedFlagExtra;
    }
    else
    {
        *extendedflags |= kExtendedFlagExtra;
    }
}

//
// DDFMobjGetPlayer
//
// Reads player number and makes sure that maxplayer is large enough.
//
void DDFMobjGetPlayer(const char *info, void *storage)
{
    int *dest = (int *)storage;

    DDFMainGetNumeric(info, storage);

    if (*dest > 32)
        DDFWarning("Player number '%d' will not work.", *dest);
}

static void DDFMobjGetGlowType(const char *info, void *storage)
{
    SectorGlowType *glow = (SectorGlowType *)storage;

    if (epi::StringCaseCompareASCII(info, "FLOOR") == 0)
        *glow = kSectorGlowTypeFloor;
    else if (epi::StringCaseCompareASCII(info, "CEILING") == 0)
        *glow = kSectorGlowTypeCeiling;
    else if (epi::StringCaseCompareASCII(info, "WALL") == 0)
        *glow = kSectorGlowTypeWall;
    else // Unknown/None
        *glow = kSectorGlowTypeNone;
}

static const DDFSpecialFlags sprite_yalign_names[] = {{"BOTTOM", SpriteYAlignmentBottomUp, 0},
                                                      {"MIDDLE", SpriteYAlignmentMiddle, 0},
                                                      {"TOP", SpriteYAlignmentTopDown, 0},

                                                      {nullptr, 0, 0}};

static void DDFMobjGetYAlign(const char *info, void *storage)
{
    if (kDDFCheckFlagPositive != DDFMainCheckSpecialFlag(info, sprite_yalign_names, (int *)storage, false, false))
    {
        DDFWarnError("DDFMobjGetYAlign: Unknown alignment: %s\n", info);
    }
}

static void DDFMobjGetPercentRange(const char *info, void *storage)
{
    EPI_ASSERT(info && storage);

    float *dest = (float *)storage;

    if (sscanf(info, "%f%%:%f%%", dest + 0, dest + 1) != 2)
        DDFError("Bad percentage range: %s\n", info);

    dest[0] /= 100.0f;
    dest[1] /= 100.0f;

    if (dest[0] > dest[1])
        DDFError("Bad percent range (low > high) : %s\n", info);
}

static void DDFMobjGetAngleRange(const char *info, void *storage)
{
    EPI_ASSERT(info && storage);

    BAMAngle *dest = (BAMAngle *)storage;

    float val1, val2;

    if (sscanf(info, "%f:%f", &val1, &val2) != 2)
        DDFError("Bad angle range: %s\n", info);

    dest[0] = epi::BAMFromDegrees(val1);
    dest[1] = epi::BAMFromDegrees(val2);
}

//
// DDFMobjStateGetString
//
static void DDFMobjStateGetString(const char *arg, State *cur_state)
{
    if (!arg || !arg[0])
        return;

    cur_state->action_par = epi::CStringDuplicate(arg);
}

//
// DDFMobjStateGetRADTrigger
//
static void DDFMobjStateGetRADTrigger(const char *arg, State *cur_state)
{
    if (!arg || !arg[0])
        return;

    //int *val_ptr = new int;
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
        *val_ptr                = epi::StringHash64(arg);
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
// DDFMobjStateGetDEHSpawn
//
static void DDFMobjStateGetDEHSpawn(const char *arg, State *cur_state)
{
    if (!arg || !arg[0])
        return;

    std::vector<std::string> args = epi::SeparatedStringVector(arg, ',');

    if (args.empty())
        return;

    DEHSpawnParameters *params = new DEHSpawnParameters;

    params->spawn_name = epi::CStringDuplicate(args[0].c_str());

    size_t arg_size = args.size();

    if (arg_size > 1)
    {
        int angle = 0;
        if (sscanf(args[1].c_str(), "%d", &angle) == 1 && angle != 0)
            params->angle = epi::BAMFromDegrees((float)angle / 65536.0f);
    }
    if (arg_size > 2)
    {
        int x_offset = 0;
        if (sscanf(args[2].c_str(), "%d", &x_offset) == 1 && x_offset != 0)
            params->x_offset = (float)x_offset / 65536.0f;
    }
    if (arg_size > 3)
    {
        int y_offset = 0;
        if (sscanf(args[3].c_str(), "%d", &y_offset) == 1 && y_offset != 0)
            params->y_offset = (float)y_offset / 65536.0f;
    }
    if (arg_size > 4)
    {
        int z_offset = 0;
        if (sscanf(args[4].c_str(), "%d", &z_offset) == 1 && z_offset != 0)
            params->z_offset = (float)z_offset / 65536.0f;
    }
    if (arg_size > 5)
    {
        int x_velocity = 0;
        if (sscanf(args[5].c_str(), "%d", &x_velocity) == 1 && x_velocity != 0)
            params->x_velocity = (float)x_velocity / 65536.0f;
    }
    if (arg_size > 6)
    {
        int y_velocity = 0;
        if (sscanf(args[6].c_str(), "%d", &y_velocity) == 1 && y_velocity != 0)
            params->y_velocity = (float)y_velocity / 65536.0f;
    }
    if (arg_size > 7)
    {
        int z_velocity = 0;
        if (sscanf(args[7].c_str(), "%d", &z_velocity) == 1 && z_velocity != 0)
            params->z_velocity = (float)z_velocity / 65536.0f;
    }

    cur_state->action_par = params;
}

//
// DDFMobjStateGetDEHMelee
//
static void DDFMobjStateGetDEHMelee(const char *arg, State *cur_state)
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
    atk->name_ = arg;
    atk->attackstyle_ = kAttackStyleCloseCombat;
    atk->attack_class_ = epi::BitSetFromChar('C');
    atk->flags_ = (AttackFlags)(kAttackFlagFaceTarget | kAttackFlagNeedSight);
    atk->damage_.Default(DamageClass::kDamageClassDefaultAttack);
    atk->damage_.nominal_ = 3.0f;
    atk->damage_.linear_max_ = 24.0f;
    atk->puff_ref_ = "PUFF";
    atk->range_ = 64.0f;

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
        int sound_id = 0;
        if (sscanf(args[2].c_str(), "%d", &sound_id) == 1 && sound_id != 0)
        {
            SoundEffectDefinition *sound = sfxdefs.DEHLookup(sound_id);
            atk->sound_ = sfxdefs.GetEffect(sound->name_.c_str());
        }
    }
    if (arg_size > 3)
    {
        int range = 0;
        if (sscanf(args[3].c_str(), "%d", &range) == 1 && range != 0)
            atk->range_ = (float)range / 65536.0f;
    }

    atkdefs.push_back(atk);
    cur_state->action_par = atk;
}

//
// DDFMobjStateGetDEHProjectile
//
static void DDFMobjStateGetDEHProjectile(const char *arg, State *cur_state)
{
    if (!arg || !arg[0])
        return;

    // Sort of a WAG based on the avergage 32 attack height vs. 56 thing height in stock DDF
    // for a lot of stock Doom monsters
    float dynamic_atk_height = dynamic_mobj->height_ * 0.5714285714285714f;
    std::string atk_check_name = epi::StringFormat("%s_%d", arg, (int)dynamic_atk_height);

    AttackDefinition *atk_check = atkdefs.Lookup(atk_check_name.c_str());

    if (atk_check)
    {
        cur_state->action_par = atk_check;
        return;
    }

    std::vector<std::string> args = epi::SeparatedStringVector(arg, ',');

    if (args.empty())
        return;

    AttackDefinition *atk = new AttackDefinition();
    atk->name_ = atk_check_name;
    atk->atk_mobj_ref_ = args[0];

    size_t arg_size = args.size();

    atk->range_ = 2048.0f;
    atk->attackstyle_ = kAttackStyleProjectile;
    atk->attack_class_ = epi::BitSetFromChar('M');
    atk->flags_ = (AttackFlags)(kAttackFlagFaceTarget|kAttackFlagInheritTracerFromTarget);
    atk->damage_.Default(DamageClass::kDamageClassDefaultAttack);
    atk->height_ = dynamic_atk_height;

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
// DDFMobjStateGetDEHBullet
//
static void DDFMobjStateGetDEHBullet(const char *arg, State *cur_state)
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
    atk->name_ = arg;
    atk->range_ = 2048.0f;
    atk->attackstyle_ = kAttackStyleShot;
    atk->attack_class_ = epi::BitSetFromChar('B');
    atk->flags_ = kAttackFlagFaceTarget;
    atk->damage_.Default(DamageClass::kDamageClassDefaultAttack);
    atk->count_ = 1;
    atk->damage_.nominal_ = 3.0f;
    atk->damage_.linear_max_ = 15.0f;
    atk->puff_ref_ = "PUFF";

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
//  CONDITION TESTERS
//
//  These return true if the name matches that particular type of
//  condition (e.g. "ROCKET" for ammo), and adjusts the condition
//  accodingly.  Otherwise returns false.
//

static bool ConditionTryCounter(const char *name, const char *sub, ConditionCheck *cond)
{
    if (kDDFCheckFlagPositive != DDFMainCheckSpecialFlag(name, counter_types, &cond->sub.type, false, false))
    {
        return false;
    }

    if (sub[0])
        sscanf(sub, " %f ", &cond->amount);

    cond->cond_type = kConditionCheckTypeCounter;
    return true;
}

static bool ConditionTryInventory(const char *name, const char *sub, ConditionCheck *cond)
{
    if (kDDFCheckFlagPositive != DDFMainCheckSpecialFlag(name, inv_types, &cond->sub.type, false, false))
    {
        return false;
    }

    if (sub[0])
        sscanf(sub, " %f ", &cond->amount);

    cond->cond_type = kConditionCheckTypeInventory;
    return true;
}

static bool ConditionTryAmmo(const char *name, const char *sub, ConditionCheck *cond)
{
    if (kDDFCheckFlagPositive != DDFMainCheckSpecialFlag(name, ammo_types, &cond->sub.type, false, false))
    {
        return false;
    }

    if ((AmmunitionType)cond->sub.type == kAmmunitionTypeNoAmmo)
    {
        DDFWarnError("Illegal ammo in condition: %s\n", name);
        return false;
    }

    if (sub[0])
        sscanf(sub, " %f ", &cond->amount);

    cond->cond_type = kConditionCheckTypeAmmo;
    return true;
}

static bool ConditionTryWeapon(const char *name, const char *sub, ConditionCheck *cond)
{
    EPI_UNUSED(sub);
    int idx = weapondefs.FindFirst(name, 0);

    if (idx < 0)
        return false;

    cond->sub.weap = weapondefs[idx];

    cond->cond_type = kConditionCheckTypeWeapon;
    return true;
}

static bool ConditionTryKey(const char *name, const char *sub, ConditionCheck *cond)
{
    EPI_UNUSED(sub);
    if (kDDFCheckFlagPositive != DDFMainCheckSpecialFlag(name, keytype_names, &cond->sub.type, false, false))
    {
        return false;
    }

    cond->cond_type = kConditionCheckTypeKey;
    return true;
}

static bool ConditionTryHealth(const char *name, const char *sub, ConditionCheck *cond)
{
    if (DDFCompareName(name, "HEALTH") != 0)
        return false;

    if (sub[0])
        sscanf(sub, " %f ", &cond->amount);

    cond->cond_type = kConditionCheckTypeHealth;
    return true;
}

static bool ConditionTryArmour(const char *name, const char *sub, ConditionCheck *cond)
{
    if (DDFCompareName(name, "ARMOUR") == 0)
    {
        cond->sub.type = kTotalArmourTypes;
    }
    else if (kDDFCheckFlagPositive != DDFMainCheckSpecialFlag(name, armourtype_names, &cond->sub.type, false, false))
    {
        return false;
    }

    if (sub[0])
        sscanf(sub, " %f ", &cond->amount);

    cond->cond_type = kConditionCheckTypeArmour;
    return true;
}

static bool ConditionTryPowerup(const char *name, const char *sub, ConditionCheck *cond)
{
    if (kDDFCheckFlagPositive != DDFMainCheckSpecialFlag(name, powertype_names, &cond->sub.type, false, false))
    {
        return false;
    }

    if (sub[0])
    {
        sscanf(sub, " %f ", &cond->amount);

        cond->amount *= (float)kTicRate;
    }

    cond->cond_type = kConditionCheckTypePowerup;
    return true;
}

static bool ConditionTryPlayerState(const char *name, const char *sub, ConditionCheck *cond)
{
    EPI_UNUSED(sub);
    return (kDDFCheckFlagPositive ==
            DDFMainCheckSpecialFlag(name, simplecond_names, (int *)&cond->cond_type, false, false));
}

//
// DDFMainParseCondition
//
// Returns `false' if parsing failed.
//
bool DDFMainParseCondition(const char *info, ConditionCheck *cond)
{
    char typebuf[100];
    char sub_buf[100];

    int         len = strlen(info);
    int         t_off;
    const char *pos;

    cond->negate    = false;
    cond->exact     = false;
    cond->cond_type = kConditionCheckTypeNone;
    cond->amount    = 1;

    EPI_CLEAR_MEMORY(&cond->sub, ConditionCheck::SubType, 1);

    pos = strchr(info, '(');

    // do we have matched parentheses ?
    if (pos && pos > info && len >= 4 && info[len - 1] == ')')
    {
        int len2 = (pos - info);

        epi::CStringCopyMax(typebuf, info, len2);

        len -= len2 + 2;
        epi::CStringCopyMax(sub_buf, pos + 1, len);
    }
    else if (pos || strchr(info, ')'))
    {
        DDFWarnError("Malformed condition string: %s\n", info);
        return false;
    }
    else
    {
        strcpy(typebuf, info);
        sub_buf[0] = 0;
    }

    // check for negation
    t_off = 0;
    if (epi::StringPrefixCaseCompareASCII(typebuf, "NOT_") == 0)
    {
        cond->negate = true;
        t_off        = 4;
    }

    if (epi::StringPrefixCaseCompareASCII(typebuf, "EXACT_") == 0)
    {
        cond->exact = true;
        t_off       = 6;
    }

    if (ConditionTryAmmo(typebuf + t_off, sub_buf, cond) || ConditionTryInventory(typebuf + t_off, sub_buf, cond) ||
        ConditionTryCounter(typebuf + t_off, sub_buf, cond) || ConditionTryWeapon(typebuf + t_off, sub_buf, cond) ||
        ConditionTryKey(typebuf + t_off, sub_buf, cond) || ConditionTryHealth(typebuf + t_off, sub_buf, cond) ||
        ConditionTryArmour(typebuf + t_off, sub_buf, cond) || ConditionTryPowerup(typebuf + t_off, sub_buf, cond) ||
        ConditionTryPlayerState(typebuf + t_off, sub_buf, cond))
    {
        return true;
    }

    DDFWarnError("Unknown/Malformed condition type: %s\n", typebuf);
    return false;
}

// ---> mobjdef class

MapObjectDefinition::MapObjectDefinition() : name_(), state_grp_()
{
    Default();
}

MapObjectDefinition::~MapObjectDefinition()
{
}

void MapObjectDefinition::CopyDetail(MapObjectDefinition &src)
{
    state_grp_.clear();

    for (size_t i = 0; i < src.state_grp_.size(); i++)
        state_grp_.push_back(src.state_grp_[i]);

    spawn_state_    = src.spawn_state_;
    idle_state_     = src.idle_state_;
    chase_state_    = src.chase_state_;
    pain_state_     = src.pain_state_;
    missile_state_  = src.missile_state_;
    melee_state_    = src.melee_state_;
    death_state_    = src.death_state_;
    overkill_state_ = src.overkill_state_;
    raise_state_    = src.raise_state_;
    res_state_      = src.res_state_;
    meander_state_  = src.meander_state_;
    morph_state_    = src.morph_state_;
    bounce_state_   = src.bounce_state_;
    touch_state_    = src.touch_state_;
    reload_state_   = src.reload_state_;
    gib_state_      = src.gib_state_;

    reaction_time_ = src.reaction_time_;
    pain_chance_   = src.pain_chance_;
    spawn_health_  = src.spawn_health_;
    speed_         = src.speed_;
    float_speed_   = src.float_speed_;
    radius_        = src.radius_;
    height_        = src.height_;
    step_size_     = src.step_size_;
    mass_          = src.mass_;

    flags_          = src.flags_;
    extended_flags_ = src.extended_flags_;
    hyper_flags_    = src.hyper_flags_;
    mbf21_flags_    = src.mbf21_flags_;

    explode_damage_ = src.explode_damage_;
    explode_radius_ = src.explode_radius_;

    // pickup_message_ = src.pickup_message_;
    // lose_benefits_ = src.lose_benefits_;
    // pickup_benefits_ = src.pickup_benefits_;
    if (src.pickup_message_ != "")
    {
        pickup_message_ = src.pickup_message_;
    }

    lose_benefits_   = nullptr;
    pickup_benefits_ = nullptr;
    kill_benefits_   = nullptr; // I think? - Dasho
    /*
    if(src.pickup_benefits_)
    {
        LogDebug("%s: Benefits info not inherited from '%s', ",name,
    src.name_.c_str()); LogDebug("You should define it explicitly.\n");
    }
    */

    pickup_effects_   = src.pickup_effects_;
    initial_benefits_ = src.initial_benefits_;

    castorder_    = src.castorder_;
    cast_title_   = src.cast_title_;
    respawntime_  = src.respawntime_;
    translucency_ = src.translucency_;
    minatkchance_ = src.minatkchance_;
    palremap_     = src.palremap_;

    jump_delay_   = src.jump_delay_;
    jumpheight_   = src.jumpheight_;
    crouchheight_ = src.crouchheight_;
    viewheight_   = src.viewheight_;
    shotheight_   = src.shotheight_;
    maxfall_      = src.maxfall_;
    fast_         = src.fast_;

    scale_  = src.scale_;
    aspect_ = src.aspect_;
    yalign_ = src.yalign_;

    model_skin_   = src.model_skin_;
    model_scale_  = src.model_scale_;
    model_aspect_ = src.model_aspect_;
    model_bias_   = src.model_bias_;
    model_rotate_ = src.model_rotate_;

    bounce_speed_  = src.bounce_speed_;
    bounce_up_     = src.bounce_up_;
    sight_slope_   = src.sight_slope_;
    sight_angle_   = src.sight_angle_;
    ride_friction_ = src.ride_friction_;
    shadow_trans_  = src.shadow_trans_;
    glow_type_     = src.glow_type_;

    seesound_       = src.seesound_;
    attacksound_    = src.attacksound_;
    painsound_      = src.painsound_;
    deathsound_     = src.deathsound_;
    overkill_sound_ = src.overkill_sound_;
    activesound_    = src.activesound_;
    walksound_      = src.walksound_;
    jump_sound_     = src.jump_sound_;
    noway_sound_    = src.noway_sound_;
    oof_sound_      = src.oof_sound_;
    fallpain_sound_ = src.fallpain_sound_;
    gasp_sound_     = src.gasp_sound_;
    secretsound_    = src.secretsound_;
    falling_sound_  = src.falling_sound_;
    rip_sound_      = src.rip_sound_;

    fuse_           = src.fuse_;
    reload_shots_   = src.reload_shots_;
    armour_protect_ = src.armour_protect_;
    armour_deplete_ = src.armour_deplete_;
    armour_class_   = src.armour_class_;

    side_          = src.side_;
    playernum_     = src.playernum_;
    lung_capacity_ = src.lung_capacity_;
    gasp_start_    = src.gasp_start_;

    // choke_damage
    choke_damage_ = src.choke_damage_;

    bobbing_           = src.bobbing_;
    immunity_          = src.immunity_;
    resistance_        = src.resistance_;
    resist_multiply_   = src.resist_multiply_;
    resist_painchance_ = src.resist_painchance_;
    ghost_             = src.ghost_;

    closecombat_ = src.closecombat_;
    rangeattack_ = src.rangeattack_;
    spareattack_ = src.spareattack_;

    // dynamic light info
    dlight_ = src.dlight_;

    weak_ = src.weak_;

    dropitem_          = src.dropitem_;
    dropitem_ref_      = src.dropitem_ref_;
    blood_             = src.blood_;
    blood_ref_         = src.blood_ref_;
    respawneffect_     = src.respawneffect_;
    respawneffect_ref_ = src.respawneffect_ref_;
    spitspot_          = src.spitspot_;
    spitspot_ref_      = src.spitspot_ref_;

    sight_distance_ = src.sight_distance_;
    hear_distance_  = src.hear_distance_;

    morphtimeout_ = src.morphtimeout_;

    infight_group_ = src.infight_group_;
    proj_group_    = src.proj_group_;
    splash_group_  = src.splash_group_;
    fast_speed_    = src.fast_speed_;
    melee_range_   = src.melee_range_;
    deh_thing_id_  = src.deh_thing_id_;
}

void MapObjectDefinition::Default()
{
    state_grp_.clear();

    spawn_state_    = 0;
    idle_state_     = 0;
    chase_state_    = 0;
    pain_state_     = 0;
    missile_state_  = 0;
    melee_state_    = 0;
    death_state_    = 0;
    overkill_state_ = 0;
    raise_state_    = 0;
    res_state_      = 0;
    meander_state_  = 0;
    morph_state_    = 0;
    bounce_state_   = 0;
    touch_state_    = 0;
    reload_state_   = 0;
    gib_state_      = 0;

    reaction_time_ = 0;
    pain_chance_   = 0.0f;
    spawn_health_  = 1000.0f;
    speed_         = 0;
    float_speed_   = 2.0f;
    radius_        = 0;
    height_        = 0;
    step_size_     = 24.0f;
    mass_          = 100.0f;

    flags_          = 0;
    extended_flags_ = 0;
    hyper_flags_    = 0;
    mbf21_flags_    = 0;

    explode_damage_.Default(DamageClass::kDamageClassDefaultMobj);
    explode_radius_ = 0;

    lose_benefits_    = nullptr;
    pickup_benefits_  = nullptr;
    kill_benefits_    = nullptr;
    pickup_effects_   = nullptr;
    pickup_message_   = "";
    initial_benefits_ = nullptr;

    castorder_ = 0;
    cast_title_.clear();
    respawntime_  = 30 * kTicRate;
    translucency_ = 1.0f;
    minatkchance_ = 0.0f;
    palremap_     = nullptr;

    jump_delay_   = 1 * kTicRate;
    jumpheight_   = 10;
    crouchheight_ = 28;
    viewheight_   = 0.75f;
    shotheight_   = 0.64f;
    maxfall_      = 0;
    fast_         = 1.0f;
    scale_        = 1.0f;
    aspect_       = 1.0f;
    yalign_       = SpriteYAlignmentBottomUp;

    model_skin_   = 1;
    model_scale_  = 1.0f;
    model_aspect_ = 1.0f;
    model_bias_   = 0.0f;
    model_rotate_ = 0;

    bounce_speed_  = 0.5f;
    bounce_up_     = 0.5f;
    sight_slope_   = 16.0f;
    sight_angle_   = kBAMAngle90;
    ride_friction_ = kRideFrictionDefault;
    shadow_trans_  = 0.5f;
    glow_type_     = kSectorGlowTypeNone;

    seesound_       = nullptr;
    attacksound_    = nullptr;
    painsound_      = nullptr;
    deathsound_     = nullptr;
    overkill_sound_ = nullptr;
    activesound_    = nullptr;
    walksound_      = nullptr;
    jump_sound_     = nullptr;
    noway_sound_    = nullptr;
    oof_sound_      = nullptr;
    fallpain_sound_ = nullptr;
    gasp_sound_     = nullptr;
    // secretsound_ = nullptr;
    secretsound_   = sfxdefs.GetEffect("SECRET");
    falling_sound_ = nullptr;
    rip_sound_     = nullptr;

    fuse_           = 0;
    reload_shots_   = 5;
    armour_protect_ = -1.0; // disabled!
    armour_deplete_ = 1.0f;
    armour_class_   = kBitSetFull;

    side_          = 0;
    playernum_     = 0;
    lung_capacity_ = 20 * kTicRate;
    gasp_start_    = 2 * kTicRate;

    choke_damage_.Default(DamageClass::kDamageClassDefaultMobjChoke);

    bobbing_           = 1.0f;
    immunity_          = 0;
    resistance_        = 0;
    resist_multiply_   = 0.4;
    resist_painchance_ = -1; // disabled
    ghost_             = 0;

    closecombat_ = nullptr;
    rangeattack_ = nullptr;
    spareattack_ = nullptr;

    // dynamic light info
    dlight_.Default();

    weak_.Default();

    dropitem_ = nullptr;
    dropitem_ref_.clear();
    blood_ = nullptr;
    blood_ref_.clear();
    respawneffect_ = nullptr;
    respawneffect_ref_.clear();
    spitspot_ = nullptr;
    spitspot_ref_.clear();

    sight_distance_ = -1;
    hear_distance_  = -1;

    morphtimeout_ = 0;

    infight_group_ = -2;
    proj_group_    = -2;
    splash_group_  = -2;
    fast_speed_    = -1;
    melee_range_   = -1;
    deh_thing_id_  =  0;
}

void MapObjectDefinition::DLightCompatibility(void)
{
    int r = epi::GetRGBARed(dlight_.colour_);
    int g = epi::GetRGBAGreen(dlight_.colour_);
    int b = epi::GetRGBABlue(dlight_.colour_);

    // dim the colour
    r = int(r * 0.8f);
    g = int(g * 0.8f);
    b = int(b * 0.8f);

    switch (dlight_.type_)
    {
    case kDynamicLightTypeCompatibilityQuadratic:
        dlight_.type_   = kDynamicLightTypeModulate;
        dlight_.radius_ = DynamicLightCompatibilityRadius(dlight_.radius_);
        dlight_.colour_ = epi::MakeRGBA(r, g, b);

        hyper_flags_ |= kHyperFlagQuadraticDynamicLight;
        break;

    case kDynamicLightTypeCompatibilityLinear:
        dlight_.type_ = kDynamicLightTypeModulate;
        dlight_.radius_ *= 1.3;
        dlight_.colour_ = epi::MakeRGBA(r, g, b);
        break;

    default: // nothing to do
        break;
    }
}

// --> MapObjectDefinitionContainer class

MapObjectDefinitionContainer::MapObjectDefinitionContainer()
{
    EPI_CLEAR_MEMORY(lookup_cache_, MapObjectDefinition *, kLookupCacheSize);
}

MapObjectDefinitionContainer::~MapObjectDefinitionContainer()
{
    for (std::vector<MapObjectDefinition *>::iterator iter = begin(), iter_end = end(); iter != iter_end; iter++)
    {
        MapObjectDefinition *m = *iter;
        delete m;
        m = nullptr;
    }
}

int MapObjectDefinitionContainer::FindFirst(const char *name, size_t startpos)
{
    for (; startpos < size(); startpos++)
    {
        MapObjectDefinition *m = at(startpos);
        if (DDFCompareName(m->name_.c_str(), name) == 0)
            return startpos;
    }

    return -1;
}

int MapObjectDefinitionContainer::FindLast(const char *name)
{
    int startpos = (int)size() - 1;

    for (; startpos >= 0; startpos--)
    {
        MapObjectDefinition *m = at(startpos);
        if (DDFCompareName(m->name_.c_str(), name) == 0)
            return startpos;
    }

    return -1;
}

bool MapObjectDefinitionContainer::MoveToEnd(int idx)
{
    // Moves an entry from its current position to end of the list.

    MapObjectDefinition *m = nullptr;

    if (idx < 0 || (size_t)idx >= size())
        return false;

    if ((size_t)idx == (size() - 1))
        return true; // Already at the end

    // Get a copy of the pointer
    m = at(idx);

    erase(begin() + idx);

    push_back(m);

    return true;
}

const MapObjectDefinition *MapObjectDefinitionContainer::Lookup(const char *refname)
{
    // Looks an mobjdef by name.
    // Fatal error if it does not exist.

    int idx = FindLast(refname);

    if (idx >= 0)
        return (*this)[idx];

    if (lax_errors)
        return default_mobjtype;

    DDFError("Unknown thing type: %s\n", refname);
    return nullptr; /* NOT REACHED */
}

const MapObjectDefinition *MapObjectDefinitionContainer::Lookup(int id)
{
    if (id == 0)
        return default_mobjtype;

    // Looks an mobjdef by number.
    // Fatal error if it does not exist.

    int slot = (((id) + kLookupCacheSize) % kLookupCacheSize);

    // check the cache
    if (lookup_cache_[slot] && lookup_cache_[slot]->number_ == id)
    {
        return lookup_cache_[slot];
    }

    for (std::vector<MapObjectDefinition *>::reverse_iterator iter = rbegin(), iter_end = rend(); iter != iter_end;
         iter++)
    {
        MapObjectDefinition *m = *iter;

        if (m->number_ == id)
        {
            // update the cache
            lookup_cache_[slot] = m;
            return m;
        }
    }

    return nullptr;
}

const MapObjectDefinition *MapObjectDefinitionContainer::LookupCastMember(int castpos)
{
    // Lookup the cast member of the one with the nearest match
    // to the position given.

    MapObjectDefinition *best = nullptr;
    MapObjectDefinition *m    = nullptr;

    for (std::vector<MapObjectDefinition *>::reverse_iterator iter = rbegin(), iter_end = rend(); iter != iter_end;
         iter++)
    {
        m = *iter;
        if (m->castorder_ > 0)
        {
            if (m->castorder_ == castpos) // Exact match
                return m;

            if (best)
            {
                if (m->castorder_ > castpos)
                {
                    if (best->castorder_ > castpos)
                    {
                        int of1 = m->castorder_ - castpos;
                        int of2 = best->castorder_ - castpos;

                        if (of2 > of1)
                            best = m;
                    }
                    else
                    {
                        // Our previous was before the requested
                        // entry in the cast order, this is later and
                        // as such always better.
                        best = m;
                    }
                }
                else
                {
                    // We only care about updating this if the
                    // best match was also prior to current
                    // entry. In this case we are looking for
                    // the first entry to wrap around to.
                    if (best->castorder_ < castpos)
                    {
                        int of1 = castpos - m->castorder_;
                        int of2 = castpos - best->castorder_;

                        if (of1 > of2)
                            best = m;
                    }
                }
            }
            else
            {
                // We don't have a best item, so this has to be our best current
                // match
                best = m;
            }
        }
    }

    return best;
}

const MapObjectDefinition *MapObjectDefinitionContainer::LookupPlayer(int playernum)
{
    // Find a player thing (needed by deathmatch code).
    for (std::vector<MapObjectDefinition *>::reverse_iterator iter = rbegin(), iter_end = rend(); iter != iter_end;
         iter++)
    {
        MapObjectDefinition *m = *iter;

        if (m->playernum_ == playernum)
            return m;
    }

    FatalError("Missing DDF entry for player number %d\n", playernum);
    return nullptr; /* NOT REACHED */
}

const MapObjectDefinition *MapObjectDefinitionContainer::LookupDoorKey(int theKey)
{
    // Find a key thing (needed by automap code).

    /*
        //run through the key flags to get the benefit name

        std::string KeyName;
        KeyName.clear();

        for (int k = 0; keytype_names[k].name; k++)
        {
            if (keytype_names[k].flags == theKey)
            {
                std::string temp_ref = epi::StringFormat("%s",
       keytype_names[k].name); KeyName = temp_ref; break;
            }
        }
    */

    for (std::vector<MapObjectDefinition *>::reverse_iterator iter = rbegin(), iter_end = rend(); iter != iter_end;
         iter++)
    {
        MapObjectDefinition *m = *iter;

        Benefit *list;
        list = m->pickup_benefits_;
        for (; list != nullptr; list = list->next)
        {
            if (list->type == kBenefitTypeKey)
            {
                if (list->sub.type == theKey)
                {
                    return m;
                }
            }
        }
    }

    LogWarning("Missing DDF entry for key %d\n", theKey);
    return nullptr;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
