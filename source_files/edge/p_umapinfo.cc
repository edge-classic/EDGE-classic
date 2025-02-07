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
#include "deh_text.h"
#include "epi_ename.h"
#include "epi_scanner.h"
#include "epi_str_compare.h"
#include "epi_str_util.h"

MapList Maps;

static std::unordered_map<int, std::pair<int16_t, int16_t>> ActorNames = {
    {epi::EName::kDoomPlayer, {1, -1}},
    {epi::EName::kZombieMan, {2, 3004}},
    {epi::EName::kShotgunGuy, {3, 9}},
    {epi::EName::kArchvile, {4, 64}},
    {epi::EName::kArchvileFire, {5, -1}},
    {epi::EName::kRevenant, {6, 66}},
    {epi::EName::kRevenantTracer, {7, -1}},
    {epi::EName::kRevenantTracerSmoke, {8, -1}},
    {epi::EName::kFatso, {9, 67}},
    {epi::EName::kFatShot, {10, -1}},
    {epi::EName::kChaingunGuy, {11, 65}},
    {epi::EName::kDoomImp, {12, 3001}},
    {epi::EName::kDemon, {13, 3002}},
    {epi::EName::kSpectre, {14, 58}},
    {epi::EName::kCacodemon, {15, 3005}},
    {epi::EName::kBaronOfHell, {16, 3003}},
    {epi::EName::kBaronBall, {17, -1}},
    {epi::EName::kHellKnight, {18, 69}},
    {epi::EName::kLostSoul, {19, 3006}},
    {epi::EName::kSpiderMastermind, {20, 7}},
    {epi::EName::kArachnotron, {21, 68}},
    {epi::EName::kCyberdemon, {22, 16}},
    {epi::EName::kPainElemental, {23, 71}},
    {epi::EName::kWolfensteinSS, {24, 84}},
    {epi::EName::kCommanderKeen, {25, 72}},
    {epi::EName::kBossBrain, {26, 88}},
    {epi::EName::kBossEye, {27, 89}},
    {epi::EName::kBossTarget, {28, 87}},
    {epi::EName::kSpawnShot, {29, -1}},
    {epi::EName::kSpawnFire, {30, -1}},
    {epi::EName::kExplosiveBarrel, {31, 2035}},
    {epi::EName::kDoomImpBall, {32, -1}},
    {epi::EName::kCacodemonBall, {33, -1}},
    {epi::EName::kRocket, {34, -1}},
    {epi::EName::kPlasmaBall, {35, -1}},
    {epi::EName::kBFGBall, {36, -1}},
    {epi::EName::kArachnotronPlasma, {37, -1}},
    {epi::EName::kBulletPuff, {38, -1}},
    {epi::EName::kBlood, {39, -1}},
    {epi::EName::kTeleportFog, {40, -1}},
    {epi::EName::kItemFog, {41, -1}},
    {epi::EName::kTeleportDest, {42, 14}},
    {epi::EName::kBFGExtra, {43, -1}},
    {epi::EName::kGreenArmor, {44, 2018}},
    {epi::EName::kBlueArmor, {45, 2019}},
    {epi::EName::kHealthBonus, {46, 2014}},
    {epi::EName::kArmorBonus, {47, 2015}},
    {epi::EName::kBlueCard, {48, 5}},
    {epi::EName::kRedCard, {49, 13}},
    {epi::EName::kYellowCard, {50, 6}},
    {epi::EName::kYellowSkull, {51, 39}},
    {epi::EName::kRedSkull, {52, 38}},
    {epi::EName::kBlueSkull, {53, 40}},
    {epi::EName::kStimpack, {54, 2011}},
    {epi::EName::kMedikit, {55, 2012}},
    {epi::EName::kSoulsphere, {56, 2013}},
    {epi::EName::kInvulnerabilitySphere, {57, 2022}},
    {epi::EName::kBerserk, {58, 2023}},
    {epi::EName::kBlurSphere, {59, 2024}},
    {epi::EName::kRadSuit, {60, 2025}},
    {epi::EName::kAllmap, {61, 2026}},
    {epi::EName::kInfrared, {62, 2045}},
    {epi::EName::kMegasphere, {63, 83}},
    {epi::EName::kClip, {64, 2007}},
    {epi::EName::kClipBox, {65, 2048}},
    {epi::EName::kRocketAmmo, {66, 2010}},
    {epi::EName::kRocketBox, {67, 2046}},
    {epi::EName::kCell, {68, 2047}},
    {epi::EName::kCellPack, {69, 17}},
    {epi::EName::kShell, {70, 2008}},
    {epi::EName::kShellBox, {71, 2049}},
    {epi::EName::kBackpack, {72, 8}},
    {epi::EName::kBFG9000, {73, 2006}},
    {epi::EName::kChaingun, {74, 2002}},
    {epi::EName::kChainsaw, {75, 2005}},
    {epi::EName::kRocketLauncher, {76, 2003}},
    {epi::EName::kPlasmaRifle, {77, 2004}},
    {epi::EName::kShotgun, {78, 2001}},
    {epi::EName::kSuperShotgun, {79, 82}},
    {epi::EName::kTechLamp, {80, 85}},
    {epi::EName::kTechLamp2, {81, 86}},
    {epi::EName::kColumn, {82, 2028}},
    {epi::EName::kTallGreenColumn, {83, 30}},
    {epi::EName::kShortGreenColumn, {84, 31}},
    {epi::EName::kTallRedColumn, {85, 32}},
    {epi::EName::kShortRedColumn, {86, 33}},
    {epi::EName::kSkullColumn, {87, 37}},
    {epi::EName::kHeartColumn, {88, 36}},
    {epi::EName::kEvilEye, {89, 41}},
    {epi::EName::kFloatingSkull, {90, 42}},
    {epi::EName::kTorchTree, {91, 43}},
    {epi::EName::kBlueTorch, {92, 44}},
    {epi::EName::kGreenTorch, {93, 45}},
    {epi::EName::kRedTorch, {94, 46}},
    {epi::EName::kShortBlueTorch, {95, 55}},
    {epi::EName::kShortGreenTorch, {96, 56}},
    {epi::EName::kShortRedTorch, {97, 57}},
    {epi::EName::kStalagtite, {98, 47}},
    {epi::EName::kTechPillar, {99, 48}},
    {epi::EName::kCandleStick, {100, 34}},
    {epi::EName::kCandelabra, {101, 35}},
    {epi::EName::kBloodyTwitch, {102, 49}},
    {epi::EName::kMeat2, {103, 50}},
    {epi::EName::kMeat3, {104, 51}},
    {epi::EName::kMeat4, {105, 52}},
    {epi::EName::kMeat5, {106, 53}},
    {epi::EName::kNonsolidMeat2, {107, 59}},
    {epi::EName::kNonsolidMeat4, {108, 60}},
    {epi::EName::kNonsolidMeat3, {109, 61}},
    {epi::EName::kNonsolidMeat5, {110, 62}},
    {epi::EName::kNonsolidTwitch, {111, 63}},
    {epi::EName::kDeadCacodemon, {112, 22}},
    {epi::EName::kDeadMarine, {113, 15}},
    {epi::EName::kDeadZombieMan, {114, 18}},
    {epi::EName::kDeadDemon, {115, 21}},
    {epi::EName::kDeadLostSoul, {116, 23}},
    {epi::EName::kDeadDoomImp, {117, 20}},
    {epi::EName::kDeadShotgunGuy, {118, 19}},
    {epi::EName::kGibbedMarine, {119, 10}},
    {epi::EName::kGibbedMarineExtra, {120, 12}},
    {epi::EName::kHeadsOnAStick, {121, 28}},
    {epi::EName::kGibs, {122, 24}},
    {epi::EName::kHeadOnAStick, {123, 27}},
    {epi::EName::kHeadCandles, {124, 29}},
    {epi::EName::kDeadStick, {125, 25}},
    {epi::EName::kLiveStick, {126, 26}},
    {epi::EName::kBigTree, {127, 54}},
    {epi::EName::kBurningBarrel, {128, 70}},
    {epi::EName::kHangNoGuts, {129, 73}},
    {epi::EName::kHangBNoBrain, {130, 74}},
    {epi::EName::kHangTLookingDown, {131, 75}},
    {epi::EName::kHangTSkull, {132, 76}},
    {epi::EName::kHangTLookingUp, {133, 77}},
    {epi::EName::kHangTNoBrain, {134, 78}},
    {epi::EName::kColonGibs, {135, 79}},
    {epi::EName::kSmallBloodPool, {136, 80}},
    {epi::EName::kBrainStem, {137, 81}},
    // Boom/MBF additions
    {epi::EName::kPointPusher, {138, 5001}},
    {epi::EName::kPointPuller, {139, 5002}},
    {epi::EName::kMBFHelperDog, {140, 888}},
    {epi::EName::kPlasmaBall1, {141, -1}},
    {epi::EName::kPlasmaBall2, {142, -1}},
    {epi::EName::kEvilSceptre, {143, -1}},
    {epi::EName::kUnholyBible, {144, -1}},
    {epi::EName::kMusicChanger, {145, -1}},
    {epi::EName::kDeh_Actor_145, {145, -1}},
    {epi::EName::kDeh_Actor_146, {146, -1}},
    {epi::EName::kDeh_Actor_147, {147, -1}},
    {epi::EName::kDeh_Actor_148, {148, -1}},
    {epi::EName::kDeh_Actor_149, {149, -1}},
    // DEHEXTRA Actors start here
    {epi::EName::kDeh_Actor_150, {151, -1}}, // MT_EXTRA0
    {epi::EName::kDeh_Actor_151, {152, -1}}, // MT_EXTRA1
    {epi::EName::kDeh_Actor_152, {153, -1}}, // MT_EXTRA2
    {epi::EName::kDeh_Actor_153, {154, -1}}, // MT_EXTRA3
    {epi::EName::kDeh_Actor_154, {155, -1}}, // MT_EXTRA4
    {epi::EName::kDeh_Actor_155, {156, -1}}, // MT_EXTRA5
    {epi::EName::kDeh_Actor_156, {157, -1}}, // MT_EXTRA6
    {epi::EName::kDeh_Actor_157, {158, -1}}, // MT_EXTRA7
    {epi::EName::kDeh_Actor_158, {159, -1}}, // MT_EXTRA8
    {epi::EName::kDeh_Actor_159, {160, -1}}, // MT_EXTRA9
    {epi::EName::kDeh_Actor_160, {161, -1}}, // MT_EXTRA10
    {epi::EName::kDeh_Actor_161, {162, -1}}, // MT_EXTRA11
    {epi::EName::kDeh_Actor_162, {163, -1}}, // MT_EXTRA12
    {epi::EName::kDeh_Actor_163, {164, -1}}, // MT_EXTRA13
    {epi::EName::kDeh_Actor_164, {165, -1}}, // MT_EXTRA14
    {epi::EName::kDeh_Actor_165, {166, -1}}, // MT_EXTRA15
    {epi::EName::kDeh_Actor_166, {167, -1}}, // MT_EXTRA16
    {epi::EName::kDeh_Actor_167, {168, -1}}, // MT_EXTRA17
    {epi::EName::kDeh_Actor_168, {169, -1}}, // MT_EXTRA18
    {epi::EName::kDeh_Actor_169, {170, -1}}, // MT_EXTRA19
    {epi::EName::kDeh_Actor_170, {171, -1}}, // MT_EXTRA20
    {epi::EName::kDeh_Actor_171, {172, -1}}, // MT_EXTRA21
    {epi::EName::kDeh_Actor_172, {173, -1}}, // MT_EXTRA22
    {epi::EName::kDeh_Actor_173, {174, -1}}, // MT_EXTRA23
    {epi::EName::kDeh_Actor_174, {175, -1}}, // MT_EXTRA24
    {epi::EName::kDeh_Actor_175, {176, -1}}, // MT_EXTRA25
    {epi::EName::kDeh_Actor_176, {177, -1}}, // MT_EXTRA26
    {epi::EName::kDeh_Actor_177, {178, -1}}, // MT_EXTRA27
    {epi::EName::kDeh_Actor_178, {179, -1}}, // MT_EXTRA28
    {epi::EName::kDeh_Actor_179, {180, -1}}, // MT_EXTRA29
    {epi::EName::kDeh_Actor_180, {181, -1}}, // MT_EXTRA30
    {epi::EName::kDeh_Actor_181, {182, -1}}, // MT_EXTRA31
    {epi::EName::kDeh_Actor_182, {183, -1}}, // MT_EXTRA32
    {epi::EName::kDeh_Actor_183, {184, -1}}, // MT_EXTRA33
    {epi::EName::kDeh_Actor_184, {185, -1}}, // MT_EXTRA34
    {epi::EName::kDeh_Actor_185, {186, -1}}, // MT_EXTRA35
    {epi::EName::kDeh_Actor_186, {187, -1}}, // MT_EXTRA36
    {epi::EName::kDeh_Actor_187, {188, -1}}, // MT_EXTRA37
    {epi::EName::kDeh_Actor_188, {189, -1}}, // MT_EXTRA38
    {epi::EName::kDeh_Actor_189, {190, -1}}, // MT_EXTRA39
    {epi::EName::kDeh_Actor_190, {191, -1}}, // MT_EXTRA40
    {epi::EName::kDeh_Actor_191, {192, -1}}, // MT_EXTRA41
    {epi::EName::kDeh_Actor_192, {193, -1}}, // MT_EXTRA42
    {epi::EName::kDeh_Actor_193, {194, -1}}, // MT_EXTRA43
    {epi::EName::kDeh_Actor_194, {195, -1}}, // MT_EXTRA44
    {epi::EName::kDeh_Actor_195, {196, -1}}, // MT_EXTRA45
    {epi::EName::kDeh_Actor_196, {197, -1}}, // MT_EXTRA46
    {epi::EName::kDeh_Actor_197, {198, -1}}, // MT_EXTRA47
    {epi::EName::kDeh_Actor_198, {199, -1}}, // MT_EXTRA48
    {epi::EName::kDeh_Actor_199, {200, -1}}, // MT_EXTRA49
    {epi::EName::kDeh_Actor_200, {201, -1}}, // MT_EXTRA50
    {epi::EName::kDeh_Actor_201, {202, -1}}, // MT_EXTRA51
    {epi::EName::kDeh_Actor_202, {203, -1}}, // MT_EXTRA52
    {epi::EName::kDeh_Actor_203, {204, -1}}, // MT_EXTRA53
    {epi::EName::kDeh_Actor_204, {205, -1}}, // MT_EXTRA54
    {epi::EName::kDeh_Actor_205, {206, -1}}, // MT_EXTRA55
    {epi::EName::kDeh_Actor_206, {207, -1}}, // MT_EXTRA56
    {epi::EName::kDeh_Actor_207, {208, -1}}, // MT_EXTRA57
    {epi::EName::kDeh_Actor_208, {209, -1}}, // MT_EXTRA58
    {epi::EName::kDeh_Actor_209, {210, -1}}, // MT_EXTRA59
    {epi::EName::kDeh_Actor_210, {211, -1}}, // MT_EXTRA60
    {epi::EName::kDeh_Actor_211, {212, -1}}, // MT_EXTRA61
    {epi::EName::kDeh_Actor_212, {213, -1}}, // MT_EXTRA62
    {epi::EName::kDeh_Actor_213, {214, -1}}, // MT_EXTRA63
    {epi::EName::kDeh_Actor_214, {215, -1}}, // MT_EXTRA64
    {epi::EName::kDeh_Actor_215, {216, -1}}, // MT_EXTRA65
    {epi::EName::kDeh_Actor_216, {217, -1}}, // MT_EXTRA66
    {epi::EName::kDeh_Actor_217, {218, -1}}, // MT_EXTRA67
    {epi::EName::kDeh_Actor_218, {219, -1}}, // MT_EXTRA68
    {epi::EName::kDeh_Actor_219, {220, -1}}, // MT_EXTRA69
    {epi::EName::kDeh_Actor_220, {221, -1}}, // MT_EXTRA70
    {epi::EName::kDeh_Actor_221, {222, -1}}, // MT_EXTRA71
    {epi::EName::kDeh_Actor_222, {223, -1}}, // MT_EXTRA72
    {epi::EName::kDeh_Actor_223, {224, -1}}, // MT_EXTRA73
    {epi::EName::kDeh_Actor_224, {225, -1}}, // MT_EXTRA74
    {epi::EName::kDeh_Actor_225, {226, -1}}, // MT_EXTRA75
    {epi::EName::kDeh_Actor_226, {227, -1}}, // MT_EXTRA76
    {epi::EName::kDeh_Actor_227, {228, -1}}, // MT_EXTRA77
    {epi::EName::kDeh_Actor_228, {229, -1}}, // MT_EXTRA78
    {epi::EName::kDeh_Actor_229, {230, -1}}, // MT_EXTRA79
    {epi::EName::kDeh_Actor_230, {231, -1}}, // MT_EXTRA80
    {epi::EName::kDeh_Actor_231, {232, -1}}, // MT_EXTRA81
    {epi::EName::kDeh_Actor_232, {233, -1}}, // MT_EXTRA82
    {epi::EName::kDeh_Actor_233, {234, -1}}, // MT_EXTRA83
    {epi::EName::kDeh_Actor_234, {235, -1}}, // MT_EXTRA84
    {epi::EName::kDeh_Actor_235, {236, -1}}, // MT_EXTRA85
    {epi::EName::kDeh_Actor_236, {237, -1}}, // MT_EXTRA86
    {epi::EName::kDeh_Actor_237, {238, -1}}, // MT_EXTRA87
    {epi::EName::kDeh_Actor_238, {239, -1}}, // MT_EXTRA88
    {epi::EName::kDeh_Actor_239, {240, -1}}, // MT_EXTRA89
    {epi::EName::kDeh_Actor_240, {241, -1}}, // MT_EXTRA90
    {epi::EName::kDeh_Actor_241, {242, -1}}, // MT_EXTRA91
    {epi::EName::kDeh_Actor_242, {243, -1}}, // MT_EXTRA92
    {epi::EName::kDeh_Actor_243, {244, -1}}, // MT_EXTRA93
    {epi::EName::kDeh_Actor_244, {245, -1}}, // MT_EXTRA94
    {epi::EName::kDeh_Actor_245, {246, -1}}, // MT_EXTRA95
    {epi::EName::kDeh_Actor_246, {247, -1}}, // MT_EXTRA96
    {epi::EName::kDeh_Actor_247, {248, -1}}, // MT_EXTRA97
    {epi::EName::kDeh_Actor_248, {249, -1}}, // MT_EXTRA98
    {epi::EName::kDeh_Actor_249, {250, -1}}  // MT_EXTRA99
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
        case epi::EName::kLevelName: {
            if (val->levelname)
                free(val->levelname);
            value          = lex.state_.string;
            val->levelname = (char *)calloc(value.size() + 1, sizeof(char));
            epi::CStringCopyMax(val->levelname, value.c_str(), value.size());
        }
        break;
        case epi::EName::kLabel: {
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
                value      = lex.state_.string;
                val->label = (char *)calloc(value.size() + 1, sizeof(char));
                epi::CStringCopyMax(val->label, value.c_str(), value.size());
            }
        }
        break;
        case epi::EName::kNext: {
            EPI_CLEAR_MEMORY(val->next_map, char, 9);
            value = lex.state_.string;
            if (value.size() > 8)
                FatalError("UMAPINFO: Mapname for \"next\" over 8 characters!\n");
            epi::CStringCopyMax(val->next_map, value.data(), 8);
        }
        break;
        case epi::EName::kNextSecret: {
            EPI_CLEAR_MEMORY(val->nextsecret, char, 9);
            value = lex.state_.string;
            if (value.size() > 8)
                FatalError("UMAPINFO: Mapname for \"nextsecret\" over 8 "
                           "characters!\n");
            epi::CStringCopyMax(val->nextsecret, value.data(), 8);
        }
        break;
        case epi::EName::kLevelPic: {
            EPI_CLEAR_MEMORY(val->levelpic, char, 9);
            value = lex.state_.string;
            if (value.size() > 8)
                FatalError("UMAPINFO: Entry for \"levelpic\" over 8 "
                           "characters!\n");
            epi::CStringCopyMax(val->levelpic, value.data(), 8);
        }
        break;
        case epi::EName::kSkyTexture: {
            EPI_CLEAR_MEMORY(val->skytexture, char, 9);
            value = lex.state_.string;
            if (value.size() > 8)
                FatalError("UMAPINFO: Entry for \"skytexture\" over 8 "
                           "characters!\n");
            epi::CStringCopyMax(val->skytexture, value.data(), 8);
        }
        break;
        case epi::EName::kMusic: {
            EPI_CLEAR_MEMORY(val->music, char, 9);
            value = lex.state_.string;
            if (value.size() > 8)
                FatalError("UMAPINFO: Entry for \"music\" over 8 characters!\n");
            epi::CStringCopyMax(val->music, value.data(), 8);
        }
        break;
        case epi::EName::kEndPic: {
            EPI_CLEAR_MEMORY(val->endpic, char, 9);
            value = lex.state_.string;
            if (value.size() > 8)
                FatalError("UMAPINFO: Entry for \"endpic\" over 8 characters!\n");
            epi::CStringCopyMax(val->endpic, value.data(), 8);
        }
        break;
        case epi::EName::kEndCast:
            val->docast = lex.state_.boolean;
            break;
        case epi::EName::kEndBunny:
            val->dobunny = lex.state_.boolean;
            break;
        case epi::EName::kEndGame:
            val->endgame = lex.state_.boolean;
            break;
        case epi::EName::kExitPic: {
            EPI_CLEAR_MEMORY(val->exitpic, char, 9);
            value = lex.state_.string;
            if (value.size() > 8)
                FatalError("UMAPINFO: Entry for \"exitpic\" over 8 characters!\n");
            epi::CStringCopyMax(val->exitpic, value.data(), 8);
        }
        break;
        case epi::EName::kEnterPic: {
            EPI_CLEAR_MEMORY(val->enterpic, char, 9);
            value = lex.state_.string;
            if (value.size() > 8)
                FatalError("UMAPINFO: Entry for \"enterpic\" over 8 "
                           "characters!\n");
            epi::CStringCopyMax(val->enterpic, value.data(), 8);
        }
        break;
        case epi::EName::kNoIntermission:
            val->nointermission = lex.state_.boolean;
            break;
        case epi::EName::kParTime:
            val->partime = 35 * lex.state_.number;
            break;
        case epi::EName::kIntertext: {
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
        case epi::EName::kIntertextSecret: {
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
        case epi::EName::kInterbackdrop: {
            EPI_CLEAR_MEMORY(val->interbackdrop, char, 9);
            value = lex.state_.string;
            if (value.size() > 8)
                FatalError("UMAPINFO: Entry for \"interbackdrop\" over 8 "
                           "characters!\n");
            epi::CStringCopyMax(val->interbackdrop, value.data(), 8);
        }
        break;
        case epi::EName::kIntermusic: {
            EPI_CLEAR_MEMORY(val->intermusic, char, 9);
            value = lex.state_.string;
            if (value.size() > 8)
                FatalError("UMAPINFO: Entry for \"intermusic\" over 8 "
                           "characters!\n");
            epi::CStringCopyMax(val->intermusic, value.data(), 8);
        }
        break;
        case epi::EName::kEpisode: {
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
        case epi::EName::kBossAction: {
            int special = 0;
            int tag     = 0;
            value       = lex.state_.string;
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
                    else                   // See if modified by Dehacked, else skip
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
        case epi::EName::kAuthor: {
            if (val->authorname)
                free(val->authorname);
            value           = lex.state_.string;
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

    while (lex.TokensLeft())
    {
        if (!lex.GetNextToken())
            break;

        if (lex.state_.token != epi::Scanner::kIdentifier || epi::StringCaseCompareASCII(lex.state_.string, "MAP") != 0)
            FatalError("Malformed UMAPINFO lump.\n");

        lex.GetNextToken();

        if (lex.state_.token != epi::Scanner::kIdentifier)
            FatalError("UMAPINFO: No mapname for map entry!\n");

        unsigned int i = 0;
        MapEntry     parsed;
        EPI_CLEAR_MEMORY(&parsed, MapEntry, 1);
        parsed.mapname = (char *)calloc(lex.state_.string.size() + 1, sizeof(char));
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