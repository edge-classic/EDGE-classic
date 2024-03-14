//------------------------------------------------------------------------
//  SOUND Definitions
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

#include <string>

namespace dehacked
{

// This file diverges slightly from the style guide with enum member naming
// as these reflect the historical code pointer/state/flag/etc names - Dasho

enum SoundEffectType
{
    ksfx_None,
    ksfx_pistol,
    ksfx_shotgn,
    ksfx_sgcock,
    ksfx_dshtgn,
    ksfx_dbopn,
    ksfx_dbcls,
    ksfx_dbload,
    ksfx_plasma,
    ksfx_bfg,
    ksfx_sawup,
    ksfx_sawidl,
    ksfx_sawful,
    ksfx_sawhit,
    ksfx_rlaunc,
    ksfx_rxplod,
    ksfx_firsht,
    ksfx_firxpl,
    ksfx_pstart,
    ksfx_pstop,
    ksfx_doropn,
    ksfx_dorcls,
    ksfx_stnmov,
    ksfx_swtchn,
    ksfx_swtchx,
    ksfx_plpain,
    ksfx_dmpain,
    ksfx_popain,
    ksfx_vipain,
    ksfx_mnpain,
    ksfx_pepain,
    ksfx_slop,
    ksfx_itemup,
    ksfx_wpnup,
    ksfx_oof,
    ksfx_telept,
    ksfx_posit1,
    ksfx_posit2,
    ksfx_posit3,
    ksfx_bgsit1,
    ksfx_bgsit2,
    ksfx_sgtsit,
    ksfx_cacsit,
    ksfx_brssit,
    ksfx_cybsit,
    ksfx_spisit,
    ksfx_bspsit,
    ksfx_kntsit,
    ksfx_vilsit,
    ksfx_mansit,
    ksfx_pesit,
    ksfx_sklatk,
    ksfx_sgtatk,
    ksfx_skepch,
    ksfx_vilatk,
    ksfx_claw,
    ksfx_skeswg,
    ksfx_pldeth,
    ksfx_pdiehi,
    ksfx_podth1,
    ksfx_podth2,
    ksfx_podth3,
    ksfx_bgdth1,
    ksfx_bgdth2,
    ksfx_sgtdth,
    ksfx_cacdth,
    ksfx_skldth,
    ksfx_brsdth,
    ksfx_cybdth,
    ksfx_spidth,
    ksfx_bspdth,
    ksfx_vildth,
    ksfx_kntdth,
    ksfx_pedth,
    ksfx_skedth,
    ksfx_posact,
    ksfx_bgact,
    ksfx_dmact,
    ksfx_bspact,
    ksfx_bspwlk,
    ksfx_vilact,
    ksfx_noway,
    ksfx_barexp,
    ksfx_punch,
    ksfx_hoof,
    ksfx_metal,
    ksfx_chgun,
    ksfx_tink,
    ksfx_bdopn,
    ksfx_bdcls,
    ksfx_itmbk,
    ksfx_flame,
    ksfx_flamst,
    ksfx_getpow,
    ksfx_bospit,
    ksfx_boscub,
    ksfx_bossit,
    ksfx_bospn,
    ksfx_bosdth,
    ksfx_manatk,
    ksfx_mandth,
    ksfx_sssit,
    ksfx_ssdth,
    ksfx_keenpn,
    ksfx_keendt,
    ksfx_skeact,
    ksfx_skesit,
    ksfx_skeatk,
    ksfx_radio,

    kTotalSoundEffects,

    // MBF sounds:
    ksfx_dgsit = kTotalSoundEffects,
    ksfx_dgatk,
    ksfx_dgact,
    ksfx_dgdth,
    ksfx_dgpain,

    kTotalSoundEffectsMBF,

    // other source ports:
    ksfx_secret = kTotalSoundEffectsMBF,
    ksfx_gibdth,
    ksfx_scrsht,

    kTotalSoundEffectsPortCompatibility,

    // Note: there is a big gap here until the DEHEXTRA sounds...

    // DEHEXTRA: 200 additional sounds
    ksfx_fre000 = 500,
    ksfx_fre199 = 699,

    kTotalSoundEffectsDEHEXTRA
};

namespace sounds
{
void Init();
void Shutdown();

// this returns true if the string was found.
bool ReplaceSound(const char *before, const char *after);

void AlterBexSound(const char *new_val);

void MarkSound(int s_num);
void AlterSound(int new_val);

const char *GetSound(int sound_id);

void ConvertSFX(void);
}  // namespace sounds

}  // namespace dehacked