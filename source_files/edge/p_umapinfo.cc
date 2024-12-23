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

#include <unordered_map> // ZDoom Actor Name <-> Dehacked/Doomednum lookups

#include "ddf_game.h"
#include "ddf_language.h"
#include "ddf_thing.h"
#if EDGE_DEHACKED_SUPPORT
#include "deh_text.h"
#endif
#include "epi_ename.h"
#include "epi_scanner.h"
#include "epi_str_compare.h"
#include "epi_str_util.h"

MapList Maps;

static std::unordered_map<int, std::pair<int16_t, int16_t>> ActorNames = {
    {epi::kENameDoomPlayer, {1, -1}},
    {epi::kENameZombieMan, {2, 3004}},
    {epi::kENameShotgunGuy, {3, 9}},
    {epi::kENameArchvile, {4, 64}},
    {epi::kENameArchvileFire, {5, -1}},
    {epi::kENameRevenant, {6, 66}},
    {epi::kENameRevenantTracer, {7, -1}},
    {epi::kENameRevenantTracerSmoke, {8, -1}},
    {epi::kENameFatso, {9, 67}},
    {epi::kENameFatShot, {10, -1}},
    {epi::kENameChaingunGuy, {11, 65}},
    {epi::kENameDoomImp, {12, 3001}},
    {epi::kENameDemon, {13, 3002}},
    {epi::kENameSpectre, {14, 58}},
    {epi::kENameCacodemon, {15, 3005}},
    {epi::kENameBaronOfHell, {16, 3003}},
    {epi::kENameBaronBall, {17, -1}},
    {epi::kENameHellKnight, {18, 69}},
    {epi::kENameLostSoul, {19, 3006}},
    {epi::kENameSpiderMastermind, {20, 7}},
    {epi::kENameArachnotron, {21, 68}},
    {epi::kENameCyberdemon, {22, 16}},
    {epi::kENamePainElemental, {23, 71}},
    {epi::kENameWolfensteinSS, {24, 84}},
    {epi::kENameCommanderKeen, {25, 72}},
    {epi::kENameBossBrain, {26, 88}},
    {epi::kENameBossEye, {27, 89}},
    {epi::kENameBossTarget, {28, 87}},
    {epi::kENameSpawnShot, {29, -1}},
    {epi::kENameSpawnFire, {30, -1}},
    {epi::kENameExplosiveBarrel, {31, 2035}},
    {epi::kENameDoomImpBall, {32, -1}},
    {epi::kENameCacodemonBall, {33, -1}},
    {epi::kENameRocket, {34, -1}},
    {epi::kENamePlasmaBall, {35, -1}},
    {epi::kENameBFGBall, {36, -1}},
    {epi::kENameArachnotronPlasma, {37, -1}},
    {epi::kENameBulletPuff, {38, -1}},
    {epi::kENameBlood, {39, -1}},
    {epi::kENameTeleportFog, {40, -1}},
    {epi::kENameItemFog, {41, -1}},
    {epi::kENameTeleportDest, {42, 14}},
    {epi::kENameBFGExtra, {43, -1}},
    {epi::kENameGreenArmor, {44, 2018}},
    {epi::kENameBlueArmor, {45, 2019}},
    {epi::kENameHealthBonus, {46, 2014}},
    {epi::kENameArmorBonus, {47, 2015}},
    {epi::kENameBlueCard, {48, 5}},
    {epi::kENameRedCard, {49, 13}},
    {epi::kENameYellowCard, {50, 6}},
    {epi::kENameYellowSkull, {51, 39}},
    {epi::kENameRedSkull, {52, 38}},
    {epi::kENameBlueSkull, {53, 40}},
    {epi::kENameStimpack, {54, 2011}},
    {epi::kENameMedikit, {55, 2012}},
    {epi::kENameSoulsphere, {56, 2013}},
    {epi::kENameInvulnerabilitySphere, {57, 2022}},
    {epi::kENameBerserk, {58, 2023}},
    {epi::kENameBlurSphere, {59, 2024}},
    {epi::kENameRadSuit, {60, 2025}},
    {epi::kENameAllmap, {61, 2026}},
    {epi::kENameInfrared, {62, 2045}},
    {epi::kENameMegasphere, {63, 83}},
    {epi::kENameClip, {64, 2007}},
    {epi::kENameClipBox, {65, 2048}},
    {epi::kENameRocketAmmo, {66, 2010}},
    {epi::kENameRocketBox, {67, 2046}},
    {epi::kENameCell, {68, 2047}},
    {epi::kENameCellPack, {69, 17}},
    {epi::kENameShell, {70, 2008}},
    {epi::kENameShellBox, {71, 2049}},
    {epi::kENameBackpack, {72, 8}},
    {epi::kENameBFG9000, {73, 2006}},
    {epi::kENameChaingun, {74, 2002}},
    {epi::kENameChainsaw, {75, 2005}},
    {epi::kENameRocketLauncher, {76, 2003}},
    {epi::kENamePlasmaRifle, {77, 2004}},
    {epi::kENameShotgun, {78, 2001}},
    {epi::kENameSuperShotgun, {79, 82}},
    {epi::kENameTechLamp, {80, 85}},
    {epi::kENameTechLamp2, {81, 86}},
    {epi::kENameColumn, {82, 2028}},
    {epi::kENameTallGreenColumn, {83, 30}},
    {epi::kENameShortGreenColumn, {84, 31}},
    {epi::kENameTallRedColumn, {85, 32}},
    {epi::kENameShortRedColumn, {86, 33}},
    {epi::kENameSkullColumn, {87, 37}},
    {epi::kENameHeartColumn, {88, 36}},
    {epi::kENameEvilEye, {89, 41}},
    {epi::kENameFloatingSkull, {90, 42}},
    {epi::kENameTorchTree, {91, 43}},
    {epi::kENameBlueTorch, {92, 44}},
    {epi::kENameGreenTorch, {93, 45}},
    {epi::kENameRedTorch, {94, 46}},
    {epi::kENameShortBlueTorch, {95, 55}},
    {epi::kENameShortGreenTorch, {96, 56}},
    {epi::kENameShortRedTorch, {97, 57}},
    {epi::kENameStalagtite, {98, 47}},
    {epi::kENameTechPillar, {99, 48}},
    {epi::kENameCandleStick, {100, 34}},
    {epi::kENameCandelabra, {101, 35}},
    {epi::kENameBloodyTwitch, {102, 49}},
    {epi::kENameMeat2, {103, 50}},
    {epi::kENameMeat3, {104, 51}},
    {epi::kENameMeat4, {105, 52}},
    {epi::kENameMeat5, {106, 53}},
    {epi::kENameNonsolidMeat2, {107, 59}},
    {epi::kENameNonsolidMeat4, {108, 60}},
    {epi::kENameNonsolidMeat3, {109, 61}},
    {epi::kENameNonsolidMeat5, {110, 62}},
    {epi::kENameNonsolidTwitch, {111, 63}},
    {epi::kENameDeadCacodemon, {112, 22}},
    {epi::kENameDeadMarine, {113, 15}},
    {epi::kENameDeadZombieMan, {114, 18}},
    {epi::kENameDeadDemon, {115, 21}},
    {epi::kENameDeadLostSoul, {116, 23}},
    {epi::kENameDeadDoomImp, {117, 20}},
    {epi::kENameDeadShotgunGuy, {118, 19}},
    {epi::kENameGibbedMarine, {119, 10}},
    {epi::kENameGibbedMarineExtra, {120, 12}},
    {epi::kENameHeadsOnAStick, {121, 28}},
    {epi::kENameGibs, {122, 24}},
    {epi::kENameHeadOnAStick, {123, 27}},
    {epi::kENameHeadCandles, {124, 29}},
    {epi::kENameDeadStick, {125, 25}},
    {epi::kENameLiveStick, {126, 26}},
    {epi::kENameBigTree, {127, 54}},
    {epi::kENameBurningBarrel, {128, 70}},
    {epi::kENameHangNoGuts, {129, 73}},
    {epi::kENameHangBNoBrain, {130, 74}},
    {epi::kENameHangTLookingDown, {131, 75}},
    {epi::kENameHangTSkull, {132, 76}},
    {epi::kENameHangTLookingUp, {133, 77}},
    {epi::kENameHangTNoBrain, {134, 78}},
    {epi::kENameColonGibs, {135, 79}},
    {epi::kENameSmallBloodPool, {136, 80}},
    {epi::kENameBrainStem, {137, 81}},
    // Boom/MBF additions
    {epi::kENamePointPusher, {138, 5001}},
    {epi::kENamePointPuller, {139, 5002}},
    {epi::kENameMBFHelperDog, {140, 888}},
    {epi::kENamePlasmaBall1, {141, -1}},
    {epi::kENamePlasmaBall2, {142, -1}},
    {epi::kENameEvilSceptre, {143, -1}},
    {epi::kENameUnholyBible, {144, -1}},
    {epi::kENameMusicChanger, {145, -1}},
    {epi::kENameDeh_Actor_145, {145, -1}},
    {epi::kENameDeh_Actor_146, {146, -1}},
    {epi::kENameDeh_Actor_147, {147, -1}},
    {epi::kENameDeh_Actor_148, {148, -1}},
    {epi::kENameDeh_Actor_149, {149, -1}},
    // DEHEXTRA Actors start here
    {epi::kENameDeh_Actor_150, {151, -1}}, // MT_EXTRA0
    {epi::kENameDeh_Actor_151, {152, -1}}, // MT_EXTRA1
    {epi::kENameDeh_Actor_152, {153, -1}}, // MT_EXTRA2
    {epi::kENameDeh_Actor_153, {154, -1}}, // MT_EXTRA3
    {epi::kENameDeh_Actor_154, {155, -1}}, // MT_EXTRA4
    {epi::kENameDeh_Actor_155, {156, -1}}, // MT_EXTRA5
    {epi::kENameDeh_Actor_156, {157, -1}}, // MT_EXTRA6
    {epi::kENameDeh_Actor_157, {158, -1}}, // MT_EXTRA7
    {epi::kENameDeh_Actor_158, {159, -1}}, // MT_EXTRA8
    {epi::kENameDeh_Actor_159, {160, -1}}, // MT_EXTRA9
    {epi::kENameDeh_Actor_160, {161, -1}}, // MT_EXTRA10
    {epi::kENameDeh_Actor_161, {162, -1}}, // MT_EXTRA11
    {epi::kENameDeh_Actor_162, {163, -1}}, // MT_EXTRA12
    {epi::kENameDeh_Actor_163, {164, -1}}, // MT_EXTRA13
    {epi::kENameDeh_Actor_164, {165, -1}}, // MT_EXTRA14
    {epi::kENameDeh_Actor_165, {166, -1}}, // MT_EXTRA15
    {epi::kENameDeh_Actor_166, {167, -1}}, // MT_EXTRA16
    {epi::kENameDeh_Actor_167, {168, -1}}, // MT_EXTRA17
    {epi::kENameDeh_Actor_168, {169, -1}}, // MT_EXTRA18
    {epi::kENameDeh_Actor_169, {170, -1}}, // MT_EXTRA19
    {epi::kENameDeh_Actor_170, {171, -1}}, // MT_EXTRA20
    {epi::kENameDeh_Actor_171, {172, -1}}, // MT_EXTRA21
    {epi::kENameDeh_Actor_172, {173, -1}}, // MT_EXTRA22
    {epi::kENameDeh_Actor_173, {174, -1}}, // MT_EXTRA23
    {epi::kENameDeh_Actor_174, {175, -1}}, // MT_EXTRA24
    {epi::kENameDeh_Actor_175, {176, -1}}, // MT_EXTRA25
    {epi::kENameDeh_Actor_176, {177, -1}}, // MT_EXTRA26
    {epi::kENameDeh_Actor_177, {178, -1}}, // MT_EXTRA27
    {epi::kENameDeh_Actor_178, {179, -1}}, // MT_EXTRA28
    {epi::kENameDeh_Actor_179, {180, -1}}, // MT_EXTRA29
    {epi::kENameDeh_Actor_180, {181, -1}}, // MT_EXTRA30
    {epi::kENameDeh_Actor_181, {182, -1}}, // MT_EXTRA31
    {epi::kENameDeh_Actor_182, {183, -1}}, // MT_EXTRA32
    {epi::kENameDeh_Actor_183, {184, -1}}, // MT_EXTRA33
    {epi::kENameDeh_Actor_184, {185, -1}}, // MT_EXTRA34
    {epi::kENameDeh_Actor_185, {186, -1}}, // MT_EXTRA35
    {epi::kENameDeh_Actor_186, {187, -1}}, // MT_EXTRA36
    {epi::kENameDeh_Actor_187, {188, -1}}, // MT_EXTRA37
    {epi::kENameDeh_Actor_188, {189, -1}}, // MT_EXTRA38
    {epi::kENameDeh_Actor_189, {190, -1}}, // MT_EXTRA39
    {epi::kENameDeh_Actor_190, {191, -1}}, // MT_EXTRA40
    {epi::kENameDeh_Actor_191, {192, -1}}, // MT_EXTRA41
    {epi::kENameDeh_Actor_192, {193, -1}}, // MT_EXTRA42
    {epi::kENameDeh_Actor_193, {194, -1}}, // MT_EXTRA43
    {epi::kENameDeh_Actor_194, {195, -1}}, // MT_EXTRA44
    {epi::kENameDeh_Actor_195, {196, -1}}, // MT_EXTRA45
    {epi::kENameDeh_Actor_196, {197, -1}}, // MT_EXTRA46
    {epi::kENameDeh_Actor_197, {198, -1}}, // MT_EXTRA47
    {epi::kENameDeh_Actor_198, {199, -1}}, // MT_EXTRA48
    {epi::kENameDeh_Actor_199, {200, -1}}, // MT_EXTRA49
    {epi::kENameDeh_Actor_200, {201, -1}}, // MT_EXTRA50
    {epi::kENameDeh_Actor_201, {202, -1}}, // MT_EXTRA51
    {epi::kENameDeh_Actor_202, {203, -1}}, // MT_EXTRA52
    {epi::kENameDeh_Actor_203, {204, -1}}, // MT_EXTRA53
    {epi::kENameDeh_Actor_204, {205, -1}}, // MT_EXTRA54
    {epi::kENameDeh_Actor_205, {206, -1}}, // MT_EXTRA55
    {epi::kENameDeh_Actor_206, {207, -1}}, // MT_EXTRA56
    {epi::kENameDeh_Actor_207, {208, -1}}, // MT_EXTRA57
    {epi::kENameDeh_Actor_208, {209, -1}}, // MT_EXTRA58
    {epi::kENameDeh_Actor_209, {210, -1}}, // MT_EXTRA59
    {epi::kENameDeh_Actor_210, {211, -1}}, // MT_EXTRA60
    {epi::kENameDeh_Actor_211, {212, -1}}, // MT_EXTRA61
    {epi::kENameDeh_Actor_212, {213, -1}}, // MT_EXTRA62
    {epi::kENameDeh_Actor_213, {214, -1}}, // MT_EXTRA63
    {epi::kENameDeh_Actor_214, {215, -1}}, // MT_EXTRA64
    {epi::kENameDeh_Actor_215, {216, -1}}, // MT_EXTRA65
    {epi::kENameDeh_Actor_216, {217, -1}}, // MT_EXTRA66
    {epi::kENameDeh_Actor_217, {218, -1}}, // MT_EXTRA67
    {epi::kENameDeh_Actor_218, {219, -1}}, // MT_EXTRA68
    {epi::kENameDeh_Actor_219, {220, -1}}, // MT_EXTRA69
    {epi::kENameDeh_Actor_220, {221, -1}}, // MT_EXTRA70
    {epi::kENameDeh_Actor_221, {222, -1}}, // MT_EXTRA71
    {epi::kENameDeh_Actor_222, {223, -1}}, // MT_EXTRA72
    {epi::kENameDeh_Actor_223, {224, -1}}, // MT_EXTRA73
    {epi::kENameDeh_Actor_224, {225, -1}}, // MT_EXTRA74
    {epi::kENameDeh_Actor_225, {226, -1}}, // MT_EXTRA75
    {epi::kENameDeh_Actor_226, {227, -1}}, // MT_EXTRA76
    {epi::kENameDeh_Actor_227, {228, -1}}, // MT_EXTRA77
    {epi::kENameDeh_Actor_228, {229, -1}}, // MT_EXTRA78
    {epi::kENameDeh_Actor_229, {230, -1}}, // MT_EXTRA79
    {epi::kENameDeh_Actor_230, {231, -1}}, // MT_EXTRA80
    {epi::kENameDeh_Actor_231, {232, -1}}, // MT_EXTRA81
    {epi::kENameDeh_Actor_232, {233, -1}}, // MT_EXTRA82
    {epi::kENameDeh_Actor_233, {234, -1}}, // MT_EXTRA83
    {epi::kENameDeh_Actor_234, {235, -1}}, // MT_EXTRA84
    {epi::kENameDeh_Actor_235, {236, -1}}, // MT_EXTRA85
    {epi::kENameDeh_Actor_236, {237, -1}}, // MT_EXTRA86
    {epi::kENameDeh_Actor_237, {238, -1}}, // MT_EXTRA87
    {epi::kENameDeh_Actor_238, {239, -1}}, // MT_EXTRA88
    {epi::kENameDeh_Actor_239, {240, -1}}, // MT_EXTRA89
    {epi::kENameDeh_Actor_240, {241, -1}}, // MT_EXTRA90
    {epi::kENameDeh_Actor_241, {242, -1}}, // MT_EXTRA91
    {epi::kENameDeh_Actor_242, {243, -1}}, // MT_EXTRA92
    {epi::kENameDeh_Actor_243, {244, -1}}, // MT_EXTRA93
    {epi::kENameDeh_Actor_244, {245, -1}}, // MT_EXTRA94
    {epi::kENameDeh_Actor_245, {246, -1}}, // MT_EXTRA95
    {epi::kENameDeh_Actor_246, {247, -1}}, // MT_EXTRA96
    {epi::kENameDeh_Actor_247, {248, -1}}, // MT_EXTRA97
    {epi::kENameDeh_Actor_248, {249, -1}}, // MT_EXTRA98
    {epi::kENameDeh_Actor_249, {250, -1}} // MT_EXTRA99
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

// -----------------------------------------------
//
// Parses a complete UMAPINFO entry
//
// -----------------------------------------------

static void ParseUMAPINFOEntry(epi::Scanner &lex, MapEntry *val)
{
    for (;;)
    {
        if (lex.CheckToken('}'))
            break;

        std::string key;
        std::string value;

        if (!lex.GetNextToken())
            FatalError("Malformed UMAPINFO lump: unclosed block\n");

        if (lex.state_.token != epi::Scanner::kIdentifier)
            FatalError("Malformed UMAPINFO lump: missing key\n");

        key = lex.state_.string;

        if (!lex.CheckToken('='))
            FatalError("Malformed UMAPINFO lump: missing '='\n");   

        if (!lex.GetNextToken() || lex.state_.token == '}')
            FatalError("Malformed UMAPINFO lump: missing value\n");

        epi::EName key_ename(key, true);

        switch (key_ename.GetIndex())
        {
        case epi::kENameLevelname: {
            if (val->levelname)
                free(val->levelname);
            value = lex.state_.string;
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
                value = lex.state_.string;
                val->label = (char *)calloc(value.size() + 1, sizeof(char));
                epi::CStringCopyMax(val->label, value.c_str(), value.size());
            }
        }
        break;
        case epi::kENameNext: {
            EPI_CLEAR_MEMORY(val->next_map, char, 9);
            value = lex.state_.string;
            if (value.size() > 8)
                FatalError("UMAPINFO: Mapname for \"next\" over 8 characters!\n");
            epi::CStringCopyMax(val->next_map, value.data(), 8);
        }
        break;
        case epi::kENameNextsecret: {
            EPI_CLEAR_MEMORY(val->nextsecret, char, 9);
            value = lex.state_.string;
            if (value.size() > 8)
                FatalError("UMAPINFO: Mapname for \"nextsecret\" over 8 "
                           "characters!\n");
            epi::CStringCopyMax(val->nextsecret, value.data(), 8);
        }
        break;
        case epi::kENameLevelpic: {
            EPI_CLEAR_MEMORY(val->levelpic, char, 9);
            value = lex.state_.string;
            if (value.size() > 8)
                FatalError("UMAPINFO: Entry for \"levelpic\" over 8 "
                           "characters!\n");
            epi::CStringCopyMax(val->levelpic, value.data(), 8);
        }
        break;
        case epi::kENameSkytexture: {
            EPI_CLEAR_MEMORY(val->skytexture, char, 9);
            value = lex.state_.string;
            if (value.size() > 8)
                FatalError("UMAPINFO: Entry for \"skytexture\" over 8 "
                           "characters!\n");
            epi::CStringCopyMax(val->skytexture, value.data(), 8);
        }
        break;
        case epi::kENameMusic: {
            EPI_CLEAR_MEMORY(val->music, char, 9);
            value = lex.state_.string;
            if (value.size() > 8)
                FatalError("UMAPINFO: Entry for \"music\" over 8 characters!\n");
            epi::CStringCopyMax(val->music, value.data(), 8);
        }
        break;
        case epi::kENameEndpic: {
            EPI_CLEAR_MEMORY(val->endpic, char, 9);
            value = lex.state_.string;
            if (value.size() > 8)
                FatalError("UMAPINFO: Entry for \"endpic\" over 8 characters!\n");
            epi::CStringCopyMax(val->endpic, value.data(), 8);
        }
        break;
        case epi::kENameEndcast:
            val->docast = lex.state_.boolean;
            break;
        case epi::kENameEndbunny:
            val->dobunny = lex.state_.boolean;
            break;
        case epi::kENameEndgame:
            val->endgame = lex.state_.boolean;
            break;
        case epi::kENameExitpic: {
            EPI_CLEAR_MEMORY(val->exitpic, char, 9);
            value = lex.state_.string;
            if (value.size() > 8)
                FatalError("UMAPINFO: Entry for \"exitpic\" over 8 characters!\n");
            epi::CStringCopyMax(val->exitpic, value.data(), 8);
        }
        break;
        case epi::kENameEnterpic: {
            EPI_CLEAR_MEMORY(val->enterpic, char, 9);
            value = lex.state_.string;
            if (value.size() > 8)
                FatalError("UMAPINFO: Entry for \"enterpic\" over 8 "
                           "characters!\n");
            epi::CStringCopyMax(val->enterpic, value.data(), 8);
        }
        break;
        case epi::kENameNointermission:
            val->nointermission = lex.state_.boolean;
            break;
        case epi::kENamePartime:
            val->partime = 35 * lex.state_.number;
            break;
        case epi::kENameIntertext: {
            std::string it_builder = value;
            while (lex.CheckToken(','))
            {
                it_builder.append("\n");
                lex.GetNextToken();
                it_builder.append(lex.state_.string);
            }
            if (val->intertext)
                free(val->intertext);
            val->intertext = (char *)calloc(it_builder.size() + 1, sizeof(char));
            epi::CStringCopyMax(val->intertext, it_builder.c_str(), it_builder.size());
        }
        break;
        case epi::kENameIntertextsecret: {
            std::string it_builder = value;
            while (lex.CheckToken(','))
            {
                it_builder.append("\n");
                lex.GetNextToken();
                it_builder.append(lex.state_.string);
            }
            if (val->intertextsecret)
                free(val->intertextsecret);
            val->intertextsecret = (char *)calloc(it_builder.size() + 1, sizeof(char));
            epi::CStringCopyMax(val->intertextsecret, it_builder.c_str(), it_builder.size());
        }
        break;
        case epi::kENameInterbackdrop: {
            EPI_CLEAR_MEMORY(val->interbackdrop, char, 9);
            value = lex.state_.string;
            if (value.size() > 8)
                FatalError("UMAPINFO: Entry for \"interbackdrop\" over 8 "
                           "characters!\n");
            epi::CStringCopyMax(val->interbackdrop, value.data(), 8);
        }
        break;
        case epi::kENameIntermusic: {
            EPI_CLEAR_MEMORY(val->intermusic, char, 9);
            value = lex.state_.string;
            if (value.size() > 8)
                FatalError("UMAPINFO: Entry for \"intermusic\" over 8 "
                           "characters!\n");
            epi::CStringCopyMax(val->intermusic, value.data(), 8);
        }
        break;
        case epi::kENameEpisode: {
            value = lex.state_.string;
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
                if (lex.CheckToken(','))
                {
                    lex.GetNextToken();
                    alttext = lex.state_.string;
                    if (lex.CheckToken(','))
                    {
                        lex.GetNextToken();
                        epikey = lex.state_.string;
                    }
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
            value = lex.state_.string;
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
                {
                    std::pair<int16_t, int16_t> nums = ActorNames[actor_check];
                    if (nums.second != -1) // DoomEd number exists already
                        actor_num = nums.second;
                    else // See if modified by Dehacked, else skip
                    {
                        for (const MapObjectDefinition *mob : mobjtypes)
                        {
                            if (mob->deh_thing_id_ == nums.first)
                            {
                                actor_num = mob->number_;
                                break;
                            }
                        }
                    }
                }
                if (actor_num == -1)
                    lex.SkipLine();
                else
                {
                    if (!lex.CheckToken(','))
                        FatalError("UMAPINFO: \"bossaction\" key missing line "
                                   "special!\n");
                    lex.GetNextToken();
                    special = lex.state_.number;
                    if (!lex.CheckToken(','))
                        FatalError("UMAPINFO: \"bossaction\" key missing tag!\n");
                    lex.GetNextToken();
                    tag = lex.state_.number;
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
            value = lex.state_.string;
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

void ParseUMAPINFO(const std::string &buffer)
{
    epi::Scanner lex(buffer);

    while(lex.TokensLeft())
    {
        if (!lex.GetNextToken())
            break;

        if (lex.state_.token != epi::Scanner::kIdentifier || epi::StringCaseCompareASCII(lex.state_.string, "MAP") != 0)
            FatalError("Malformed UMAPINFO lump.\n");

        lex.GetNextToken();

        if (lex.state_.token != epi::Scanner::kIdentifier)
            FatalError("UMAPINFO: No mapname for map entry!\n");

        unsigned int i      = 0;
        MapEntry     parsed;
        EPI_CLEAR_MEMORY(&parsed, MapEntry, 1);
        parsed.mapname      = (char *)calloc(lex.state_.string.size() + 1, sizeof(char));
        epi::CStringCopyMax(parsed.mapname, lex.state_.string.data(), lex.state_.string.size());

        if (!lex.CheckToken('{'))
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