//------------------------------------------------------------------------
//  MOBJ Definitions
//------------------------------------------------------------------------
//
//  DEH_EDGE  Copyright (C) 2004-2024 The EDGE Team
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License (in COPYING.txt) for more details.
//
//------------------------------------------------------------------------
//
//  DEH_EDGE is based on:
//
//  +  DeHackEd source code, by Greg Lewis.
//  -  DOOM source code (C) 1993-1996 id Software, Inc.
//  -  Linux DOOM Hack Editor, by Sam Lantinga.
//  -  PrBoom's DEH/BEX code, by Ty Halderman, TeamTNT.
//
//------------------------------------------------------------------------

#include "deh_mobj.h"

#include "deh_edge.h"
#include "deh_info.h"
#include "deh_sounds.h"

namespace dehacked
{

DehackedMapObjectDefinition mobjinfo[kTotalDehackedMapObjectTypesPortCompatibility] = {
    // MT_PLAYER
    {
        "OUR_HERO",      // name
        -1,              // doomednum
        kS_PLAY,         // spawnstate
        100,             // spawnhealth
        kS_PLAY_RUN1,    // seestate
        ksfx_None,       // seesound
        0,               // reactiontime
        ksfx_None,       // attacksound
        kS_PLAY_PAIN,    // painstate
        255,             // painchance
        ksfx_plpain,     // painsound
        kS_NULL,         // meleestate
        kS_PLAY_ATK1,    // missilestate
        kS_PLAY_DIE1,    // deathstate
        kS_PLAY_XDIE1,   // xdeathstate
        ksfx_pldeth,     // deathsound
        0,               // speed
        16 * kFracUnit,  // radius
        56 * kFracUnit,  // height
        100,             // mass
        0,               // damage
        ksfx_None,       // activesound
        kMF_SOLID | kMF_SHOOTABLE | kMF_DROPOFF | kMF_PICKUP |
            kMF_NOTDMATCH,  // flags
        0,                  // MBF21 flags
        -2,                 // Infighting group
        -2,                 // Projectile group
        -2,                 // Splash group
        ksfx_None,          // Rip sound
        0,                  // Fast speed
        0,                  // Melee range
        0,                  // Gib health
        -1,                 // Dropped item
        0,                  // Pickup width
        0,                  // Projectile pass height
        0,                  // Fullbright
        kS_NULL             // raisestate
    },

    // MT_POSSESSED
    {
        "ZOMBIEMAN",                                // name
        3004,                                       // doomednum
        kS_POSS_STND,                               // spawnstate
        20,                                         // spawnhealth
        kS_POSS_RUN1,                               // seestate
        ksfx_posit1,                                // seesound
        8,                                          // reactiontime
        ksfx_pistol,                                // attacksound
        kS_POSS_PAIN,                               // painstate
        200,                                        // painchance
        ksfx_popain,                                // painsound
        0,                                          // meleestate
        kS_POSS_ATK1,                               // missilestate
        kS_POSS_DIE1,                               // deathstate
        kS_POSS_XDIE1,                              // xdeathstate
        ksfx_podth1,                                // deathsound
        8,                                          // speed
        20 * kFracUnit,                             // radius
        56 * kFracUnit,                             // height
        100,                                        // mass
        0,                                          // damage
        ksfx_posact,                                // activesound
        kMF_SOLID | kMF_SHOOTABLE | kMF_COUNTKILL,  // flags
        0,                                          // MBF21 flags
        -2,                                         // Infighting group
        -2,                                         // Projectile group
        -2,                                         // Splash group
        ksfx_None,                                  // Rip sound
        0,                                          // Fast speed
        0,                                          // Melee range
        0,                                          // Gib health
        -1,                                         // Dropped item
        0,                                          // Pickup width
        0,                                          // Projectile pass height
        0,                                          // Fullbright
        kS_POSS_RAISE1                              // raisestate
    },

    // MT_SHOTGUY
    {
        "SHOTGUN_GUY",                              // name
        9,                                          // doomednum
        kS_SPOS_STND,                               // spawnstate
        30,                                         // spawnhealth
        kS_SPOS_RUN1,                               // seestate
        ksfx_posit2,                                // seesound
        8,                                          // reactiontime
        0,                                          // attacksound
        kS_SPOS_PAIN,                               // painstate
        170,                                        // painchance
        ksfx_popain,                                // painsound
        0,                                          // meleestate
        kS_SPOS_ATK1,                               // missilestate
        kS_SPOS_DIE1,                               // deathstate
        kS_SPOS_XDIE1,                              // xdeathstate
        ksfx_podth2,                                // deathsound
        8,                                          // speed
        20 * kFracUnit,                             // radius
        56 * kFracUnit,                             // height
        100,                                        // mass
        0,                                          // damage
        ksfx_posact,                                // activesound
        kMF_SOLID | kMF_SHOOTABLE | kMF_COUNTKILL,  // flags
        0,                                          // MBF21 flags
        -2,                                         // Infighting group
        -2,                                         // Projectile group
        -2,                                         // Splash group
        ksfx_None,                                  // Rip sound
        0,                                          // Fast speed
        0,                                          // Melee range
        0,                                          // Gib health
        -1,                                         // Dropped item
        0,                                          // Pickup width
        0,                                          // Projectile pass height
        0,                                          // Fullbright
        kS_SPOS_RAISE1                              // raisestate
    },

    // MT_VILE
    {
        "ARCHVILE",                                 // name
        64,                                         // doomednum
        kS_VILE_STND,                               // spawnstate
        700,                                        // spawnhealth
        kS_VILE_RUN1,                               // seestate
        ksfx_vilsit,                                // seesound
        8,                                          // reactiontime
        0,                                          // attacksound
        kS_VILE_PAIN,                               // painstate
        10,                                         // painchance
        ksfx_vipain,                                // painsound
        0,                                          // meleestate
        kS_VILE_ATK1,                               // missilestate
        kS_VILE_DIE1,                               // deathstate
        kS_NULL,                                    // xdeathstate
        ksfx_vildth,                                // deathsound
        15,                                         // speed
        20 * kFracUnit,                             // radius
        56 * kFracUnit,                             // height
        500,                                        // mass
        0,                                          // damage
        ksfx_vilact,                                // activesound
        kMF_SOLID | kMF_SHOOTABLE | kMF_COUNTKILL,  // flags
        0,                                          // MBF21 flags
        -2,                                         // Infighting group
        -2,                                         // Projectile group
        -2,                                         // Splash group
        ksfx_None,                                  // Rip sound
        0,                                          // Fast speed
        0,                                          // Melee range
        0,                                          // Gib health
        -1,                                         // Dropped item
        0,                                          // Pickup width
        0,                                          // Projectile pass height
        0,                                          // Fullbright
        kS_NULL                                     // raisestate
    },

    // MT_FIRE
    {
        "*ARCHVILE_FIRE",                // (attack) name
        -1,                              // doomednum
        kS_FIRE1,                        // spawnstate
        1000,                            // spawnhealth
        kS_NULL,                         // seestate
        ksfx_None,                       // seesound
        8,                               // reactiontime
        ksfx_None,                       // attacksound
        kS_NULL,                         // painstate
        0,                               // painchance
        ksfx_None,                       // painsound
        kS_NULL,                         // meleestate
        kS_NULL,                         // missilestate
        kS_NULL,                         // deathstate
        kS_NULL,                         // xdeathstate
        ksfx_None,                       // deathsound
        0,                               // speed
        20 * kFracUnit,                  // radius
        16 * kFracUnit,                  // height
        100,                             // mass
        0,                               // damage
        ksfx_None,                       // activesound
        kMF_NOBLOCKMAP | kMF_NOGRAVITY,  // flags
        0,                               // MBF21 flags
        -2,                              // Infighting group
        -2,                              // Projectile group
        -2,                              // Splash group
        ksfx_None,                       // Rip sound
        0,                               // Fast speed
        0,                               // Melee range
        0,                               // Gib health
        -1,                              // Dropped item
        0,                               // Pickup width
        0,                               // Projectile pass height
        0,                               // Fullbright
        kS_NULL                          // raisestate
    },

    // MT_UNDEAD
    {
        "REVENANT",                                 // name
        66,                                         // doomednum
        kS_SKEL_STND,                               // spawnstate
        300,                                        // spawnhealth
        kS_SKEL_RUN1,                               // seestate
        ksfx_skesit,                                // seesound
        8,                                          // reactiontime
        0,                                          // attacksound
        kS_SKEL_PAIN,                               // painstate
        100,                                        // painchance
        ksfx_popain,                                // painsound
        kS_SKEL_FIST1,                              // meleestate
        kS_SKEL_MISS1,                              // missilestate
        kS_SKEL_DIE1,                               // deathstate
        kS_NULL,                                    // xdeathstate
        ksfx_skedth,                                // deathsound
        10,                                         // speed
        20 * kFracUnit,                             // radius
        56 * kFracUnit,                             // height
        500,                                        // mass
        0,                                          // damage
        ksfx_skeact,                                // activesound
        kMF_SOLID | kMF_SHOOTABLE | kMF_COUNTKILL,  // flags
        0,                                          // MBF21 flags
        -2,                                         // Infighting group
        -2,                                         // Projectile group
        -2,                                         // Splash group
        ksfx_None,                                  // Rip sound
        0,                                          // Fast speed
        0,                                          // Melee range
        0,                                          // Gib health
        -1,                                         // Dropped item
        0,                                          // Pickup width
        0,                                          // Projectile pass height
        0,                                          // Fullbright
        kS_SKEL_RAISE1                              // raisestate
    },

    // MT_TRACER
    {
        "*REVENANT_MISSILE",  // (attack) name
        -1,                   // doomednum
        kS_TRACER,            // spawnstate
        1000,                 // spawnhealth
        kS_NULL,              // seestate
        ksfx_skeatk,          // seesound
        8,                    // reactiontime
        ksfx_None,            // attacksound
        kS_NULL,              // painstate
        0,                    // painchance
        ksfx_None,            // painsound
        kS_NULL,              // meleestate
        kS_NULL,              // missilestate
        kS_TRACEEXP1,         // deathstate
        kS_NULL,              // xdeathstate
        ksfx_barexp,          // deathsound
        10 * kFracUnit,       // speed
        11 * kFracUnit,       // radius
        8 * kFracUnit,        // height
        100,                  // mass
        10,                   // damage
        ksfx_None,            // activesound
        kMF_NOBLOCKMAP | kMF_MISSILE | kMF_DROPOFF | kMF_NOGRAVITY,  // flags
        0,          // MBF21 flags
        -2,         // Infighting group
        -2,         // Projectile group
        -2,         // Splash group
        ksfx_None,  // Rip sound
        0,          // Fast speed
        0,          // Melee range
        0,          // Gib health
        -1,         // Dropped item
        0,          // Pickup width
        0,          // Projectile pass height
        0,          // Fullbright
        kS_NULL     // raisestate
    },

    // MT_SMOKE
    {
        "SMOKE",                         // name
        -1,                              // doomednum
        kS_SMOKE1,                       // spawnstate
        1000,                            // spawnhealth
        kS_NULL,                         // seestate
        ksfx_None,                       // seesound
        8,                               // reactiontime
        ksfx_None,                       // attacksound
        kS_NULL,                         // painstate
        0,                               // painchance
        ksfx_None,                       // painsound
        kS_NULL,                         // meleestate
        kS_NULL,                         // missilestate
        kS_NULL,                         // deathstate
        kS_NULL,                         // xdeathstate
        ksfx_None,                       // deathsound
        0,                               // speed
        20 * kFracUnit,                  // radius
        16 * kFracUnit,                  // height
        100,                             // mass
        0,                               // damage
        ksfx_None,                       // activesound
        kMF_NOBLOCKMAP | kMF_NOGRAVITY,  // flags
        0,                               // MBF21 flags
        -2,                              // Infighting group
        -2,                              // Projectile group
        -2,                              // Splash group
        ksfx_None,                       // Rip sound
        0,                               // Fast speed
        0,                               // Melee range
        0,                               // Gib health
        -1,                              // Dropped item
        0,                               // Pickup width
        0,                               // Projectile pass height
        0,                               // Fullbright
        kS_NULL                          // raisestate
    },

    // MT_FATSO
    {
        "MANCUBUS",                                 // name
        67,                                         // doomednum
        kS_FATT_STND,                               // spawnstate
        600,                                        // spawnhealth
        kS_FATT_RUN1,                               // seestate
        ksfx_mansit,                                // seesound
        8,                                          // reactiontime
        0,                                          // attacksound
        kS_FATT_PAIN,                               // painstate
        80,                                         // painchance
        ksfx_mnpain,                                // painsound
        0,                                          // meleestate
        kS_FATT_ATK1,                               // missilestate
        kS_FATT_DIE1,                               // deathstate
        kS_NULL,                                    // xdeathstate
        ksfx_mandth,                                // deathsound
        8,                                          // speed
        48 * kFracUnit,                             // radius
        64 * kFracUnit,                             // height
        1000,                                       // mass
        0,                                          // damage
        ksfx_posact,                                // activesound
        kMF_SOLID | kMF_SHOOTABLE | kMF_COUNTKILL,  // flags
        kMBF21_MAP07BOSS1,                          // MBF21 flags
        -2,                                         // Infighting group
        -2,                                         // Projectile group
        -2,                                         // Splash group
        ksfx_None,                                  // Rip sound
        0,                                          // Fast speed
        0,                                          // Melee range
        0,                                          // Gib health
        -1,                                         // Dropped item
        0,                                          // Pickup width
        0,                                          // Projectile pass height
        0,                                          // Fullbright
        kS_FATT_RAISE1                              // raisestate
    },

    // MT_FATSHOT
    {
        "*MANCUBUS_FIREBALL",  // (attack) name
        -1,                    // doomednum
        kS_FATSHOT1,           // spawnstate
        1000,                  // spawnhealth
        kS_NULL,               // seestate
        ksfx_firsht,           // seesound
        8,                     // reactiontime
        ksfx_None,             // attacksound
        kS_NULL,               // painstate
        0,                     // painchance
        ksfx_None,             // painsound
        kS_NULL,               // meleestate
        kS_NULL,               // missilestate
        kS_FATSHOTX1,          // deathstate
        kS_NULL,               // xdeathstate
        ksfx_firxpl,           // deathsound
        20 * kFracUnit,        // speed
        6 * kFracUnit,         // radius
        8 * kFracUnit,         // height
        100,                   // mass
        8,                     // damage
        ksfx_None,             // activesound
        kMF_NOBLOCKMAP | kMF_MISSILE | kMF_DROPOFF | kMF_NOGRAVITY,  // flags
        0,          // MBF21 flags
        -2,         // Infighting group
        -2,         // Projectile group
        -2,         // Splash group
        ksfx_None,  // Rip sound
        0,          // Fast speed
        0,          // Melee range
        0,          // Gib health
        -1,         // Dropped item
        0,          // Pickup width
        0,          // Projectile pass height
        0,          // Fullbright
        kS_NULL     // raisestate
    },

    // MT_CHAINGUY
    {
        "HEAVY_WEAPON_DUDE",                        // name
        65,                                         // doomednum
        kS_CPOS_STND,                               // spawnstate
        70,                                         // spawnhealth
        kS_CPOS_RUN1,                               // seestate
        ksfx_posit2,                                // seesound
        8,                                          // reactiontime
        0,                                          // attacksound
        kS_CPOS_PAIN,                               // painstate
        170,                                        // painchance
        ksfx_popain,                                // painsound
        0,                                          // meleestate
        kS_CPOS_ATK1,                               // missilestate
        kS_CPOS_DIE1,                               // deathstate
        kS_CPOS_XDIE1,                              // xdeathstate
        ksfx_podth2,                                // deathsound
        8,                                          // speed
        20 * kFracUnit,                             // radius
        56 * kFracUnit,                             // height
        100,                                        // mass
        0,                                          // damage
        ksfx_posact,                                // activesound
        kMF_SOLID | kMF_SHOOTABLE | kMF_COUNTKILL,  // flags
        0,                                          // MBF21 flags
        -2,                                         // Infighting group
        -2,                                         // Projectile group
        -2,                                         // Splash group
        ksfx_None,                                  // Rip sound
        0,                                          // Fast speed
        0,                                          // Melee range
        0,                                          // Gib health
        -1,                                         // Dropped item
        0,                                          // Pickup width
        0,                                          // Projectile pass height
        0,                                          // Fullbright
        kS_CPOS_RAISE1                              // raisestate
    },

    // MT_TROOP
    {
        "IMP",                                      // name
        3001,                                       // doomednum
        kS_TROO_STND,                               // spawnstate
        60,                                         // spawnhealth
        kS_TROO_RUN1,                               // seestate
        ksfx_bgsit1,                                // seesound
        8,                                          // reactiontime
        0,                                          // attacksound
        kS_TROO_PAIN,                               // painstate
        200,                                        // painchance
        ksfx_popain,                                // painsound
        kS_TROO_ATK1,                               // meleestate
        kS_TROO_ATK1,                               // missilestate
        kS_TROO_DIE1,                               // deathstate
        kS_TROO_XDIE1,                              // xdeathstate
        ksfx_bgdth1,                                // deathsound
        8,                                          // speed
        20 * kFracUnit,                             // radius
        56 * kFracUnit,                             // height
        100,                                        // mass
        0,                                          // damage
        ksfx_bgact,                                 // activesound
        kMF_SOLID | kMF_SHOOTABLE | kMF_COUNTKILL,  // flags
        0,                                          // MBF21 flags
        -2,                                         // Infighting group
        -2,                                         // Projectile group
        -2,                                         // Splash group
        ksfx_None,                                  // Rip sound
        0,                                          // Fast speed
        0,                                          // Melee range
        0,                                          // Gib health
        -1,                                         // Dropped item
        0,                                          // Pickup width
        0,                                          // Projectile pass height
        0,                                          // Fullbright
        kS_TROO_RAISE1                              // raisestate
    },

    // MT_SERGEANT
    {
        "DEMON",                                    // name
        3002,                                       // doomednum
        kS_SARG_STND,                               // spawnstate
        150,                                        // spawnhealth
        kS_SARG_RUN1,                               // seestate
        ksfx_sgtsit,                                // seesound
        8,                                          // reactiontime
        ksfx_sgtatk,                                // attacksound
        kS_SARG_PAIN,                               // painstate
        180,                                        // painchance
        ksfx_dmpain,                                // painsound
        kS_SARG_ATK1,                               // meleestate
        0,                                          // missilestate
        kS_SARG_DIE1,                               // deathstate
        kS_NULL,                                    // xdeathstate
        ksfx_sgtdth,                                // deathsound
        10,                                         // speed
        30 * kFracUnit,                             // radius
        56 * kFracUnit,                             // height
        400,                                        // mass
        0,                                          // damage
        ksfx_dmact,                                 // activesound
        kMF_SOLID | kMF_SHOOTABLE | kMF_COUNTKILL,  // flags
        0,                                          // MBF21 flags
        -2,                                         // Infighting group
        -2,                                         // Projectile group
        -2,                                         // Splash group
        ksfx_None,                                  // Rip sound
        0,                                          // Fast speed
        0,                                          // Melee range
        0,                                          // Gib health
        -1,                                         // Dropped item
        0,                                          // Pickup width
        0,                                          // Projectile pass height
        0,                                          // Fullbright
        kS_SARG_RAISE1                              // raisestate
    },

    // MT_SHADOWS
    {
        "SPECTRE",                                               // name
        58,                                                      // doomednum
        kS_SARG_STND,                                            // spawnstate
        150,                                                     // spawnhealth
        kS_SARG_RUN1,                                            // seestate
        ksfx_sgtsit,                                             // seesound
        8,                                                       // reactiontime
        ksfx_sgtatk,                                             // attacksound
        kS_SARG_PAIN,                                            // painstate
        180,                                                     // painchance
        ksfx_dmpain,                                             // painsound
        kS_SARG_ATK1,                                            // meleestate
        0,                                                       // missilestate
        kS_SARG_DIE1,                                            // deathstate
        kS_NULL,                                                 // xdeathstate
        ksfx_sgtdth,                                             // deathsound
        10,                                                      // speed
        30 * kFracUnit,                                          // radius
        56 * kFracUnit,                                          // height
        400,                                                     // mass
        0,                                                       // damage
        ksfx_dmact,                                              // activesound
        kMF_SOLID | kMF_SHOOTABLE | kMF_SHADOW | kMF_COUNTKILL,  // flags
        0,                                                       // MBF21 flags
        -2,             // Infighting group
        -2,             // Projectile group
        -2,             // Splash group
        ksfx_None,      // Rip sound
        0,              // Fast speed
        0,              // Melee range
        0,              // Gib health
        -1,             // Dropped item
        0,              // Pickup width
        0,              // Projectile pass height
        0,              // Fullbright
        kS_SARG_RAISE1  // raisestate
    },

    // MT_HEAD
    {
        "CACODEMON",     // name
        3005,            // doomednum
        kS_HEAD_STND,    // spawnstate
        400,             // spawnhealth
        kS_HEAD_RUN1,    // seestate
        ksfx_cacsit,     // seesound
        8,               // reactiontime
        0,               // attacksound
        kS_HEAD_PAIN,    // painstate
        128,             // painchance
        ksfx_dmpain,     // painsound
        0,               // meleestate
        kS_HEAD_ATK1,    // missilestate
        kS_HEAD_DIE1,    // deathstate
        kS_NULL,         // xdeathstate
        ksfx_cacdth,     // deathsound
        8,               // speed
        31 * kFracUnit,  // radius
        56 * kFracUnit,  // height
        400,             // mass
        0,               // damage
        ksfx_dmact,      // activesound
        kMF_SOLID | kMF_SHOOTABLE | kMF_FLOAT | kMF_NOGRAVITY |
            kMF_COUNTKILL,  // flags
        0,                  // MBF21 flags
        -2,                 // Infighting group
        -2,                 // Projectile group
        -2,                 // Splash group
        ksfx_None,          // Rip sound
        0,                  // Fast speed
        0,                  // Melee range
        0,                  // Gib health
        -1,                 // Dropped item
        0,                  // Pickup width
        0,                  // Projectile pass height
        0,                  // Fullbright
        kS_HEAD_RAISE1      // raisestate
    },

    // MT_BRUISER
    {
        "BARON_OF_HELL",                            // name
        3003,                                       // doomednum
        kS_BOSS_STND,                               // spawnstate
        1000,                                       // spawnhealth
        kS_BOSS_RUN1,                               // seestate
        ksfx_brssit,                                // seesound
        8,                                          // reactiontime
        0,                                          // attacksound
        kS_BOSS_PAIN,                               // painstate
        50,                                         // painchance
        ksfx_dmpain,                                // painsound
        kS_BOSS_ATK1,                               // meleestate
        kS_BOSS_ATK1,                               // missilestate
        kS_BOSS_DIE1,                               // deathstate
        kS_NULL,                                    // xdeathstate
        ksfx_brsdth,                                // deathsound
        8,                                          // speed
        24 * kFracUnit,                             // radius
        64 * kFracUnit,                             // height
        1000,                                       // mass
        0,                                          // damage
        ksfx_dmact,                                 // activesound
        kMF_SOLID | kMF_SHOOTABLE | kMF_COUNTKILL,  // flags
        kMBF21_E1M8BOSS,                            // MBF21 flags
        -2,                                         // Infighting group
        -2,                                         // Projectile group
        -2,                                         // Splash group
        ksfx_None,                                  // Rip sound
        0,                                          // Fast speed
        0,                                          // Melee range
        0,                                          // Gib health
        -1,                                         // Dropped item
        0,                                          // Pickup width
        0,                                          // Projectile pass height
        0,                                          // Fullbright
        kS_BOSS_RAISE1                              // raisestate
    },

    // MT_BRUISERSHOT
    {
        "*BARON_FIREBALL",  // (attack) name
        -1,                 // doomednum
        kS_BRBALL1,         // spawnstate
        1000,               // spawnhealth
        kS_NULL,            // seestate
        ksfx_firsht,        // seesound
        8,                  // reactiontime
        ksfx_None,          // attacksound
        kS_NULL,            // painstate
        0,                  // painchance
        ksfx_None,          // painsound
        kS_NULL,            // meleestate
        kS_NULL,            // missilestate
        kS_BRBALLX1,        // deathstate
        kS_NULL,            // xdeathstate
        ksfx_firxpl,        // deathsound
        15 * kFracUnit,     // speed
        6 * kFracUnit,      // radius
        8 * kFracUnit,      // height
        100,                // mass
        8,                  // damage
        ksfx_None,          // activesound
        kMF_NOBLOCKMAP | kMF_MISSILE | kMF_DROPOFF | kMF_NOGRAVITY,  // flags
        0,          // MBF21 flags
        -2,         // Infighting group
        -2,         // Projectile group
        -2,         // Splash group
        ksfx_None,  // Rip sound
        0,          // Fast speed
        0,          // Melee range
        0,          // Gib health
        -1,         // Dropped item
        0,          // Pickup width
        0,          // Projectile pass height
        0,          // Fullbright
        kS_NULL     // raisestate
    },

    // MT_KNIGHT
    {
        "HELL_KNIGHT",                              // name
        69,                                         // doomednum
        kS_BOS2_STND,                               // spawnstate
        500,                                        // spawnhealth
        kS_BOS2_RUN1,                               // seestate
        ksfx_kntsit,                                // seesound
        8,                                          // reactiontime
        0,                                          // attacksound
        kS_BOS2_PAIN,                               // painstate
        50,                                         // painchance
        ksfx_dmpain,                                // painsound
        kS_BOS2_ATK1,                               // meleestate
        kS_BOS2_ATK1,                               // missilestate
        kS_BOS2_DIE1,                               // deathstate
        kS_NULL,                                    // xdeathstate
        ksfx_kntdth,                                // deathsound
        8,                                          // speed
        24 * kFracUnit,                             // radius
        64 * kFracUnit,                             // height
        1000,                                       // mass
        0,                                          // damage
        ksfx_dmact,                                 // activesound
        kMF_SOLID | kMF_SHOOTABLE | kMF_COUNTKILL,  // flags
        0,                                          // MBF21 flags
        -2,                                         // Infighting group
        -2,                                         // Projectile group
        -2,                                         // Splash group
        ksfx_None,                                  // Rip sound
        0,                                          // Fast speed
        0,                                          // Melee range
        0,                                          // Gib health
        -1,                                         // Dropped item
        0,                                          // Pickup width
        0,                                          // Projectile pass height
        0,                                          // Fullbright
        kS_BOS2_RAISE1                              // raisestate
    },

    // MT_SKULL
    {
        "LOST_SOUL",                                            // name
        3006,                                                   // doomednum
        kS_SKULL_STND,                                          // spawnstate
        100,                                                    // spawnhealth
        kS_SKULL_RUN1,                                          // seestate
        0,                                                      // seesound
        8,                                                      // reactiontime
        ksfx_sklatk,                                            // attacksound
        kS_SKULL_PAIN,                                          // painstate
        256,                                                    // painchance
        ksfx_dmpain,                                            // painsound
        0,                                                      // meleestate
        kS_SKULL_ATK1,                                          // missilestate
        kS_SKULL_DIE1,                                          // deathstate
        kS_NULL,                                                // xdeathstate
        ksfx_firxpl,                                            // deathsound
        8,                                                      // speed
        16 * kFracUnit,                                         // radius
        56 * kFracUnit,                                         // height
        50,                                                     // mass
        3,                                                      // damage
        ksfx_dmact,                                             // activesound
        kMF_SOLID | kMF_SHOOTABLE | kMF_FLOAT | kMF_NOGRAVITY,  // flags
        0,                                                      // MBF21 flags
        -2,         // Infighting group
        -2,         // Projectile group
        -2,         // Splash group
        ksfx_None,  // Rip sound
        0,          // Fast speed
        0,          // Melee range
        0,          // Gib health
        -1,         // Dropped item
        0,          // Pickup width
        0,          // Projectile pass height
        0,          // Fullbright
        kS_NULL     // raisestate
    },

    // MT_SPIDER
    {
        "THE_SPIDER_MASTERMIND",                    // name
        7,                                          // doomednum
        kS_SPID_STND,                               // spawnstate
        3000,                                       // spawnhealth
        kS_SPID_RUN1,                               // seestate
        ksfx_spisit,                                // seesound
        8,                                          // reactiontime
        ksfx_shotgn,                                // attacksound
        kS_SPID_PAIN,                               // painstate
        40,                                         // painchance
        ksfx_dmpain,                                // painsound
        0,                                          // meleestate
        kS_SPID_ATK1,                               // missilestate
        kS_SPID_DIE1,                               // deathstate
        kS_NULL,                                    // xdeathstate
        ksfx_spidth,                                // deathsound
        12,                                         // speed
        128 * kFracUnit,                            // radius
        100 * kFracUnit,                            // height
        1000,                                       // mass
        0,                                          // damage
        ksfx_dmact,                                 // activesound
        kMF_SOLID | kMF_SHOOTABLE | kMF_COUNTKILL,  // flags
        kMBF21_E3M8BOSS | kMBF21_E4M8BOSS,          // MBF21 flags
        -2,                                         // Infighting group
        -2,                                         // Projectile group
        -2,                                         // Splash group
        ksfx_None,                                  // Rip sound
        0,                                          // Fast speed
        0,                                          // Melee range
        0,                                          // Gib health
        -1,                                         // Dropped item
        0,                                          // Pickup width
        0,                                          // Projectile pass height
        0,                                          // Fullbright
        kS_NULL                                     // raisestate
    },

    // MT_BABY
    {
        "ARACHNOTRON",                              // name
        68,                                         // doomednum
        kS_BSPI_STND,                               // spawnstate
        500,                                        // spawnhealth
        kS_BSPI_SIGHT,                              // seestate
        ksfx_bspsit,                                // seesound
        8,                                          // reactiontime
        0,                                          // attacksound
        kS_BSPI_PAIN,                               // painstate
        128,                                        // painchance
        ksfx_dmpain,                                // painsound
        0,                                          // meleestate
        kS_BSPI_ATK1,                               // missilestate
        kS_BSPI_DIE1,                               // deathstate
        kS_NULL,                                    // xdeathstate
        ksfx_bspdth,                                // deathsound
        12,                                         // speed
        64 * kFracUnit,                             // radius
        64 * kFracUnit,                             // height
        600,                                        // mass
        0,                                          // damage
        ksfx_bspact,                                // activesound
        kMF_SOLID | kMF_SHOOTABLE | kMF_COUNTKILL,  // flags
        kMBF21_MAP07BOSS2,                          // MBF21 flags
        -2,                                         // Infighting group
        -2,                                         // Projectile group
        -2,                                         // Splash group
        ksfx_None,                                  // Rip sound
        0,                                          // Fast speed
        0,                                          // Melee range
        0,                                          // Gib health
        -1,                                         // Dropped item
        0,                                          // Pickup width
        0,                                          // Projectile pass height
        0,                                          // Fullbright
        kS_BSPI_RAISE1                              // raisestate
    },

    // MT_CYBORG
    {
        "THE_CYBERDEMON",                           // name
        16,                                         // doomednum
        kS_CYBER_STND,                              // spawnstate
        4000,                                       // spawnhealth
        kS_CYBER_RUN1,                              // seestate
        ksfx_cybsit,                                // seesound
        8,                                          // reactiontime
        0,                                          // attacksound
        kS_CYBER_PAIN,                              // painstate
        20,                                         // painchance
        ksfx_dmpain,                                // painsound
        0,                                          // meleestate
        kS_CYBER_ATK1,                              // missilestate
        kS_CYBER_DIE1,                              // deathstate
        kS_NULL,                                    // xdeathstate
        ksfx_cybdth,                                // deathsound
        16,                                         // speed
        40 * kFracUnit,                             // radius
        110 * kFracUnit,                            // height
        1000,                                       // mass
        0,                                          // damage
        ksfx_dmact,                                 // activesound
        kMF_SOLID | kMF_SHOOTABLE | kMF_COUNTKILL,  // flags
        kMBF21_E2M8BOSS | kMBF21_E4M6BOSS,          // MBF21 flags
        -2,                                         // Infighting group
        -2,                                         // Projectile group
        -2,                                         // Splash group
        ksfx_None,                                  // Rip sound
        0,                                          // Fast speed
        0,                                          // Melee range
        0,                                          // Gib health
        -1,                                         // Dropped item
        0,                                          // Pickup width
        0,                                          // Projectile pass height
        0,                                          // Fullbright
        kS_NULL                                     // raisestate
    },

    // MT_PAIN
    {
        "PAIN_ELEMENTAL",  // name
        71,                // doomednum
        kS_PAIN_STND,      // spawnstate
        400,               // spawnhealth
        kS_PAIN_RUN1,      // seestate
        ksfx_pesit,        // seesound
        8,                 // reactiontime
        0,                 // attacksound
        kS_PAIN_PAIN,      // painstate
        128,               // painchance
        ksfx_pepain,       // painsound
        0,                 // meleestate
        kS_PAIN_ATK1,      // missilestate
        kS_PAIN_DIE1,      // deathstate
        kS_NULL,           // xdeathstate
        ksfx_pedth,        // deathsound
        8,                 // speed
        31 * kFracUnit,    // radius
        56 * kFracUnit,    // height
        400,               // mass
        0,                 // damage
        ksfx_dmact,        // activesound
        kMF_SOLID | kMF_SHOOTABLE | kMF_FLOAT | kMF_NOGRAVITY |
            kMF_COUNTKILL,  // flags
        0,                  // MBF21 flags
        -2,                 // Infighting group
        -2,                 // Projectile group
        -2,                 // Splash group
        ksfx_None,          // Rip sound
        0,                  // Fast speed
        0,                  // Melee range
        0,                  // Gib health
        -1,                 // Dropped item
        0,                  // Pickup width
        0,                  // Projectile pass height
        0,                  // Fullbright
        kS_PAIN_RAISE1      // raisestate
    },

    // MT_WOLFSS
    {
        "WOLFENSTEIN_SS",                           // name
        84,                                         // doomednum
        kS_SSWV_STND,                               // spawnstate
        50,                                         // spawnhealth
        kS_SSWV_RUN1,                               // seestate
        ksfx_sssit,                                 // seesound
        8,                                          // reactiontime
        0,                                          // attacksound
        kS_SSWV_PAIN,                               // painstate
        170,                                        // painchance
        ksfx_popain,                                // painsound
        0,                                          // meleestate
        kS_SSWV_ATK1,                               // missilestate
        kS_SSWV_DIE1,                               // deathstate
        kS_SSWV_XDIE1,                              // xdeathstate
        ksfx_ssdth,                                 // deathsound
        8,                                          // speed
        20 * kFracUnit,                             // radius
        56 * kFracUnit,                             // height
        100,                                        // mass
        0,                                          // damage
        ksfx_posact,                                // activesound
        kMF_SOLID | kMF_SHOOTABLE | kMF_COUNTKILL,  // flags
        0,                                          // MBF21 flags
        -2,                                         // Infighting group
        -2,                                         // Projectile group
        -2,                                         // Splash group
        ksfx_None,                                  // Rip sound
        0,                                          // Fast speed
        0,                                          // Melee range
        0,                                          // Gib health
        -1,                                         // Dropped item
        0,                                          // Pickup width
        0,                                          // Projectile pass height
        0,                                          // Fullbright
        kS_SSWV_RAISE1                              // raisestate
    },

    // MT_KEEN
    {
        "COMMANDER_KEEN",  // name
        72,                // doomednum
        kS_KEENSTND,       // spawnstate
        100,               // spawnhealth
        kS_NULL,           // seestate
        ksfx_None,         // seesound
        8,                 // reactiontime
        ksfx_None,         // attacksound
        kS_KEENPAIN,       // painstate
        256,               // painchance
        ksfx_keenpn,       // painsound
        kS_NULL,           // meleestate
        kS_NULL,           // missilestate
        kS_COMMKEEN,       // deathstate
        kS_NULL,           // xdeathstate
        ksfx_keendt,       // deathsound
        0,                 // speed
        16 * kFracUnit,    // radius
        72 * kFracUnit,    // height
        10000000,          // mass
        0,                 // damage
        ksfx_None,         // activesound
        kMF_SOLID | kMF_SPAWNCEILING | kMF_NOGRAVITY | kMF_SHOOTABLE |
            kMF_COUNTKILL,  // flags
        0,                  // MBF21 flags
        -2,                 // Infighting group
        -2,                 // Projectile group
        -2,                 // Splash group
        ksfx_None,          // Rip sound
        0,                  // Fast speed
        0,                  // Melee range
        0,                  // Gib health
        -1,                 // Dropped item
        0,                  // Pickup width
        0,                  // Projectile pass height
        0,                  // Fullbright
        kS_NULL             // raisestate
    },

    // MT_BOSSBRAIN
    {
        "BOSS_BRAIN",               // name
        88,                         // doomednum
        kS_BRAIN,                   // spawnstate
        250,                        // spawnhealth
        kS_NULL,                    // seestate
        ksfx_None,                  // seesound
        8,                          // reactiontime
        ksfx_None,                  // attacksound
        kS_BRAIN_PAIN,              // painstate
        255,                        // painchance
        ksfx_bospn,                 // painsound
        kS_NULL,                    // meleestate
        kS_NULL,                    // missilestate
        kS_BRAIN_DIE1,              // deathstate
        kS_NULL,                    // xdeathstate
        ksfx_bosdth,                // deathsound
        0,                          // speed
        16 * kFracUnit,             // radius
        16 * kFracUnit,             // height
        10000000,                   // mass
        0,                          // damage
        ksfx_None,                  // activesound
        kMF_SOLID | kMF_SHOOTABLE,  // flags
        0,                          // MBF21 flags
        -2,                         // Infighting group
        -2,                         // Projectile group
        -2,                         // Splash group
        ksfx_None,                  // Rip sound
        0,                          // Fast speed
        0,                          // Melee range
        0,                          // Gib health
        -1,                         // Dropped item
        0,                          // Pickup width
        0,                          // Projectile pass height
        0,                          // Fullbright
        kS_NULL                     // raisestate
    },

    // MT_BOSSSPIT
    {
        "BRAIN_SHOOTER",                // name
        89,                             // doomednum
        kS_BRAINEYE,                    // spawnstate
        1000,                           // spawnhealth
        kS_BRAINEYESEE,                 // seestate
        ksfx_None,                      // seesound
        8,                              // reactiontime
        ksfx_None,                      // attacksound
        kS_NULL,                        // painstate
        0,                              // painchance
        ksfx_None,                      // painsound
        kS_NULL,                        // meleestate
        kS_NULL,                        // missilestate
        kS_NULL,                        // deathstate
        kS_NULL,                        // xdeathstate
        ksfx_None,                      // deathsound
        0,                              // speed
        20 * kFracUnit,                 // radius
        32 * kFracUnit,                 // height
        100,                            // mass
        0,                              // damage
        ksfx_None,                      // activesound
        kMF_NOBLOCKMAP | kMF_NOSECTOR,  // flags
        0,                              // MBF21 flags
        -2,                             // Infighting group
        -2,                             // Projectile group
        -2,                             // Splash group
        ksfx_None,                      // Rip sound
        0,                              // Fast speed
        0,                              // Melee range
        0,                              // Gib health
        -1,                             // Dropped item
        0,                              // Pickup width
        0,                              // Projectile pass height
        0,                              // Fullbright
        kS_NULL                         // raisestate
    },

    // MT_BOSSTARGET
    {
        "BRAIN_SPAWNSPOT",              // name
        87,                             // doomednum
        kS_NULL,                        // spawnstate
        1000,                           // spawnhealth
        kS_NULL,                        // seestate
        ksfx_None,                      // seesound
        8,                              // reactiontime
        ksfx_None,                      // attacksound
        kS_NULL,                        // painstate
        0,                              // painchance
        ksfx_None,                      // painsound
        kS_NULL,                        // meleestate
        kS_NULL,                        // missilestate
        kS_NULL,                        // deathstate
        kS_NULL,                        // xdeathstate
        ksfx_None,                      // deathsound
        0,                              // speed
        20 * kFracUnit,                 // radius
        32 * kFracUnit,                 // height
        100,                            // mass
        0,                              // damage
        ksfx_None,                      // activesound
        kMF_NOBLOCKMAP | kMF_NOSECTOR,  // flags
        0,                              // MBF21 flags
        -2,                             // Infighting group
        -2,                             // Projectile group
        -2,                             // Splash group
        ksfx_None,                      // Rip sound
        0,                              // Fast speed
        0,                              // Melee range
        0,                              // Gib health
        -1,                             // Dropped item
        0,                              // Pickup width
        0,                              // Projectile pass height
        0,                              // Fullbright
        kS_NULL                         // raisestate
    },

    // MT_SPAWNSHOT
    {
        "*BRAIN_CUBE",   // (attack) name
        -1,              // doomednum
        kS_SPAWN1,       // spawnstate
        1000,            // spawnhealth
        kS_NULL,         // seestate
        ksfx_bospit,     // seesound
        8,               // reactiontime
        ksfx_None,       // attacksound
        kS_NULL,         // painstate
        0,               // painchance
        ksfx_None,       // painsound
        kS_NULL,         // meleestate
        kS_NULL,         // missilestate
        kS_NULL,         // deathstate
        kS_NULL,         // xdeathstate
        ksfx_firxpl,     // deathsound
        10 * kFracUnit,  // speed
        6 * kFracUnit,   // radius
        32 * kFracUnit,  // height
        100,             // mass
        3,               // damage
        ksfx_None,       // activesound
        kMF_NOBLOCKMAP | kMF_MISSILE | kMF_DROPOFF | kMF_NOGRAVITY |
            kMF_NOCLIP,  // flags
        0,               // MBF21 flags
        -2,              // Infighting group
        -2,              // Projectile group
        -2,              // Splash group
        ksfx_None,       // Rip sound
        0,               // Fast speed
        0,               // Melee range
        0,               // Gib health
        -1,              // Dropped item
        0,               // Pickup width
        0,               // Projectile pass height
        0,               // Fullbright
        kS_NULL          // raisestate
    },

    // MT_SPAWNFIRE
    {
        "*SPAWNFIRE",                    // name
        -1,                              // doomednum
        kS_SPAWNFIRE1,                   // spawnstate
        1000,                            // spawnhealth
        kS_NULL,                         // seestate
        ksfx_None,                       // seesound
        8,                               // reactiontime
        ksfx_None,                       // attacksound
        kS_NULL,                         // painstate
        0,                               // painchance
        ksfx_None,                       // painsound
        kS_NULL,                         // meleestate
        kS_NULL,                         // missilestate
        kS_NULL,                         // deathstate
        kS_NULL,                         // xdeathstate
        ksfx_None,                       // deathsound
        0,                               // speed
        20 * kFracUnit,                  // radius
        16 * kFracUnit,                  // height
        100,                             // mass
        0,                               // damage
        ksfx_None,                       // activesound
        kMF_NOBLOCKMAP | kMF_NOGRAVITY,  // flags
        0,                               // MBF21 flags
        -2,                              // Infighting group
        -2,                              // Projectile group
        -2,                              // Splash group
        ksfx_None,                       // Rip sound
        0,                               // Fast speed
        0,                               // Melee range
        0,                               // Gib health
        -1,                              // Dropped item
        0,                               // Pickup width
        0,                               // Projectile pass height
        0,                               // Fullbright
        kS_NULL                          // raisestate
    },

    // MT_BARREL
    {
        "BARREL",                                 // name
        2035,                                     // doomednum
        kS_BAR1,                                  // spawnstate
        20,                                       // spawnhealth
        kS_NULL,                                  // seestate
        ksfx_None,                                // seesound
        8,                                        // reactiontime
        ksfx_None,                                // attacksound
        kS_NULL,                                  // painstate
        0,                                        // painchance
        ksfx_None,                                // painsound
        kS_NULL,                                  // meleestate
        kS_NULL,                                  // missilestate
        kS_BEXP,                                  // deathstate
        kS_NULL,                                  // xdeathstate
        ksfx_barexp,                              // deathsound
        0,                                        // speed
        10 * kFracUnit,                           // radius
        42 * kFracUnit,                           // height
        100,                                      // mass
        0,                                        // damage
        ksfx_None,                                // activesound
        kMF_SOLID | kMF_SHOOTABLE | kMF_NOBLOOD,  // flags
        0,                                        // MBF21 flags
        -2,                                       // Infighting group
        -2,                                       // Projectile group
        -2,                                       // Splash group
        ksfx_None,                                // Rip sound
        0,                                        // Fast speed
        0,                                        // Melee range
        0,                                        // Gib health
        -1,                                       // Dropped item
        0,                                        // Pickup width
        0,                                        // Projectile pass height
        0,                                        // Fullbright
        kS_NULL                                   // raisestate
    },

    // MT_TROOPSHOT
    {
        "*IMP_FIREBALL",  // (attack) name
        -1,               // doomednum
        kS_TBALL1,        // spawnstate
        1000,             // spawnhealth
        kS_NULL,          // seestate
        ksfx_firsht,      // seesound
        8,                // reactiontime
        ksfx_None,        // attacksound
        kS_NULL,          // painstate
        0,                // painchance
        ksfx_None,        // painsound
        kS_NULL,          // meleestate
        kS_NULL,          // missilestate
        kS_TBALLX1,       // deathstate
        kS_NULL,          // xdeathstate
        ksfx_firxpl,      // deathsound
        10 * kFracUnit,   // speed
        6 * kFracUnit,    // radius
        8 * kFracUnit,    // height
        100,              // mass
        3,                // damage
        ksfx_None,        // activesound
        kMF_NOBLOCKMAP | kMF_MISSILE | kMF_DROPOFF | kMF_NOGRAVITY,  // flags
        0,          // MBF21 flags
        -2,         // Infighting group
        -2,         // Projectile group
        -2,         // Splash group
        ksfx_None,  // Rip sound
        0,          // Fast speed
        0,          // Melee range
        0,          // Gib health
        -1,         // Dropped item
        0,          // Pickup width
        0,          // Projectile pass height
        0,          // Fullbright
        kS_NULL     // raisestate
    },

    // MT_HEADSHOT
    {
        "*CACO_FIREBALL",  // (attack) name
        -1,                // doomednum
        kS_RBALL1,         // spawnstate
        1000,              // spawnhealth
        kS_NULL,           // seestate
        ksfx_firsht,       // seesound
        8,                 // reactiontime
        ksfx_None,         // attacksound
        kS_NULL,           // painstate
        0,                 // painchance
        ksfx_None,         // painsound
        kS_NULL,           // meleestate
        kS_NULL,           // missilestate
        kS_RBALLX1,        // deathstate
        kS_NULL,           // xdeathstate
        ksfx_firxpl,       // deathsound
        10 * kFracUnit,    // speed
        6 * kFracUnit,     // radius
        8 * kFracUnit,     // height
        100,               // mass
        5,                 // damage
        ksfx_None,         // activesound
        kMF_NOBLOCKMAP | kMF_MISSILE | kMF_DROPOFF | kMF_NOGRAVITY,  // flags
        0,          // MBF21 flags
        -2,         // Infighting group
        -2,         // Projectile group
        -2,         // Splash group
        ksfx_None,  // Rip sound
        0,          // Fast speed
        0,          // Melee range
        0,          // Gib health
        -1,         // Dropped item
        0,          // Pickup width
        0,          // Projectile pass height
        0,          // Fullbright
        kS_NULL     // raisestate
    },

    // MT_ROCKET
    {
        "*CYBERDEMON_MISSILE",  // (attack) name
        -1,                     // doomednum
        kS_ROCKET,              // spawnstate
        1000,                   // spawnhealth
        kS_NULL,                // seestate
        ksfx_rlaunc,            // seesound
        8,                      // reactiontime
        ksfx_None,              // attacksound
        kS_NULL,                // painstate
        0,                      // painchance
        ksfx_None,              // painsound
        kS_NULL,                // meleestate
        kS_NULL,                // missilestate
        kS_EXPLODE1,            // deathstate
        kS_NULL,                // xdeathstate
        ksfx_barexp,            // deathsound
        20 * kFracUnit,         // speed
        11 * kFracUnit,         // radius
        8 * kFracUnit,          // height
        100,                    // mass
        20,                     // damage
        ksfx_None,              // activesound
        kMF_NOBLOCKMAP | kMF_MISSILE | kMF_DROPOFF | kMF_NOGRAVITY,  // flags
        0,          // MBF21 flags
        -2,         // Infighting group
        -2,         // Projectile group
        -2,         // Splash group
        ksfx_None,  // Rip sound
        0,          // Fast speed
        0,          // Melee range
        0,          // Gib health
        -1,         // Dropped item
        0,          // Pickup width
        0,          // Projectile pass height
        0,          // Fullbright
        kS_NULL     // raisestate
    },

    // MT_PLASMA
    {
        "*PLAYER_PLASMA",  // (attack) name
        -1,                // doomednum
        kS_PLASBALL,       // spawnstate
        1000,              // spawnhealth
        kS_NULL,           // seestate
        ksfx_plasma,       // seesound
        8,                 // reactiontime
        ksfx_None,         // attacksound
        kS_NULL,           // painstate
        0,                 // painchance
        ksfx_None,         // painsound
        kS_NULL,           // meleestate
        kS_NULL,           // missilestate
        kS_PLASEXP,        // deathstate
        kS_NULL,           // xdeathstate
        ksfx_firxpl,       // deathsound
        25 * kFracUnit,    // speed
        13 * kFracUnit,    // radius
        8 * kFracUnit,     // height
        100,               // mass
        5,                 // damage
        ksfx_None,         // activesound
        kMF_NOBLOCKMAP | kMF_MISSILE | kMF_DROPOFF | kMF_NOGRAVITY,  // flags
        0,          // MBF21 flags
        -2,         // Infighting group
        -2,         // Projectile group
        -2,         // Splash group
        ksfx_None,  // Rip sound
        0,          // Fast speed
        0,          // Melee range
        0,          // Gib health
        -1,         // Dropped item
        0,          // Pickup width
        0,          // Projectile pass height
        0,          // Fullbright
        kS_NULL     // raisestate
    },

    // MT_BFG
    {
        "*PLAYER_BFG9000",  // (attack) name
        -1,                 // doomednum
        kS_BFGSHOT,         // spawnstate
        1000,               // spawnhealth
        kS_NULL,            // seestate
        0,                  // seesound
        8,                  // reactiontime
        ksfx_None,          // attacksound
        kS_NULL,            // painstate
        0,                  // painchance
        ksfx_None,          // painsound
        kS_NULL,            // meleestate
        kS_NULL,            // missilestate
        kS_BFGLAND,         // deathstate
        kS_NULL,            // xdeathstate
        ksfx_rxplod,        // deathsound
        25 * kFracUnit,     // speed
        13 * kFracUnit,     // radius
        8 * kFracUnit,      // height
        100,                // mass
        100,                // damage
        ksfx_None,          // activesound
        kMF_NOBLOCKMAP | kMF_MISSILE | kMF_DROPOFF | kMF_NOGRAVITY,  // flags
        0,          // MBF21 flags
        -2,         // Infighting group
        -2,         // Projectile group
        -2,         // Splash group
        ksfx_None,  // Rip sound
        0,          // Fast speed
        0,          // Melee range
        0,          // Gib health
        -1,         // Dropped item
        0,          // Pickup width
        0,          // Projectile pass height
        0,          // Fullbright
        kS_NULL     // raisestate
    },

    // MT_ARACHPLAZ
    {
        "*ARACHNOTRON_PLASMA",  // name
        -1,                     // doomednum
        kS_ARACH_PLAZ,          // spawnstate
        1000,                   // spawnhealth
        kS_NULL,                // seestate
        ksfx_plasma,            // seesound
        8,                      // reactiontime
        ksfx_None,              // attacksound
        kS_NULL,                // painstate
        0,                      // painchance
        ksfx_None,              // painsound
        kS_NULL,                // meleestate
        kS_NULL,                // missilestate
        kS_ARACH_PLEX,          // deathstate
        kS_NULL,                // xdeathstate
        ksfx_firxpl,            // deathsound
        25 * kFracUnit,         // speed
        13 * kFracUnit,         // radius
        8 * kFracUnit,          // height
        100,                    // mass
        5,                      // damage
        ksfx_None,              // activesound
        kMF_NOBLOCKMAP | kMF_MISSILE | kMF_DROPOFF | kMF_NOGRAVITY,  // flags
        0,          // MBF21 flags
        -2,         // Infighting group
        -2,         // Projectile group
        -2,         // Splash group
        ksfx_None,  // Rip sound
        0,          // Fast speed
        0,          // Melee range
        0,          // Gib health
        -1,         // Dropped item
        0,          // Pickup width
        0,          // Projectile pass height
        0,          // Fullbright
        kS_NULL     // raisestate
    },

    // MT_PUFF
    {
        "PUFF",                          // name
        -1,                              // doomednum
        kS_PUFF1,                        // spawnstate
        1000,                            // spawnhealth
        kS_NULL,                         // seestate
        ksfx_None,                       // seesound
        8,                               // reactiontime
        ksfx_None,                       // attacksound
        kS_NULL,                         // painstate
        0,                               // painchance
        ksfx_None,                       // painsound
        kS_NULL,                         // meleestate
        kS_NULL,                         // missilestate
        kS_NULL,                         // deathstate
        kS_NULL,                         // xdeathstate
        ksfx_None,                       // deathsound
        0,                               // speed
        20 * kFracUnit,                  // radius
        16 * kFracUnit,                  // height
        100,                             // mass
        0,                               // damage
        ksfx_None,                       // activesound
        kMF_NOBLOCKMAP | kMF_NOGRAVITY,  // flags
        0,                               // MBF21 flags
        -2,                              // Infighting group
        -2,                              // Projectile group
        -2,                              // Splash group
        ksfx_None,                       // Rip sound
        0,                               // Fast speed
        0,                               // Melee range
        0,                               // Gib health
        -1,                              // Dropped item
        0,                               // Pickup width
        0,                               // Projectile pass height
        0,                               // Fullbright
        kS_NULL                          // raisestate
    },

    // MT_BLOOD
    {
        "BLOOD",         // name
        -1,              // doomednum
        kS_BLOOD1,       // spawnstate
        1000,            // spawnhealth
        kS_NULL,         // seestate
        ksfx_None,       // seesound
        8,               // reactiontime
        ksfx_None,       // attacksound
        kS_NULL,         // painstate
        0,               // painchance
        ksfx_None,       // painsound
        kS_NULL,         // meleestate
        kS_NULL,         // missilestate
        kS_NULL,         // deathstate
        kS_NULL,         // xdeathstate
        ksfx_None,       // deathsound
        0,               // speed
        20 * kFracUnit,  // radius
        16 * kFracUnit,  // height
        100,             // mass
        0,               // damage
        ksfx_None,       // activesound
        kMF_NOBLOCKMAP,  // flags
        0,               // MBF21 flags
        -2,              // Infighting group
        -2,              // Projectile group
        -2,              // Splash group
        ksfx_None,       // Rip sound
        0,               // Fast speed
        0,               // Melee range
        0,               // Gib health
        -1,              // Dropped item
        0,               // Pickup width
        0,               // Projectile pass height
        0,               // Fullbright
        kS_NULL          // raisestate
    },

    // MT_TFOG
    {
        "TELEPORT_FOG",                  // name
        -1,                              // doomednum
        kS_TFOG,                         // spawnstate
        1000,                            // spawnhealth
        kS_NULL,                         // seestate
        ksfx_None,                       // seesound
        8,                               // reactiontime
        ksfx_None,                       // attacksound
        kS_NULL,                         // painstate
        0,                               // painchance
        ksfx_None,                       // painsound
        kS_NULL,                         // meleestate
        kS_NULL,                         // missilestate
        kS_NULL,                         // deathstate
        kS_NULL,                         // xdeathstate
        ksfx_None,                       // deathsound
        0,                               // speed
        20 * kFracUnit,                  // radius
        16 * kFracUnit,                  // height
        100,                             // mass
        0,                               // damage
        ksfx_None,                       // activesound
        kMF_NOBLOCKMAP | kMF_NOGRAVITY,  // flags
        0,                               // MBF21 flags
        -2,                              // Infighting group
        -2,                              // Projectile group
        -2,                              // Splash group
        ksfx_None,                       // Rip sound
        0,                               // Fast speed
        0,                               // Melee range
        0,                               // Gib health
        -1,                              // Dropped item
        0,                               // Pickup width
        0,                               // Projectile pass height
        0,                               // Fullbright
        kS_NULL                          // raisestate
    },

    // MT_IFOG
    {
        "RESPAWN_FOG",                   // name
        -1,                              // doomednum
        kS_IFOG,                         // spawnstate
        1000,                            // spawnhealth
        kS_NULL,                         // seestate
        ksfx_None,                       // seesound
        8,                               // reactiontime
        ksfx_None,                       // attacksound
        kS_NULL,                         // painstate
        0,                               // painchance
        ksfx_None,                       // painsound
        kS_NULL,                         // meleestate
        kS_NULL,                         // missilestate
        kS_NULL,                         // deathstate
        kS_NULL,                         // xdeathstate
        ksfx_None,                       // deathsound
        0,                               // speed
        20 * kFracUnit,                  // radius
        16 * kFracUnit,                  // height
        100,                             // mass
        0,                               // damage
        ksfx_None,                       // activesound
        kMF_NOBLOCKMAP | kMF_NOGRAVITY,  // flags
        0,                               // MBF21 flags
        -2,                              // Infighting group
        -2,                              // Projectile group
        -2,                              // Splash group
        ksfx_None,                       // Rip sound
        0,                               // Fast speed
        0,                               // Melee range
        0,                               // Gib health
        -1,                              // Dropped item
        0,                               // Pickup width
        0,                               // Projectile pass height
        0,                               // Fullbright
        kS_NULL                          // raisestate
    },

    // MT_TELEPORTMAN
    {
        "TELEPORT_FLASH",               // name
        14,                             // doomednum
        kS_NULL,                        // spawnstate
        1000,                           // spawnhealth
        kS_NULL,                        // seestate
        ksfx_None,                      // seesound
        8,                              // reactiontime
        ksfx_None,                      // attacksound
        kS_NULL,                        // painstate
        0,                              // painchance
        ksfx_None,                      // painsound
        kS_NULL,                        // meleestate
        kS_NULL,                        // missilestate
        kS_NULL,                        // deathstate
        kS_NULL,                        // xdeathstate
        ksfx_None,                      // deathsound
        0,                              // speed
        20 * kFracUnit,                 // radius
        16 * kFracUnit,                 // height
        100,                            // mass
        0,                              // damage
        ksfx_None,                      // activesound
        kMF_NOBLOCKMAP | kMF_NOSECTOR,  // flags
        0,                              // MBF21 flags
        -2,                             // Infighting group
        -2,                             // Projectile group
        -2,                             // Splash group
        ksfx_None,                      // Rip sound
        0,                              // Fast speed
        0,                              // Melee range
        0,                              // Gib health
        -1,                             // Dropped item
        0,                              // Pickup width
        0,                              // Projectile pass height
        0,                              // Fullbright
        kS_NULL                         // raisestate
    },

    // MT_EXTRABFG
    {
        "*BFG9000_SPRAY",                // name
        -1,                              // doomednum
        kS_BFGEXP,                       // spawnstate
        1000,                            // spawnhealth
        kS_NULL,                         // seestate
        ksfx_None,                       // seesound
        8,                               // reactiontime
        ksfx_None,                       // attacksound
        kS_NULL,                         // painstate
        0,                               // painchance
        ksfx_None,                       // painsound
        kS_NULL,                         // meleestate
        kS_NULL,                         // missilestate
        kS_NULL,                         // deathstate
        kS_NULL,                         // xdeathstate
        ksfx_None,                       // deathsound
        0,                               // speed
        20 * kFracUnit,                  // radius
        16 * kFracUnit,                  // height
        100,                             // mass
        0,                               // damage
        ksfx_None,                       // activesound
        kMF_NOBLOCKMAP | kMF_NOGRAVITY,  // flags
        0,                               // MBF21 flags
        -2,                              // Infighting group
        -2,                              // Projectile group
        -2,                              // Splash group
        ksfx_None,                       // Rip sound
        0,                               // Fast speed
        0,                               // Melee range
        0,                               // Gib health
        -1,                              // Dropped item
        0,                               // Pickup width
        0,                               // Projectile pass height
        0,                               // Fullbright
        kS_NULL                          // raisestate
    },

    // MT_MISC0
    {
        "GREEN_ARMOUR",  // name
        2018,            // doomednum
        kS_ARM1,         // spawnstate
        1000,            // spawnhealth
        kS_NULL,         // seestate
        ksfx_None,       // seesound
        8,               // reactiontime
        ksfx_None,       // attacksound
        kS_NULL,         // painstate
        0,               // painchance
        ksfx_None,       // painsound
        kS_NULL,         // meleestate
        kS_NULL,         // missilestate
        kS_NULL,         // deathstate
        kS_NULL,         // xdeathstate
        ksfx_None,       // deathsound
        0,               // speed
        20 * kFracUnit,  // radius
        16 * kFracUnit,  // height
        100,             // mass
        0,               // damage
        ksfx_None,       // activesound
        kMF_SPECIAL,     // flags
        0,               // MBF21 flags
        -2,              // Infighting group
        -2,              // Projectile group
        -2,              // Splash group
        ksfx_None,       // Rip sound
        0,               // Fast speed
        0,               // Melee range
        0,               // Gib health
        -1,              // Dropped item
        0,               // Pickup width
        0,               // Projectile pass height
        0,               // Fullbright
        kS_NULL          // raisestate
    },

    // MT_MISC1
    {
        "BLUE_ARMOUR",   // name
        2019,            // doomednum
        kS_ARM2,         // spawnstate
        1000,            // spawnhealth
        kS_NULL,         // seestate
        ksfx_None,       // seesound
        8,               // reactiontime
        ksfx_None,       // attacksound
        kS_NULL,         // painstate
        0,               // painchance
        ksfx_None,       // painsound
        kS_NULL,         // meleestate
        kS_NULL,         // missilestate
        kS_NULL,         // deathstate
        kS_NULL,         // xdeathstate
        ksfx_None,       // deathsound
        0,               // speed
        20 * kFracUnit,  // radius
        16 * kFracUnit,  // height
        100,             // mass
        0,               // damage
        ksfx_None,       // activesound
        kMF_SPECIAL,     // flags
        0,               // MBF21 flags
        -2,              // Infighting group
        -2,              // Projectile group
        -2,              // Splash group
        ksfx_None,       // Rip sound
        0,               // Fast speed
        0,               // Melee range
        0,               // Gib health
        -1,              // Dropped item
        0,               // Pickup width
        0,               // Projectile pass height
        0,               // Fullbright
        kS_NULL          // raisestate
    },

    // MT_MISC2
    {
        "HEALTH_POTION",              // name
        2014,                         // doomednum
        kS_BON1,                      // spawnstate
        1000,                         // spawnhealth
        kS_NULL,                      // seestate
        ksfx_None,                    // seesound
        8,                            // reactiontime
        ksfx_None,                    // attacksound
        kS_NULL,                      // painstate
        0,                            // painchance
        ksfx_None,                    // painsound
        kS_NULL,                      // meleestate
        kS_NULL,                      // missilestate
        kS_NULL,                      // deathstate
        kS_NULL,                      // xdeathstate
        ksfx_None,                    // deathsound
        0,                            // speed
        20 * kFracUnit,               // radius
        16 * kFracUnit,               // height
        100,                          // mass
        0,                            // damage
        ksfx_None,                    // activesound
        kMF_SPECIAL | kMF_COUNTITEM,  // flags
        0,                            // MBF21 flags
        -2,                           // Infighting group
        -2,                           // Projectile group
        -2,                           // Splash group
        ksfx_None,                    // Rip sound
        0,                            // Fast speed
        0,                            // Melee range
        0,                            // Gib health
        -1,                           // Dropped item
        0,                            // Pickup width
        0,                            // Projectile pass height
        0,                            // Fullbright
        kS_NULL                       // raisestate
    },

    // MT_MISC3
    {
        "ARMOUR_HELMET",              // name
        2015,                         // doomednum
        kS_BON2,                      // spawnstate
        1000,                         // spawnhealth
        kS_NULL,                      // seestate
        ksfx_None,                    // seesound
        8,                            // reactiontime
        ksfx_None,                    // attacksound
        kS_NULL,                      // painstate
        0,                            // painchance
        ksfx_None,                    // painsound
        kS_NULL,                      // meleestate
        kS_NULL,                      // missilestate
        kS_NULL,                      // deathstate
        kS_NULL,                      // xdeathstate
        ksfx_None,                    // deathsound
        0,                            // speed
        20 * kFracUnit,               // radius
        16 * kFracUnit,               // height
        100,                          // mass
        0,                            // damage
        ksfx_None,                    // activesound
        kMF_SPECIAL | kMF_COUNTITEM,  // flags
        0,                            // MBF21 flags
        -2,                           // Infighting group
        -2,                           // Projectile group
        -2,                           // Splash group
        ksfx_None,                    // Rip sound
        0,                            // Fast speed
        0,                            // Melee range
        0,                            // Gib health
        -1,                           // Dropped item
        0,                            // Pickup width
        0,                            // Projectile pass height
        0,                            // Fullbright
        kS_NULL                       // raisestate
    },

    // MT_MISC4
    {
        "BLUE_KEY",                   // name
        5,                            // doomednum
        kS_BKEY,                      // spawnstate
        1000,                         // spawnhealth
        kS_NULL,                      // seestate
        ksfx_None,                    // seesound
        8,                            // reactiontime
        ksfx_None,                    // attacksound
        kS_NULL,                      // painstate
        0,                            // painchance
        ksfx_None,                    // painsound
        kS_NULL,                      // meleestate
        kS_NULL,                      // missilestate
        kS_NULL,                      // deathstate
        kS_NULL,                      // xdeathstate
        ksfx_None,                    // deathsound
        0,                            // speed
        20 * kFracUnit,               // radius
        16 * kFracUnit,               // height
        100,                          // mass
        0,                            // damage
        ksfx_None,                    // activesound
        kMF_SPECIAL | kMF_NOTDMATCH,  // flags
        0,                            // MBF21 flags
        -2,                           // Infighting group
        -2,                           // Projectile group
        -2,                           // Splash group
        ksfx_None,                    // Rip sound
        0,                            // Fast speed
        0,                            // Melee range
        0,                            // Gib health
        -1,                           // Dropped item
        0,                            // Pickup width
        0,                            // Projectile pass height
        0,                            // Fullbright
        kS_NULL                       // raisestate
    },

    // MT_MISC5
    {
        "RED_KEY",                    // name
        13,                           // doomednum
        kS_RKEY,                      // spawnstate
        1000,                         // spawnhealth
        kS_NULL,                      // seestate
        ksfx_None,                    // seesound
        8,                            // reactiontime
        ksfx_None,                    // attacksound
        kS_NULL,                      // painstate
        0,                            // painchance
        ksfx_None,                    // painsound
        kS_NULL,                      // meleestate
        kS_NULL,                      // missilestate
        kS_NULL,                      // deathstate
        kS_NULL,                      // xdeathstate
        ksfx_None,                    // deathsound
        0,                            // speed
        20 * kFracUnit,               // radius
        16 * kFracUnit,               // height
        100,                          // mass
        0,                            // damage
        ksfx_None,                    // activesound
        kMF_SPECIAL | kMF_NOTDMATCH,  // flags
        0,                            // MBF21 flags
        -2,                           // Infighting group
        -2,                           // Projectile group
        -2,                           // Splash group
        ksfx_None,                    // Rip sound
        0,                            // Fast speed
        0,                            // Melee range
        0,                            // Gib health
        -1,                           // Dropped item
        0,                            // Pickup width
        0,                            // Projectile pass height
        0,                            // Fullbright
        kS_NULL                       // raisestate
    },

    // MT_MISC6
    {
        "YELLOW_KEY",                 // name
        6,                            // doomednum
        kS_YKEY,                      // spawnstate
        1000,                         // spawnhealth
        kS_NULL,                      // seestate
        ksfx_None,                    // seesound
        8,                            // reactiontime
        ksfx_None,                    // attacksound
        kS_NULL,                      // painstate
        0,                            // painchance
        ksfx_None,                    // painsound
        kS_NULL,                      // meleestate
        kS_NULL,                      // missilestate
        kS_NULL,                      // deathstate
        kS_NULL,                      // xdeathstate
        ksfx_None,                    // deathsound
        0,                            // speed
        20 * kFracUnit,               // radius
        16 * kFracUnit,               // height
        100,                          // mass
        0,                            // damage
        ksfx_None,                    // activesound
        kMF_SPECIAL | kMF_NOTDMATCH,  // flags
        0,                            // MBF21 flags
        -2,                           // Infighting group
        -2,                           // Projectile group
        -2,                           // Splash group
        ksfx_None,                    // Rip sound
        0,                            // Fast speed
        0,                            // Melee range
        0,                            // Gib health
        -1,                           // Dropped item
        0,                            // Pickup width
        0,                            // Projectile pass height
        0,                            // Fullbright
        kS_NULL                       // raisestate
    },

    // MT_MISC7
    {
        "YELLOW_SKULLKEY",            // name
        39,                           // doomednum
        kS_YSKULL,                    // spawnstate
        1000,                         // spawnhealth
        kS_NULL,                      // seestate
        ksfx_None,                    // seesound
        8,                            // reactiontime
        ksfx_None,                    // attacksound
        kS_NULL,                      // painstate
        0,                            // painchance
        ksfx_None,                    // painsound
        kS_NULL,                      // meleestate
        kS_NULL,                      // missilestate
        kS_NULL,                      // deathstate
        kS_NULL,                      // xdeathstate
        ksfx_None,                    // deathsound
        0,                            // speed
        20 * kFracUnit,               // radius
        16 * kFracUnit,               // height
        100,                          // mass
        0,                            // damage
        ksfx_None,                    // activesound
        kMF_SPECIAL | kMF_NOTDMATCH,  // flags
        0,                            // MBF21 flags
        -2,                           // Infighting group
        -2,                           // Projectile group
        -2,                           // Splash group
        ksfx_None,                    // Rip sound
        0,                            // Fast speed
        0,                            // Melee range
        0,                            // Gib health
        -1,                           // Dropped item
        0,                            // Pickup width
        0,                            // Projectile pass height
        0,                            // Fullbright
        kS_NULL                       // raisestate
    },

    // MT_MISC8
    {
        "RED_SKULLKEY",               // name
        38,                           // doomednum
        kS_RSKULL,                    // spawnstate
        1000,                         // spawnhealth
        kS_NULL,                      // seestate
        ksfx_None,                    // seesound
        8,                            // reactiontime
        ksfx_None,                    // attacksound
        kS_NULL,                      // painstate
        0,                            // painchance
        ksfx_None,                    // painsound
        kS_NULL,                      // meleestate
        kS_NULL,                      // missilestate
        kS_NULL,                      // deathstate
        kS_NULL,                      // xdeathstate
        ksfx_None,                    // deathsound
        0,                            // speed
        20 * kFracUnit,               // radius
        16 * kFracUnit,               // height
        100,                          // mass
        0,                            // damage
        ksfx_None,                    // activesound
        kMF_SPECIAL | kMF_NOTDMATCH,  // flags
        0,                            // MBF21 flags
        -2,                           // Infighting group
        -2,                           // Projectile group
        -2,                           // Splash group
        ksfx_None,                    // Rip sound
        0,                            // Fast speed
        0,                            // Melee range
        0,                            // Gib health
        -1,                           // Dropped item
        0,                            // Pickup width
        0,                            // Projectile pass height
        0,                            // Fullbright
        kS_NULL                       // raisestate
    },

    // MT_MISC9
    {
        "BLUE_SKULLKEY",              // name
        40,                           // doomednum
        kS_BSKULL,                    // spawnstate
        1000,                         // spawnhealth
        kS_NULL,                      // seestate
        ksfx_None,                    // seesound
        8,                            // reactiontime
        ksfx_None,                    // attacksound
        kS_NULL,                      // painstate
        0,                            // painchance
        ksfx_None,                    // painsound
        kS_NULL,                      // meleestate
        kS_NULL,                      // missilestate
        kS_NULL,                      // deathstate
        kS_NULL,                      // xdeathstate
        ksfx_None,                    // deathsound
        0,                            // speed
        20 * kFracUnit,               // radius
        16 * kFracUnit,               // height
        100,                          // mass
        0,                            // damage
        ksfx_None,                    // activesound
        kMF_SPECIAL | kMF_NOTDMATCH,  // flags
        0,                            // MBF21 flags
        -2,                           // Infighting group
        -2,                           // Projectile group
        -2,                           // Splash group
        ksfx_None,                    // Rip sound
        0,                            // Fast speed
        0,                            // Melee range
        0,                            // Gib health
        -1,                           // Dropped item
        0,                            // Pickup width
        0,                            // Projectile pass height
        0,                            // Fullbright
        kS_NULL                       // raisestate
    },

    // MT_MISC10
    {
        "STIMPACK",      // name
        2011,            // doomednum
        kS_STIM,         // spawnstate
        1000,            // spawnhealth
        kS_NULL,         // seestate
        ksfx_None,       // seesound
        8,               // reactiontime
        ksfx_None,       // attacksound
        kS_NULL,         // painstate
        0,               // painchance
        ksfx_None,       // painsound
        kS_NULL,         // meleestate
        kS_NULL,         // missilestate
        kS_NULL,         // deathstate
        kS_NULL,         // xdeathstate
        ksfx_None,       // deathsound
        0,               // speed
        20 * kFracUnit,  // radius
        16 * kFracUnit,  // height
        100,             // mass
        0,               // damage
        ksfx_None,       // activesound
        kMF_SPECIAL,     // flags
        0,               // MBF21 flags
        -2,              // Infighting group
        -2,              // Projectile group
        -2,              // Splash group
        ksfx_None,       // Rip sound
        0,               // Fast speed
        0,               // Melee range
        0,               // Gib health
        -1,              // Dropped item
        0,               // Pickup width
        0,               // Projectile pass height
        0,               // Fullbright
        kS_NULL          // raisestate
    },

    // MT_MISC11
    {
        "MEDIKIT",       // name
        2012,            // doomednum
        kS_MEDI,         // spawnstate
        1000,            // spawnhealth
        kS_NULL,         // seestate
        ksfx_None,       // seesound
        8,               // reactiontime
        ksfx_None,       // attacksound
        kS_NULL,         // painstate
        0,               // painchance
        ksfx_None,       // painsound
        kS_NULL,         // meleestate
        kS_NULL,         // missilestate
        kS_NULL,         // deathstate
        kS_NULL,         // xdeathstate
        ksfx_None,       // deathsound
        0,               // speed
        20 * kFracUnit,  // radius
        16 * kFracUnit,  // height
        100,             // mass
        0,               // damage
        ksfx_None,       // activesound
        kMF_SPECIAL,     // flags
        0,               // MBF21 flags
        -2,              // Infighting group
        -2,              // Projectile group
        -2,              // Splash group
        ksfx_None,       // Rip sound
        0,               // Fast speed
        0,               // Melee range
        0,               // Gib health
        -1,              // Dropped item
        0,               // Pickup width
        0,               // Projectile pass height
        0,               // Fullbright
        kS_NULL          // raisestate
    },

    // MT_MISC12
    {
        "SOULSPHERE",                 // name
        2013,                         // doomednum
        kS_SOUL,                      // spawnstate
        1000,                         // spawnhealth
        kS_NULL,                      // seestate
        ksfx_None,                    // seesound
        8,                            // reactiontime
        ksfx_None,                    // attacksound
        kS_NULL,                      // painstate
        0,                            // painchance
        ksfx_None,                    // painsound
        kS_NULL,                      // meleestate
        kS_NULL,                      // missilestate
        kS_NULL,                      // deathstate
        kS_NULL,                      // xdeathstate
        ksfx_None,                    // deathsound
        0,                            // speed
        20 * kFracUnit,               // radius
        16 * kFracUnit,               // height
        100,                          // mass
        0,                            // damage
        ksfx_None,                    // activesound
        kMF_SPECIAL | kMF_COUNTITEM,  // flags
        0,                            // MBF21 flags
        -2,                           // Infighting group
        -2,                           // Projectile group
        -2,                           // Splash group
        ksfx_None,                    // Rip sound
        0,                            // Fast speed
        0,                            // Melee range
        0,                            // Gib health
        -1,                           // Dropped item
        0,                            // Pickup width
        0,                            // Projectile pass height
        0,                            // Fullbright
        kS_NULL                       // raisestate
    },

    // MT_INV
    {
        "INVULNERABILITY_SPHERE",     // name
        2022,                         // doomednum
        kS_PINV,                      // spawnstate
        1000,                         // spawnhealth
        kS_NULL,                      // seestate
        ksfx_None,                    // seesound
        8,                            // reactiontime
        ksfx_None,                    // attacksound
        kS_NULL,                      // painstate
        0,                            // painchance
        ksfx_None,                    // painsound
        kS_NULL,                      // meleestate
        kS_NULL,                      // missilestate
        kS_NULL,                      // deathstate
        kS_NULL,                      // xdeathstate
        ksfx_None,                    // deathsound
        0,                            // speed
        20 * kFracUnit,               // radius
        16 * kFracUnit,               // height
        100,                          // mass
        0,                            // damage
        ksfx_None,                    // activesound
        kMF_SPECIAL | kMF_COUNTITEM,  // flags
        0,                            // MBF21 flags
        -2,                           // Infighting group
        -2,                           // Projectile group
        -2,                           // Splash group
        ksfx_None,                    // Rip sound
        0,                            // Fast speed
        0,                            // Melee range
        0,                            // Gib health
        -1,                           // Dropped item
        0,                            // Pickup width
        0,                            // Projectile pass height
        0,                            // Fullbright
        kS_NULL                       // raisestate
    },

    // MT_MISC13
    {
        "BERSERKER",                  // name
        2023,                         // doomednum
        kS_PSTR,                      // spawnstate
        1000,                         // spawnhealth
        kS_NULL,                      // seestate
        ksfx_None,                    // seesound
        8,                            // reactiontime
        ksfx_None,                    // attacksound
        kS_NULL,                      // painstate
        0,                            // painchance
        ksfx_None,                    // painsound
        kS_NULL,                      // meleestate
        kS_NULL,                      // missilestate
        kS_NULL,                      // deathstate
        kS_NULL,                      // xdeathstate
        ksfx_None,                    // deathsound
        0,                            // speed
        20 * kFracUnit,               // radius
        16 * kFracUnit,               // height
        100,                          // mass
        0,                            // damage
        ksfx_None,                    // activesound
        kMF_SPECIAL | kMF_COUNTITEM,  // flags
        0,                            // MBF21 flags
        -2,                           // Infighting group
        -2,                           // Projectile group
        -2,                           // Splash group
        ksfx_None,                    // Rip sound
        0,                            // Fast speed
        0,                            // Melee range
        0,                            // Gib health
        -1,                           // Dropped item
        0,                            // Pickup width
        0,                            // Projectile pass height
        0,                            // Fullbright
        kS_NULL                       // raisestate
    },

    // MT_INS
    {
        "BLURSPHERE",                 // name
        2024,                         // doomednum
        kS_PINS,                      // spawnstate
        1000,                         // spawnhealth
        kS_NULL,                      // seestate
        ksfx_None,                    // seesound
        8,                            // reactiontime
        ksfx_None,                    // attacksound
        kS_NULL,                      // painstate
        0,                            // painchance
        ksfx_None,                    // painsound
        kS_NULL,                      // meleestate
        kS_NULL,                      // missilestate
        kS_NULL,                      // deathstate
        kS_NULL,                      // xdeathstate
        ksfx_None,                    // deathsound
        0,                            // speed
        20 * kFracUnit,               // radius
        16 * kFracUnit,               // height
        100,                          // mass
        0,                            // damage
        ksfx_None,                    // activesound
        kMF_SPECIAL | kMF_COUNTITEM,  // flags
        0,                            // MBF21 flags
        -2,                           // Infighting group
        -2,                           // Projectile group
        -2,                           // Splash group
        ksfx_None,                    // Rip sound
        0,                            // Fast speed
        0,                            // Melee range
        0,                            // Gib health
        -1,                           // Dropped item
        0,                            // Pickup width
        0,                            // Projectile pass height
        0,                            // Fullbright
        kS_NULL                       // raisestate
    },

    // MT_MISC14
    {
        "RADIATION_SUIT",  // name
        2025,              // doomednum
        kS_SUIT,           // spawnstate
        1000,              // spawnhealth
        kS_NULL,           // seestate
        ksfx_None,         // seesound
        8,                 // reactiontime
        ksfx_None,         // attacksound
        kS_NULL,           // painstate
        0,                 // painchance
        ksfx_None,         // painsound
        kS_NULL,           // meleestate
        kS_NULL,           // missilestate
        kS_NULL,           // deathstate
        kS_NULL,           // xdeathstate
        ksfx_None,         // deathsound
        0,                 // speed
        20 * kFracUnit,    // radius
        16 * kFracUnit,    // height
        100,               // mass
        0,                 // damage
        ksfx_None,         // activesound
        kMF_SPECIAL,       // flags
        0,                 // MBF21 flags
        -2,                // Infighting group
        -2,                // Projectile group
        -2,                // Splash group
        ksfx_None,         // Rip sound
        0,                 // Fast speed
        0,                 // Melee range
        0,                 // Gib health
        -1,                // Dropped item
        0,                 // Pickup width
        0,                 // Projectile pass height
        0,                 // Fullbright
        kS_NULL            // raisestate
    },

    // MT_MISC15
    {
        "AUTOMAP",                    // name
        2026,                         // doomednum
        kS_PMAP,                      // spawnstate
        1000,                         // spawnhealth
        kS_NULL,                      // seestate
        ksfx_None,                    // seesound
        8,                            // reactiontime
        ksfx_None,                    // attacksound
        kS_NULL,                      // painstate
        0,                            // painchance
        ksfx_None,                    // painsound
        kS_NULL,                      // meleestate
        kS_NULL,                      // missilestate
        kS_NULL,                      // deathstate
        kS_NULL,                      // xdeathstate
        ksfx_None,                    // deathsound
        0,                            // speed
        20 * kFracUnit,               // radius
        16 * kFracUnit,               // height
        100,                          // mass
        0,                            // damage
        ksfx_None,                    // activesound
        kMF_SPECIAL | kMF_COUNTITEM,  // flags
        0,                            // MBF21 flags
        -2,                           // Infighting group
        -2,                           // Projectile group
        -2,                           // Splash group
        ksfx_None,                    // Rip sound
        0,                            // Fast speed
        0,                            // Melee range
        0,                            // Gib health
        -1,                           // Dropped item
        0,                            // Pickup width
        0,                            // Projectile pass height
        0,                            // Fullbright
        kS_NULL                       // raisestate
    },

    // MT_MISC16
    {
        "LIGHT_SPECS",                // name
        2045,                         // doomednum
        kS_PVIS,                      // spawnstate
        1000,                         // spawnhealth
        kS_NULL,                      // seestate
        ksfx_None,                    // seesound
        8,                            // reactiontime
        ksfx_None,                    // attacksound
        kS_NULL,                      // painstate
        0,                            // painchance
        ksfx_None,                    // painsound
        kS_NULL,                      // meleestate
        kS_NULL,                      // missilestate
        kS_NULL,                      // deathstate
        kS_NULL,                      // xdeathstate
        ksfx_None,                    // deathsound
        0,                            // speed
        20 * kFracUnit,               // radius
        16 * kFracUnit,               // height
        100,                          // mass
        0,                            // damage
        ksfx_None,                    // activesound
        kMF_SPECIAL | kMF_COUNTITEM,  // flags
        0,                            // MBF21 flags
        -2,                           // Infighting group
        -2,                           // Projectile group
        -2,                           // Splash group
        ksfx_None,                    // Rip sound
        0,                            // Fast speed
        0,                            // Melee range
        0,                            // Gib health
        -1,                           // Dropped item
        0,                            // Pickup width
        0,                            // Projectile pass height
        0,                            // Fullbright
        kS_NULL                       // raisestate
    },

    // MT_MEGA
    {
        "MEGASPHERE",                 // name
        83,                           // doomednum
        kS_MEGA,                      // spawnstate
        1000,                         // spawnhealth
        kS_NULL,                      // seestate
        ksfx_None,                    // seesound
        8,                            // reactiontime
        ksfx_None,                    // attacksound
        kS_NULL,                      // painstate
        0,                            // painchance
        ksfx_None,                    // painsound
        kS_NULL,                      // meleestate
        kS_NULL,                      // missilestate
        kS_NULL,                      // deathstate
        kS_NULL,                      // xdeathstate
        ksfx_None,                    // deathsound
        0,                            // speed
        20 * kFracUnit,               // radius
        16 * kFracUnit,               // height
        100,                          // mass
        0,                            // damage
        ksfx_None,                    // activesound
        kMF_SPECIAL | kMF_COUNTITEM,  // flags
        0,                            // MBF21 flags
        -2,                           // Infighting group
        -2,                           // Projectile group
        -2,                           // Splash group
        ksfx_None,                    // Rip sound
        0,                            // Fast speed
        0,                            // Melee range
        0,                            // Gib health
        -1,                           // Dropped item
        0,                            // Pickup width
        0,                            // Projectile pass height
        0,                            // Fullbright
        kS_NULL                       // raisestate
    },

    // MT_CLIP
    {
        "CLIP",          // name
        2007,            // doomednum
        kS_CLIP,         // spawnstate
        1000,            // spawnhealth
        kS_NULL,         // seestate
        ksfx_None,       // seesound
        8,               // reactiontime
        ksfx_None,       // attacksound
        kS_NULL,         // painstate
        0,               // painchance
        ksfx_None,       // painsound
        kS_NULL,         // meleestate
        kS_NULL,         // missilestate
        kS_NULL,         // deathstate
        kS_NULL,         // xdeathstate
        ksfx_None,       // deathsound
        0,               // speed
        20 * kFracUnit,  // radius
        16 * kFracUnit,  // height
        100,             // mass
        0,               // damage
        ksfx_None,       // activesound
        kMF_SPECIAL,     // flags
        0,               // MBF21 flags
        -2,              // Infighting group
        -2,              // Projectile group
        -2,              // Splash group
        ksfx_None,       // Rip sound
        0,               // Fast speed
        0,               // Melee range
        0,               // Gib health
        -1,              // Dropped item
        0,               // Pickup width
        0,               // Projectile pass height
        0,               // Fullbright
        kS_NULL          // raisestate
    },

    // MT_MISC17
    {
        "BOX_OF_BULLETS",  // name
        2048,              // doomednum
        kS_AMMO,           // spawnstate
        1000,              // spawnhealth
        kS_NULL,           // seestate
        ksfx_None,         // seesound
        8,                 // reactiontime
        ksfx_None,         // attacksound
        kS_NULL,           // painstate
        0,                 // painchance
        ksfx_None,         // painsound
        kS_NULL,           // meleestate
        kS_NULL,           // missilestate
        kS_NULL,           // deathstate
        kS_NULL,           // xdeathstate
        ksfx_None,         // deathsound
        0,                 // speed
        20 * kFracUnit,    // radius
        16 * kFracUnit,    // height
        100,               // mass
        0,                 // damage
        ksfx_None,         // activesound
        kMF_SPECIAL,       // flags
        0,                 // MBF21 flags
        -2,                // Infighting group
        -2,                // Projectile group
        -2,                // Splash group
        ksfx_None,         // Rip sound
        0,                 // Fast speed
        0,                 // Melee range
        0,                 // Gib health
        -1,                // Dropped item
        0,                 // Pickup width
        0,                 // Projectile pass height
        0,                 // Fullbright
        kS_NULL            // raisestate
    },

    // MT_MISC18
    {
        "ROCKET",        // name
        2010,            // doomednum
        kS_ROCK,         // spawnstate
        1000,            // spawnhealth
        kS_NULL,         // seestate
        ksfx_None,       // seesound
        8,               // reactiontime
        ksfx_None,       // attacksound
        kS_NULL,         // painstate
        0,               // painchance
        ksfx_None,       // painsound
        kS_NULL,         // meleestate
        kS_NULL,         // missilestate
        kS_NULL,         // deathstate
        kS_NULL,         // xdeathstate
        ksfx_None,       // deathsound
        0,               // speed
        20 * kFracUnit,  // radius
        16 * kFracUnit,  // height
        100,             // mass
        0,               // damage
        ksfx_None,       // activesound
        kMF_SPECIAL,     // flags
        0,               // MBF21 flags
        -2,              // Infighting group
        -2,              // Projectile group
        -2,              // Splash group
        ksfx_None,       // Rip sound
        0,               // Fast speed
        0,               // Melee range
        0,               // Gib health
        -1,              // Dropped item
        0,               // Pickup width
        0,               // Projectile pass height
        0,               // Fullbright
        kS_NULL          // raisestate
    },

    // MT_MISC19
    {
        "BOX_OF_ROCKETS",  // name
        2046,              // doomednum
        kS_BROK,           // spawnstate
        1000,              // spawnhealth
        kS_NULL,           // seestate
        ksfx_None,         // seesound
        8,                 // reactiontime
        ksfx_None,         // attacksound
        kS_NULL,           // painstate
        0,                 // painchance
        ksfx_None,         // painsound
        kS_NULL,           // meleestate
        kS_NULL,           // missilestate
        kS_NULL,           // deathstate
        kS_NULL,           // xdeathstate
        ksfx_None,         // deathsound
        0,                 // speed
        20 * kFracUnit,    // radius
        16 * kFracUnit,    // height
        100,               // mass
        0,                 // damage
        ksfx_None,         // activesound
        kMF_SPECIAL,       // flags
        0,                 // MBF21 flags
        -2,                // Infighting group
        -2,                // Projectile group
        -2,                // Splash group
        ksfx_None,         // Rip sound
        0,                 // Fast speed
        0,                 // Melee range
        0,                 // Gib health
        -1,                // Dropped item
        0,                 // Pickup width
        0,                 // Projectile pass height
        0,                 // Fullbright
        kS_NULL            // raisestate
    },

    // MT_MISC20
    {
        "CELLS",         // name
        2047,            // doomednum
        kS_CELL,         // spawnstate
        1000,            // spawnhealth
        kS_NULL,         // seestate
        ksfx_None,       // seesound
        8,               // reactiontime
        ksfx_None,       // attacksound
        kS_NULL,         // painstate
        0,               // painchance
        ksfx_None,       // painsound
        kS_NULL,         // meleestate
        kS_NULL,         // missilestate
        kS_NULL,         // deathstate
        kS_NULL,         // xdeathstate
        ksfx_None,       // deathsound
        0,               // speed
        20 * kFracUnit,  // radius
        16 * kFracUnit,  // height
        100,             // mass
        0,               // damage
        ksfx_None,       // activesound
        kMF_SPECIAL,     // flags
        0,               // MBF21 flags
        -2,              // Infighting group
        -2,              // Projectile group
        -2,              // Splash group
        ksfx_None,       // Rip sound
        0,               // Fast speed
        0,               // Melee range
        0,               // Gib health
        -1,              // Dropped item
        0,               // Pickup width
        0,               // Projectile pass height
        0,               // Fullbright
        kS_NULL          // raisestate
    },

    // MT_MISC21
    {
        "CELL_PACK",     // name
        17,              // doomednum
        kS_CELP,         // spawnstate
        1000,            // spawnhealth
        kS_NULL,         // seestate
        ksfx_None,       // seesound
        8,               // reactiontime
        ksfx_None,       // attacksound
        kS_NULL,         // painstate
        0,               // painchance
        ksfx_None,       // painsound
        kS_NULL,         // meleestate
        kS_NULL,         // missilestate
        kS_NULL,         // deathstate
        kS_NULL,         // xdeathstate
        ksfx_None,       // deathsound
        0,               // speed
        20 * kFracUnit,  // radius
        16 * kFracUnit,  // height
        100,             // mass
        0,               // damage
        ksfx_None,       // activesound
        kMF_SPECIAL,     // flags
        0,               // MBF21 flags
        -2,              // Infighting group
        -2,              // Projectile group
        -2,              // Splash group
        ksfx_None,       // Rip sound
        0,               // Fast speed
        0,               // Melee range
        0,               // Gib health
        -1,              // Dropped item
        0,               // Pickup width
        0,               // Projectile pass height
        0,               // Fullbright
        kS_NULL          // raisestate
    },

    // MT_MISC22
    {
        "SHELLS",        // name
        2008,            // doomednum
        kS_SHEL,         // spawnstate
        1000,            // spawnhealth
        kS_NULL,         // seestate
        ksfx_None,       // seesound
        8,               // reactiontime
        ksfx_None,       // attacksound
        kS_NULL,         // painstate
        0,               // painchance
        ksfx_None,       // painsound
        kS_NULL,         // meleestate
        kS_NULL,         // missilestate
        kS_NULL,         // deathstate
        kS_NULL,         // xdeathstate
        ksfx_None,       // deathsound
        0,               // speed
        20 * kFracUnit,  // radius
        16 * kFracUnit,  // height
        100,             // mass
        0,               // damage
        ksfx_None,       // activesound
        kMF_SPECIAL,     // flags
        0,               // MBF21 flags
        -2,              // Infighting group
        -2,              // Projectile group
        -2,              // Splash group
        ksfx_None,       // Rip sound
        0,               // Fast speed
        0,               // Melee range
        0,               // Gib health
        -1,              // Dropped item
        0,               // Pickup width
        0,               // Projectile pass height
        0,               // Fullbright
        kS_NULL          // raisestate
    },

    // MT_MISC23
    {
        "BOX_OF_SHELLS",  // name
        2049,             // doomednum
        kS_SBOX,          // spawnstate
        1000,             // spawnhealth
        kS_NULL,          // seestate
        ksfx_None,        // seesound
        8,                // reactiontime
        ksfx_None,        // attacksound
        kS_NULL,          // painstate
        0,                // painchance
        ksfx_None,        // painsound
        kS_NULL,          // meleestate
        kS_NULL,          // missilestate
        kS_NULL,          // deathstate
        kS_NULL,          // xdeathstate
        ksfx_None,        // deathsound
        0,                // speed
        20 * kFracUnit,   // radius
        16 * kFracUnit,   // height
        100,              // mass
        0,                // damage
        ksfx_None,        // activesound
        kMF_SPECIAL,      // flags
        0,                // MBF21 flags
        -2,               // Infighting group
        -2,               // Projectile group
        -2,               // Splash group
        ksfx_None,        // Rip sound
        0,                // Fast speed
        0,                // Melee range
        0,                // Gib health
        -1,               // Dropped item
        0,                // Pickup width
        0,                // Projectile pass height
        0,                // Fullbright
        kS_NULL           // raisestate
    },

    // MT_MISC24
    {
        "BACKPACK",      // name
        8,               // doomednum
        kS_BPAK,         // spawnstate
        1000,            // spawnhealth
        kS_NULL,         // seestate
        ksfx_None,       // seesound
        8,               // reactiontime
        ksfx_None,       // attacksound
        kS_NULL,         // painstate
        0,               // painchance
        ksfx_None,       // painsound
        kS_NULL,         // meleestate
        kS_NULL,         // missilestate
        kS_NULL,         // deathstate
        kS_NULL,         // xdeathstate
        ksfx_None,       // deathsound
        0,               // speed
        20 * kFracUnit,  // radius
        16 * kFracUnit,  // height
        100,             // mass
        0,               // damage
        ksfx_None,       // activesound
        kMF_SPECIAL,     // flags
        0,               // MBF21 flags
        -2,              // Infighting group
        -2,              // Projectile group
        -2,              // Splash group
        ksfx_None,       // Rip sound
        0,               // Fast speed
        0,               // Melee range
        0,               // Gib health
        -1,              // Dropped item
        0,               // Pickup width
        0,               // Projectile pass height
        0,               // Fullbright
        kS_NULL          // raisestate
    },

    // MT_MISC25
    {
        "BFG",           // name
        2006,            // doomednum
        kS_BFUG,         // spawnstate
        1000,            // spawnhealth
        kS_NULL,         // seestate
        ksfx_None,       // seesound
        8,               // reactiontime
        ksfx_None,       // attacksound
        kS_NULL,         // painstate
        0,               // painchance
        ksfx_None,       // painsound
        kS_NULL,         // meleestate
        kS_NULL,         // missilestate
        kS_NULL,         // deathstate
        kS_NULL,         // xdeathstate
        ksfx_None,       // deathsound
        0,               // speed
        20 * kFracUnit,  // radius
        16 * kFracUnit,  // height
        100,             // mass
        0,               // damage
        ksfx_None,       // activesound
        kMF_SPECIAL,     // flags
        0,               // MBF21 flags
        -2,              // Infighting group
        -2,              // Projectile group
        -2,              // Splash group
        ksfx_None,       // Rip sound
        0,               // Fast speed
        0,               // Melee range
        0,               // Gib health
        -1,              // Dropped item
        0,               // Pickup width
        0,               // Projectile pass height
        0,               // Fullbright
        kS_NULL          // raisestate
    },

    // MT_CHAINGUN
    {
        "CHAINGUN",      // name
        2002,            // doomednum
        kS_MGUN,         // spawnstate
        1000,            // spawnhealth
        kS_NULL,         // seestate
        ksfx_None,       // seesound
        8,               // reactiontime
        ksfx_None,       // attacksound
        kS_NULL,         // painstate
        0,               // painchance
        ksfx_None,       // painsound
        kS_NULL,         // meleestate
        kS_NULL,         // missilestate
        kS_NULL,         // deathstate
        kS_NULL,         // xdeathstate
        ksfx_None,       // deathsound
        0,               // speed
        20 * kFracUnit,  // radius
        16 * kFracUnit,  // height
        100,             // mass
        0,               // damage
        ksfx_None,       // activesound
        kMF_SPECIAL,     // flags
        0,               // MBF21 flags
        -2,              // Infighting group
        -2,              // Projectile group
        -2,              // Splash group
        ksfx_None,       // Rip sound
        0,               // Fast speed
        0,               // Melee range
        0,               // Gib health
        -1,              // Dropped item
        0,               // Pickup width
        0,               // Projectile pass height
        0,               // Fullbright
        kS_NULL          // raisestate
    },

    // MT_MISC26
    {
        "CHAINSAW",      // name
        2005,            // doomednum
        kS_CSAW,         // spawnstate
        1000,            // spawnhealth
        kS_NULL,         // seestate
        ksfx_None,       // seesound
        8,               // reactiontime
        ksfx_None,       // attacksound
        kS_NULL,         // painstate
        0,               // painchance
        ksfx_None,       // painsound
        kS_NULL,         // meleestate
        kS_NULL,         // missilestate
        kS_NULL,         // deathstate
        kS_NULL,         // xdeathstate
        ksfx_None,       // deathsound
        0,               // speed
        20 * kFracUnit,  // radius
        16 * kFracUnit,  // height
        100,             // mass
        0,               // damage
        ksfx_None,       // activesound
        kMF_SPECIAL,     // flags
        0,               // MBF21 flags
        -2,              // Infighting group
        -2,              // Projectile group
        -2,              // Splash group
        ksfx_None,       // Rip sound
        0,               // Fast speed
        0,               // Melee range
        0,               // Gib health
        -1,              // Dropped item
        0,               // Pickup width
        0,               // Projectile pass height
        0,               // Fullbright
        kS_NULL          // raisestate
    },

    // MT_MISC27
    {
        "MISSILE_LAUNCHER",  // name
        2003,                // doomednum
        kS_LAUN,             // spawnstate
        1000,                // spawnhealth
        kS_NULL,             // seestate
        ksfx_None,           // seesound
        8,                   // reactiontime
        ksfx_None,           // attacksound
        kS_NULL,             // painstate
        0,                   // painchance
        ksfx_None,           // painsound
        kS_NULL,             // meleestate
        kS_NULL,             // missilestate
        kS_NULL,             // deathstate
        kS_NULL,             // xdeathstate
        ksfx_None,           // deathsound
        0,                   // speed
        20 * kFracUnit,      // radius
        16 * kFracUnit,      // height
        100,                 // mass
        0,                   // damage
        ksfx_None,           // activesound
        kMF_SPECIAL,         // flags
        0,                   // MBF21 flags
        -2,                  // Infighting group
        -2,                  // Projectile group
        -2,                  // Splash group
        ksfx_None,           // Rip sound
        0,                   // Fast speed
        0,                   // Melee range
        0,                   // Gib health
        -1,                  // Dropped item
        0,                   // Pickup width
        0,                   // Projectile pass height
        0,                   // Fullbright
        kS_NULL              // raisestate
    },

    // MT_MISC28
    {
        "PLASMA_RIFLE",  // name
        2004,            // doomednum
        kS_PLAS,         // spawnstate
        1000,            // spawnhealth
        kS_NULL,         // seestate
        ksfx_None,       // seesound
        8,               // reactiontime
        ksfx_None,       // attacksound
        kS_NULL,         // painstate
        0,               // painchance
        ksfx_None,       // painsound
        kS_NULL,         // meleestate
        kS_NULL,         // missilestate
        kS_NULL,         // deathstate
        kS_NULL,         // xdeathstate
        ksfx_None,       // deathsound
        0,               // speed
        20 * kFracUnit,  // radius
        16 * kFracUnit,  // height
        100,             // mass
        0,               // damage
        ksfx_None,       // activesound
        kMF_SPECIAL,     // flags
        0,               // MBF21 flags
        -2,              // Infighting group
        -2,              // Projectile group
        -2,              // Splash group
        ksfx_None,       // Rip sound
        0,               // Fast speed
        0,               // Melee range
        0,               // Gib health
        -1,              // Dropped item
        0,               // Pickup width
        0,               // Projectile pass height
        0,               // Fullbright
        kS_NULL          // raisestate
    },

    // MT_SHOTGUN
    {
        "SHOTGUN",       // name
        2001,            // doomednum
        kS_SHOT,         // spawnstate
        1000,            // spawnhealth
        kS_NULL,         // seestate
        ksfx_None,       // seesound
        8,               // reactiontime
        ksfx_None,       // attacksound
        kS_NULL,         // painstate
        0,               // painchance
        ksfx_None,       // painsound
        kS_NULL,         // meleestate
        kS_NULL,         // missilestate
        kS_NULL,         // deathstate
        kS_NULL,         // xdeathstate
        ksfx_None,       // deathsound
        0,               // speed
        20 * kFracUnit,  // radius
        16 * kFracUnit,  // height
        100,             // mass
        0,               // damage
        ksfx_None,       // activesound
        kMF_SPECIAL,     // flags
        0,               // MBF21 flags
        -2,              // Infighting group
        -2,              // Projectile group
        -2,              // Splash group
        ksfx_None,       // Rip sound
        0,               // Fast speed
        0,               // Melee range
        0,               // Gib health
        -1,              // Dropped item
        0,               // Pickup width
        0,               // Projectile pass height
        0,               // Fullbright
        kS_NULL          // raisestate
    },

    // MT_SUPERSHOTGUN
    {
        "SUPER_SHOTGUN",  // name
        82,               // doomednum
        kS_SHOT2,         // spawnstate
        1000,             // spawnhealth
        kS_NULL,          // seestate
        ksfx_None,        // seesound
        8,                // reactiontime
        ksfx_None,        // attacksound
        kS_NULL,          // painstate
        0,                // painchance
        ksfx_None,        // painsound
        kS_NULL,          // meleestate
        kS_NULL,          // missilestate
        kS_NULL,          // deathstate
        kS_NULL,          // xdeathstate
        ksfx_None,        // deathsound
        0,                // speed
        20 * kFracUnit,   // radius
        16 * kFracUnit,   // height
        100,              // mass
        0,                // damage
        ksfx_None,        // activesound
        kMF_SPECIAL,      // flags
        0,                // MBF21 flags
        -2,               // Infighting group
        -2,               // Projectile group
        -2,               // Splash group
        ksfx_None,        // Rip sound
        0,                // Fast speed
        0,                // Melee range
        0,                // Gib health
        -1,               // Dropped item
        0,                // Pickup width
        0,                // Projectile pass height
        0,                // Fullbright
        kS_NULL           // raisestate
    },

    // MT_MISC29
    {
        "TALL_TECH_LAMP",  // name
        85,                // doomednum
        kS_TECHLAMP,       // spawnstate
        1000,              // spawnhealth
        kS_NULL,           // seestate
        ksfx_None,         // seesound
        8,                 // reactiontime
        ksfx_None,         // attacksound
        kS_NULL,           // painstate
        0,                 // painchance
        ksfx_None,         // painsound
        kS_NULL,           // meleestate
        kS_NULL,           // missilestate
        kS_NULL,           // deathstate
        kS_NULL,           // xdeathstate
        ksfx_None,         // deathsound
        0,                 // speed
        16 * kFracUnit,    // radius
        16 * kFracUnit,    // height
        100,               // mass
        0,                 // damage
        ksfx_None,         // activesound
        kMF_SOLID,         // flags
        0,                 // MBF21 flags
        -2,                // Infighting group
        -2,                // Projectile group
        -2,                // Splash group
        ksfx_None,         // Rip sound
        0,                 // Fast speed
        0,                 // Melee range
        0,                 // Gib health
        -1,                // Dropped item
        0,                 // Pickup width
        0,                 // Projectile pass height
        0,                 // Fullbright
        kS_NULL            // raisestate
    },

    // MT_MISC30
    {
        "SMALL_TECH_LAMP",  // name
        86,                 // doomednum
        kS_TECH2LAMP,       // spawnstate
        1000,               // spawnhealth
        kS_NULL,            // seestate
        ksfx_None,          // seesound
        8,                  // reactiontime
        ksfx_None,          // attacksound
        kS_NULL,            // painstate
        0,                  // painchance
        ksfx_None,          // painsound
        kS_NULL,            // meleestate
        kS_NULL,            // missilestate
        kS_NULL,            // deathstate
        kS_NULL,            // xdeathstate
        ksfx_None,          // deathsound
        0,                  // speed
        16 * kFracUnit,     // radius
        16 * kFracUnit,     // height
        100,                // mass
        0,                  // damage
        ksfx_None,          // activesound
        kMF_SOLID,          // flags
        0,                  // MBF21 flags
        -2,                 // Infighting group
        -2,                 // Projectile group
        -2,                 // Splash group
        ksfx_None,          // Rip sound
        0,                  // Fast speed
        0,                  // Melee range
        0,                  // Gib health
        -1,                 // Dropped item
        0,                  // Pickup width
        0,                  // Projectile pass height
        0,                  // Fullbright
        kS_NULL             // raisestate
    },

    // MT_MISC31
    {
        "SMALL_BOLLARD_LAMP",  // name
        2028,                  // doomednum
        kS_COLU,               // spawnstate
        1000,                  // spawnhealth
        kS_NULL,               // seestate
        ksfx_None,             // seesound
        8,                     // reactiontime
        ksfx_None,             // attacksound
        kS_NULL,               // painstate
        0,                     // painchance
        ksfx_None,             // painsound
        kS_NULL,               // meleestate
        kS_NULL,               // missilestate
        kS_NULL,               // deathstate
        kS_NULL,               // xdeathstate
        ksfx_None,             // deathsound
        0,                     // speed
        16 * kFracUnit,        // radius
        16 * kFracUnit,        // height
        100,                   // mass
        0,                     // damage
        ksfx_None,             // activesound
        kMF_SOLID,             // flags
        0,                     // MBF21 flags
        -2,                    // Infighting group
        -2,                    // Projectile group
        -2,                    // Splash group
        ksfx_None,             // Rip sound
        0,                     // Fast speed
        0,                     // Melee range
        0,                     // Gib health
        -1,                    // Dropped item
        0,                     // Pickup width
        0,                     // Projectile pass height
        0,                     // Fullbright
        kS_NULL                // raisestate
    },

    // MT_MISC32
    {
        "TALL_GREEN_COLUMN",  // name
        30,                   // doomednum
        kS_TALLGRNCOL,        // spawnstate
        1000,                 // spawnhealth
        kS_NULL,              // seestate
        ksfx_None,            // seesound
        8,                    // reactiontime
        ksfx_None,            // attacksound
        kS_NULL,              // painstate
        0,                    // painchance
        ksfx_None,            // painsound
        kS_NULL,              // meleestate
        kS_NULL,              // missilestate
        kS_NULL,              // deathstate
        kS_NULL,              // xdeathstate
        ksfx_None,            // deathsound
        0,                    // speed
        16 * kFracUnit,       // radius
        16 * kFracUnit,       // height
        100,                  // mass
        0,                    // damage
        ksfx_None,            // activesound
        kMF_SOLID,            // flags
        0,                    // MBF21 flags
        -2,                   // Infighting group
        -2,                   // Projectile group
        -2,                   // Splash group
        ksfx_None,            // Rip sound
        0,                    // Fast speed
        0,                    // Melee range
        0,                    // Gib health
        -1,                   // Dropped item
        0,                    // Pickup width
        0,                    // Projectile pass height
        0,                    // Fullbright
        kS_NULL               // raisestate
    },

    // MT_MISC33
    {
        "SHORT_GREEN_COLUMN",  // name
        31,                    // doomednum
        kS_SHRTGRNCOL,         // spawnstate
        1000,                  // spawnhealth
        kS_NULL,               // seestate
        ksfx_None,             // seesound
        8,                     // reactiontime
        ksfx_None,             // attacksound
        kS_NULL,               // painstate
        0,                     // painchance
        ksfx_None,             // painsound
        kS_NULL,               // meleestate
        kS_NULL,               // missilestate
        kS_NULL,               // deathstate
        kS_NULL,               // xdeathstate
        ksfx_None,             // deathsound
        0,                     // speed
        16 * kFracUnit,        // radius
        16 * kFracUnit,        // height
        100,                   // mass
        0,                     // damage
        ksfx_None,             // activesound
        kMF_SOLID,             // flags
        0,                     // MBF21 flags
        -2,                    // Infighting group
        -2,                    // Projectile group
        -2,                    // Splash group
        ksfx_None,             // Rip sound
        0,                     // Fast speed
        0,                     // Melee range
        0,                     // Gib health
        -1,                    // Dropped item
        0,                     // Pickup width
        0,                     // Projectile pass height
        0,                     // Fullbright
        kS_NULL                // raisestate
    },

    // MT_MISC34
    {
        "TALL_RED_COLUMN",  // name
        32,                 // doomednum
        kS_TALLREDCOL,      // spawnstate
        1000,               // spawnhealth
        kS_NULL,            // seestate
        ksfx_None,          // seesound
        8,                  // reactiontime
        ksfx_None,          // attacksound
        kS_NULL,            // painstate
        0,                  // painchance
        ksfx_None,          // painsound
        kS_NULL,            // meleestate
        kS_NULL,            // missilestate
        kS_NULL,            // deathstate
        kS_NULL,            // xdeathstate
        ksfx_None,          // deathsound
        0,                  // speed
        16 * kFracUnit,     // radius
        16 * kFracUnit,     // height
        100,                // mass
        0,                  // damage
        ksfx_None,          // activesound
        kMF_SOLID,          // flags
        0,                  // MBF21 flags
        -2,                 // Infighting group
        -2,                 // Projectile group
        -2,                 // Splash group
        ksfx_None,          // Rip sound
        0,                  // Fast speed
        0,                  // Melee range
        0,                  // Gib health
        -1,                 // Dropped item
        0,                  // Pickup width
        0,                  // Projectile pass height
        0,                  // Fullbright
        kS_NULL             // raisestate
    },

    // MT_MISC35
    {
        "SHORT_RED_COLUMN",  // name
        33,                  // doomednum
        kS_SHRTREDCOL,       // spawnstate
        1000,                // spawnhealth
        kS_NULL,             // seestate
        ksfx_None,           // seesound
        8,                   // reactiontime
        ksfx_None,           // attacksound
        kS_NULL,             // painstate
        0,                   // painchance
        ksfx_None,           // painsound
        kS_NULL,             // meleestate
        kS_NULL,             // missilestate
        kS_NULL,             // deathstate
        kS_NULL,             // xdeathstate
        ksfx_None,           // deathsound
        0,                   // speed
        16 * kFracUnit,      // radius
        16 * kFracUnit,      // height
        100,                 // mass
        0,                   // damage
        ksfx_None,           // activesound
        kMF_SOLID,           // flags
        0,                   // MBF21 flags
        -2,                  // Infighting group
        -2,                  // Projectile group
        -2,                  // Splash group
        ksfx_None,           // Rip sound
        0,                   // Fast speed
        0,                   // Melee range
        0,                   // Gib health
        -1,                  // Dropped item
        0,                   // Pickup width
        0,                   // Projectile pass height
        0,                   // Fullbright
        kS_NULL              // raisestate
    },

    // MT_MISC36
    {
        "SKULL_ON_COLUMN",  // name
        37,                 // doomednum
        kS_SKULLCOL,        // spawnstate
        1000,               // spawnhealth
        kS_NULL,            // seestate
        ksfx_None,          // seesound
        8,                  // reactiontime
        ksfx_None,          // attacksound
        kS_NULL,            // painstate
        0,                  // painchance
        ksfx_None,          // painsound
        kS_NULL,            // meleestate
        kS_NULL,            // missilestate
        kS_NULL,            // deathstate
        kS_NULL,            // xdeathstate
        ksfx_None,          // deathsound
        0,                  // speed
        16 * kFracUnit,     // radius
        16 * kFracUnit,     // height
        100,                // mass
        0,                  // damage
        ksfx_None,          // activesound
        kMF_SOLID,          // flags
        0,                  // MBF21 flags
        -2,                 // Infighting group
        -2,                 // Projectile group
        -2,                 // Splash group
        ksfx_None,          // Rip sound
        0,                  // Fast speed
        0,                  // Melee range
        0,                  // Gib health
        -1,                 // Dropped item
        0,                  // Pickup width
        0,                  // Projectile pass height
        0,                  // Fullbright
        kS_NULL             // raisestate
    },

    // MT_MISC37
    {
        "BEATING_HEART_COLUMN",  // name
        36,                      // doomednum
        kS_HEARTCOL,             // spawnstate
        1000,                    // spawnhealth
        kS_NULL,                 // seestate
        ksfx_None,               // seesound
        8,                       // reactiontime
        ksfx_None,               // attacksound
        kS_NULL,                 // painstate
        0,                       // painchance
        ksfx_None,               // painsound
        kS_NULL,                 // meleestate
        kS_NULL,                 // missilestate
        kS_NULL,                 // deathstate
        kS_NULL,                 // xdeathstate
        ksfx_None,               // deathsound
        0,                       // speed
        16 * kFracUnit,          // radius
        16 * kFracUnit,          // height
        100,                     // mass
        0,                       // damage
        ksfx_None,               // activesound
        kMF_SOLID,               // flags
        0,                       // MBF21 flags
        -2,                      // Infighting group
        -2,                      // Projectile group
        -2,                      // Splash group
        ksfx_None,               // Rip sound
        0,                       // Fast speed
        0,                       // Melee range
        0,                       // Gib health
        -1,                      // Dropped item
        0,                       // Pickup width
        0,                       // Projectile pass height
        0,                       // Fullbright
        kS_NULL                  // raisestate
    },

    // MT_MISC38
    {
        "EYE_SYMBOL",    // name
        41,              // doomednum
        kS_EVILEYE,      // spawnstate
        1000,            // spawnhealth
        kS_NULL,         // seestate
        ksfx_None,       // seesound
        8,               // reactiontime
        ksfx_None,       // attacksound
        kS_NULL,         // painstate
        0,               // painchance
        ksfx_None,       // painsound
        kS_NULL,         // meleestate
        kS_NULL,         // missilestate
        kS_NULL,         // deathstate
        kS_NULL,         // xdeathstate
        ksfx_None,       // deathsound
        0,               // speed
        16 * kFracUnit,  // radius
        16 * kFracUnit,  // height
        100,             // mass
        0,               // damage
        ksfx_None,       // activesound
        kMF_SOLID,       // flags
        0,               // MBF21 flags
        -2,              // Infighting group
        -2,              // Projectile group
        -2,              // Splash group
        ksfx_None,       // Rip sound
        0,               // Fast speed
        0,               // Melee range
        0,               // Gib health
        -1,              // Dropped item
        0,               // Pickup width
        0,               // Projectile pass height
        0,               // Fullbright
        kS_NULL          // raisestate
    },

    // MT_MISC39
    {
        "FLOATING_SKULLROCK",  // name
        42,                    // doomednum
        kS_FLOATSKULL,         // spawnstate
        1000,                  // spawnhealth
        kS_NULL,               // seestate
        ksfx_None,             // seesound
        8,                     // reactiontime
        ksfx_None,             // attacksound
        kS_NULL,               // painstate
        0,                     // painchance
        ksfx_None,             // painsound
        kS_NULL,               // meleestate
        kS_NULL,               // missilestate
        kS_NULL,               // deathstate
        kS_NULL,               // xdeathstate
        ksfx_None,             // deathsound
        0,                     // speed
        16 * kFracUnit,        // radius
        16 * kFracUnit,        // height
        100,                   // mass
        0,                     // damage
        ksfx_None,             // activesound
        kMF_SOLID,             // flags
        0,                     // MBF21 flags
        -2,                    // Infighting group
        -2,                    // Projectile group
        -2,                    // Splash group
        ksfx_None,             // Rip sound
        0,                     // Fast speed
        0,                     // Melee range
        0,                     // Gib health
        -1,                    // Dropped item
        0,                     // Pickup width
        0,                     // Projectile pass height
        0,                     // Fullbright
        kS_NULL                // raisestate
    },

    // MT_MISC40
    {
        "TORCHED_TREE",  // name
        43,              // doomednum
        kS_TORCHTREE,    // spawnstate
        1000,            // spawnhealth
        kS_NULL,         // seestate
        ksfx_None,       // seesound
        8,               // reactiontime
        ksfx_None,       // attacksound
        kS_NULL,         // painstate
        0,               // painchance
        ksfx_None,       // painsound
        kS_NULL,         // meleestate
        kS_NULL,         // missilestate
        kS_NULL,         // deathstate
        kS_NULL,         // xdeathstate
        ksfx_None,       // deathsound
        0,               // speed
        16 * kFracUnit,  // radius
        16 * kFracUnit,  // height
        100,             // mass
        0,               // damage
        ksfx_None,       // activesound
        kMF_SOLID,       // flags
        0,               // MBF21 flags
        -2,              // Infighting group
        -2,              // Projectile group
        -2,              // Splash group
        ksfx_None,       // Rip sound
        0,               // Fast speed
        0,               // Melee range
        0,               // Gib health
        -1,              // Dropped item
        0,               // Pickup width
        0,               // Projectile pass height
        0,               // Fullbright
        kS_NULL          // raisestate
    },

    // MT_MISC41
    {
        "BRONZE_BLUE_TORCH",  // name
        44,                   // doomednum
        kS_BLUETORCH,         // spawnstate
        1000,                 // spawnhealth
        kS_NULL,              // seestate
        ksfx_None,            // seesound
        8,                    // reactiontime
        ksfx_None,            // attacksound
        kS_NULL,              // painstate
        0,                    // painchance
        ksfx_None,            // painsound
        kS_NULL,              // meleestate
        kS_NULL,              // missilestate
        kS_NULL,              // deathstate
        kS_NULL,              // xdeathstate
        ksfx_None,            // deathsound
        0,                    // speed
        16 * kFracUnit,       // radius
        16 * kFracUnit,       // height
        100,                  // mass
        0,                    // damage
        ksfx_None,            // activesound
        kMF_SOLID,            // flags
        0,                    // MBF21 flags
        -2,                   // Infighting group
        -2,                   // Projectile group
        -2,                   // Splash group
        ksfx_None,            // Rip sound
        0,                    // Fast speed
        0,                    // Melee range
        0,                    // Gib health
        -1,                   // Dropped item
        0,                    // Pickup width
        0,                    // Projectile pass height
        0,                    // Fullbright
        kS_NULL               // raisestate
    },

    // MT_MISC42
    {
        "BRONZE_GREEN_TORCH",  // name
        45,                    // doomednum
        kS_GREENTORCH,         // spawnstate
        1000,                  // spawnhealth
        kS_NULL,               // seestate
        ksfx_None,             // seesound
        8,                     // reactiontime
        ksfx_None,             // attacksound
        kS_NULL,               // painstate
        0,                     // painchance
        ksfx_None,             // painsound
        kS_NULL,               // meleestate
        kS_NULL,               // missilestate
        kS_NULL,               // deathstate
        kS_NULL,               // xdeathstate
        ksfx_None,             // deathsound
        0,                     // speed
        16 * kFracUnit,        // radius
        16 * kFracUnit,        // height
        100,                   // mass
        0,                     // damage
        ksfx_None,             // activesound
        kMF_SOLID,             // flags
        0,                     // MBF21 flags
        -2,                    // Infighting group
        -2,                    // Projectile group
        -2,                    // Splash group
        ksfx_None,             // Rip sound
        0,                     // Fast speed
        0,                     // Melee range
        0,                     // Gib health
        -1,                    // Dropped item
        0,                     // Pickup width
        0,                     // Projectile pass height
        0,                     // Fullbright
        kS_NULL                // raisestate
    },

    // MT_MISC43
    {
        "BRONZE_RED_TORCH",  // name
        46,                  // doomednum
        kS_REDTORCH,         // spawnstate
        1000,                // spawnhealth
        kS_NULL,             // seestate
        ksfx_None,           // seesound
        8,                   // reactiontime
        ksfx_None,           // attacksound
        kS_NULL,             // painstate
        0,                   // painchance
        ksfx_None,           // painsound
        kS_NULL,             // meleestate
        kS_NULL,             // missilestate
        kS_NULL,             // deathstate
        kS_NULL,             // xdeathstate
        ksfx_None,           // deathsound
        0,                   // speed
        16 * kFracUnit,      // radius
        16 * kFracUnit,      // height
        100,                 // mass
        0,                   // damage
        ksfx_None,           // activesound
        kMF_SOLID,           // flags
        0,                   // MBF21 flags
        -2,                  // Infighting group
        -2,                  // Projectile group
        -2,                  // Splash group
        ksfx_None,           // Rip sound
        0,                   // Fast speed
        0,                   // Melee range
        0,                   // Gib health
        -1,                  // Dropped item
        0,                   // Pickup width
        0,                   // Projectile pass height
        0,                   // Fullbright
        kS_NULL              // raisestate
    },

    // MT_MISC44
    {
        "WOODEN_BLUE_TORCH",  // name
        55,                   // doomednum
        kS_BTORCHSHRT,        // spawnstate
        1000,                 // spawnhealth
        kS_NULL,              // seestate
        ksfx_None,            // seesound
        8,                    // reactiontime
        ksfx_None,            // attacksound
        kS_NULL,              // painstate
        0,                    // painchance
        ksfx_None,            // painsound
        kS_NULL,              // meleestate
        kS_NULL,              // missilestate
        kS_NULL,              // deathstate
        kS_NULL,              // xdeathstate
        ksfx_None,            // deathsound
        0,                    // speed
        16 * kFracUnit,       // radius
        16 * kFracUnit,       // height
        100,                  // mass
        0,                    // damage
        ksfx_None,            // activesound
        kMF_SOLID,            // flags
        0,                    // MBF21 flags
        -2,                   // Infighting group
        -2,                   // Projectile group
        -2,                   // Splash group
        ksfx_None,            // Rip sound
        0,                    // Fast speed
        0,                    // Melee range
        0,                    // Gib health
        -1,                   // Dropped item
        0,                    // Pickup width
        0,                    // Projectile pass height
        0,                    // Fullbright
        kS_NULL               // raisestate
    },

    // MT_MISC45
    {
        "WOODEN_GREEN_TORCH",  // name
        56,                    // doomednum
        kS_GTORCHSHRT,         // spawnstate
        1000,                  // spawnhealth
        kS_NULL,               // seestate
        ksfx_None,             // seesound
        8,                     // reactiontime
        ksfx_None,             // attacksound
        kS_NULL,               // painstate
        0,                     // painchance
        ksfx_None,             // painsound
        kS_NULL,               // meleestate
        kS_NULL,               // missilestate
        kS_NULL,               // deathstate
        kS_NULL,               // xdeathstate
        ksfx_None,             // deathsound
        0,                     // speed
        16 * kFracUnit,        // radius
        16 * kFracUnit,        // height
        100,                   // mass
        0,                     // damage
        ksfx_None,             // activesound
        kMF_SOLID,             // flags
        0,                     // MBF21 flags
        -2,                    // Infighting group
        -2,                    // Projectile group
        -2,                    // Splash group
        ksfx_None,             // Rip sound
        0,                     // Fast speed
        0,                     // Melee range
        0,                     // Gib health
        -1,                    // Dropped item
        0,                     // Pickup width
        0,                     // Projectile pass height
        0,                     // Fullbright
        kS_NULL                // raisestate
    },

    // MT_MISC46
    {
        "WOODEN_RED_TORCH",  // name
        57,                  // doomednum
        kS_RTORCHSHRT,       // spawnstate
        1000,                // spawnhealth
        kS_NULL,             // seestate
        ksfx_None,           // seesound
        8,                   // reactiontime
        ksfx_None,           // attacksound
        kS_NULL,             // painstate
        0,                   // painchance
        ksfx_None,           // painsound
        kS_NULL,             // meleestate
        kS_NULL,             // missilestate
        kS_NULL,             // deathstate
        kS_NULL,             // xdeathstate
        ksfx_None,           // deathsound
        0,                   // speed
        16 * kFracUnit,      // radius
        16 * kFracUnit,      // height
        100,                 // mass
        0,                   // damage
        ksfx_None,           // activesound
        kMF_SOLID,           // flags
        0,                   // MBF21 flags
        -2,                  // Infighting group
        -2,                  // Projectile group
        -2,                  // Splash group
        ksfx_None,           // Rip sound
        0,                   // Fast speed
        0,                   // Melee range
        0,                   // Gib health
        -1,                  // Dropped item
        0,                   // Pickup width
        0,                   // Projectile pass height
        0,                   // Fullbright
        kS_NULL              // raisestate
    },

    // MT_MISC47
    {
        "SPIKY_STUMP",   // name
        47,              // doomednum
        kS_STALAGTITE,   // spawnstate
        1000,            // spawnhealth
        kS_NULL,         // seestate
        ksfx_None,       // seesound
        8,               // reactiontime
        ksfx_None,       // attacksound
        kS_NULL,         // painstate
        0,               // painchance
        ksfx_None,       // painsound
        kS_NULL,         // meleestate
        kS_NULL,         // missilestate
        kS_NULL,         // deathstate
        kS_NULL,         // xdeathstate
        ksfx_None,       // deathsound
        0,               // speed
        16 * kFracUnit,  // radius
        16 * kFracUnit,  // height
        100,             // mass
        0,               // damage
        ksfx_None,       // activesound
        kMF_SOLID,       // flags
        0,               // MBF21 flags
        -2,              // Infighting group
        -2,              // Projectile group
        -2,              // Splash group
        ksfx_None,       // Rip sound
        0,               // Fast speed
        0,               // Melee range
        0,               // Gib health
        -1,              // Dropped item
        0,               // Pickup width
        0,               // Projectile pass height
        0,               // Fullbright
        kS_NULL          // raisestate
    },

    // MT_MISC48
    {
        "TECHNOCOLUMN",  // name
        48,              // doomednum
        kS_TECHPILLAR,   // spawnstate
        1000,            // spawnhealth
        kS_NULL,         // seestate
        ksfx_None,       // seesound
        8,               // reactiontime
        ksfx_None,       // attacksound
        kS_NULL,         // painstate
        0,               // painchance
        ksfx_None,       // painsound
        kS_NULL,         // meleestate
        kS_NULL,         // missilestate
        kS_NULL,         // deathstate
        kS_NULL,         // xdeathstate
        ksfx_None,       // deathsound
        0,               // speed
        16 * kFracUnit,  // radius
        16 * kFracUnit,  // height
        100,             // mass
        0,               // damage
        ksfx_None,       // activesound
        kMF_SOLID,       // flags
        0,               // MBF21 flags
        -2,              // Infighting group
        -2,              // Projectile group
        -2,              // Splash group
        ksfx_None,       // Rip sound
        0,               // Fast speed
        0,               // Melee range
        0,               // Gib health
        -1,              // Dropped item
        0,               // Pickup width
        0,               // Projectile pass height
        0,               // Fullbright
        kS_NULL          // raisestate
    },

    // MT_MISC49
    {
        "BLACK_CANDLE",  // name
        34,              // doomednum
        kS_CANDLESTIK,   // spawnstate
        1000,            // spawnhealth
        kS_NULL,         // seestate
        ksfx_None,       // seesound
        8,               // reactiontime
        ksfx_None,       // attacksound
        kS_NULL,         // painstate
        0,               // painchance
        ksfx_None,       // painsound
        kS_NULL,         // meleestate
        kS_NULL,         // missilestate
        kS_NULL,         // deathstate
        kS_NULL,         // xdeathstate
        ksfx_None,       // deathsound
        0,               // speed
        20 * kFracUnit,  // radius
        16 * kFracUnit,  // height
        100,             // mass
        0,               // damage
        ksfx_None,       // activesound
        0,               // flags
        0,               // MBF21 flags
        -2,              // Infighting group
        -2,              // Projectile group
        -2,              // Splash group
        ksfx_None,       // Rip sound
        0,               // Fast speed
        0,               // Melee range
        0,               // Gib health
        -1,              // Dropped item
        0,               // Pickup width
        0,               // Projectile pass height
        0,               // Fullbright
        kS_NULL          // raisestate
    },

    // MT_MISC50
    {
        "CANDELABRA",    // name
        35,              // doomednum
        kS_CANDELABRA,   // spawnstate
        1000,            // spawnhealth
        kS_NULL,         // seestate
        ksfx_None,       // seesound
        8,               // reactiontime
        ksfx_None,       // attacksound
        kS_NULL,         // painstate
        0,               // painchance
        ksfx_None,       // painsound
        kS_NULL,         // meleestate
        kS_NULL,         // missilestate
        kS_NULL,         // deathstate
        kS_NULL,         // xdeathstate
        ksfx_None,       // deathsound
        0,               // speed
        16 * kFracUnit,  // radius
        16 * kFracUnit,  // height
        100,             // mass
        0,               // damage
        ksfx_None,       // activesound
        kMF_SOLID,       // flags
        0,               // MBF21 flags
        -2,              // Infighting group
        -2,              // Projectile group
        -2,              // Splash group
        ksfx_None,       // Rip sound
        0,               // Fast speed
        0,               // Melee range
        0,               // Gib health
        -1,              // Dropped item
        0,               // Pickup width
        0,               // Projectile pass height
        0,               // Fullbright
        kS_NULL          // raisestate
    },

    // MT_MISC51
    {
        "TWITCHING_BLOKE_I",                           // name
        49,                                            // doomednum
        kS_BLOODYTWITCH,                               // spawnstate
        1000,                                          // spawnhealth
        kS_NULL,                                       // seestate
        ksfx_None,                                     // seesound
        8,                                             // reactiontime
        ksfx_None,                                     // attacksound
        kS_NULL,                                       // painstate
        0,                                             // painchance
        ksfx_None,                                     // painsound
        kS_NULL,                                       // meleestate
        kS_NULL,                                       // missilestate
        kS_NULL,                                       // deathstate
        kS_NULL,                                       // xdeathstate
        ksfx_None,                                     // deathsound
        0,                                             // speed
        16 * kFracUnit,                                // radius
        68 * kFracUnit,                                // height
        100,                                           // mass
        0,                                             // damage
        ksfx_None,                                     // activesound
        kMF_SOLID | kMF_SPAWNCEILING | kMF_NOGRAVITY,  // flags
        0,                                             // MBF21 flags
        -2,                                            // Infighting group
        -2,                                            // Projectile group
        -2,                                            // Splash group
        ksfx_None,                                     // Rip sound
        0,                                             // Fast speed
        0,                                             // Melee range
        0,                                             // Gib health
        -1,                                            // Dropped item
        0,                                             // Pickup width
        0,                                             // Projectile pass height
        0,                                             // Fullbright
        kS_NULL                                        // raisestate
    },

    // MT_MISC52
    {
        "HANGING_DEAD_BLOKE_I",                        // name
        50,                                            // doomednum
        kS_MEAT2,                                      // spawnstate
        1000,                                          // spawnhealth
        kS_NULL,                                       // seestate
        ksfx_None,                                     // seesound
        8,                                             // reactiontime
        ksfx_None,                                     // attacksound
        kS_NULL,                                       // painstate
        0,                                             // painchance
        ksfx_None,                                     // painsound
        kS_NULL,                                       // meleestate
        kS_NULL,                                       // missilestate
        kS_NULL,                                       // deathstate
        kS_NULL,                                       // xdeathstate
        ksfx_None,                                     // deathsound
        0,                                             // speed
        16 * kFracUnit,                                // radius
        84 * kFracUnit,                                // height
        100,                                           // mass
        0,                                             // damage
        ksfx_None,                                     // activesound
        kMF_SOLID | kMF_SPAWNCEILING | kMF_NOGRAVITY,  // flags
        0,                                             // MBF21 flags
        -2,                                            // Infighting group
        -2,                                            // Projectile group
        -2,                                            // Splash group
        ksfx_None,                                     // Rip sound
        0,                                             // Fast speed
        0,                                             // Melee range
        0,                                             // Gib health
        -1,                                            // Dropped item
        0,                                             // Pickup width
        0,                                             // Projectile pass height
        0,                                             // Fullbright
        kS_NULL                                        // raisestate
    },

    // MT_MISC53
    {
        "HANGING_DEAD_BLOKE_II",                       // name
        51,                                            // doomednum
        kS_MEAT3,                                      // spawnstate
        1000,                                          // spawnhealth
        kS_NULL,                                       // seestate
        ksfx_None,                                     // seesound
        8,                                             // reactiontime
        ksfx_None,                                     // attacksound
        kS_NULL,                                       // painstate
        0,                                             // painchance
        ksfx_None,                                     // painsound
        kS_NULL,                                       // meleestate
        kS_NULL,                                       // missilestate
        kS_NULL,                                       // deathstate
        kS_NULL,                                       // xdeathstate
        ksfx_None,                                     // deathsound
        0,                                             // speed
        16 * kFracUnit,                                // radius
        84 * kFracUnit,                                // height
        100,                                           // mass
        0,                                             // damage
        ksfx_None,                                     // activesound
        kMF_SOLID | kMF_SPAWNCEILING | kMF_NOGRAVITY,  // flags
        0,                                             // MBF21 flags
        -2,                                            // Infighting group
        -2,                                            // Projectile group
        -2,                                            // Splash group
        ksfx_None,                                     // Rip sound
        0,                                             // Fast speed
        0,                                             // Melee range
        0,                                             // Gib health
        -1,                                            // Dropped item
        0,                                             // Pickup width
        0,                                             // Projectile pass height
        0,                                             // Fullbright
        kS_NULL                                        // raisestate
    },

    // MT_MISC54
    {
        "HANGING_DEAD_BLOKE_III",                      // name
        52,                                            // doomednum
        kS_MEAT4,                                      // spawnstate
        1000,                                          // spawnhealth
        kS_NULL,                                       // seestate
        ksfx_None,                                     // seesound
        8,                                             // reactiontime
        ksfx_None,                                     // attacksound
        kS_NULL,                                       // painstate
        0,                                             // painchance
        ksfx_None,                                     // painsound
        kS_NULL,                                       // meleestate
        kS_NULL,                                       // missilestate
        kS_NULL,                                       // deathstate
        kS_NULL,                                       // xdeathstate
        ksfx_None,                                     // deathsound
        0,                                             // speed
        16 * kFracUnit,                                // radius
        68 * kFracUnit,                                // height
        100,                                           // mass
        0,                                             // damage
        ksfx_None,                                     // activesound
        kMF_SOLID | kMF_SPAWNCEILING | kMF_NOGRAVITY,  // flags
        0,                                             // MBF21 flags
        -2,                                            // Infighting group
        -2,                                            // Projectile group
        -2,                                            // Splash group
        ksfx_None,                                     // Rip sound
        0,                                             // Fast speed
        0,                                             // Melee range
        0,                                             // Gib health
        -1,                                            // Dropped item
        0,                                             // Pickup width
        0,                                             // Projectile pass height
        0,                                             // Fullbright
        kS_NULL                                        // raisestate
    },

    // MT_MISC55
    {
        "HANGING_DEAD_BLOKE_IV",                       // name
        53,                                            // doomednum
        kS_MEAT5,                                      // spawnstate
        1000,                                          // spawnhealth
        kS_NULL,                                       // seestate
        ksfx_None,                                     // seesound
        8,                                             // reactiontime
        ksfx_None,                                     // attacksound
        kS_NULL,                                       // painstate
        0,                                             // painchance
        ksfx_None,                                     // painsound
        kS_NULL,                                       // meleestate
        kS_NULL,                                       // missilestate
        kS_NULL,                                       // deathstate
        kS_NULL,                                       // xdeathstate
        ksfx_None,                                     // deathsound
        0,                                             // speed
        16 * kFracUnit,                                // radius
        52 * kFracUnit,                                // height
        100,                                           // mass
        0,                                             // damage
        ksfx_None,                                     // activesound
        kMF_SOLID | kMF_SPAWNCEILING | kMF_NOGRAVITY,  // flags
        0,                                             // MBF21 flags
        -2,                                            // Infighting group
        -2,                                            // Projectile group
        -2,                                            // Splash group
        ksfx_None,                                     // Rip sound
        0,                                             // Fast speed
        0,                                             // Melee range
        0,                                             // Gib health
        -1,                                            // Dropped item
        0,                                             // Pickup width
        0,                                             // Projectile pass height
        0,                                             // Fullbright
        kS_NULL                                        // raisestate
    },

    // MT_MISC56
    {
        "HANGING_DEAD_BLOKE_V",            // name
        59,                                // doomednum
        kS_MEAT2,                          // spawnstate
        1000,                              // spawnhealth
        kS_NULL,                           // seestate
        ksfx_None,                         // seesound
        8,                                 // reactiontime
        ksfx_None,                         // attacksound
        kS_NULL,                           // painstate
        0,                                 // painchance
        ksfx_None,                         // painsound
        kS_NULL,                           // meleestate
        kS_NULL,                           // missilestate
        kS_NULL,                           // deathstate
        kS_NULL,                           // xdeathstate
        ksfx_None,                         // deathsound
        0,                                 // speed
        20 * kFracUnit,                    // radius
        84 * kFracUnit,                    // height
        100,                               // mass
        0,                                 // damage
        ksfx_None,                         // activesound
        kMF_SPAWNCEILING | kMF_NOGRAVITY,  // flags
        0,                                 // MBF21 flags
        -2,                                // Infighting group
        -2,                                // Projectile group
        -2,                                // Splash group
        ksfx_None,                         // Rip sound
        0,                                 // Fast speed
        0,                                 // Melee range
        0,                                 // Gib health
        -1,                                // Dropped item
        0,                                 // Pickup width
        0,                                 // Projectile pass height
        0,                                 // Fullbright
        kS_NULL                            // raisestate
    },

    // MT_MISC57
    {
        "HANGING_DEAD_BLOKE_VI",           // name
        60,                                // doomednum
        kS_MEAT4,                          // spawnstate
        1000,                              // spawnhealth
        kS_NULL,                           // seestate
        ksfx_None,                         // seesound
        8,                                 // reactiontime
        ksfx_None,                         // attacksound
        kS_NULL,                           // painstate
        0,                                 // painchance
        ksfx_None,                         // painsound
        kS_NULL,                           // meleestate
        kS_NULL,                           // missilestate
        kS_NULL,                           // deathstate
        kS_NULL,                           // xdeathstate
        ksfx_None,                         // deathsound
        0,                                 // speed
        20 * kFracUnit,                    // radius
        68 * kFracUnit,                    // height
        100,                               // mass
        0,                                 // damage
        ksfx_None,                         // activesound
        kMF_SPAWNCEILING | kMF_NOGRAVITY,  // flags
        0,                                 // MBF21 flags
        -2,                                // Infighting group
        -2,                                // Projectile group
        -2,                                // Splash group
        ksfx_None,                         // Rip sound
        0,                                 // Fast speed
        0,                                 // Melee range
        0,                                 // Gib health
        -1,                                // Dropped item
        0,                                 // Pickup width
        0,                                 // Projectile pass height
        0,                                 // Fullbright
        kS_NULL                            // raisestate
    },

    // MT_MISC58
    {
        "HANGING_DEAD_BLOKE_VII",          // name
        61,                                // doomednum
        kS_MEAT3,                          // spawnstate
        1000,                              // spawnhealth
        kS_NULL,                           // seestate
        ksfx_None,                         // seesound
        8,                                 // reactiontime
        ksfx_None,                         // attacksound
        kS_NULL,                           // painstate
        0,                                 // painchance
        ksfx_None,                         // painsound
        kS_NULL,                           // meleestate
        kS_NULL,                           // missilestate
        kS_NULL,                           // deathstate
        kS_NULL,                           // xdeathstate
        ksfx_None,                         // deathsound
        0,                                 // speed
        20 * kFracUnit,                    // radius
        52 * kFracUnit,                    // height
        100,                               // mass
        0,                                 // damage
        ksfx_None,                         // activesound
        kMF_SPAWNCEILING | kMF_NOGRAVITY,  // flags
        0,                                 // MBF21 flags
        -2,                                // Infighting group
        -2,                                // Projectile group
        -2,                                // Splash group
        ksfx_None,                         // Rip sound
        0,                                 // Fast speed
        0,                                 // Melee range
        0,                                 // Gib health
        -1,                                // Dropped item
        0,                                 // Pickup width
        0,                                 // Projectile pass height
        0,                                 // Fullbright
        kS_NULL                            // raisestate
    },

    // MT_MISC59
    {
        "HANGING_DEAD_BLOKE_VIII",         // name
        62,                                // doomednum
        kS_MEAT5,                          // spawnstate
        1000,                              // spawnhealth
        kS_NULL,                           // seestate
        ksfx_None,                         // seesound
        8,                                 // reactiontime
        ksfx_None,                         // attacksound
        kS_NULL,                           // painstate
        0,                                 // painchance
        ksfx_None,                         // painsound
        kS_NULL,                           // meleestate
        kS_NULL,                           // missilestate
        kS_NULL,                           // deathstate
        kS_NULL,                           // xdeathstate
        ksfx_None,                         // deathsound
        0,                                 // speed
        20 * kFracUnit,                    // radius
        52 * kFracUnit,                    // height
        100,                               // mass
        0,                                 // damage
        ksfx_None,                         // activesound
        kMF_SPAWNCEILING | kMF_NOGRAVITY,  // flags
        0,                                 // MBF21 flags
        -2,                                // Infighting group
        -2,                                // Projectile group
        -2,                                // Splash group
        ksfx_None,                         // Rip sound
        0,                                 // Fast speed
        0,                                 // Melee range
        0,                                 // Gib health
        -1,                                // Dropped item
        0,                                 // Pickup width
        0,                                 // Projectile pass height
        0,                                 // Fullbright
        kS_NULL                            // raisestate
    },

    // MT_MISC60
    {
        "TWITCHING_BLOKE_II",              // name
        63,                                // doomednum
        kS_BLOODYTWITCH,                   // spawnstate
        1000,                              // spawnhealth
        kS_NULL,                           // seestate
        ksfx_None,                         // seesound
        8,                                 // reactiontime
        ksfx_None,                         // attacksound
        kS_NULL,                           // painstate
        0,                                 // painchance
        ksfx_None,                         // painsound
        kS_NULL,                           // meleestate
        kS_NULL,                           // missilestate
        kS_NULL,                           // deathstate
        kS_NULL,                           // xdeathstate
        ksfx_None,                         // deathsound
        0,                                 // speed
        20 * kFracUnit,                    // radius
        68 * kFracUnit,                    // height
        100,                               // mass
        0,                                 // damage
        ksfx_None,                         // activesound
        kMF_SPAWNCEILING | kMF_NOGRAVITY,  // flags
        0,                                 // MBF21 flags
        -2,                                // Infighting group
        -2,                                // Projectile group
        -2,                                // Splash group
        ksfx_None,                         // Rip sound
        0,                                 // Fast speed
        0,                                 // Melee range
        0,                                 // Gib health
        -1,                                // Dropped item
        0,                                 // Pickup width
        0,                                 // Projectile pass height
        0,                                 // Fullbright
        kS_NULL                            // raisestate
    },

    // MT_MISC61
    {
        "DEAD_CACODEMON",  // name
        22,                // doomednum
        kS_HEAD_DIE6,      // spawnstate
        1000,              // spawnhealth
        kS_NULL,           // seestate
        ksfx_None,         // seesound
        8,                 // reactiontime
        ksfx_None,         // attacksound
        kS_NULL,           // painstate
        0,                 // painchance
        ksfx_None,         // painsound
        kS_NULL,           // meleestate
        kS_NULL,           // missilestate
        kS_NULL,           // deathstate
        kS_NULL,           // xdeathstate
        ksfx_None,         // deathsound
        0,                 // speed
        20 * kFracUnit,    // radius
        16 * kFracUnit,    // height
        100,               // mass
        0,                 // damage
        ksfx_None,         // activesound
        0,                 // flags
        0,                 // MBF21 flags
        -2,                // Infighting group
        -2,                // Projectile group
        -2,                // Splash group
        ksfx_None,         // Rip sound
        0,                 // Fast speed
        0,                 // Melee range
        0,                 // Gib health
        -1,                // Dropped item
        0,                 // Pickup width
        0,                 // Projectile pass height
        0,                 // Fullbright
        kS_NULL            // raisestate
    },

    // MT_MISC62
    {
        "DEAD_PLAYER",   // name
        15,              // doomednum
        kS_PLAY_DIE7,    // spawnstate
        1000,            // spawnhealth
        kS_NULL,         // seestate
        ksfx_None,       // seesound
        8,               // reactiontime
        ksfx_None,       // attacksound
        kS_NULL,         // painstate
        0,               // painchance
        ksfx_None,       // painsound
        kS_NULL,         // meleestate
        kS_NULL,         // missilestate
        kS_NULL,         // deathstate
        kS_NULL,         // xdeathstate
        ksfx_None,       // deathsound
        0,               // speed
        20 * kFracUnit,  // radius
        16 * kFracUnit,  // height
        100,             // mass
        0,               // damage
        ksfx_None,       // activesound
        0,               // flags
        0,               // MBF21 flags
        -2,              // Infighting group
        -2,              // Projectile group
        -2,              // Splash group
        ksfx_None,       // Rip sound
        0,               // Fast speed
        0,               // Melee range
        0,               // Gib health
        -1,              // Dropped item
        0,               // Pickup width
        0,               // Projectile pass height
        0,               // Fullbright
        kS_NULL          // raisestate
    },

    // MT_MISC63
    {
        "DEAD_FORMER_HUMAN",  // name
        18,                   // doomednum
        kS_POSS_DIE5,         // spawnstate
        1000,                 // spawnhealth
        kS_NULL,              // seestate
        ksfx_None,            // seesound
        8,                    // reactiontime
        ksfx_None,            // attacksound
        kS_NULL,              // painstate
        0,                    // painchance
        ksfx_None,            // painsound
        kS_NULL,              // meleestate
        kS_NULL,              // missilestate
        kS_NULL,              // deathstate
        kS_NULL,              // xdeathstate
        ksfx_None,            // deathsound
        0,                    // speed
        20 * kFracUnit,       // radius
        16 * kFracUnit,       // height
        100,                  // mass
        0,                    // damage
        ksfx_None,            // activesound
        0,                    // flags
        0,                    // MBF21 flags
        -2,                   // Infighting group
        -2,                   // Projectile group
        -2,                   // Splash group
        ksfx_None,            // Rip sound
        0,                    // Fast speed
        0,                    // Melee range
        0,                    // Gib health
        -1,                   // Dropped item
        0,                    // Pickup width
        0,                    // Projectile pass height
        0,                    // Fullbright
        kS_NULL               // raisestate
    },

    // MT_MISC64
    {
        "DEAD_DEMON",    // name
        21,              // doomednum
        kS_SARG_DIE6,    // spawnstate
        1000,            // spawnhealth
        kS_NULL,         // seestate
        ksfx_None,       // seesound
        8,               // reactiontime
        ksfx_None,       // attacksound
        kS_NULL,         // painstate
        0,               // painchance
        ksfx_None,       // painsound
        kS_NULL,         // meleestate
        kS_NULL,         // missilestate
        kS_NULL,         // deathstate
        kS_NULL,         // xdeathstate
        ksfx_None,       // deathsound
        0,               // speed
        20 * kFracUnit,  // radius
        16 * kFracUnit,  // height
        100,             // mass
        0,               // damage
        ksfx_None,       // activesound
        0,               // flags
        0,               // MBF21 flags
        -2,              // Infighting group
        -2,              // Projectile group
        -2,              // Splash group
        ksfx_None,       // Rip sound
        0,               // Fast speed
        0,               // Melee range
        0,               // Gib health
        -1,              // Dropped item
        0,               // Pickup width
        0,               // Projectile pass height
        0,               // Fullbright
        kS_NULL          // raisestate
    },

    // MT_MISC65
    {
        "DEAD_LOSTSOUL",  // name
        23,               // doomednum
        kS_SKULL_DIE6,    // spawnstate
        1000,             // spawnhealth
        kS_NULL,          // seestate
        ksfx_None,        // seesound
        8,                // reactiontime
        ksfx_None,        // attacksound
        kS_NULL,          // painstate
        0,                // painchance
        ksfx_None,        // painsound
        kS_NULL,          // meleestate
        kS_NULL,          // missilestate
        kS_NULL,          // deathstate
        kS_NULL,          // xdeathstate
        ksfx_None,        // deathsound
        0,                // speed
        20 * kFracUnit,   // radius
        16 * kFracUnit,   // height
        100,              // mass
        0,                // damage
        ksfx_None,        // activesound
        0,                // flags
        0,                // MBF21 flags
        -2,               // Infighting group
        -2,               // Projectile group
        -2,               // Splash group
        ksfx_None,        // Rip sound
        0,                // Fast speed
        0,                // Melee range
        0,                // Gib health
        -1,               // Dropped item
        0,                // Pickup width
        0,                // Projectile pass height
        0,                // Fullbright
        kS_NULL           // raisestate
    },

    // MT_MISC66
    {
        "DEAD_IMP",      // name
        20,              // doomednum
        kS_TROO_DIE5,    // spawnstate
        1000,            // spawnhealth
        kS_NULL,         // seestate
        ksfx_None,       // seesound
        8,               // reactiontime
        ksfx_None,       // attacksound
        kS_NULL,         // painstate
        0,               // painchance
        ksfx_None,       // painsound
        kS_NULL,         // meleestate
        kS_NULL,         // missilestate
        kS_NULL,         // deathstate
        kS_NULL,         // xdeathstate
        ksfx_None,       // deathsound
        0,               // speed
        20 * kFracUnit,  // radius
        16 * kFracUnit,  // height
        100,             // mass
        0,               // damage
        ksfx_None,       // activesound
        0,               // flags
        0,               // MBF21 flags
        -2,              // Infighting group
        -2,              // Projectile group
        -2,              // Splash group
        ksfx_None,       // Rip sound
        0,               // Fast speed
        0,               // Melee range
        0,               // Gib health
        -1,              // Dropped item
        0,               // Pickup width
        0,               // Projectile pass height
        0,               // Fullbright
        kS_NULL          // raisestate
    },

    // MT_MISC67
    {
        "DEAD_FORMER_SARG",  // name
        19,                  // doomednum
        kS_SPOS_DIE5,        // spawnstate
        1000,                // spawnhealth
        kS_NULL,             // seestate
        ksfx_None,           // seesound
        8,                   // reactiontime
        ksfx_None,           // attacksound
        kS_NULL,             // painstate
        0,                   // painchance
        ksfx_None,           // painsound
        kS_NULL,             // meleestate
        kS_NULL,             // missilestate
        kS_NULL,             // deathstate
        kS_NULL,             // xdeathstate
        ksfx_None,           // deathsound
        0,                   // speed
        20 * kFracUnit,      // radius
        16 * kFracUnit,      // height
        100,                 // mass
        0,                   // damage
        ksfx_None,           // activesound
        0,                   // flags
        0,                   // MBF21 flags
        -2,                  // Infighting group
        -2,                  // Projectile group
        -2,                  // Splash group
        ksfx_None,           // Rip sound
        0,                   // Fast speed
        0,                   // Melee range
        0,                   // Gib health
        -1,                  // Dropped item
        0,                   // Pickup width
        0,                   // Projectile pass height
        0,                   // Fullbright
        kS_NULL              // raisestate
    },

    // MT_MISC68
    {
        "DEAD_GIBBER_PLAYER1",  // name
        10,                     // doomednum
        kS_PLAY_XDIE9,          // spawnstate
        1000,                   // spawnhealth
        kS_NULL,                // seestate
        ksfx_None,              // seesound
        8,                      // reactiontime
        ksfx_None,              // attacksound
        kS_NULL,                // painstate
        0,                      // painchance
        ksfx_None,              // painsound
        kS_NULL,                // meleestate
        kS_NULL,                // missilestate
        kS_NULL,                // deathstate
        kS_NULL,                // xdeathstate
        ksfx_None,              // deathsound
        0,                      // speed
        20 * kFracUnit,         // radius
        16 * kFracUnit,         // height
        100,                    // mass
        0,                      // damage
        ksfx_None,              // activesound
        0,                      // flags
        0,                      // MBF21 flags
        -2,                     // Infighting group
        -2,                     // Projectile group
        -2,                     // Splash group
        ksfx_None,              // Rip sound
        0,                      // Fast speed
        0,                      // Melee range
        0,                      // Gib health
        -1,                     // Dropped item
        0,                      // Pickup width
        0,                      // Projectile pass height
        0,                      // Fullbright
        kS_NULL                 // raisestate
    },

    // MT_MISC69
    {
        "DEAD_GIBBED_PLAYER2",  // name
        12,                     // doomednum
        kS_PLAY_XDIE9,          // spawnstate
        1000,                   // spawnhealth
        kS_NULL,                // seestate
        ksfx_None,              // seesound
        8,                      // reactiontime
        ksfx_None,              // attacksound
        kS_NULL,                // painstate
        0,                      // painchance
        ksfx_None,              // painsound
        kS_NULL,                // meleestate
        kS_NULL,                // missilestate
        kS_NULL,                // deathstate
        kS_NULL,                // xdeathstate
        ksfx_None,              // deathsound
        0,                      // speed
        20 * kFracUnit,         // radius
        16 * kFracUnit,         // height
        100,                    // mass
        0,                      // damage
        ksfx_None,              // activesound
        0,                      // flags
        0,                      // MBF21 flags
        -2,                     // Infighting group
        -2,                     // Projectile group
        -2,                     // Splash group
        ksfx_None,              // Rip sound
        0,                      // Fast speed
        0,                      // Melee range
        0,                      // Gib health
        -1,                     // Dropped item
        0,                      // Pickup width
        0,                      // Projectile pass height
        0,                      // Fullbright
        kS_NULL                 // raisestate
    },

    // MT_MISC70
    {
        "HEADS_ON_A_STICK",  // name
        28,                  // doomednum
        kS_HEADSONSTICK,     // spawnstate
        1000,                // spawnhealth
        kS_NULL,             // seestate
        ksfx_None,           // seesound
        8,                   // reactiontime
        ksfx_None,           // attacksound
        kS_NULL,             // painstate
        0,                   // painchance
        ksfx_None,           // painsound
        kS_NULL,             // meleestate
        kS_NULL,             // missilestate
        kS_NULL,             // deathstate
        kS_NULL,             // xdeathstate
        ksfx_None,           // deathsound
        0,                   // speed
        16 * kFracUnit,      // radius
        16 * kFracUnit,      // height
        100,                 // mass
        0,                   // damage
        ksfx_None,           // activesound
        kMF_SOLID,           // flags
        0,                   // MBF21 flags
        -2,                  // Infighting group
        -2,                  // Projectile group
        -2,                  // Splash group
        ksfx_None,           // Rip sound
        0,                   // Fast speed
        0,                   // Melee range
        0,                   // Gib health
        -1,                  // Dropped item
        0,                   // Pickup width
        0,                   // Projectile pass height
        0,                   // Fullbright
        kS_NULL              // raisestate
    },

    // MT_MISC71
    {
        "POOL_OF_BLOOD",  // name
        24,               // doomednum
        kS_GIBS,          // spawnstate
        1000,             // spawnhealth
        kS_NULL,          // seestate
        ksfx_None,        // seesound
        8,                // reactiontime
        ksfx_None,        // attacksound
        kS_NULL,          // painstate
        0,                // painchance
        ksfx_None,        // painsound
        kS_NULL,          // meleestate
        kS_NULL,          // missilestate
        kS_NULL,          // deathstate
        kS_NULL,          // xdeathstate
        ksfx_None,        // deathsound
        0,                // speed
        20 * kFracUnit,   // radius
        16 * kFracUnit,   // height
        100,              // mass
        0,                // damage
        ksfx_None,        // activesound
        0,                // flags
        0,                // MBF21 flags
        -2,               // Infighting group
        -2,               // Projectile group
        -2,               // Splash group
        ksfx_None,        // Rip sound
        0,                // Fast speed
        0,                // Melee range
        0,                // Gib health
        -1,               // Dropped item
        0,                // Pickup width
        0,                // Projectile pass height
        0,                // Fullbright
        kS_NULL           // raisestate
    },

    // MT_MISC72
    {
        "SKULL_ON_A_STICK",  // name
        27,                  // doomednum
        kS_HEADONASTICK,     // spawnstate
        1000,                // spawnhealth
        kS_NULL,             // seestate
        ksfx_None,           // seesound
        8,                   // reactiontime
        ksfx_None,           // attacksound
        kS_NULL,             // painstate
        0,                   // painchance
        ksfx_None,           // painsound
        kS_NULL,             // meleestate
        kS_NULL,             // missilestate
        kS_NULL,             // deathstate
        kS_NULL,             // xdeathstate
        ksfx_None,           // deathsound
        0,                   // speed
        16 * kFracUnit,      // radius
        16 * kFracUnit,      // height
        100,                 // mass
        0,                   // damage
        ksfx_None,           // activesound
        kMF_SOLID,           // flags
        0,                   // MBF21 flags
        -2,                  // Infighting group
        -2,                  // Projectile group
        -2,                  // Splash group
        ksfx_None,           // Rip sound
        0,                   // Fast speed
        0,                   // Melee range
        0,                   // Gib health
        -1,                  // Dropped item
        0,                   // Pickup width
        0,                   // Projectile pass height
        0,                   // Fullbright
        kS_NULL              // raisestate
    },

    // MT_MISC73
    {
        "SKULL_CENTREPIECE",  // name
        29,                   // doomednum
        kS_HEADCANDLES,       // spawnstate
        1000,                 // spawnhealth
        kS_NULL,              // seestate
        ksfx_None,            // seesound
        8,                    // reactiontime
        ksfx_None,            // attacksound
        kS_NULL,              // painstate
        0,                    // painchance
        ksfx_None,            // painsound
        kS_NULL,              // meleestate
        kS_NULL,              // missilestate
        kS_NULL,              // deathstate
        kS_NULL,              // xdeathstate
        ksfx_None,            // deathsound
        0,                    // speed
        16 * kFracUnit,       // radius
        16 * kFracUnit,       // height
        100,                  // mass
        0,                    // damage
        ksfx_None,            // activesound
        kMF_SOLID,            // flags
        0,                    // MBF21 flags
        -2,                   // Infighting group
        -2,                   // Projectile group
        -2,                   // Splash group
        ksfx_None,            // Rip sound
        0,                    // Fast speed
        0,                    // Melee range
        0,                    // Gib health
        -1,                   // Dropped item
        0,                    // Pickup width
        0,                    // Projectile pass height
        0,                    // Fullbright
        kS_NULL               // raisestate
    },

    // MT_MISC74
    {
        "SKEWERED_BLOKE",  // name
        25,                // doomednum
        kS_DEADSTICK,      // spawnstate
        1000,              // spawnhealth
        kS_NULL,           // seestate
        ksfx_None,         // seesound
        8,                 // reactiontime
        ksfx_None,         // attacksound
        kS_NULL,           // painstate
        0,                 // painchance
        ksfx_None,         // painsound
        kS_NULL,           // meleestate
        kS_NULL,           // missilestate
        kS_NULL,           // deathstate
        kS_NULL,           // xdeathstate
        ksfx_None,         // deathsound
        0,                 // speed
        16 * kFracUnit,    // radius
        16 * kFracUnit,    // height
        100,               // mass
        0,                 // damage
        ksfx_None,         // activesound
        kMF_SOLID,         // flags
        0,                 // MBF21 flags
        -2,                // Infighting group
        -2,                // Projectile group
        -2,                // Splash group
        ksfx_None,         // Rip sound
        0,                 // Fast speed
        0,                 // Melee range
        0,                 // Gib health
        -1,                // Dropped item
        0,                 // Pickup width
        0,                 // Projectile pass height
        0,                 // Fullbright
        kS_NULL            // raisestate
    },

    // MT_MISC75
    {
        "DYING_SKEWERED_BLOKE",  // name
        26,                      // doomednum
        kS_LIVESTICK,            // spawnstate
        1000,                    // spawnhealth
        kS_NULL,                 // seestate
        ksfx_None,               // seesound
        8,                       // reactiontime
        ksfx_None,               // attacksound
        kS_NULL,                 // painstate
        0,                       // painchance
        ksfx_None,               // painsound
        kS_NULL,                 // meleestate
        kS_NULL,                 // missilestate
        kS_NULL,                 // deathstate
        kS_NULL,                 // xdeathstate
        ksfx_None,               // deathsound
        0,                       // speed
        16 * kFracUnit,          // radius
        16 * kFracUnit,          // height
        100,                     // mass
        0,                       // damage
        ksfx_None,               // activesound
        kMF_SOLID,               // flags
        0,                       // MBF21 flags
        -2,                      // Infighting group
        -2,                      // Projectile group
        -2,                      // Splash group
        ksfx_None,               // Rip sound
        0,                       // Fast speed
        0,                       // Melee range
        0,                       // Gib health
        -1,                      // Dropped item
        0,                       // Pickup width
        0,                       // Projectile pass height
        0,                       // Fullbright
        kS_NULL                  // raisestate
    },

    // MT_MISC76
    {
        "BIG_TREE",      // name
        54,              // doomednum
        kS_BIGTREE,      // spawnstate
        1000,            // spawnhealth
        kS_NULL,         // seestate
        ksfx_None,       // seesound
        8,               // reactiontime
        ksfx_None,       // attacksound
        kS_NULL,         // painstate
        0,               // painchance
        ksfx_None,       // painsound
        kS_NULL,         // meleestate
        kS_NULL,         // missilestate
        kS_NULL,         // deathstate
        kS_NULL,         // xdeathstate
        ksfx_None,       // deathsound
        0,               // speed
        32 * kFracUnit,  // radius
        16 * kFracUnit,  // height
        100,             // mass
        0,               // damage
        ksfx_None,       // activesound
        kMF_SOLID,       // flags
        0,               // MBF21 flags
        -2,              // Infighting group
        -2,              // Projectile group
        -2,              // Splash group
        ksfx_None,       // Rip sound
        0,               // Fast speed
        0,               // Melee range
        0,               // Gib health
        -1,              // Dropped item
        0,               // Pickup width
        0,               // Projectile pass height
        0,               // Fullbright
        kS_NULL          // raisestate
    },

    // MT_MISC77
    {
        "BURNING_BARREL",  // name
        70,                // doomednum
        kS_BBAR1,          // spawnstate
        1000,              // spawnhealth
        kS_NULL,           // seestate
        ksfx_None,         // seesound
        8,                 // reactiontime
        ksfx_None,         // attacksound
        kS_NULL,           // painstate
        0,                 // painchance
        ksfx_None,         // painsound
        kS_NULL,           // meleestate
        kS_NULL,           // missilestate
        kS_NULL,           // deathstate
        kS_NULL,           // xdeathstate
        ksfx_None,         // deathsound
        0,                 // speed
        16 * kFracUnit,    // radius
        16 * kFracUnit,    // height
        100,               // mass
        0,                 // damage
        ksfx_None,         // activesound
        kMF_SOLID,         // flags
        0,                 // MBF21 flags
        -2,                // Infighting group
        -2,                // Projectile group
        -2,                // Splash group
        ksfx_None,         // Rip sound
        0,                 // Fast speed
        0,                 // Melee range
        0,                 // Gib health
        -1,                // Dropped item
        0,                 // Pickup width
        0,                 // Projectile pass height
        0,                 // Fullbright
        kS_NULL            // raisestate
    },

    // MT_MISC78
    {
        "GUTTED_HUNG_BLOKE_I",                         // name
        73,                                            // doomednum
        kS_HANGNOGUTS,                                 // spawnstate
        1000,                                          // spawnhealth
        kS_NULL,                                       // seestate
        ksfx_None,                                     // seesound
        8,                                             // reactiontime
        ksfx_None,                                     // attacksound
        kS_NULL,                                       // painstate
        0,                                             // painchance
        ksfx_None,                                     // painsound
        kS_NULL,                                       // meleestate
        kS_NULL,                                       // missilestate
        kS_NULL,                                       // deathstate
        kS_NULL,                                       // xdeathstate
        ksfx_None,                                     // deathsound
        0,                                             // speed
        16 * kFracUnit,                                // radius
        88 * kFracUnit,                                // height
        100,                                           // mass
        0,                                             // damage
        ksfx_None,                                     // activesound
        kMF_SOLID | kMF_SPAWNCEILING | kMF_NOGRAVITY,  // flags
        0,                                             // MBF21 flags
        -2,                                            // Infighting group
        -2,                                            // Projectile group
        -2,                                            // Splash group
        ksfx_None,                                     // Rip sound
        0,                                             // Fast speed
        0,                                             // Melee range
        0,                                             // Gib health
        -1,                                            // Dropped item
        0,                                             // Pickup width
        0,                                             // Projectile pass height
        0,                                             // Fullbright
        kS_NULL                                        // raisestate
    },

    // MT_MISC79
    {
        "GUTTED_HUNG_BLOKE_II",                        // name
        74,                                            // doomednum
        kS_HANGBNOBRAIN,                               // spawnstate
        1000,                                          // spawnhealth
        kS_NULL,                                       // seestate
        ksfx_None,                                     // seesound
        8,                                             // reactiontime
        ksfx_None,                                     // attacksound
        kS_NULL,                                       // painstate
        0,                                             // painchance
        ksfx_None,                                     // painsound
        kS_NULL,                                       // meleestate
        kS_NULL,                                       // missilestate
        kS_NULL,                                       // deathstate
        kS_NULL,                                       // xdeathstate
        ksfx_None,                                     // deathsound
        0,                                             // speed
        16 * kFracUnit,                                // radius
        88 * kFracUnit,                                // height
        100,                                           // mass
        0,                                             // damage
        ksfx_None,                                     // activesound
        kMF_SOLID | kMF_SPAWNCEILING | kMF_NOGRAVITY,  // flags
        0,                                             // MBF21 flags
        -2,                                            // Infighting group
        -2,                                            // Projectile group
        -2,                                            // Splash group
        ksfx_None,                                     // Rip sound
        0,                                             // Fast speed
        0,                                             // Melee range
        0,                                             // Gib health
        -1,                                            // Dropped item
        0,                                             // Pickup width
        0,                                             // Projectile pass height
        0,                                             // Fullbright
        kS_NULL                                        // raisestate
    },

    // MT_MISC80
    {
        "GUTTED_TORSO_I",                              // name
        75,                                            // doomednum
        kS_HANGTLOOKDN,                                // spawnstate
        1000,                                          // spawnhealth
        kS_NULL,                                       // seestate
        ksfx_None,                                     // seesound
        8,                                             // reactiontime
        ksfx_None,                                     // attacksound
        kS_NULL,                                       // painstate
        0,                                             // painchance
        ksfx_None,                                     // painsound
        kS_NULL,                                       // meleestate
        kS_NULL,                                       // missilestate
        kS_NULL,                                       // deathstate
        kS_NULL,                                       // xdeathstate
        ksfx_None,                                     // deathsound
        0,                                             // speed
        16 * kFracUnit,                                // radius
        64 * kFracUnit,                                // height
        100,                                           // mass
        0,                                             // damage
        ksfx_None,                                     // activesound
        kMF_SOLID | kMF_SPAWNCEILING | kMF_NOGRAVITY,  // flags
        0,                                             // MBF21 flags
        -2,                                            // Infighting group
        -2,                                            // Projectile group
        -2,                                            // Splash group
        ksfx_None,                                     // Rip sound
        0,                                             // Fast speed
        0,                                             // Melee range
        0,                                             // Gib health
        -1,                                            // Dropped item
        0,                                             // Pickup width
        0,                                             // Projectile pass height
        0,                                             // Fullbright
        kS_NULL                                        // raisestate
    },

    // MT_MISC81
    {
        "GUTTED_TORSO_II",                             // name
        76,                                            // doomednum
        kS_HANGTSKULL,                                 // spawnstate
        1000,                                          // spawnhealth
        kS_NULL,                                       // seestate
        ksfx_None,                                     // seesound
        8,                                             // reactiontime
        ksfx_None,                                     // attacksound
        kS_NULL,                                       // painstate
        0,                                             // painchance
        ksfx_None,                                     // painsound
        kS_NULL,                                       // meleestate
        kS_NULL,                                       // missilestate
        kS_NULL,                                       // deathstate
        kS_NULL,                                       // xdeathstate
        ksfx_None,                                     // deathsound
        0,                                             // speed
        16 * kFracUnit,                                // radius
        64 * kFracUnit,                                // height
        100,                                           // mass
        0,                                             // damage
        ksfx_None,                                     // activesound
        kMF_SOLID | kMF_SPAWNCEILING | kMF_NOGRAVITY,  // flags
        0,                                             // MBF21 flags
        -2,                                            // Infighting group
        -2,                                            // Projectile group
        -2,                                            // Splash group
        ksfx_None,                                     // Rip sound
        0,                                             // Fast speed
        0,                                             // Melee range
        0,                                             // Gib health
        -1,                                            // Dropped item
        0,                                             // Pickup width
        0,                                             // Projectile pass height
        0,                                             // Fullbright
        kS_NULL                                        // raisestate
    },

    // MT_MISC82
    {
        "GUTTED_TORSO_III",                            // name
        77,                                            // doomednum
        kS_HANGTLOOKUP,                                // spawnstate
        1000,                                          // spawnhealth
        kS_NULL,                                       // seestate
        ksfx_None,                                     // seesound
        8,                                             // reactiontime
        ksfx_None,                                     // attacksound
        kS_NULL,                                       // painstate
        0,                                             // painchance
        ksfx_None,                                     // painsound
        kS_NULL,                                       // meleestate
        kS_NULL,                                       // missilestate
        kS_NULL,                                       // deathstate
        kS_NULL,                                       // xdeathstate
        ksfx_None,                                     // deathsound
        0,                                             // speed
        16 * kFracUnit,                                // radius
        64 * kFracUnit,                                // height
        100,                                           // mass
        0,                                             // damage
        ksfx_None,                                     // activesound
        kMF_SOLID | kMF_SPAWNCEILING | kMF_NOGRAVITY,  // flags
        0,                                             // MBF21 flags
        -2,                                            // Infighting group
        -2,                                            // Projectile group
        -2,                                            // Splash group
        ksfx_None,                                     // Rip sound
        0,                                             // Fast speed
        0,                                             // Melee range
        0,                                             // Gib health
        -1,                                            // Dropped item
        0,                                             // Pickup width
        0,                                             // Projectile pass height
        0,                                             // Fullbright
        kS_NULL                                        // raisestate
    },

    // MT_MISC83
    {
        "GUTTED_TORSO_IV",                             // name
        78,                                            // doomednum
        kS_HANGTNOBRAIN,                               // spawnstate
        1000,                                          // spawnhealth
        kS_NULL,                                       // seestate
        ksfx_None,                                     // seesound
        8,                                             // reactiontime
        ksfx_None,                                     // attacksound
        kS_NULL,                                       // painstate
        0,                                             // painchance
        ksfx_None,                                     // painsound
        kS_NULL,                                       // meleestate
        kS_NULL,                                       // missilestate
        kS_NULL,                                       // deathstate
        kS_NULL,                                       // xdeathstate
        ksfx_None,                                     // deathsound
        0,                                             // speed
        16 * kFracUnit,                                // radius
        64 * kFracUnit,                                // height
        100,                                           // mass
        0,                                             // damage
        ksfx_None,                                     // activesound
        kMF_SOLID | kMF_SPAWNCEILING | kMF_NOGRAVITY,  // flags
        0,                                             // MBF21 flags
        -2,                                            // Infighting group
        -2,                                            // Projectile group
        -2,                                            // Splash group
        ksfx_None,                                     // Rip sound
        0,                                             // Fast speed
        0,                                             // Melee range
        0,                                             // Gib health
        -1,                                            // Dropped item
        0,                                             // Pickup width
        0,                                             // Projectile pass height
        0,                                             // Fullbright
        kS_NULL                                        // raisestate
    },

    // MT_MISC84
    {
        "POOL_OF_BLOOD_I",  // name
        79,                 // doomednum
        kS_COLONGIBS,       // spawnstate
        1000,               // spawnhealth
        kS_NULL,            // seestate
        ksfx_None,          // seesound
        8,                  // reactiontime
        ksfx_None,          // attacksound
        kS_NULL,            // painstate
        0,                  // painchance
        ksfx_None,          // painsound
        kS_NULL,            // meleestate
        kS_NULL,            // missilestate
        kS_NULL,            // deathstate
        kS_NULL,            // xdeathstate
        ksfx_None,          // deathsound
        0,                  // speed
        20 * kFracUnit,     // radius
        16 * kFracUnit,     // height
        100,                // mass
        0,                  // damage
        ksfx_None,          // activesound
        kMF_NOBLOCKMAP,     // flags
        0,                  // MBF21 flags
        -2,                 // Infighting group
        -2,                 // Projectile group
        -2,                 // Splash group
        ksfx_None,          // Rip sound
        0,                  // Fast speed
        0,                  // Melee range
        0,                  // Gib health
        -1,                 // Dropped item
        0,                  // Pickup width
        0,                  // Projectile pass height
        0,                  // Fullbright
        kS_NULL             // raisestate
    },

    // MT_MISC85
    {
        "POOL_OF_BLOOD_II",  // name
        80,                  // doomednum
        kS_SMALLPOOL,        // spawnstate
        1000,                // spawnhealth
        kS_NULL,             // seestate
        ksfx_None,           // seesound
        8,                   // reactiontime
        ksfx_None,           // attacksound
        kS_NULL,             // painstate
        0,                   // painchance
        ksfx_None,           // painsound
        kS_NULL,             // meleestate
        kS_NULL,             // missilestate
        kS_NULL,             // deathstate
        kS_NULL,             // xdeathstate
        ksfx_None,           // deathsound
        0,                   // speed
        20 * kFracUnit,      // radius
        16 * kFracUnit,      // height
        100,                 // mass
        0,                   // damage
        ksfx_None,           // activesound
        kMF_NOBLOCKMAP,      // flags
        0,                   // MBF21 flags
        -2,                  // Infighting group
        -2,                  // Projectile group
        -2,                  // Splash group
        ksfx_None,           // Rip sound
        0,                   // Fast speed
        0,                   // Melee range
        0,                   // Gib health
        -1,                  // Dropped item
        0,                   // Pickup width
        0,                   // Projectile pass height
        0,                   // Fullbright
        kS_NULL              // raisestate
    },

    // MT_MISC86
    {
        "BRAINSTEM",     // name
        81,              // doomednum
        kS_BRAINSTEM,    // spawnstate
        1000,            // spawnhealth
        kS_NULL,         // seestate
        ksfx_None,       // seesound
        8,               // reactiontime
        ksfx_None,       // attacksound
        kS_NULL,         // painstate
        0,               // painchance
        ksfx_None,       // painsound
        kS_NULL,         // meleestate
        kS_NULL,         // missilestate
        kS_NULL,         // deathstate
        kS_NULL,         // xdeathstate
        ksfx_None,       // deathsound
        0,               // speed
        20 * kFracUnit,  // radius
        16 * kFracUnit,  // height
        100,             // mass
        0,               // damage
        ksfx_None,       // activesound
        kMF_NOBLOCKMAP,  // flags
        0,               // MBF21 flags
        -2,              // Infighting group
        -2,              // Projectile group
        -2,              // Splash group
        ksfx_None,       // Rip sound
        0,               // Fast speed
        0,               // Melee range
        0,               // Gib health
        -1,              // Dropped item
        0,               // Pickup width
        0,               // Projectile pass height
        0,               // Fullbright
        kS_NULL          // raisestate
    },

    // ============= BOOM and MBF things =============

    // MT_PUSH
    {
        "POINT_PUSHER",  // name
        5001,            // doomednum
        kS_TNT1,         // spawnstate
        1000,            // spawnhealth
        kS_NULL,         // seestate
        ksfx_None,       // seesound
        8,               // reactiontime
        ksfx_None,       // attacksound
        kS_NULL,         // painstate
        0,               // painchance
        ksfx_None,       // painsound
        kS_NULL,         // meleestate
        kS_NULL,         // missilestate
        kS_NULL,         // deathstate
        kS_NULL,         // xdeathstate
        ksfx_None,       // deathsound
        0,               // speed
        kFracUnit / 8,   // radius   /* MOD */
        kFracUnit / 8,   // height   /* MOD */
        10,              // mass
        0,               // damage
        ksfx_None,       // activesound
        kMF_NOBLOCKMAP,  // flags
        0,               // MBF21 flags
        -2,              // Infighting group
        -2,              // Projectile group
        -2,              // Splash group
        ksfx_None,       // Rip sound
        0,               // Fast speed
        0,               // Melee range
        0,               // Gib health
        -1,              // Dropped item
        0,               // Pickup width
        0,               // Projectile pass height
        0,               // Fullbright
        kS_NULL          // raisestate
    },

    // MT_PULL
    {
        "POINT_PULLER",  // name
        5002,            // doomednum
        kS_TNT1,         // spawnstate
        1000,            // spawnhealth
        kS_NULL,         // seestate
        ksfx_None,       // seesound
        8,               // reactiontime
        ksfx_None,       // attacksound
        kS_NULL,         // painstate
        0,               // painchance
        ksfx_None,       // painsound
        kS_NULL,         // meleestate
        kS_NULL,         // missilestate
        kS_NULL,         // deathstate
        kS_NULL,         // xdeathstate
        ksfx_None,       // deathsound
        0,               // speed
        kFracUnit / 8,   // radius   /* MOD */
        kFracUnit / 8,   // height   /* MOD */
        10,              // mass
        0,               // damage
        ksfx_None,       // activesound
        kMF_NOBLOCKMAP,  // flags
        0,               // MBF21 flags
        -2,              // Infighting group
        -2,              // Projectile group
        -2,              // Splash group
        ksfx_None,       // Rip sound
        0,               // Fast speed
        0,               // Melee range
        0,               // Gib health
        -1,              // Dropped item
        0,               // Pickup width
        0,               // Projectile pass height
        0,               // Fullbright
        kS_NULL          // raisestate
    },

    // Marine's best friend :)      // killough 7/19/98

    // MT_DOGS
    {
        "DOG",                                      // name
        888,                                        // doomednum
        kS_DOGS_STND,                               // spawnstate
        500,                                        // spawnhealth
        kS_DOGS_RUN1,                               // seestate
        ksfx_dgsit,                                 // seesound
        8,                                          // reactiontime
        ksfx_dgatk,                                 // attacksound
        kS_DOGS_PAIN,                               // painstate
        180,                                        // painchance
        ksfx_dgpain,                                // painsound
        kS_DOGS_ATK1,                               // meleestate
        0,                                          // missilestate
        kS_DOGS_DIE1,                               // deathstate
        kS_NULL,                                    // xdeathstate
        ksfx_dgdth,                                 // deathsound
        10,                                         // speed
        12 * kFracUnit,                             // radius
        28 * kFracUnit,                             // height
        100,                                        // mass
        0,                                          // damage
        ksfx_dgact,                                 // activesound
        kMF_SOLID | kMF_SHOOTABLE | kMF_COUNTKILL,  // flags
        0,                                          // MBF21 flags
        -2,                                         // Infighting group
        -2,                                         // Projectile group
        -2,                                         // Splash group
        ksfx_None,                                  // Rip sound
        0,                                          // Fast speed
        0,                                          // Melee range
        0,                                          // Gib health
        -1,                                         // Dropped item
        0,                                          // Pickup width
        0,                                          // Projectile pass height
        0,                                          // Fullbright
        kS_DOGS_RAISE1                              // raisestate
    },

    // MT_PLASMA1
    {
        "BETA_PLASMA_1",  // name
        -1,               // doomednum
        kS_PLS1BALL,      // spawnstate
        1000,             // spawnhealth
        kS_NULL,          // seestate
        ksfx_plasma,      // seesound
        8,                // reactiontime
        ksfx_None,        // attacksound
        kS_NULL,          // painstate
        0,                // painchance
        ksfx_None,        // painsound
        kS_NULL,          // meleestate
        0,                // missilestate
        kS_PLS1EXP,       // deathstate
        kS_NULL,          // xdeathstate
        ksfx_firxpl,      // deathsound
        25 * kFracUnit,   // speed
        13 * kFracUnit,   // radius
        8 * kFracUnit,    // height
        100,              // mass
        4,                // damage
        ksfx_None,        // activesound
        kMF_NOBLOCKMAP | kMF_MISSILE | kMF_DROPOFF | kMF_BOUNCES,  // flags
        0,          // MBF21 flags
        -2,         // Infighting group
        -2,         // Projectile group
        -2,         // Splash group
        ksfx_None,  // Rip sound
        0,          // Fast speed
        0,          // Melee range
        0,          // Gib health
        -1,         // Dropped item
        0,          // Pickup width
        0,          // Projectile pass height
        0,          // Fullbright
        kS_NULL     // raisestate
    },

    // MT_PLASMA2
    {
        "BETA_PLASMA_2",  // name
        -1,               // doomednum
        kS_PLS2BALL,      // spawnstate
        1000,             // spawnhealth
        kS_NULL,          // seestate
        ksfx_plasma,      // seesound
        8,                // reactiontime
        ksfx_None,        // attacksound
        kS_NULL,          // painstate
        0,                // painchance
        ksfx_None,        // painsound
        kS_NULL,          // meleestate
        0,                // missilestate
        kS_PLS2BALLX1,    // deathstate
        kS_NULL,          // xdeathstate
        ksfx_firxpl,      // deathsound
        25 * kFracUnit,   // speed
        6 * kFracUnit,    // radius
        8 * kFracUnit,    // height
        100,              // mass
        4,                // damage
        ksfx_None,        // activesound
        kMF_NOBLOCKMAP | kMF_MISSILE | kMF_DROPOFF | kMF_BOUNCES,  // flags
        0,          // MBF21 flags
        -2,         // Infighting group
        -2,         // Projectile group
        -2,         // Splash group
        ksfx_None,  // Rip sound
        0,          // Fast speed
        0,          // Melee range
        0,          // Gib health
        -1,         // Dropped item
        0,          // Pickup width
        0,          // Projectile pass height
        0,          // Fullbright
        kS_NULL     // raisestate
    },

    // MT_SCEPTRE
    {
        "BETA_SCEPTRE",               // name
        2016,                         // doomednum
        kS_BON3,                      // spawnstate
        1000,                         // spawnhealth
        kS_NULL,                      // seestate
        ksfx_None,                    // seesound
        8,                            // reactiontime
        ksfx_None,                    // attacksound
        kS_NULL,                      // painstate
        0,                            // painchance
        ksfx_None,                    // painsound
        kS_NULL,                      // meleestate
        0,                            // missilestate
        kS_NULL,                      // deathstate
        kS_NULL,                      // xdeathstate
        ksfx_None,                    // deathsound
        0,                            // speed
        10 * kFracUnit,               // radius
        16 * kFracUnit,               // height
        100,                          // mass
        0,                            // damage
        ksfx_None,                    // activesound
        kMF_SPECIAL | kMF_COUNTITEM,  // flags
        0,                            // MBF21 flags
        -2,                           // Infighting group
        -2,                           // Projectile group
        -2,                           // Splash group
        ksfx_None,                    // Rip sound
        0,                            // Fast speed
        0,                            // Melee range
        0,                            // Gib health
        -1,                           // Dropped item
        0,                            // Pickup width
        0,                            // Projectile pass height
        0,                            // Fullbright
        kS_NULL                       // raisestate
    },

    // MT_BIBLE
    {
        "BETA_BIBLE",                 // name
        2017,                         // doomednum
        kS_BON4,                      // spawnstate
        1000,                         // spawnhealth
        kS_NULL,                      // seestate
        ksfx_None,                    // seesound
        8,                            // reactiontime
        ksfx_None,                    // attacksound
        kS_NULL,                      // painstate
        0,                            // painchance
        ksfx_None,                    // painsound
        kS_NULL,                      // meleestate
        0,                            // missilestate
        kS_NULL,                      // deathstate
        kS_NULL,                      // xdeathstate
        ksfx_None,                    // deathsound
        0,                            // speed
        20 * kFracUnit,               // radius
        10 * kFracUnit,               // height
        100,                          // mass
        0,                            // damage
        ksfx_None,                    // activesound
        kMF_SPECIAL | kMF_COUNTITEM,  // flags
        0,                            // MBF21 flags
        -2,                           // Infighting group
        -2,                           // Projectile group
        -2,                           // Splash group
        ksfx_None,                    // Rip sound
        0,                            // Fast speed
        0,                            // Melee range
        0,                            // Gib health
        -1,                           // Dropped item
        0,                            // Pickup width
        0,                            // Projectile pass height
        0,                            // Fullbright
        kS_NULL                       // raisestate
    },

    // MT_MUSICSOURCE
    {
        "MUSIC_SOURCE",  // name
        14164,           // doomednum ....not sure what to put here yet - Dasho
        kS_TNT1,         // spawnstate
        1000,            // spawnhealth
        kS_NULL,         // seestate
        ksfx_None,       // seesound
        8,               // reactiontime
        ksfx_None,       // attacksound
        kS_NULL,         // painstate
        0,               // painchance
        ksfx_None,       // painsound
        kS_NULL,         // meleestate
        0,               // missilestate
        kS_NULL,         // deathstate
        kS_NULL,         // xdeathstate
        ksfx_None,       // deathsound
        0,               // speed
        16,              // radius
        16,              // height
        100,             // mass
        0,               // damage
        ksfx_None,       // activesound
        kMF_NOBLOCKMAP,  // flags
        0,               // MBF21 flags
        -2,              // Infighting group
        -2,              // Projectile group
        -2,              // Splash group
        ksfx_None,       // Rip sound
        0,               // Fast speed
        0,               // Melee range
        0,               // Gib health
        -1,              // Dropped item
        0,               // Pickup width
        0,               // Projectile pass height
        0,               // Fullbright
        kS_NULL          // raisestate
    },

    // MT_GIBDTH
    {
        "GIB_DEATH",                   // name
        -1,                            // doomednum
        kS_TNT1,                       // spawnstate
        1000,                          // spawnhealth
        kS_NULL,                       // seestate
        ksfx_None,                     // seesound
        8,                             // reactiontime
        ksfx_None,                     // attacksound
        kS_NULL,                       // painstate
        0,                             // painchance
        ksfx_None,                     // painsound
        kS_NULL,                       // meleestate
        0,                             // missilestate
        kS_NULL,                       // deathstate
        kS_NULL,                       // xdeathstate
        ksfx_None,                     // deathsound
        0,                             // speed
        4 * kFracUnit,                 // radius
        4 * kFracUnit,                 // height
        100,                           // mass
        0,                             // damage
        ksfx_None,                     // activesound
        kMF_NOBLOCKMAP | kMF_DROPOFF,  // flags
        0,                             // MBF21 flags
        -2,                            // Infighting group
        -2,                            // Projectile group
        -2,                            // Splash group
        ksfx_None,                     // Rip sound
        0,                             // Fast speed
        0,                             // Melee range
        0,                             // Gib health
        -1,                            // Dropped item
        0,                             // Pickup width
        0,                             // Projectile pass height
        0,                             // Fullbright
        kS_NULL                        // raisestate
    },
};

DehackedMapObjectDefinition brain_explode_mobj = {
    "BRAIN_DEATH_MISSILE",                                       // name
    -1,                                                          // doomednum
    kS_BRAINEXPLODE1,                                            // spawnstate
    1000,                                                        // spawnhealth
    kS_NULL,                                                     // seestate
    ksfx_rlaunc,                                                 // seesound
    8,                                                           // reactiontime
    ksfx_None,                                                   // attacksound
    kS_NULL,                                                     // painstate
    0,                                                           // painchance
    ksfx_None,                                                   // painsound
    kS_NULL,                                                     // meleestate
    kS_NULL,                                                     // missilestate
    kS_NULL,                                                     // deathstate
    kS_NULL,                                                     // xdeathstate
    ksfx_barexp,                                                 // deathsound
    20 * kFracUnit,                                              // speed
    11 * kFracUnit,                                              // radius
    8 * kFracUnit,                                               // height
    100,                                                         // mass
    128,                                                         // damage
    ksfx_None,                                                   // activesound
    kMF_NOBLOCKMAP | kMF_MISSILE | kMF_DROPOFF | kMF_NOGRAVITY,  // flags
    0,                                                           // MBF21 flags
    -2,         // Infighting group
    -2,         // Projectile group
    -2,         // Splash group
    ksfx_None,  // Rip sound
    0,          // Fast speed
    0,          // Melee range
    0,          // Gib health
    -1,         // Dropped item
    0,          // Pickup width
    0,          // Projectile pass height
    0,          // Fullbright
    kS_NULL     // raisestate
};

}  // namespace dehacked
