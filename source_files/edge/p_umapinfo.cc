//----------------------------------------------------------------------------
//  EDGE UMAPINFO Parsing Code
//----------------------------------------------------------------------------
//
//  Copyright (c) 2023-2024 The EDGE Team.
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
//  Based on the UMAPINFO reference implementation, released by Christoph
//  Oelckers under the following copyright:
//
//  Copyright 2017 Christoph Oelckers
//
//----------------------------------------------------------------------------

#include "p_umapinfo.h"

#include <unordered_map> // ZDoom Actor Name <-> Doomednum lookups

#include "deh_text.h"
#include "game.h"
#include "language.h"
#include "str_compare.h"
#include "str_ename.h"
#include "str_lexer.h"
#include "str_util.h"

MapList Maps;

static std::unordered_map<int, int16_t> ActorNames = {
    {epi::kENameDoomPlayer, -1},
    {epi::kENameZombieMan, 3004},
    {epi::kENameShotgunGuy, 9},
    {epi::kENameArchvile, 64},
    {epi::kENameArchvileFire, -1},
    {epi::kENameRevenant, 66},
    {epi::kENameRevenantTracer, -1},
    {epi::kENameRevenantTracerSmoke, -1},
    {epi::kENameFatso, 67},
    {epi::kENameFatShot, -1},
    {epi::kENameChaingunGuy, 65},
    {epi::kENameDoomImp, 3001},
    {epi::kENameDemon, 3002},
    {epi::kENameSpectre, 58},
    {epi::kENameCacodemon, 3005},
    {epi::kENameBaronOfHell, 3003},
    {epi::kENameBaronBall, -1},
    {epi::kENameHellKnight, 69},
    {epi::kENameLostSoul, 3006},
    {epi::kENameSpiderMastermind, 7},
    {epi::kENameArachnotron, 68},
    {epi::kENameCyberdemon, 16},
    {epi::kENamePainElemental, 71},
    {epi::kENameWolfensteinSS, 84},
    {epi::kENameCommanderKeen, 72},
    {epi::kENameBossBrain, 88},
    {epi::kENameBossEye, 89},
    {epi::kENameBossTarget, 87},
    {epi::kENameSpawnShot, -1},
    {epi::kENameSpawnFire, -1},
    {epi::kENameExplosiveBarrel, 2035},
    {epi::kENameDoomImpBall, -1},
    {epi::kENameCacodemonBall, -1},
    {epi::kENameRocket, -1},
    {epi::kENamePlasmaBall, -1},
    {epi::kENameBFGBall, -1},
    {epi::kENameArachnotronPlasma, -1},
    {epi::kENameBulletPuff, -1},
    {epi::kENameBlood, -1},
    {epi::kENameTeleportFog, -1},
    {epi::kENameItemFog, -1},
    {epi::kENameTeleportDest, 14},
    {epi::kENameBFGExtra, -1},
    {epi::kENameGreenArmor, 2018},
    {epi::kENameBlueArmor, 2019},
    {epi::kENameHealthBonus, 2014},
    {epi::kENameArmorBonus, 2015},
    {epi::kENameBlueCard, 5},
    {epi::kENameRedCard, 13},
    {epi::kENameYellowCard, 6},
    {epi::kENameYellowSkull, 39},
    {epi::kENameRedSkull, 38},
    {epi::kENameBlueSkull, 40},
    {epi::kENameStimpack, 2011},
    {epi::kENameMedikit, 2012},
    {epi::kENameSoulsphere, 2013},
    {epi::kENameInvulnerabilitySphere, 2022},
    {epi::kENameBerserk, 2023},
    {epi::kENameBlurSphere, 2024},
    {epi::kENameRadSuit, 2025},
    {epi::kENameAllmap, 2026},
    {epi::kENameInfrared, 2045},
    {epi::kENameMegasphere, 83},
    {epi::kENameClip, 2007},
    {epi::kENameClipBox, 2048},
    {epi::kENameRocketAmmo, 2010},
    {epi::kENameRocketBox, 2046},
    {epi::kENameCell, 2047},
    {epi::kENameCellPack, 17},
    {epi::kENameShell, 2008},
    {epi::kENameShellBox, 2049},
    {epi::kENameBackpack, 8},
    {epi::kENameBFG9000, 2006},
    {epi::kENameChaingun, 2002},
    {epi::kENameChainsaw, 2005},
    {epi::kENameRocketLauncher, 2003},
    {epi::kENamePlasmaRifle, 2004},
    {epi::kENameShotgun, 2001},
    {epi::kENameSuperShotgun, 82},
    {epi::kENameTechLamp, 85},
    {epi::kENameTechLamp2, 86},
    {epi::kENameColumn, 2028},
    {epi::kENameTallGreenColumn, 30},
    {epi::kENameShortGreenColumn, 31},
    {epi::kENameTallRedColumn, 32},
    {epi::kENameShortRedColumn, 33},
    {epi::kENameSkullColumn, 37},
    {epi::kENameHeartColumn, 36},
    {epi::kENameEvilEye, 41},
    {epi::kENameFloatingSkull, 42},
    {epi::kENameTorchTree, 43},
    {epi::kENameBlueTorch, 44},
    {epi::kENameGreenTorch, 45},
    {epi::kENameRedTorch, 46},
    {epi::kENameShortBlueTorch, 55},
    {epi::kENameShortGreenTorch, 56},
    {epi::kENameShortRedTorch, 57},
    {epi::kENameStalagtite, 47},
    {epi::kENameTechPillar, 48},
    {epi::kENameCandleStick, 34},
    {epi::kENameCandelabra, 35},
    {epi::kENameBloodyTwitch, 49},
    {epi::kENameMeat2, 50},
    {epi::kENameMeat3, 51},
    {epi::kENameMeat4, 52},
    {epi::kENameMeat5, 53},
    {epi::kENameNonsolidMeat2, 59},
    {epi::kENameNonsolidMeat4, 60},
    {epi::kENameNonsolidMeat3, 61},
    {epi::kENameNonsolidMeat5, 62},
    {epi::kENameNonsolidTwitch, 63},
    {epi::kENameDeadCacodemon, 22},
    {epi::kENameDeadMarine, 15},
    {epi::kENameDeadZombieMan, 18},
    {epi::kENameDeadDemon, 21},
    {epi::kENameDeadLostSoul, 23},
    {epi::kENameDeadDoomImp, 20},
    {epi::kENameDeadShotgunGuy, 19},
    {epi::kENameGibbedMarine, 10},
    {epi::kENameGibbedMarineExtra, 12},
    {epi::kENameHeadsOnAStick, 28},
    {epi::kENameGibs, 24},
    {epi::kENameHeadOnAStick, 27},
    {epi::kENameHeadCandles, 29},
    {epi::kENameDeadStick, 25},
    {epi::kENameLiveStick, 26},
    {epi::kENameBigTree, 54},
    {epi::kENameBurningBarrel, 70},
    {epi::kENameHangNoGuts, 73},
    {epi::kENameHangBNoBrain, 74},
    {epi::kENameHangTLookingDown, 75},
    {epi::kENameHangTSkull, 76},
    {epi::kENameHangTLookingUp, 77},
    {epi::kENameHangTNoBrain, 78},
    {epi::kENameColonGibs, 79},
    {epi::kENameSmallBloodPool, 80},
    {epi::kENameBrainStem, 81},
    // Boom/MBF additions
    {epi::kENamePointPusher, 5001},
    {epi::kENamePointPuller, 5002},
    {epi::kENameMBFHelperDog, 888},
    {epi::kENamePlasmaBall1, -1},
    {epi::kENamePlasmaBall2, -1},
    {epi::kENameEvilSceptre, -1},
    {epi::kENameUnholyBible, -1},
    {epi::kENameMusicChanger, -1}, // Doomednums 14101-14165, but I don't think we need this
    // I'm guessing below here
    {epi::kENameDeh_Actor_145, 145},
    {epi::kENameDeh_Actor_146, 146},
    {epi::kENameDeh_Actor_147, 147},
    {epi::kENameDeh_Actor_148, 148},
    {epi::kENameDeh_Actor_149, 149},
    // DEHEXTRA Actors start here
    {epi::kENameDeh_Actor_150, 150}, // Extra thing 0
    {epi::kENameDeh_Actor_151, 151}, // Extra thing 1
    {epi::kENameDeh_Actor_152, 152}, // Extra thing 2
    {epi::kENameDeh_Actor_153, 153}, // Extra thing 3
    {epi::kENameDeh_Actor_154, 154}, // Extra thing 4
    {epi::kENameDeh_Actor_155, 155}, // Extra thing 5
    {epi::kENameDeh_Actor_156, 156}, // Extra thing 6
    {epi::kENameDeh_Actor_157, 157}, // Extra thing 7
    {epi::kENameDeh_Actor_158, 158}, // Extra thing 8
    {epi::kENameDeh_Actor_159, 159}, // Extra thing 9
    {epi::kENameDeh_Actor_160, 160}, // Extra thing 10
    {epi::kENameDeh_Actor_161, 161}, // Extra thing 11
    {epi::kENameDeh_Actor_162, 162}, // Extra thing 12
    {epi::kENameDeh_Actor_163, 163}, // Extra thing 13
    {epi::kENameDeh_Actor_164, 164}, // Extra thing 14
    {epi::kENameDeh_Actor_165, 165}, // Extra thing 15
    {epi::kENameDeh_Actor_166, 166}, // Extra thing 16
    {epi::kENameDeh_Actor_167, 167}, // Extra thing 17
    {epi::kENameDeh_Actor_168, 168}, // Extra thing 18
    {epi::kENameDeh_Actor_169, 169}, // Extra thing 19
    {epi::kENameDeh_Actor_170, 170}, // Extra thing 20
    {epi::kENameDeh_Actor_171, 171}, // Extra thing 21
    {epi::kENameDeh_Actor_172, 172}, // Extra thing 22
    {epi::kENameDeh_Actor_173, 173}, // Extra thing 23
    {epi::kENameDeh_Actor_174, 174}, // Extra thing 24
    {epi::kENameDeh_Actor_175, 175}, // Extra thing 25
    {epi::kENameDeh_Actor_176, 176}, // Extra thing 26
    {epi::kENameDeh_Actor_177, 177}, // Extra thing 27
    {epi::kENameDeh_Actor_178, 178}, // Extra thing 28
    {epi::kENameDeh_Actor_179, 179}, // Extra thing 29
    {epi::kENameDeh_Actor_180, 180}, // Extra thing 30
    {epi::kENameDeh_Actor_181, 181}, // Extra thing 31
    {epi::kENameDeh_Actor_182, 182}, // Extra thing 32
    {epi::kENameDeh_Actor_183, 183}, // Extra thing 33
    {epi::kENameDeh_Actor_184, 184}, // Extra thing 34
    {epi::kENameDeh_Actor_185, 185}, // Extra thing 35
    {epi::kENameDeh_Actor_186, 186}, // Extra thing 36
    {epi::kENameDeh_Actor_187, 187}, // Extra thing 37
    {epi::kENameDeh_Actor_188, 188}, // Extra thing 38
    {epi::kENameDeh_Actor_189, 189}, // Extra thing 39
    {epi::kENameDeh_Actor_190, 190}, // Extra thing 40
    {epi::kENameDeh_Actor_191, 191}, // Extra thing 41
    {epi::kENameDeh_Actor_192, 192}, // Extra thing 42
    {epi::kENameDeh_Actor_193, 193}, // Extra thing 43
    {epi::kENameDeh_Actor_194, 194}, // Extra thing 44
    {epi::kENameDeh_Actor_195, 195}, // Extra thing 45
    {epi::kENameDeh_Actor_196, 196}, // Extra thing 46
    {epi::kENameDeh_Actor_197, 197}, // Extra thing 47
    {epi::kENameDeh_Actor_198, 198}, // Extra thing 48
    {epi::kENameDeh_Actor_199, 199}, // Extra thing 49
    {epi::kENameDeh_Actor_200, 200}, // Extra thing 50
    {epi::kENameDeh_Actor_201, 201}, // Extra thing 51
    {epi::kENameDeh_Actor_202, 202}, // Extra thing 52
    {epi::kENameDeh_Actor_203, 203}, // Extra thing 53
    {epi::kENameDeh_Actor_204, 204}, // Extra thing 54
    {epi::kENameDeh_Actor_205, 205}, // Extra thing 55
    {epi::kENameDeh_Actor_206, 206}, // Extra thing 56
    {epi::kENameDeh_Actor_207, 207}, // Extra thing 57
    {epi::kENameDeh_Actor_208, 208}, // Extra thing 58
    {epi::kENameDeh_Actor_209, 209}, // Extra thing 59
    {epi::kENameDeh_Actor_210, 210}, // Extra thing 60
    {epi::kENameDeh_Actor_211, 211}, // Extra thing 61
    {epi::kENameDeh_Actor_212, 212}, // Extra thing 62
    {epi::kENameDeh_Actor_213, 213}, // Extra thing 63
    {epi::kENameDeh_Actor_214, 214}, // Extra thing 64
    {epi::kENameDeh_Actor_215, 215}, // Extra thing 65
    {epi::kENameDeh_Actor_216, 216}, // Extra thing 66
    {epi::kENameDeh_Actor_217, 217}, // Extra thing 67
    {epi::kENameDeh_Actor_218, 218}, // Extra thing 68
    {epi::kENameDeh_Actor_219, 219}, // Extra thing 69
    {epi::kENameDeh_Actor_220, 220}, // Extra thing 70
    {epi::kENameDeh_Actor_221, 221}, // Extra thing 71
    {epi::kENameDeh_Actor_222, 222}, // Extra thing 72
    {epi::kENameDeh_Actor_223, 223}, // Extra thing 73
    {epi::kENameDeh_Actor_224, 224}, // Extra thing 74
    {epi::kENameDeh_Actor_225, 225}, // Extra thing 75
    {epi::kENameDeh_Actor_226, 226}, // Extra thing 76
    {epi::kENameDeh_Actor_227, 227}, // Extra thing 77
    {epi::kENameDeh_Actor_228, 228}, // Extra thing 78
    {epi::kENameDeh_Actor_229, 229}, // Extra thing 79
    {epi::kENameDeh_Actor_230, 230}, // Extra thing 80
    {epi::kENameDeh_Actor_231, 231}, // Extra thing 81
    {epi::kENameDeh_Actor_232, 232}, // Extra thing 82
    {epi::kENameDeh_Actor_233, 233}, // Extra thing 83
    {epi::kENameDeh_Actor_234, 234}, // Extra thing 84
    {epi::kENameDeh_Actor_235, 235}, // Extra thing 85
    {epi::kENameDeh_Actor_236, 236}, // Extra thing 86
    {epi::kENameDeh_Actor_237, 237}, // Extra thing 87
    {epi::kENameDeh_Actor_238, 238}, // Extra thing 88
    {epi::kENameDeh_Actor_239, 239}, // Extra thing 89
    {epi::kENameDeh_Actor_240, 240}, // Extra thing 90
    {epi::kENameDeh_Actor_241, 241}, // Extra thing 91
    {epi::kENameDeh_Actor_242, 242}, // Extra thing 92
    {epi::kENameDeh_Actor_243, 243}, // Extra thing 93
    {epi::kENameDeh_Actor_244, 244}, // Extra thing 94
    {epi::kENameDeh_Actor_245, 245}, // Extra thing 95
    {epi::kENameDeh_Actor_246, 246}, // Extra thing 96
    {epi::kENameDeh_Actor_247, 247}, // Extra thing 97
    {epi::kENameDeh_Actor_248, 248}, // Extra thing 98
    {epi::kENameDeh_Actor_249, 249}, // Extra thing 99
};

static void FreeMap(MapEntry *mape)
{
    if (mape->mapname)
        free(mape->mapname);
    if (mape->levelname)
        free(mape->levelname);
    if (mape->label)
        free(mape->label);
    if (mape->intertext)
        free(mape->intertext);
    if (mape->intertextsecret)
        free(mape->intertextsecret);
    if (mape->bossactions)
        free(mape->bossactions);
    if (mape->authorname)
        free(mape->authorname);
    mape->mapname = nullptr;
}

void FreeMapList()
{
    unsigned i;

    for (i = 0; i < Maps.mapcount; i++)
    {
        FreeMap(&Maps.maps[i]);
    }
    free(Maps.maps);
    Maps.maps     = nullptr;
    Maps.mapcount = 0;
}

static void SkipToNextLine(epi::Lexer &lex, epi::TokenKind &tok, std::string &value)
{
    int skip_line = lex.LastLine();
    for (;;)
    {
        lex.MatchKeep("linecheck");
        if (lex.LastLine() == skip_line)
        {
            tok = lex.Next(value);
            if (tok == epi::kTokenEOF)
                break;
        }
        else
            break;
    }
}

// -----------------------------------------------
//
// Parses a complete UMAPINFO entry
//
// -----------------------------------------------

static void ParseUMAPINFOEntry(epi::Lexer &lex, MapEntry *val)
{
    for (;;)
    {
        if (lex.Match("}"))
            break;

        std::string key;
        std::string value;

        epi::TokenKind tok = lex.Next(key);

        if (tok == epi::kTokenEOF)
            FatalError("Malformed UMAPINFO lump: unclosed block\n");

        if (tok != epi::kTokenIdentifier)
            FatalError("Malformed UMAPINFO lump: missing key\n");

        if (!lex.Match("="))
            FatalError("Malformed UMAPINFO lump: missing '='\n");

        tok = lex.Next(value);

        if (tok == epi::kTokenEOF || tok == epi::kTokenError || value == "}")
            FatalError("Malformed UMAPINFO lump: missing value\n");

        epi::EName key_ename(key, true);

        switch (key_ename.GetIndex())
        {
        case epi::kENameLevelname: {
            if (val->levelname)
                free(val->levelname);
            val->levelname = (char *)calloc(value.size() + 1, sizeof(char));
            epi::CStringCopyMax(val->levelname, value.c_str(), value.size());
        }
        break;
        case epi::kENameLabel: {
            if (epi::StringCaseCompareASCII(value, "clear") == 0)
            {
                if (val->label)
                    free(val->label);
                val->label    = (char *)calloc(2, sizeof(char));
                val->label[0] = '-';
            }
            else
            {
                if (val->label)
                    free(val->label);
                val->label = (char *)calloc(value.size() + 1, sizeof(char));
                epi::CStringCopyMax(val->label, value.c_str(), value.size());
            }
        }
        break;
        case epi::kENameNext: {
            EPI_CLEAR_MEMORY(val->next_map, char, 9);
            if (value.size() > 8)
                FatalError("UMAPINFO: Mapname for \"next\" over 8 characters!\n");
            epi::CStringCopyMax(val->next_map, value.data(), 8);
        }
        break;
        case epi::kENameNextsecret: {
            EPI_CLEAR_MEMORY(val->nextsecret, char, 9);
            if (value.size() > 8)
                FatalError("UMAPINFO: Mapname for \"nextsecret\" over 8 "
                           "characters!\n");
            epi::CStringCopyMax(val->nextsecret, value.data(), 8);
        }
        break;
        case epi::kENameLevelpic: {
            EPI_CLEAR_MEMORY(val->levelpic, char, 9);
            if (value.size() > 8)
                FatalError("UMAPINFO: Entry for \"levelpic\" over 8 "
                           "characters!\n");
            epi::CStringCopyMax(val->levelpic, value.data(), 8);
        }
        break;
        case epi::kENameSkytexture: {
            EPI_CLEAR_MEMORY(val->skytexture, char, 9);
            if (value.size() > 8)
                FatalError("UMAPINFO: Entry for \"skytexture\" over 8 "
                           "characters!\n");
            epi::CStringCopyMax(val->skytexture, value.data(), 8);
        }
        break;
        case epi::kENameMusic: {
            EPI_CLEAR_MEMORY(val->music, char, 9);
            if (value.size() > 8)
                FatalError("UMAPINFO: Entry for \"music\" over 8 characters!\n");
            epi::CStringCopyMax(val->music, value.data(), 8);
        }
        break;
        case epi::kENameEndpic: {
            EPI_CLEAR_MEMORY(val->endpic, char, 9);
            if (value.size() > 8)
                FatalError("UMAPINFO: Entry for \"endpic\" over 8 characters!\n");
            epi::CStringCopyMax(val->endpic, value.data(), 8);
        }
        break;
        case epi::kENameEndcast:
            val->docast = epi::LexBoolean(value);
            break;
        case epi::kENameEndbunny:
            val->dobunny = epi::LexBoolean(value);
            break;
        case epi::kENameEndgame:
            val->endgame = epi::LexBoolean(value);
            break;
        case epi::kENameExitpic: {
            EPI_CLEAR_MEMORY(val->exitpic, char, 9);
            if (value.size() > 8)
                FatalError("UMAPINFO: Entry for \"exitpic\" over 8 characters!\n");
            epi::CStringCopyMax(val->exitpic, value.data(), 8);
        }
        break;
        case epi::kENameEnterpic: {
            EPI_CLEAR_MEMORY(val->enterpic, char, 9);
            if (value.size() > 8)
                FatalError("UMAPINFO: Entry for \"enterpic\" over 8 "
                           "characters!\n");
            epi::CStringCopyMax(val->enterpic, value.data(), 8);
        }
        break;
        case epi::kENameNointermission:
            val->nointermission = epi::LexBoolean(value);
            break;
        case epi::kENamePartime:
            val->partime = 35 * epi::LexInteger(value);
            break;
        case epi::kENameIntertext: {
            std::string it_builder = value;
            while (lex.Match(","))
            {
                it_builder.append("\n");
                lex.Next(value);
                it_builder.append(value);
            }
            if (val->intertext)
                free(val->intertext);
            val->intertext = (char *)calloc(it_builder.size() + 1, sizeof(char));
            epi::CStringCopyMax(val->intertext, it_builder.c_str(), it_builder.size());
        }
        break;
        case epi::kENameIntertextsecret: {
            std::string it_builder = value;
            while (lex.Match(","))
            {
                it_builder.append("\n");
                lex.Next(value);
                it_builder.append(value);
            }
            if (val->intertextsecret)
                free(val->intertextsecret);
            val->intertextsecret = (char *)calloc(it_builder.size() + 1, sizeof(char));
            epi::CStringCopyMax(val->intertextsecret, it_builder.c_str(), it_builder.size());
        }
        break;
        case epi::kENameInterbackdrop: {
            EPI_CLEAR_MEMORY(val->interbackdrop, char, 9);
            if (value.size() > 8)
                FatalError("UMAPINFO: Entry for \"interbackdrop\" over 8 "
                           "characters!\n");
            epi::CStringCopyMax(val->interbackdrop, value.data(), 8);
        }
        break;
        case epi::kENameIntermusic: {
            EPI_CLEAR_MEMORY(val->intermusic, char, 9);
            if (value.size() > 8)
                FatalError("UMAPINFO: Entry for \"intermusic\" over 8 "
                           "characters!\n");
            epi::CStringCopyMax(val->intermusic, value.data(), 8);
        }
        break;
        case epi::kENameEpisode: {
            if (epi::StringCaseCompareASCII(value, "clear") == 0)
            {
                // This should leave the initial [EDGE] episode and nothing
                // else Since 'clear' is supposed to come before any custom
                // definitions this should not clear out any
                // UMAPINFO-defined episodes
                for (auto iter = gamedefs.begin() + 1; iter != gamedefs.end();)
                {
                    GameDefinition *game = *iter;
                    if (game->firstmap_.empty() && epi::StringCaseCompareASCII(game->name_, "UMAPINFO_TEMPLATE") != 0)
                    {
                        delete game;
                        game = nullptr;
                        iter = gamedefs.erase(iter);
                    }
                    else
                        ++iter;
                }
            }
            else
            {
                GameDefinition *new_epi = nullptr;
                // Check for episode to replace
                for (auto game : gamedefs)
                {
                    if (epi::StringCaseCompareASCII(game->firstmap_, val->mapname) == 0 &&
                        epi::StringCaseCompareASCII(game->name_, "UMAPINFO_TEMPLATE") != 0)
                    {
                        new_epi = game;
                        break;
                    }
                }
                if (!new_epi)
                {
                    // Create a new episode from game-specific UMAPINFO
                    // template data
                    GameDefinition *um_template = nullptr;
                    for (auto game : gamedefs)
                    {
                        if (epi::StringCaseCompareASCII(game->name_, "UMAPINFO_TEMPLATE") == 0)
                        {
                            um_template = game;
                            break;
                        }
                    }
                    if (!um_template)
                        FatalError("UMAPINFO: No custom episode template exists "
                                   "for this IWAD! Check DDFGAME!\n");
                    new_epi = new GameDefinition;
                    new_epi->CopyDetail(*um_template);
                    new_epi->firstmap_ = val->mapname;
                    gamedefs.push_back(new_epi);
                }
                char        lumpname[9] = {0};
                std::string alttext;
                std::string epikey; // Do we use this?
                if (value.size() > 8)
                    FatalError("UMAPINFO: Entry for \"enterpic\" over 8 "
                               "characters!\n");
                epi::CStringCopyMax(lumpname, value.data(), 8);
                if (lex.Match(","))
                {
                    lex.Next(alttext);
                    if (lex.Match(","))
                        lex.Next(epikey);
                }
                new_epi->namegraphic_ = lumpname;
                new_epi->description_ = alttext;
                new_epi->name_        = epi::StringFormat("UMAPINFO_%s\n", val->mapname); // Internal
            }
        }
        break;
        case epi::kENameBossaction: {
            int special = 0;
            int tag     = 0;
            if (epi::StringCaseCompareASCII(value, "clear") == 0)
            {
                special = tag = -1;
                if (val->bossactions)
                    free(val->bossactions);
                val->bossactions    = nullptr;
                val->numbossactions = -1;
            }
            else
            {
                int actor_num   = -1;
                int actor_check = epi::EName(value, true).GetIndex();
                if (!ActorNames.count(actor_check))
                    FatalError("UMAPINFO: Unknown thing type %s\n", value.c_str());
                else
                    actor_num = ActorNames[actor_check];
                if (actor_num == -1)
                    SkipToNextLine(lex, tok, value);
                else
                {
                    if (!lex.Match(","))
                        FatalError("UMAPINFO: \"bossaction\" key missing line "
                                   "special!\n");
                    lex.Next(value);
                    special = epi::LexInteger(value);
                    if (!lex.Match(","))
                        FatalError("UMAPINFO: \"bossaction\" key missing tag!\n");
                    lex.Next(value);
                    tag = epi::LexInteger(value);
                    if (tag != 0 || special == 11 || special == 51 || special == 52 || special == 124)
                    {
                        if (val->numbossactions == -1)
                            val->numbossactions = 1;
                        else
                            val->numbossactions++;
                        val->bossactions = (struct BossAction *)realloc(val->bossactions, sizeof(struct BossAction) *
                                                                                              val->numbossactions);
                        val->bossactions[val->numbossactions - 1].type    = actor_num;
                        val->bossactions[val->numbossactions - 1].special = special;
                        val->bossactions[val->numbossactions - 1].tag     = tag;
                    }
                }
            }
        }
        break;
        case epi::kENameAuthor: {
            if (val->authorname)
                free(val->authorname);
            val->authorname = (char *)calloc(value.size() + 1, sizeof(char));
            epi::CStringCopyMax(val->authorname, value.c_str(), value.size());
        }
        break;
        default:
            break;
        }
    }
    // Some fallback handling
    if (!val->nextsecret[0])
    {
        if (val->next_map[0])
            epi::CStringCopyMax(val->nextsecret, val->next_map, 8);
    }
    if (!val->enterpic[0])
    {
        for (size_t i = 0; i < Maps.mapcount; i++)
        {
            if (!strcmp(val->mapname, Maps.maps[i].next_map))
            {
                if (Maps.maps[i].exitpic[0])
                    epi::CStringCopyMax(val->enterpic, Maps.maps[i].exitpic, 8);
                break;
            }
        }
    }
}

// -----------------------------------------------
//
// Parses a complete UMAPINFO lump
//
// -----------------------------------------------

void ParseUmapinfo(const std::string &buffer)
{
    epi::Lexer lex(buffer);

    for (;;)
    {
        std::string    section;
        epi::TokenKind tok = lex.Next(section);

        if (tok == epi::kTokenEOF)
            break;

        if (tok != epi::kTokenIdentifier || epi::StringCaseCompareASCII(section, "MAP") != 0)
            FatalError("Malformed UMAPINFO lump.\n");

        tok = lex.Next(section);

        if (tok != epi::kTokenIdentifier)
            FatalError("UMAPINFO: No mapname for map entry!\n");

        unsigned int i      = 0;
        MapEntry     parsed = {0};
        parsed.mapname      = (char *)calloc(section.size() + 1, sizeof(char));
        epi::CStringCopyMax(parsed.mapname, section.data(), section.size());

        if (!lex.Match("{"))
            FatalError("Malformed UMAPINFO lump: missing '{'\n");

        ParseUMAPINFOEntry(lex, &parsed);
        // Does this map entry already exist? If yes, replace it.
        for (i = 0; i < Maps.mapcount; i++)
        {
            if (epi::StringCaseCompareASCII(parsed.mapname, Maps.maps[i].mapname) == 0)
            {
                FreeMap(&Maps.maps[i]);
                Maps.maps[i] = parsed;
                break;
            }
        }
        // Not found so create a new one.
        if (i == Maps.mapcount)
        {
            Maps.mapcount++;
            Maps.maps                    = (MapEntry *)realloc(Maps.maps, sizeof(MapEntry) * Maps.mapcount);
            Maps.maps[Maps.mapcount - 1] = parsed;
        }
    }
}