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

#include <string.h>

#include "local.h"
#include "thing.h"
#include "types.h"

#include "p_action.h"
#include "str_compare.h"
#include "str_util.h"

#undef DF
#define DF DDF_FIELD

#define DDF_MobjHashFunc(x) (((x) + LOOKUP_CACHESIZE) % LOOKUP_CACHESIZE)

const char *TemplateThing = nullptr; // Lobo 2022: TEMPLATE inheritance fix

mobjtype_container_c mobjtypes;

static mobjtype_c *default_mobjtype;

void DDF_MobjGetSpecial(const char *info);
void DDF_MobjGetBenefit(const char *info, void *storage);
void DDF_MobjGetPickupEffect(const char *info, void *storage);
void DDF_MobjGetDLight(const char *info, void *storage);

static void DDF_MobjGetGlowType(const char *info, void *storage);
static void DDF_MobjGetYAlign(const char *info, void *storage);
static void DDF_MobjGetPercentRange(const char *info, void *storage);
static void DDF_MobjGetAngleRange(const char *info, void *storage);
static void DDF_MobjStateGetRADTrigger(const char *arg, state_t *cur_state);

static void AddPickupEffect(pickup_effect_c **list, pickup_effect_c *cur);

static int dlight_radius_warnings = 0;

#undef DDF_CMD_BASE
#define DDF_CMD_BASE dummy_dlight
static dlight_info_c dummy_dlight;

const commandlist_t dlight_commands[] = {DF("TYPE", type, DDF_MobjGetDLight), DF("GRAPHIC", shape, DDF_MainGetString),
                                         DF("RADIUS", radius, DDF_MainGetFloat), DF("COLOUR", colour, DDF_MainGetRGB),
                                         DF("HEIGHT", height, DDF_MainGetPercent),
                                         DF("LEAKY", leaky, DDF_MainGetBoolean),

                                         // backwards compatibility
                                         DF("INTENSITY", radius, DDF_MainGetFloat),

                                         DDF_CMD_END};

#undef DDF_CMD_BASE
#define DDF_CMD_BASE dummy_weakness
static weakness_info_c dummy_weakness;

const commandlist_t weakness_commands[] = {DF("CLASS", classes, DDF_MainGetBitSet),
                                           DF("HEIGHTS", height, DDF_MobjGetPercentRange),
                                           DF("ANGLES", angle, DDF_MobjGetAngleRange),
                                           DF("MULTIPLY", multiply, DDF_MainGetFloat),
                                           DF("PAINCHANCE", painchance, DDF_MainGetPercent),

                                           DDF_CMD_END};

mobjtype_c *dynamic_mobj;

#undef DDF_CMD_BASE
#define DDF_CMD_BASE dummy_mobj
static mobjtype_c dummy_mobj;

const commandlist_t thing_commands[] = {
    // sub-commands
    DDF_SUB_LIST("DLIGHT", dlight[0], dlight_commands), DDF_SUB_LIST("DLIGHT2", dlight[1], dlight_commands),
    DDF_SUB_LIST("WEAKNESS", weak, weakness_commands), DDF_SUB_LIST("EXPLODE_DAMAGE", explode_damage, damage_commands),
    DDF_SUB_LIST("CHOKE_DAMAGE", choke_damage, damage_commands),

    DF("SPAWNHEALTH", spawnhealth, DDF_MainGetFloat), DF("RADIUS", radius, DDF_MainGetFloat),
    DF("HEIGHT", height, DDF_MainGetFloat), DF("MASS", mass, DDF_MainGetFloat), DF("SPEED", speed, DDF_MainGetFloat),
    DF("FAST", fast, DDF_MainGetFloat), DF("EXTRA", extendedflags, DDF_MobjGetExtra),
    DF("RESPAWN_TIME", respawntime, DDF_MainGetTime), DF("FUSE", fuse, DDF_MainGetTime),
    DF("LIFESPAN", fuse, DDF_MainGetTime), DF("PALETTE_REMAP", palremap, DDF_MainGetColourmap),
    DF("TRANSLUCENCY", translucency, DDF_MainGetPercent),

    DF("INITIAL_BENEFIT", initial_benefits, DDF_MobjGetBenefit), DF("LOSE_BENEFIT", lose_benefits, DDF_MobjGetBenefit),
    DF("PICKUP_BENEFIT", pickup_benefits, DDF_MobjGetBenefit), DF("KILL_BENEFIT", kill_benefits, DDF_MobjGetBenefit),
    DF("PICKUP_MESSAGE", pickup_message, DDF_MainGetString),
    DF("PICKUP_EFFECT", pickup_effects, DDF_MobjGetPickupEffect),

    DF("PAINCHANCE", painchance, DDF_MainGetPercent), DF("MINATTACK_CHANCE", minatkchance, DDF_MainGetPercent),
    DF("REACTION_TIME", reactiontime, DDF_MainGetTime), DF("JUMP_DELAY", jump_delay, DDF_MainGetTime),
    DF("JUMP_HEIGHT", jumpheight, DDF_MainGetFloat), DF("CROUCH_HEIGHT", crouchheight, DDF_MainGetFloat),
    DF("VIEW_HEIGHT", viewheight, DDF_MainGetPercent), DF("SHOT_HEIGHT", shotheight, DDF_MainGetPercent),
    DF("MAX_FALL", maxfall, DDF_MainGetFloat), DF("CASTORDER", castorder, DDF_MainGetNumeric),
    DF("CAST_TITLE", cast_title, DDF_MainGetString), DF("PLAYER", playernum, DDF_MobjGetPlayer),
    DF("SIDE", side, DDF_MainGetBitSet), DF("CLOSE_ATTACK", closecombat, DDF_MainRefAttack),
    DF("RANGE_ATTACK", rangeattack, DDF_MainRefAttack), DF("SPARE_ATTACK", spareattack, DDF_MainRefAttack),
    DF("DROPITEM", dropitem_ref, DDF_MainGetString), DF("BLOOD", blood_ref, DDF_MainGetString),
    DF("RESPAWN_EFFECT", respawneffect_ref, DDF_MainGetString), DF("SPIT_SPOT", spitspot_ref, DDF_MainGetString),

    DF("PICKUP_SOUND", activesound, DDF_MainLookupSound), DF("ACTIVE_SOUND", activesound, DDF_MainLookupSound),
    DF("LAUNCH_SOUND", seesound, DDF_MainLookupSound), DF("AMBIENT_SOUND", seesound, DDF_MainLookupSound),
    DF("SIGHTING_SOUND", seesound, DDF_MainLookupSound), DF("DEATH_SOUND", deathsound, DDF_MainLookupSound),
    DF("OVERKILL_SOUND", overkill_sound, DDF_MainLookupSound), DF("PAIN_SOUND", painsound, DDF_MainLookupSound),
    DF("STARTCOMBAT_SOUND", attacksound, DDF_MainLookupSound), DF("WALK_SOUND", walksound, DDF_MainLookupSound),
    DF("JUMP_SOUND", jump_sound, DDF_MainLookupSound), DF("NOWAY_SOUND", noway_sound, DDF_MainLookupSound),
    DF("OOF_SOUND", oof_sound, DDF_MainLookupSound), DF("FALLPAIN_SOUND", fallpain_sound, DDF_MainLookupSound),
    DF("GASP_SOUND", gasp_sound, DDF_MainLookupSound), DF("SECRET_SOUND", secretsound, DDF_MainLookupSound),
    DF("FALLING_SOUND", falling_sound, DDF_MainLookupSound), DF("RIP_SOUND", rip_sound, DDF_MainLookupSound),

    DF("FLOAT_SPEED", float_speed, DDF_MainGetFloat), DF("STEP_SIZE", step_size, DDF_MainGetFloat),
    DF("SPRITE_SCALE", scale, DDF_MainGetFloat), DF("SPRITE_ASPECT", aspect, DDF_MainGetFloat),
    DF("SPRITE_YALIGN", yalign, DDF_MobjGetYAlign),   // -AJA- 2007/08/08
    DF("MODEL_SKIN", model_skin, DDF_MainGetNumeric), // -AJA- 2007/10/16
    DF("MODEL_SCALE", model_scale, DDF_MainGetFloat), DF("MODEL_ASPECT", model_aspect, DDF_MainGetFloat),
    DF("MODEL_BIAS", model_bias, DDF_MainGetFloat), DF("MODEL_ROTATE", model_rotate, DDF_MainGetNumeric),
    DF("BOUNCE_SPEED", bounce_speed, DDF_MainGetFloat), DF("BOUNCE_UP", bounce_up, DDF_MainGetFloat),
    DF("SIGHT_SLOPE", sight_slope, DDF_MainGetSlope), DF("SIGHT_ANGLE", sight_angle, DDF_MainGetAngle),
    DF("RIDE_FRICTION", ride_friction, DDF_MainGetFloat), DF("BOBBING", bobbing, DDF_MainGetPercent),
    DF("IMMUNITY_CLASS", immunity, DDF_MainGetBitSet), DF("RESISTANCE_CLASS", resistance, DDF_MainGetBitSet),
    DF("RESISTANCE_MULTIPLY", resist_multiply, DDF_MainGetFloat),
    DF("RESISTANCE_PAINCHANCE", resist_painchance, DDF_MainGetPercent),
    DF("GHOST_CLASS", ghost, DDF_MainGetBitSet), // -AJA- 2005/05/15
    DF("SHADOW_TRANSLUCENCY", shadow_trans, DDF_MainGetPercent), DF("LUNG_CAPACITY", lung_capacity, DDF_MainGetTime),
    DF("GASP_START", gasp_start, DDF_MainGetTime), DF("EXPLODE_RADIUS", explode_radius, DDF_MainGetFloat),
    DF("RELOAD_SHOTS", reload_shots, DDF_MainGetNumeric),          // -AJA- 2004/11/15
    DF("GLOW_TYPE", glow_type, DDF_MobjGetGlowType),               // -AJA- 2007/08/19
    DF("ARMOUR_PROTECTION", armour_protect, DDF_MainGetPercent),   // -AJA- 2007/08/22
    DF("ARMOUR_DEPLETION", armour_deplete, DDF_MainGetPercentAny), // -AJA- 2007/08/22
    DF("ARMOUR_CLASS", armour_class, DDF_MainGetBitSet),           // -AJA- 2007/08/22

    DF("SIGHT_DISTANCE", sight_distance, DDF_MainGetFloat), // Lobo 2022
    DF("HEAR_DISTANCE", hear_distance, DDF_MainGetFloat),   // Lobo 2022

    DF("MORPH_TIMEOUT", morphtimeout, DDF_MainGetTime), // Lobo 2023

    // DEHEXTRA
    DF("GIB_HEALTH", gib_health, DDF_MainGetFloat),

    DF("INFIGHTING_GROUP", infight_group, DDF_MainGetNumeric), DF("PROJECTILE_GROUP", proj_group, DDF_MainGetNumeric),
    DF("SPLASH_GROUP", splash_group, DDF_MainGetNumeric), DF("FAST_SPEED", fast_speed, DDF_MainGetNumeric),
    DF("MELEE_RANGE", melee_range, DDF_MainGetNumeric),

    // -AJA- backwards compatibility cruft...
    DF("EXPLOD_DAMAGE", explode_damage.nominal, DDF_MainGetFloat),
    DF("EXPLOSION_DAMAGE", explode_damage.nominal, DDF_MainGetFloat),
    DF("EXPLOD_DAMAGERANGE", explode_damage.nominal, DDF_MainGetFloat),

    DDF_CMD_END};

const state_starter_t thing_starters[] = {DDF_STATE("SPAWN", "IDLE", spawn_state),
                                          DDF_STATE("IDLE", "IDLE", idle_state),
                                          DDF_STATE("CHASE", "CHASE", chase_state),
                                          DDF_STATE("PAIN", "IDLE", pain_state),
                                          DDF_STATE("MISSILE", "IDLE", missile_state),
                                          DDF_STATE("MELEE", "IDLE", melee_state),
                                          DDF_STATE("DEATH", "REMOVE", death_state),
                                          DDF_STATE("OVERKILL", "REMOVE", overkill_state),
                                          DDF_STATE("RESPAWN", "IDLE", raise_state),
                                          DDF_STATE("RESURRECT", "IDLE", res_state),
                                          DDF_STATE("MEANDER", "MEANDER", meander_state),
                                          DDF_STATE("MORPH", "MORPH", morph_state),
                                          DDF_STATE("BOUNCE", "IDLE", bounce_state),
                                          DDF_STATE("TOUCH", "IDLE", touch_state),
                                          DDF_STATE("RELOAD", "IDLE", reload_state),
                                          DDF_STATE("GIB", "REMOVE", gib_state),

                                          DDF_STATE_END};

// -KM- 1998/11/25 Added weapon functions.
// -AJA- 1999/08/09: Moved this here from p_action.h, and added an extra
// field `handle_arg' for things like "WEAPON_SHOOT(FIREBALL)".

const actioncode_t thing_actions[] = {{"NOTHING", nullptr, nullptr},

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
                                      {"RANDOM_TRACER", P_ActHomingProjectile, nullptr}, // same as above
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
                                      {"MORPH", P_ActMorph, DDF_StateGetMorph}, // same as BECOME but resets health
                                      {"UNMORPH", P_ActUnMorph, nullptr},          // same as UNBECOME but resets health

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

const specflags_t keytype_names[] = {{"BLUECARD", KF_BlueCard, 0},
                                     {"YELLOWCARD", KF_YellowCard, 0},
                                     {"REDCARD", KF_RedCard, 0},
                                     {"GREENCARD", KF_GreenCard, 0},

                                     {"BLUESKULL", KF_BlueSkull, 0},
                                     {"YELLOWSKULL", KF_YellowSkull, 0},
                                     {"REDSKULL", KF_RedSkull, 0},
                                     {"GREENSKULL", KF_GreenSkull, 0},

                                     {"GOLD_KEY", KF_GoldKey, 0},
                                     {"SILVER_KEY", KF_SilverKey, 0},
                                     {"BRASS_KEY", KF_BrassKey, 0},
                                     {"COPPER_KEY", KF_CopperKey, 0},
                                     {"STEEL_KEY", KF_SteelKey, 0},
                                     {"WOODEN_KEY", KF_WoodenKey, 0},
                                     {"FIRE_KEY", KF_FireKey, 0},
                                     {"WATER_KEY", KF_WaterKey, 0},

                                     // -AJA- compatibility (this way is the easiest)
                                     {"KEY_BLUECARD", KF_BlueCard, 0},
                                     {"KEY_YELLOWCARD", KF_YellowCard, 0},
                                     {"KEY_REDCARD", KF_RedCard, 0},
                                     {"KEY_GREENCARD", KF_GreenCard, 0},

                                     {"KEY_BLUESKULL", KF_BlueSkull, 0},
                                     {"KEY_YELLOWSKULL", KF_YellowSkull, 0},
                                     {"KEY_REDSKULL", KF_RedSkull, 0},
                                     {"KEY_GREENSKULL", KF_GreenSkull, 0},

                                     {nullptr, 0, 0}};

const specflags_t armourtype_names[] = {{"GREEN_ARMOUR", ARMOUR_Green, 0},   {"BLUE_ARMOUR", ARMOUR_Blue, 0},
                                        {"PURPLE_ARMOUR", ARMOUR_Purple, 0}, {"YELLOW_ARMOUR", ARMOUR_Yellow, 0},
                                        {"RED_ARMOUR", ARMOUR_Red, 0},       {nullptr, 0, 0}};

const specflags_t powertype_names[] = {
    {"POWERUP_INVULNERABLE", PW_Invulnerable, 0}, {"POWERUP_BARE_BERSERK", PW_Berserk, 0},
    {"POWERUP_BERSERK", PW_Berserk, 0},           {"POWERUP_PARTINVIS", PW_PartInvis, 0},
    {"POWERUP_ACIDSUIT", PW_AcidSuit, 0},         {"POWERUP_AUTOMAP", PW_AllMap, 0},
    {"POWERUP_LIGHTGOGGLES", PW_Infrared, 0},     {"POWERUP_JETPACK", PW_Jetpack, 0},
    {"POWERUP_NIGHTVISION", PW_NightVision, 0},   {"POWERUP_SCUBA", PW_Scuba, 0},
    {"POWERUP_TIMESTOP", PW_TimeStop, 0},         {nullptr, 0, 0}};

const specflags_t simplecond_names[] = {{"JUMPING", COND_Jumping, 0},     {"CROUCHING", COND_Crouching, 0},
                                        {"SWIMMING", COND_Swimming, 0},   {"ATTACKING", COND_Attacking, 0},
                                        {"RAMPAGING", COND_Rampaging, 0}, {"USING", COND_Using, 0},
                                        {"ACTION1", COND_Action1, 0},     {"ACTION2", COND_Action2, 0},
                                        {"WALKING", COND_Walking, 0},     {nullptr, 0, 0}};

const specflags_t inv_types[] = {
    {"INVENTORY01", INV_01, 0}, {"INVENTORY02", INV_02, 0}, {"INVENTORY03", INV_03, 0}, {"INVENTORY04", INV_04, 0},
    {"INVENTORY05", INV_05, 0}, {"INVENTORY06", INV_06, 0}, {"INVENTORY07", INV_07, 0}, {"INVENTORY08", INV_08, 0},
    {"INVENTORY09", INV_09, 0}, {"INVENTORY10", INV_10, 0}, {"INVENTORY11", INV_11, 0}, {"INVENTORY12", INV_12, 0},
    {"INVENTORY13", INV_13, 0}, {"INVENTORY14", INV_14, 0}, {"INVENTORY15", INV_15, 0}, {"INVENTORY16", INV_16, 0},
    {"INVENTORY17", INV_17, 0}, {"INVENTORY18", INV_18, 0}, {"INVENTORY19", INV_19, 0}, {"INVENTORY20", INV_20, 0},
    {"INVENTORY21", INV_21, 0}, {"INVENTORY22", INV_22, 0}, {"INVENTORY23", INV_23, 0}, {"INVENTORY24", INV_24, 0},
    {"INVENTORY25", INV_25, 0}, {"INVENTORY26", INV_26, 0}, {"INVENTORY27", INV_27, 0}, {"INVENTORY28", INV_28, 0},
    {"INVENTORY29", INV_29, 0}, {"INVENTORY30", INV_30, 0}, {"INVENTORY31", INV_31, 0}, {"INVENTORY32", INV_32, 0},
    {"INVENTORY33", INV_33, 0}, {"INVENTORY34", INV_34, 0}, {"INVENTORY35", INV_35, 0}, {"INVENTORY36", INV_36, 0},
    {"INVENTORY37", INV_37, 0}, {"INVENTORY38", INV_38, 0}, {"INVENTORY39", INV_39, 0}, {"INVENTORY40", INV_40, 0},
    {"INVENTORY41", INV_41, 0}, {"INVENTORY42", INV_42, 0}, {"INVENTORY43", INV_43, 0}, {"INVENTORY44", INV_44, 0},
    {"INVENTORY45", INV_45, 0}, {"INVENTORY46", INV_46, 0}, {"INVENTORY47", INV_47, 0}, {"INVENTORY48", INV_48, 0},
    {"INVENTORY49", INV_49, 0}, {"INVENTORY50", INV_50, 0}, {"INVENTORY51", INV_51, 0}, {"INVENTORY52", INV_52, 0},
    {"INVENTORY53", INV_53, 0}, {"INVENTORY54", INV_54, 0}, {"INVENTORY55", INV_55, 0}, {"INVENTORY56", INV_56, 0},
    {"INVENTORY57", INV_57, 0}, {"INVENTORY58", INV_58, 0}, {"INVENTORY59", INV_59, 0}, {"INVENTORY60", INV_60, 0},
    {"INVENTORY61", INV_61, 0}, {"INVENTORY62", INV_62, 0}, {"INVENTORY63", INV_63, 0}, {"INVENTORY64", INV_64, 0},
    {"INVENTORY65", INV_65, 0}, {"INVENTORY66", INV_66, 0}, {"INVENTORY67", INV_67, 0}, {"INVENTORY68", INV_68, 0},
    {"INVENTORY69", INV_69, 0}, {"INVENTORY70", INV_70, 0}, {"INVENTORY71", INV_71, 0}, {"INVENTORY72", INV_72, 0},
    {"INVENTORY73", INV_73, 0}, {"INVENTORY74", INV_74, 0}, {"INVENTORY75", INV_75, 0}, {"INVENTORY76", INV_76, 0},
    {"INVENTORY77", INV_77, 0}, {"INVENTORY78", INV_78, 0}, {"INVENTORY79", INV_79, 0}, {"INVENTORY80", INV_80, 0},
    {"INVENTORY81", INV_81, 0}, {"INVENTORY82", INV_82, 0}, {"INVENTORY83", INV_83, 0}, {"INVENTORY84", INV_84, 0},
    {"INVENTORY85", INV_85, 0}, {"INVENTORY86", INV_86, 0}, {"INVENTORY87", INV_87, 0}, {"INVENTORY88", INV_88, 0},
    {"INVENTORY89", INV_89, 0}, {"INVENTORY90", INV_90, 0}, {"INVENTORY91", INV_91, 0}, {"INVENTORY92", INV_92, 0},
    {"INVENTORY93", INV_93, 0}, {"INVENTORY94", INV_94, 0}, {"INVENTORY95", INV_95, 0}, {"INVENTORY96", INV_96, 0},
    {"INVENTORY97", INV_97, 0}, {"INVENTORY98", INV_98, 0}, {"INVENTORY99", INV_99, 0}, {nullptr, 0, 0}};

const specflags_t counter_types[] = {{"LIVES", CT_Lives, 0},     {"SCORE", CT_Score, 0},
                                     {"MONEY", CT_Money, 0},     {"EXPERIENCE", CT_Experience, 0},
                                     {"COUNTER01", CT_Lives, 0}, {"COUNTER02", CT_Score, 0},
                                     {"COUNTER03", CT_Money, 0}, {"COUNTER04", CT_Experience, 0},
                                     {"COUNTER05", COUNT_05, 0}, {"COUNTER06", COUNT_06, 0},
                                     {"COUNTER07", COUNT_07, 0}, {"COUNTER08", COUNT_08, 0},
                                     {"COUNTER09", COUNT_09, 0}, {"COUNTER10", COUNT_10, 0},
                                     {"COUNTER11", COUNT_11, 0}, {"COUNTER12", COUNT_12, 0},
                                     {"COUNTER13", COUNT_13, 0}, {"COUNTER14", COUNT_14, 0},
                                     {"COUNTER15", COUNT_15, 0}, {"COUNTER16", COUNT_16, 0},
                                     {"COUNTER17", COUNT_17, 0}, {"COUNTER18", COUNT_18, 0},
                                     {"COUNTER19", COUNT_19, 0}, {"COUNTER20", COUNT_20, 0},
                                     {"COUNTER21", COUNT_21, 0}, {"COUNTER22", COUNT_22, 0},
                                     {"COUNTER23", COUNT_23, 0}, {"COUNTER24", COUNT_24, 0},
                                     {"COUNTER25", COUNT_25, 0}, {"COUNTER26", COUNT_26, 0},
                                     {"COUNTER27", COUNT_27, 0}, {"COUNTER28", COUNT_28, 0},
                                     {"COUNTER29", COUNT_29, 0}, {"COUNTER30", COUNT_30, 0},
                                     {"COUNTER31", COUNT_31, 0}, {"COUNTER32", COUNT_32, 0},
                                     {"COUNTER33", COUNT_33, 0}, {"COUNTER34", COUNT_34, 0},
                                     {"COUNTER35", COUNT_35, 0}, {"COUNTER36", COUNT_36, 0},
                                     {"COUNTER37", COUNT_37, 0}, {"COUNTER38", COUNT_38, 0},
                                     {"COUNTER39", COUNT_39, 0}, {"COUNTER40", COUNT_40, 0},
                                     {"COUNTER41", COUNT_41, 0}, {"COUNTER42", COUNT_42, 0},
                                     {"COUNTER43", COUNT_43, 0}, {"COUNTER44", COUNT_44, 0},
                                     {"COUNTER45", COUNT_45, 0}, {"COUNTER46", COUNT_46, 0},
                                     {"COUNTER47", COUNT_47, 0}, {"COUNTER48", COUNT_48, 0},
                                     {"COUNTER49", COUNT_49, 0}, {"COUNTER50", COUNT_50, 0},
                                     {"COUNTER51", COUNT_51, 0}, {"COUNTER52", COUNT_52, 0},
                                     {"COUNTER53", COUNT_53, 0}, {"COUNTER54", COUNT_54, 0},
                                     {"COUNTER55", COUNT_55, 0}, {"COUNTER56", COUNT_56, 0},
                                     {"COUNTER57", COUNT_57, 0}, {"COUNTER58", COUNT_58, 0},
                                     {"COUNTER59", COUNT_59, 0}, {"COUNTER60", COUNT_60, 0},
                                     {"COUNTER61", COUNT_61, 0}, {"COUNTER62", COUNT_62, 0},
                                     {"COUNTER63", COUNT_63, 0}, {"COUNTER64", COUNT_64, 0},
                                     {"COUNTER65", COUNT_65, 0}, {"COUNTER66", COUNT_66, 0},
                                     {"COUNTER67", COUNT_67, 0}, {"COUNTER68", COUNT_68, 0},
                                     {"COUNTER69", COUNT_69, 0}, {"COUNTER70", COUNT_70, 0},
                                     {"COUNTER71", COUNT_71, 0}, {"COUNTER72", COUNT_72, 0},
                                     {"COUNTER73", COUNT_73, 0}, {"COUNTER74", COUNT_74, 0},
                                     {"COUNTER75", COUNT_75, 0}, {"COUNTER76", COUNT_76, 0},
                                     {"COUNTER77", COUNT_77, 0}, {"COUNTER78", COUNT_78, 0},
                                     {"COUNTER79", COUNT_79, 0}, {"COUNTER80", COUNT_80, 0},
                                     {"COUNTER81", COUNT_81, 0}, {"COUNTER82", COUNT_82, 0},
                                     {"COUNTER83", COUNT_83, 0}, {"COUNTER84", COUNT_84, 0},
                                     {"COUNTER85", COUNT_85, 0}, {"COUNTER86", COUNT_86, 0},
                                     {"COUNTER87", COUNT_87, 0}, {"COUNTER88", COUNT_88, 0},
                                     {"COUNTER89", COUNT_89, 0}, {"COUNTER90", COUNT_90, 0},
                                     {"COUNTER91", COUNT_91, 0}, {"COUNTER92", COUNT_92, 0},
                                     {"COUNTER93", COUNT_93, 0}, {"COUNTER94", COUNT_94, 0},
                                     {"COUNTER95", COUNT_95, 0}, {"COUNTER96", COUNT_96, 0},
                                     {"COUNTER97", COUNT_97, 0}, {"COUNTER98", COUNT_98, 0},
                                     {"COUNTER99", COUNT_99, 0}, {nullptr, 0, 0}};

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

        if (number > 0)
            dynamic_mobj->number = number;

        DDF_StateBeginRange(dynamic_mobj->state_grp);
        return;
    }

    // replaces an existing entry?
    if (dynamic_mobj)
    {
        dynamic_mobj->Default();
        dynamic_mobj->number = number;
    }
    else
    {
        // not found, create a new one
        dynamic_mobj         = new mobjtype_c;
        dynamic_mobj->name   = name.c_str();
        dynamic_mobj->number = number;

        mobjtypes.push_back(dynamic_mobj);
    }

    DDF_StateBeginRange(dynamic_mobj->state_grp);
}

static void ThingDoTemplate(const char *contents)
{
    int idx = mobjtypes.FindFirst(contents, 0);
    if (idx < 0)
        DDF_Error("Unknown thing template: '%s'\n", contents);

    mobjtype_c *other = mobjtypes[idx];
    SYS_ASSERT(other);

    if (other == dynamic_mobj)
        DDF_Error("Bad thing template: '%s'\n", contents);

    dynamic_mobj->CopyDetail(*other);

    TemplateThing = other->name.c_str();

    DDF_StateBeginRange(dynamic_mobj->state_grp);
}

void ThingParseField(const char *field, const char *contents, int index, bool is_last)
{
#if (DEBUG_DDF)
    I_Debugf("THING_PARSE: %s = %s;\n", field, contents);
#endif

    if (DDF_CompareName(field, "TEMPLATE") == 0)
    {
        ThingDoTemplate(contents);
        return;
    }

    // -AJA- this needs special handling (it touches several fields)
    if (DDF_CompareName(field, "SPECIAL") == 0 || DDF_CompareName(field, "PROJECTILE_SPECIAL") == 0)
    {
        DDF_MobjGetSpecial(contents);
        return;
    }

    // handle the "MODEL_ROTATE" command
    if (DDF_CompareName(field, "MODEL_ROTATE") == 0)
    {
        if (DDF_MainParseField(thing_commands, field, contents, (uint8_t *)dynamic_mobj))
        {
            dynamic_mobj->model_rotate *= kBAMAngle1; // apply the rotation
            return;
        }
    }

    if (DDF_MainParseField(thing_commands, field, contents, (uint8_t *)dynamic_mobj))
        return;

    if (DDF_MainParseState((uint8_t *)dynamic_mobj, dynamic_mobj->state_grp, field, contents, index, is_last,
                           false /* is_weapon */, thing_starters, thing_actions))
        return;

    DDF_WarnError("Unknown thing/attack command: %s\n", field);
}

static void ThingFinishEntry(void)
{
    DDF_StateFinishRange(dynamic_mobj->state_grp);

    // count-as-kill things are automatically monsters
    if (dynamic_mobj->flags & MF_COUNTKILL)
        dynamic_mobj->extendedflags |= EF_MONSTER;

    // countable items are always pick-up-able
    if (dynamic_mobj->flags & MF_COUNTITEM)
        dynamic_mobj->hyperflags |= HF_FORCEPICKUP;

    // shootable things are always pushable
    if (dynamic_mobj->flags & MF_SHOOTABLE)
        dynamic_mobj->hyperflags |= HF_PUSHABLE;

    // check stuff...

    if (dynamic_mobj->mass < 1)
    {
        DDF_WarnError("Bad MASS value %f in DDF.\n", dynamic_mobj->mass);
        dynamic_mobj->mass = 1;
    }

    // check CAST stuff
    if (dynamic_mobj->castorder > 0)
    {
        if (!dynamic_mobj->chase_state)
            DDF_Error("Cast object must have CHASE states !\n");

        if (!dynamic_mobj->death_state)
            DDF_Error("Cast object must have DEATH states !\n");
    }

    // check DAMAGE stuff
    if (dynamic_mobj->explode_damage.nominal < 0)
    {
        DDF_WarnError("Bad EXPLODE_DAMAGE.VAL value %f in DDF.\n", dynamic_mobj->explode_damage.nominal);
    }

    if (dynamic_mobj->explode_radius < 0)
    {
        DDF_Error("Bad EXPLODE_RADIUS value %f in DDF.\n", dynamic_mobj->explode_radius);
    }

    if (dynamic_mobj->reload_shots <= 0)
    {
        DDF_Error("Bad RELOAD_SHOTS value %d in DDF.\n", dynamic_mobj->reload_shots);
    }

    if (dynamic_mobj->choke_damage.nominal < 0)
    {
        DDF_WarnError("Bad CHOKE_DAMAGE.VAL value %f in DDF.\n", dynamic_mobj->choke_damage.nominal);
    }

    if (dynamic_mobj->model_skin < 0 || dynamic_mobj->model_skin > 9)
        DDF_Error("Bad MODEL_SKIN value %d in DDF (must be 0-9).\n", dynamic_mobj->model_skin);

    if (dynamic_mobj->dlight[0].radius > 512)
    {
        if (dlight_radius_warnings < 3)
            DDF_Warning("DLIGHT_RADIUS value %1.1f too large (over 512).\n", dynamic_mobj->dlight[0].radius);
        else if (dlight_radius_warnings == 3)
            I_Warning("More too large DLIGHT_RADIUS values found....\n");

        dlight_radius_warnings++;
    }

    // FIXME: check more stuff

    // backwards compatibility: if no idle state, re-use spawn state
    if (dynamic_mobj->idle_state == 0)
        dynamic_mobj->idle_state = dynamic_mobj->spawn_state;

    dynamic_mobj->DLightCompatibility();

    if (TemplateThing)
    {
        int idx = mobjtypes.FindFirst(TemplateThing, 0);
        if (idx < 0)
            DDF_Error("Unknown thing template: \n");

        mobjtype_c *other = mobjtypes[idx];

        if (!dynamic_mobj->lose_benefits)
        {
            if (other->lose_benefits)
            {
                dynamic_mobj->lose_benefits  = new benefit_t;
                *dynamic_mobj->lose_benefits = *other->lose_benefits;
            }
        }

        if (!dynamic_mobj->pickup_benefits)
        {
            if (other->pickup_benefits)
            {
                dynamic_mobj->pickup_benefits  = new benefit_t;
                *dynamic_mobj->pickup_benefits = *other->pickup_benefits;
            }
        }

        if (!dynamic_mobj->kill_benefits)
        {
            if (other->kill_benefits)
            {
                dynamic_mobj->kill_benefits  = new benefit_t;
                *dynamic_mobj->kill_benefits = *other->kill_benefits;
            }
        }

        if (dynamic_mobj->pickup_message.empty())
        {
            dynamic_mobj->pickup_message = other->pickup_message;
        }
    }
    TemplateThing = nullptr;
}

static void ThingClearAll(void)
{
    I_Warning("Ignoring #CLEARALL in things.ddf\n");
}

void DDF_ReadThings(const std::string &data)
{
    readinfo_t things;

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
    for (auto m : mobjtypes)
    {
        delete m;
        m = nullptr;
    }
    mobjtypes.clear();

    default_mobjtype         = new mobjtype_c();
    default_mobjtype->name   = "__DEFAULT_MOBJ";
    default_mobjtype->number = 0;
}

void DDF_MobjCleanUp(void)
{
    // lookup references
    for (auto m : mobjtypes)
    {
        cur_ddf_entryname = epi::StringFormat("[%s]  (things.ddf)", m->name.c_str());

        m->dropitem = m->dropitem_ref != "" ? mobjtypes.Lookup(m->dropitem_ref.c_str()) : nullptr;
        m->blood    = m->blood_ref != "" ? mobjtypes.Lookup(m->blood_ref.c_str()) : mobjtypes.Lookup("BLOOD");

        m->respawneffect = m->respawneffect_ref != "" ? mobjtypes.Lookup(m->respawneffect_ref.c_str())
                           : (m->flags & MF_SPECIAL)  ? mobjtypes.Lookup("ITEM_RESPAWN")
                                                      : mobjtypes.Lookup("RESPAWN_FLASH");

        m->spitspot = m->spitspot_ref != "" ? mobjtypes.Lookup(m->spitspot_ref.c_str()) : nullptr;

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

static bool BenefitTryCounter(const char *name, benefit_t *be, int num_vals)
{
    if (CHKF_Positive != DDF_MainCheckSpecialFlag(name, counter_types, &be->sub.type, false, false))
    {
        return false;
    }

    be->type = BENEFIT_Counter;

    if (num_vals < 1)
    {
        DDF_WarnError("Counter benefit used, but amount is missing.\n");
        return false;
    }

    if (num_vals < 2)
    {
        be->limit = be->amount;
    }

    return true;
}

static bool BenefitTryCounterLimit(const char *name, benefit_t *be, int num_vals)
{
    char   namebuf[200];
    size_t len = strlen(name);

    // check for ".LIMIT" prefix
    if (len < 7 || DDF_CompareName(name + len - 6, ".LIMIT") != 0)
        return false;

    len -= 6;
    epi::CStringCopyMax(namebuf, name, len);

    if (CHKF_Positive != DDF_MainCheckSpecialFlag(namebuf, counter_types, &be->sub.type, false, false))
    {
        return false;
    }

    be->type  = BENEFIT_CounterLimit;
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

static bool BenefitTryInventory(const char *name, benefit_t *be, int num_vals)
{
    if (CHKF_Positive != DDF_MainCheckSpecialFlag(name, inv_types, &be->sub.type, false, false))
    {
        return false;
    }

    be->type = BENEFIT_Inventory;

    if (num_vals < 1)
    {
        DDF_WarnError("Inventory benefit used, but amount is missing.\n");
        return false;
    }

    if (num_vals < 2)
    {
        be->limit = be->amount;
    }

    return true;
}

static bool BenefitTryInventoryLimit(const char *name, benefit_t *be, int num_vals)
{
    char namebuf[200];
    int  len = strlen(name);

    // check for ".LIMIT" prefix
    if (len < 7 || DDF_CompareName(name + len - 6, ".LIMIT") != 0)
        return false;

    len -= 6;
    epi::CStringCopyMax(namebuf, name, len);

    if (CHKF_Positive != DDF_MainCheckSpecialFlag(namebuf, inv_types, &be->sub.type, false, false))
    {
        return false;
    }

    be->type  = BENEFIT_InventoryLimit;
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

static bool BenefitTryAmmo(const char *name, benefit_t *be, int num_vals)
{
    if (CHKF_Positive != DDF_MainCheckSpecialFlag(name, ammo_types, &be->sub.type, false, false))
    {
        return false;
    }

    be->type = BENEFIT_Ammo;

    if ((ammotype_e)be->sub.type == AM_NoAmmo)
    {
        DDF_WarnError("Illegal ammo benefit: %s\n", name);
        return false;
    }

    if (num_vals < 1)
    {
        DDF_WarnError("Ammo benefit used, but amount is missing.\n");
        return false;
    }

    if (num_vals < 2)
    {
        be->limit = be->amount;
    }

    return true;
}

static bool BenefitTryAmmoLimit(const char *name, benefit_t *be, int num_vals)
{
    char   namebuf[200];
    size_t len = strlen(name);

    // check for ".LIMIT" prefix

    if (len < 7 || DDF_CompareName(name + len - 6, ".LIMIT") != 0)
        return false;

    len -= 6;
    epi::CStringCopyMax(namebuf, name, len);

    if (CHKF_Positive != DDF_MainCheckSpecialFlag(namebuf, ammo_types, &be->sub.type, false, false))
    {
        return false;
    }

    be->type  = BENEFIT_AmmoLimit;
    be->limit = 0;

    if (be->sub.type == AM_NoAmmo)
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

static bool BenefitTryWeapon(const char *name, benefit_t *be, int num_vals)
{
    int idx = weapondefs.FindFirst(name, 0);

    if (idx < 0)
        return false;

    be->sub.weap = weapondefs[idx];

    be->type  = BENEFIT_Weapon;
    be->limit = 1.0f;

    if (num_vals < 1)
        be->amount = 1.0f;
    else if (be->amount != 0.0f && be->amount != 1.0f)
    {
        DDF_WarnError("Weapon benefit used, bad amount value: %1.1f\n", be->amount);
        return false;
    }

    if (num_vals > 1)
    {
        DDF_WarnError("Weapon benefit cannot have a limit value.\n");
        return false;
    }

    return true;
}

static bool BenefitTryKey(const char *name, benefit_t *be, int num_vals)
{
    if (CHKF_Positive != DDF_MainCheckSpecialFlag(name, keytype_names, &be->sub.type, false, false))
    {
        return false;
    }

    be->type  = BENEFIT_Key;
    be->limit = 1.0f;

    if (num_vals < 1)
        be->amount = 1.0f;
    else if (be->amount != 0.0f && be->amount != 1.0f)
    {
        DDF_WarnError("Key benefit used, bad amount value: %1.1f\n", be->amount);
        return false;
    }

    if (num_vals > 1)
    {
        DDF_WarnError("Key benefit cannot have a limit value.\n");
        return false;
    }

    return true;
}

static bool BenefitTryHealth(const char *name, benefit_t *be, int num_vals)
{
    if (DDF_CompareName(name, "HEALTH") != 0)
        return false;

    be->type     = BENEFIT_Health;
    be->sub.type = 0;

    if (num_vals < 1)
    {
        DDF_WarnError("Health benefit used, but amount is missing.\n");
        return false;
    }

    if (num_vals < 2)
        be->limit = 100.0f;

    return true;
}

static bool BenefitTryArmour(const char *name, benefit_t *be, int num_vals)
{
    if (CHKF_Positive != DDF_MainCheckSpecialFlag(name, armourtype_names, &be->sub.type, false, false))
    {
        return false;
    }

    be->type = BENEFIT_Armour;

    if (num_vals < 1)
    {
        DDF_WarnError("Armour benefit used, but amount is missing.\n");
        return false;
    }

    if (num_vals < 2)
    {
        switch (be->sub.type)
        {
        case ARMOUR_Green:
            be->limit = 100;
            break;
        case ARMOUR_Blue:
            be->limit = 200;
            break;
        case ARMOUR_Purple:
            be->limit = 200;
            break;
        case ARMOUR_Yellow:
            be->limit = 200;
            break;
        case ARMOUR_Red:
            be->limit = 200;
            break;
        default:;
        }
    }

    return true;
}

static bool BenefitTryPowerup(const char *name, benefit_t *be, int num_vals)
{
    if (CHKF_Positive != DDF_MainCheckSpecialFlag(name, powertype_names, &be->sub.type, false, false))
    {
        return false;
    }

    be->type = BENEFIT_Powerup;

    if (num_vals < 1)
        be->amount = 999999.0f;

    if (num_vals < 2)
        be->limit = 999999.0f;

    // -AJA- backwards compatibility (need Fist for Berserk)
    if (be->sub.type == PW_Berserk && DDF_CompareName(name, "POWERUP_BERSERK") == 0)
    {
        int idx = weapondefs.FindFirst("FIST", 0);

        if (idx >= 0)
        {
            AddPickupEffect(&dynamic_mobj->pickup_effects,
                            new pickup_effect_c(PUFX_SwitchWeapon, weapondefs[idx], 0, 0));

            AddPickupEffect(&dynamic_mobj->pickup_effects, new pickup_effect_c(PUFX_KeepPowerup, PW_Berserk, 0, 0));
        }
    }

    return true;
}

static void BenefitAdd(benefit_t **list, benefit_t *source)
{
    benefit_t *cur, *tail;

    // check if this benefit overrides a previous one
    for (cur = (*list); cur; cur = cur->next)
    {
        if (cur->type == BENEFIT_Weapon)
            continue;

        if (cur->type == source->type && cur->sub.type == source->sub.type)
        {
            cur->amount = source->amount;
            cur->limit  = source->limit;
            return;
        }
    }

    // nope, create a new one and link it onto the _TAIL_
    cur = new benefit_t;

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

    benefit_t temp;

    SYS_ASSERT(storage);

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
        BenefitAdd((benefit_t **)storage, &temp);
        return;
    }

    DDF_WarnError("Unknown/Malformed benefit type: %s\n", namebuf);
}

pickup_effect_c::pickup_effect_c(pickup_effect_type_e _type, int _sub, int _slot, float _time)
    : next(nullptr), type(_type), slot(_slot), time(_time)
{
    sub.type = _sub;
}

pickup_effect_c::pickup_effect_c(pickup_effect_type_e _type, weapondef_c *_weap, int _slot, float _time)
    : next(nullptr), type(_type), slot(_slot), time(_time)
{
    sub.weap = _weap;
}

static void AddPickupEffect(pickup_effect_c **list, pickup_effect_c *cur)
{
    cur->next = nullptr;

    if ((*list) == nullptr)
    {
        (*list) = cur;
        return;
    }

    pickup_effect_c *tail;

    for (tail = (*list); tail && tail->next; tail = tail->next)
    {
    }

    tail->next = cur;
}

void BA_ParsePowerupEffect(pickup_effect_c **list, int pnum, float par1, float par2, const char *word_par)
{
    int p_up = (int)par1;
    int slot = (int)par2;

    SYS_ASSERT(0 <= p_up && p_up < NUMPOWERS);

    if (slot < 0 || slot >= NUM_FX_SLOT)
        DDF_Error("POWERUP_EFFECT: bad FX slot #%d\n", p_up);

    AddPickupEffect(list, new pickup_effect_c(PUFX_PowerupEffect, p_up, slot, 0));
}

void BA_ParseScreenEffect(pickup_effect_c **list, int pnum, float par1, float par2, const char *word_par)
{
    int slot = (int)par1;

    if (slot < 0 || slot >= NUM_FX_SLOT)
        DDF_Error("SCREEN_EFFECT: bad FX slot #%d\n", slot);

    if (par2 <= 0)
        DDF_Error("SCREEN_EFFECT: bad time value: %1.2f\n", par2);

    AddPickupEffect(list, new pickup_effect_c(PUFX_ScreenEffect, 0, slot, par2));
}

void BA_ParseSwitchWeapon(pickup_effect_c **list, int pnum, float par1, float par2, const char *word_par)
{
    if (pnum != -1)
        DDF_Error("SWITCH_WEAPON: missing weapon name !\n");

    SYS_ASSERT(word_par && word_par[0]);

    weapondef_c *weap = weapondefs.Lookup(word_par);

    AddPickupEffect(list, new pickup_effect_c(PUFX_SwitchWeapon, weap, 0, 0));
}

void BA_ParseKeepPowerup(pickup_effect_c **list, int pnum, float par1, float par2, const char *word_par)
{
    if (pnum != -1)
        DDF_Error("KEEP_POWERUP: missing powerup name !\n");

    SYS_ASSERT(word_par && word_par[0]);

    if (DDF_CompareName(word_par, "BERSERK") != 0)
        DDF_Error("KEEP_POWERUP: %s is not supported\n", word_par);

    AddPickupEffect(list, new pickup_effect_c(PUFX_KeepPowerup, PW_Berserk, 0, 0));
}

typedef struct pick_fx_parser_s
{
    const char *name;
    int         num_pars; // -1 means a single word
    void (*parser)(pickup_effect_c **list, int pnum, float par1, float par2, const char *word_par);
} pick_fx_parser_t;

static const pick_fx_parser_t pick_fx_parsers[] = {{"SCREEN_EFFECT", 2, BA_ParseScreenEffect},
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

    pickup_effect_c **fx_list = (pickup_effect_c **)storage;

    benefit_t temp; // FIXME kludge (write new parser method ?)

    num_vals = ParseBenefitString(info, namebuf, parambuf, &temp.amount, &temp.limit);

    // an error occurred ?
    if (num_vals < 0)
        return;

    if (parambuf[0])
        num_vals = -1;

    for (int i = 0; pick_fx_parsers[i].name; i++)
    {
        if (DDF_CompareName(pick_fx_parsers[i].name, namebuf) != 0)
            continue;

        (*pick_fx_parsers[i].parser)(fx_list, num_vals, temp.amount, temp.limit, parambuf);

        return;
    }

    // secondly, try the powerups
    for (int p = 0; powertype_names[p].name; p++)
    {
        if (DDF_CompareName(powertype_names[p].name, namebuf) != 0)
            continue;

        BA_ParsePowerupEffect(fx_list, num_vals, p, temp.amount, parambuf);

        return;
    }

    DDF_Error("Unknown/Malformed benefit effect: %s\n", namebuf);
}

// -KM- 1998/11/25 Translucency to fractional.
// -KM- 1998/12/16 Added individual flags for all.
// -AJA- 2000/02/02: Split into two lists.

static const specflags_t normal_specials[] = {{"AMBUSH", MF_AMBUSH, 0},
                                              {"FUZZY", MF_FUZZY, 0},
                                              {"SOLID", MF_SOLID, 0},
                                              {"ON_CEILING", MF_SPAWNCEILING + MF_NOGRAVITY, 0},
                                              {"FLOATER", MF_FLOAT + MF_NOGRAVITY, 0},
                                              {"INERT", MF_NOBLOCKMAP, 0},
                                              {"TELEPORT_TYPE", MF_NOGRAVITY, 0},
                                              {"LINKS", MF_NOBLOCKMAP + MF_NOSECTOR, 1},
                                              {"DAMAGESMOKE", MF_NOBLOOD, 0},
                                              {"SHOOTABLE", MF_SHOOTABLE, 0},
                                              {"COUNT_AS_KILL", MF_COUNTKILL, 0},
                                              {"COUNT_AS_ITEM", MF_COUNTITEM, 0},
                                              {"SKULLFLY", MF_SKULLFLY, 0},
                                              {"SPECIAL", MF_SPECIAL, 0},
                                              {"SECTOR", MF_NOSECTOR, 1},
                                              {"BLOCKMAP", MF_NOBLOCKMAP, 1},
                                              {"SPAWNCEILING", MF_SPAWNCEILING, 0},
                                              {"GRAVITY", MF_NOGRAVITY, 1},
                                              {"DROPOFF", MF_DROPOFF, 0},
                                              {"PICKUP", MF_PICKUP, 0},
                                              {"CLIP", MF_NOCLIP, 1},
                                              {"SLIDER", MF_SLIDE, 0},
                                              {"FLOAT", MF_FLOAT, 0},
                                              {"TELEPORT", MF_TELEPORT, 0},
                                              {"MISSILE", MF_MISSILE, 0}, // has a special check
                                              {"BARE_MISSILE", MF_MISSILE, 0},
                                              {"DROPPED", MF_DROPPED, 0},
                                              {"CORPSE", MF_CORPSE, 0},
                                              {"STEALTH", MF_STEALTH, 0},
                                              {"PRESERVE_MOMENTUM", MF_PRESERVEMOMENTUM, 0},
                                              {"DEATHMATCH", MF_NOTDMATCH, 1},
                                              {"TOUCHY", MF_TOUCHY, 0},
                                              {nullptr, 0, 0}};

static specflags_t extended_specials[] = {{"RESPAWN", EF_NORESPAWN, 1},
                                          {"RESURRECT", EF_NORESURRECT, 1},
                                          {"DISLOYAL", EF_DISLOYALTYPE, 0},
                                          {"TRIGGER_HAPPY", EF_TRIGGERHAPPY, 0},
                                          {"ATTACK_HURTS", EF_OWNATTACKHURTS, 0},
                                          {"EXPLODE_IMMUNE", EF_EXPLODEIMMUNE, 0},
                                          {"ALWAYS_LOUD", EF_ALWAYSLOUD, 0},
                                          {"BOSSMAN", EF_EXPLODEIMMUNE + EF_ALWAYSLOUD, 0},
                                          {"NEVERTARGETED", EF_NEVERTARGET, 0},
                                          {"GRAV_KILL", EF_NOGRAVKILL, 1},
                                          {"GRUDGE", EF_NOGRUDGE, 1},
                                          {"BOUNCE", EF_BOUNCE, 0},
                                          {"EDGEWALKER", EF_EDGEWALKER, 0},
                                          {"GRAVFALL", EF_GRAVFALL, 0},
                                          {"CLIMBABLE", EF_CLIMBABLE, 0},
                                          {"WATERWALKER", EF_WATERWALKER, 0},
                                          {"MONSTER", EF_MONSTER, 0},
                                          {"CROSSLINES", EF_CROSSLINES, 0},
                                          {"FRICTION", EF_NOFRICTION, 1},
                                          {"USABLE", EF_USABLE, 0},
                                          {"BLOCK_SHOTS", EF_BLOCKSHOTS, 0},
                                          {"TUNNEL", EF_TUNNEL, 0},
                                          {"SIMPLE_ARMOUR", EF_SIMPLEARMOUR, 0},
                                          {nullptr, 0, 0}};

static specflags_t hyper_specials[] = {{"FORCE_PICKUP", HF_FORCEPICKUP, 0},
                                       {"SIDE_IMMUNE", HF_SIDEIMMUNE, 0},
                                       {"SIDE_GHOST", HF_SIDEGHOST, 0},
                                       {"ULTRA_LOYAL", HF_ULTRALOYAL, 0},
                                       {"ZBUFFER", HF_NOZBUFFER, 1},
                                       {"HOVER", HF_HOVER, 0},
                                       {"PUSHABLE", HF_PUSHABLE, 0},
                                       {"POINT_FORCE", HF_POINT_FORCE, 0},
                                       {"PASS_MISSILE", HF_PASSMISSILE, 0},
                                       {"INVULNERABLE", HF_INVULNERABLE, 0},
                                       {"VAMPIRE", HF_VAMPIRE, 0},
                                       {"AUTOAIM", HF_NO_AUTOAIM, 1},
                                       {"TILT", HF_TILT, 0},
                                       {"IMMORTAL", HF_IMMORTAL, 0},
                                       {"FLOOR_CLIP", HF_FLOORCLIP, 0},         // Lobo: new FLOOR_CLIP flag
                                       {"TRIGGER_LINES", HF_NOTRIGGERLINES, 1}, // Lobo: Cannot activate doors etc.
                                       {"SHOVEABLE", HF_SHOVEABLE, 0},          // Lobo: can be pushed
                                       {"SPLASH", HF_NOSPLASH, 1},              // Lobo: causes no splash on liquids
                                       {"DEHACKED_COMPAT", HF_DEHACKED_COMPAT, 0},
                                       {"IMMOVABLE", HF_IMMOVABLE, 0},
                                       {"MUSIC_CHANGER", HF_MUSIC_CHANGER, 0},
                                       {nullptr, 0, 0}};

static specflags_t mbf21_specials[] = {{"LOGRAV", MBF21_LOGRAV, 0}, {nullptr, 0, 0}};

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
        dynamic_mobj->translucency = PERCENT_MAKE(0);
        return;
    }

    // handle the "NOSHADOW" tag
    if (DDF_CompareName(info, "NOSHADOW") == 0)
    {
        dynamic_mobj->shadow_trans = PERCENT_MAKE(0);
        return;
    }

    // the "MISSILE" tag needs special treatment, since it sets both
    // normal flags & extended flags.
    if (DDF_CompareName(info, "MISSILE") == 0)
    {
        dynamic_mobj->flags |= MF_MISSILE;
        dynamic_mobj->extendedflags |= EF_CROSSLINES | EF_NOFRICTION;
        return;
    }

    int *flag_ptr = &dynamic_mobj->flags;

    checkflag_result_e res = DDF_MainCheckSpecialFlag(info, normal_specials, &flag_value, true, false);

    if (res == CHKF_User || res == CHKF_Unknown)
    {
        // wasn't a normal special.  Try the extended ones...
        flag_ptr = &dynamic_mobj->extendedflags;

        res = DDF_MainCheckSpecialFlag(info, extended_specials, &flag_value, true, false);
    }

    if (res == CHKF_User || res == CHKF_Unknown)
    {
        // -AJA- 2004/08/25: Try the hyper specials...
        flag_ptr = &dynamic_mobj->hyperflags;

        res = DDF_MainCheckSpecialFlag(info, hyper_specials, &flag_value, true, false);
    }

    if (res == CHKF_User || res == CHKF_Unknown)
    {
        // Try the MBF21 specials...
        flag_ptr = &dynamic_mobj->mbf21flags;

        res = DDF_MainCheckSpecialFlag(info, mbf21_specials, &flag_value, true, false);
    }

    switch (res)
    {
    case CHKF_Positive:
        *flag_ptr |= flag_value;
        break;

    case CHKF_Negative:
        *flag_ptr &= ~flag_value;
        break;

    case CHKF_User:
    case CHKF_Unknown:
        DDF_WarnError("DDF_MobjGetSpecial: Unknown special '%s'\n", info);
        break;
    }
}

static specflags_t dlight_type_names[] = {{"NONE", DLITE_None, 0},
                                          {"MODULATE", DLITE_Modulate, 0},
                                          {"ADD", DLITE_Add, 0},

                                          // backwards compatibility
                                          {"LINEAR", DLITE_Compat_LIN, 0},
                                          {"QUADRATIC", DLITE_Compat_QUAD, 0},
                                          {"CONSTANT", DLITE_Compat_LIN, 0},

                                          {nullptr, 0, 0}};

//
// DDF_MobjGetDLight
//
void DDF_MobjGetDLight(const char *info, void *storage)
{
    dlight_type_e *dtype = (dlight_type_e *)storage;
    int            flag_value;

    SYS_ASSERT(dtype);

    if (CHKF_Positive != DDF_MainCheckSpecialFlag(info, dlight_type_names, &flag_value, false, false))
    {
        DDF_WarnError("Unknown dlight type '%s'\n", info);
        return;
    }

    (*dtype) = (dlight_type_e)flag_value;
}

//
// DDF_MobjGetExtra
//
void DDF_MobjGetExtra(const char *info, void *storage)
{
    int *extendedflags = (int *)storage;

    // If keyword is "nullptr", then the mobj is not marked as extra.
    // Otherwise it is.

    if (DDF_CompareName(info, "nullptr") == 0)
    {
        *extendedflags &= ~EF_EXTRA;
    }
    else
    {
        *extendedflags |= EF_EXTRA;
    }
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

    if (*dest > 32)
        DDF_Warning("Player number '%d' will not work.", *dest);
}

static void DDF_MobjGetGlowType(const char *info, void *storage)
{
    glow_sector_type_e *glow = (glow_sector_type_e *)storage;

    if (epi::StringCaseCompareASCII(info, "FLOOR") == 0)
        *glow = GLOW_Floor;
    else if (epi::StringCaseCompareASCII(info, "CEILING") == 0)
        *glow = GLOW_Ceiling;
    else if (epi::StringCaseCompareASCII(info, "WALL") == 0)
        *glow = GLOW_Wall;
    else // Unknown/None
        *glow = GLOW_None;
}

static const specflags_t sprite_yalign_names[] = {{"BOTTOM", SPYA_BottomUp, 0},
                                                  {"MIDDLE", SPYA_Middle, 0},
                                                  {"TOP", SPYA_TopDown, 0},

                                                  {nullptr, 0, 0}};

static void DDF_MobjGetYAlign(const char *info, void *storage)
{
    if (CHKF_Positive != DDF_MainCheckSpecialFlag(info, sprite_yalign_names, (int *)storage, false, false))
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
static void DDF_MobjStateGetRADTrigger(const char *arg, state_t *cur_state)
{
    if (!arg || !arg[0])
        return;

    int *val_ptr = new int;

    // Modified RAD_CheckForInt
    const char *pos    = arg;
    int         count  = 0;
    int         length = strlen(arg);

    while (epi::IsDigitASCII(*pos++))
        count++;

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

static bool ConditionTryCounter(const char *name, const char *sub, condition_check_t *cond)
{
    if (CHKF_Positive != DDF_MainCheckSpecialFlag(name, counter_types, &cond->sub.type, false, false))
    {
        return false;
    }

    if (sub[0])
        sscanf(sub, " %f ", &cond->amount);

    cond->cond_type = COND_Counter;
    return true;
}

static bool ConditionTryInventory(const char *name, const char *sub, condition_check_t *cond)
{
    if (CHKF_Positive != DDF_MainCheckSpecialFlag(name, inv_types, &cond->sub.type, false, false))
    {
        return false;
    }

    if (sub[0])
        sscanf(sub, " %f ", &cond->amount);

    cond->cond_type = COND_Inventory;
    return true;
}

static bool ConditionTryAmmo(const char *name, const char *sub, condition_check_t *cond)
{
    if (CHKF_Positive != DDF_MainCheckSpecialFlag(name, ammo_types, &cond->sub.type, false, false))
    {
        return false;
    }

    if ((ammotype_e)cond->sub.type == AM_NoAmmo)
    {
        DDF_WarnError("Illegal ammo in condition: %s\n", name);
        return false;
    }

    if (sub[0])
        sscanf(sub, " %f ", &cond->amount);

    cond->cond_type = COND_Ammo;
    return true;
}

static bool ConditionTryWeapon(const char *name, const char *sub, condition_check_t *cond)
{
    int idx = weapondefs.FindFirst(name, 0);

    if (idx < 0)
        return false;

    cond->sub.weap = weapondefs[idx];

    cond->cond_type = COND_Weapon;
    return true;
}

static bool ConditionTryKey(const char *name, const char *sub, condition_check_t *cond)
{
    if (CHKF_Positive != DDF_MainCheckSpecialFlag(name, keytype_names, &cond->sub.type, false, false))
    {
        return false;
    }

    cond->cond_type = COND_Key;
    return true;
}

static bool ConditionTryHealth(const char *name, const char *sub, condition_check_t *cond)
{
    if (DDF_CompareName(name, "HEALTH") != 0)
        return false;

    if (sub[0])
        sscanf(sub, " %f ", &cond->amount);

    cond->cond_type = COND_Health;
    return true;
}

static bool ConditionTryArmour(const char *name, const char *sub, condition_check_t *cond)
{
    if (DDF_CompareName(name, "ARMOUR") == 0)
    {
        cond->sub.type = ARMOUR_Total;
    }
    else if (CHKF_Positive != DDF_MainCheckSpecialFlag(name, armourtype_names, &cond->sub.type, false, false))
    {
        return false;
    }

    if (sub[0])
        sscanf(sub, " %f ", &cond->amount);

    cond->cond_type = COND_Armour;
    return true;
}

static bool ConditionTryPowerup(const char *name, const char *sub, condition_check_t *cond)
{
    if (CHKF_Positive != DDF_MainCheckSpecialFlag(name, powertype_names, &cond->sub.type, false, false))
    {
        return false;
    }

    if (sub[0])
    {
        sscanf(sub, " %f ", &cond->amount);

        cond->amount *= (float)TICRATE;
    }

    cond->cond_type = COND_Powerup;
    return true;
}

static bool ConditionTryPlayerState(const char *name, const char *sub, condition_check_t *cond)
{
    return (CHKF_Positive == DDF_MainCheckSpecialFlag(name, simplecond_names, (int *)&cond->cond_type, false, false));
}

//
// DDF_MainParseCondition
//
// Returns `false' if parsing failed.
//
bool DDF_MainParseCondition(const char *info, condition_check_t *cond)
{
    char typebuf[100];
    char sub_buf[100];

    int         len = strlen(info);
    int         t_off;
    const char *pos;

    cond->negate    = false;
    cond->exact     = false;
    cond->cond_type = COND_NONE;
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

    if (ConditionTryAmmo(typebuf + t_off, sub_buf, cond) || ConditionTryInventory(typebuf + t_off, sub_buf, cond) ||
        ConditionTryCounter(typebuf + t_off, sub_buf, cond) || ConditionTryWeapon(typebuf + t_off, sub_buf, cond) ||
        ConditionTryKey(typebuf + t_off, sub_buf, cond) || ConditionTryHealth(typebuf + t_off, sub_buf, cond) ||
        ConditionTryArmour(typebuf + t_off, sub_buf, cond) || ConditionTryPowerup(typebuf + t_off, sub_buf, cond) ||
        ConditionTryPlayerState(typebuf + t_off, sub_buf, cond))
    {
        return true;
    }

    DDF_WarnError("Unknown/Malformed condition type: %s\n", typebuf);
    return false;
}

// ---> mobjdef class

mobjtype_c::mobjtype_c() : name(), state_grp()
{
    Default();
}

mobjtype_c::~mobjtype_c()
{
}

void mobjtype_c::CopyDetail(mobjtype_c &src)
{
    state_grp.clear();

    for (unsigned int i = 0; i < src.state_grp.size(); i++)
        state_grp.push_back(src.state_grp[i]);

    spawn_state    = src.spawn_state;
    idle_state     = src.idle_state;
    chase_state    = src.chase_state;
    pain_state     = src.pain_state;
    missile_state  = src.missile_state;
    melee_state    = src.melee_state;
    death_state    = src.death_state;
    overkill_state = src.overkill_state;
    raise_state    = src.raise_state;
    res_state      = src.res_state;
    meander_state  = src.meander_state;
    morph_state    = src.morph_state;
    bounce_state   = src.bounce_state;
    touch_state    = src.touch_state;
    reload_state   = src.reload_state;
    gib_state      = src.gib_state;

    reactiontime = src.reactiontime;
    painchance   = src.painchance;
    spawnhealth  = src.spawnhealth;
    speed        = src.speed;
    float_speed  = src.float_speed;
    radius       = src.radius;
    height       = src.height;
    step_size    = src.step_size;
    mass         = src.mass;

    flags         = src.flags;
    extendedflags = src.extendedflags;
    hyperflags    = src.hyperflags;
    mbf21flags    = src.mbf21flags;

    explode_damage = src.explode_damage;
    explode_radius = src.explode_radius;

    // pickup_message = src.pickup_message;
    // lose_benefits = src.lose_benefits;
    // pickup_benefits = src.pickup_benefits;
    if (src.pickup_message != "")
    {
        pickup_message = src.pickup_message;
    }

    lose_benefits   = nullptr;
    pickup_benefits = nullptr;
    kill_benefits   = nullptr; // I think? - Dasho
    /*
    if(src.pickup_benefits)
    {
        I_Debugf("%s: Benefits info not inherited from '%s', ",name, src.name.c_str());
        I_Debugf("You should define it explicitly.\n");
    }
    */

    pickup_effects   = src.pickup_effects;
    initial_benefits = src.initial_benefits;

    castorder    = src.castorder;
    cast_title   = src.cast_title;
    respawntime  = src.respawntime;
    translucency = src.translucency;
    minatkchance = src.minatkchance;
    palremap     = src.palremap;

    jump_delay   = src.jump_delay;
    jumpheight   = src.jumpheight;
    crouchheight = src.crouchheight;
    viewheight   = src.viewheight;
    shotheight   = src.shotheight;
    maxfall      = src.maxfall;
    fast         = src.fast;

    scale  = src.scale;
    aspect = src.aspect;
    yalign = src.yalign;

    model_skin   = src.model_skin;
    model_scale  = src.model_scale;
    model_aspect = src.model_aspect;
    model_bias   = src.model_bias;
    model_rotate = src.model_rotate;

    bounce_speed  = src.bounce_speed;
    bounce_up     = src.bounce_up;
    sight_slope   = src.sight_slope;
    sight_angle   = src.sight_angle;
    ride_friction = src.ride_friction;
    shadow_trans  = src.shadow_trans;
    glow_type     = src.glow_type;

    seesound       = src.seesound;
    attacksound    = src.attacksound;
    painsound      = src.painsound;
    deathsound     = src.deathsound;
    overkill_sound = src.overkill_sound;
    activesound    = src.activesound;
    walksound      = src.walksound;
    jump_sound     = src.jump_sound;
    noway_sound    = src.noway_sound;
    oof_sound      = src.oof_sound;
    fallpain_sound = src.fallpain_sound;
    gasp_sound     = src.gasp_sound;
    secretsound    = src.secretsound;
    falling_sound  = src.falling_sound;
    rip_sound      = src.rip_sound;

    fuse           = src.fuse;
    reload_shots   = src.reload_shots;
    armour_protect = src.armour_protect;
    armour_deplete = src.armour_deplete;
    armour_class   = src.armour_class;

    side          = src.side;
    playernum     = src.playernum;
    lung_capacity = src.lung_capacity;
    gasp_start    = src.gasp_start;

    // choke_damage
    choke_damage = src.choke_damage;

    bobbing           = src.bobbing;
    immunity          = src.immunity;
    resistance        = src.resistance;
    resist_multiply   = src.resist_multiply;
    resist_painchance = src.resist_painchance;
    ghost             = src.ghost;

    closecombat = src.closecombat;
    rangeattack = src.rangeattack;
    spareattack = src.spareattack;

    // dynamic light info
    dlight[0] = src.dlight[0];
    dlight[1] = src.dlight[1];

    weak = src.weak;

    dropitem          = src.dropitem;
    dropitem_ref      = src.dropitem_ref;
    blood             = src.blood;
    blood_ref         = src.blood_ref;
    respawneffect     = src.respawneffect;
    respawneffect_ref = src.respawneffect_ref;
    spitspot          = src.spitspot;
    spitspot_ref      = src.spitspot_ref;

    sight_distance = src.sight_distance;
    hear_distance  = src.hear_distance;

    morphtimeout = src.morphtimeout;

    gib_health = src.gib_health;

    infight_group = src.infight_group;
    proj_group    = src.proj_group;
    splash_group  = src.splash_group;
    fast_speed    = src.fast_speed;
    melee_range   = src.melee_range;
}

void mobjtype_c::Default()
{
    state_grp.clear();

    spawn_state    = 0;
    idle_state     = 0;
    chase_state    = 0;
    pain_state     = 0;
    missile_state  = 0;
    melee_state    = 0;
    death_state    = 0;
    overkill_state = 0;
    raise_state    = 0;
    res_state      = 0;
    meander_state  = 0;
    morph_state    = 0;
    bounce_state   = 0;
    touch_state    = 0;
    reload_state   = 0;
    gib_state      = 0;

    reactiontime = 0;
    painchance   = PERCENT_MAKE(0);
    spawnhealth  = 1000.0f;
    speed        = 0;
    float_speed  = 2.0f;
    radius       = 0;
    height       = 0;
    step_size    = 24.0f;
    mass         = 100.0f;

    flags         = 0;
    extendedflags = 0;
    hyperflags    = 0;
    mbf21flags    = 0;

    explode_damage.Default(damage_c::DEFAULT_Mobj);
    explode_radius = 0;

    lose_benefits    = nullptr;
    pickup_benefits  = nullptr;
    kill_benefits    = nullptr;
    pickup_effects   = nullptr;
    pickup_message   = "";
    initial_benefits = nullptr;

    castorder = 0;
    cast_title.clear();
    respawntime  = 30 * TICRATE;
    translucency = PERCENT_MAKE(100);
    minatkchance = PERCENT_MAKE(0);
    palremap     = nullptr;

    jump_delay   = 1 * TICRATE;
    jumpheight   = 10;
    crouchheight = 28;
    viewheight   = PERCENT_MAKE(75);
    shotheight   = PERCENT_MAKE(64);
    maxfall      = 0;
    fast         = 1.0f;
    scale        = 1.0f;
    aspect       = 1.0f;
    yalign       = SPYA_BottomUp;

    model_skin   = 1;
    model_scale  = 1.0f;
    model_aspect = 1.0f;
    model_bias   = 0.0f;
    model_rotate = 0;

    bounce_speed  = 0.5f;
    bounce_up     = 0.5f;
    sight_slope   = 16.0f;
    sight_angle   = kBAMAngle90;
    ride_friction = RIDE_FRICTION;
    shadow_trans  = PERCENT_MAKE(50);
    glow_type     = GLOW_None;

    seesound       = sfx_None;
    attacksound    = sfx_None;
    painsound      = sfx_None;
    deathsound     = sfx_None;
    overkill_sound = sfx_None;
    activesound    = sfx_None;
    walksound      = sfx_None;
    jump_sound     = sfx_None;
    noway_sound    = sfx_None;
    oof_sound      = sfx_None;
    fallpain_sound = sfx_None;
    gasp_sound     = sfx_None;
    // secretsound = sfx_None;
    secretsound   = sfxdefs.GetEffect("SECRET");
    falling_sound = sfx_None;
    rip_sound     = sfx_None;

    fuse           = 0;
    reload_shots   = 5;
    armour_protect = -1.0; // disabled!
    armour_deplete = PERCENT_MAKE(100);
    armour_class   = BITSET_FULL;

    side          = BITSET_EMPTY;
    playernum     = 0;
    lung_capacity = 20 * TICRATE;
    gasp_start    = 2 * TICRATE;

    choke_damage.Default(damage_c::DEFAULT_MobjChoke);

    bobbing           = PERCENT_MAKE(100);
    immunity          = BITSET_EMPTY;
    resistance        = BITSET_EMPTY;
    resist_multiply   = 0.4;
    resist_painchance = -1; // disabled
    ghost             = BITSET_EMPTY;

    closecombat = nullptr;
    rangeattack = nullptr;
    spareattack = nullptr;

    // dynamic light info
    dlight[0].Default();
    dlight[1].Default();

    weak.Default();

    dropitem = nullptr;
    dropitem_ref.clear();
    blood = nullptr;
    blood_ref.clear();
    respawneffect = nullptr;
    respawneffect_ref.clear();
    spitspot = nullptr;
    spitspot_ref.clear();

    gib_health = 0;

    sight_distance = -1;
    hear_distance  = -1;

    morphtimeout = 0;

    infight_group = -2;
    proj_group    = -2;
    splash_group  = -2;
    fast_speed    = -1;
    melee_range   = -1;
}

void mobjtype_c::DLightCompatibility(void)
{
    for (int DL = 0; DL < 2; DL++)
    {
        int r = epi::GetRGBARed(dlight[DL].colour);
        int g = epi::GetRGBAGreen(dlight[DL].colour);
        int b = epi::GetRGBABlue(dlight[DL].colour);

        // dim the colour
        r = int(r * DLIT_COMPAT_ITY);
        g = int(g * DLIT_COMPAT_ITY);
        b = int(b * DLIT_COMPAT_ITY);

        switch (dlight[DL].type)
        {
        case DLITE_Compat_QUAD:
            dlight[DL].type   = DLITE_Modulate;
            dlight[DL].radius = DLIT_COMPAT_RAD(dlight[DL].radius);
            dlight[DL].colour = epi::MakeRGBA(r, g, b);

            hyperflags |= HF_QUADRATIC_COMPAT;
            break;

        case DLITE_Compat_LIN:
            dlight[DL].type = DLITE_Modulate;
            dlight[DL].radius *= 1.3;
            dlight[DL].colour = epi::MakeRGBA(r, g, b);
            break;

        default: // nothing to do
            break;
        }

        //??	if (dlight[DL].radius > 500)
        //??		dlight[DL].radius = 500;
    }
}

// --> mobjtype_container_c class

mobjtype_container_c::mobjtype_container_c()
{
    memset(lookup_cache, 0, sizeof(mobjtype_c *) * LOOKUP_CACHESIZE);
}

mobjtype_container_c::~mobjtype_container_c()
{
    for (auto iter = begin(); iter != end(); iter++)
    {
        mobjtype_c *m = *iter;
        delete m;
        m = nullptr;
    }
}

int mobjtype_container_c::FindFirst(const char *name, int startpos)
{
    startpos = HMM_MAX(startpos, 0);

    for (startpos; startpos < size(); startpos++)
    {
        mobjtype_c *m = at(startpos);
        if (DDF_CompareName(m->name.c_str(), name) == 0)
            return startpos;
    }

    return -1;
}

int mobjtype_container_c::FindLast(const char *name, int startpos)
{
    startpos = HMM_MIN(startpos, size()-1);

    for (startpos; startpos >= 0; startpos--)
    {
        mobjtype_c *m = at(startpos);
        if (DDF_CompareName(m->name.c_str(), name) == 0)
            return startpos;
    }

    return -1;
}

bool mobjtype_container_c::MoveToEnd(int idx)
{
    // Moves an entry from its current position to end of the list.

    mobjtype_c *m = nullptr;

    if (idx < 0 || idx >= size())
        return false;

    if (idx == (size() - 1))
        return true; // Already at the end

    // Get a copy of the pointer
    m = at(idx);

    erase(begin()+idx);

    push_back(m);

    return true;
}

const mobjtype_c *mobjtype_container_c::Lookup(const char *refname)
{
    // Looks an mobjdef by name.
    // Fatal error if it does not exist.

    int idx = FindLast(refname);

    if (idx >= 0)
        return (*this)[idx];

    if (lax_errors)
        return default_mobjtype;

    DDF_Error("Unknown thing type: %s\n", refname);
    return nullptr; /* NOT REACHED */
}

const mobjtype_c *mobjtype_container_c::Lookup(int id)
{
    if (id == 0)
        return default_mobjtype;

    // Looks an mobjdef by number.
    // Fatal error if it does not exist.

    int slot = DDF_MobjHashFunc(id);

    // check the cache
    if (lookup_cache[slot] && lookup_cache[slot]->number == id)
    {
        return lookup_cache[slot];
    }

    for (auto iter = rbegin(); iter != rend(); iter++)
    {
        mobjtype_c *m = *iter;

        if (m->number == id)
        {
            // update the cache
            lookup_cache[slot] = m;
            return m;
        }
    }

    return nullptr;
}

const mobjtype_c *mobjtype_container_c::LookupCastMember(int castpos)
{
    // Lookup the cast member of the one with the nearest match
    // to the position given.

    mobjtype_c           *best = nullptr;
    mobjtype_c           *m = nullptr;

    for (auto iter = rbegin(); iter != rend(); iter++)
    {
        m = *iter;
        if (m->castorder > 0)
        {
            if (m->castorder == castpos) // Exact match
                return m;

            if (best)
            {
                if (m->castorder > castpos)
                {
                    if (best->castorder > castpos)
                    {
                        int of1 = m->castorder - castpos;
                        int of2 = best->castorder - castpos;

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
                    if (best->castorder < castpos)
                    {
                        int of1 = castpos - m->castorder;
                        int of2 = castpos - best->castorder;

                        if (of1 > of2)
                            best = m;
                    }
                }
            }
            else
            {
                // We don't have a best item, so this has to be our best current match
                best = m;
            }
        }
    }

    return best;
}

const mobjtype_c *mobjtype_container_c::LookupPlayer(int playernum)
{
    // Find a player thing (needed by deathmatch code).
    for (auto iter = rbegin(); iter != rend(); iter++)
    {
        mobjtype_c *m = *iter;

        if (m->playernum == playernum)
            return m;
    }

    I_Error("Missing DDF entry for player number %d\n", playernum);
    return nullptr; /* NOT REACHED */
}

const mobjtype_c *mobjtype_container_c::LookupDoorKey(int theKey)
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
                std::string temp_ref = epi::StringFormat("%s", keytype_names[k].name);
                KeyName = temp_ref;
                break;
            }
        }
    */

    for (auto iter = rbegin(); iter != rend(); iter++)
    {
        mobjtype_c *m = *iter;

        benefit_t *list;
        list = m->pickup_benefits;
        for (; list != nullptr; list = list->next)
        {
            if (list->type == BENEFIT_Key)
            {
                if (list->sub.type == theKey)
                {
                    return m;
                }
            }
        }
    }

    I_Warning("Missing DDF entry for key %d\n", theKey);
    return nullptr;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
