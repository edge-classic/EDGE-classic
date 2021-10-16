//------------------------------------------------------------------------
//  TEXT Strings
//------------------------------------------------------------------------
//
//  DEH_EDGE  Copyright (C) 2004-2005  The EDGE Team
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
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

#include "i_defs.h"
#include "text.h"

#include "buffer.h"
#include "dh_plugin.h"
#include "info.h"
#include "english.h"
#include "frames.h"
#include "mobj.h"
#include "patch.h"
#include "storage.h"
#include "sounds.h"
#include "system.h"
#include "util.h"
#include "wad.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

namespace Deh_Edge
{

spritename_t sprnames[NUMSPRITES_BEX] =
{
    {"TROO",NULL}, {"SHTG",NULL}, {"PUNG",NULL}, {"PISG",NULL},
	{"PISF",NULL}, {"SHTF",NULL}, {"SHT2",NULL}, {"CHGG",NULL},
	{"CHGF",NULL}, {"MISG",NULL}, {"MISF",NULL}, {"SAWG",NULL},
	{"PLSG",NULL}, {"PLSF",NULL}, {"BFGG",NULL}, {"BFGF",NULL},
	{"BLUD",NULL}, {"PUFF",NULL}, {"BAL1",NULL}, {"BAL2",NULL},
	{"PLSS",NULL}, {"PLSE",NULL}, {"MISL",NULL}, {"BFS1",NULL},
	{"BFE1",NULL}, {"BFE2",NULL}, {"TFOG",NULL}, {"IFOG",NULL},
	{"PLAY",NULL}, {"POSS",NULL}, {"SPOS",NULL}, {"VILE",NULL},
	{"FIRE",NULL}, {"FATB",NULL}, {"FBXP",NULL}, {"SKEL",NULL},
	{"MANF",NULL}, {"FATT",NULL}, {"CPOS",NULL}, {"SARG",NULL},
	{"HEAD",NULL}, {"BAL7",NULL}, {"BOSS",NULL}, {"BOS2",NULL},
	{"SKUL",NULL}, {"SPID",NULL}, {"BSPI",NULL}, {"APLS",NULL},
	{"APBX",NULL}, {"CYBR",NULL}, {"PAIN",NULL}, {"SSWV",NULL},
	{"KEEN",NULL}, {"BBRN",NULL}, {"BOSF",NULL}, {"ARM1",NULL},
	{"ARM2",NULL}, {"BAR1",NULL}, {"BEXP",NULL}, {"FCAN",NULL},
	{"BON1",NULL}, {"BON2",NULL}, {"BKEY",NULL}, {"RKEY",NULL},
	{"YKEY",NULL}, {"BSKU",NULL}, {"RSKU",NULL}, {"YSKU",NULL},
	{"STIM",NULL}, {"MEDI",NULL}, {"SOUL",NULL}, {"PINV",NULL},
	{"PSTR",NULL}, {"PINS",NULL}, {"MEGA",NULL}, {"SUIT",NULL},
	{"PMAP",NULL}, {"PVIS",NULL}, {"CLIP",NULL}, {"AMMO",NULL},
	{"ROCK",NULL}, {"BROK",NULL}, {"CELL",NULL}, {"CELP",NULL},
	{"SHEL",NULL}, {"SBOX",NULL}, {"BPAK",NULL}, {"BFUG",NULL},
	{"MGUN",NULL}, {"CSAW",NULL}, {"LAUN",NULL}, {"PLAS",NULL},
	{"SHOT",NULL}, {"SGN2",NULL}, {"COLU",NULL}, {"SMT2",NULL},
	{"GOR1",NULL}, {"POL2",NULL}, {"POL5",NULL}, {"POL4",NULL},
	{"POL3",NULL}, {"POL1",NULL}, {"POL6",NULL}, {"GOR2",NULL},
	{"GOR3",NULL}, {"GOR4",NULL}, {"GOR5",NULL}, {"SMIT",NULL},
	{"COL1",NULL}, {"COL2",NULL}, {"COL3",NULL}, {"COL4",NULL},
	{"CAND",NULL}, {"CBRA",NULL}, {"COL6",NULL}, {"TRE1",NULL},
	{"TRE2",NULL}, {"ELEC",NULL}, {"CEYE",NULL}, {"FSKU",NULL},
	{"COL5",NULL}, {"TBLU",NULL}, {"TGRN",NULL}, {"TRED",NULL},
	{"SMBT",NULL}, {"SMGT",NULL}, {"SMRT",NULL}, {"HDB1",NULL},
	{"HDB2",NULL}, {"HDB3",NULL}, {"HDB4",NULL}, {"HDB5",NULL},
	{"HDB6",NULL}, {"POB1",NULL}, {"POB2",NULL}, {"BRS1",NULL},
	{"TLMP",NULL}, {"TLP2",NULL},
	// BOOM and MBF:
	{"TNT1",NULL}, {"DOGS",NULL}
};


//------------------------------------------------------------------------

typedef struct
{
	const char *orig_text;
	const char *ldf_name;
	const char *deh_name;  // also BEX name

	int v166_index;  // starts at 1, or -1 for no match

	// holds modified version (NULL means not modified).  Guaranteed to
	// have space for an additional four (4) characters.
	char *new_text;
}
langinfo_t;

langinfo_t lang_list[] =
{
    { AMSTR_FOLLOWOFF, "AutoMapFollowOff", "AMSTR_FOLLOWOFF", 409, NULL },
    { AMSTR_FOLLOWON, "AutoMapFollowOn", "AMSTR_FOLLOWON", 408, NULL },
    { AMSTR_GRIDOFF, "AutoMapGridOff", "AMSTR_GRIDOFF", 411, NULL },
    { AMSTR_GRIDON, "AutoMapGridOn", "AMSTR_GRIDON", 410, NULL },
    { AMSTR_MARKEDSPOT, "AutoMapMarkedSpot", "AMSTR_MARKEDSPOT", 412, NULL },
    { AMSTR_MARKSCLEARED, "AutoMapMarksClear", "AMSTR_MARKSCLEARED", 414, NULL },
    { D_DEVSTR, "DevelopmentMode", "D_DEVSTR", 197, NULL },
    { DOSY, "PressToQuit", "DOSY", -1, NULL },
    { EMPTYSTRING, "EmptySlot", "EMPTYSTRING", 300, NULL },
    { ENDGAME, "EndGameCheck", "ENDGAME", 328, NULL },
    { GAMMALVL0, "GammaOff", "GAMMALVL0", -1, NULL },
    { GAMMALVL1, "GammaLevelOne", "GAMMALVL1", -1, NULL },
    { GAMMALVL2, "GammaLevelTwo", "GAMMALVL2", -1, NULL },
    { GAMMALVL3, "GammaLevelThree", "GAMMALVL3", -1, NULL },
    { GAMMALVL4, "GammaLevelFour", "GAMMALVL4", -1, NULL },
    { GGSAVED, "GameSaved", "GGSAVED", 285, NULL },
    { GOTARMBONUS, "GotArmourHelmet", "GOTARMBONUS", 428, NULL },
    { GOTARMOR, "GotArmour", "GOTARMOR", 425, NULL },
    { GOTBACKPACK, "GotBackpack", "GOTBACKPACK", 454, NULL },
    { GOTBERSERK, "GotBerserk", "GOTBERSERK", 441, NULL },
    { GOTBFG9000, "GotBFG", "GOTBFG9000", 455, NULL },
    { GOTBLUECARD, "GotBlueCard", "GOTBLUECARD", 431, NULL },
    { GOTBLUESKUL, "GotBlueSkull", "GOTBLUESKUL", 434, NULL },
    { GOTCELLBOX, "GotCellPack", "GOTCELLBOX", 451, NULL },
    { GOTCELL, "GotCell", "GOTCELL", 450, NULL },
    { GOTCHAINGUN, "GotChainGun", "GOTCHAINGUN", 456, NULL },
    { GOTCHAINSAW, "GotChainSaw", "GOTCHAINSAW", 457, NULL },
    { GOTCLIPBOX, "GotClipBox", "GOTCLIPBOX", 447, NULL },
    { GOTCLIP, "GotClip", "GOTCLIP", 446, NULL },
    { GOTHTHBONUS, "GotHealthPotion", "GOTHTHBONUS", 427, NULL },
    { GOTINVIS, "GotInvis", "GOTINVIS", 442, NULL },
    { GOTINVUL, "GotInvulner", "GOTINVUL", 440, NULL },
    { GOTLAUNCHER, "GotRocketLauncher", "GOTLAUNCHER", 458, NULL },
    { GOTMAP, "GotMap", "GOTMAP", 444, NULL },
    { GOTMEDIKIT, "GotMedi", "GOTMEDIKIT", 439, NULL },
    { GOTMEDINEED, "GotMediNeed", "GOTMEDINEED", 438, NULL },  // not supported by EDGE
    { GOTMEGA, "GotMegaArmour", "GOTMEGA", 426, NULL },
    { GOTMSPHERE, "GotMega", "GOTMSPHERE", 430, NULL },
    { GOTPLASMA, "GotPlasmaGun", "GOTPLASMA", 459, NULL },
    { GOTREDCARD, "GotRedCard", "GOTREDCARD", 433, NULL },
    { GOTREDSKULL, "GotRedSkull", "GOTREDSKULL", 436, NULL },
    { GOTROCKBOX, "GotRocketBox", "GOTROCKBOX", 449, NULL },
    { GOTROCKET, "GotRocket", "GOTROCKET", 448, NULL },
    { GOTSHELLBOX, "GotShellBox", "GOTSHELLBOX", 453, NULL },
    { GOTSHELLS, "GotShells", "GOTSHELLS", 452, NULL },
    { GOTSHOTGUN2, "GotDoubleBarrel", "GOTSHOTGUN2", 461, NULL },
    { GOTSHOTGUN, "GotShotgun", "GOTSHOTGUN", 460, NULL },
    { GOTSTIM, "GotStim", "GOTSTIM", 437, NULL },
    { GOTSUIT, "GotSuit", "GOTSUIT", 443, NULL },
    { GOTSUPER, "GotSoul", "GOTSUPER", 429, NULL },
    { GOTVISOR, "GotVisor", "GOTVISOR", 445, NULL },
    { GOTYELWCARD, "GotYellowCard", "GOTYELWCARD", 432, NULL },
    { GOTYELWSKUL, "GotYellowSkull", "GOTYELWSKUL", 435, NULL },
    { HUSTR_CHATMACRO0, "DefaultCHATMACRO0", "HUSTR_CHATMACRO0", 374, NULL },
    { HUSTR_CHATMACRO1, "DefaultCHATMACRO1", "HUSTR_CHATMACRO1", 376, NULL },
    { HUSTR_CHATMACRO2, "DefaultCHATMACRO2", "HUSTR_CHATMACRO2", 378, NULL },
    { HUSTR_CHATMACRO3, "DefaultCHATMACRO3", "HUSTR_CHATMACRO3", 380, NULL },
    { HUSTR_CHATMACRO4, "DefaultCHATMACRO4", "HUSTR_CHATMACRO4", 382, NULL },
    { HUSTR_CHATMACRO5, "DefaultCHATMACRO5", "HUSTR_CHATMACRO5", 384, NULL },
    { HUSTR_CHATMACRO6, "DefaultCHATMACRO6", "HUSTR_CHATMACRO6", 386, NULL },
    { HUSTR_CHATMACRO7, "DefaultCHATMACRO7", "HUSTR_CHATMACRO7", 388, NULL },
    { HUSTR_CHATMACRO8, "DefaultCHATMACRO8", "HUSTR_CHATMACRO8", 390, NULL },
    { HUSTR_CHATMACRO9, "DefaultCHATMACRO9", "HUSTR_CHATMACRO9", 392, NULL },
    { HUSTR_MESSAGESENT, "Sent", "HUSTR_MESSAGESENT", -1, NULL },
    { HUSTR_MSGU, "UnsentMsg", "HUSTR_MSGU", 686, NULL },
    { HUSTR_PLRBROWN, "Player3Name", "HUSTR_PLRBROWN", 623, NULL },
    { HUSTR_PLRGREEN, "Player1Name", "HUSTR_PLRGREEN", 621, NULL },
    { HUSTR_PLRINDIGO, "Player2Name", "HUSTR_PLRINDIGO", 622, NULL },
    { HUSTR_PLRRED, "Player4Name", "HUSTR_PLRRED", 624, NULL },
    { HUSTR_TALKTOSELF1, "TALKTOSELF1", "HUSTR_TALKTOSELF1", 687, NULL },
    { HUSTR_TALKTOSELF2, "TALKTOSELF2", "HUSTR_TALKTOSELF2", 688, NULL },
    { HUSTR_TALKTOSELF3, "TALKTOSELF3", "HUSTR_TALKTOSELF3", 689, NULL },
    { HUSTR_TALKTOSELF4, "TALKTOSELF4", "HUSTR_TALKTOSELF4", 690, NULL },
    { HUSTR_TALKTOSELF5, "TALKTOSELF5", "HUSTR_TALKTOSELF5", 691, NULL },
    { LOADNET, "NoLoadInNetGame", "LOADNET", 305, NULL },
    { MSGOFF, "MessagesOff", "MSGOFF", 325, NULL },
    { MSGON, "MessagesOn", "MSGON", 326, NULL },
    { NETEND, "EndNetGame", "NETEND", 327, NULL },
    { NEWGAME, "NewNetGame", "NEWGAME", 320, NULL },
    { NIGHTMARE, "NightmareCheck", "NIGHTMARE", 322, NULL },
    { PD_BLUEK, "NeedBlueForDoor", "PD_BLUEK", 419, NULL },
    { PD_BLUEO, "NeedBlueForObject", "PD_BLUEO", 416, NULL },
    { PD_REDK, "NeedRedForDoor", "PD_REDK", 421, NULL },
    { PD_REDO, "NeedRedForObject", "PD_REDO", 417, NULL },
    { PD_YELLOWK, "NeedYellowForDoor", "PD_YELLOWK", 420, NULL },
    { PD_YELLOWO, "NeedYellowForObject", "PD_YELLOWO", 418, NULL },
    { PRESSKEY, "PressKey", "PRESSKEY", -1, NULL },
    { PRESSYN, "PressYorN", "PRESSYN", -1, NULL },
    { QLOADNET, "NoQLoadInNetGame", "QLOADNET", 310, NULL },
    { QLPROMPT, "QuickLoad", "QLPROMPT", 312, NULL },
    { QSAVESPOT, "NoQuickSaveSlot", "QSAVESPOT", 311, NULL },
    { QSPROMPT, "QuickSaveOver", "QSPROMPT", 309, NULL },
    { SAVEDEAD, "SaveWhenNotPlaying", "SAVEDEAD", 308, NULL },
    { STSTR_BEHOLD, "BEHOLDNote", "STSTR_BEHOLD", 585, NULL },
    { STSTR_BEHOLDX, "BEHOLDUsed", "STSTR_BEHOLDX", 584, NULL },
    { STSTR_CHOPPERS, "ChoppersNote", "STSTR_CHOPPERS", 586, NULL },
    { STSTR_CLEV, "LevelChange", "STSTR_CLEV", 588, NULL },
    { STSTR_DQDOFF, "GodModeOFF", "STSTR_DQDOFF", 578, NULL },
    { STSTR_DQDON, "GodModeON", "STSTR_DQDON", 577, NULL },
    { STSTR_FAADDED, "AmmoAdded", "STSTR_FAADDED", 579, NULL },
    { STSTR_KFAADDED, "VeryHappyAmmo", "STSTR_KFAADDED", 580, NULL },
    { STSTR_MUS, "MusChange", "STSTR_MUS", 581, NULL },
    { STSTR_NCOFF, "ClipOFF", "STSTR_NCOFF", 583, NULL },
    { STSTR_NCON, "ClipON", "STSTR_NCON", 582, NULL },
    { STSTR_NOMUS, "ImpossibleChange", "STSTR_NOMUS", -1, NULL },

	// DOOM I strings
    { HUSTR_E1M1, "E1M1Desc", "HUSTR_E1M1", 625, NULL },
    { HUSTR_E1M2, "E1M2Desc", "HUSTR_E1M2", 626, NULL },
    { HUSTR_E1M3, "E1M3Desc", "HUSTR_E1M3", 627, NULL },
    { HUSTR_E1M4, "E1M4Desc", "HUSTR_E1M4", 628, NULL },
    { HUSTR_E1M5, "E1M5Desc", "HUSTR_E1M5", 629, NULL },
    { HUSTR_E1M6, "E1M6Desc", "HUSTR_E1M6", 630, NULL },
    { HUSTR_E1M7, "E1M7Desc", "HUSTR_E1M7", 631, NULL },
    { HUSTR_E1M8, "E1M8Desc", "HUSTR_E1M8", 632, NULL },
    { HUSTR_E1M9, "E1M9Desc", "HUSTR_E1M9", 633, NULL },
    { HUSTR_E2M1, "E2M1Desc", "HUSTR_E2M1", 634, NULL },
    { HUSTR_E2M2, "E2M2Desc", "HUSTR_E2M2", 635, NULL },
    { HUSTR_E2M3, "E2M3Desc", "HUSTR_E2M3", 636, NULL },
    { HUSTR_E2M4, "E2M4Desc", "HUSTR_E2M4", 637, NULL },
    { HUSTR_E2M5, "E2M5Desc", "HUSTR_E2M5", 638, NULL },
    { HUSTR_E2M6, "E2M6Desc", "HUSTR_E2M6", 639, NULL },
    { HUSTR_E2M7, "E2M7Desc", "HUSTR_E2M7", 640, NULL },
    { HUSTR_E2M8, "E2M8Desc", "HUSTR_E2M8", 641, NULL },
    { HUSTR_E2M9, "E2M9Desc", "HUSTR_E2M9", 642, NULL },
    { HUSTR_E3M1, "E3M1Desc", "HUSTR_E3M1", 643, NULL },
    { HUSTR_E3M2, "E3M2Desc", "HUSTR_E3M2", 644, NULL },
    { HUSTR_E3M3, "E3M3Desc", "HUSTR_E3M3", 645, NULL },
    { HUSTR_E3M4, "E3M4Desc", "HUSTR_E3M4", 646, NULL },
    { HUSTR_E3M5, "E3M5Desc", "HUSTR_E3M5", 647, NULL },
    { HUSTR_E3M6, "E3M6Desc", "HUSTR_E3M6", 648, NULL },
    { HUSTR_E3M7, "E3M7Desc", "HUSTR_E3M7", 649, NULL },
    { HUSTR_E3M8, "E3M8Desc", "HUSTR_E3M8", 650, NULL },
    { HUSTR_E3M9, "E3M9Desc", "HUSTR_E3M9", 651, NULL },
    { HUSTR_E4M1, "E4M1Desc", "HUSTR_E4M1", -1, NULL },
    { HUSTR_E4M2, "E4M2Desc", "HUSTR_E4M2", -1, NULL },
    { HUSTR_E4M3, "E4M3Desc", "HUSTR_E4M3", -1, NULL },
    { HUSTR_E4M4, "E4M4Desc", "HUSTR_E4M4", -1, NULL },
    { HUSTR_E4M5, "E4M5Desc", "HUSTR_E4M5", -1, NULL },
    { HUSTR_E4M6, "E4M6Desc", "HUSTR_E4M6", -1, NULL },
    { HUSTR_E4M7, "E4M7Desc", "HUSTR_E4M7", -1, NULL },
    { HUSTR_E4M8, "E4M8Desc", "HUSTR_E4M8", -1, NULL },
    { HUSTR_E4M9, "E4M9Desc", "HUSTR_E4M9", -1, NULL },
    { E1TEXT, "Episode1Text", "E1TEXT", 111, NULL },
    { E2TEXT, "Episode2Text", "E2TEXT", 112, NULL },
    { E3TEXT, "Episode3Text", "E3TEXT", 113, NULL },
    { E4TEXT, "Episode4Text", "E4TEXT", -1, NULL },

	// DOOM II strings
    { HUSTR_10, "Map10Desc", "HUSTR_10", 662, NULL },
    { HUSTR_11, "Map11Desc", "HUSTR_11", 663, NULL },
    { HUSTR_12, "Map12Desc", "HUSTR_12", 664, NULL },
    { HUSTR_13, "Map13Desc", "HUSTR_13", 665, NULL },
    { HUSTR_14, "Map14Desc", "HUSTR_14", 666, NULL },
    { HUSTR_15, "Map15Desc", "HUSTR_15", 667, NULL },
    { HUSTR_16, "Map16Desc", "HUSTR_16", 668, NULL },
    { HUSTR_17, "Map17Desc", "HUSTR_17", 669, NULL },
    { HUSTR_18, "Map18Desc", "HUSTR_18", 670, NULL },
    { HUSTR_19, "Map19Desc", "HUSTR_19", 671, NULL },
    { HUSTR_1,  "Map01Desc", "HUSTR_1",  653, NULL },
    { HUSTR_20, "Map20Desc", "HUSTR_20", 672, NULL },
    { HUSTR_21, "Map21Desc", "HUSTR_21", 673, NULL },
    { HUSTR_22, "Map22Desc", "HUSTR_22", 674, NULL },
    { HUSTR_23, "Map23Desc", "HUSTR_23", 675, NULL },
    { HUSTR_24, "Map24Desc", "HUSTR_24", 676, NULL },
    { HUSTR_25, "Map25Desc", "HUSTR_25", 677, NULL },
    { HUSTR_26, "Map26Desc", "HUSTR_26", 678, NULL },
    { HUSTR_27, "Map27Desc", "HUSTR_27", 679, NULL },
    { HUSTR_28, "Map28Desc", "HUSTR_28", 680, NULL },
    { HUSTR_29, "Map29Desc", "HUSTR_29", 681, NULL },
    { HUSTR_2,  "Map02Desc", "HUSTR_2", 654, NULL },
    { HUSTR_30, "Map30Desc", "HUSTR_30", 682, NULL },
    { HUSTR_31, "Map31Desc", "HUSTR_31", 683, NULL },
    { HUSTR_32, "Map32Desc", "HUSTR_32", 684, NULL },
    { HUSTR_3,  "Map03Desc", "HUSTR_3", 655, NULL },
    { HUSTR_4,  "Map04Desc", "HUSTR_4", 656, NULL },
    { HUSTR_5,  "Map05Desc", "HUSTR_5", 657, NULL },
    { HUSTR_6,  "Map06Desc", "HUSTR_6", 658, NULL },
    { HUSTR_7,  "Map07Desc", "HUSTR_7", 659, NULL },
    { HUSTR_8,  "Map08Desc", "HUSTR_8", 660, NULL },
    { HUSTR_9,  "Map09Desc", "HUSTR_9", 661, NULL },
    { C1TEXT, "Level7Text",  "C1TEXT", 114, NULL },
    { C2TEXT, "Level12Text", "C2TEXT", 115, NULL },
    { C3TEXT, "Level21Text", "C3TEXT", 116, NULL },
    { C4TEXT, "EndGameText", "C4TEXT", 117, NULL },
    { C5TEXT, "Level31Text", "C5TEXT", 118, NULL },
    { C6TEXT, "Level32Text", "C6TEXT", 119, NULL },

	// TNT strings
    { THUSTR_10, "Tnt10Desc", "THUSTR_10", -1, NULL },
    { THUSTR_11, "Tnt11Desc", "THUSTR_11", -1, NULL },
    { THUSTR_12, "Tnt12Desc", "THUSTR_12", -1, NULL },
    { THUSTR_13, "Tnt13Desc", "THUSTR_13", -1, NULL },
    { THUSTR_14, "Tnt14Desc", "THUSTR_14", -1, NULL },
    { THUSTR_15, "Tnt15Desc", "THUSTR_15", -1, NULL },
    { THUSTR_16, "Tnt16Desc", "THUSTR_16", -1, NULL },
    { THUSTR_17, "Tnt17Desc", "THUSTR_17", -1, NULL },
    { THUSTR_18, "Tnt18Desc", "THUSTR_18", -1, NULL },
    { THUSTR_19, "Tnt19Desc", "THUSTR_19", -1, NULL },
    { THUSTR_1,  "Tnt01Desc", "THUSTR_1",  -1, NULL },
    { THUSTR_20, "Tnt20Desc", "THUSTR_20", -1, NULL },
    { THUSTR_21, "Tnt21Desc", "THUSTR_21", -1, NULL },
    { THUSTR_22, "Tnt22Desc", "THUSTR_22", -1, NULL },
    { THUSTR_23, "Tnt23Desc", "THUSTR_23", -1, NULL },
    { THUSTR_24, "Tnt24Desc", "THUSTR_24", -1, NULL },
    { THUSTR_25, "Tnt25Desc", "THUSTR_25", -1, NULL },
    { THUSTR_26, "Tnt26Desc", "THUSTR_26", -1, NULL },
    { THUSTR_27, "Tnt27Desc", "THUSTR_27", -1, NULL },
    { THUSTR_28, "Tnt28Desc", "THUSTR_28", -1, NULL },
    { THUSTR_29, "Tnt29Desc", "THUSTR_29", -1, NULL },
    { THUSTR_2,  "Tnt02Desc", "THUSTR_2",  -1, NULL },
    { THUSTR_30, "Tnt30Desc", "THUSTR_30", -1, NULL },
    { THUSTR_31, "Tnt31Desc", "THUSTR_31", -1, NULL },
    { THUSTR_32, "Tnt32Desc", "THUSTR_32", -1, NULL },
    { THUSTR_3,  "Tnt03Desc", "THUSTR_3",  -1, NULL },
    { THUSTR_4,  "Tnt04Desc", "THUSTR_4",  -1, NULL },
    { THUSTR_5,  "Tnt05Desc", "THUSTR_5",  -1, NULL },
    { THUSTR_6,  "Tnt06Desc", "THUSTR_6",  -1, NULL },
    { THUSTR_7,  "Tnt07Desc", "THUSTR_7",  -1, NULL },
    { THUSTR_8,  "Tnt08Desc", "THUSTR_8",  -1, NULL },
    { THUSTR_9,  "Tnt09Desc", "THUSTR_9",  -1, NULL },
    { T1TEXT, "TntLevel7Text",  "T1TEXT",  -1, NULL },
    { T2TEXT, "TntLevel12Text", "T2TEXT",  -1, NULL },
    { T3TEXT, "TntLevel21Text", "T3TEXT",  -1, NULL },
    { T4TEXT, "TntEndGameText", "T4TEXT",  -1, NULL },
    { T5TEXT, "TntLevel31Text", "T5TEXT",  -1, NULL },
    { T6TEXT, "TntLevel32Text", "T6TEXT",  -1, NULL },

	// PLUTONIA strings
    { PHUSTR_10, "Plut10Desc", "PHUSTR_10", -1, NULL },
    { PHUSTR_11, "Plut11Desc", "PHUSTR_11", -1, NULL },
    { PHUSTR_12, "Plut12Desc", "PHUSTR_12", -1, NULL },
    { PHUSTR_13, "Plut13Desc", "PHUSTR_13", -1, NULL },
    { PHUSTR_14, "Plut14Desc", "PHUSTR_14", -1, NULL },
    { PHUSTR_15, "Plut15Desc", "PHUSTR_15", -1, NULL },
    { PHUSTR_16, "Plut16Desc", "PHUSTR_16", -1, NULL },
    { PHUSTR_17, "Plut17Desc", "PHUSTR_17", -1, NULL },
    { PHUSTR_18, "Plut18Desc", "PHUSTR_18", -1, NULL },
    { PHUSTR_19, "Plut19Desc", "PHUSTR_19", -1, NULL },
    { PHUSTR_1,  "Plut01Desc", "PHUSTR_1",  -1, NULL },
    { PHUSTR_20, "Plut20Desc", "PHUSTR_20", -1, NULL },
    { PHUSTR_21, "Plut21Desc", "PHUSTR_21", -1, NULL },
    { PHUSTR_22, "Plut22Desc", "PHUSTR_22", -1, NULL },
    { PHUSTR_23, "Plut23Desc", "PHUSTR_23", -1, NULL },
    { PHUSTR_24, "Plut24Desc", "PHUSTR_24", -1, NULL },
    { PHUSTR_25, "Plut25Desc", "PHUSTR_25", -1, NULL },
    { PHUSTR_26, "Plut26Desc", "PHUSTR_26", -1, NULL },
    { PHUSTR_27, "Plut27Desc", "PHUSTR_27", -1, NULL },
    { PHUSTR_28, "Plut28Desc", "PHUSTR_28", -1, NULL },
    { PHUSTR_29, "Plut29Desc", "PHUSTR_29", -1, NULL },
    { PHUSTR_2,  "Plut02Desc", "PHUSTR_2",  -1, NULL },
    { PHUSTR_30, "Plut30Desc", "PHUSTR_30", -1, NULL },
    { PHUSTR_31, "Plut31Desc", "PHUSTR_31", -1, NULL },
    { PHUSTR_32, "Plut32Desc", "PHUSTR_32", -1, NULL },
    { PHUSTR_3,  "Plut03Desc", "PHUSTR_3",  -1, NULL },
    { PHUSTR_4,  "Plut04Desc", "PHUSTR_4",  -1, NULL },
    { PHUSTR_5,  "Plut05Desc", "PHUSTR_5",  -1, NULL },
    { PHUSTR_6,  "Plut06Desc", "PHUSTR_6",  -1, NULL },
    { PHUSTR_7,  "Plut07Desc", "PHUSTR_7",  -1, NULL },
    { PHUSTR_8,  "Plut08Desc", "PHUSTR_8",  -1, NULL },
    { PHUSTR_9,  "Plut09Desc", "PHUSTR_9",  -1, NULL },
    { P1TEXT, "PlutLevel7Text",  "P1TEXT",  -1, NULL },
    { P2TEXT, "PlutLevel12Text", "P2TEXT",  -1, NULL },
    { P3TEXT, "PlutLevel21Text", "P3TEXT",  -1, NULL },
    { P4TEXT, "PlutEndGameText", "P4TEXT",  -1, NULL },
    { P5TEXT, "PlutLevel31Text", "P5TEXT",  -1, NULL },
    { P6TEXT, "PlutLevel32Text", "P6TEXT",  -1, NULL },

	// Extra strings (not found in LANGUAGE.LDF)
    { X_COMMERC,  "Commercial", "X_COMMERC",  233, NULL },
    { X_REGIST,   "Registered", "X_REGIST",   230, NULL },
    { X_TITLE1,   "Title1",     "X_TITLE1",    -1, NULL },
    { X_TITLE2,   "Title2",     "X_TITLE2",   194, NULL },
    { X_TITLE3,   "Title3",     "X_TITLE3",   195, NULL },
    { X_MODIFIED, "Notice",     "X_MODIFIED", 229, NULL },
    { X_NODIST1,  "Notice",     "X_NODIST1",  231, NULL },
    { X_NODIST2,  "Notice",     "X_NODIST2",  234, NULL },
	{ D_CDROM,    "CDRom",      "D_CDROM",    199, NULL },
	{ DETAILHI,   "DetailHigh", "DETAILHI",   330, NULL },
	{ DETAILLO,   "DetailLow",  "DETAILLO",   331, NULL },
	{ QUITMSG,    "QuitMsg",    "QUITMSG",     -1, NULL },
	{ SWSTRING,   "Shareware",  "SWSTRING",   323, NULL },

	// Monster cast names...
    { CC_ZOMBIE, "ZombiemanName",         "CC_ZOMBIE",  129, NULL },
    { CC_SHOTGUN,"ShotgunGuyName",        "CC_SHOTGUN", 130, NULL },
    { CC_HEAVY,  "HeavyWeaponDudeName",   "CC_HEAVY",   131, NULL },
    { CC_IMP,    "ImpName",               "CC_IMP",     132, NULL },
    { CC_DEMON,  "DemonName",             "CC_DEMON",   133, NULL },
	{ CC_LOST,   "LostSoulName",          "CC_LOST",    134, NULL },
    { CC_CACO,   "CacodemonName",         "CC_CACO",    135, NULL },
    { CC_HELL,   "HellKnightName",        "CC_HELL",    136, NULL },
    { CC_BARON,  "BaronOfHellName",       "CC_BARON",   137, NULL },
    { CC_ARACH,  "ArachnotronName",       "CC_ARACH",   138, NULL },
    { CC_PAIN,   "PainElementalName",     "CC_PAIN",    139, NULL },
    { CC_REVEN,  "RevenantName",          "CC_REVEN",   140, NULL },
    { CC_MANCU,  "MancubusName",          "CC_MANCU",   141, NULL },
    { CC_ARCH,   "ArchVileName",          "CC_ARCH",    142, NULL },
    { CC_SPIDER, "SpiderMastermindName",  "CC_SPIDER",  143, NULL },
    { CC_CYBER,  "CyberdemonName",        "CC_CYBER",   144, NULL },
	{ CC_HERO,   "OurHeroName",           "CC_HERO",    145, NULL },

	{ NULL, NULL, NULL, -1, NULL }  // End sentinel
};

langinfo_t cheat_list[] =
{
	{ "idbehold",   "idbehold9",  "BEHOLD menu",  -1, NULL },
	{ "idbeholda",  "idbehold5",  "Auto-map",     -1, NULL },
	{ "idbeholdi",  "idbehold3",  "Invisibility", -1, NULL },
	{ "idbeholdl",  "idbehold6",  "Lite-Amp Goggles", -1, NULL },
	{ "idbeholdr",  "idbehold4",  "Radiation Suit",   -1, NULL },
	{ "idbeholds",  "idbehold2",  "Berserk", -1, NULL },
	{ "idbeholdv",  "idbehold1",  "Invincibility", -1, NULL },
	{ "idchoppers", "idchoppers", "Chainsaw",   -1, NULL },
	{ "idclev",     "idclev",     "Level Warp", -1, NULL },
	{ "idclip",     "idclip",     "No Clipping 2", -1, NULL },
	{ "iddqd",      "iddqd",      "God mode",  -1, NULL },
	{ "iddt",       "iddt",       "Map cheat", -1, NULL },
	{ "idfa",       "idfa",       "Ammo", -1, NULL },
	{ "idkfa",      "idkfa",      "Ammo & Keys",  -1, NULL },
	{ "idmus",      "idmus",      "Change music", -1, NULL },
	{ "idmypos",    "idmypos",    "Player Position", -1, NULL },
	{ "idspispopd", "idspispopd", "No Clipping 1",   -1, NULL },

	{ NULL, NULL, NULL, -1, NULL }  // End sentinel
};

const char *lang_bex_unsupported[] =
{
	"BGCASTCALL", "BGFLAT06", "BGFLAT11", "BGFLAT15",
	"BGFLAT20", "BGFLAT30", "BGFLAT31",
	"BGFLATE1", "BGFLATE2", "BGFLATE3", "BGFLATE4",
	"PD_ALL3", "PD_ALL6", "PD_ANY",
	"PD_BLUEC", "PD_REDC", "PD_YELLOWC",
	"PD_BLUES", "PD_REDS", "PD_YELLOWS",
	"RESTARTLEVEL", "SAVEGAMENAME",
	"STARTUP1", "STARTUP2", "STARTUP3", "STARTUP4", "STARTUP5",
	"STSTR_COMPOFF", "STSTR_COMPON",

	NULL
};


//------------------------------------------------------------------------

void TextStr::Startup(void)
{
	for (int s = 0; s < NUMSPRITES_BEX; s++)
	{
		free(sprnames[s].new_name);
		sprnames[s].new_name = NULL;
	}

	for (int i = 0; lang_list[i].orig_text; i++)
	{
		free(lang_list[i].new_text);
		lang_list[i].new_text = NULL;
	}

	for (int c = 0; cheat_list[c].orig_text; c++)
	{
		free(cheat_list[c].new_text);
		cheat_list[c].new_text = NULL;
	}
}

namespace TextStr
{
	void SpriteDependencies(void)
	{
		for (int i = 0; i < NUMSPRITES_BEX; i++)
		{
			if (! sprnames[i].new_name)
				continue;

			// find this sprite amongst the states...
			for (int st = 1; st < NUMSTATES_BEX; st++)
				if (states[st].sprite == i)
					Frames::MarkState(st);
		}
	}
}

bool TextStr::ReplaceSprite(const char *before, const char *after)
{
	assert(strlen(before) == 4);
	assert(strlen(after)  == 4);

	for (int i = 0; i < NUMSPRITES_BEX; i++)
	{
		spritename_t *spr = sprnames + i;

		if (StrCaseCmp(before, spr->orig_name) != 0)
			continue;

		if (! spr->new_name)
			spr->new_name = StringNew(5);

		strcpy(spr->new_name, after);

		return true;
	}

	return false;
}

void TextStr::AlterBexSprite(const char *new_val)
{
	const char *old_val = Patch::line_buf;

	if (strlen(old_val) != 4)
	{
		PrintWarn("Bad length for sprite name '%s'.\n", old_val);
		return;
	}

	if (strlen(new_val) != 4)
	{
		PrintWarn("Bad length for sprite name '%s'.\n", new_val);
		return;
	}

	if (! ReplaceSprite(old_val, new_val))
		PrintWarn("Line %d: unknown sprite name '%s'.\n",
			Patch::line_num, old_val);
}

bool TextStr::ReplaceString(const char *before, const char *after)
{
	assert(after[0]);

	for (int i = 0; lang_list[i].orig_text; i++)
	{
		langinfo_t *lang = lang_list + i;

		if (StrCaseCmp(before, lang->orig_text) != 0)
			continue;

		int len = strlen(lang->orig_text);

		if (! lang->new_text)
			lang->new_text = StringNew(len + 5);

		StrMaxCopy(lang->new_text, after, len + 4);

		return true;
	}

	return false;
}

bool TextStr::ReplaceBexString(const char *bex_name, const char *after)
{
	assert(after[0]);

	for (int i = 0; lang_list[i].orig_text; i++)
	{
		langinfo_t *lang = lang_list + i;

		if (StrCaseCmp(bex_name, lang->deh_name) != 0)
			continue;

		if (lang->new_text)
			free(lang->new_text);

		lang->new_text = StringDup(after);

		return true;
	}

	return false;
}

void TextStr::ReplaceBinaryString(int v166_index, const char *str)
{
	assert(str[0]);

	for (int i = 0; lang_list[i].orig_text; i++)
	{
		langinfo_t *lang = lang_list + i;

		if (lang->v166_index != v166_index)
			continue;

		// OK, found it, so check if it has changed

		if (StrCaseCmp(str, lang->orig_text) != 0)
		{
			int len = strlen(lang->orig_text);

			if (! lang->new_text)
				lang->new_text = StringNew(len + 5);

			StrMaxCopy(lang->new_text, str, len + 4);
		}

		return;
	}
}

bool TextStr::ReplaceCheat(const char *deh_name, const char *str)
{
	assert(str[0]);

	// DOOM cheats were terminated with an 0xff byte
	char eoln = (char) 0xff;

	for (int i = 0; cheat_list[i].orig_text; i++)
	{
		langinfo_t *cht = cheat_list + i;

		if (StrCaseCmp(deh_name, cht->deh_name) != 0)
			continue;

		int len = strlen(cht->orig_text);

		const char *end_mark = strchr(str, eoln);

		if (end_mark)
		{
			int end_pos = end_mark - str;

			if (end_pos > 1 && end_pos < len)
				len = end_pos;
		}

		if (! cht->new_text)
			cht->new_text = StringNew(len + 1);

		StrMaxCopy(cht->new_text, str, len);

		return true;
	}

	return false;
}

void TextStr::AlterCheat(const char * new_val)
{
	const char *deh_field = Patch::line_buf;

	if (! ReplaceCheat(deh_field, new_val))
	{
		PrintWarn("UNKNOWN CHEAT FIELD: %s\n", deh_field);
	}
}

const char *TextStr::GetSprite(int spr_num)
{
	assert(0 <= spr_num && spr_num < NUMSPRITES_BEX);

	const spritename_t *spr = sprnames + spr_num;

	const char *name = spr->new_name ? spr->new_name : spr->orig_name;

	// Boom support: TNT1 is an invisible sprite
	if (StrCaseCmp(name, "TNT1") == 0)
		return "NULL";
	
	return name;
}


//------------------------------------------------------------------------

namespace TextStr
{
	bool got_one;

	void BeginTextLump(void)
	{
		WAD::NewLump("DDFLANG");

		WAD::Printf(GEN_BY_COMMENT);
		WAD::Printf("<LANGUAGES>\n\n");
		WAD::Printf("[ENGLISH]\n");
	}

	void FinishTextLump(void)
	{
		WAD::Printf("\n");
		WAD::FinishLump();
	}

	void WriteTextString(const langinfo_t *info)
	{
		if (! got_one)
		{
			got_one = true;
			BeginTextLump();
		}

		WAD::Printf("%s = \"", info->ldf_name);

		const char *str = info->new_text ? info->new_text : info->orig_text;

		for (; *str; str++)
		{
			if (*str == '\n')
			{
				WAD::Printf("\\n\"\n  \"");
				continue;
			}

			if (*str == '"')
			{
				WAD::Printf("\\\"");
				continue;
			}

			// XXX may need special handling for non-english chars
			WAD::Printf("%c", *str);
		}

		WAD::Printf("\";\n");
	}
}

void TextStr::ConvertLDF(void)
{
	got_one = false;

	for (int i = 0; lang_list[i].orig_text; i++)
	{
	    if (! all_mode && ! lang_list[i].new_text)
			continue;

		WriteTextString(lang_list + i);
	}

	// --- cheats ---

	if (got_one)
		WAD::Printf("\n");

	for (int i = 0; cheat_list[i].orig_text; i++)
	{
	    if (! all_mode && ! cheat_list[i].new_text)
			continue;

		WriteTextString(cheat_list + i);
	}

	if (got_one)
		FinishTextLump();
}

}  // Deh_Edge
