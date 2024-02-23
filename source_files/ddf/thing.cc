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
// -ACB- 1998/09/12 Use DDF_MainGetFixed for fixed number references.
// -ACB- 1998/09/13 Use DDF_MainGetTime for Time count references.
// -KM- 1998/11/25 Translucency is now a fixed_t. Fixed spelling of available.
// -KM- 1998/12/16 No limit on number of ammo types.
//

#include "thing.h"

#include <string.h>

#include "local.h"
#include "p_action.h"
#include "str_compare.h"
#include "str_util.h"
#include "types.h"

#undef DF
#define DF DDF_FIELD

const char *TemplateThing = nullptr;  // Lobo 2022: TEMPLATE inheritance fix

MapObjectDefinitionContainer mobjtypes;

static MapObjectDefinition *default_mobjtype;

void DDF_MobjGetSpecial(const char *info);
void DDF_MobjGetBenefit(const char *info, void *storage);
void DDF_MobjGetPickupEffect(const char *info, void *storage);
void DDF_MobjGetDLight(const char *info, void *storage);

static void DDF_MobjGetGlowType(const char *info, void *storage);
static void DDF_MobjGetYAlign(const char *info, void *storage);
static void DDF_MobjGetPercentRange(const char *info, void *storage);
static void DDF_MobjGetAngleRange(const char *info, void *storage);
static void DDF_MobjStateGetRADTrigger(const char *arg, State *cur_state);

static void AddPickupEffect(PickupEffect **list, PickupEffect *cur);

static int dlight_radius_warnings = 0;

#undef DDF_CMD_BASE
#define DDF_CMD_BASE dummy_dlight
static DynamicLightDefinition dummy_dlight;

const DDFCommandList dlight_commands[] = {
    DF("TYPE", type_, DDF_MobjGetDLight),
    DF("GRAPHIC", shape_, DDF_MainGetString),
    DF("RADIUS", radius_, DDF_MainGetFloat),
    DF("COLOUR", colour_, DDF_MainGetRGB),
    DF("HEIGHT", height_, DDF_MainGetPercent),
    DF("LEAKY", leaky_, DDF_MainGetBoolean),

    // backwards compatibility
    DF("INTENSITY", radius_, DDF_MainGetFloat),

    DDF_CMD_END};

#undef DDF_CMD_BASE
#define DDF_CMD_BASE dummy_weakness
static WeaknessDefinition dummy_weakness;

const DDFCommandList weakness_commands[] = {
    DF("CLASS", classes_, DDF_MainGetBitSet),
    DF("HEIGHTS", height_, DDF_MobjGetPercentRange),
    DF("ANGLES", angle_, DDF_MobjGetAngleRange),
    DF("MULTIPLY", multiply_, DDF_MainGetFloat),
    DF("PAINCHANCE", painchance_, DDF_MainGetPercent),

    DDF_CMD_END};

MapObjectDefinition *dynamic_mobj;

#undef DDF_CMD_BASE
#define DDF_CMD_BASE dummy_mobj
static MapObjectDefinition dummy_mobj;

const DDFCommandList thing_commands[] = {
    // sub-commands
    DDF_SUB_LIST("DLIGHT", dlight_[0], dlight_commands),
    DDF_SUB_LIST("DLIGHT2", dlight_[1], dlight_commands),
    DDF_SUB_LIST("WEAKNESS", weak_, weakness_commands),
    DDF_SUB_LIST("EXPLODE_DAMAGE", explode_damage_, damage_commands),
    DDF_SUB_LIST("CHOKE_DAMAGE", choke_damage_, damage_commands),

    DF("SPAWNHEALTH", spawnhealth_, DDF_MainGetFloat),
    DF("RADIUS", radius_, DDF_MainGetFloat),
    DF("HEIGHT", height_, DDF_MainGetFloat),
    DF("MASS", mass_, DDF_MainGetFloat), DF("SPEED", speed_, DDF_MainGetFloat),
    DF("FAST", fast_, DDF_MainGetFloat),
    DF("EXTRA", extendedflags_, DDF_MobjGetExtra),
    DF("RESPAWN_TIME", respawntime_, DDF_MainGetTime),
    DF("FUSE", fuse_, DDF_MainGetTime), DF("LIFESPAN", fuse_, DDF_MainGetTime),
    DF("PALETTE_REMAP", palremap_, DDF_MainGetColourmap),
    DF("TRANSLUCENCY", translucency_, DDF_MainGetPercent),

    DF("INITIAL_BENEFIT", initial_benefits_, DDF_MobjGetBenefit),
    DF("LOSE_BENEFIT", lose_benefits_, DDF_MobjGetBenefit),
    DF("PICKUP_BENEFIT", pickup_benefits_, DDF_MobjGetBenefit),
    DF("KILL_BENEFIT", kill_benefits_, DDF_MobjGetBenefit),
    DF("PICKUP_MESSAGE", pickup_message_, DDF_MainGetString),
    DF("PICKUP_EFFECT", pickup_effects_, DDF_MobjGetPickupEffect),

    DF("PAINCHANCE", painchance_, DDF_MainGetPercent),
    DF("MINATTACK_CHANCE", minatkchance_, DDF_MainGetPercent),
    DF("REACTION_TIME", reactiontime_, DDF_MainGetTime),
    DF("JUMP_DELAY", jump_delay_, DDF_MainGetTime),
    DF("JUMP_HEIGHT", jumpheight_, DDF_MainGetFloat),
    DF("CROUCH_HEIGHT", crouchheight_, DDF_MainGetFloat),
    DF("VIEW_HEIGHT", viewheight_, DDF_MainGetPercent),
    DF("SHOT_HEIGHT", shotheight_, DDF_MainGetPercent),
    DF("MAX_FALL", maxfall_, DDF_MainGetFloat),
    DF("CASTORDER", castorder_, DDF_MainGetNumeric),
    DF("CAST_TITLE", cast_title_, DDF_MainGetString),
    DF("PLAYER", playernum_, DDF_MobjGetPlayer),
    DF("SIDE", side_, DDF_MainGetBitSet),
    DF("CLOSE_ATTACK", closecombat_, DDF_MainRefAttack),
    DF("RANGE_ATTACK", rangeattack_, DDF_MainRefAttack),
    DF("SPARE_ATTACK", spareattack_, DDF_MainRefAttack),
    DF("DROPITEM", dropitem_ref_, DDF_MainGetString),
    DF("BLOOD", blood_ref_, DDF_MainGetString),
    DF("RESPAWN_EFFECT", respawneffect_ref_, DDF_MainGetString),
    DF("SPIT_SPOT", spitspot_ref_, DDF_MainGetString),

    DF("PICKUP_SOUND", activesound_, DDF_MainLookupSound),
    DF("ACTIVE_SOUND", activesound_, DDF_MainLookupSound),
    DF("LAUNCH_SOUND", seesound_, DDF_MainLookupSound),
    DF("AMBIENT_SOUND", seesound_, DDF_MainLookupSound),
    DF("SIGHTING_SOUND", seesound_, DDF_MainLookupSound),
    DF("DEATH_SOUND", deathsound_, DDF_MainLookupSound),
    DF("OVERKILL_SOUND", overkill_sound_, DDF_MainLookupSound),
    DF("PAIN_SOUND", painsound_, DDF_MainLookupSound),
    DF("STARTCOMBAT_SOUND", attacksound_, DDF_MainLookupSound),
    DF("WALK_SOUND", walksound_, DDF_MainLookupSound),
    DF("JUMP_SOUND", jump_sound_, DDF_MainLookupSound),
    DF("NOWAY_SOUND", noway_sound_, DDF_MainLookupSound),
    DF("OOF_SOUND", oof_sound_, DDF_MainLookupSound),
    DF("FALLPAIN_SOUND", fallpain_sound_, DDF_MainLookupSound),
    DF("GASP_SOUND", gasp_sound_, DDF_MainLookupSound),
    DF("SECRET_SOUND", secretsound_, DDF_MainLookupSound),
    DF("FALLING_SOUND", falling_sound_, DDF_MainLookupSound),
    DF("RIP_SOUND", rip_sound_, DDF_MainLookupSound),

    DF("FLOAT_SPEED", float_speed_, DDF_MainGetFloat),
    DF("STEP_SIZE", step_size_, DDF_MainGetFloat),
    DF("SPRITE_SCALE", scale_, DDF_MainGetFloat),
    DF("SPRITE_ASPECT", aspect_, DDF_MainGetFloat),
    DF("SPRITE_YALIGN", yalign_, DDF_MobjGetYAlign),    // -AJA- 2007/08/08
    DF("MODEL_SKIN", model_skin_, DDF_MainGetNumeric),  // -AJA- 2007/10/16
    DF("MODEL_SCALE", model_scale_, DDF_MainGetFloat),
    DF("MODEL_ASPECT", model_aspect_, DDF_MainGetFloat),
    DF("MODEL_BIAS", model_bias_, DDF_MainGetFloat),
    DF("MODEL_ROTATE", model_rotate_, DDF_MainGetNumeric),
    DF("BOUNCE_SPEED", bounce_speed_, DDF_MainGetFloat),
    DF("BOUNCE_UP", bounce_up_, DDF_MainGetFloat),
    DF("SIGHT_SLOPE", sight_slope_, DDF_MainGetSlope),
    DF("SIGHT_ANGLE", sight_angle_, DDF_MainGetAngle),
    DF("RIDE_FRICTION", ride_friction_, DDF_MainGetFloat),
    DF("BOBBING", bobbing_, DDF_MainGetPercent),
    DF("IMMUNITY_CLASS", immunity_, DDF_MainGetBitSet),
    DF("RESISTANCE_CLASS", resistance_, DDF_MainGetBitSet),
    DF("RESISTANCE_MULTIPLY", resist_multiply_, DDF_MainGetFloat),
    DF("RESISTANCE_PAINCHANCE", resist_painchance_, DDF_MainGetPercent),
    DF("GHOST_CLASS", ghost_, DDF_MainGetBitSet),  // -AJA- 2005/05/15
    DF("SHADOW_TRANSLUCENCY", shadow_trans_, DDF_MainGetPercent),
    DF("LUNG_CAPACITY", lung_capacity_, DDF_MainGetTime),
    DF("GASP_START", gasp_start_, DDF_MainGetTime),
    DF("EXPLODE_RADIUS", explode_radius_, DDF_MainGetFloat),
    DF("RELOAD_SHOTS", reload_shots_, DDF_MainGetNumeric),  // -AJA- 2004/11/15
    DF("GLOW_TYPE", glow_type_, DDF_MobjGetGlowType),       // -AJA- 2007/08/19
    DF("ARMOUR_PROTECTION", armour_protect_,
       DDF_MainGetPercent),  // -AJA- 2007/08/22
    DF("ARMOUR_DEPLETION", armour_deplete_,
       DDF_MainGetPercentAny),                             // -AJA- 2007/08/22
    DF("ARMOUR_CLASS", armour_class_, DDF_MainGetBitSet),  // -AJA- 2007/08/22

    DF("SIGHT_DISTANCE", sight_distance_, DDF_MainGetFloat),  // Lobo 2022
    DF("HEAR_DISTANCE", hear_distance_, DDF_MainGetFloat),    // Lobo 2022

    DF("MORPH_TIMEOUT", morphtimeout_, DDF_MainGetTime),  // Lobo 2023

    // DEHEXTRA
    DF("GIB_HEALTH", gib_health_, DDF_MainGetFloat),

    DF("INFIGHTING_GROUP", infight_group_, DDF_MainGetNumeric),
    DF("PROJECTILE_GROUP", proj_group_, DDF_MainGetNumeric),
    DF("SPLASH_GROUP", splash_group_, DDF_MainGetNumeric),
    DF("FAST_SPEED", fast_speed_, DDF_MainGetNumeric),
    DF("MELEE_RANGE", melee_range_, DDF_MainGetNumeric),

    // -AJA- backwards compatibility cruft...
    DF("EXPLOD_DAMAGE", explode_damage_.nominal_, DDF_MainGetFloat),
    DF("EXPLOSION_DAMAGE", explode_damage_.nominal_, DDF_MainGetFloat),
    DF("EXPLOD_DAMAGERANGE", explode_damage_.nominal_, DDF_MainGetFloat),

    DDF_CMD_END};

const DDFStateStarter thing_starters[] = {
    DDF_STATE("SPAWN", "IDLE", spawn_state_),
    DDF_STATE("IDLE", "IDLE", idle_state_),
    DDF_STATE("CHASE", "CHASE", chase_state_),
    DDF_STATE("PAIN", "IDLE", pain_state_),
    DDF_STATE("MISSILE", "IDLE", missile_state_),
    DDF_STATE("MELEE", "IDLE", melee_state_),
    DDF_STATE("DEATH", "REMOVE", death_state_),
    DDF_STATE("OVERKILL", "REMOVE", overkill_state_),
    DDF_STATE("RESPAWN", "IDLE", raise_state_),
    DDF_STATE("RESURRECT", "IDLE", res_state_),
    DDF_STATE("MEANDER", "MEANDER", meander_state_),
    DDF_STATE("MORPH", "MORPH", morph_state_),
    DDF_STATE("BOUNCE", "IDLE", bounce_state_),
    DDF_STATE("TOUCH", "IDLE", touch_state_),
    DDF_STATE("RELOAD", "IDLE", reload_state_),
    DDF_STATE("GIB", "REMOVE", gib_state_),

    DDF_STATE_END};

// -KM- 1998/11/25 Added weapon functions.
// -AJA- 1999/08/09: Moved this here from p_action.h, and added an extra
// field `handle_arg' for things like "WEAPON_SHOOT(FIREBALL)".

const DDFActionCode thing_actions[] = {
    {"NOTHING", nullptr, nullptr},

    {"CLOSEATTEMPTSND", P_ActMakeCloseAttemptSound, nullptr},
    {"COMBOATTACK", P_ActComboAttack, nullptr},
    {"FACETARGET", P_ActFaceTarget, nullptr},
    {"PLAYSOUND", P_ActPlaySound, DDF_StateGetSound},
    {"PLAYSOUND_BOSS", P_ActPlaySoundBoss, DDF_StateGetSound},
    {"KILLSOUND", P_ActKillSound, nullptr},
    {"MAKESOUND", P_ActMakeAmbientSound, nullptr},
    {"MAKEACTIVESOUND", P_ActMakeActiveSound, nullptr},
    {"MAKESOUNDRANDOM", P_ActMakeAmbientSoundRandom, nullptr},
    {"MAKEDEATHSOUND", P_ActMakeDyingSound, nullptr},
    {"MAKEDEAD", P_ActMakeIntoCorpse, nullptr},
    {"MAKEOVERKILLSOUND", P_ActMakeOverKillSound, nullptr},
    {"MAKEPAINSOUND", P_ActMakePainSound, nullptr},
    {"PLAYER_SCREAM", P_ActPlayerScream, nullptr},
    {"CLOSE_ATTACK", P_ActMeleeAttack, DDF_StateGetAttack},
    {"RANGE_ATTACK", P_ActRangeAttack, DDF_StateGetAttack},
    {"SPARE_ATTACK", P_ActSpareAttack, DDF_StateGetAttack},

    {"RANGEATTEMPTSND", P_ActMakeRangeAttemptSound, nullptr},
    {"REFIRE_CHECK", P_ActRefireCheck, nullptr},
    {"RELOAD_CHECK", P_ActReloadCheck, nullptr},
    {"RELOAD_RESET", P_ActReloadReset, nullptr},
    {"LOOKOUT", P_ActStandardLook, nullptr},
    {"SUPPORT_LOOKOUT", P_ActPlayerSupportLook, nullptr},
    {"CHASE", P_ActStandardChase, nullptr},
    {"RESCHASE", P_ActResurrectChase, nullptr},
    {"WALKSOUND_CHASE", P_ActWalkSoundChase, nullptr},
    {"MEANDER", P_ActStandardMeander, nullptr},
    {"SUPPORT_MEANDER", P_ActPlayerSupportMeander, nullptr},
    {"EXPLOSIONDAMAGE", P_ActDamageExplosion, nullptr},
    {"THRUST", P_ActThrust, nullptr},
    {"TRACER", P_ActHomingProjectile, nullptr},
    {"RANDOM_TRACER", P_ActHomingProjectile, nullptr},  // same as above
    {"RESET_SPREADER", P_ActResetSpreadCount, nullptr},
    {"SMOKING", P_ActCreateSmokeTrail, nullptr},
    {"TRACKERACTIVE", P_ActTrackerActive, nullptr},
    {"TRACKERFOLLOW", P_ActTrackerFollow, nullptr},
    {"TRACKERSTART", P_ActTrackerStart, nullptr},
    {"EFFECTTRACKER", P_ActEffectTracker, nullptr},
    {"CHECKBLOOD", P_ActCheckBlood, nullptr},
    {"CHECKMOVING", P_ActCheckMoving, nullptr},
    {"CHECK_ACTIVITY", P_ActCheckActivity, nullptr},
    {"JUMP", P_ActJump, DDF_StateGetJump},
    {"JUMP_LIQUID", P_ActJumpLiquid, DDF_StateGetJump},
    {"JUMP_SKY", P_ActJumpSky, DDF_StateGetJump},
    //{"JUMP_STUCK",        P_ActJumpStuck, DDF_StateGetJump},
    {"BECOME", P_ActBecome, DDF_StateGetBecome},
    {"UNBECOME", P_ActUnBecome, nullptr},
    {"MORPH", P_ActMorph,
     DDF_StateGetMorph},                 // same as BECOME but resets health
    {"UNMORPH", P_ActUnMorph, nullptr},  // same as UNBECOME but resets health

    {"EXPLODE", P_ActExplode, nullptr},
    {"ACTIVATE_LINETYPE", P_ActActivateLineType, DDF_StateGetIntPair},
    {"RTS_ENABLE_TAGGED", P_ActEnableRadTrig, DDF_MobjStateGetRADTrigger},
    {"RTS_DISABLE_TAGGED", P_ActDisableRadTrig, DDF_MobjStateGetRADTrigger},
    {"TOUCHY_REARM", P_ActTouchyRearm, nullptr},
    {"TOUCHY_DISARM", P_ActTouchyDisarm, nullptr},
    {"BOUNCE_REARM", P_ActBounceRearm, nullptr},
    {"BOUNCE_DISARM", P_ActBounceDisarm, nullptr},
    {"PATH_CHECK", P_ActPathCheck, nullptr},
    {"PATH_FOLLOW", P_ActPathFollow, nullptr},
    {"SET_INVULNERABLE", P_ActSetInvuln, nullptr},
    {"CLEAR_INVULNERABLE", P_ActClearInvuln, nullptr},
    {"SET_PAINCHANCE", P_ActPainChanceSet, DDF_StateGetPercent},

    {"DROPITEM", P_ActDropItem, DDF_StateGetMobj},
    {"SPAWN", P_ActSpawn, DDF_StateGetMobj},
    {"TRANS_SET", P_ActTransSet, DDF_StateGetPercent},
    {"TRANS_FADE", P_ActTransFade, DDF_StateGetPercent},
    {"TRANS_MORE", P_ActTransMore, DDF_StateGetPercent},
    {"TRANS_LESS", P_ActTransLess, DDF_StateGetPercent},
    {"TRANS_ALTERNATE", P_ActTransAlternate, DDF_StateGetPercent},
    {"DLIGHT_SET", P_ActDLightSet, DDF_StateGetInteger},
    {"DLIGHT_FADE", P_ActDLightFade, DDF_StateGetInteger},
    {"DLIGHT_RANDOM", P_ActDLightRandom, DDF_StateGetIntPair},
    {"DLIGHT_COLOUR", P_ActDLightColour, DDF_StateGetRGB},
    {"SET_SKIN", P_ActSetSkin, DDF_StateGetInteger},

    {"FACE", P_ActFaceDir, DDF_StateGetAngle},
    {"TURN", P_ActTurnDir, DDF_StateGetAngle},
    {"TURN_RANDOM", P_ActTurnRandom, DDF_StateGetAngle},
    {"MLOOK_FACE", P_ActMlookFace, DDF_StateGetSlope},
    {"MLOOK_TURN", P_ActMlookTurn, DDF_StateGetSlope},
    {"MOVE_FWD", P_ActMoveFwd, DDF_StateGetFloat},
    {"MOVE_RIGHT", P_ActMoveRight, DDF_StateGetFloat},
    {"MOVE_UP", P_ActMoveUp, DDF_StateGetFloat},
    {"STOP", P_ActStopMoving, nullptr},

    // Boom/MBF compatibility
    {"DIE", P_ActDie, nullptr},
    {"KEEN_DIE", P_ActKeenDie, nullptr},
    {"MUSHROOM", P_ActMushroom, nullptr},
    {"NOISE_ALERT", P_ActNoiseAlert, nullptr},

    // bossbrain actions
    {"BRAINSPIT", P_ActBrainSpit, nullptr},
    {"CUBESPAWN", P_ActCubeSpawn, nullptr},
    {"CUBETRACER", P_ActHomeToSpot, nullptr},
    {"BRAINSCREAM", P_ActBrainScream, nullptr},
    {"BRAINMISSILEEXPLODE", P_ActBrainMissileExplode, nullptr},
    {"BRAINDIE", P_ActBrainDie, nullptr},

    // -AJA- backwards compatibility cruft...
    {"VARIEDEXPDAMAGE", P_ActDamageExplosion, nullptr},
    {"VARIED_THRUST", P_ActThrust, nullptr},

    {nullptr, nullptr, nullptr}};

const DDFSpecialFlags keytype_names[] = {
    {"BLUECARD", kDoorKeyBlueCard, 0},
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
    {"GREEN_ARMOUR", kArmourTypeGreen, 0},
    {"BLUE_ARMOUR", kArmourTypeBlue, 0},
    {"PURPLE_ARMOUR", kArmourTypePurple, 0},
    {"YELLOW_ARMOUR", kArmourTypeYellow, 0},
    {"RED_ARMOUR", kArmourTypeRed, 0},
    {nullptr, 0, 0}};

const DDFSpecialFlags powertype_names[] = {
    {"POWERUP_INVULNERABLE", kPowerTypeInvulnerable, 0},
    {"POWERUP_BARE_BERSERK", kPowerTypeBerserk, 0},
    {"POWERUP_BERSERK", kPowerTypeBerserk, 0},
    {"POWERUP_PARTINVIS", kPowerTypePartInvis, 0},
    {"POWERUP_ACIDSUIT", kPowerTypeAcidSuit, 0},
    {"POWERUP_AUTOMAP", kPowerTypeAllMap, 0},
    {"POWERUP_LIGHTGOGGLES", kPowerTypeInfrared, 0},
    {"POWERUP_JETPACK", kPowerTypeJetpack, 0},
    {"POWERUP_NIGHTVISION", kPowerTypeNightVision, 0},
    {"POWERUP_SCUBA", kPowerTypeScuba, 0},
    {"POWERUP_TIMESTOP", kPowerTypeTimeStop, 0},
    {nullptr, 0, 0}};

const DDFSpecialFlags simplecond_names[] = {
    {"JUMPING", kConditionCheckTypeJumping, 0},
    {"CROUCHING", kConditionCheckTypeCrouching, 0},
    {"SWIMMING", kConditionCheckTypeSwimming, 0},
    {"ATTACKING", kConditionCheckTypeAttacking, 0},
    {"RAMPAGING", kConditionCheckTypeRampaging, 0},
    {"USING", kConditionCheckTypeUsing, 0},
    {"ACTION1", kConditionCheckTypeAction1, 0},
    {"ACTION2", kConditionCheckTypeAction2, 0},
    {"WALKING", kConditionCheckTypeWalking, 0},
    {nullptr, 0, 0}};

const DDFSpecialFlags inv_types[] = {
    {"INVENTORY01", kInventoryType01, 0}, {"INVENTORY02", kInventoryType02, 0},
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

const DDFSpecialFlags counter_types[] = {
    {"LIVES", kCounterTypeLives, 0},
    {"SCORE", kCounterTypeScore, 0},
    {"MONEY", kCounterTypeMoney, 0},
    {"EXPERIENCE", kCounterTypeExperience, 0},
    {"COUNTER01", kCounterTypeLives, 0},
    {"COUNTER02", kCounterTypeScore, 0},
    {"COUNTER03", kCounterTypeMoney, 0},
    {"COUNTER04", kCounterTypeExperience, 0},
    {"COUNTER05", kCounterType05, 0},
    {"COUNTER06", kCounterType06, 0},
    {"COUNTER07", kCounterType07, 0},
    {"COUNTER08", kCounterType08, 0},
    {"COUNTER09", kCounterType09, 0},
    {"COUNTER10", kCounterType10, 0},
    {"COUNTER11", kCounterType11, 0},
    {"COUNTER12", kCounterType12, 0},
    {"COUNTER13", kCounterType13, 0},
    {"COUNTER14", kCounterType14, 0},
    {"COUNTER15", kCounterType15, 0},
    {"COUNTER16", kCounterType16, 0},
    {"COUNTER17", kCounterType17, 0},
    {"COUNTER18", kCounterType18, 0},
    {"COUNTER19", kCounterType19, 0},
    {"COUNTER20", kCounterType20, 0},
    {"COUNTER21", kCounterType21, 0},
    {"COUNTER22", kCounterType22, 0},
    {"COUNTER23", kCounterType23, 0},
    {"COUNTER24", kCounterType24, 0},
    {"COUNTER25", kCounterType25, 0},
    {"COUNTER26", kCounterType26, 0},
    {"COUNTER27", kCounterType27, 0},
    {"COUNTER28", kCounterType28, 0},
    {"COUNTER29", kCounterType29, 0},
    {"COUNTER30", kCounterType30, 0},
    {"COUNTER31", kCounterType31, 0},
    {"COUNTER32", kCounterType32, 0},
    {"COUNTER33", kCounterType33, 0},
    {"COUNTER34", kCounterType34, 0},
    {"COUNTER35", kCounterType35, 0},
    {"COUNTER36", kCounterType36, 0},
    {"COUNTER37", kCounterType37, 0},
    {"COUNTER38", kCounterType38, 0},
    {"COUNTER39", kCounterType39, 0},
    {"COUNTER40", kCounterType40, 0},
    {"COUNTER41", kCounterType41, 0},
    {"COUNTER42", kCounterType42, 0},
    {"COUNTER43", kCounterType43, 0},
    {"COUNTER44", kCounterType44, 0},
    {"COUNTER45", kCounterType45, 0},
    {"COUNTER46", kCounterType46, 0},
    {"COUNTER47", kCounterType47, 0},
    {"COUNTER48", kCounterType48, 0},
    {"COUNTER49", kCounterType49, 0},
    {"COUNTER50", kCounterType50, 0},
    {"COUNTER51", kCounterType51, 0},
    {"COUNTER52", kCounterType52, 0},
    {"COUNTER53", kCounterType53, 0},
    {"COUNTER54", kCounterType54, 0},
    {"COUNTER55", kCounterType55, 0},
    {"COUNTER56", kCounterType56, 0},
    {"COUNTER57", kCounterType57, 0},
    {"COUNTER58", kCounterType58, 0},
    {"COUNTER59", kCounterType59, 0},
    {"COUNTER60", kCounterType60, 0},
    {"COUNTER61", kCounterType61, 0},
    {"COUNTER62", kCounterType62, 0},
    {"COUNTER63", kCounterType63, 0},
    {"COUNTER64", kCounterType64, 0},
    {"COUNTER65", kCounterType65, 0},
    {"COUNTER66", kCounterType66, 0},
    {"COUNTER67", kCounterType67, 0},
    {"COUNTER68", kCounterType68, 0},
    {"COUNTER69", kCounterType69, 0},
    {"COUNTER70", kCounterType70, 0},
    {"COUNTER71", kCounterType71, 0},
    {"COUNTER72", kCounterType72, 0},
    {"COUNTER73", kCounterType73, 0},
    {"COUNTER74", kCounterType74, 0},
    {"COUNTER75", kCounterType75, 0},
    {"COUNTER76", kCounterType76, 0},
    {"COUNTER77", kCounterType77, 0},
    {"COUNTER78", kCounterType78, 0},
    {"COUNTER79", kCounterType79, 0},
    {"COUNTER80", kCounterType80, 0},
    {"COUNTER81", kCounterType81, 0},
    {"COUNTER82", kCounterType82, 0},
    {"COUNTER83", kCounterType83, 0},
    {"COUNTER84", kCounterType84, 0},
    {"COUNTER85", kCounterType85, 0},
    {"COUNTER86", kCounterType86, 0},
    {"COUNTER87", kCounterType87, 0},
    {"COUNTER88", kCounterType88, 0},
    {"COUNTER89", kCounterType89, 0},
    {"COUNTER90", kCounterType90, 0},
    {"COUNTER91", kCounterType91, 0},
    {"COUNTER92", kCounterType92, 0},
    {"COUNTER93", kCounterType93, 0},
    {"COUNTER94", kCounterType94, 0},
    {"COUNTER95", kCounterType95, 0},
    {"COUNTER96", kCounterType96, 0},
    {"COUNTER97", kCounterType97, 0},
    {"COUNTER98", kCounterType98, 0},
    {"COUNTER99", kCounterType99, 0},
    {nullptr, 0, 0}};

//
// DDF_CompareName
//
// Compare two names. This is like stricmp(), except that spaces
// and underscors are ignored for comparison purposes.
//
// -AJA- 1999/09/11: written.
//
int DDF_CompareName(const char *A, const char *B)
{
    for (;;)
    {
        // Note: must skip stuff BEFORE checking for NUL
        while (*A == ' ' || *A == '_') A++;
        while (*B == ' ' || *B == '_') B++;

        if (*A == 0 && *B == 0) return 0;

        if (*A == 0) return -1;
        if (*B == 0) return +1;

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
        DDF_WarnError("New thing entry is missing a name!");
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
            DDF_WarnError("New thing entry is missing a name!");
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
            DDF_Error("Unknown thing to extend: %s\n", name.c_str());

        if (number > 0) dynamic_mobj->number_ = number;

        DDF_StateBeginRange(dynamic_mobj->state_grp_);
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

    DDF_StateBeginRange(dynamic_mobj->state_grp_);
}

static void ThingDoTemplate(const char *contents)
{
    int idx = mobjtypes.FindFirst(contents, 0);
    if (idx < 0) DDF_Error("Unknown thing template: '%s'\n", contents);

    MapObjectDefinition *other = mobjtypes[idx];
    SYS_ASSERT(other);

    if (other == dynamic_mobj)
        DDF_Error("Bad thing template: '%s'\n", contents);

    dynamic_mobj->CopyDetail(*other);

    TemplateThing = other->name_.c_str();

    DDF_StateBeginRange(dynamic_mobj->state_grp_);
}

void ThingParseField(const char *field, const char *contents, int index,
                     bool is_last)
{
#if (DEBUG_DDF)
    EDGEDebugf("THING_PARSE: %s = %s;\n", field, contents);
#endif

    if (DDF_CompareName(field, "TEMPLATE") == 0)
    {
        ThingDoTemplate(contents);
        return;
    }

    // -AJA- this needs special handling (it touches several fields)
    if (DDF_CompareName(field, "SPECIAL") == 0 ||
        DDF_CompareName(field, "PROJECTILE_SPECIAL") == 0)
    {
        DDF_MobjGetSpecial(contents);
        return;
    }

    // handle the "MODEL_ROTATE" command
    if (DDF_CompareName(field, "MODEL_ROTATE") == 0)
    {
        if (DDF_MainParseField(thing_commands, field, contents,
                               (uint8_t *)dynamic_mobj))
        {
            dynamic_mobj->model_rotate_ *= kBAMAngle1;  // apply the rotation
            return;
        }
    }

    if (DDF_MainParseField(thing_commands, field, contents,
                           (uint8_t *)dynamic_mobj))
        return;

    if (DDF_MainParseState((uint8_t *)dynamic_mobj, dynamic_mobj->state_grp_,
                           field, contents, index, is_last,
                           false /* is_weapon */, thing_starters,
                           thing_actions))
        return;

    DDF_WarnError("Unknown thing/attack command: %s\n", field);
}

static void ThingFinishEntry(void)
{
    DDF_StateFinishRange(dynamic_mobj->state_grp_);

    // count-as-kill things are automatically monsters
    if (dynamic_mobj->flags_ & kMapObjectFlagCountKill)
        dynamic_mobj->extendedflags_ |= kExtendedFlagMonster;

    // countable items are always pick-up-able
    if (dynamic_mobj->flags_ & kMapObjectFlagCountItem)
        dynamic_mobj->hyperflags_ |= kHyperFlagForcePickup;

    // shootable things are always pushable
    if (dynamic_mobj->flags_ & kMapObjectFlagShootable)
        dynamic_mobj->hyperflags_ |= kHyperFlagPushable;

    // check stuff...

    if (dynamic_mobj->mass_ < 1)
    {
        DDF_WarnError("Bad MASS value %f in DDF.\n", dynamic_mobj->mass_);
        dynamic_mobj->mass_ = 1;
    }

    // check CAST stuff
    if (dynamic_mobj->castorder_ > 0)
    {
        if (!dynamic_mobj->chase_state_)
            DDF_Error("Cast object must have CHASE states !\n");

        if (!dynamic_mobj->death_state_)
            DDF_Error("Cast object must have DEATH states !\n");
    }

    // check DAMAGE stuff
    if (dynamic_mobj->explode_damage_.nominal_ < 0)
    {
        DDF_WarnError("Bad EXPLODE_DAMAGE.VAL value %f in DDF.\n",
                      dynamic_mobj->explode_damage_.nominal_);
    }

    if (dynamic_mobj->explode_radius_ < 0)
    {
        DDF_Error("Bad EXPLODE_RADIUS value %f in DDF.\n",
                  dynamic_mobj->explode_radius_);
    }

    if (dynamic_mobj->reload_shots_ <= 0)
    {
        DDF_Error("Bad RELOAD_SHOTS value %d in DDF.\n",
                  dynamic_mobj->reload_shots_);
    }

    if (dynamic_mobj->choke_damage_.nominal_ < 0)
    {
        DDF_WarnError("Bad CHOKE_DAMAGE.VAL value %f in DDF.\n",
                      dynamic_mobj->choke_damage_.nominal_);
    }

    if (dynamic_mobj->model_skin_ < 0 || dynamic_mobj->model_skin_ > 9)
        DDF_Error("Bad MODEL_SKIN value %d in DDF (must be 0-9).\n",
                  dynamic_mobj->model_skin_);

    if (dynamic_mobj->dlight_[0].radius_ > 512)
    {
        if (dlight_radius_warnings < 3)
            DDF_Warning("DLIGHT_RADIUS value %1.1f too large (over 512).\n",
                        dynamic_mobj->dlight_[0].radius_);
        else if (dlight_radius_warnings == 3)
            EDGEWarning("More too large DLIGHT_RADIUS values found....\n");

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
        if (idx < 0) DDF_Error("Unknown thing template: \n");

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
    EDGEWarning("Ignoring #CLEARALL in things.ddf\n");
}

void DDF_ReadThings(const std::string &data)
{
    DDFReadInfo things;

    things.tag      = "THINGS";
    things.lumpname = "DDFTHING";

    things.start_entry  = ThingStartEntry;
    things.parse_field  = ThingParseField;
    things.finish_entry = ThingFinishEntry;
    things.clear_all    = ThingClearAll;

    DDF_MainReadFile(&things, data);
}

void DDF_MobjInit(void)
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

void DDF_MobjCleanUp(void)
{
    // lookup references
    for (MapObjectDefinition *m : mobjtypes)
    {
        cur_ddf_entryname =
            epi::StringFormat("[%s]  (things.ddf)", m->name_.c_str());

        m->dropitem_ = m->dropitem_ref_ != ""
                           ? mobjtypes.Lookup(m->dropitem_ref_.c_str())
                           : nullptr;
        m->blood_    = m->blood_ref_ != ""
                           ? mobjtypes.Lookup(m->blood_ref_.c_str())
                           : mobjtypes.Lookup("BLOOD");

        m->respawneffect_ =
            m->respawneffect_ref_ != ""
                ? mobjtypes.Lookup(m->respawneffect_ref_.c_str())
            : (m->flags_ & kMapObjectFlagSpecial)
                ? mobjtypes.Lookup("ITEM_RESPAWN")
                : mobjtypes.Lookup("RESPAWN_FLASH");

        m->spitspot_ = m->spitspot_ref_ != ""
                           ? mobjtypes.Lookup(m->spitspot_ref_.c_str())
                           : nullptr;

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
static int ParseBenefitString(const char *info, char *name, char *param,
                              float *value, float *limit)
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
                DDF_WarnError("Bad value in benefit string: %s\n", info);
                return -1;
        }
    }
    else if (pos)
    {
        DDF_WarnError("Malformed benefit string: %s\n", info);
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
    if (kDDFCheckFlagPositive != DDF_MainCheckSpecialFlag(name, counter_types,
                                                          &be->sub.type, false,
                                                          false))
    {
        return false;
    }

    be->type = kBenefitTypeCounter;

    if (num_vals < 1)
    {
        DDF_WarnError("Counter benefit used, but amount is missing.\n");
        return false;
    }

    if (num_vals < 2) { be->limit = be->amount; }

    return true;
}

static bool BenefitTryCounterLimit(const char *name, Benefit *be, int num_vals)
{
    char   namebuf[200];
    size_t len = strlen(name);

    // check for ".LIMIT" prefix
    if (len < 7 || DDF_CompareName(name + len - 6, ".LIMIT") != 0) return false;

    len -= 6;
    epi::CStringCopyMax(namebuf, name, len);

    if (kDDFCheckFlagPositive !=
        DDF_MainCheckSpecialFlag(namebuf, counter_types, &be->sub.type, false,
                                 false))
    {
        return false;
    }

    be->type  = kBenefitTypeCounterLimit;
    be->limit = 0;

    if (num_vals < 1)
    {
        DDF_WarnError("CounterLimit benefit used, but amount is missing.\n");
        return false;
    }

    if (num_vals > 1)
    {
        DDF_WarnError("CounterLimit benefit cannot have a limit value.\n");
        return false;
    }
    return true;
}

static bool BenefitTryInventory(const char *name, Benefit *be, int num_vals)
{
    if (kDDFCheckFlagPositive !=
        DDF_MainCheckSpecialFlag(name, inv_types, &be->sub.type, false, false))
    {
        return false;
    }

    be->type = kBenefitTypeInventory;

    if (num_vals < 1)
    {
        DDF_WarnError("Inventory benefit used, but amount is missing.\n");
        return false;
    }

    if (num_vals < 2) { be->limit = be->amount; }

    return true;
}

static bool BenefitTryInventoryLimit(const char *name, Benefit *be,
                                     int num_vals)
{
    char namebuf[200];
    int  len = strlen(name);

    // check for ".LIMIT" prefix
    if (len < 7 || DDF_CompareName(name + len - 6, ".LIMIT") != 0) return false;

    len -= 6;
    epi::CStringCopyMax(namebuf, name, len);

    if (kDDFCheckFlagPositive != DDF_MainCheckSpecialFlag(namebuf, inv_types,
                                                          &be->sub.type, false,
                                                          false))
    {
        return false;
    }

    be->type  = kBenefitTypeInventoryLimit;
    be->limit = 0;

    if (num_vals < 1)
    {
        DDF_WarnError("InventoryLimit benefit used, but amount is missing.\n");
        return false;
    }

    if (num_vals > 1)
    {
        DDF_WarnError("InventoryLimit benefit cannot have a limit value.\n");
        return false;
    }
    return true;
}

static bool BenefitTryAmmo(const char *name, Benefit *be, int num_vals)
{
    if (kDDFCheckFlagPositive !=
        DDF_MainCheckSpecialFlag(name, ammo_types, &be->sub.type, false, false))
    {
        return false;
    }

    be->type = kBenefitTypeAmmo;

    if ((AmmunitionType)be->sub.type == kAmmunitionTypeNoAmmo)
    {
        DDF_WarnError("Illegal ammo benefit: %s\n", name);
        return false;
    }

    if (num_vals < 1)
    {
        DDF_WarnError("Ammo benefit used, but amount is missing.\n");
        return false;
    }

    if (num_vals < 2) { be->limit = be->amount; }

    return true;
}

static bool BenefitTryAmmoLimit(const char *name, Benefit *be, int num_vals)
{
    char   namebuf[200];
    size_t len = strlen(name);

    // check for ".LIMIT" prefix

    if (len < 7 || DDF_CompareName(name + len - 6, ".LIMIT") != 0) return false;

    len -= 6;
    epi::CStringCopyMax(namebuf, name, len);

    if (kDDFCheckFlagPositive != DDF_MainCheckSpecialFlag(namebuf, ammo_types,
                                                          &be->sub.type, false,
                                                          false))
    {
        return false;
    }

    be->type  = kBenefitTypeAmmoLimit;
    be->limit = 0;

    if (be->sub.type == kAmmunitionTypeNoAmmo)
    {
        DDF_WarnError("Illegal ammolimit benefit: %s\n", name);
        return false;
    }

    if (num_vals < 1)
    {
        DDF_WarnError("AmmoLimit benefit used, but amount is missing.\n");
        return false;
    }

    if (num_vals > 1)
    {
        DDF_WarnError("AmmoLimit benefit cannot have a limit value.\n");
        return false;
    }

    return true;
}

static bool BenefitTryWeapon(const char *name, Benefit *be, int num_vals)
{
    int idx = weapondefs.FindFirst(name, 0);

    if (idx < 0) return false;

    be->sub.weap = weapondefs[idx];

    be->type  = kBenefitTypeWeapon;
    be->limit = 1.0f;

    if (num_vals < 1)
        be->amount = 1.0f;
    else if (be->amount != 0.0f && be->amount != 1.0f)
    {
        DDF_WarnError("Weapon benefit used, bad amount value: %1.1f\n",
                      be->amount);
        return false;
    }

    if (num_vals > 1)
    {
        DDF_WarnError("Weapon benefit cannot have a limit value.\n");
        return false;
    }

    return true;
}

static bool BenefitTryKey(const char *name, Benefit *be, int num_vals)
{
    if (kDDFCheckFlagPositive != DDF_MainCheckSpecialFlag(name, keytype_names,
                                                          &be->sub.type, false,
                                                          false))
    {
        return false;
    }

    be->type  = kBenefitTypeKey;
    be->limit = 1.0f;

    if (num_vals < 1)
        be->amount = 1.0f;
    else if (be->amount != 0.0f && be->amount != 1.0f)
    {
        DDF_WarnError("Key benefit used, bad amount value: %1.1f\n",
                      be->amount);
        return false;
    }

    if (num_vals > 1)
    {
        DDF_WarnError("Key benefit cannot have a limit value.\n");
        return false;
    }

    return true;
}

static bool BenefitTryHealth(const char *name, Benefit *be, int num_vals)
{
    if (DDF_CompareName(name, "HEALTH") != 0) return false;

    be->type     = kBenefitTypeHealth;
    be->sub.type = 0;

    if (num_vals < 1)
    {
        DDF_WarnError("Health benefit used, but amount is missing.\n");
        return false;
    }

    if (num_vals < 2) be->limit = 100.0f;

    return true;
}

static bool BenefitTryArmour(const char *name, Benefit *be, int num_vals)
{
    if (kDDFCheckFlagPositive !=
        DDF_MainCheckSpecialFlag(name, armourtype_names, &be->sub.type, false,
                                 false))
    {
        return false;
    }

    be->type = kBenefitTypeArmour;

    if (num_vals < 1)
    {
        DDF_WarnError("Armour benefit used, but amount is missing.\n");
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
    if (kDDFCheckFlagPositive != DDF_MainCheckSpecialFlag(name, powertype_names,
                                                          &be->sub.type, false,
                                                          false))
    {
        return false;
    }

    be->type = kBenefitTypePowerup;

    if (num_vals < 1) be->amount = 999999.0f;

    if (num_vals < 2) be->limit = 999999.0f;

    // -AJA- backwards compatibility (need Fist for Berserk)
    if (be->sub.type == kPowerTypeBerserk &&
        DDF_CompareName(name, "POWERUP_BERSERK") == 0)
    {
        int idx = weapondefs.FindFirst("FIST", 0);

        if (idx >= 0)
        {
            AddPickupEffect(&dynamic_mobj->pickup_effects_,
                            new PickupEffect(kPickupEffectTypeSwitchWeapon,
                                             weapondefs[idx], 0, 0));

            AddPickupEffect(&dynamic_mobj->pickup_effects_,
                            new PickupEffect(kPickupEffectTypeKeepPowerup,
                                             kPowerTypeBerserk, 0, 0));
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
        if (cur->type == kBenefitTypeWeapon) continue;

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

    for (tail = (*list); tail && tail->next; tail = tail->next) {}

    tail->next = cur;
}

//
// DDF_MobjGetBenefit
//
// Parse a single benefit and update the benefit list accordingly.  If
// the type/subtype are not in the list, add a new entry, otherwise
// just modify the existing entry.
//
void DDF_MobjGetBenefit(const char *info, void *storage)
{
    char namebuf[200];
    char parambuf[200];
    int  num_vals;

    Benefit temp;

    SYS_ASSERT(storage);

    num_vals =
        ParseBenefitString(info, namebuf, parambuf, &temp.amount, &temp.limit);

    // an error occurred ?
    if (num_vals < 0) return;

    if (BenefitTryAmmo(namebuf, &temp, num_vals) ||
        BenefitTryAmmoLimit(namebuf, &temp, num_vals) ||
        BenefitTryWeapon(namebuf, &temp, num_vals) ||
        BenefitTryKey(namebuf, &temp, num_vals) ||
        BenefitTryHealth(namebuf, &temp, num_vals) ||
        BenefitTryArmour(namebuf, &temp, num_vals) ||
        BenefitTryPowerup(namebuf, &temp, num_vals) ||
        BenefitTryInventory(namebuf, &temp, num_vals) ||
        BenefitTryInventoryLimit(namebuf, &temp, num_vals) ||
        BenefitTryCounter(namebuf, &temp, num_vals) ||
        BenefitTryCounterLimit(namebuf, &temp, num_vals))
    {
        BenefitAdd((Benefit **)storage, &temp);
        return;
    }

    DDF_WarnError("Unknown/Malformed benefit type: %s\n", namebuf);
}

PickupEffect::PickupEffect(PickupEffectType type, int sub, int slot, float time)
    : next_(nullptr), type_(type), slot_(slot), time_(time)
{
    sub_.type = sub;
}

PickupEffect::PickupEffect(PickupEffectType type, WeaponDefinition *weap,
                           int slot, float time)
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

    for (tail = (*list); tail && tail->next_; tail = tail->next_) {}

    tail->next_ = cur;
}

void BA_ParsePowerupEffect(PickupEffect **list, int pnum, float par1,
                           float par2, const char *word_par)
{
    int p_up = (int)par1;
    int slot = (int)par2;

    SYS_ASSERT(0 <= p_up && p_up < kTotalPowerTypes);

    if (slot < 0 || slot >= kTotalEffectsSlots)
        DDF_Error("POWERUP_EFFECT: bad FX slot #%d\n", p_up);

    AddPickupEffect(
        list, new PickupEffect(kPickupEffectTypePowerupEffect, p_up, slot, 0));
}

void BA_ParseScreenEffect(PickupEffect **list, int pnum, float par1, float par2,
                          const char *word_par)
{
    int slot = (int)par1;

    if (slot < 0 || slot >= kTotalEffectsSlots)
        DDF_Error("SCREEN_EFFECT: bad FX slot #%d\n", slot);

    if (par2 <= 0) DDF_Error("SCREEN_EFFECT: bad time value: %1.2f\n", par2);

    AddPickupEffect(
        list, new PickupEffect(kPickupEffectTypeScreenEffect, 0, slot, par2));
}

void BA_ParseSwitchWeapon(PickupEffect **list, int pnum, float par1, float par2,
                          const char *word_par)
{
    if (pnum != -1) DDF_Error("SWITCH_WEAPON: missing weapon name !\n");

    SYS_ASSERT(word_par && word_par[0]);

    WeaponDefinition *weap = weapondefs.Lookup(word_par);

    AddPickupEffect(
        list, new PickupEffect(kPickupEffectTypeSwitchWeapon, weap, 0, 0));
}

void BA_ParseKeepPowerup(PickupEffect **list, int pnum, float par1, float par2,
                         const char *word_par)
{
    if (pnum != -1) DDF_Error("KEEP_POWERUP: missing powerup name !\n");

    SYS_ASSERT(word_par && word_par[0]);

    if (DDF_CompareName(word_par, "BERSERK") != 0)
        DDF_Error("KEEP_POWERUP: %s is not supported\n", word_par);

    AddPickupEffect(list, new PickupEffect(kPickupEffectTypeKeepPowerup,
                                           kPowerTypeBerserk, 0, 0));
}

struct PickupEffectParser
{
    const char *name;
    int         num_pars;  // -1 means a single word
    void (*parser)(PickupEffect **list, int pnum, float par1, float par2,
                   const char *word_par);
};

static const PickupEffectParser pick_fx_parsers[] = {
    {"SCREEN_EFFECT", 2, BA_ParseScreenEffect},
    {"SWITCH_WEAPON", -1, BA_ParseSwitchWeapon},
    {"KEEP_POWERUP", -1, BA_ParseKeepPowerup},

    // that's all, folks.
    {nullptr, 0, nullptr}};

//
// DDF_MobjGetPickupEffect
//
// Parse a single effect and add it to the effect list accordingly.
// No merging is done.
//
void DDF_MobjGetPickupEffect(const char *info, void *storage)
{
    char namebuf[200];
    char parambuf[200];
    int  num_vals;

    SYS_ASSERT(storage);

    PickupEffect **fx_list = (PickupEffect **)storage;

    Benefit temp;  // FIXME kludge (write new parser method ?)

    num_vals =
        ParseBenefitString(info, namebuf, parambuf, &temp.amount, &temp.limit);

    // an error occurred ?
    if (num_vals < 0) return;

    if (parambuf[0]) num_vals = -1;

    for (int i = 0; pick_fx_parsers[i].name; i++)
    {
        if (DDF_CompareName(pick_fx_parsers[i].name, namebuf) != 0) continue;

        (*pick_fx_parsers[i].parser)(fx_list, num_vals, temp.amount, temp.limit,
                                     parambuf);

        return;
    }

    // secondly, try the powerups
    for (int p = 0; powertype_names[p].name; p++)
    {
        if (DDF_CompareName(powertype_names[p].name, namebuf) != 0) continue;

        BA_ParsePowerupEffect(fx_list, num_vals, p, temp.amount, parambuf);

        return;
    }

    DDF_Error("Unknown/Malformed benefit effect: %s\n", namebuf);
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
    {"MISSILE", kMapObjectFlagMissile, 0},  // has a special check
    {"BARE_MISSILE", kMapObjectFlagMissile, 0},
    {"DROPPED", kMapObjectFlagDropped, 0},
    {"CORPSE", kMapObjectFlagCorpse, 0},
    {"STEALTH", kMapObjectFlagStealth, 0},
    {"PRESERVE_MOMENTUM", kMapObjectFlagPreserveMomentum, 0},
    {"DEATHMATCH", kMapObjectFlagNotDeathmatch, 1},
    {"TOUCHY", kMapObjectFlagTouchy, 0},
    {nullptr, 0, 0}};

static DDFSpecialFlags extended_specials[] = {
    {"RESPAWN", kExtendedFlagNoRespawn, 1},
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
    {"FLOOR_CLIP", kHyperFlagFloorClip, 0},  // Lobo: new FLOOR_CLIP flag
    {"TRIGGER_LINES", kHyperFlagNoTriggerLines,
     1},                                    // Lobo: Cannot activate doors etc.
    {"SHOVEABLE", kHyperFlagShoveable, 0},  // Lobo: can be pushed
    {"SPLASH", kHyperFlagNoSplash, 1},      // Lobo: causes no splash on liquids
    {"DEHACKED_COMPAT", kHyperFlagDehackedCompatibility, 0},
    {"IMMOVABLE", kHyperFlagImmovable, 0},
    {"MUSIC_CHANGER", kHyperFlagMusicChanger, 0},
    {nullptr, 0, 0}};

static DDFSpecialFlags mbf21_specials[] = {{"LOGRAV", kMBF21FlagLowGravity, 0},
                                           {nullptr, 0, 0}};

//
// DDF_MobjGetSpecial
//
// Compares info the the entries in special flag lists.
// If found, apply attributes for it to current mobj.
//
void DDF_MobjGetSpecial(const char *info)
{
    int flag_value;

    // handle the "INVISIBLE" tag
    if (DDF_CompareName(info, "INVISIBLE") == 0)
    {
        dynamic_mobj->translucency_ = 0.0f;
        return;
    }

    // handle the "NOSHADOW" tag
    if (DDF_CompareName(info, "NOSHADOW") == 0)
    {
        dynamic_mobj->shadow_trans_ = 0.0f;
        return;
    }

    // the "MISSILE" tag needs special treatment, since it sets both
    // normal flags & extended flags.
    if (DDF_CompareName(info, "MISSILE") == 0)
    {
        dynamic_mobj->flags_ |= kMapObjectFlagMissile;
        dynamic_mobj->extendedflags_ |=
            kExtendedFlagCrossBlockingLines | kExtendedFlagNoFriction;
        return;
    }

    int *flag_ptr = &dynamic_mobj->flags_;

    DDFCheckFlagResult res = DDF_MainCheckSpecialFlag(info, normal_specials,
                                                      &flag_value, true, false);

    if (res == kDDFCheckFlagUser || res == kDDFCheckFlagUnknown)
    {
        // wasn't a normal special.  Try the extended ones...
        flag_ptr = &dynamic_mobj->extendedflags_;

        res = DDF_MainCheckSpecialFlag(info, extended_specials, &flag_value,
                                       true, false);
    }

    if (res == kDDFCheckFlagUser || res == kDDFCheckFlagUnknown)
    {
        // -AJA- 2004/08/25: Try the hyper specials...
        flag_ptr = &dynamic_mobj->hyperflags_;

        res = DDF_MainCheckSpecialFlag(info, hyper_specials, &flag_value, true,
                                       false);
    }

    if (res == kDDFCheckFlagUser || res == kDDFCheckFlagUnknown)
    {
        // Try the MBF21 specials...
        flag_ptr = &dynamic_mobj->mbf21flags_;

        res = DDF_MainCheckSpecialFlag(info, mbf21_specials, &flag_value, true,
                                       false);
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
            DDF_WarnError("DDF_MobjGetSpecial: Unknown special '%s'\n", info);
            break;
    }
}

static DDFSpecialFlags dlight_type_names[] = {
    {"NONE", kDynamicLightTypeNone, 0},
    {"MODULATE", kDynamicLightTypeModulate, 0},
    {"ADD", kDynamicLightTypeAdd, 0},

    // backwards compatibility
    {"LINEAR", kDynamicLightTypeCompatibilityLinear, 0},
    {"QUADRATIC", kDynamicLightTypeCompatibilityQuadratic, 0},
    {"CONSTANT", kDynamicLightTypeCompatibilityLinear, 0},

    {nullptr, 0, 0}};

//
// DDF_MobjGetDLight
//
void DDF_MobjGetDLight(const char *info, void *storage)
{
    DynamicLightType *dtype = (DynamicLightType *)storage;
    int               flag_value;

    SYS_ASSERT(dtype);

    if (kDDFCheckFlagPositive !=
        DDF_MainCheckSpecialFlag(info, dlight_type_names, &flag_value, false,
                                 false))
    {
        DDF_WarnError("Unknown dlight type '%s'\n", info);
        return;
    }

    (*dtype) = (DynamicLightType)flag_value;
}

//
// DDF_MobjGetExtra
//
void DDF_MobjGetExtra(const char *info, void *storage)
{
    int *extendedflags = (int *)storage;

    // If keyword is "NULL", then the mobj is not marked as extra.
    // Otherwise it is.

    if (DDF_CompareName(info, "NULL") == 0)
    {
        *extendedflags &= ~kExtendedFlagExtra;
    }
    else { *extendedflags |= kExtendedFlagExtra; }
}

//
// DDF_MobjGetPlayer
//
// Reads player number and makes sure that maxplayer is large enough.
//
void DDF_MobjGetPlayer(const char *info, void *storage)
{
    int *dest = (int *)storage;

    DDF_MainGetNumeric(info, storage);

    if (*dest > 32) DDF_Warning("Player number '%d' will not work.", *dest);
}

static void DDF_MobjGetGlowType(const char *info, void *storage)
{
    SectorGlowType *glow = (SectorGlowType *)storage;

    if (epi::StringCaseCompareASCII(info, "FLOOR") == 0)
        *glow = kSectorGlowTypeFloor;
    else if (epi::StringCaseCompareASCII(info, "CEILING") == 0)
        *glow = kSectorGlowTypeCeiling;
    else if (epi::StringCaseCompareASCII(info, "WALL") == 0)
        *glow = kSectorGlowTypeWall;
    else  // Unknown/None
        *glow = kSectorGlowTypeNone;
}

static const DDFSpecialFlags sprite_yalign_names[] = {
    {"BOTTOM", SpriteYAlignmentBottomUp, 0},
    {"MIDDLE", SpriteYAlignmentMiddle, 0},
    {"TOP", SpriteYAlignmentTopDown, 0},

    {nullptr, 0, 0}};

static void DDF_MobjGetYAlign(const char *info, void *storage)
{
    if (kDDFCheckFlagPositive !=
        DDF_MainCheckSpecialFlag(info, sprite_yalign_names, (int *)storage,
                                 false, false))
    {
        DDF_WarnError("DDF_MobjGetYAlign: Unknown alignment: %s\n", info);
    }
}

static void DDF_MobjGetPercentRange(const char *info, void *storage)
{
    SYS_ASSERT(info && storage);

    float *dest = (float *)storage;

    if (sscanf(info, "%f%%:%f%%", dest + 0, dest + 1) != 2)
        DDF_Error("Bad percentage range: %s\n", info);

    dest[0] /= 100.0f;
    dest[1] /= 100.0f;

    if (dest[0] > dest[1])
        DDF_Error("Bad percent range (low > high) : %s\n", info);
}

static void DDF_MobjGetAngleRange(const char *info, void *storage)
{
    SYS_ASSERT(info && storage);

    BAMAngle *dest = (BAMAngle *)storage;

    float val1, val2;

    if (sscanf(info, "%f:%f", &val1, &val2) != 2)
        DDF_Error("Bad angle range: %s\n", info);

    dest[0] = epi::BAMFromDegrees(val1);
    dest[1] = epi::BAMFromDegrees(val2);
}

//
// DDF_MobjStateGetRADTrigger
//
static void DDF_MobjStateGetRADTrigger(const char *arg, State *cur_state)
{
    if (!arg || !arg[0]) return;

    int *val_ptr = new int;

    // Modified RAD_CheckForInt
    const char *pos    = arg;
    int         count  = 0;
    int         length = strlen(arg);

    while (epi::IsDigitASCII(*pos++)) count++;

    // Is the value an integer?
    if (length != count)
    {
        *val_ptr                = epi::StringHash32(arg);
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
//  CONDITION TESTERS
//
//  These return true if the name matches that particular type of
//  condition (e.g. "ROCKET" for ammo), and adjusts the condition
//  accodingly.  Otherwise returns false.
//

static bool ConditionTryCounter(const char *name, const char *sub,
                                ConditionCheck *cond)
{
    if (kDDFCheckFlagPositive != DDF_MainCheckSpecialFlag(name, counter_types,
                                                          &cond->sub.type,
                                                          false, false))
    {
        return false;
    }

    if (sub[0]) sscanf(sub, " %f ", &cond->amount);

    cond->cond_type = kConditionCheckTypeCounter;
    return true;
}

static bool ConditionTryInventory(const char *name, const char *sub,
                                  ConditionCheck *cond)
{
    if (kDDFCheckFlagPositive != DDF_MainCheckSpecialFlag(name, inv_types,
                                                          &cond->sub.type,
                                                          false, false))
    {
        return false;
    }

    if (sub[0]) sscanf(sub, " %f ", &cond->amount);

    cond->cond_type = kConditionCheckTypeInventory;
    return true;
}

static bool ConditionTryAmmo(const char *name, const char *sub,
                             ConditionCheck *cond)
{
    if (kDDFCheckFlagPositive != DDF_MainCheckSpecialFlag(name, ammo_types,
                                                          &cond->sub.type,
                                                          false, false))
    {
        return false;
    }

    if ((AmmunitionType)cond->sub.type == kAmmunitionTypeNoAmmo)
    {
        DDF_WarnError("Illegal ammo in condition: %s\n", name);
        return false;
    }

    if (sub[0]) sscanf(sub, " %f ", &cond->amount);

    cond->cond_type = kConditionCheckTypeAmmo;
    return true;
}

static bool ConditionTryWeapon(const char *name, const char *sub,
                               ConditionCheck *cond)
{
    int idx = weapondefs.FindFirst(name, 0);

    if (idx < 0) return false;

    cond->sub.weap = weapondefs[idx];

    cond->cond_type = kConditionCheckTypeWeapon;
    return true;
}

static bool ConditionTryKey(const char *name, const char *sub,
                            ConditionCheck *cond)
{
    if (kDDFCheckFlagPositive != DDF_MainCheckSpecialFlag(name, keytype_names,
                                                          &cond->sub.type,
                                                          false, false))
    {
        return false;
    }

    cond->cond_type = kConditionCheckTypeKey;
    return true;
}

static bool ConditionTryHealth(const char *name, const char *sub,
                               ConditionCheck *cond)
{
    if (DDF_CompareName(name, "HEALTH") != 0) return false;

    if (sub[0]) sscanf(sub, " %f ", &cond->amount);

    cond->cond_type = kConditionCheckTypeHealth;
    return true;
}

static bool ConditionTryArmour(const char *name, const char *sub,
                               ConditionCheck *cond)
{
    if (DDF_CompareName(name, "ARMOUR") == 0)
    {
        cond->sub.type = kTotalArmourTypes;
    }
    else if (kDDFCheckFlagPositive !=
             DDF_MainCheckSpecialFlag(name, armourtype_names, &cond->sub.type,
                                      false, false))
    {
        return false;
    }

    if (sub[0]) sscanf(sub, " %f ", &cond->amount);

    cond->cond_type = kConditionCheckTypeArmour;
    return true;
}

static bool ConditionTryPowerup(const char *name, const char *sub,
                                ConditionCheck *cond)
{
    if (kDDFCheckFlagPositive != DDF_MainCheckSpecialFlag(name, powertype_names,
                                                          &cond->sub.type,
                                                          false, false))
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

static bool ConditionTryPlayerState(const char *name, const char *sub,
                                    ConditionCheck *cond)
{
    return (kDDFCheckFlagPositive ==
            DDF_MainCheckSpecialFlag(name, simplecond_names,
                                     (int *)&cond->cond_type, false, false));
}

//
// DDF_MainParseCondition
//
// Returns `false' if parsing failed.
//
bool DDF_MainParseCondition(const char *info, ConditionCheck *cond)
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

    memset(&cond->sub, 0, sizeof(cond->sub));

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
        DDF_WarnError("Malformed condition string: %s\n", info);
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

    if (ConditionTryAmmo(typebuf + t_off, sub_buf, cond) ||
        ConditionTryInventory(typebuf + t_off, sub_buf, cond) ||
        ConditionTryCounter(typebuf + t_off, sub_buf, cond) ||
        ConditionTryWeapon(typebuf + t_off, sub_buf, cond) ||
        ConditionTryKey(typebuf + t_off, sub_buf, cond) ||
        ConditionTryHealth(typebuf + t_off, sub_buf, cond) ||
        ConditionTryArmour(typebuf + t_off, sub_buf, cond) ||
        ConditionTryPowerup(typebuf + t_off, sub_buf, cond) ||
        ConditionTryPlayerState(typebuf + t_off, sub_buf, cond))
    {
        return true;
    }

    DDF_WarnError("Unknown/Malformed condition type: %s\n", typebuf);
    return false;
}

// ---> mobjdef class

MapObjectDefinition::MapObjectDefinition() : name_(), state_grp_()
{
    Default();
}

MapObjectDefinition::~MapObjectDefinition() {}

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

    reactiontime_ = src.reactiontime_;
    painchance_   = src.painchance_;
    spawnhealth_  = src.spawnhealth_;
    speed_        = src.speed_;
    float_speed_  = src.float_speed_;
    radius_       = src.radius_;
    height_       = src.height_;
    step_size_    = src.step_size_;
    mass_         = src.mass_;

    flags_         = src.flags_;
    extendedflags_ = src.extendedflags_;
    hyperflags_    = src.hyperflags_;
    mbf21flags_    = src.mbf21flags_;

    explode_damage_ = src.explode_damage_;
    explode_radius_ = src.explode_radius_;

    // pickup_message_ = src.pickup_message_;
    // lose_benefits_ = src.lose_benefits_;
    // pickup_benefits_ = src.pickup_benefits_;
    if (src.pickup_message_ != "") { pickup_message_ = src.pickup_message_; }

    lose_benefits_   = nullptr;
    pickup_benefits_ = nullptr;
    kill_benefits_   = nullptr;  // I think? - Dasho
    /*
    if(src.pickup_benefits_)
    {
        EDGEDebugf("%s: Benefits info not inherited from '%s', ",name,
    src.name_.c_str()); EDGEDebugf("You should define it explicitly.\n");
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
    dlight_[0] = src.dlight_[0];
    dlight_[1] = src.dlight_[1];

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

    gib_health_ = src.gib_health_;

    infight_group_ = src.infight_group_;
    proj_group_    = src.proj_group_;
    splash_group_  = src.splash_group_;
    fast_speed_    = src.fast_speed_;
    melee_range_   = src.melee_range_;
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

    reactiontime_ = 0;
    painchance_   = 0.0f;
    spawnhealth_  = 1000.0f;
    speed_        = 0;
    float_speed_  = 2.0f;
    radius_       = 0;
    height_       = 0;
    step_size_    = 24.0f;
    mass_         = 100.0f;

    flags_         = 0;
    extendedflags_ = 0;
    hyperflags_    = 0;
    mbf21flags_    = 0;

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
    armour_protect_ = -1.0;  // disabled!
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
    resist_painchance_ = -1;  // disabled
    ghost_             = 0;

    closecombat_ = nullptr;
    rangeattack_ = nullptr;
    spareattack_ = nullptr;

    // dynamic light info
    dlight_[0].Default();
    dlight_[1].Default();

    weak_.Default();

    dropitem_ = nullptr;
    dropitem_ref_.clear();
    blood_ = nullptr;
    blood_ref_.clear();
    respawneffect_ = nullptr;
    respawneffect_ref_.clear();
    spitspot_ = nullptr;
    spitspot_ref_.clear();

    gib_health_ = 0;

    sight_distance_ = -1;
    hear_distance_  = -1;

    morphtimeout_ = 0;

    infight_group_ = -2;
    proj_group_    = -2;
    splash_group_  = -2;
    fast_speed_    = -1;
    melee_range_   = -1;
}

void MapObjectDefinition::DLightCompatibility(void)
{
    for (int DL = 0; DL < 2; DL++)
    {
        int r = epi::GetRGBARed(dlight_[DL].colour_);
        int g = epi::GetRGBAGreen(dlight_[DL].colour_);
        int b = epi::GetRGBABlue(dlight_[DL].colour_);

        // dim the colour
        r = int(r * 0.8f);
        g = int(g * 0.8f);
        b = int(b * 0.8f);

        switch (dlight_[DL].type_)
        {
            case kDynamicLightTypeCompatibilityQuadratic:
                dlight_[DL].type_ = kDynamicLightTypeModulate;
                dlight_[DL].radius_ =
                    DynamicLightCompatibilityRadius(dlight_[DL].radius_);
                dlight_[DL].colour_ = epi::MakeRGBA(r, g, b);

                hyperflags_ |= kHyperFlagQuadraticDynamicLight;
                break;

            case kDynamicLightTypeCompatibilityLinear:
                dlight_[DL].type_ = kDynamicLightTypeModulate;
                dlight_[DL].radius_ *= 1.3;
                dlight_[DL].colour_ = epi::MakeRGBA(r, g, b);
                break;

            default:  // nothing to do
                break;
        }
    }
}

// --> MapObjectDefinitionContainer class

MapObjectDefinitionContainer::MapObjectDefinitionContainer()
{
    memset(lookupatch_font_cache_, 0, sizeof(MapObjectDefinition *) * kLookupCacheSize);
}

MapObjectDefinitionContainer::~MapObjectDefinitionContainer()
{
    for (std::vector<MapObjectDefinition *>::iterator iter     = begin(),
                                                      iter_end = end();
         iter != iter_end; iter++)
    {
        MapObjectDefinition *m = *iter;
        delete m;
        m = nullptr;
    }
}

int MapObjectDefinitionContainer::FindFirst(const char *name, int startpos)
{
    startpos = HMM_MAX(startpos, 0);

    for (startpos; startpos < size(); startpos++)
    {
        MapObjectDefinition *m = at(startpos);
        if (DDF_CompareName(m->name_.c_str(), name) == 0) return startpos;
    }

    return -1;
}

int MapObjectDefinitionContainer::FindLast(const char *name, int startpos)
{
    startpos = HMM_MIN(startpos, size() - 1);

    for (startpos; startpos >= 0; startpos--)
    {
        MapObjectDefinition *m = at(startpos);
        if (DDF_CompareName(m->name_.c_str(), name) == 0) return startpos;
    }

    return -1;
}

bool MapObjectDefinitionContainer::MoveToEnd(int idx)
{
    // Moves an entry from its current position to end of the list.

    MapObjectDefinition *m = nullptr;

    if (idx < 0 || idx >= size()) return false;

    if (idx == (size() - 1)) return true;  // Already at the end

    // Get a copy of the pointer
    m = at(idx);

    erase(begin() + idx);

    push_back(m);

    return true;
}

const MapObjectDefinition *MapObjectDefinitionContainer::Lookup(
    const char *refname)
{
    // Looks an mobjdef by name.
    // Fatal error if it does not exist.

    int idx = FindLast(refname);

    if (idx >= 0) return (*this)[idx];

    if (lax_errors) return default_mobjtype;

    DDF_Error("Unknown thing type: %s\n", refname);
    return nullptr; /* NOT REACHED */
}

const MapObjectDefinition *MapObjectDefinitionContainer::Lookup(int id)
{
    if (id == 0) return default_mobjtype;

    // Looks an mobjdef by number.
    // Fatal error if it does not exist.

    int slot = (((id) + kLookupCacheSize) % kLookupCacheSize);

    // check the cache
    if (lookupatch_font_cache_[slot] && lookupatch_font_cache_[slot]->number_ == id)
    {
        return lookupatch_font_cache_[slot];
    }

    for (std::vector<MapObjectDefinition *>::reverse_iterator iter = rbegin(),
                                                              iter_end = rend();
         iter != iter_end; iter++)
    {
        MapObjectDefinition *m = *iter;

        if (m->number_ == id)
        {
            // update the cache
            lookupatch_font_cache_[slot] = m;
            return m;
        }
    }

    return nullptr;
}

const MapObjectDefinition *MapObjectDefinitionContainer::LookupCastMember(
    int castpos)
{
    // Lookup the cast member of the one with the nearest match
    // to the position given.

    MapObjectDefinition *best = nullptr;
    MapObjectDefinition *m    = nullptr;

    for (std::vector<MapObjectDefinition *>::reverse_iterator iter = rbegin(),
                                                              iter_end = rend();
         iter != iter_end; iter++)
    {
        m = *iter;
        if (m->castorder_ > 0)
        {
            if (m->castorder_ == castpos)  // Exact match
                return m;

            if (best)
            {
                if (m->castorder_ > castpos)
                {
                    if (best->castorder_ > castpos)
                    {
                        int of1 = m->castorder_ - castpos;
                        int of2 = best->castorder_ - castpos;

                        if (of2 > of1) best = m;
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

                        if (of1 > of2) best = m;
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

const MapObjectDefinition *MapObjectDefinitionContainer::LookupPlayer(
    int playernum)
{
    // Find a player thing (needed by deathmatch code).
    for (std::vector<MapObjectDefinition *>::reverse_iterator iter = rbegin(),
                                                              iter_end = rend();
         iter != iter_end; iter++)
    {
        MapObjectDefinition *m = *iter;

        if (m->playernum_ == playernum) return m;
    }

    EDGEError("Missing DDF entry for player number %d\n", playernum);
    return nullptr; /* NOT REACHED */
}

const MapObjectDefinition *MapObjectDefinitionContainer::LookupDoorKey(
    int theKey)
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

    for (std::vector<MapObjectDefinition *>::reverse_iterator iter = rbegin(),
                                                              iter_end = rend();
         iter != iter_end; iter++)
    {
        MapObjectDefinition *m = *iter;

        Benefit *list;
        list = m->pickup_benefits_;
        for (; list != nullptr; list = list->next)
        {
            if (list->type == kBenefitTypeKey)
            {
                if (list->sub.type == theKey) { return m; }
            }
        }
    }

    EDGEWarning("Missing DDF entry for key %d\n", theKey);
    return nullptr;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
