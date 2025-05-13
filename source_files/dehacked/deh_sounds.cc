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
//
//  DEH_EDGE is based on:
//
//  +  DeHackEd source code, by Greg Lewis.
//  -  DOOM source code (C) 1993-1996 id Software, Inc.
//  -  Linux DOOM Hack Editor, by Sam Lantinga.
//  -  PrBoom's DEH/BEX code, by Ty Halderman, TeamTNT.
//
//------------------------------------------------------------------------

#include "deh_sounds.h"

#include <stdlib.h>
#include <string.h>

#include "deh_buffer.h"
#include "deh_edge.h"
#include "deh_patch.h"
#include "deh_system.h"
#include "deh_wad.h"
#include "epi.h"
#include "epi_str_compare.h"
#include "epi_str_util.h"
#include "stb_sprintf.h"
namespace dehacked
{

struct SoundEffectInfo
{
    // up to 6-character name
    char name[8];

    // Sfx singularity (only one at a time), 0 = normal
    int singularity;

    // Sfx priority (lower is MORE important)
    int priority;
};

//------------------------------------------------------------------------
//
// Information about all the sfx
//

const SoundEffectInfo S_sfx_orig[kTotalSoundEffectsPortCompatibility] = {
    // S_sfx[0] needs to be a dummy for odd reasons.
    {"", 0, 127},

    {"pistol", 0, 64},
    {"shotgn", 0, 64},
    {"sgcock", 0, 64},
    {"dshtgn", 0, 64},
    {"dbopn", 0, 64},
    {"dbcls", 0, 64},
    {"dbload", 0, 64},
    {"plasma", 0, 64},
    {"bfg", 0, 64},
    {"sawup", 2, 64},
    {"sawidl", 2, 118},
    {"sawful", 2, 64},
    {"sawhit", 2, 64},
    {"rlaunc", 0, 64},
    {"rxplod", 0, 70},
    {"firsht", 0, 70},
    {"firxpl", 0, 70},
    {"pstart", 18, 100},
    {"pstop", 18, 100},
    {"doropn", 0, 100},
    {"dorcls", 0, 100},
    {"stnmov", 18, 119},
    {"swtchn", 0, 78},
    {"swtchx", 0, 78},
    {"plpain", 0, 96},
    {"dmpain", 0, 96},
    {"popain", 0, 96},
    {"vipain", 0, 96},
    {"mnpain", 0, 96},
    {"pepain", 0, 96},
    {"slop", 0, 78},
    {"itemup", 20, 78},
    {"wpnup", 21, 78},
    {"oof", 0, 96},
    {"telept", 0, 32},
    {"posit1", 3, 98},
    {"posit2", 3, 98},
    {"posit3", 3, 98},
    {"bgsit1", 4, 98},
    {"bgsit2", 4, 98},
    {"sgtsit", 5, 98},
    {"cacsit", 6, 98},
    {"brssit", 7, 94},
    {"cybsit", 8, 92},
    {"spisit", 9, 90},
    {"bspsit", 10, 90},
    {"kntsit", 11, 90},
    {"vilsit", 12, 90},
    {"mansit", 13, 90},
    {"pesit", 14, 90},
    {"sklatk", 0, 70},
    {"sgtatk", 0, 70},
    {"skepch", 0, 70},
    {"vilatk", 0, 70},
    {"claw", 0, 70},
    {"skeswg", 0, 70},
    {"pldeth", 0, 32},
    {"pdiehi", 0, 32},
    {"podth1", 0, 70},
    {"podth2", 0, 70},
    {"podth3", 0, 70},
    {"bgdth1", 0, 70},
    {"bgdth2", 0, 70},
    {"sgtdth", 0, 70},
    {"cacdth", 0, 70},
    {"skldth", 0, 70},
    {"brsdth", 0, 32},
    {"cybdth", 0, 32},
    {"spidth", 0, 32},
    {"bspdth", 0, 32},
    {"vildth", 0, 32},
    {"kntdth", 0, 32},
    {"pedth", 0, 32},
    {"skedth", 0, 32},
    {"posact", 3, 120},
    {"bgact", 4, 120},
    {"dmact", 15, 120},
    {"bspact", 10, 100},
    {"bspwlk", 16, 100},
    {"vilact", 12, 100},
    {"noway", 0, 78},
    {"barexp", 0, 60},
    {"punch", 0, 64},
    {"hoof", 0, 70},
    {"metal", 0, 70},
    {"chgun", 0, 64},
    {"tink", 0, 60},
    {"bdopn", 0, 100},
    {"bdcls", 0, 100},
    {"itmbk", 0, 100},
    {"flame", 0, 32},
    {"flamst", 0, 32},
    {"getpow", 0, 60},
    {"bospit", 0, 70},
    {"boscub", 0, 70},
    {"bossit", 0, 70},
    {"bospn", 0, 70},
    {"bosdth", 0, 70},
    {"manatk", 0, 70},
    {"mandth", 0, 70},
    {"sssit", 0, 70},
    {"ssdth", 0, 70},
    {"keenpn", 0, 70},
    {"keendt", 0, 70},
    {"skeact", 0, 70},
    {"skesit", 0, 70},
    {"skeatk", 0, 70},
    {"radio", 0, 60},

    // MBF sounds...
    {"dgsit", 0, 98},
    {"dgatk", 0, 70},
    {"dgact", 0, 120},
    {"dgdth", 0, 70},
    {"dgpain", 0, 96},

    // other source ports...
    {"secret", 0, 60},
    {"gibdth", 0, 60},
    {"scrsht", 0, 0},
};

// DEHEXTRA : 500 to 699
const SoundEffectInfo S_sfx_dehextra[200] = {
    {"fre000", 0, 127}, {"fre001", 0, 127}, {"fre002", 0, 127}, {"fre003", 0, 127}, {"fre004", 0, 127},
    {"fre005", 0, 127}, {"fre006", 0, 127}, {"fre007", 0, 127}, {"fre008", 0, 127}, {"fre009", 0, 127},
    {"fre010", 0, 127}, {"fre011", 0, 127}, {"fre012", 0, 127}, {"fre013", 0, 127}, {"fre014", 0, 127},
    {"fre015", 0, 127}, {"fre016", 0, 127}, {"fre017", 0, 127}, {"fre018", 0, 127}, {"fre019", 0, 127},
    {"fre020", 0, 127}, {"fre021", 0, 127}, {"fre022", 0, 127}, {"fre023", 0, 127}, {"fre024", 0, 127},
    {"fre025", 0, 127}, {"fre026", 0, 127}, {"fre027", 0, 127}, {"fre028", 0, 127}, {"fre029", 0, 127},

    {"fre030", 0, 127}, {"fre031", 0, 127}, {"fre032", 0, 127}, {"fre033", 0, 127}, {"fre034", 0, 127},
    {"fre035", 0, 127}, {"fre036", 0, 127}, {"fre037", 0, 127}, {"fre038", 0, 127}, {"fre039", 0, 127},
    {"fre040", 0, 127}, {"fre041", 0, 127}, {"fre042", 0, 127}, {"fre043", 0, 127}, {"fre044", 0, 127},
    {"fre045", 0, 127}, {"fre046", 0, 127}, {"fre047", 0, 127}, {"fre048", 0, 127}, {"fre049", 0, 127},
    {"fre050", 0, 127}, {"fre051", 0, 127}, {"fre052", 0, 127}, {"fre053", 0, 127}, {"fre054", 0, 127},
    {"fre055", 0, 127}, {"fre056", 0, 127}, {"fre057", 0, 127}, {"fre058", 0, 127}, {"fre059", 0, 127},

    {"fre060", 0, 127}, {"fre061", 0, 127}, {"fre062", 0, 127}, {"fre063", 0, 127}, {"fre064", 0, 127},
    {"fre065", 0, 127}, {"fre066", 0, 127}, {"fre067", 0, 127}, {"fre068", 0, 127}, {"fre069", 0, 127},
    {"fre070", 0, 127}, {"fre071", 0, 127}, {"fre072", 0, 127}, {"fre073", 0, 127}, {"fre074", 0, 127},
    {"fre075", 0, 127}, {"fre076", 0, 127}, {"fre077", 0, 127}, {"fre078", 0, 127}, {"fre079", 0, 127},
    {"fre080", 0, 127}, {"fre081", 0, 127}, {"fre082", 0, 127}, {"fre083", 0, 127}, {"fre084", 0, 127},
    {"fre085", 0, 127}, {"fre086", 0, 127}, {"fre087", 0, 127}, {"fre088", 0, 127}, {"fre089", 0, 127},

    {"fre090", 0, 127}, {"fre091", 0, 127}, {"fre092", 0, 127}, {"fre093", 0, 127}, {"fre094", 0, 127},
    {"fre095", 0, 127}, {"fre096", 0, 127}, {"fre097", 0, 127}, {"fre098", 0, 127}, {"fre099", 0, 127},
    {"fre100", 0, 127}, {"fre101", 0, 127}, {"fre102", 0, 127}, {"fre103", 0, 127}, {"fre104", 0, 127},
    {"fre105", 0, 127}, {"fre106", 0, 127}, {"fre107", 0, 127}, {"fre108", 0, 127}, {"fre109", 0, 127},
    {"fre110", 0, 127}, {"fre111", 0, 127}, {"fre112", 0, 127}, {"fre113", 0, 127}, {"fre114", 0, 127},
    {"fre115", 0, 127}, {"fre116", 0, 127}, {"fre117", 0, 127}, {"fre118", 0, 127}, {"fre119", 0, 127},
    {"fre120", 0, 127}, {"fre121", 0, 127}, {"fre122", 0, 127},

    {"fre123", 0, 127}, {"fre124", 0, 127}, {"fre125", 0, 127}, {"fre126", 0, 127}, {"fre127", 0, 127},
    {"fre128", 0, 127}, {"fre129", 0, 127}, {"fre130", 0, 127}, {"fre131", 0, 127}, {"fre132", 0, 127},
    {"fre133", 0, 127}, {"fre134", 0, 127}, {"fre135", 0, 127}, {"fre136", 0, 127}, {"fre137", 0, 127},
    {"fre138", 0, 127}, {"fre139", 0, 127}, {"fre140", 0, 127}, {"fre141", 0, 127}, {"fre142", 0, 127},
    {"fre143", 0, 127}, {"fre144", 0, 127}, {"fre145", 0, 127}, {"fre146", 0, 127}, {"fre147", 0, 127},
    {"fre148", 0, 127}, {"fre149", 0, 127},

    {"fre150", 0, 127}, {"fre151", 0, 127}, {"fre152", 0, 127}, {"fre153", 0, 127}, {"fre154", 0, 127},
    {"fre155", 0, 127}, {"fre156", 0, 127}, {"fre157", 0, 127}, {"fre158", 0, 127}, {"fre159", 0, 127},
    {"fre160", 0, 127}, {"fre161", 0, 127}, {"fre162", 0, 127}, {"fre163", 0, 127}, {"fre164", 0, 127},
    {"fre165", 0, 127}, {"fre166", 0, 127}, {"fre167", 0, 127}, {"fre168", 0, 127}, {"fre169", 0, 127},
    {"fre170", 0, 127}, {"fre171", 0, 127}, {"fre172", 0, 127}, {"fre173", 0, 127}, {"fre174", 0, 127},
    {"fre175", 0, 127}, {"fre176", 0, 127}, {"fre177", 0, 127}, {"fre178", 0, 127}, {"fre179", 0, 127},

    {"fre180", 0, 127}, {"fre181", 0, 127}, {"fre182", 0, 127}, {"fre183", 0, 127}, {"fre184", 0, 127},
    {"fre185", 0, 127}, {"fre186", 0, 127}, {"fre187", 0, 127}, {"fre188", 0, 127}, {"fre189", 0, 127},
    {"fre190", 0, 127}, {"fre191", 0, 127}, {"fre192", 0, 127}, {"fre193", 0, 127}, {"fre194", 0, 127},
    {"fre195", 0, 127}, {"fre196", 0, 127}, {"fre197", 0, 127}, {"fre198", 0, 127}, {"fre199", 0, 127}};

//
// all the modified entries.
// NOTE: some pointers may be nullptr!
//
std::vector<SoundEffectInfo *> S_sfx;

//------------------------------------------------------------------------

namespace sounds
{
void BeginLump();
void FinishLump();

void                   MarkSound(int s_num);
const SoundEffectInfo *GetOriginalSFX(int num);
std::string            GetEdgeSfxName(int sound_id);
void                   WriteSound(int s_num);
} // namespace sounds

void sounds::Init()
{
    S_sfx.clear();
}

void sounds::Shutdown()
{
    for (size_t i = 0; i < S_sfx.size(); i++)
        if (S_sfx[i] != nullptr)
            delete S_sfx[i];

    S_sfx.clear();
}

void sounds::BeginLump()
{
    wad::NewLump(kDDFTypeSFX);

    wad::Printf("<SOUNDS>\n\n");
}

void sounds::FinishLump()
{
    wad::Printf("\n");
}

const SoundEffectInfo *sounds::GetOriginalSFX(int num)
{
    if (0 <= num && num < kTotalSoundEffectsPortCompatibility)
        return &S_sfx_orig[num];

    if (ksfx_fre000 <= num && num <= ksfx_fre199)
        return &S_sfx_dehextra[num - ksfx_fre000];

    // no actual original, return the dummy template
    return &S_sfx_orig[0];
}

void sounds::MarkSound(int num)
{
    // can happen since the binary patches contain the dummy sound
    if (num == ksfx_None)
        return;

    // fill any missing slots with nullptrs, including the one we want
    while ((int)S_sfx.size() < num + 1)
    {
        S_sfx.push_back(nullptr);
    }

    // already have a modified entry?
    if (S_sfx[num] != nullptr)
        return;

    SoundEffectInfo *entry = new SoundEffectInfo;
    S_sfx[num]             = entry;

    // copy the original info
    const SoundEffectInfo *orig = GetOriginalSFX(num);

    strcpy(entry->name, orig->name);

    entry->singularity = orig->singularity;
    entry->priority    = orig->priority;
}

void sounds::AlterSound(int new_val)
{
    int         s_num     = patch::active_obj;
    const char *deh_field = patch::line_buf;

    EPI_ASSERT(s_num >= 0);

    if (epi::StringPrefixCaseCompareASCII(deh_field, "Zero") == 0 ||
        epi::StringPrefixCaseCompareASCII(deh_field, "Neg. One") == 0)
        return;

    if (epi::StringCaseCompareASCII(deh_field, "Zero/One") == 0) // singularity, ignored
        return;

    if (epi::StringCaseCompareASCII(deh_field, "Offset") == 0)
    {
        LogDebug("Dehacked: Warning - Line %d: raw sound Offset not supported.\n", patch::line_num);
        return;
    }

    if (epi::StringCaseCompareASCII(deh_field, "Value") == 0) // priority
    {
        if (new_val < 0)
        {
            LogDebug("Dehacked: Warning - Line %d: bad sound priority value: %d.\n", patch::line_num, new_val);
            new_val = 0;
        }

        MarkSound(s_num);

        S_sfx[s_num]->priority = new_val;
        return;
    }

    LogDebug("Dehacked: Warning - UNKNOWN SOUND FIELD: %s\n", deh_field);
}

std::string sounds::GetEdgeSfxName(int sound_id)
{
    std::string name;

    if (sound_id == ksfx_None)
        return name;

    switch (sound_id)
    {
    // EDGE uses different names for the DOG sounds
    case ksfx_dgsit:
        name = "DOG_SIGHT";
        break;
    case ksfx_dgatk:
        name = "DOG_BITE";
        break;
    case ksfx_dgact:
        name = "DOG_LOOK";
        break;
    case ksfx_dgdth:
        name = "DOG_DIE";
        break;
    case ksfx_dgpain:
        name = "DOG_PAIN";
        break;

    default:
        break;
    }

    if (!name.empty())
        return name;

    const SoundEffectInfo *orig = GetOriginalSFX(sound_id);

    if (orig->name[0] != 0)
    {
        name = orig->name;
        epi::StringUpperASCII(name);
        return name;
    }

    // we get here for sounds with no original name (only possible
    // for DSDehacked / MBF21).  check if modified name is empty too.

    if (sound_id >= (int)S_sfx.size())
        return name;

    const SoundEffectInfo *mod = S_sfx[sound_id];
    if (mod == nullptr || mod->name[0] == 0)
        return name;

    // create a suitable name
    name = epi::StringFormat("BEX_%d", sound_id);
    epi::StringUpperASCII(name);
    return name;
}

std::string sounds::GetSound(int sound_id)
{
    std::string name;

    if (sound_id == ksfx_None)
        name = "NULL";

    // handle random sounds
    switch (sound_id)
    {
    case ksfx_podth1:
    case ksfx_podth2:
    case ksfx_podth3:
        name = "PODTH?";
        break;

    case ksfx_posit1:
    case ksfx_posit2:
    case ksfx_posit3:
        name = "POSIT?";
        break;

    case ksfx_bgdth1:
    case ksfx_bgdth2:
        name = "BGDTH?";
        break;

    case ksfx_bgsit1:
    case ksfx_bgsit2:
        name = "BGSIT?";
        break;

    default:
        break;
    }

    if (!name.empty())
        return name;

    // if something uses DEHEXTRA sounds (+ a few others), ensure we
    // generate DDFSFX entries for them.
    if ((ksfx_fre000 <= sound_id && sound_id <= ksfx_fre199) || (sound_id == ksfx_gibdth) || (sound_id == ksfx_scrsht))
    {
        MarkSound(sound_id);
    }

    name = GetEdgeSfxName(sound_id);
    if (name.empty())
        name = "NULL";

    return name;
}

void sounds::WriteSound(int sound_id)
{
    const SoundEffectInfo *sound = S_sfx[sound_id];

    // in the unlikely event the sound did not get a name (which is
    // only possible with DSDehacked / MBF21), just skip it.
    const char *lump = sound->name;

    if (lump[0] == 0)
        return;

    std::string ddf_name = GetEdgeSfxName(sound_id);
    if (ddf_name.empty())
        FatalError("Dehacked: Error - No DDF name for sound %d ??\n", sound_id);

    wad::Printf("[%s]\n", ddf_name.c_str());

    // only one sound has a `link` field in standard DOOM.
    // we emulate that here.
    if (sound_id == ksfx_chgun)
    {
        const SoundEffectInfo *link = &S_sfx_orig[ksfx_pistol];

        if (ksfx_pistol < (int)S_sfx.size() && S_sfx[ksfx_pistol] != nullptr)
            link = S_sfx[ksfx_pistol];

        if (link->name[0] != 0)
            lump = link->name;
    }

    ddf_name = lump;
    epi::StringUpperASCII(ddf_name);

    wad::Printf("LUMP_NAME = \"DS%s\";\n", ddf_name.c_str());
    wad::Printf("DEH_SOUND_ID = %d;\n", sound_id);
    wad::Printf("PRIORITY = %d;\n", sound->priority);

    if (sound->singularity != 0)
        wad::Printf("SINGULAR = %d;\n", sound->singularity);

    if (sound_id == ksfx_stnmov)
        wad::Printf("LOOP = TRUE;\n");

    wad::Printf("\n");
}

void sounds::ConvertSFX(void)
{
    if (all_mode)
    {
        for (int i = 1; i < kTotalSoundEffectsPortCompatibility; i++)
            MarkSound(i);

        /* this is debatable....
        for (int i = ksfx_fre000 ; i <= ksfx_fre199 ; i++)
            MarkSound(i);
        */
    }

    bool got_one = false;

    for (int i = 1; i < (int)S_sfx.size(); i++)
    {
        if (S_sfx[i] == nullptr)
            continue;

        if (!got_one)
        {
            BeginLump();
            got_one = true;
        }

        WriteSound(i);
    }

    if (got_one)
        FinishLump();
}

bool sounds::ReplaceSound(const char *before, const char *after)
{
    EPI_ASSERT(strlen(before) <= 6);
    EPI_ASSERT(strlen(after) <= 6);

    for (int i = 1; i < kTotalSoundEffectsDEHEXTRA; i++)
    {
        const SoundEffectInfo *orig = GetOriginalSFX(i);

        if (orig->name[0] == 0)
            continue;

        if (epi::StringCaseCompareASCII(orig->name, before) != 0)
            continue;

        MarkSound(i);

        strcpy(S_sfx[i]->name, after);
        return true;
    }

    return false;
}

void sounds::AlterBexSound(const char *new_val)
{
    const char *old_val = patch::line_buf;

    if (strlen(new_val) < 1 || strlen(new_val) > 6)
    {
        LogDebug("Dehacked: Warning - Bad length for sound name '%s'.\n", new_val);
        return;
    }

    // for DSDehacked, support a numeric target
    if (epi::IsDigitASCII(old_val[0]))
    {
        int num = atoi(old_val);
        if (num < 1 || num > 32767)
        {
            LogDebug("Dehacked: Warning - Line %d: illegal sound number '%s'.\n", patch::line_num, old_val);
        }
        else
        {
            MarkSound(num);
            strcpy(S_sfx[num]->name, new_val);
        }
        return;
    }

    if (strlen(old_val) < 1 || strlen(old_val) > 6)
    {
        LogDebug("Dehacked: Warning - Bad length for sound name '%s'.\n", old_val);
        return;
    }

    if (!ReplaceSound(old_val, new_val))
        LogDebug("Dehacked: Warning - Line %d: unknown sound name '%s'.\n", patch::line_num, old_val);
}

} // namespace dehacked
