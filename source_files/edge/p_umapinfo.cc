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
#include "epi_scanner.h"
#include "epi_str_compare.h"
#include "epi_str_hash.h"
#include "epi_str_util.h"

MapList Maps;

namespace umapinfo
{

EPI_KNOWN_STRINGHASH(kLevelName, "LEVELNAME")
EPI_KNOWN_STRINGHASH(kLabel, "LABEL")
EPI_KNOWN_STRINGHASH(kNext, "NEXT")
EPI_KNOWN_STRINGHASH(kNextSecret, "NEXTSECRET")
EPI_KNOWN_STRINGHASH(kLevelPic, "LEVELPIC")
EPI_KNOWN_STRINGHASH(kSkyTexture, "SKYTEXTURE")
EPI_KNOWN_STRINGHASH(kMusic, "MUSIC")
EPI_KNOWN_STRINGHASH(kEndPic, "ENDPIC")
EPI_KNOWN_STRINGHASH(kEndCast, "ENDCAST")
EPI_KNOWN_STRINGHASH(kEndBunny, "ENDBUNNY")
EPI_KNOWN_STRINGHASH(kEndGame, "ENDGAME")
EPI_KNOWN_STRINGHASH(kExitPic, "EXITPIC")
EPI_KNOWN_STRINGHASH(kEnterPic, "ENTERPIC")
EPI_KNOWN_STRINGHASH(kNoIntermission, "NOINTERMISSION")
EPI_KNOWN_STRINGHASH(kParTime, "PARTIME")
EPI_KNOWN_STRINGHASH(kIntertext, "INTERTEXT")
EPI_KNOWN_STRINGHASH(kIntertextSecret, "INTERTEXTSECRET")
EPI_KNOWN_STRINGHASH(kInterbackdrop, "INTERBACKDROP")
EPI_KNOWN_STRINGHASH(kIntermusic, "INTERMUSIC")
EPI_KNOWN_STRINGHASH(kEpisode, "EPISODE")
EPI_KNOWN_STRINGHASH(kBossAction, "BOSSACTION")
EPI_KNOWN_STRINGHASH(kAuthor, "AUTHOR")
EPI_KNOWN_STRINGHASH(kDoomPlayer, "DOOMPLAYER")
EPI_KNOWN_STRINGHASH(kZombieMan, "ZOMBIEMAN")
EPI_KNOWN_STRINGHASH(kShotgunGuy, "SHOTGUNGUY")
EPI_KNOWN_STRINGHASH(kArchvile, "ARCHVILE")
EPI_KNOWN_STRINGHASH(kArchvileFire, "ARCHVILEFIRE")
EPI_KNOWN_STRINGHASH(kRevenant, "REVENANT")
EPI_KNOWN_STRINGHASH(kRevenantTracer, "REVENANTTRACER")
EPI_KNOWN_STRINGHASH(kRevenantTracerSmoke, "REVENANTTRACERSMOKE")
EPI_KNOWN_STRINGHASH(kFatso, "FATSO")
EPI_KNOWN_STRINGHASH(kFatShot, "FATSHOT")
EPI_KNOWN_STRINGHASH(kChaingunGuy, "CHAINGUNGUY")
EPI_KNOWN_STRINGHASH(kDoomImp, "DOOMIMP")
EPI_KNOWN_STRINGHASH(kDemon, "DEMON")
EPI_KNOWN_STRINGHASH(kSpectre, "SPECTRE")
EPI_KNOWN_STRINGHASH(kCacodemon, "CACODEMON")
EPI_KNOWN_STRINGHASH(kBaronOfHell, "BARONOFHELL")
EPI_KNOWN_STRINGHASH(kBaronBall, "BARONBALL")
EPI_KNOWN_STRINGHASH(kHellKnight, "HELLKNIGHT")
EPI_KNOWN_STRINGHASH(kLostSoul, "LOSTSOUL")
EPI_KNOWN_STRINGHASH(kSpiderMastermind, "SPIDERMASTERMIND")
EPI_KNOWN_STRINGHASH(kArachnotron, "ARACHNOTRON")
EPI_KNOWN_STRINGHASH(kCyberdemon, "CYBERDEMON")
EPI_KNOWN_STRINGHASH(kPainElemental, "PAINELEMENTAL")
EPI_KNOWN_STRINGHASH(kWolfensteinSS, "WOLFENSTEINSS")
EPI_KNOWN_STRINGHASH(kCommanderKeen, "COMMANDERKEEN")
EPI_KNOWN_STRINGHASH(kBossBrain, "BOSSBRAIN")
EPI_KNOWN_STRINGHASH(kBossEye, "BOSSEYE")
EPI_KNOWN_STRINGHASH(kBossTarget, "BOSSTARGET")
EPI_KNOWN_STRINGHASH(kSpawnShot, "SPAWNSHOT")
EPI_KNOWN_STRINGHASH(kSpawnFire, "SPAWNFIRE")
EPI_KNOWN_STRINGHASH(kExplosiveBarrel, "EXPLOSIVEBARREL")
EPI_KNOWN_STRINGHASH(kDoomImpBall, "DOOMIMPBALL")
EPI_KNOWN_STRINGHASH(kCacodemonBall, "CACODEMONBALL")
EPI_KNOWN_STRINGHASH(kRocket, "ROCKET")
EPI_KNOWN_STRINGHASH(kPlasmaBall, "PLASMABALL")
EPI_KNOWN_STRINGHASH(kBFGBall, "BFGBALL")
EPI_KNOWN_STRINGHASH(kArachnotronPlasma, "ARACHNOTRONPLASMA")
EPI_KNOWN_STRINGHASH(kBulletPuff, "BULLETPUFF")
EPI_KNOWN_STRINGHASH(kBlood, "BLOOD")
EPI_KNOWN_STRINGHASH(kTeleportFog, "TELEPORTFOG")
EPI_KNOWN_STRINGHASH(kItemFog, "ITEMFOG")
EPI_KNOWN_STRINGHASH(kTeleportDest, "TELEPORTDEST")
EPI_KNOWN_STRINGHASH(kBFGExtra, "BFGEXTRA")
EPI_KNOWN_STRINGHASH(kGreenArmor, "GREENARMOR")
EPI_KNOWN_STRINGHASH(kBlueArmor, "BLUEARMOR")
EPI_KNOWN_STRINGHASH(kHealthBonus, "HEALTHBONUS")
EPI_KNOWN_STRINGHASH(kArmorBonus, "ARMORBONUS")
EPI_KNOWN_STRINGHASH(kBlueCard, "BLUECARD")
EPI_KNOWN_STRINGHASH(kRedCard, "REDCARD")
EPI_KNOWN_STRINGHASH(kYellowCard, "YELLOWCARD")
EPI_KNOWN_STRINGHASH(kYellowSkull, "YELLOWSKULL")
EPI_KNOWN_STRINGHASH(kRedSkull, "REDSKULL")
EPI_KNOWN_STRINGHASH(kBlueSkull, "BLUESKULL")
EPI_KNOWN_STRINGHASH(kStimpack, "STIMPACK")
EPI_KNOWN_STRINGHASH(kMedikit, "MEDIKIT")
EPI_KNOWN_STRINGHASH(kSoulsphere, "SOULSPHERE")
EPI_KNOWN_STRINGHASH(kInvulnerabilitySphere, "INVULNERABILITYSPHERE")
EPI_KNOWN_STRINGHASH(kBerserk, "BERSERK")
EPI_KNOWN_STRINGHASH(kBlurSphere, "BLURSPHERE")
EPI_KNOWN_STRINGHASH(kRadSuit, "RADSUIT")
EPI_KNOWN_STRINGHASH(kAllmap, "ALLMAP")
EPI_KNOWN_STRINGHASH(kInfrared, "INFRARED")
EPI_KNOWN_STRINGHASH(kMegasphere, "MEGASPHERE")
EPI_KNOWN_STRINGHASH(kClip, "CLIP")
EPI_KNOWN_STRINGHASH(kClipBox, "CLIPBOX")
EPI_KNOWN_STRINGHASH(kRocketAmmo, "ROCKETAMMO")
EPI_KNOWN_STRINGHASH(kRocketBox, "ROCKETBOX")
EPI_KNOWN_STRINGHASH(kCell, "CELL")
EPI_KNOWN_STRINGHASH(kCellPack, "CELLPACK")
EPI_KNOWN_STRINGHASH(kShell, "SHELL")
EPI_KNOWN_STRINGHASH(kShellBox, "SHELLBOX")
EPI_KNOWN_STRINGHASH(kBackpack, "BACKPACK")
EPI_KNOWN_STRINGHASH(kBFG9000, "BFG9000")
EPI_KNOWN_STRINGHASH(kChaingun, "CHAINGUN")
EPI_KNOWN_STRINGHASH(kChainsaw, "CHAINSAW")
EPI_KNOWN_STRINGHASH(kRocketLauncher, "ROCKETLAUNCHER")
EPI_KNOWN_STRINGHASH(kPlasmaRifle, "PLASMARIFLE")
EPI_KNOWN_STRINGHASH(kShotgun, "SHOTGUN")
EPI_KNOWN_STRINGHASH(kSuperShotgun, "SUPERSHOTGUN")
EPI_KNOWN_STRINGHASH(kTechLamp, "TECHLAMP")
EPI_KNOWN_STRINGHASH(kTechLamp2, "TECHLAMP2")
EPI_KNOWN_STRINGHASH(kColumn, "COLUMN")
EPI_KNOWN_STRINGHASH(kTallGreenColumn, "TALLGREENCOLUMN")
EPI_KNOWN_STRINGHASH(kShortGreenColumn, "SHORTGREENCOLUMN")
EPI_KNOWN_STRINGHASH(kTallRedColumn, "TALLREDCOLUMN")
EPI_KNOWN_STRINGHASH(kShortRedColumn, "SHORTREDCOLUMN")
EPI_KNOWN_STRINGHASH(kSkullColumn, "SKULLCOLUMN")
EPI_KNOWN_STRINGHASH(kHeartColumn, "HEARTCOLUMN")
EPI_KNOWN_STRINGHASH(kEvilEye, "EVILEYE")
EPI_KNOWN_STRINGHASH(kFloatingSkull, "FLOATINGSKULL")
EPI_KNOWN_STRINGHASH(kTorchTree, "TORCHTREE")
EPI_KNOWN_STRINGHASH(kBlueTorch, "BLUETORCH")
EPI_KNOWN_STRINGHASH(kGreenTorch, "GREENTORCH")
EPI_KNOWN_STRINGHASH(kRedTorch, "REDTORCH")
EPI_KNOWN_STRINGHASH(kShortBlueTorch, "SHORTBLUETORCH")
EPI_KNOWN_STRINGHASH(kShortGreenTorch, "SHORTGREENTORCH")
EPI_KNOWN_STRINGHASH(kShortRedTorch, "SHORTREDTORCH")
EPI_KNOWN_STRINGHASH(kStalagtite, "STALAGTITE")
EPI_KNOWN_STRINGHASH(kTechPillar, "TECHPILLAR")
EPI_KNOWN_STRINGHASH(kCandleStick, "CANDLESTICK")
EPI_KNOWN_STRINGHASH(kCandelabra, "CANDELABRA")
EPI_KNOWN_STRINGHASH(kBloodyTwitch, "BLOODYTWITCH")
EPI_KNOWN_STRINGHASH(kMeat2, "MEAT2")
EPI_KNOWN_STRINGHASH(kMeat3, "MEAT3")
EPI_KNOWN_STRINGHASH(kMeat4, "MEAT4")
EPI_KNOWN_STRINGHASH(kMeat5, "MEAT5")
EPI_KNOWN_STRINGHASH(kNonsolidMeat2, "NONSOLIDMEAT2")
EPI_KNOWN_STRINGHASH(kNonsolidMeat4, "NONSOLIDMEAT4")
EPI_KNOWN_STRINGHASH(kNonsolidMeat3, "NONSOLIDMEAT3")
EPI_KNOWN_STRINGHASH(kNonsolidMeat5, "NONSOLIDMEAT5")
EPI_KNOWN_STRINGHASH(kNonsolidTwitch, "NONSOLIDTWITCH")
EPI_KNOWN_STRINGHASH(kDeadCacodemon, "DEADCACODEMON")
EPI_KNOWN_STRINGHASH(kDeadMarine, "DEADMARINE")
EPI_KNOWN_STRINGHASH(kDeadZombieMan, "DEADZOMBIEMAN")
EPI_KNOWN_STRINGHASH(kDeadDemon, "DEADDEMON")
EPI_KNOWN_STRINGHASH(kDeadLostSoul, "DEADLOSTSOUL")
EPI_KNOWN_STRINGHASH(kDeadDoomImp, "DEADDOOMIMP")
EPI_KNOWN_STRINGHASH(kDeadShotgunGuy, "DEADSHOTGUNGUY")
EPI_KNOWN_STRINGHASH(kGibbedMarine, "GIBBEDMARINE")
EPI_KNOWN_STRINGHASH(kGibbedMarineExtra, "GIBBEDMARINEEXTRA")
EPI_KNOWN_STRINGHASH(kHeadsOnAStick, "HEADSONASTICK")
EPI_KNOWN_STRINGHASH(kGibs, "GIBS")
EPI_KNOWN_STRINGHASH(kHeadOnAStick, "HEADONASTICK")
EPI_KNOWN_STRINGHASH(kHeadCandles, "HEADCANDLES")
EPI_KNOWN_STRINGHASH(kDeadStick, "DEADSTICK")
EPI_KNOWN_STRINGHASH(kLiveStick, "LIVESTICK")
EPI_KNOWN_STRINGHASH(kBigTree, "BIGTREE")
EPI_KNOWN_STRINGHASH(kBurningBarrel, "BURNINGBARREL")
EPI_KNOWN_STRINGHASH(kHangNoGuts, "HANGNOGUTS")
EPI_KNOWN_STRINGHASH(kHangBNoBrain, "HANGBNOBRAIN")
EPI_KNOWN_STRINGHASH(kHangTLookingDown, "HANGTLOOKINGDOWN")
EPI_KNOWN_STRINGHASH(kHangTSkull, "HANGTSKULL")
EPI_KNOWN_STRINGHASH(kHangTLookingUp, "HANGTLOOKINGUP")
EPI_KNOWN_STRINGHASH(kHangTNoBrain, "HANGTNOBRAIN")
EPI_KNOWN_STRINGHASH(kColonGibs, "COLONGIBS")
EPI_KNOWN_STRINGHASH(kSmallBloodPool, "SMALLBLOODPOOL")
EPI_KNOWN_STRINGHASH(kBrainStem, "BRAINSTEM")
EPI_KNOWN_STRINGHASH(kPointPusher, "POINTPUSHER")
EPI_KNOWN_STRINGHASH(kPointPuller, "POINTPULLER")
EPI_KNOWN_STRINGHASH(kMBFHelperDog, "MBFHELPERDOG")
EPI_KNOWN_STRINGHASH(kPlasmaBall1, "PLASMABALL1")
EPI_KNOWN_STRINGHASH(kPlasmaBall2, "PLASMABALL2")
EPI_KNOWN_STRINGHASH(kEvilSceptre, "EVILSCEPTRE")
EPI_KNOWN_STRINGHASH(kUnholyBible, "UNHOLYBIBLE")
EPI_KNOWN_STRINGHASH(kMusicChanger, "MUSICCHANGER")
EPI_KNOWN_STRINGHASH(kDehActor145, "DEH_ACTOR_145")
EPI_KNOWN_STRINGHASH(kDehActor146, "DEH_ACTOR_146")
EPI_KNOWN_STRINGHASH(kDehActor147, "DEH_ACTOR_147")
EPI_KNOWN_STRINGHASH(kDehActor148, "DEH_ACTOR_148")
EPI_KNOWN_STRINGHASH(kDehActor149, "DEH_ACTOR_149")
EPI_KNOWN_STRINGHASH(kDehActor150, "DEH_ACTOR_150")
EPI_KNOWN_STRINGHASH(kDehActor151, "DEH_ACTOR_151")
EPI_KNOWN_STRINGHASH(kDehActor152, "DEH_ACTOR_152")
EPI_KNOWN_STRINGHASH(kDehActor153, "DEH_ACTOR_153")
EPI_KNOWN_STRINGHASH(kDehActor154, "DEH_ACTOR_154")
EPI_KNOWN_STRINGHASH(kDehActor155, "DEH_ACTOR_155")
EPI_KNOWN_STRINGHASH(kDehActor156, "DEH_ACTOR_156")
EPI_KNOWN_STRINGHASH(kDehActor157, "DEH_ACTOR_157")
EPI_KNOWN_STRINGHASH(kDehActor158, "DEH_ACTOR_158")
EPI_KNOWN_STRINGHASH(kDehActor159, "DEH_ACTOR_159")
EPI_KNOWN_STRINGHASH(kDehActor160, "DEH_ACTOR_160")
EPI_KNOWN_STRINGHASH(kDehActor161, "DEH_ACTOR_161")
EPI_KNOWN_STRINGHASH(kDehActor162, "DEH_ACTOR_162")
EPI_KNOWN_STRINGHASH(kDehActor163, "DEH_ACTOR_163")
EPI_KNOWN_STRINGHASH(kDehActor164, "DEH_ACTOR_164")
EPI_KNOWN_STRINGHASH(kDehActor165, "DEH_ACTOR_165")
EPI_KNOWN_STRINGHASH(kDehActor166, "DEH_ACTOR_166")
EPI_KNOWN_STRINGHASH(kDehActor167, "DEH_ACTOR_167")
EPI_KNOWN_STRINGHASH(kDehActor168, "DEH_ACTOR_168")
EPI_KNOWN_STRINGHASH(kDehActor169, "DEH_ACTOR_169")
EPI_KNOWN_STRINGHASH(kDehActor170, "DEH_ACTOR_170")
EPI_KNOWN_STRINGHASH(kDehActor171, "DEH_ACTOR_171")
EPI_KNOWN_STRINGHASH(kDehActor172, "DEH_ACTOR_172")
EPI_KNOWN_STRINGHASH(kDehActor173, "DEH_ACTOR_173")
EPI_KNOWN_STRINGHASH(kDehActor174, "DEH_ACTOR_174")
EPI_KNOWN_STRINGHASH(kDehActor175, "DEH_ACTOR_175")
EPI_KNOWN_STRINGHASH(kDehActor176, "DEH_ACTOR_176")
EPI_KNOWN_STRINGHASH(kDehActor177, "DEH_ACTOR_177")
EPI_KNOWN_STRINGHASH(kDehActor178, "DEH_ACTOR_178")
EPI_KNOWN_STRINGHASH(kDehActor179, "DEH_ACTOR_179")
EPI_KNOWN_STRINGHASH(kDehActor180, "DEH_ACTOR_180")
EPI_KNOWN_STRINGHASH(kDehActor181, "DEH_ACTOR_181")
EPI_KNOWN_STRINGHASH(kDehActor182, "DEH_ACTOR_182")
EPI_KNOWN_STRINGHASH(kDehActor183, "DEH_ACTOR_183")
EPI_KNOWN_STRINGHASH(kDehActor184, "DEH_ACTOR_184")
EPI_KNOWN_STRINGHASH(kDehActor185, "DEH_ACTOR_185")
EPI_KNOWN_STRINGHASH(kDehActor186, "DEH_ACTOR_186")
EPI_KNOWN_STRINGHASH(kDehActor187, "DEH_ACTOR_187")
EPI_KNOWN_STRINGHASH(kDehActor188, "DEH_ACTOR_188")
EPI_KNOWN_STRINGHASH(kDehActor189, "DEH_ACTOR_189")
EPI_KNOWN_STRINGHASH(kDehActor190, "DEH_ACTOR_190")
EPI_KNOWN_STRINGHASH(kDehActor191, "DEH_ACTOR_191")
EPI_KNOWN_STRINGHASH(kDehActor192, "DEH_ACTOR_192")
EPI_KNOWN_STRINGHASH(kDehActor193, "DEH_ACTOR_193")
EPI_KNOWN_STRINGHASH(kDehActor194, "DEH_ACTOR_194")
EPI_KNOWN_STRINGHASH(kDehActor195, "DEH_ACTOR_195")
EPI_KNOWN_STRINGHASH(kDehActor196, "DEH_ACTOR_196")
EPI_KNOWN_STRINGHASH(kDehActor197, "DEH_ACTOR_197")
EPI_KNOWN_STRINGHASH(kDehActor198, "DEH_ACTOR_198")
EPI_KNOWN_STRINGHASH(kDehActor199, "DEH_ACTOR_199")
EPI_KNOWN_STRINGHASH(kDehActor200, "DEH_ACTOR_200")
EPI_KNOWN_STRINGHASH(kDehActor201, "DEH_ACTOR_201")
EPI_KNOWN_STRINGHASH(kDehActor202, "DEH_ACTOR_202")
EPI_KNOWN_STRINGHASH(kDehActor203, "DEH_ACTOR_203")
EPI_KNOWN_STRINGHASH(kDehActor204, "DEH_ACTOR_204")
EPI_KNOWN_STRINGHASH(kDehActor205, "DEH_ACTOR_205")
EPI_KNOWN_STRINGHASH(kDehActor206, "DEH_ACTOR_206")
EPI_KNOWN_STRINGHASH(kDehActor207, "DEH_ACTOR_207")
EPI_KNOWN_STRINGHASH(kDehActor208, "DEH_ACTOR_208")
EPI_KNOWN_STRINGHASH(kDehActor209, "DEH_ACTOR_209")
EPI_KNOWN_STRINGHASH(kDehActor210, "DEH_ACTOR_210")
EPI_KNOWN_STRINGHASH(kDehActor211, "DEH_ACTOR_211")
EPI_KNOWN_STRINGHASH(kDehActor212, "DEH_ACTOR_212")
EPI_KNOWN_STRINGHASH(kDehActor213, "DEH_ACTOR_213")
EPI_KNOWN_STRINGHASH(kDehActor214, "DEH_ACTOR_214")
EPI_KNOWN_STRINGHASH(kDehActor215, "DEH_ACTOR_215")
EPI_KNOWN_STRINGHASH(kDehActor216, "DEH_ACTOR_216")
EPI_KNOWN_STRINGHASH(kDehActor217, "DEH_ACTOR_217")
EPI_KNOWN_STRINGHASH(kDehActor218, "DEH_ACTOR_218")
EPI_KNOWN_STRINGHASH(kDehActor219, "DEH_ACTOR_219")
EPI_KNOWN_STRINGHASH(kDehActor220, "DEH_ACTOR_220")
EPI_KNOWN_STRINGHASH(kDehActor221, "DEH_ACTOR_221")
EPI_KNOWN_STRINGHASH(kDehActor222, "DEH_ACTOR_222")
EPI_KNOWN_STRINGHASH(kDehActor223, "DEH_ACTOR_223")
EPI_KNOWN_STRINGHASH(kDehActor224, "DEH_ACTOR_224")
EPI_KNOWN_STRINGHASH(kDehActor225, "DEH_ACTOR_225")
EPI_KNOWN_STRINGHASH(kDehActor226, "DEH_ACTOR_226")
EPI_KNOWN_STRINGHASH(kDehActor227, "DEH_ACTOR_227")
EPI_KNOWN_STRINGHASH(kDehActor228, "DEH_ACTOR_228")
EPI_KNOWN_STRINGHASH(kDehActor229, "DEH_ACTOR_229")
EPI_KNOWN_STRINGHASH(kDehActor230, "DEH_ACTOR_230")
EPI_KNOWN_STRINGHASH(kDehActor231, "DEH_ACTOR_231")
EPI_KNOWN_STRINGHASH(kDehActor232, "DEH_ACTOR_232")
EPI_KNOWN_STRINGHASH(kDehActor233, "DEH_ACTOR_233")
EPI_KNOWN_STRINGHASH(kDehActor234, "DEH_ACTOR_234")
EPI_KNOWN_STRINGHASH(kDehActor235, "DEH_ACTOR_235")
EPI_KNOWN_STRINGHASH(kDehActor236, "DEH_ACTOR_236")
EPI_KNOWN_STRINGHASH(kDehActor237, "DEH_ACTOR_237")
EPI_KNOWN_STRINGHASH(kDehActor238, "DEH_ACTOR_238")
EPI_KNOWN_STRINGHASH(kDehActor239, "DEH_ACTOR_239")
EPI_KNOWN_STRINGHASH(kDehActor240, "DEH_ACTOR_240")
EPI_KNOWN_STRINGHASH(kDehActor241, "DEH_ACTOR_241")
EPI_KNOWN_STRINGHASH(kDehActor242, "DEH_ACTOR_242")
EPI_KNOWN_STRINGHASH(kDehActor243, "DEH_ACTOR_243")
EPI_KNOWN_STRINGHASH(kDehActor244, "DEH_ACTOR_244")
EPI_KNOWN_STRINGHASH(kDehActor245, "DEH_ACTOR_245")
EPI_KNOWN_STRINGHASH(kDehActor246, "DEH_ACTOR_246")
EPI_KNOWN_STRINGHASH(kDehActor247, "DEH_ACTOR_247")
EPI_KNOWN_STRINGHASH(kDehActor248, "DEH_ACTOR_248")
EPI_KNOWN_STRINGHASH(kDehActor249, "DEH_ACTOR_249")

} // namespace umapinfo

static std::unordered_map<epi::StringHash, std::pair<int16_t, int16_t>> ActorNames = {
    {umapinfo::kDoomPlayer, {1, -1}},
    {umapinfo::kZombieMan, {2, 3004}},
    {umapinfo::kShotgunGuy, {3, 9}},
    {umapinfo::kArchvile, {4, 64}},
    {umapinfo::kArchvileFire, {5, -1}},
    {umapinfo::kRevenant, {6, 66}},
    {umapinfo::kRevenantTracer, {7, -1}},
    {umapinfo::kRevenantTracerSmoke, {8, -1}},
    {umapinfo::kFatso, {9, 67}},
    {umapinfo::kFatShot, {10, -1}},
    {umapinfo::kChaingunGuy, {11, 65}},
    {umapinfo::kDoomImp, {12, 3001}},
    {umapinfo::kDemon, {13, 3002}},
    {umapinfo::kSpectre, {14, 58}},
    {umapinfo::kCacodemon, {15, 3005}},
    {umapinfo::kBaronOfHell, {16, 3003}},
    {umapinfo::kBaronBall, {17, -1}},
    {umapinfo::kHellKnight, {18, 69}},
    {umapinfo::kLostSoul, {19, 3006}},
    {umapinfo::kSpiderMastermind, {20, 7}},
    {umapinfo::kArachnotron, {21, 68}},
    {umapinfo::kCyberdemon, {22, 16}},
    {umapinfo::kPainElemental, {23, 71}},
    {umapinfo::kWolfensteinSS, {24, 84}},
    {umapinfo::kCommanderKeen, {25, 72}},
    {umapinfo::kBossBrain, {26, 88}},
    {umapinfo::kBossEye, {27, 89}},
    {umapinfo::kBossTarget, {28, 87}},
    {umapinfo::kSpawnShot, {29, -1}},
    {umapinfo::kSpawnFire, {30, -1}},
    {umapinfo::kExplosiveBarrel, {31, 2035}},
    {umapinfo::kDoomImpBall, {32, -1}},
    {umapinfo::kCacodemonBall, {33, -1}},
    {umapinfo::kRocket, {34, -1}},
    {umapinfo::kPlasmaBall, {35, -1}},
    {umapinfo::kBFGBall, {36, -1}},
    {umapinfo::kArachnotronPlasma, {37, -1}},
    {umapinfo::kBulletPuff, {38, -1}},
    {umapinfo::kBlood, {39, -1}},
    {umapinfo::kTeleportFog, {40, -1}},
    {umapinfo::kItemFog, {41, -1}},
    {umapinfo::kTeleportDest, {42, 14}},
    {umapinfo::kBFGExtra, {43, -1}},
    {umapinfo::kGreenArmor, {44, 2018}},
    {umapinfo::kBlueArmor, {45, 2019}},
    {umapinfo::kHealthBonus, {46, 2014}},
    {umapinfo::kArmorBonus, {47, 2015}},
    {umapinfo::kBlueCard, {48, 5}},
    {umapinfo::kRedCard, {49, 13}},
    {umapinfo::kYellowCard, {50, 6}},
    {umapinfo::kYellowSkull, {51, 39}},
    {umapinfo::kRedSkull, {52, 38}},
    {umapinfo::kBlueSkull, {53, 40}},
    {umapinfo::kStimpack, {54, 2011}},
    {umapinfo::kMedikit, {55, 2012}},
    {umapinfo::kSoulsphere, {56, 2013}},
    {umapinfo::kInvulnerabilitySphere, {57, 2022}},
    {umapinfo::kBerserk, {58, 2023}},
    {umapinfo::kBlurSphere, {59, 2024}},
    {umapinfo::kRadSuit, {60, 2025}},
    {umapinfo::kAllmap, {61, 2026}},
    {umapinfo::kInfrared, {62, 2045}},
    {umapinfo::kMegasphere, {63, 83}},
    {umapinfo::kClip, {64, 2007}},
    {umapinfo::kClipBox, {65, 2048}},
    {umapinfo::kRocketAmmo, {66, 2010}},
    {umapinfo::kRocketBox, {67, 2046}},
    {umapinfo::kCell, {68, 2047}},
    {umapinfo::kCellPack, {69, 17}},
    {umapinfo::kShell, {70, 2008}},
    {umapinfo::kShellBox, {71, 2049}},
    {umapinfo::kBackpack, {72, 8}},
    {umapinfo::kBFG9000, {73, 2006}},
    {umapinfo::kChaingun, {74, 2002}},
    {umapinfo::kChainsaw, {75, 2005}},
    {umapinfo::kRocketLauncher, {76, 2003}},
    {umapinfo::kPlasmaRifle, {77, 2004}},
    {umapinfo::kShotgun, {78, 2001}},
    {umapinfo::kSuperShotgun, {79, 82}},
    {umapinfo::kTechLamp, {80, 85}},
    {umapinfo::kTechLamp2, {81, 86}},
    {umapinfo::kColumn, {82, 2028}},
    {umapinfo::kTallGreenColumn, {83, 30}},
    {umapinfo::kShortGreenColumn, {84, 31}},
    {umapinfo::kTallRedColumn, {85, 32}},
    {umapinfo::kShortRedColumn, {86, 33}},
    {umapinfo::kSkullColumn, {87, 37}},
    {umapinfo::kHeartColumn, {88, 36}},
    {umapinfo::kEvilEye, {89, 41}},
    {umapinfo::kFloatingSkull, {90, 42}},
    {umapinfo::kTorchTree, {91, 43}},
    {umapinfo::kBlueTorch, {92, 44}},
    {umapinfo::kGreenTorch, {93, 45}},
    {umapinfo::kRedTorch, {94, 46}},
    {umapinfo::kShortBlueTorch, {95, 55}},
    {umapinfo::kShortGreenTorch, {96, 56}},
    {umapinfo::kShortRedTorch, {97, 57}},
    {umapinfo::kStalagtite, {98, 47}},
    {umapinfo::kTechPillar, {99, 48}},
    {umapinfo::kCandleStick, {100, 34}},
    {umapinfo::kCandelabra, {101, 35}},
    {umapinfo::kBloodyTwitch, {102, 49}},
    {umapinfo::kMeat2, {103, 50}},
    {umapinfo::kMeat3, {104, 51}},
    {umapinfo::kMeat4, {105, 52}},
    {umapinfo::kMeat5, {106, 53}},
    {umapinfo::kNonsolidMeat2, {107, 59}},
    {umapinfo::kNonsolidMeat4, {108, 60}},
    {umapinfo::kNonsolidMeat3, {109, 61}},
    {umapinfo::kNonsolidMeat5, {110, 62}},
    {umapinfo::kNonsolidTwitch, {111, 63}},
    {umapinfo::kDeadCacodemon, {112, 22}},
    {umapinfo::kDeadMarine, {113, 15}},
    {umapinfo::kDeadZombieMan, {114, 18}},
    {umapinfo::kDeadDemon, {115, 21}},
    {umapinfo::kDeadLostSoul, {116, 23}},
    {umapinfo::kDeadDoomImp, {117, 20}},
    {umapinfo::kDeadShotgunGuy, {118, 19}},
    {umapinfo::kGibbedMarine, {119, 10}},
    {umapinfo::kGibbedMarineExtra, {120, 12}},
    {umapinfo::kHeadsOnAStick, {121, 28}},
    {umapinfo::kGibs, {122, 24}},
    {umapinfo::kHeadOnAStick, {123, 27}},
    {umapinfo::kHeadCandles, {124, 29}},
    {umapinfo::kDeadStick, {125, 25}},
    {umapinfo::kLiveStick, {126, 26}},
    {umapinfo::kBigTree, {127, 54}},
    {umapinfo::kBurningBarrel, {128, 70}},
    {umapinfo::kHangNoGuts, {129, 73}},
    {umapinfo::kHangBNoBrain, {130, 74}},
    {umapinfo::kHangTLookingDown, {131, 75}},
    {umapinfo::kHangTSkull, {132, 76}},
    {umapinfo::kHangTLookingUp, {133, 77}},
    {umapinfo::kHangTNoBrain, {134, 78}},
    {umapinfo::kColonGibs, {135, 79}},
    {umapinfo::kSmallBloodPool, {136, 80}},
    {umapinfo::kBrainStem, {137, 81}},
    // Boom/MBF additions
    {umapinfo::kPointPusher, {138, 5001}},
    {umapinfo::kPointPuller, {139, 5002}},
    {umapinfo::kMBFHelperDog, {140, 888}},
    {umapinfo::kPlasmaBall1, {141, -1}},
    {umapinfo::kPlasmaBall2, {142, -1}},
    {umapinfo::kEvilSceptre, {143, -1}},
    {umapinfo::kUnholyBible, {144, -1}},
    {umapinfo::kMusicChanger, {145, -1}},
    {umapinfo::kDehActor145, {145, -1}},
    {umapinfo::kDehActor146, {146, -1}},
    {umapinfo::kDehActor147, {147, -1}},
    {umapinfo::kDehActor148, {148, -1}},
    {umapinfo::kDehActor149, {149, -1}},
    // DEHEXTRA Actors start here
    {umapinfo::kDehActor150, {151, -1}}, // MT_EXTRA0
    {umapinfo::kDehActor151, {152, -1}}, // MT_EXTRA1
    {umapinfo::kDehActor152, {153, -1}}, // MT_EXTRA2
    {umapinfo::kDehActor153, {154, -1}}, // MT_EXTRA3
    {umapinfo::kDehActor154, {155, -1}}, // MT_EXTRA4
    {umapinfo::kDehActor155, {156, -1}}, // MT_EXTRA5
    {umapinfo::kDehActor156, {157, -1}}, // MT_EXTRA6
    {umapinfo::kDehActor157, {158, -1}}, // MT_EXTRA7
    {umapinfo::kDehActor158, {159, -1}}, // MT_EXTRA8
    {umapinfo::kDehActor159, {160, -1}}, // MT_EXTRA9
    {umapinfo::kDehActor160, {161, -1}}, // MT_EXTRA10
    {umapinfo::kDehActor161, {162, -1}}, // MT_EXTRA11
    {umapinfo::kDehActor162, {163, -1}}, // MT_EXTRA12
    {umapinfo::kDehActor163, {164, -1}}, // MT_EXTRA13
    {umapinfo::kDehActor164, {165, -1}}, // MT_EXTRA14
    {umapinfo::kDehActor165, {166, -1}}, // MT_EXTRA15
    {umapinfo::kDehActor166, {167, -1}}, // MT_EXTRA16
    {umapinfo::kDehActor167, {168, -1}}, // MT_EXTRA17
    {umapinfo::kDehActor168, {169, -1}}, // MT_EXTRA18
    {umapinfo::kDehActor169, {170, -1}}, // MT_EXTRA19
    {umapinfo::kDehActor170, {171, -1}}, // MT_EXTRA20
    {umapinfo::kDehActor171, {172, -1}}, // MT_EXTRA21
    {umapinfo::kDehActor172, {173, -1}}, // MT_EXTRA22
    {umapinfo::kDehActor173, {174, -1}}, // MT_EXTRA23
    {umapinfo::kDehActor174, {175, -1}}, // MT_EXTRA24
    {umapinfo::kDehActor175, {176, -1}}, // MT_EXTRA25
    {umapinfo::kDehActor176, {177, -1}}, // MT_EXTRA26
    {umapinfo::kDehActor177, {178, -1}}, // MT_EXTRA27
    {umapinfo::kDehActor178, {179, -1}}, // MT_EXTRA28
    {umapinfo::kDehActor179, {180, -1}}, // MT_EXTRA29
    {umapinfo::kDehActor180, {181, -1}}, // MT_EXTRA30
    {umapinfo::kDehActor181, {182, -1}}, // MT_EXTRA31
    {umapinfo::kDehActor182, {183, -1}}, // MT_EXTRA32
    {umapinfo::kDehActor183, {184, -1}}, // MT_EXTRA33
    {umapinfo::kDehActor184, {185, -1}}, // MT_EXTRA34
    {umapinfo::kDehActor185, {186, -1}}, // MT_EXTRA35
    {umapinfo::kDehActor186, {187, -1}}, // MT_EXTRA36
    {umapinfo::kDehActor187, {188, -1}}, // MT_EXTRA37
    {umapinfo::kDehActor188, {189, -1}}, // MT_EXTRA38
    {umapinfo::kDehActor189, {190, -1}}, // MT_EXTRA39
    {umapinfo::kDehActor190, {191, -1}}, // MT_EXTRA40
    {umapinfo::kDehActor191, {192, -1}}, // MT_EXTRA41
    {umapinfo::kDehActor192, {193, -1}}, // MT_EXTRA42
    {umapinfo::kDehActor193, {194, -1}}, // MT_EXTRA43
    {umapinfo::kDehActor194, {195, -1}}, // MT_EXTRA44
    {umapinfo::kDehActor195, {196, -1}}, // MT_EXTRA45
    {umapinfo::kDehActor196, {197, -1}}, // MT_EXTRA46
    {umapinfo::kDehActor197, {198, -1}}, // MT_EXTRA47
    {umapinfo::kDehActor198, {199, -1}}, // MT_EXTRA48
    {umapinfo::kDehActor199, {200, -1}}, // MT_EXTRA49
    {umapinfo::kDehActor200, {201, -1}}, // MT_EXTRA50
    {umapinfo::kDehActor201, {202, -1}}, // MT_EXTRA51
    {umapinfo::kDehActor202, {203, -1}}, // MT_EXTRA52
    {umapinfo::kDehActor203, {204, -1}}, // MT_EXTRA53
    {umapinfo::kDehActor204, {205, -1}}, // MT_EXTRA54
    {umapinfo::kDehActor205, {206, -1}}, // MT_EXTRA55
    {umapinfo::kDehActor206, {207, -1}}, // MT_EXTRA56
    {umapinfo::kDehActor207, {208, -1}}, // MT_EXTRA57
    {umapinfo::kDehActor208, {209, -1}}, // MT_EXTRA58
    {umapinfo::kDehActor209, {210, -1}}, // MT_EXTRA59
    {umapinfo::kDehActor210, {211, -1}}, // MT_EXTRA60
    {umapinfo::kDehActor211, {212, -1}}, // MT_EXTRA61
    {umapinfo::kDehActor212, {213, -1}}, // MT_EXTRA62
    {umapinfo::kDehActor213, {214, -1}}, // MT_EXTRA63
    {umapinfo::kDehActor214, {215, -1}}, // MT_EXTRA64
    {umapinfo::kDehActor215, {216, -1}}, // MT_EXTRA65
    {umapinfo::kDehActor216, {217, -1}}, // MT_EXTRA66
    {umapinfo::kDehActor217, {218, -1}}, // MT_EXTRA67
    {umapinfo::kDehActor218, {219, -1}}, // MT_EXTRA68
    {umapinfo::kDehActor219, {220, -1}}, // MT_EXTRA69
    {umapinfo::kDehActor220, {221, -1}}, // MT_EXTRA70
    {umapinfo::kDehActor221, {222, -1}}, // MT_EXTRA71
    {umapinfo::kDehActor222, {223, -1}}, // MT_EXTRA72
    {umapinfo::kDehActor223, {224, -1}}, // MT_EXTRA73
    {umapinfo::kDehActor224, {225, -1}}, // MT_EXTRA74
    {umapinfo::kDehActor225, {226, -1}}, // MT_EXTRA75
    {umapinfo::kDehActor226, {227, -1}}, // MT_EXTRA76
    {umapinfo::kDehActor227, {228, -1}}, // MT_EXTRA77
    {umapinfo::kDehActor228, {229, -1}}, // MT_EXTRA78
    {umapinfo::kDehActor229, {230, -1}}, // MT_EXTRA79
    {umapinfo::kDehActor230, {231, -1}}, // MT_EXTRA80
    {umapinfo::kDehActor231, {232, -1}}, // MT_EXTRA81
    {umapinfo::kDehActor232, {233, -1}}, // MT_EXTRA82
    {umapinfo::kDehActor233, {234, -1}}, // MT_EXTRA83
    {umapinfo::kDehActor234, {235, -1}}, // MT_EXTRA84
    {umapinfo::kDehActor235, {236, -1}}, // MT_EXTRA85
    {umapinfo::kDehActor236, {237, -1}}, // MT_EXTRA86
    {umapinfo::kDehActor237, {238, -1}}, // MT_EXTRA87
    {umapinfo::kDehActor238, {239, -1}}, // MT_EXTRA88
    {umapinfo::kDehActor239, {240, -1}}, // MT_EXTRA89
    {umapinfo::kDehActor240, {241, -1}}, // MT_EXTRA90
    {umapinfo::kDehActor241, {242, -1}}, // MT_EXTRA91
    {umapinfo::kDehActor242, {243, -1}}, // MT_EXTRA92
    {umapinfo::kDehActor243, {244, -1}}, // MT_EXTRA93
    {umapinfo::kDehActor244, {245, -1}}, // MT_EXTRA94
    {umapinfo::kDehActor245, {246, -1}}, // MT_EXTRA95
    {umapinfo::kDehActor246, {247, -1}}, // MT_EXTRA96
    {umapinfo::kDehActor247, {248, -1}}, // MT_EXTRA97
    {umapinfo::kDehActor248, {249, -1}}, // MT_EXTRA98
    {umapinfo::kDehActor249, {250, -1}}  // MT_EXTRA99
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

        epi::StringHash key_hash(key);

        switch (key_hash.Value())
        {
        case umapinfo::kLevelName: {
            if (val->levelname)
                free(val->levelname);
            value          = lex.state_.string;
            val->levelname = (char *)calloc(value.size() + 1, sizeof(char));
            epi::CStringCopyMax(val->levelname, value.c_str(), value.size());
        }
        break;
        case umapinfo::kLabel: {
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
        case umapinfo::kNext: {
            EPI_CLEAR_MEMORY(val->next_map, char, 9);
            value = lex.state_.string;
            if (value.size() > 8)
                FatalError("UMAPINFO: Mapname for \"next\" over 8 characters!\n");
            epi::CStringCopyMax(val->next_map, value.data(), 8);
        }
        break;
        case umapinfo::kNextSecret: {
            EPI_CLEAR_MEMORY(val->nextsecret, char, 9);
            value = lex.state_.string;
            if (value.size() > 8)
                FatalError("UMAPINFO: Mapname for \"nextsecret\" over 8 "
                           "characters!\n");
            epi::CStringCopyMax(val->nextsecret, value.data(), 8);
        }
        break;
        case umapinfo::kLevelPic: {
            EPI_CLEAR_MEMORY(val->levelpic, char, 9);
            value = lex.state_.string;
            if (value.size() > 8)
                FatalError("UMAPINFO: Entry for \"levelpic\" over 8 "
                           "characters!\n");
            epi::CStringCopyMax(val->levelpic, value.data(), 8);
        }
        break;
        case umapinfo::kSkyTexture: {
            EPI_CLEAR_MEMORY(val->skytexture, char, 9);
            value = lex.state_.string;
            if (value.size() > 8)
                FatalError("UMAPINFO: Entry for \"skytexture\" over 8 "
                           "characters!\n");
            epi::CStringCopyMax(val->skytexture, value.data(), 8);
        }
        break;
        case umapinfo::kMusic: {
            EPI_CLEAR_MEMORY(val->music, char, 9);
            value = lex.state_.string;
            if (value.size() > 8)
                FatalError("UMAPINFO: Entry for \"music\" over 8 characters!\n");
            epi::CStringCopyMax(val->music, value.data(), 8);
        }
        break;
        case umapinfo::kEndPic: {
            EPI_CLEAR_MEMORY(val->endpic, char, 9);
            value = lex.state_.string;
            if (value.size() > 8)
                FatalError("UMAPINFO: Entry for \"endpic\" over 8 characters!\n");
            epi::CStringCopyMax(val->endpic, value.data(), 8);
        }
        break;
        case umapinfo::kEndCast:
            val->docast = lex.state_.boolean;
            break;
        case umapinfo::kEndBunny:
            val->dobunny = lex.state_.boolean;
            break;
        case umapinfo::kEndGame:
            val->endgame = lex.state_.boolean;
            break;
        case umapinfo::kExitPic: {
            EPI_CLEAR_MEMORY(val->exitpic, char, 9);
            value = lex.state_.string;
            if (value.size() > 8)
                FatalError("UMAPINFO: Entry for \"exitpic\" over 8 characters!\n");
            epi::CStringCopyMax(val->exitpic, value.data(), 8);
        }
        break;
        case umapinfo::kEnterPic: {
            EPI_CLEAR_MEMORY(val->enterpic, char, 9);
            value = lex.state_.string;
            if (value.size() > 8)
                FatalError("UMAPINFO: Entry for \"enterpic\" over 8 "
                           "characters!\n");
            epi::CStringCopyMax(val->enterpic, value.data(), 8);
        }
        break;
        case umapinfo::kNoIntermission:
            val->nointermission = lex.state_.boolean;
            break;
        case umapinfo::kParTime:
            val->partime = 35 * lex.state_.number;
            break;
        case umapinfo::kIntertext: {
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
        case umapinfo::kIntertextSecret: {
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
        case umapinfo::kInterbackdrop: {
            EPI_CLEAR_MEMORY(val->interbackdrop, char, 9);
            value = lex.state_.string;
            if (value.size() > 8)
                FatalError("UMAPINFO: Entry for \"interbackdrop\" over 8 "
                           "characters!\n");
            epi::CStringCopyMax(val->interbackdrop, value.data(), 8);
        }
        break;
        case umapinfo::kIntermusic: {
            EPI_CLEAR_MEMORY(val->intermusic, char, 9);
            value = lex.state_.string;
            if (value.size() > 8)
                FatalError("UMAPINFO: Entry for \"intermusic\" over 8 "
                           "characters!\n");
            epi::CStringCopyMax(val->intermusic, value.data(), 8);
        }
        break;
        case umapinfo::kEpisode: {
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
        case umapinfo::kBossAction: {
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
                int             actor_num = -1;
                epi::StringHash actor_check(value);
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
        case umapinfo::kAuthor: {
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