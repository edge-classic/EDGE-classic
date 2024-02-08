//------------------------------------------------------------------------
//  TEXT Strings
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

#include <stdlib.h>
#include <string.h>

#include "deh_edge.h"

#include "deh_buffer.h"
#include "deh_info.h"
#include "deh_english.h"
#include "deh_frames.h"
#include "deh_patch.h"
#include "deh_text.h"
#include "deh_sounds.h"
#include "deh_system.h"
#include "deh_util.h"
#include "deh_wad.h"

namespace dehacked
{

struct LanguageInfo
{
    const char *orig_text;
    const char *ldf_name;
    const char *deh_name; // also BEX name

    int v166_index; // starts at 1, or -1 for no match

    // holds modified version (NULL means not modified).  Guaranteed to
    // have space for an additional four (4) characters.
    char *new_text;
};

LanguageInfo lang_list[] = {
    {kAMSTR_FOLLOWOFF, "AutoMapFollowOff", "AMSTR_FOLLOWOFF", 409, NULL},
    {kAMSTR_FOLLOWON, "AutoMapFollowOn", "AMSTR_FOLLOWON", 408, NULL},
    {kAMSTR_GRIDOFF, "AutoMapGridOff", "AMSTR_GRIDOFF", 411, NULL},
    {kAMSTR_GRIDON, "AutoMapGridOn", "AMSTR_GRIDON", 410, NULL},
    {kAMSTR_MARKEDSPOT, "AutoMapMarkedSpot", "AMSTR_MARKEDSPOT", 412, NULL},
    {kAMSTR_MARKSCLEARED, "AutoMapMarksClear", "AMSTR_MARKSCLEARED", 414, NULL},
    {kD_DEVSTR, "DevelopmentMode", "D_DEVSTR", 197, NULL},
    {kDOSY, "PressToQuit", "DOSY", -1, NULL},
    {kEMPTYSTRING, "EmptySlot", "EMPTYSTRING", 300, NULL},
    {kENDGAME, "EndGameCheck", "ENDGAME", 328, NULL},
    {kGAMMALVL0, "GammaOff", "GAMMALVL0", -1, NULL},
    {kGAMMALVL1, "GammaLevelOne", "GAMMALVL1", -1, NULL},
    {kGAMMALVL2, "GammaLevelTwo", "GAMMALVL2", -1, NULL},
    {kGAMMALVL3, "GammaLevelThree", "GAMMALVL3", -1, NULL},
    {kGAMMALVL4, "GammaLevelFour", "GAMMALVL4", -1, NULL},
    {kGGSAVED, "GameSaved", "GGSAVED", 285, NULL},
    {kGOTARMBONUS, "GotArmourHelmet", "GOTARMBONUS", 428, NULL},
    {kGOTARMOR, "GotArmour", "GOTARMOR", 425, NULL},
    {kGOTBACKPACK, "GotBackpack", "GOTBACKPACK", 454, NULL},
    {kGOTBERSERK, "GotBerserk", "GOTBERSERK", 441, NULL},
    {kGOTBFG9000, "GotBFG", "GOTBFG9000", 455, NULL},
    {kGOTBLUECARD, "GotBlueCard", "GOTBLUECARD", 431, NULL},
    {kGOTBLUESKUL, "GotBlueSkull", "GOTBLUESKUL", 434, NULL},
    {kGOTCELLBOX, "GotCellPack", "GOTCELLBOX", 451, NULL},
    {kGOTCELL, "GotCell", "GOTCELL", 450, NULL},
    {kGOTCHAINGUN, "GotChainGun", "GOTCHAINGUN", 456, NULL},
    {kGOTCHAINSAW, "GotChainSaw", "GOTCHAINSAW", 457, NULL},
    {kGOTCLIPBOX, "GotClipBox", "GOTCLIPBOX", 447, NULL},
    {kGOTCLIP, "GotClip", "GOTCLIP", 446, NULL},
    {kGOTHTHBONUS, "GotHealthPotion", "GOTHTHBONUS", 427, NULL},
    {kGOTINVIS, "GotInvis", "GOTINVIS", 442, NULL},
    {kGOTINVUL, "GotInvulner", "GOTINVUL", 440, NULL},
    {kGOTLAUNCHER, "GotRocketLauncher", "GOTLAUNCHER", 458, NULL},
    {kGOTMAP, "GotMap", "GOTMAP", 444, NULL},
    {kGOTMEDIKIT, "GotMedi", "GOTMEDIKIT", 439, NULL},
    {kGOTMEDINEED, "GotMediNeed", "GOTMEDINEED", 438, NULL}, // not supported by EDGE
    {kGOTMEGA, "GotMegaArmour", "GOTMEGA", 426, NULL},
    {kGOTMSPHERE, "GotMega", "GOTMSPHERE", 430, NULL},
    {kGOTPLASMA, "GotPlasmaGun", "GOTPLASMA", 459, NULL},
    {kGOTREDCARD, "GotRedCard", "GOTREDCARD", 433, NULL},
    {kGOTREDSKULL, "GotRedSkull", "GOTREDSKULL", 436, NULL},
    {kGOTROCKBOX, "GotRocketBox", "GOTROCKBOX", 449, NULL},
    {kGOTROCKET, "GotRocket", "GOTROCKET", 448, NULL},
    {kGOTSHELLBOX, "GotShellBox", "GOTSHELLBOX", 453, NULL},
    {kGOTSHELLS, "GotShells", "GOTSHELLS", 452, NULL},
    {kGOTSHOTGUN2, "GotDoubleBarrel", "GOTSHOTGUN2", 461, NULL},
    {kGOTSHOTGUN, "GotShotgun", "GOTSHOTGUN", 460, NULL},
    {kGOTSTIM, "GotStim", "GOTSTIM", 437, NULL},
    {kGOTSUIT, "GotSuit", "GOTSUIT", 443, NULL},
    {kGOTSUPER, "GotSoul", "GOTSUPER", 429, NULL},
    {kGOTVISOR, "GotVisor", "GOTVISOR", 445, NULL},
    {kGOTYELWCARD, "GotYellowCard", "GOTYELWCARD", 432, NULL},
    {kGOTYELWSKUL, "GotYellowSkull", "GOTYELWSKUL", 435, NULL},
    {kHUSTR_CHATMACRO0, "DefaultCHATMACRO0", "HUSTR_CHATMACRO0", 374, NULL},
    {kHUSTR_CHATMACRO1, "DefaultCHATMACRO1", "HUSTR_CHATMACRO1", 376, NULL},
    {kHUSTR_CHATMACRO2, "DefaultCHATMACRO2", "HUSTR_CHATMACRO2", 378, NULL},
    {kHUSTR_CHATMACRO3, "DefaultCHATMACRO3", "HUSTR_CHATMACRO3", 380, NULL},
    {kHUSTR_CHATMACRO4, "DefaultCHATMACRO4", "HUSTR_CHATMACRO4", 382, NULL},
    {kHUSTR_CHATMACRO5, "DefaultCHATMACRO5", "HUSTR_CHATMACRO5", 384, NULL},
    {kHUSTR_CHATMACRO6, "DefaultCHATMACRO6", "HUSTR_CHATMACRO6", 386, NULL},
    {kHUSTR_CHATMACRO7, "DefaultCHATMACRO7", "HUSTR_CHATMACRO7", 388, NULL},
    {kHUSTR_CHATMACRO8, "DefaultCHATMACRO8", "HUSTR_CHATMACRO8", 390, NULL},
    {kHUSTR_CHATMACRO9, "DefaultCHATMACRO9", "HUSTR_CHATMACRO9", 392, NULL},
    {kHUSTR_MESSAGESENT, "Sent", "HUSTR_MESSAGESENT", -1, NULL},
    {kHUSTR_MSGU, "UnsentMsg", "HUSTR_MSGU", 686, NULL},
    {kHUSTR_PLRBROWN, "Player3Name", "HUSTR_PLRBROWN", 623, NULL},
    {kHUSTR_PLRGREEN, "Player1Name", "HUSTR_PLRGREEN", 621, NULL},
    {kHUSTR_PLRINDIGO, "Player2Name", "HUSTR_PLRINDIGO", 622, NULL},
    {kHUSTR_PLRRED, "Player4Name", "HUSTR_PLRRED", 624, NULL},
    {kHUSTR_TALKTOSELF1, "TALKTOSELF1", "HUSTR_TALKTOSELF1", 687, NULL},
    {kHUSTR_TALKTOSELF2, "TALKTOSELF2", "HUSTR_TALKTOSELF2", 688, NULL},
    {kHUSTR_TALKTOSELF3, "TALKTOSELF3", "HUSTR_TALKTOSELF3", 689, NULL},
    {kHUSTR_TALKTOSELF4, "TALKTOSELF4", "HUSTR_TALKTOSELF4", 690, NULL},
    {kHUSTR_TALKTOSELF5, "TALKTOSELF5", "HUSTR_TALKTOSELF5", 691, NULL},
    {kLOADNET, "NoLoadInNetGame", "LOADNET", 305, NULL},
    {kMSGOFF, "MessagesOff", "MSGOFF", 325, NULL},
    {kMSGON, "MessagesOn", "MSGON", 326, NULL},
    {kNETEND, "EndNetGame", "NETEND", 327, NULL},
    {kNEWGAME, "NewNetGame", "NEWGAME", 320, NULL},
    {kNIGHTMARE, "NightmareCheck", "NIGHTMARE", 322, NULL},
    {kPD_BLUEC, "NeedBlueCardForDoor", "PD_BLUEC", -1, NULL},
    {kPD_BLUEK, "NeedBlueForDoor", "PD_BLUEK", 419, NULL},
    {kPD_BLUEO, "NeedBlueForObject", "PD_BLUEO", 416, NULL},
    {kPD_BLUES, "NeedBlueSkullForDoor", "PD_BLUES", -1, NULL},
    {kPD_REDC, "NeedRedCardForDoor", "PD_REDC", -1, NULL},
    {kPD_REDK, "NeedRedForDoor", "PD_REDK", 421, NULL},
    {kPD_REDO, "NeedRedForObject", "PD_REDO", 417, NULL},
    {kPD_REDS, "NeedRedSkullForDoor", "PD_REDS", -1, NULL},
    {kPD_YELLOWC, "NeedYellowCardForDoor", "PD_YELLOWC", -1, NULL},
    {kPD_YELLOWK, "NeedYellowForDoor", "PD_YELLOWK", 420, NULL},
    {kPD_YELLOWS, "NeedYellowSkullForDoor", "PD_YELLOWS", -1, NULL},
    {kPD_YELLOWO, "NeedYellowForObject", "PD_YELLOWO", 418, NULL},
    {kPRESSKEY, "PressKey", "PRESSKEY", -1, NULL},
    {kPRESSYN, "PressYorN", "PRESSYN", -1, NULL},
    {kQLOADNET, "NoQLoadInNetGame", "QLOADNET", 310, NULL},
    {kQLPROMPT, "QuickLoad", "QLPROMPT", 312, NULL},
    {kQSAVESPOT, "NoQuickSaveSlot", "QSAVESPOT", 311, NULL},
    {kQSPROMPT, "QuickSaveOver", "QSPROMPT", 309, NULL},
    {kSAVEDEAD, "SaveWhenNotPlaying", "SAVEDEAD", 308, NULL},
    {kSTSTR_BEHOLD, "BEHOLDNote", "STSTR_BEHOLD", 585, NULL},
    {kSTSTR_BEHOLDX, "BEHOLDUsed", "STSTR_BEHOLDX", 584, NULL},
    {kSTSTR_CHOPPERS, "ChoppersNote", "STSTR_CHOPPERS", 586, NULL},
    {kSTSTR_CLEV, "LevelChange", "STSTR_CLEV", 588, NULL},
    {kSTSTR_DQDOFF, "GodModeOFF", "STSTR_DQDOFF", 578, NULL},
    {kSTSTR_DQDON, "GodModeON", "STSTR_DQDON", 577, NULL},
    {kSTSTR_FAADDED, "AmmoAdded", "STSTR_FAADDED", 579, NULL},
    {kSTSTR_KFAADDED, "VeryHappyAmmo", "STSTR_KFAADDED", 580, NULL},
    {kSTSTR_MUS, "MusChange", "STSTR_MUS", 581, NULL},
    {kSTSTR_NCOFF, "ClipOFF", "STSTR_NCOFF", 583, NULL},
    {kSTSTR_NCON, "ClipON", "STSTR_NCON", 582, NULL},
    {kSTSTR_NOMUS, "ImpossibleChange", "STSTR_NOMUS", -1, NULL},

    // DOOM I strings
    {kHUSTR_E1M1, "E1M1Desc", "HUSTR_E1M1", 625, NULL},
    {kHUSTR_E1M2, "E1M2Desc", "HUSTR_E1M2", 626, NULL},
    {kHUSTR_E1M3, "E1M3Desc", "HUSTR_E1M3", 627, NULL},
    {kHUSTR_E1M4, "E1M4Desc", "HUSTR_E1M4", 628, NULL},
    {kHUSTR_E1M5, "E1M5Desc", "HUSTR_E1M5", 629, NULL},
    {kHUSTR_E1M6, "E1M6Desc", "HUSTR_E1M6", 630, NULL},
    {kHUSTR_E1M7, "E1M7Desc", "HUSTR_E1M7", 631, NULL},
    {kHUSTR_E1M8, "E1M8Desc", "HUSTR_E1M8", 632, NULL},
    {kHUSTR_E1M9, "E1M9Desc", "HUSTR_E1M9", 633, NULL},
    {kHUSTR_E2M1, "E2M1Desc", "HUSTR_E2M1", 634, NULL},
    {kHUSTR_E2M2, "E2M2Desc", "HUSTR_E2M2", 635, NULL},
    {kHUSTR_E2M3, "E2M3Desc", "HUSTR_E2M3", 636, NULL},
    {kHUSTR_E2M4, "E2M4Desc", "HUSTR_E2M4", 637, NULL},
    {kHUSTR_E2M5, "E2M5Desc", "HUSTR_E2M5", 638, NULL},
    {kHUSTR_E2M6, "E2M6Desc", "HUSTR_E2M6", 639, NULL},
    {kHUSTR_E2M7, "E2M7Desc", "HUSTR_E2M7", 640, NULL},
    {kHUSTR_E2M8, "E2M8Desc", "HUSTR_E2M8", 641, NULL},
    {kHUSTR_E2M9, "E2M9Desc", "HUSTR_E2M9", 642, NULL},
    {kHUSTR_E3M1, "E3M1Desc", "HUSTR_E3M1", 643, NULL},
    {kHUSTR_E3M2, "E3M2Desc", "HUSTR_E3M2", 644, NULL},
    {kHUSTR_E3M3, "E3M3Desc", "HUSTR_E3M3", 645, NULL},
    {kHUSTR_E3M4, "E3M4Desc", "HUSTR_E3M4", 646, NULL},
    {kHUSTR_E3M5, "E3M5Desc", "HUSTR_E3M5", 647, NULL},
    {kHUSTR_E3M6, "E3M6Desc", "HUSTR_E3M6", 648, NULL},
    {kHUSTR_E3M7, "E3M7Desc", "HUSTR_E3M7", 649, NULL},
    {kHUSTR_E3M8, "E3M8Desc", "HUSTR_E3M8", 650, NULL},
    {kHUSTR_E3M9, "E3M9Desc", "HUSTR_E3M9", 651, NULL},
    {kHUSTR_E4M1, "E4M1Desc", "HUSTR_E4M1", -1, NULL},
    {kHUSTR_E4M2, "E4M2Desc", "HUSTR_E4M2", -1, NULL},
    {kHUSTR_E4M3, "E4M3Desc", "HUSTR_E4M3", -1, NULL},
    {kHUSTR_E4M4, "E4M4Desc", "HUSTR_E4M4", -1, NULL},
    {kHUSTR_E4M5, "E4M5Desc", "HUSTR_E4M5", -1, NULL},
    {kHUSTR_E4M6, "E4M6Desc", "HUSTR_E4M6", -1, NULL},
    {kHUSTR_E4M7, "E4M7Desc", "HUSTR_E4M7", -1, NULL},
    {kHUSTR_E4M8, "E4M8Desc", "HUSTR_E4M8", -1, NULL},
    {kHUSTR_E4M9, "E4M9Desc", "HUSTR_E4M9", -1, NULL},
    {kE1TEXT, "Episode1Text", "E1TEXT", 111, NULL},
    {kE2TEXT, "Episode2Text", "E2TEXT", 112, NULL},
    {kE3TEXT, "Episode3Text", "E3TEXT", 113, NULL},
    {kE4TEXT, "Episode4Text", "E4TEXT", -1, NULL},

    // DOOM II strings
    {kHUSTR_10, "Map10Desc", "HUSTR_10", 662, NULL},
    {kHUSTR_11, "Map11Desc", "HUSTR_11", 663, NULL},
    {kHUSTR_12, "Map12Desc", "HUSTR_12", 664, NULL},
    {kHUSTR_13, "Map13Desc", "HUSTR_13", 665, NULL},
    {kHUSTR_14, "Map14Desc", "HUSTR_14", 666, NULL},
    {kHUSTR_15, "Map15Desc", "HUSTR_15", 667, NULL},
    {kHUSTR_16, "Map16Desc", "HUSTR_16", 668, NULL},
    {kHUSTR_17, "Map17Desc", "HUSTR_17", 669, NULL},
    {kHUSTR_18, "Map18Desc", "HUSTR_18", 670, NULL},
    {kHUSTR_19, "Map19Desc", "HUSTR_19", 671, NULL},
    {kHUSTR_1, "Map01Desc", "HUSTR_1", 653, NULL},
    {kHUSTR_20, "Map20Desc", "HUSTR_20", 672, NULL},
    {kHUSTR_21, "Map21Desc", "HUSTR_21", 673, NULL},
    {kHUSTR_22, "Map22Desc", "HUSTR_22", 674, NULL},
    {kHUSTR_23, "Map23Desc", "HUSTR_23", 675, NULL},
    {kHUSTR_24, "Map24Desc", "HUSTR_24", 676, NULL},
    {kHUSTR_25, "Map25Desc", "HUSTR_25", 677, NULL},
    {kHUSTR_26, "Map26Desc", "HUSTR_26", 678, NULL},
    {kHUSTR_27, "Map27Desc", "HUSTR_27", 679, NULL},
    {kHUSTR_28, "Map28Desc", "HUSTR_28", 680, NULL},
    {kHUSTR_29, "Map29Desc", "HUSTR_29", 681, NULL},
    {kHUSTR_2, "Map02Desc", "HUSTR_2", 654, NULL},
    {kHUSTR_30, "Map30Desc", "HUSTR_30", 682, NULL},
    {kHUSTR_31, "Map31Desc", "HUSTR_31", 683, NULL},
    {kHUSTR_32, "Map32Desc", "HUSTR_32", 684, NULL},
    {kHUSTR_3, "Map03Desc", "HUSTR_3", 655, NULL},
    {kHUSTR_4, "Map04Desc", "HUSTR_4", 656, NULL},
    {kHUSTR_5, "Map05Desc", "HUSTR_5", 657, NULL},
    {kHUSTR_6, "Map06Desc", "HUSTR_6", 658, NULL},
    {kHUSTR_7, "Map07Desc", "HUSTR_7", 659, NULL},
    {kHUSTR_8, "Map08Desc", "HUSTR_8", 660, NULL},
    {kHUSTR_9, "Map09Desc", "HUSTR_9", 661, NULL},
    {kC1TEXT, "Level7Text", "C1TEXT", 114, NULL},
    {kC2TEXT, "Level12Text", "C2TEXT", 115, NULL},
    {kC3TEXT, "Level21Text", "C3TEXT", 116, NULL},
    {kC4TEXT, "EndGameText", "C4TEXT", 117, NULL},
    {kC5TEXT, "Level31Text", "C5TEXT", 118, NULL},
    {kC6TEXT, "Level32Text", "C6TEXT", 119, NULL},

    // TNT strings
    {kTHUSTR_10, "Tnt10Desc", "THUSTR_10", -1, NULL},
    {kTHUSTR_11, "Tnt11Desc", "THUSTR_11", -1, NULL},
    {kTHUSTR_12, "Tnt12Desc", "THUSTR_12", -1, NULL},
    {kTHUSTR_13, "Tnt13Desc", "THUSTR_13", -1, NULL},
    {kTHUSTR_14, "Tnt14Desc", "THUSTR_14", -1, NULL},
    {kTHUSTR_15, "Tnt15Desc", "THUSTR_15", -1, NULL},
    {kTHUSTR_16, "Tnt16Desc", "THUSTR_16", -1, NULL},
    {kTHUSTR_17, "Tnt17Desc", "THUSTR_17", -1, NULL},
    {kTHUSTR_18, "Tnt18Desc", "THUSTR_18", -1, NULL},
    {kTHUSTR_19, "Tnt19Desc", "THUSTR_19", -1, NULL},
    {kTHUSTR_1, "Tnt01Desc", "THUSTR_1", -1, NULL},
    {kTHUSTR_20, "Tnt20Desc", "THUSTR_20", -1, NULL},
    {kTHUSTR_21, "Tnt21Desc", "THUSTR_21", -1, NULL},
    {kTHUSTR_22, "Tnt22Desc", "THUSTR_22", -1, NULL},
    {kTHUSTR_23, "Tnt23Desc", "THUSTR_23", -1, NULL},
    {kTHUSTR_24, "Tnt24Desc", "THUSTR_24", -1, NULL},
    {kTHUSTR_25, "Tnt25Desc", "THUSTR_25", -1, NULL},
    {kTHUSTR_26, "Tnt26Desc", "THUSTR_26", -1, NULL},
    {kTHUSTR_27, "Tnt27Desc", "THUSTR_27", -1, NULL},
    {kTHUSTR_28, "Tnt28Desc", "THUSTR_28", -1, NULL},
    {kTHUSTR_29, "Tnt29Desc", "THUSTR_29", -1, NULL},
    {kTHUSTR_2, "Tnt02Desc", "THUSTR_2", -1, NULL},
    {kTHUSTR_30, "Tnt30Desc", "THUSTR_30", -1, NULL},
    {kTHUSTR_31, "Tnt31Desc", "THUSTR_31", -1, NULL},
    {kTHUSTR_32, "Tnt32Desc", "THUSTR_32", -1, NULL},
    {kTHUSTR_3, "Tnt03Desc", "THUSTR_3", -1, NULL},
    {kTHUSTR_4, "Tnt04Desc", "THUSTR_4", -1, NULL},
    {kTHUSTR_5, "Tnt05Desc", "THUSTR_5", -1, NULL},
    {kTHUSTR_6, "Tnt06Desc", "THUSTR_6", -1, NULL},
    {kTHUSTR_7, "Tnt07Desc", "THUSTR_7", -1, NULL},
    {kTHUSTR_8, "Tnt08Desc", "THUSTR_8", -1, NULL},
    {kTHUSTR_9, "Tnt09Desc", "THUSTR_9", -1, NULL},
    {kT1TEXT, "TntLevel7Text", "T1TEXT", -1, NULL},
    {kT2TEXT, "TntLevel12Text", "T2TEXT", -1, NULL},
    {kT3TEXT, "TntLevel21Text", "T3TEXT", -1, NULL},
    {kT4TEXT, "TntEndGameText", "T4TEXT", -1, NULL},
    {kT5TEXT, "TntLevel31Text", "T5TEXT", -1, NULL},
    {kT6TEXT, "TntLevel32Text", "T6TEXT", -1, NULL},

    // PLUTONIA strings
    {kPHUSTR_10, "Plut10Desc", "PHUSTR_10", -1, NULL},
    {kPHUSTR_11, "Plut11Desc", "PHUSTR_11", -1, NULL},
    {kPHUSTR_12, "Plut12Desc", "PHUSTR_12", -1, NULL},
    {kPHUSTR_13, "Plut13Desc", "PHUSTR_13", -1, NULL},
    {kPHUSTR_14, "Plut14Desc", "PHUSTR_14", -1, NULL},
    {kPHUSTR_15, "Plut15Desc", "PHUSTR_15", -1, NULL},
    {kPHUSTR_16, "Plut16Desc", "PHUSTR_16", -1, NULL},
    {kPHUSTR_17, "Plut17Desc", "PHUSTR_17", -1, NULL},
    {kPHUSTR_18, "Plut18Desc", "PHUSTR_18", -1, NULL},
    {kPHUSTR_19, "Plut19Desc", "PHUSTR_19", -1, NULL},
    {kPHUSTR_1, "Plut01Desc", "PHUSTR_1", -1, NULL},
    {kPHUSTR_20, "Plut20Desc", "PHUSTR_20", -1, NULL},
    {kPHUSTR_21, "Plut21Desc", "PHUSTR_21", -1, NULL},
    {kPHUSTR_22, "Plut22Desc", "PHUSTR_22", -1, NULL},
    {kPHUSTR_23, "Plut23Desc", "PHUSTR_23", -1, NULL},
    {kPHUSTR_24, "Plut24Desc", "PHUSTR_24", -1, NULL},
    {kPHUSTR_25, "Plut25Desc", "PHUSTR_25", -1, NULL},
    {kPHUSTR_26, "Plut26Desc", "PHUSTR_26", -1, NULL},
    {kPHUSTR_27, "Plut27Desc", "PHUSTR_27", -1, NULL},
    {kPHUSTR_28, "Plut28Desc", "PHUSTR_28", -1, NULL},
    {kPHUSTR_29, "Plut29Desc", "PHUSTR_29", -1, NULL},
    {kPHUSTR_2, "Plut02Desc", "PHUSTR_2", -1, NULL},
    {kPHUSTR_30, "Plut30Desc", "PHUSTR_30", -1, NULL},
    {kPHUSTR_31, "Plut31Desc", "PHUSTR_31", -1, NULL},
    {kPHUSTR_32, "Plut32Desc", "PHUSTR_32", -1, NULL},
    {kPHUSTR_3, "Plut03Desc", "PHUSTR_3", -1, NULL},
    {kPHUSTR_4, "Plut04Desc", "PHUSTR_4", -1, NULL},
    {kPHUSTR_5, "Plut05Desc", "PHUSTR_5", -1, NULL},
    {kPHUSTR_6, "Plut06Desc", "PHUSTR_6", -1, NULL},
    {kPHUSTR_7, "Plut07Desc", "PHUSTR_7", -1, NULL},
    {kPHUSTR_8, "Plut08Desc", "PHUSTR_8", -1, NULL},
    {kPHUSTR_9, "Plut09Desc", "PHUSTR_9", -1, NULL},
    {kP1TEXT, "PlutLevel7Text", "P1TEXT", -1, NULL},
    {kP2TEXT, "PlutLevel12Text", "P2TEXT", -1, NULL},
    {kP3TEXT, "PlutLevel21Text", "P3TEXT", -1, NULL},
    {kP4TEXT, "PlutEndGameText", "P4TEXT", -1, NULL},
    {kP5TEXT, "PlutLevel31Text", "P5TEXT", -1, NULL},
    {kP6TEXT, "PlutLevel32Text", "P6TEXT", -1, NULL},

    // Extra strings (not found in LANGUAGE.LDF)
    {kX_COMMERC, "Commercial", "X_COMMERC", 233, NULL},
    {kX_REGIST, "Registered", "X_REGIST", 230, NULL},
    {kX_TITLE1, "Title1", "X_TITLE1", -1, NULL},
    {kX_TITLE2, "Title2", "X_TITLE2", 194, NULL},
    {kX_TITLE3, "Title3", "X_TITLE3", 195, NULL},
    {kX_MODIFIED, "Notice", "X_MODIFIED", 229, NULL},
    {kX_NODIST1, "Notice", "X_NODIST1", 231, NULL},
    {kX_NODIST2, "Notice", "X_NODIST2", 234, NULL},
    {kD_CDROM, "CDRom", "D_CDROM", 199, NULL},
    {kDETAILHI, "DetailHigh", "DETAILHI", 330, NULL},
    {kDETAILLO, "DetailLow", "DETAILLO", 331, NULL},
    {kQUITMSG, "QuitMsg", "QUITMSG", -1, NULL},
    {kSWSTRING, "Shareware", "SWSTRING", 323, NULL},

    // Monster cast names...
    {kCC_ZOMBIE, "ZombiemanName", "CC_ZOMBIE", 129, NULL},
    {kCC_SHOTGUN, "ShotgunGuyName", "CC_SHOTGUN", 130, NULL},
    {kCC_HEAVY, "HeavyWeaponDudeName", "CC_HEAVY", 131, NULL},
    {kCC_IMP, "ImpName", "CC_IMP", 132, NULL},
    {kCC_DEMON, "DemonName", "CC_DEMON", 133, NULL},
    {kCC_LOST, "LostSoulName", "CC_LOST", 134, NULL},
    {kCC_CACO, "CacodemonName", "CC_CACO", 135, NULL},
    {kCC_HELL, "HellKnightName", "CC_HELL", 136, NULL},
    {kCC_BARON, "BaronOfHellName", "CC_BARON", 137, NULL},
    {kCC_ARACH, "ArachnotronName", "CC_ARACH", 138, NULL},
    {kCC_PAIN, "PainElementalName", "CC_PAIN", 139, NULL},
    {kCC_REVEN, "RevenantName", "CC_REVEN", 140, NULL},
    {kCC_MANCU, "MancubusName", "CC_MANCU", 141, NULL},
    {kCC_ARCH, "ArchVileName", "CC_ARCH", 142, NULL},
    {kCC_SPIDER, "SpiderMastermindName", "CC_SPIDER", 143, NULL},
    {kCC_CYBER, "CyberdemonName", "CC_CYBER", 144, NULL},
    {kCC_HERO, "OurHeroName", "CC_HERO", 145, NULL},

    // Obituaries (not strictly BEX, but Freedoom 1/2 use them)
    {kOB_BABY, "OB_Arachnotron", "OB_BABY", -1, NULL},
    {kOB_VILE, "OB_Archvile", "OB_VILE", -1, NULL},
    {kOB_BARON, "OB_Baron", "OB_BARON", -1, NULL},
    {kOB_BARONHIT, "OB_BaronClaw", "OB_BARONHIT", -1, NULL},
    {kOB_CACOHIT, "OB_CacoBite", "OB_CACOHIT", -1, NULL},
    {kOB_CACO, "OB_Cacodemon", "OB_CACO", -1, NULL},
    {kOB_CHAINGUY, "OB_ChaingunGuy", "OB_CHAINGUY", -1, NULL},
    {kOB_CYBORG, "OB_Cyberdemon", "OB_CYBORG", -1, NULL},
    {kOB_SPIDER, "OB_Mastermind", "OB_SPIDER", -1, NULL},
    {kOB_WOLFSS, "OB_WolfSS", "OB_WOLFSS", -1, NULL},
    {kOB_DEMONHIT, "OB_Demon", "OB_DEMONHIT", -1, NULL},
    {kOB_IMP, "OB_Imp", "OB_IMP", -1, NULL},
    {kOB_IMPHIT, "OB_ImpClaw", "OB_IMPHIT", -1, NULL},
    {kOB_FATSO, "OB_Mancubus", "OB_FATSO", -1, NULL},
    {kOB_UNDEAD, "OB_Revenant", "OB_UNDEAD", -1, NULL},
    {kOB_UNDEADHIT, "OB_RevPunch", "OB_UNDEADHIT", -1, NULL},
    {kOB_SHOTGUY, "OB_ShotgunGuy", "OB_SHOTGUY", -1, NULL},
    {kOB_SKULL, "OB_Skull", "OB_SKULL", -1, NULL},
    {kOB_ZOMBIE, "OB_Zombie", "OB_ZOMBIE", -1, NULL},
    {kOB_MPCHAINGUN, "OB_Chaingun", "OB_MPCHAINGUN", -1, NULL},
    {kOB_MPPISTOL, "OB_Pistol", "OB_MPPISTOL", -1, NULL},
    {kOB_MPROCKET, "OB_Missile", "OB_MPROCKET", -1, NULL},
    {kOB_MPR_SPLASH, "OB_Missile", "OB_MPR_SPLASH", -1, NULL},
    {kOB_MPPLASMARIFLE, "OB_Plasma", "OB_MPPLASMARIFLE", -1, NULL},
    {kOB_MPFIST, "OB_Punch", "OB_MPFIST", -1, NULL},
    {kOB_MPCHAINSAW, "OB_Saw", "OB_MPCHAINSAW", -1, NULL},
    {kOB_MPSHOTGUN, "OB_Shotgun", "OB_MPSHOTGUN", -1, NULL},
    {kOB_MPBFG_BOOM, "OB_BFG", "OB_MPBFG_BOOM", -1, NULL},
    {kOB_MPBFG_SPLASH, "OB_BFG", "OB_MPBFG_SPLASH", -1, NULL},
    {kOB_MPBFG_MBF, "OB_BFG", "OB_MPBFG_MBF", -1, NULL},

    {NULL, NULL, NULL, -1, NULL} // End sentinel
};

LanguageInfo cheat_list[] = {
    {"idbehold", "idbehold9", "BEHOLD menu", -1, NULL},
    {"idbeholda", "idbehold5", "Auto-map", -1, NULL},
    {"idbeholdi", "idbehold3", "Invisibility", -1, NULL},
    {"idbeholdl", "idbehold6", "Lite-Amp Goggles", -1, NULL},
    {"idbeholdr", "idbehold4", "Radiation Suit", -1, NULL},
    {"idbeholds", "idbehold2", "Berserk", -1, NULL},
    {"idbeholdv", "idbehold1", "Invincibility", -1, NULL},
    {"idchoppers", "idchoppers", "Chainsaw", -1, NULL},
    {"idclev", "idclev", "Level Warp", -1, NULL},
    {"idclip", "idclip", "No Clipping 2", -1, NULL},
    {"iddqd", "iddqd", "God mode", -1, NULL},
    {"iddt", "iddt", "Map cheat", -1, NULL},
    {"idfa", "idfa", "Ammo", -1, NULL},
    {"idkfa", "idkfa", "Ammo & Keys", -1, NULL},
    {"idmus", "idmus", "Change music", -1, NULL},
    {"idmypos", "idmypos", "Player Position", -1, NULL},
    {"idspispopd", "idspispopd", "No Clipping 1", -1, NULL},

    {NULL, NULL, NULL, -1, NULL} // End sentinel
};

const char *lang_bex_unsupported[] = {
    "BGCASTCALL", "BGFLAT06", "BGFLAT11", "BGFLAT15", "BGFLAT20", "BGFLAT30",      "BGFLAT31",     "BGFLATE1",
    "BGFLATE2",   "BGFLATE3", "BGFLATE4", "PD_ALL3",  "PD_ALL6",  "PD_ANY",        "RESTARTLEVEL", "SAVEGAMENAME",
    "STARTUP1",   "STARTUP2", "STARTUP3", "STARTUP4", "STARTUP5", "STSTR_COMPOFF", "STSTR_COMPON",

    NULL};

//------------------------------------------------------------------------

void text_strings::Init()
{
}

void text_strings::Shutdown()
{
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

bool text_strings::ReplaceString(const char *before, const char *after)
{
    SYS_ASSERT(after[0]);

    for (int i = 0; lang_list[i].orig_text; i++)
    {
        LanguageInfo *lang = lang_list + i;

        if (StrCaseCmp(before, lang->orig_text) != 0)
            continue;

        int len = strlen(lang->orig_text);

        if (!lang->new_text)
            lang->new_text = StringNew(len + 5);

        StrMaxCopy(lang->new_text, after, len + 4);

        return true;
    }

    return false;
}

bool text_strings::ReplaceBexString(const char *bex_name, const char *after)
{
    SYS_ASSERT(after[0]);

    for (int i = 0; lang_list[i].orig_text; i++)
    {
        LanguageInfo *lang = lang_list + i;

        if (StrCaseCmp(bex_name, lang->deh_name) != 0)
            continue;

        if (lang->new_text)
            free(lang->new_text);

        lang->new_text = StringDup(after);

        return true;
    }

    return false;
}

void text_strings::ReplaceBinaryString(int v166_index, const char *str)
{
    SYS_ASSERT(str[0]);

    for (int i = 0; lang_list[i].orig_text; i++)
    {
        LanguageInfo *lang = lang_list + i;

        if (lang->v166_index != v166_index)
            continue;

        // OK, found it, so check if it has changed

        if (StrCaseCmp(str, lang->orig_text) != 0)
        {
            int len = strlen(lang->orig_text);

            if (!lang->new_text)
                lang->new_text = StringNew(len + 5);

            StrMaxCopy(lang->new_text, str, len + 4);
        }

        return;
    }
}

bool text_strings::ReplaceCheat(const char *deh_name, const char *str)
{
    SYS_ASSERT(str[0]);

    // DOOM cheats were terminated with an 0xff byte
    int eoln = 0xff;

    for (int i = 0; cheat_list[i].orig_text; i++)
    {
        LanguageInfo *cht = cheat_list + i;

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

        if (!cht->new_text)
            cht->new_text = StringNew(len + 1);

        StrMaxCopy(cht->new_text, str, len);

        return true;
    }

    return false;
}

void text_strings::AlterCheat(const char *new_val)
{
    const char *deh_field = patch::line_buf;

    if (!ReplaceCheat(deh_field, new_val))
    {
        I_Debugf("Dehacked: Warning - UNKNOWN CHEAT FIELD: %s\n", deh_field);
    }
}

//------------------------------------------------------------------------

namespace text_strings
{
bool got_one;

void BeginTextLump();
void FinishTextLump();
void WriteTextString(const LanguageInfo *info);
} // namespace text_strings

void text_strings::BeginTextLump()
{
    wad::NewLump(DDF_Language);

    wad::Printf("<LANGUAGES>\n\n");
    wad::Printf("[ENGLISH]\n");
}

void text_strings::FinishTextLump()
{
    wad::Printf("\n");
}

void text_strings::WriteTextString(const LanguageInfo *info)
{
    if (!got_one)
    {
        got_one = true;
        BeginTextLump();
    }

    wad::Printf("%s = \"", info->ldf_name);

    const char *str = info->new_text ? info->new_text : info->orig_text;

    for (; *str; str++)
    {
        if (*str == '\n')
        {
            wad::Printf("\\n\"\n  \"");
            continue;
        }

        if (*str == '"')
        {
            wad::Printf("\\\"");
            continue;
        }

        // XXX may need special handling for non-english chars
        wad::Printf("%c", *str);
    }

    wad::Printf("\";\n");
}

const char *text_strings::GetLDFForBex(const char *bex_name)
{
    for (auto entry : lang_list)
    {
        if (entry.deh_name && StrCaseCmp(entry.deh_name, bex_name) == 0)
            return entry.ldf_name;
    }
    return nullptr;
}

void text_strings::ConvertLDF(void)
{
    got_one = false;

    for (int i = 0; lang_list[i].orig_text; i++)
    {
        if (!all_mode && !lang_list[i].new_text)
            continue;

        WriteTextString(lang_list + i);
    }

    // --- cheats ---

    if (got_one)
        wad::Printf("\n");

    for (int i = 0; cheat_list[i].orig_text; i++)
    {
        if (!all_mode && !cheat_list[i].new_text)
            continue;

        WriteTextString(cheat_list + i);
    }

    if (got_one)
        FinishTextLump();
}

} // namespace dehacked
