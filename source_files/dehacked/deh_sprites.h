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

#pragma once

namespace dehacked
{

// This file diverges slightly from the style guide with enum member naming
// as these reflect the historical code pointer/state/flag/etc names - Dasho

enum SpriteNumber
{
    kSPR_TROO,
    kSPR_SHTG,
    kSPR_PUNG,
    kSPR_PISG,
    kSPR_PISF,
    kSPR_SHTF,
    kSPR_SHT2,
    kSPR_CHGG,
    kSPR_CHGF,
    kSPR_MISG,
    kSPR_MISF,
    kSPR_SAWG,
    kSPR_PLSG,
    kSPR_PLSF,
    kSPR_BFGG,
    kSPR_BFGF,
    kSPR_BLUD,
    kSPR_PUFF,
    kSPR_BAL1,
    kSPR_BAL2,
    kSPR_PLSS,
    kSPR_PLSE,
    kSPR_MISL,
    kSPR_BFS1,
    kSPR_BFE1,
    kSPR_BFE2,
    kSPR_TFOG,
    kSPR_IFOG,
    kSPR_PLAY,
    kSPR_POSS,
    kSPR_SPOS,
    kSPR_VILE,
    kSPR_FIRE,
    kSPR_FATB,
    kSPR_FBXP,
    kSPR_SKEL,
    kSPR_MANF,
    kSPR_FATT,
    kSPR_CPOS,
    kSPR_SARG,
    kSPR_HEAD,
    kSPR_BAL7,
    kSPR_BOSS,
    kSPR_BOS2,
    kSPR_SKUL,
    kSPR_SPID,
    kSPR_BSPI,
    kSPR_APLS,
    kSPR_APBX,
    kSPR_CYBR,
    kSPR_PAIN,
    kSPR_SSWV,
    kSPR_KEEN,
    kSPR_BBRN,
    kSPR_BOSF,
    kSPR_ARM1,
    kSPR_ARM2,
    kSPR_BAR1,
    kSPR_BEXP,
    kSPR_FCAN,
    kSPR_BON1,
    kSPR_BON2,
    kSPR_BKEY,
    kSPR_RKEY,
    kSPR_YKEY,
    kSPR_BSKU,
    kSPR_RSKU,
    kSPR_YSKU,
    kSPR_STIM,
    kSPR_MEDI,
    kSPR_SOUL,
    kSPR_PINV,
    kSPR_PSTR,
    kSPR_PINS,
    kSPR_MEGA,
    kSPR_SUIT,
    kSPR_PMAP,
    kSPR_PVIS,
    kSPR_CLIP,
    kSPR_AMMO,
    kSPR_ROCK,
    kSPR_BROK,
    kSPR_CELL,
    kSPR_CELP,
    kSPR_SHEL,
    kSPR_SBOX,
    kSPR_BPAK,
    kSPR_BFUG,
    kSPR_MGUN,
    kSPR_CSAW,
    kSPR_LAUN,
    kSPR_PLAS,
    kSPR_SHOT,
    kSPR_SGN2,
    kSPR_COLU,
    kSPR_SMT2,
    kSPR_GOR1,
    kSPR_POL2,
    kSPR_POL5,
    kSPR_POL4,
    kSPR_POL3,
    kSPR_POL1,
    kSPR_POL6,
    kSPR_GOR2,
    kSPR_GOR3,
    kSPR_GOR4,
    kSPR_GOR5,
    kSPR_SMIT,
    kSPR_COL1,
    kSPR_COL2,
    kSPR_COL3,
    kSPR_COL4,
    kSPR_CAND,
    kSPR_CBRA,
    kSPR_COL6,
    kSPR_TRE1,
    kSPR_TRE2,
    kSPR_ELEC,
    kSPR_CEYE,
    kSPR_FSKU,
    kSPR_COL5,
    kSPR_TBLU,
    kSPR_TGRN,
    kSPR_TRED,
    kSPR_SMBT,
    kSPR_SMGT,
    kSPR_SMRT,
    kSPR_HDB1,
    kSPR_HDB2,
    kSPR_HDB3,
    kSPR_HDB4,
    kSPR_HDB5,
    kSPR_HDB6,
    kSPR_POB1,
    kSPR_POB2,
    kSPR_BRS1,
    kSPR_TLMP,
    kSPR_TLP2,

    kTotalSprites,

    // BOOM/MBF/Doom Retro sprites:
    kSPR_TNT1 = kTotalSprites,
    kSPR_DOGS,
    kSPR_PLS1,
    kSPR_PLS2,
    kSPR_BON3,
    kSPR_BON4,
    kSPR_BLD2,

    kTotalSpritesMBF,

    // DEHEXTRA: 100 additional sprites
    kTotalSpritesDEHEXTRA = kTotalSpritesMBF + 100
};

namespace sprites
{
void Init();
void Shutdown();

// returns true if the string was found.
bool ReplaceSprite(const char *before, const char *after);

void AlterBexSprite(const char *new_val);

const char *GetSprite(int spr_num);
const char *GetOriginalName(int spr_num);

void SpriteDependencies();
} // namespace sprites

} // namespace dehacked