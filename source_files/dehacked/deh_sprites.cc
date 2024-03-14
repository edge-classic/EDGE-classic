//------------------------------------------------------------------------
//  SPRITES
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

#include "deh_sprites.h"

#include <stdlib.h>
#include <string.h>

#include "deh_buffer.h"
#include "deh_edge.h"
#include "deh_frames.h"
#include "deh_patch.h"
#include "deh_sounds.h"
#include "deh_system.h"
#include "deh_text.h"
#include "deh_wad.h"
#include "epi.h"
#include "str_compare.h"
#include "str_util.h"
namespace dehacked
{

const char *sprnames_orig[kTotalSpritesDEHEXTRA] = {
    "TROO", "SHTG", "PUNG", "PISG", "PISF", "SHTF", "SHT2", "CHGG", "CHGF",
    "MISG", "MISF", "SAWG", "PLSG", "PLSF", "BFGG", "BFGF", "BLUD", "PUFF",
    "BAL1", "BAL2", "PLSS", "PLSE", "MISL", "BFS1", "BFE1", "BFE2", "TFOG",
    "IFOG", "PLAY", "POSS", "SPOS", "VILE", "FIRE", "FATB", "FBXP", "SKEL",
    "MANF", "FATT", "CPOS", "SARG", "HEAD", "BAL7", "BOSS", "BOS2", "SKUL",
    "SPID", "BSPI", "APLS", "APBX", "CYBR", "PAIN", "SSWV", "KEEN", "BBRN",
    "BOSF", "ARM1", "ARM2", "BAR1", "BEXP", "FCAN", "BON1", "BON2", "BKEY",
    "RKEY", "YKEY", "BSKU", "RSKU", "YSKU", "STIM", "MEDI", "SOUL", "PINV",
    "PSTR", "PINS", "MEGA", "SUIT", "PMAP", "PVIS", "CLIP", "AMMO", "ROCK",
    "BROK", "CELL", "CELP", "SHEL", "SBOX", "BPAK", "BFUG", "MGUN", "CSAW",
    "LAUN", "PLAS", "SHOT", "SGN2", "COLU", "SMT2", "GOR1", "POL2", "POL5",
    "POL4", "POL3", "POL1", "POL6", "GOR2", "GOR3", "GOR4", "GOR5", "SMIT",
    "COL1", "COL2", "COL3", "COL4", "CAND", "CBRA", "COL6", "TRE1", "TRE2",
    "ELEC", "CEYE", "FSKU", "COL5", "TBLU", "TGRN", "TRED", "SMBT", "SMGT",
    "SMRT", "HDB1", "HDB2", "HDB3", "HDB4", "HDB5", "HDB6", "POB1", "POB2",
    "BRS1", "TLMP", "TLP2",

    // BOOM/MBF/Doom Retro:
    "TNT1", "DOGS", "PLS1", "PLS2", "BON3", "BON4", "BLD2",

    // DEHEXTRA sprites:
    "SP00", "SP01", "SP02", "SP03", "SP04", "SP05", "SP06", "SP07", "SP08",
    "SP09", "SP10", "SP11", "SP12", "SP13", "SP14", "SP15", "SP16", "SP17",
    "SP18", "SP19", "SP20", "SP21", "SP22", "SP23", "SP24", "SP25", "SP26",
    "SP27", "SP28", "SP29", "SP30", "SP31", "SP32", "SP33", "SP34", "SP35",
    "SP36", "SP37", "SP38", "SP39", "SP40", "SP41", "SP42", "SP43", "SP44",
    "SP45", "SP46", "SP47", "SP48", "SP49", "SP50", "SP51", "SP52", "SP53",
    "SP54", "SP55", "SP56", "SP57", "SP58", "SP59", "SP60", "SP61", "SP62",
    "SP63", "SP64", "SP65", "SP66", "SP67", "SP68", "SP69", "SP70", "SP71",
    "SP72", "SP73", "SP74", "SP75", "SP76", "SP77", "SP78", "SP79", "SP80",
    "SP81", "SP82", "SP83", "SP84", "SP85", "SP86", "SP87", "SP88", "SP89",
    "SP90", "SP91", "SP92", "SP93", "SP94", "SP95", "SP96", "SP97", "SP98",
    "SP99"};

// elements here can be "" for unmodified names
std::vector<std::string> sprnames;

//------------------------------------------------------------------------

namespace sprites
{
void MarkEntry(int num);
}

void sprites::Init() { sprnames.clear(); }

void sprites::Shutdown() { sprnames.clear(); }

void sprites::MarkEntry(int num)
{
    // fill any missing slots with "", including the one we want.
    while ((int)sprnames.size() < num + 1) sprnames.push_back("");

    // for the modified sprite, copy the original name
    if (sprnames[num].empty()) sprnames[num] = GetOriginalName(num);
}

void sprites::SpriteDependencies()
{
    for (size_t i = 0; i < sprnames.size(); i++)
    {
        if (sprnames[i] != "" && sprnames[i] != GetOriginalName(i))
        {
            frames::MarkStatesWithSprite((int)i);
        }
    }
}

bool sprites::ReplaceSprite(const char *before, const char *after)
{
    EPI_ASSERT(strlen(before) == 4);
    EPI_ASSERT(strlen(after) == 4);

    for (int i = 0; i < kTotalSpritesDEHEXTRA; i++)
    {
        if (epi::StringCaseCompareASCII(before, sprnames_orig[i]) == 0)
        {
            MarkEntry(i);

            sprnames[i] = after;
            return true;
        }
    }

    return false;
}

void sprites::AlterBexSprite(const char *new_val)
{
    const char *old_val = patch::line_buf;

    if (strlen(new_val) != 4)
    {
        LogDebug("Dehacked: Warning - Bad length for sprite name '%s'.\n",
                 new_val);
        return;
    }

    // for DSDehacked, support a numeric target
    if (epi::IsDigitASCII(old_val[0]))
    {
        int num = atoi(old_val);
        if (num < 0 || num > 32767)
        {
            LogDebug(
                "Dehacked: Warning - Line %d: illegal sprite entry '%s'.\n",
                patch::line_num, old_val);
        }
        else
        {
            MarkEntry(num);
            sprnames[num] = new_val;
        }
        return;
    }

    if (strlen(old_val) != 4)
    {
        LogDebug("Dehacked: Warning - Bad length for sprite name '%s'.\n",
                 old_val);
        return;
    }

    if (!ReplaceSprite(old_val, new_val))
        LogDebug("Dehacked: Warning - Line %d: unknown sprite name '%s'.\n",
                 patch::line_num, old_val);
}

const char *sprites::GetSprite(int spr_num)
{
    if (spr_num < 0 || spr_num > 32767) return "XXXX";

    const char *name = "";

    if (spr_num < (int)sprnames.size()) name = sprnames[spr_num].c_str();

    if (strlen(name) == 0) name = GetOriginalName(spr_num);

    // Boom support: TNT1 is an invisible sprite
    if (epi::StringCaseCompareASCII(name, "TNT1") == 0) return "NULL";

    return name;
}

const char *sprites::GetOriginalName(int spr_num)
{
    if (spr_num < kTotalSpritesDEHEXTRA) return sprnames_orig[spr_num];

    return "NULL";
}

}  // namespace dehacked
