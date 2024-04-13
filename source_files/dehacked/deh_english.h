//------------------------------------------------------------------------
//  English Language Strings
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

#pragma once

namespace dehacked
{

// This file diverges slightly from the style guide with constant naming
// as these reflect the historical string names - Dasho

//
// D_Main.C
//
constexpr const char *kD_DEVSTR = "Development mode ON.\n";
constexpr const char *kD_CDROM  = "CD-ROM Version: default.cfg from c:\\doomdata\n";

//
//  M_Menu.C
//
constexpr const char *kPRESSKEY    = "press a key.";
constexpr const char *kPRESSYN     = "press y or n.";
constexpr const char *kQUITMSG     = "are you sure you want to\nquit this great game?";
constexpr const char *kLOADNET     = "you can't do load while in a net game!\n\npress a key.";
constexpr const char *kQLOADNET    = "you can't quickload during a netgame!\n\npress a key.";
constexpr const char *kQSAVESPOT   = "you haven't picked a quicksave slot yet!\n\npress a key.";
constexpr const char *kSAVEDEAD    = "you can't save if you aren't playing!\n\npress a key.";
constexpr const char *kQSPROMPT    = "quicksave over your game named\n\n'%s'?\n\npress y or n.";
constexpr const char *kQLPROMPT    = "do you want to quickload the game named\n\n'%s'?\n\npress y or n.";
constexpr const char *kNEWGAME     = "you can't start a new game\nwhile in a network game.\n\npress a key.";
constexpr const char *kNIGHTMARE   = "are you sure? this skill level\nisn't even remotely fair.\n\npress y or "
                                     "n.";
constexpr const char *kSWSTRING    = "this is the shareware version of doom.\n\nyou need to order the entire "
                                     "trilogy.\n\npress a key.";
constexpr const char *kMSGOFF      = "Messages OFF";
constexpr const char *kMSGON       = "Messages ON";
constexpr const char *kNETEND      = "you can't end a netgame!\n\npress a key.";
constexpr const char *kENDGAME     = "are you sure you want to end the game?\n\npress y or n.";
constexpr const char *kDOSY        = "(press y to quit)";
constexpr const char *kDETAILHI    = "High detail";
constexpr const char *kDETAILLO    = "Low detail";
constexpr const char *kGAMMALVL0   = "Gamma correction OFF";
constexpr const char *kGAMMALVL1   = "Gamma correction level 1";
constexpr const char *kGAMMALVL2   = "Gamma correction level 2";
constexpr const char *kGAMMALVL3   = "Gamma correction level 3";
constexpr const char *kGAMMALVL4   = "Gamma correction level 4";
constexpr const char *kEMPTYSTRING = "empty slot";

//
//  P_inter.C
//
constexpr const char *kGOTARMOR    = "Picked up the armor.";
constexpr const char *kGOTMEGA     = "Picked up the MegaArmor!";
constexpr const char *kGOTHTHBONUS = "Picked up a health bonus.";
constexpr const char *kGOTARMBONUS = "Picked up an armor bonus.";
constexpr const char *kGOTSTIM     = "Picked up a stimpack.";
constexpr const char *kGOTMEDINEED = "Picked up a medikit that you REALLY need!";
constexpr const char *kGOTMEDIKIT  = "Picked up a medikit.";
constexpr const char *kGOTSUPER    = "Supercharge!";
constexpr const char *kGOTBLUECARD = "Picked up a blue keycard.";
constexpr const char *kGOTYELWCARD = "Picked up a yellow keycard.";
constexpr const char *kGOTREDCARD  = "Picked up a red keycard.";
constexpr const char *kGOTBLUESKUL = "Picked up a blue skull key.";
constexpr const char *kGOTYELWSKUL = "Picked up a yellow skull key.";
constexpr const char *kGOTREDSKUL = "Picked up a red skull key.";
constexpr const char *kGOTINVUL    = "Invulnerability!";
constexpr const char *kGOTBERSERK  = "Berserk!";
constexpr const char *kGOTINVIS    = "Partial Invisibility";
constexpr const char *kGOTSUIT     = "Radiation Shielding Suit";
constexpr const char *kGOTMAP      = "Computer Area Map";
constexpr const char *kGOTVISOR    = "Light Amplification Visor";
constexpr const char *kGOTMSPHERE  = "MegaSphere!";
constexpr const char *kGOTCLIP     = "Picked up a clip.";
constexpr const char *kGOTCLIPBOX  = "Picked up a box of bullets.";
constexpr const char *kGOTROCKET   = "Picked up a rocket.";
constexpr const char *kGOTROCKBOX  = "Picked up a box of rockets.";
constexpr const char *kGOTCELL     = "Picked up an energy cell.";
constexpr const char *kGOTCELLBOX  = "Picked up an energy cell pack.";
constexpr const char *kGOTSHELLS   = "Picked up 4 shotgun shells.";
constexpr const char *kGOTSHELLBOX = "Picked up a box of shotgun shells.";
constexpr const char *kGOTBACKPACK = "Picked up a backpack full of ammo!";
constexpr const char *kGOTBFG9000  = "You got the BFG9000!  Oh, yes.";
constexpr const char *kGOTCHAINGUN = "You got the chaingun!";
constexpr const char *kGOTCHAINSAW = "A chainsaw!  Find some meat!";
constexpr const char *kGOTLAUNCHER = "You got the rocket launcher!";
constexpr const char *kGOTPLASMA   = "You got the plasma gun!";
constexpr const char *kGOTSHOTGUN  = "You got the shotgun!";
constexpr const char *kGOTSHOTGUN2 = "You got the super shotgun!";

//
// P_Doors.C
//
constexpr const char *kPD_BLUEC   = "You need a blue card to open this door";
constexpr const char *kPD_REDC    = "You need a red card to open this door";
constexpr const char *kPD_YELLOWC = "You need a yellow card to open this door";
constexpr const char *kPD_BLUECO   = "You need a blue card to activate this object";
constexpr const char *kPD_REDCO    = "You need a red card to activate this object";
constexpr const char *kPD_YELLOWCO = "You need a yellow card to activate this object";
constexpr const char *kPD_BLUEO   = "You need a blue key to activate this object";
constexpr const char *kPD_REDO    = "You need a red key to activate this object";
constexpr const char *kPD_YELLOWO = "You need a yellow key to activate this object";
constexpr const char *kPD_BLUEK   = "You need a blue key to open this door";
constexpr const char *kPD_BLUES   = "You need a blue skull to open this door";
constexpr const char *kPD_REDK    = "You need a red key to open this door";
constexpr const char *kPD_REDS    = "You need a red skull to open this door";
constexpr const char *kPD_YELLOWK = "You need a yellow key to open this door";
constexpr const char *kPD_YELLOWS = "You need a yellow skull to open this door";
constexpr const char *kPD_BLUESO   = "You need a blue skull to activate this object";
constexpr const char *kPD_REDSO    = "You need a red skull to activate this object";
constexpr const char *kPD_YELLOWSO = "You need a yellow skull to activate this object";
constexpr const char *kPD_ANY = "Any key will open this door";
constexpr const char *kPD_ALL3 = "You need all three keys to open this door";
constexpr const char *kPD_ALL6 = "You need all six keys to open this door";

//
//  G_game.C
//
constexpr const char *kGGSAVED = "game saved.";

//
//  HU_stuff.C
//
constexpr const char *kHUSTR_MSGU        = "[Message unsent]";
constexpr const char *kHUSTR_E1M1        = "E1M1: Hangar";
constexpr const char *kHUSTR_E1M2        = "E1M2: Nuclear Plant";
constexpr const char *kHUSTR_E1M3        = "E1M3: Toxin Refinery";
constexpr const char *kHUSTR_E1M4        = "E1M4: Command Control";
constexpr const char *kHUSTR_E1M5        = "E1M5: Phobos Lab";
constexpr const char *kHUSTR_E1M6        = "E1M6: Central Processing";
constexpr const char *kHUSTR_E1M7        = "E1M7: Computer Station";
constexpr const char *kHUSTR_E1M8        = "E1M8: Phobos Anomaly";
constexpr const char *kHUSTR_E1M9        = "E1M9: Military Base";
constexpr const char *kHUSTR_E2M1        = "E2M1: Deimos Anomaly";
constexpr const char *kHUSTR_E2M2        = "E2M2: Containment Area";
constexpr const char *kHUSTR_E2M3        = "E2M3: Refinery";
constexpr const char *kHUSTR_E2M4        = "E2M4: Deimos Lab";
constexpr const char *kHUSTR_E2M5        = "E2M5: Command Center";
constexpr const char *kHUSTR_E2M6        = "E2M6: Halls of the Damned";
constexpr const char *kHUSTR_E2M7        = "E2M7: Spawning Vats";
constexpr const char *kHUSTR_E2M8        = "E2M8: Tower of Babel";
constexpr const char *kHUSTR_E2M9        = "E2M9: Fortress of Mystery";
constexpr const char *kHUSTR_E3M1        = "E3M1: Hell Keep";
constexpr const char *kHUSTR_E3M2        = "E3M2: Slough of Despair";
constexpr const char *kHUSTR_E3M3        = "E3M3: Pandemonium";
constexpr const char *kHUSTR_E3M4        = "E3M4: House of Pain";
constexpr const char *kHUSTR_E3M5        = "E3M5: Unholy Cathedral";
constexpr const char *kHUSTR_E3M6        = "E3M6: Mt. Erebus";
constexpr const char *kHUSTR_E3M7        = "E3M7: Limbo";
constexpr const char *kHUSTR_E3M8        = "E3M8: Dis";
constexpr const char *kHUSTR_E3M9        = "E3M9: Warrens";
constexpr const char *kHUSTR_E4M1        = "E4M1: Hell Beneath";
constexpr const char *kHUSTR_E4M2        = "E4M2: Perfect Hatred";
constexpr const char *kHUSTR_E4M3        = "E4M3: Sever The Wicked";
constexpr const char *kHUSTR_E4M4        = "E4M4: Unruly Evil";
constexpr const char *kHUSTR_E4M5        = "E4M5: They Will Repent";
constexpr const char *kHUSTR_E4M6        = "E4M6: Against Thee Wickedly";
constexpr const char *kHUSTR_E4M7        = "E4M7: And Hell Followed";
constexpr const char *kHUSTR_E4M8        = "E4M8: Unto The Cruel";
constexpr const char *kHUSTR_E4M9        = "E4M9: Fear";
constexpr const char *kHUSTR_1           = "level 1: entryway";
constexpr const char *kHUSTR_2           = "level 2: underhalls";
constexpr const char *kHUSTR_3           = "level 3: the gantlet";
constexpr const char *kHUSTR_4           = "level 4: the focus";
constexpr const char *kHUSTR_5           = "level 5: the waste tunnels";
constexpr const char *kHUSTR_6           = "level 6: the crusher";
constexpr const char *kHUSTR_7           = "level 7: dead simple";
constexpr const char *kHUSTR_8           = "level 8: tricks and traps";
constexpr const char *kHUSTR_9           = "level 9: the pit";
constexpr const char *kHUSTR_10          = "level 10: refueling base";
constexpr const char *kHUSTR_11          = "level 11: 'o' of destruction!";
constexpr const char *kHUSTR_12          = "level 12: the factory";
constexpr const char *kHUSTR_13          = "level 13: downtown";
constexpr const char *kHUSTR_14          = "level 14: the inmost dens";
constexpr const char *kHUSTR_15          = "level 15: industrial zone";
constexpr const char *kHUSTR_16          = "level 16: suburbs";
constexpr const char *kHUSTR_17          = "level 17: tenements";
constexpr const char *kHUSTR_18          = "level 18: the courtyard";
constexpr const char *kHUSTR_19          = "level 19: the citadel";
constexpr const char *kHUSTR_20          = "level 20: gotcha!";
constexpr const char *kHUSTR_21          = "level 21: nirvana";
constexpr const char *kHUSTR_22          = "level 22: the catacombs";
constexpr const char *kHUSTR_23          = "level 23: barrels o' fun";
constexpr const char *kHUSTR_24          = "level 24: the chasm";
constexpr const char *kHUSTR_25          = "level 25: bloodfalls";
constexpr const char *kHUSTR_26          = "level 26: the abandoned mines";
constexpr const char *kHUSTR_27          = "level 27: monster condo";
constexpr const char *kHUSTR_28          = "level 28: the spirit world";
constexpr const char *kHUSTR_29          = "level 29: the living end";
constexpr const char *kHUSTR_30          = "level 30: icon of sin";
constexpr const char *kHUSTR_31          = "level 31: wolfenstein";
constexpr const char *kHUSTR_32          = "level 32: grosse";
constexpr const char *kPHUSTR_1          = "level 1: congo";
constexpr const char *kPHUSTR_2          = "level 2: well of souls";
constexpr const char *kPHUSTR_3          = "level 3: aztec";
constexpr const char *kPHUSTR_4          = "level 4: caged";
constexpr const char *kPHUSTR_5          = "level 5: ghost town";
constexpr const char *kPHUSTR_6          = "level 6: baron's lair";
constexpr const char *kPHUSTR_7          = "level 7: caughtyard";
constexpr const char *kPHUSTR_8          = "level 8: realm";
constexpr const char *kPHUSTR_9          = "level 9: abattoire";
constexpr const char *kPHUSTR_10         = "level 10: onslaught";
constexpr const char *kPHUSTR_11         = "level 11: hunted";
constexpr const char *kPHUSTR_12         = "level 12: speed";
constexpr const char *kPHUSTR_13         = "level 13: the crypt";
constexpr const char *kPHUSTR_14         = "level 14: genesis";
constexpr const char *kPHUSTR_15         = "level 15: the twilight";
constexpr const char *kPHUSTR_16         = "level 16: the omen";
constexpr const char *kPHUSTR_17         = "level 17: compound";
constexpr const char *kPHUSTR_18         = "level 18: neurosphere";
constexpr const char *kPHUSTR_19         = "level 19: nme";
constexpr const char *kPHUSTR_20         = "level 20: the death domain";
constexpr const char *kPHUSTR_21         = "level 21: slayer";
constexpr const char *kPHUSTR_22         = "level 22: impossible mission";
constexpr const char *kPHUSTR_23         = "level 23: tombstone";
constexpr const char *kPHUSTR_24         = "level 24: the final frontier";
constexpr const char *kPHUSTR_25         = "level 25: the temple of darkness";
constexpr const char *kPHUSTR_26         = "level 26: bunker";
constexpr const char *kPHUSTR_27         = "level 27: anti-christ";
constexpr const char *kPHUSTR_28         = "level 28: the sewers";
constexpr const char *kPHUSTR_29         = "level 29: odyssey of noises";
constexpr const char *kPHUSTR_30         = "level 30: the gateway of hell";
constexpr const char *kPHUSTR_31         = "level 31: cyberden";
constexpr const char *kPHUSTR_32         = "level 32: go 2 it";
constexpr const char *kTHUSTR_1          = "level 1: system control";
constexpr const char *kTHUSTR_2          = "level 2: human bbq";
constexpr const char *kTHUSTR_3          = "level 3: power control";
constexpr const char *kTHUSTR_4          = "level 4: wormhole";
constexpr const char *kTHUSTR_5          = "level 5: hanger";
constexpr const char *kTHUSTR_6          = "level 6: open season";
constexpr const char *kTHUSTR_7          = "level 7: prison";
constexpr const char *kTHUSTR_8          = "level 8: metal";
constexpr const char *kTHUSTR_9          = "level 9: stronghold";
constexpr const char *kTHUSTR_10         = "level 10: redemption";
constexpr const char *kTHUSTR_11         = "level 11: storage facility";
constexpr const char *kTHUSTR_12         = "level 12: crater";
constexpr const char *kTHUSTR_13         = "level 13: nukage processing";
constexpr const char *kTHUSTR_14         = "level 14: steel works";
constexpr const char *kTHUSTR_15         = "level 15: dead zone";
constexpr const char *kTHUSTR_16         = "level 16: deepest reaches";
constexpr const char *kTHUSTR_17         = "level 17: processing area";
constexpr const char *kTHUSTR_18         = "level 18: mill";
constexpr const char *kTHUSTR_19         = "level 19: shipping/respawning";
constexpr const char *kTHUSTR_20         = "level 20: central processing";
constexpr const char *kTHUSTR_21         = "level 21: administration center";
constexpr const char *kTHUSTR_22         = "level 22: habitat";
constexpr const char *kTHUSTR_23         = "level 23: lunar mining project";
constexpr const char *kTHUSTR_24         = "level 24: quarry";
constexpr const char *kTHUSTR_25         = "level 25: baron's den";
constexpr const char *kTHUSTR_26         = "level 26: ballistyx";
constexpr const char *kTHUSTR_27         = "level 27: mount pain";
constexpr const char *kTHUSTR_28         = "level 28: heck";
constexpr const char *kTHUSTR_29         = "level 29: river styx";
constexpr const char *kTHUSTR_30         = "level 30: last call";
constexpr const char *kTHUSTR_31         = "level 31: pharaoh";
constexpr const char *kTHUSTR_32         = "level 32: caribbean";
constexpr const char *kHUSTR_CHATMACRO1  = "I'm ready to kick butt!";
constexpr const char *kHUSTR_CHATMACRO2  = "I'm OK.";
constexpr const char *kHUSTR_CHATMACRO3  = "I'm not looking too good!";
constexpr const char *kHUSTR_CHATMACRO4  = "Help!";
constexpr const char *kHUSTR_CHATMACRO5  = "You suck!";
constexpr const char *kHUSTR_CHATMACRO6  = "Next time, scumbag...";
constexpr const char *kHUSTR_CHATMACRO7  = "Come here!";
constexpr const char *kHUSTR_CHATMACRO8  = "I'll take care of it.";
constexpr const char *kHUSTR_CHATMACRO9  = "Yes";
constexpr const char *kHUSTR_CHATMACRO0  = "No";
constexpr const char *kHUSTR_TALKTOSELF1 = "You mumble to yourself";
constexpr const char *kHUSTR_TALKTOSELF2 = "Who's there?";
constexpr const char *kHUSTR_TALKTOSELF3 = "You scare yourself";
constexpr const char *kHUSTR_TALKTOSELF4 = "You start to rave";
constexpr const char *kHUSTR_TALKTOSELF5 = "You've lost it...";
constexpr const char *kHUSTR_MESSAGESENT = "[Message Sent]";

// The following should NOT be changed unless it seems
// just AWFULLY necessary

constexpr const char *kHUSTR_PLRGREEN  = "Green: ";
constexpr const char *kHUSTR_PLRINDIGO = "Indigo: ";
constexpr const char *kHUSTR_PLRBROWN  = "Brown: ";
constexpr const char *kHUSTR_PLRRED    = "Red: ";
constexpr char        kHUSTR_KEYGREEN  = 'g';
constexpr char        kHUSTR_KEYINDIGO = 'i';
constexpr char        kHUSTR_KEYBROWN  = 'b';
constexpr char        kHUSTR_KEYRED    = 'r';

//
//  AM_map.C
//

constexpr const char *kAMSTR_FOLLOWON     = "Follow Mode ON";
constexpr const char *kAMSTR_FOLLOWOFF    = "Follow Mode OFF";
constexpr const char *kAMSTR_GRIDON       = "Grid ON";
constexpr const char *kAMSTR_GRIDOFF      = "Grid OFF";
constexpr const char *kAMSTR_MARKEDSPOT   = "Marked Spot";
constexpr const char *kAMSTR_MARKSCLEARED = "All Marks Cleared";

//
//  ST_stuff.C
//

constexpr const char *kSTSTR_MUS      = "Music Change";
constexpr const char *kSTSTR_NOMUS    = "IMPOSSIBLE SELECTION";
constexpr const char *kSTSTR_DQDON    = "Degreelessness Mode On";
constexpr const char *kSTSTR_DQDOFF   = "Degreelessness Mode Off";
constexpr const char *kSTSTR_KFAADDED = "Very Happy Ammo Added";
constexpr const char *kSTSTR_FAADDED  = "Ammo (no keys) Added";
constexpr const char *kSTSTR_NCON     = "No Clipping Mode ON";
constexpr const char *kSTSTR_NCOFF    = "No Clipping Mode OFF";
constexpr const char *kSTSTR_BEHOLD   = "inVuln, Str, Inviso, Rad, Allmap, or Lite-amp";
constexpr const char *kSTSTR_BEHOLDX  = "Power-up Toggled";
constexpr const char *kSTSTR_CHOPPERS = "... doesn't suck - GM";
constexpr const char *kSTSTR_CLEV     = "Changing Level...";

//
//  F_Finale.C
//
constexpr const char *kE1TEXT = "Once you beat the big badasses and\n"
                                "clean out the moon base you're supposed\n"
                                "to win, aren't you? Aren't you? Where's\n"
                                "your fat reward and ticket home? What\n"
                                "the hell is this? It's not supposed to\n"
                                "end this way!\n"
                                "\n"
                                "It stinks like rotten meat, but looks\n"
                                "like the lost Deimos base.  Looks like\n"
                                "you're stuck on The Shores of Hell.\n"
                                "The only way out is through.\n"
                                "\n"
                                "To continue the DOOM experience, play\n"
                                "The Shores of Hell and its amazing\n"
                                "sequel, Inferno!\n";

constexpr const char *kE2TEXT = "You've done it! The hideous cyber-\n"
                                "demon lord that ruled the lost Deimos\n"
                                "moon base has been slain and you\n"
                                "are triumphant! But ... where are\n"
                                "you? You clamber to the edge of the\n"
                                "moon and look down to see the awful\n"
                                "truth.\n"
                                "\n"
                                "Deimos floats above Hell itself!\n"
                                "You've never heard of anyone escaping\n"
                                "from Hell, but you'll make the bastards\n"
                                "sorry they ever heard of you! Quickly,\n"
                                "you rappel down to  the surface of\n"
                                "Hell.\n"
                                "\n"
                                "Now, it's on to the final chapter of\n"
                                "DOOM! -- Inferno.";

constexpr const char *kE3TEXT = "The loathsome spiderdemon that\n"
                                "masterminded the invasion of the moon\n"
                                "bases and caused so much death has had\n"
                                "its ass kicked for all time.\n"
                                "\n"
                                "A hidden doorway opens and you enter.\n"
                                "You've proven too tough for Hell to\n"
                                "contain, and now Hell at last plays\n"
                                "fair -- for you emerge from the door\n"
                                "to see the green fields of Earth!\n"
                                "Home at last.\n"
                                "\n"
                                "You wonder what's been happening on\n"
                                "Earth while you were battling evil\n"
                                "unleashed. It's good that no Hell-\n"
                                "spawn could have come through that\n"
                                "door with you ...";

constexpr const char *kE4TEXT = "the spider mastermind must have sent forth\n"
                                "its legions of hellspawn before your\n"
                                "final confrontation with that terrible\n"
                                "beast from hell.  but you stepped forward\n"
                                "and brought forth eternal damnation and\n"
                                "suffering upon the horde as a true hero\n"
                                "would in the face of something so evil.\n"
                                "\n"
                                "besides, someone was gonna pay for what\n"
                                "happened to daisy, your pet rabbit.\n"
                                "\n"
                                "but now, you see spread before you more\n"
                                "potential pain and gibbitude as a nation\n"
                                "of demons run amok among our cities.\n"
                                "\n"
                                "next stop, hell on earth!";

// after level 6, put this:

constexpr const char *kC1TEXT = "YOU HAVE ENTERED DEEPLY INTO THE INFESTED\n"
                                "STARPORT. BUT SOMETHING IS WRONG. THE\n"
                                "MONSTERS HAVE BROUGHT THEIR OWN REALITY\n"
                                "WITH THEM, AND THE STARPORT'S TECHNOLOGY\n"
                                "IS BEING SUBVERTED BY THEIR PRESENCE.\n"
                                "\n"
                                "AHEAD, YOU SEE AN OUTPOST OF HELL, A\n"
                                "FORTIFIED ZONE. IF YOU CAN GET PAST IT,\n"
                                "YOU CAN PENETRATE INTO THE HAUNTED HEART\n"
                                "OF THE STARBASE AND FIND THE CONTROLLING\n"
                                "SWITCH WHICH HOLDS EARTH'S POPULATION\n"
                                "HOSTAGE.";

// After level 11, put this:

constexpr const char *kC2TEXT = "YOU HAVE WON! YOUR VICTORY HAS ENABLED\n"
                                "HUMANKIND TO EVACUATE EARTH AND ESCAPE\n"
                                "THE NIGHTMARE.  NOW YOU ARE THE ONLY\n"
                                "HUMAN LEFT ON THE FACE OF THE PLANET.\n"
                                "CANNIBAL MUTATIONS, CARNIVOROUS ALIENS,\n"
                                "AND EVIL SPIRITS ARE YOUR ONLY NEIGHBORS.\n"
                                "YOU SIT BACK AND WAIT FOR DEATH, CONTENT\n"
                                "THAT YOU HAVE SAVED YOUR SPECIES.\n"
                                "\n"
                                "BUT THEN, EARTH CONTROL BEAMS DOWN A\n"
                                "MESSAGE FROM SPACE: \"SENSORS HAVE LOCATED\n"
                                "THE SOURCE OF THE ALIEN INVASION. IF YOU\n"
                                "GO THERE, YOU MAY BE ABLE TO BLOCK THEIR\n"
                                "ENTRY.  THE ALIEN BASE IS IN THE HEART OF\n"
                                "YOUR OWN HOME CITY, NOT FAR FROM THE\n"
                                "STARPORT.\" SLOWLY AND PAINFULLY YOU GET\n"
                                "UP AND RETURN TO THE FRAY.";

// After level 20, put this:

constexpr const char *kC3TEXT = "YOU ARE AT THE CORRUPT HEART OF THE CITY,\n"
                                "SURROUNDED BY THE CORPSES OF YOUR ENEMIES.\n"
                                "YOU SEE NO WAY TO DESTROY THE CREATURES'\n"
                                "ENTRYWAY ON THIS SIDE, SO YOU CLENCH YOUR\n"
                                "TEETH AND PLUNGE THROUGH IT.\n"
                                "\n"
                                "THERE MUST BE A WAY TO CLOSE IT ON THE\n"
                                "OTHER SIDE. WHAT DO YOU CARE IF YOU'VE\n"
                                "GOT TO GO THROUGH HELL TO GET TO IT?";

// After level 29, put this:

constexpr const char *kC4TEXT = "THE HORRENDOUS VISAGE OF THE BIGGEST\n"
                                "DEMON YOU'VE EVER SEEN CRUMBLES BEFORE\n"
                                "YOU, AFTER YOU PUMP YOUR ROCKETS INTO\n"
                                "HIS EXPOSED BRAIN. THE MONSTER SHRIVELS\n"
                                "UP AND DIES, ITS THRASHING LIMBS\n"
                                "DEVASTATING UNTOLD MILES OF HELL'S\n"
                                "SURFACE.\n"
                                "\n"
                                "YOU'VE DONE IT. THE INVASION IS OVER.\n"
                                "EARTH IS SAVED. HELL IS A WRECK. YOU\n"
                                "WONDER WHERE BAD FOLKS WILL GO WHEN THEY\n"
                                "DIE, NOW. WIPING THE SWEAT FROM YOUR\n"
                                "FOREHEAD YOU BEGIN THE LONG TREK BACK\n"
                                "HOME. REBUILDING EARTH OUGHT TO BE A\n"
                                "LOT MORE FUN THAN RUINING IT WAS.\n";

// Before level 31, put this:

constexpr const char *kC5TEXT = "CONGRATULATIONS, YOU'VE FOUND THE SECRET\n"
                                "LEVEL! LOOKS LIKE IT'S BEEN BUILT BY\n"
                                "HUMANS, RATHER THAN DEMONS. YOU WONDER\n"
                                "WHO THE INMATES OF THIS CORNER OF HELL\n"
                                "WILL BE.";

// Before level 32, put this:

constexpr const char *kC6TEXT = "CONGRATULATIONS, YOU'VE FOUND THE\n"
                                "SUPER SECRET LEVEL!  YOU'D BETTER\n"
                                "BLAZE THROUGH THIS ONE!\n";

// after map 06

constexpr const char *kP1TEXT = "You gloat over the steaming carcass of the\n"
                                "Guardian.  With its death, you've wrested\n"
                                "the Accelerator from the stinking claws\n"
                                "of Hell.  You relax and glance around the\n"
                                "room.  Damn!  There was supposed to be at\n"
                                "least one working prototype, but you can't\n"
                                "see it. The demons must have taken it.\n"
                                "\n"
                                "You must find the prototype, or all your\n"
                                "struggles will have been wasted. Keep\n"
                                "moving, keep fighting, keep killing.\n"
                                "Oh yes, keep living, too.";

// after map 11

constexpr const char *kP2TEXT = "Even the deadly Arch-Vile labyrinth could\n"
                                "not stop you, and you've gotten to the\n"
                                "prototype Accelerator which is soon\n"
                                "efficiently and permanently deactivated.\n"
                                "\n"
                                "You're good at that kind of thing.";

// after map 20

constexpr const char *kP3TEXT = "You've bashed and battered your way into\n"
                                "the heart of the devil-hive.  Time for a\n"
                                "Search-and-Destroy mission, aimed at the\n"
                                "Gatekeeper, whose foul offspring is\n"
                                "cascading to Earth.  Yeah, he's bad. But\n"
                                "you know who's worse!\n"
                                "\n"
                                "Grinning evilly, you check your gear, and\n"
                                "get ready to give the bastard a little Hell\n"
                                "of your own making!";

// after map 30

constexpr const char *kP4TEXT = "The Gatekeeper's evil face is splattered\n"
                                "all over the place.  As its tattered corpse\n"
                                "collapses, an inverted Gate forms and\n"
                                "sucks down the shards of the last\n"
                                "prototype Accelerator, not to mention the\n"
                                "few remaining demons.  You're done. Hell\n"
                                "has gone back to pounding bad dead folks \n"
                                "instead of good live ones.  Remember to\n"
                                "tell your grandkids to put a rocket\n"
                                "launcher in your coffin. If you go to Hell\n"
                                "when you die, you'll need it for some\n"
                                "final cleaning-up ...";

// before map 31

constexpr const char *kP5TEXT = "You've found the second-hardest level we\n"
                                "got. Hope you have a saved game a level or\n"
                                "two previous.  If not, be prepared to die\n"
                                "aplenty. For master marines only.";

// before map 32

constexpr const char *kP6TEXT = "Betcha wondered just what WAS the hardest\n"
                                "level we had ready for ya?  Now you know.\n"
                                "No one gets out alive.";

constexpr const char *kT1TEXT = "You've fought your way out of the infested\n"
                                "experimental labs.   It seems that UAC has\n"
                                "once again gulped it down.  With their\n"
                                "high turnover, it must be hard for poor\n"
                                "old UAC to buy corporate health insurance\n"
                                "nowadays..\n"
                                "\n"
                                "Ahead lies the military complex, now\n"
                                "swarming with diseased horrors hot to get\n"
                                "their teeth into you. With luck, the\n"
                                "complex still has some warlike ordnance\n"
                                "laying around.";

constexpr const char *kT2TEXT = "You hear the grinding of heavy machinery\n"
                                "ahead.  You sure hope they're not stamping\n"
                                "out new hellspawn, but you're ready to\n"
                                "ream out a whole herd if you have to.\n"
                                "They might be planning a blood feast, but\n"
                                "you feel about as mean as two thousand\n"
                                "maniacs packed into one mad killer.\n"
                                "\n"
                                "You don't plan to go down easy.";

constexpr const char *kT3TEXT = "The vista opening ahead looks real damn\n"
                                "familiar. Smells familiar, too -- like\n"
                                "fried excrement. You didn't like this\n"
                                "place before, and you sure as hell ain't\n"
                                "planning to like it now. The more you\n"
                                "brood on it, the madder you get.\n"
                                "Hefting your gun, an evil grin trickles\n"
                                "onto your face. Time to take some names.";

constexpr const char *kT4TEXT = "Suddenly, all is silent, from one horizon\n"
                                "to the other. The agonizing echo of Hell\n"
                                "fades away, the nightmare sky turns to\n"
                                "blue, the heaps of monster corpses start \n"
                                "to evaporate along with the evil stench \n"
                                "that filled the air. Jeeze, maybe you've\n"
                                "done it. Have you really won?\n"
                                "\n"
                                "Something rumbles in the distance.\n"
                                "A blue light begins to glow inside the\n"
                                "ruined skull of the demon-spitter.";

constexpr const char *kT5TEXT = "What now? Looks totally different. Kind\n"
                                "of like King Tut's condo. Well,\n"
                                "whatever's here can't be any worse\n"
                                "than usual. Can it?  Or maybe it's best\n"
                                "to let sleeping gods lie..";

constexpr const char *kT6TEXT = "Time for a vacation. You've burst the\n"
                                "bowels of hell and by golly you're ready\n"
                                "for a break. You mutter to yourself,\n"
                                "Maybe someone else can kick Hell's ass\n"
                                "next time around. Ahead lies a quiet town,\n"
                                "with peaceful flowing water, quaint\n"
                                "buildings, and presumably no Hellspawn.\n"
                                "\n"
                                "As you step off the transport, you hear\n"
                                "the stomp of a cyberdemon's iron shoe.";

//
// Character cast strings F_FINALE.C
//
constexpr const char *kCC_ZOMBIE  = "ZOMBIEMAN";
constexpr const char *kCC_SHOTGUN = "SHOTGUN GUY";
constexpr const char *kCC_HEAVY   = "HEAVY WEAPON DUDE";
constexpr const char *kCC_IMP     = "IMP";
constexpr const char *kCC_DEMON   = "DEMON";
constexpr const char *kCC_LOST    = "LOST SOUL";
constexpr const char *kCC_CACO    = "CACODEMON";
constexpr const char *kCC_HELL    = "HELL KNIGHT";
constexpr const char *kCC_BARON   = "BARON OF HELL";
constexpr const char *kCC_ARACH   = "ARACHNOTRON";
constexpr const char *kCC_PAIN    = "PAIN ELEMENTAL";
constexpr const char *kCC_REVEN   = "REVENANT";
constexpr const char *kCC_MANCU   = "MANCUBUS";
constexpr const char *kCC_ARCH    = "ARCH-VILE";
constexpr const char *kCC_SPIDER  = "THE SPIDER MASTERMIND";
constexpr const char *kCC_CYBER   = "THE CYBERDEMON";
constexpr const char *kCC_HERO    = "OUR HERO";

// Obituaries (not strictly BEX, but Freedoom 1/2 use them)
constexpr const char *kOB_BABY          = "%o was melted by an Arachnotron.";
constexpr const char *kOB_VILE          = "%o was charred by an Archvile.";
constexpr const char *kOB_BARON         = "%o was killed by a Baron of Hell.";
constexpr const char *kOB_BARONHIT      = "%o was clawed by a Baron of Hell.";
constexpr const char *kOB_KNIGHT         = "%o was killed by a Hell Knight.";
constexpr const char *kOB_KNIGHTHIT      = "%o was clawed by a Hell Knight.";
constexpr const char *kOB_CACOHIT       = "A Cacodemon chewed up %o!";
constexpr const char *kOB_CACO          = "%o was killed by a Cacodemon.";
constexpr const char *kOB_CHAINGUY      = "%o was mowed down by a Chaingunner.";
constexpr const char *kOB_CYBORG        = "%o was blown to bits by a Cyberdemon.";
constexpr const char *kOB_DOG           = "%o was bitten by a Dog.";
constexpr const char *kOB_SPIDER        = "A Spider Mastermind ripped %o apart!";
constexpr const char *kOB_WOLFSS        = "%o was executed by a Nazi.";
constexpr const char *kOB_DEMONHIT      = "%o was mauled by a Demon.";
constexpr const char *kOB_IMP           = "%o was killed by an Imp.";
constexpr const char *kOB_IMPHIT        = "An Imp tore %o a new one!";
constexpr const char *kOB_FATSO         = "%o was killed by a Mancubus.";
constexpr const char *kOB_UNDEAD        = "%o was killed by a Revenant.";
constexpr const char *kOB_UNDEADHIT     = "A revenant knocked %o's head off.";
constexpr const char *kOB_SHOTGUY       = "%o was shot by a Shotgun Sergeant.";
constexpr const char *kOB_SKULL         = "%o was eaten by a Lost Soul!";
constexpr const char *kOB_ZOMBIE        = "%o was gunned down by a Zombie.";
constexpr const char *kOB_MPCHAINGUN    = "%o was torn apart by %k.";
constexpr const char *kOB_MPPISTOL      = "%o was gunned down by %k.";
constexpr const char *kOB_MPROCKET      = "%o was blown up by %k.";
constexpr const char *kOB_MPR_SPLASH    = "%o was blown up by %k.";
constexpr const char *kOB_MPPLASMARIFLE = "%o was melted by %k.";
constexpr const char *kOB_MPFIST        = "%k punched %o's lights out.";
constexpr const char *kOB_MPCHAINSAW    = "%k sawed %o's head off.";
constexpr const char *kOB_MPSHOTGUN     = "%o was shot by %k.";
constexpr const char *kOB_MPBFG_BOOM    = "%k annihilated %o.";
constexpr const char *kOB_MPBFG_SPLASH  = "%k annihilated %o.";
constexpr const char *kOB_MPBFG_MBF     = "%k annihilated %o.";

//------------------------------------------------------------------------
//
// -AJA- Other common strings
//

constexpr const char *kX_COMMERC = "\tcommercial version.\n";
constexpr const char *kX_REGIST  = "\tregistered version.\n";

constexpr const char *kX_TITLE1 = "                         The Ultimate DOOM Startup v%i.%i                 "
                                  "       ";
constexpr const char *kX_TITLE2 = "                          DOOM System Startup v%i.%i                      "
                                  "    ";
constexpr const char *kX_TITLE3 = "                         DOOM 2: Hell on Earth v%i.%i                     "
                                  "      ";

constexpr const char *kX_NODIST1 = "=========================================================================="
                                   "=\n"
                                   "             This version is NOT SHAREWARE, do not distribute!\n"
                                   "         Please report software piracy to the SPA: 1-800-388-PIR8\n"
                                   "=========================================================================="
                                   "=\n";

constexpr const char *kX_NODIST2 = "=========================================================================="
                                   "=\n"
                                   "                            Do not distribute!\n"
                                   "         Please report software piracy to the SPA: 1-800-388-PIR8\n"
                                   "=========================================================================="
                                   "=\n";

constexpr const char *kX_MODIFIED = "=========================================================================="
                                    "=\n"
                                    "ATTENTION:  This version of DOOM has been modified.  If you would like "
                                    "to\n"
                                    "get a copy of the original game, call 1-800-IDGAMES or see the readme "
                                    "file.\n"
                                    "        You will not receive technical support for modified games.\n"
                                    "                      press enter to continue\n"
                                    "=========================================================================="
                                    "=\n";

} // namespace dehacked