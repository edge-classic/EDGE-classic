//----------------------------------------------------------------------------
//  EDGE UMAPINFO Parsing Code
//----------------------------------------------------------------------------
// 
//  Copyright (c) 2023 The EDGE Team.
// 
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//----------------------------------------------------------------------------
//
//  Based on the UMAPINFO reference implementation, released by Christoph Oelckers under the
//  following copyright:
//
//    Copyright 2017 Christoph Oelckers
//
//----------------------------------------------------------------------------

#include "language.h"
#include "str_lexer.h"
#include "p_umapinfo.h"
#include "deh_text.h"
#include <unordered_map> // ZDoom Actor Name <-> Doomednum lookups

MapList Maps;

//==========================================================================
//
// The Doom actors in their original order
//
//==========================================================================

static std::unordered_map<const char *, int> ActorNames =
{
    {"DoomPlayer", -1},
    {"ZombieMan", 3004},
    {"ShotgunGuy", 9},
    {"Archvile", 64},
    {"ArchvileFire", -1},
    {"Revenant", 66},
    {"RevenantTracer", -1},
    {"RevenantTracerSmoke", -1},
    {"Fatso", 67},
    {"FatShot", -1},
    {"ChaingunGuy", 65},
    {"DoomImp", 3001},
    {"Demon", 3002},
    {"Spectre", 58},
    {"Cacodemon", 3005},
    {"BaronOfHell", 3003},
    {"BaronBall", -1},
    {"HellKnight", 69},
    {"LostSoul", 3006},
    {"SpiderMastermind", 7},
    {"Arachnotron", 68},
    {"Cyberdemon", 16},
    {"PainElemental", 71},
    {"WolfensteinSS", 84},
    {"CommanderKeen", 72},
    {"BossBrain", 88},
    {"BossEye", 89},
    {"BossTarget", 87},
    {"SpawnShot", -1},
    {"SpawnFire", -1},
    {"ExplosiveBarrel", 2035},
    {"DoomImpBall", -1},
    {"CacodemonBall", -1},
    {"Rocket", -1},
    {"PlasmaBall", -1},
    {"BFGBall", -1},
    {"ArachnotronPlasma", -1},
    {"BulletPuff", -1},
    {"Blood", -1},
    {"TeleportFog", -1},
    {"ItemFog", -1},
    {"TeleportDest", 14},
    {"BFGExtra", -1},
    {"GreenArmor", 2018},
    {"BlueArmor", 2019},
    {"HealthBonus", 2014},
    {"ArmorBonus", 2015},
    {"BlueCard", 5},
    {"RedCard", 13},
    {"YellowCard", 6},
    {"YellowSkull", 39},
    {"RedSkull", 38},
    {"BlueSkull", 40},
    {"Stimpack", 2011},
    {"Medikit", 2012},
    {"Soulsphere", 2013},
    {"InvulnerabilitySphere", 2022},
    {"Berserk", 2023},
    {"BlurSphere", 2024},
    {"RadSuit", 2025},
    {"Allmap", 2026},
    {"Infrared", 2045},
    {"Megasphere", 83},
    {"Clip", 2007},
    {"ClipBox", 2048},
    {"RocketAmmo", 2010},
    {"RocketBox", 2046},
    {"Cell", 2047},
    {"CellPack", 17},
    {"Shell", 2008},
    {"ShellBox", 2049},
    {"Backpack", 8},
    {"BFG9000", 2006},
    {"Chaingun", 2002},
    {"Chainsaw", 2005},
    {"RocketLauncher", 2003},
    {"PlasmaRifle", 2004},
    {"Shotgun", 2001},
    {"SuperShotgun", 82},
    {"TechLamp", 85},
    {"TechLamp2", 86},
    {"Column", 2028},
    {"TallGreenColumn", 30},
    {"ShortGreenColumn", 31},
    {"TallRedColumn", 32},
    {"ShortRedColumn", 33},
    {"SkullColumn", 37},
    {"HeartColumn", 36},
    {"EvilEye", 41},
    {"FloatingSkull", 42},
    {"TorchTree", 43},
    {"BlueTorch", 44},
    {"GreenTorch", 45},
    {"RedTorch", 46},
    {"ShortBlueTorch", 55},
    {"ShortGreenTorch", 56},
    {"ShortRedTorch", 57},
    {"Stalagtite", 47},
    {"TechPillar", 48},
    {"CandleStick", 34},
    {"Candelabra", 35},
    {"BloodyTwitch", 49},
    {"Meat2", 50},
    {"Meat3", 51},
    {"Meat4", 52},
    {"Meat5", 53},
    {"NonsolidMeat2", 59},
    {"NonsolidMeat4", 60},
    {"NonsolidMeat3", 61},
    {"NonsolidMeat5", 62},
    {"NonsolidTwitch", 63},
    {"DeadCacodemon", 22},
    {"DeadMarine", 15},
    {"DeadZombieMan", 18},
    {"DeadDemon", 21},
    {"DeadLostSoul", 23},
    {"DeadDoomImp", 20},
    {"DeadShotgunGuy", 19},
    {"GibbedMarine", 10},
    {"GibbedMarineExtra", 12},
    {"HeadsOnAStick", 28},
    {"Gibs", 24},
    {"HeadOnAStick", 27},
    {"HeadCandles", 29},
    {"DeadStick", 25},
    {"LiveStick", 26},
    {"BigTree", 54},
    {"BurningBarrel", 70},
    {"HangNoGuts", 73},
    {"HangBNoBrain", 74},
    {"HangTLookingDown", 75},
    {"HangTSkull", 76},
    {"HangTLookingUp", 77},
    {"HangTNoBrain", 78},
    {"ColonGibs", 79},
    {"SmallBloodPool", 80},
    {"BrainStem", 81},
	//Boom/MBF additions
    {"PointPusher", 5001},
    {"PointPuller", 5002},
    {"MBFHelperDog", 888},
    {"PlasmaBall1", -1},
    {"PlasmaBall2", -1},
    {"EvilSceptre", -1},
    {"UnholyBible", -1},
    {"MusicChanger", -1}, // Doomednums 14101-14165, but I don't think we need this
	// I'm guessing below here
    {"Deh_Actor_145", 145},
    {"Deh_Actor_146", 146},
    {"Deh_Actor_147", 147},
    {"Deh_Actor_148", 148},
    {"Deh_Actor_149", 149},
	// DEHEXTRA Actors start here
    {"Deh_Actor_150", 150}, // Extra thing 0
    {"Deh_Actor_151", 151}, // Extra thing 1
    {"Deh_Actor_152", 152}, // Extra thing 2
    {"Deh_Actor_153", 153}, // Extra thing 3
    {"Deh_Actor_154", 154}, // Extra thing 4
    {"Deh_Actor_155", 155}, // Extra thing 5
    {"Deh_Actor_156", 156}, // Extra thing 6
    {"Deh_Actor_157", 157}, // Extra thing 7
    {"Deh_Actor_158", 158}, // Extra thing 8
    {"Deh_Actor_159", 159}, // Extra thing 9
    {"Deh_Actor_160", 160}, // Extra thing 10
    {"Deh_Actor_161", 161}, // Extra thing 11
    {"Deh_Actor_162", 162}, // Extra thing 12
    {"Deh_Actor_163", 163}, // Extra thing 13
    {"Deh_Actor_164", 164}, // Extra thing 14
    {"Deh_Actor_165", 165}, // Extra thing 15
    {"Deh_Actor_166", 166}, // Extra thing 16
    {"Deh_Actor_167", 167}, // Extra thing 17
    {"Deh_Actor_168", 168}, // Extra thing 18
    {"Deh_Actor_169", 169}, // Extra thing 19
    {"Deh_Actor_170", 170}, // Extra thing 20
    {"Deh_Actor_171", 171}, // Extra thing 21
    {"Deh_Actor_172", 172}, // Extra thing 22
    {"Deh_Actor_173", 173}, // Extra thing 23
    {"Deh_Actor_174", 174}, // Extra thing 24
    {"Deh_Actor_175", 175}, // Extra thing 25
    {"Deh_Actor_176", 176}, // Extra thing 26
    {"Deh_Actor_177", 177}, // Extra thing 27
    {"Deh_Actor_178", 178}, // Extra thing 28
    {"Deh_Actor_179", 179}, // Extra thing 29
    {"Deh_Actor_180", 180}, // Extra thing 30
    {"Deh_Actor_181", 181}, // Extra thing 31
    {"Deh_Actor_182", 182}, // Extra thing 32
    {"Deh_Actor_183", 183}, // Extra thing 33
    {"Deh_Actor_184", 184}, // Extra thing 34
    {"Deh_Actor_185", 185}, // Extra thing 35
    {"Deh_Actor_186", 186}, // Extra thing 36
    {"Deh_Actor_187", 187}, // Extra thing 37
    {"Deh_Actor_188", 188}, // Extra thing 38
    {"Deh_Actor_189", 189}, // Extra thing 39
    {"Deh_Actor_190", 190}, // Extra thing 40
    {"Deh_Actor_191", 191}, // Extra thing 41
    {"Deh_Actor_192", 192}, // Extra thing 42
    {"Deh_Actor_193", 193}, // Extra thing 43
    {"Deh_Actor_194", 194}, // Extra thing 44
    {"Deh_Actor_195", 195}, // Extra thing 45
    {"Deh_Actor_196", 196}, // Extra thing 46
    {"Deh_Actor_197", 197}, // Extra thing 47
    {"Deh_Actor_198", 198}, // Extra thing 48
    {"Deh_Actor_199", 199}, // Extra thing 49
    {"Deh_Actor_200", 200}, // Extra thing 50
    {"Deh_Actor_201", 201}, // Extra thing 51
    {"Deh_Actor_202", 202}, // Extra thing 52
    {"Deh_Actor_203", 203}, // Extra thing 53
    {"Deh_Actor_204", 204}, // Extra thing 54
    {"Deh_Actor_205", 205}, // Extra thing 55
    {"Deh_Actor_206", 206}, // Extra thing 56
    {"Deh_Actor_207", 207}, // Extra thing 57
    {"Deh_Actor_208", 208}, // Extra thing 58
    {"Deh_Actor_209", 209}, // Extra thing 59
    {"Deh_Actor_210", 210}, // Extra thing 60
    {"Deh_Actor_211", 211}, // Extra thing 61
    {"Deh_Actor_212", 212}, // Extra thing 62
    {"Deh_Actor_213", 213}, // Extra thing 63
    {"Deh_Actor_214", 214}, // Extra thing 64
    {"Deh_Actor_215", 215}, // Extra thing 65
    {"Deh_Actor_216", 216}, // Extra thing 66
    {"Deh_Actor_217", 217}, // Extra thing 67
    {"Deh_Actor_218", 218}, // Extra thing 68
    {"Deh_Actor_219", 219}, // Extra thing 69
    {"Deh_Actor_220", 220}, // Extra thing 70
    {"Deh_Actor_221", 221}, // Extra thing 71
    {"Deh_Actor_222", 222}, // Extra thing 72
    {"Deh_Actor_223", 223}, // Extra thing 73
    {"Deh_Actor_224", 224}, // Extra thing 74
    {"Deh_Actor_225", 225}, // Extra thing 75
    {"Deh_Actor_226", 226}, // Extra thing 76
    {"Deh_Actor_227", 227}, // Extra thing 77
    {"Deh_Actor_228", 228}, // Extra thing 78
    {"Deh_Actor_229", 229}, // Extra thing 79
    {"Deh_Actor_230", 230}, // Extra thing 80
    {"Deh_Actor_231", 231}, // Extra thing 81
    {"Deh_Actor_232", 232}, // Extra thing 82
    {"Deh_Actor_233", 233}, // Extra thing 83
    {"Deh_Actor_234", 234}, // Extra thing 84
    {"Deh_Actor_235", 235}, // Extra thing 85
    {"Deh_Actor_236", 236}, // Extra thing 86
    {"Deh_Actor_237", 237}, // Extra thing 87
    {"Deh_Actor_238", 238}, // Extra thing 88
    {"Deh_Actor_239", 239}, // Extra thing 89
    {"Deh_Actor_240", 240}, // Extra thing 90
    {"Deh_Actor_241", 241}, // Extra thing 91
    {"Deh_Actor_242", 242}, // Extra thing 92
    {"Deh_Actor_243", 243}, // Extra thing 93
    {"Deh_Actor_244", 244}, // Extra thing 94
    {"Deh_Actor_245", 245}, // Extra thing 95
    {"Deh_Actor_246", 246}, // Extra thing 96
    {"Deh_Actor_247", 247}, // Extra thing 97
    {"Deh_Actor_248", 248}, // Extra thing 98
    {"Deh_Actor_249", 249}, // Extra thing 99
};

static void FreeMap(MapEntry *mape)
{
	if (mape->mapname) free(mape->mapname);
	if (mape->levelname) free(mape->levelname);
	if (mape->label) free(mape->label);
	if (mape->intertext) free(mape->intertext);
	if (mape->intertextsecret) free(mape->intertextsecret);
	if (mape->bossactions) free(mape->bossactions);
	if (mape->authorname) free(mape->authorname);
	mape->mapname = NULL;
}

void FreeMapList()
{
	unsigned i;
	
	for(i = 0; i < Maps.mapcount; i++)
	{
		FreeMap(&Maps.maps[i]);
	}
	free(Maps.maps);
	Maps.maps = NULL;
	Maps.mapcount = 0;
}

static void SkipToNextLine(epi::lexer_c& lex, epi::token_kind_e& tok, std::string& value)
{
	int skip_line = lex.LastLine();
	for (;;)
	{
		lex.MatchKeep("linecheck");
		if (lex.LastLine() == skip_line)
		{
			tok = lex.Next(value);
			if (tok == epi::TOK_EOF)
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

static void ParseUMAPINFOEntry(epi::lexer_c& lex, MapEntry *val)
{
	for (;;)
	{
		if (lex.Match("}"))
			break;

		std::string key;
		std::string value;

		epi::token_kind_e tok = lex.Next(key);

		if (tok == epi::TOK_EOF)
			I_Error("Malformed UMAPINFO lump: unclosed block\n");

		if (tok != epi::TOK_Ident)
			I_Error("Malformed UMAPINFO lump: missing key\n");

		if (! lex.Match("="))
			I_Error("Malformed UMAPINFO lump: missing '='\n");

		tok = lex.Next(value);

		if (tok == epi::TOK_EOF || tok == epi::TOK_ERROR || value == "}")
			I_Error("Malformed UMAPINFO lump: missing value\n");

		if (epi::case_cmp(key, "levelname") == 0)
		{
			if (val->levelname) free(val->levelname);
			val->levelname = (char *)calloc(value.size()+1, sizeof(char));
			Z_StrNCpy(val->levelname, value.c_str(), value.size());
		}
		else if (epi::case_cmp(key, "label") == 0)
		{
			if (epi::case_cmp(value, "clear") == 0)
			{
				if (val->label)	free(val->label);
				val->label = (char *)calloc(2, sizeof(char));
				val->label[0] = '-';
			}
			else
			{
				if (val->label)	free(val->label);
				val->label = (char *)calloc(value.size()+1, sizeof(char));
				Z_StrNCpy(val->label, value.c_str(), value.size());
			}
		}
		else if (epi::case_cmp(key, "next") == 0)
		{
			Z_Clear(val->nextmap, char, 9);
			if (value.size() > 8)
				I_Error("UMAPINFO: Mapname for \"next\" over 8 characters!\n");
			Z_StrNCpy(val->nextmap, value.data(), 8);
		}
		else if (epi::case_cmp(key, "nextsecret") == 0)
		{
			Z_Clear(val->nextsecret, char, 9);
			if (value.size() > 8)
				I_Error("UMAPINFO: Mapname for \"nextsecret\" over 8 characters!\n");
			Z_StrNCpy(val->nextsecret, value.data(), 8);
		}
		else if (epi::case_cmp(key, "levelpic") == 0)
		{
			Z_Clear(val->levelpic, char, 9);
			if (value.size() > 8)
				I_Error("UMAPINFO: Mapname for \"levelpic\" over 8 characters!\n");
			Z_StrNCpy(val->levelpic, value.data(), 8);
		}
		else if (epi::case_cmp(key, "skytexture") == 0)
		{
			Z_Clear(val->skytexture, char, 9);
			if (value.size() > 8)
				I_Error("UMAPINFO: Mapname for \"skytexture\" over 8 characters!\n");
			Z_StrNCpy(val->skytexture, value.data(), 8);
		}
		else if (epi::case_cmp(key, "music") == 0)
		{
			Z_Clear(val->music, char, 9);
			if (value.size() > 8)
				I_Error("UMAPINFO: Mapname for \"music\" over 8 characters!\n");
			Z_StrNCpy(val->music, value.data(), 8);
		}
		else if (epi::case_cmp(key, "endpic") == 0)
		{
			Z_Clear(val->endpic, char, 9);
			if (value.size() > 8)
				I_Error("UMAPINFO: Mapname for \"endpic\" over 8 characters!\n");
			Z_StrNCpy(val->endpic, value.data(), 8);
		}
		else if (epi::case_cmp(key, "endcast") == 0)
		{
			val->docast = epi::LEX_Boolean(value);
			/*if (val->docast)
			{
				Z_Clear(val->endpic, char, 9);
				strcpy(val->endpic, "$CAST");
			}
			else
			{
				Z_Clear(val->endpic, char, 9);
				val->endpic[0] = '-';
			}*/
		}
		else if (epi::case_cmp(key, "endbunny") == 0)
		{
			val->dobunny = epi::LEX_Boolean(value);
			/*if (val->dobunny)
			{
				Z_Clear(val->endpic, char, 9);
				strcpy(val->endpic, "$BUNNY");
			}
			else
			{
				Z_Clear(val->endpic, char, 9);
				val->endpic[0] = '-';
			}*/
		}
		else if (epi::case_cmp(key, "endgame") == 0)
		{
			val->endgame = epi::LEX_Boolean(value);
			/*if (val->endgame)
			{
				Z_Clear(val->endpic, char, 9);
				strcpy(val->endpic, "!");
			}
			else
			{
				Z_Clear(val->endpic, char, 9);
				val->endpic[0] = '-';
			}*/
		}
		else if (epi::case_cmp(key, "exitpic") == 0)
		{
			Z_Clear(val->exitpic, char, 9);
			if (value.size() > 8)
				I_Error("UMAPINFO: Mapname for \"exitpic\" over 8 characters!\n");
			Z_StrNCpy(val->exitpic, value.data(), 8);
		}
		else if (epi::case_cmp(key, "enterpic") == 0)
		{
			Z_Clear(val->enterpic, char, 9);
			if (value.size() > 8)
				I_Error("UMAPINFO: Mapname for \"enterpic\" over 8 characters!\n");
			Z_StrNCpy(val->enterpic, value.data(), 8);
		}
		else if (epi::case_cmp(key, "nointermission") == 0)
		{
			val->nointermission = epi::LEX_Boolean(value);
		}
		else if (epi::case_cmp(key, "partime") == 0)
		{
			val->partime = 35 * epi::LEX_Int(value);
		}
		else if (epi::case_cmp(key, "intertext") == 0)
		{
			std::string it_builder = value;
			while (lex.Match(","))
			{
				it_builder.append("\n");
				lex.Next(value);
				it_builder.append(value);
			}
			if (val->intertext) free(val->intertext);
			val->intertext = (char *)calloc(it_builder.size()+1, sizeof(char));
			Z_StrNCpy(val->intertext, it_builder.c_str(), it_builder.size());
		}
		else if (epi::case_cmp(key, "intertextsecret") == 0)
		{
			std::string it_builder = value;
			while (lex.Match(","))
			{
				it_builder.append("\n");
				lex.Next(value);
				it_builder.append(value);
			}
			if (val->intertextsecret) free(val->intertextsecret);
			val->intertextsecret = (char *)calloc(it_builder.size()+1, sizeof(char));
			Z_StrNCpy(val->intertextsecret, it_builder.c_str(), it_builder.size());
		}
		else if (epi::case_cmp(key, "interbackdrop") == 0)
		{
			Z_Clear(val->interbackdrop, char, 9);
			if (value.size() > 8)
				I_Error("UMAPINFO: Mapname for \"interbackdrop\" over 8 characters!\n");
			Z_StrNCpy(val->interbackdrop, value.data(), 8);
		}
		else if (epi::case_cmp(key, "intermusic") == 0)
		{
			Z_Clear(val->intermusic, char, 9);
			if (value.size() > 8)
				I_Error("UMAPINFO: Mapname for \"intermusic\" over 8 characters!\n");
			Z_StrNCpy(val->intermusic, value.data(), 8);
		}
		else if (epi::case_cmp(key, "episode") == 0)
		{
			if (epi::case_cmp(value, "clear") == 0)
			{
				//M_ClearEpisodes();
			}
			else
			{
				char lumpname[9] = {0};
				std::string alttext;
				std::string epikey;
				if (value.size() > 8)
					I_Error("UMAPINFO: Mapname for \"enterpic\" over 8 characters!\n");
				Z_StrNCpy(lumpname, value.data(), 8);
				if (lex.Match(","))
				{
					lex.Next(alttext);
					if (lex.Match(","))
						lex.Next(epikey);
				}
				//M_AddEpisode(val->mapname, lumpname, alttext.c_str(), epikey.c_str());
			}
		}
		else if (epi::case_cmp(key, "bossaction") == 0)
		{
			int special = 0;
			int tag = 0;
			if (epi::case_cmp(value, "clear") == 0)
			{
				special = tag = -1;
				if (val->bossactions) free(val->bossactions);
				val->bossactions = NULL;
				val->numbossactions = -1;
			}
			else
			{
				int actor_num = -1;
				bool found_actor = false;
				for (auto actor : ActorNames)
				{
					if (epi::case_cmp(actor.first, value.c_str()) == 0)
					{
						found_actor = true;
						actor_num = actor.second;
						break;
					}
				}
				if (!found_actor)
					I_Error("UMAPINFO: Unknown thing type %s\n", value.c_str());
				if (actor_num == -1)
					SkipToNextLine(lex, tok, value);
				else
				{
					if (!lex.Match(","))
						I_Error("UMAPINFO: \"bossaction\" key missing line special!\n");
					lex.Next(value);
					special = epi::LEX_Int(value);
					if (!lex.Match(","))
						I_Error("UMAPINFO: \"bossaction\" key missing tag!\n");
					lex.Next(value);
					tag = epi::LEX_Int(value);
					if (tag != 0 || special == 11 || special == 51 || special == 52 || special == 124)
					{
						if (val->numbossactions == -1) 
							val->numbossactions = 1;
						else
							val->numbossactions++;
						val->bossactions = (struct BossAction *)realloc(val->bossactions, sizeof(struct BossAction) * val->numbossactions);
						val->bossactions[val->numbossactions - 1].type = actor_num;
						val->bossactions[val->numbossactions - 1].special = special;
						val->bossactions[val->numbossactions - 1].tag = tag;
					}
				}
			}
		}
		else if (epi::case_cmp(key, "author") == 0)
		{
			if (val->authorname) free(val->authorname);
			val->authorname = (char *)calloc(value.size()+1, sizeof(char));
			Z_StrNCpy(val->authorname, value.c_str(), value.size());
		}
	}
	// Some fallback handling
	if (!val->nextsecret[0])
	{
		if (val->nextmap[0])
			Z_StrNCpy(val->nextsecret, val->nextmap, 8);
	}
	if (!val->enterpic[0])
	{
		for(size_t i = 0; i < Maps.mapcount; i++)
		{
			if (!strcmp(val->mapname, Maps.maps[i].nextmap))
			{
				if (Maps.maps[i].exitpic[0])
					Z_StrNCpy(val->enterpic, Maps.maps[i].exitpic, 8);
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

void Parse_UMAPINFO(const std::string& buffer)
{
	epi::lexer_c lex(buffer);

	for (;;)
	{
		std::string section;
		epi::token_kind_e tok = lex.Next(section);

		if (tok == epi::TOK_EOF)
			break;

		if (tok != epi::TOK_Ident || epi::case_cmp(section, "MAP") != 0)
			I_Error("Malformed UMAPINFO lump.\n");

		tok = lex.Next(section);

		if (tok != epi::TOK_Ident)
			I_Error("UMAPINFO: No mapname for map entry!\n");

		unsigned int i = 0;
		MapEntry parsed = { 0 };
		parsed.mapname = (char *)calloc(section.size()+1, sizeof(char));
		Z_StrNCpy(parsed.mapname, section.data(), section.size());

		if (! lex.Match("{"))
			I_Error("Malformed UMAPINFO lump: missing '{'\n");

		ParseUMAPINFOEntry(lex, &parsed);
		// Does this map entry already exist? If yes, replace it.
		for (i = 0; i < Maps.mapcount; i++)
		{
			if (epi::case_cmp(parsed.mapname, Maps.maps[i].mapname) == 0)
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
			Maps.maps = (MapEntry*)realloc(Maps.maps, sizeof(MapEntry)*Maps.mapcount);
			Maps.maps[Maps.mapcount-1] = parsed;
		}
	}
}

// -----------------------------------------------
//
// Parses a complete DMAPINFO entry
//
// -----------------------------------------------

static void ParseDMAPINFOEntry(epi::lexer_c& lex, MapEntry *val)
{
	for (;;)
	{
		if (lex.Match("}"))
			break;

		std::string key;
		std::string value;

		epi::token_kind_e tok = lex.Next(key);

		if (tok == epi::TOK_EOF)
			I_Error("Malformed DMAPINFO lump: unclosed block\n");

		if (tok != epi::TOK_Ident)
			I_Error("Malformed DMAPINFO lump: missing key\n");

		// Need to read the spec and see which standalone keys have a UMAPINFO equivalent
		if (! lex.Match("="))
			continue;

		tok = lex.Next(value);

		if (tok == epi::TOK_EOF || tok == epi::TOK_ERROR || value == "}")
			I_Error("Malformed DMAPINFO lump: missing value\n");

		if (epi::case_cmp(key, "next") == 0)
		{
			Z_Clear(val->nextmap, char, 9);
			if (value.size() > 8)
				I_Error("DMAPINFO: Mapname for \"next\" over 8 characters!\n");
			Z_StrNCpy(val->nextmap, value.data(), 8);
		}
		else if (epi::case_cmp(key, "secretnext") == 0)
		{
			Z_Clear(val->nextsecret, char, 9);
			if (value.size() > 8)
				I_Error("DMAPINFO: Mapname for \"secretnext\" over 8 characters!\n");
			Z_StrNCpy(val->nextsecret, value.data(), 8);
		}
		else if (epi::case_cmp(key, "sky1") == 0)
		{
			Z_Clear(val->skytexture, char, 9);
			if (value.size() > 8)
				I_Error("DMAPINFO: Image name for sky over 8 characters!\n");
			Z_StrNCpy(val->skytexture, value.data(), 8);
			SkipToNextLine(lex, tok, value);
		}
		else if (epi::case_cmp(key, "music") == 0)
		{
			Z_Clear(val->music, char, 9);
			if (value.size() > 8)
				I_Error("DMAPINFO: Song name for \"music\" over 8 characters!\n");
			else
				Z_StrNCpy(val->music, value.data(), 8);
		}
		else if (epi::case_cmp(key, "par") == 0)
		{
			val->partime = 35 * epi::LEX_Int(value);
		}
		else if (epi::case_cmp(key, "map07special") == 0)
		{
			val->map07special = true;
		}
		else
		{
			SkipToNextLine(lex, tok, value);
		}
	}
	// Some fallback handling
	if (!val->nextsecret[0])
	{
		if (val->nextmap[0])
			Z_StrNCpy(val->nextsecret, val->nextmap, 8);
	}
}

// -----------------------------------------------
//
// Parses a complete DMAPINFO lump
//
// -----------------------------------------------

void Parse_DMAPINFO(const std::string& buffer)
{
	epi::lexer_c lex(buffer);

	for (;;)
	{
		std::string section;
		epi::token_kind_e tok = lex.Next(section);

		if (tok == epi::TOK_EOF)
			break;

		// skip default/cluster/etc; we just want map entries
		if (epi::case_cmp(section, "map") != 0)
			continue;

		tok = lex.Next(section);

		if (tok != epi::TOK_String)
			I_Error("DMAPINFO: No mapname for map entry!\n");

		unsigned int i = 0;
		MapEntry parsed = { 0 };
		parsed.mapname = (char *)calloc(section.size()+1, sizeof(char));
		Z_StrNCpy(parsed.mapname, section.data(), section.size());

		tok = lex.Next(section);

		if (tok == epi::TOK_String)
		{
			parsed.levelname = (char *)calloc(section.size()+1, sizeof(char));
			Z_StrNCpy(parsed.levelname, section.data(), section.size());
		}
		else
			I_Error("Malformed DMAPINFO lump: missing mapname for %s!\n", parsed.mapname);

		if (! lex.Match("{"))
			I_Error("Malformed DMAPINFO lump: missing '{'\n");

		ParseDMAPINFOEntry(lex, &parsed);
		// Does this map entry already exist? If yes, replace it.
		for (i = 0; i < Maps.mapcount; i++)
		{
			if (epi::case_cmp(parsed.mapname, Maps.maps[i].mapname) == 0)
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
			Maps.maps = (MapEntry*)realloc(Maps.maps, sizeof(MapEntry)*Maps.mapcount);
			Maps.maps[Maps.mapcount-1] = parsed;
		}
	}
}

// -----------------------------------------------
//
// Parses a complete MAPINFO/RMAPINFO entry
//
// -----------------------------------------------

static void ParseMAPINFOEntry(epi::lexer_c& lex, MapEntry *val)
{
	for (;;)
	{
		if (lex.Match("}"))
			break;

		if (lex.MatchKeep("map"))
			break;

		std::string key;
		std::string value;

		epi::token_kind_e tok = lex.Next(key);

		if (tok == epi::TOK_EOF)
			break;

		if (tok != epi::TOK_Ident)
			I_Error("Malformed MAPINFO lump: missing key\n");

		int key_line = lex.LastLine();

		lex.MatchKeep("linecheck"); // doesn't matter, just need to update LastLine

		if (key_line != lex.LastLine()) // key-only value, parse if supported and move on
		{
			if (epi::case_cmp(key, "nointermission") == 0)
				val->nointermission = true;
			continue;
		}

		lex.Match("="); // optional

		tok = lex.Next(value);

		if (tok == epi::TOK_ERROR)
			I_Error("Malformed MAPINFO lump: missing value\n");

		if (tok == epi::TOK_EOF)
			break;

		if (epi::case_cmp(key, "next") == 0)
		{
			Z_Clear(val->nextmap, char, 9);
			if (epi::case_cmp(value, "endpic") == 0)
			{
				Z_Clear(val->endpic, char, 9);
				lex.Match(","); // optional
				int nextmap_line = lex.LastLine();
				tok = lex.Next(value);
				if (lex.LastLine() != nextmap_line)
					I_Error("MAPINFO: Missing lump for \"endpic\"!\n");
				else if (value.size() > 8)
					I_Error("MAPINFO: Name for \"endpic\" lump over 8 characters!\n");
				else
					Z_StrNCpy(val->endpic, value.data(), 8);
			}
			else if (epi::case_cmp(value, "endgame1") == 0 || epi::case_cmp(value, "endgame2") == 0 ||
				epi::case_cmp(value, "endgamew") == 0 || epi::case_cmp(value, "endgame4") == 0 ||
				epi::case_cmp(value, "enddemon") == 0 || epi::case_cmp(value, "endtitle") == 0)
			{
				val->endgame = true;
			}
			else if (epi::case_cmp(value, "endgamec") == 0)
				val->docast = true;
			else if (epi::case_cmp(value, "endgame3") == 0)
				val->dobunny = true;
			else if (epi::case_cmp(value, "endgames") == 0 || epi::case_cmp(value, "endchess") == 0)
				I_Warning("MAPINFO: Hexen/Strife ending sequence referenced!\n");
			else if (epi::case_cmp(value, "endgame") == 0)
			{
				val->endgame = true;
				if (lex.Match("{"))
				{
					for (;;)
					{
						if (lex.Match("}"))
							break;
						tok = lex.Next(key);
						if (tok == epi::TOK_EOF)
							I_Error("Malformed MAPINFO lump: unclosed block\n");
						if (epi::case_cmp(key, "cast") == 0)
						{
							val->docast = true;
							continue;
						}
						if (epi::case_cmp(key, "pic") == 0)
						{
							lex.Match("="); // optional
							tok = lex.Next(value);
							if (tok == epi::TOK_EOF || tok == epi::TOK_ERROR || value == "}")
								I_Error("Malformed MAPINFO lump: missing value\n");
							Z_Clear(val->endpic, char, 9);
							if (value.size() > 8)
								I_Error("MAPINFO: Name for \"endpic\" lump over 8 characters!\n");
							Z_StrNCpy(val->endpic, value.data(), 8);
						}
						else
							continue;
					}
				}
			}
			else
			{
				if (value.size() > 8)
					I_Error("MAPINFO: Mapname for \"next\" over 8 characters!\n");
				Z_StrNCpy(val->nextmap, value.data(), 8);
			}
		}
		else if (epi::case_cmp(key, "secretnext") == 0)
		{
			Z_Clear(val->nextsecret, char, 9);
			// We will only actually use a map lumpname, if present, but consume and ignore
			// the various end* options for secretnext since they don't seem to have
			// a UMAPINFO equivalent
			if (epi::case_cmp(value, "endpic") == 0)
			{
				SkipToNextLine(lex, tok, value);
			}
			else if (epi::case_cmp(value, "endgame1") == 0 || epi::case_cmp(value, "endgame2") == 0 ||
				epi::case_cmp(value, "endgamew") == 0 || epi::case_cmp(value, "endgame4") == 0 ||
				epi::case_cmp(value, "enddemon") == 0 || epi::case_cmp(value, "endtitle") == 0)
			{
				// nothing needed
			}
			else if (epi::case_cmp(value, "endgamec") == 0)
			{
				// nothing needed
			}
			else if (epi::case_cmp(value, "endgame3") == 0)
			{
				// nothing needed
			}
			else if (epi::case_cmp(value, "endgames") == 0 || epi::case_cmp(value, "endchess") == 0)
				I_Warning("MAPINFO: Hexen/Strife ending sequence referenced!\n");
			else if (epi::case_cmp(value, "endgame") == 0)
			{
				if (lex.Match("{"))
				{
					for (;;)
					{
						if (lex.Match("}"))
							break;
						tok = lex.Next(value);
						if (tok == epi::TOK_EOF || tok == epi::TOK_ERROR)
							I_Error("Malformed MAPINFO lump: missing closing bracket!\n");
					}
				}
			}
			else
			{
				if (value.size() > 8)
					I_Error("MAPINFO: Mapname for \"secretnext\" over 8 characters!\n");
				Z_StrNCpy(val->nextsecret, value.data(), 8);
			}
		}
		else if (epi::case_cmp(key, "titlepatch") == 0)
		{
			Z_Clear(val->levelpic, char, 9);
			if (value.size() > 8)
				I_Error("MAPINFO: Image name for \"titlepatch\" over 8 characters!\n");
			Z_StrNCpy(val->levelpic, value.data(), 8);
		}
		else if (epi::case_cmp(key, "sky1") == 0)
		{
			Z_Clear(val->skytexture, char, 9);
			if (value.size() > 8)
				I_Error("MAPINFO: Image name for \"sky1\" over 8 characters!\n");
			Z_StrNCpy(val->skytexture, value.data(), 8);
			SkipToNextLine(lex, tok, value);
		}
		else if (epi::case_cmp(key, "music") == 0)
		{
			Z_Clear(val->music, char, 9);
			if (epi::strncmp(value, "$MUSIC_", 7) == 0)
			{
				std::string music_sub = language["MusicPrefix"];
				music_sub.append(value.substr(7));
				if (music_sub.size() > 8)
					I_Error("MAPINFO: Song name for \"music\" over 8 characters!\n");
				Z_StrNCpy(val->music, music_sub.data(), 8);
			}
			else if (value.size() > 8)
				I_Error("MAPINFO: Song name for \"music\" over 8 characters!\n");
			else
				Z_StrNCpy(val->music, value.data(), 8);
		}
		else if (epi::case_cmp(key, "exitpic") == 0)
		{
			Z_Clear(val->exitpic, char, 9);
			if (value[0] == '$')
			{
				// Do nothing; this references intermission script lump stuff
			}
			else if (value.size() > 8)
				I_Error("MAPINFO: Mapname for \"exitpic\" over 8 characters!\n");
			else
				Z_StrNCpy(val->exitpic, value.data(), 8);
		}
		else if (epi::case_cmp(key, "enterpic") == 0)
		{
			Z_Clear(val->enterpic, char, 9);
			if (value[0] == '$')
			{
				// Do nothing; this references intermission script lump stuff
			}
			else if (value.size() > 8)
				I_Error("MAPINFO: Mapname for \"exitpic\" over 8 characters!\n");
			else
				Z_StrNCpy(val->enterpic, value.data(), 8);
		}
		else if (epi::case_cmp(key, "par") == 0)
		{
			val->partime = 35 * epi::LEX_Int(value);
		}
		else if (epi::case_cmp(key, "author") == 0)
		{
			if (val->authorname) free(val->authorname);
			val->authorname = (char *)calloc(value.size()+1, sizeof(char));
			Z_StrNCpy(val->authorname, value.c_str(), value.size());
		}
		else if (epi::case_cmp(key, "map07special") == 0)
		{
			val->map07special = true;
		}
		else if (epi::case_cmp(key, "baronspecial") == 0)
		{
			val->baronspecial = true;
		}
		else if (epi::case_cmp(key, "cyberdemonspecial") == 0)
		{
			val->cyberdemonspecial = true;
		}
		else if (epi::case_cmp(key, "spidermastermindspecial") == 0)
		{
			val->spidermastermindspecial = true;
		}
		else if (epi::prefix_case_cmp(key, "specialaction_") == 0)
		{
			// Only one specialaction_* should be used at a time
			if (val->specialaction) free(val->specialaction);
			val->specialaction = (char *)calloc(key.size()+1, sizeof(char));
			Z_StrNCpy(val->specialaction, key.c_str(), key.size());
		}
		else
		{
			SkipToNextLine(lex, tok, value);
		}
	}
	// Some fallback handling
	if (!val->nextsecret[0])
	{
		if (val->nextmap[0])
			Z_StrNCpy(val->nextsecret, val->nextmap, 8);
	}
	if (!val->enterpic[0])
	{
		for(size_t i = 0; i < Maps.mapcount; i++)
		{
			if (!strcmp(val->mapname, Maps.maps[i].nextmap))
			{
				if (Maps.maps[i].exitpic[0])
					Z_StrNCpy(val->enterpic, Maps.maps[i].exitpic, 8);
				break;
			}
		}
	}
}

// -----------------------------------------------
//
// Parses a complete MAPINFO/RMAPINFO lump
//
// -----------------------------------------------

void Parse_MAPINFO(const std::string& buffer)
{
	epi::lexer_c lex(buffer);

	for (;;)
	{
		std::string section;
		epi::token_kind_e tok = lex.Next(section);

		if (tok == epi::TOK_EOF)
			break;

		// skip default/cluster/etc; we just want map entries
		if (epi::case_cmp(section, "map") != 0)
			continue;

		tok = lex.Next(section);

		if (tok != epi::TOK_Ident)
			I_Error("MAPINFO: No mapname for map entry!\n");

		unsigned int i = 0;
		MapEntry parsed = { 0 };
		parsed.mapname = (char *)calloc(section.size()+1, sizeof(char));
		Z_StrNCpy(parsed.mapname, section.data(), section.size());

		tok = lex.Next(section);

		if (epi::case_cmp(section, "lookup") == 0)
		{
			tok = lex.Next(section);
			if (tok != epi::TOK_String)
				I_Error("Malformed MAPINFO lump: missing lookup string for for %s!\n", parsed.mapname);
			else
			{
				const char *ldf_check = language.GetRefOrNull(Deh_Edge::TextStr::GetLDFForBex(section.c_str()));
				if (ldf_check)
				{
					parsed.levelname = (char *)calloc(strlen(ldf_check)+1, sizeof(char));
					Z_StrNCpy(parsed.levelname, ldf_check, strlen(ldf_check));
				}
			}
		}
		else if (tok == epi::TOK_String)
		{
			if (section[0] == '$')
			{
				const char *ldf_check = language.GetRefOrNull(Deh_Edge::TextStr::GetLDFForBex(section.substr(1).c_str()));
				if (ldf_check)
				{
					parsed.levelname = (char *)calloc(strlen(ldf_check)+1, sizeof(char));
					Z_StrNCpy(parsed.levelname, ldf_check, strlen(ldf_check));
				}
			}
			else
			{
				parsed.levelname = (char *)calloc(section.size()+1, sizeof(char));
				Z_StrNCpy(parsed.levelname, section.data(), section.size());
			}
		}
		else
			I_Error("Malformed MAPINFO lump: missing mapname for %s!\n", parsed.mapname);

		lex.Match("{"); // apparently these can optionally be in MAPINFO?

		ParseMAPINFOEntry(lex, &parsed);
		// Does this map entry already exist? If yes, replace it.
		for (i = 0; i < Maps.mapcount; i++)
		{
			if (epi::case_cmp(parsed.mapname, Maps.maps[i].mapname) == 0)
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
			Maps.maps = (MapEntry*)realloc(Maps.maps, sizeof(MapEntry)*Maps.mapcount);
			Maps.maps[Maps.mapcount-1] = parsed;
		}
	}
}

// -----------------------------------------------
//
// Parses a complete ZMAPINFO entry
//
// -----------------------------------------------

static void ParseZMAPINFOEntry(epi::lexer_c& lex, MapEntry *val)
{
	for (;;)
	{
		if (lex.Match("}"))
			break;

		std::string key;
		std::string value;

		epi::token_kind_e tok = lex.Next(key);

		if (tok == epi::TOK_EOF)
			I_Error("Malformed ZMAPINFO lump: unclosed block\n");

		if (tok != epi::TOK_Ident)
			I_Error("Malformed ZMAPINFO lump: missing key\n");

		// Need to read the spec and see which other standalone keys have a UMAPINFO equivalent
		if (! lex.Match("="))
		{
			if (epi::case_cmp(key, "nointermission") == 0)
				val->nointermission = true;
			else if (epi::case_cmp(key, "intermission") == 0)
				val->nointermission = false;
			continue;
		}

		tok = lex.Next(value);

		if (tok == epi::TOK_EOF || tok == epi::TOK_ERROR || value == "}")
			I_Error("Malformed ZMAPINFO lump: missing value\n");

		if (epi::case_cmp(key, "next") == 0)
		{
			Z_Clear(val->nextmap, char, 9);
			if (epi::case_cmp(value, "endpic") == 0)
			{
				Z_Clear(val->endpic, char, 9);
				if (!lex.Match(","))
					I_Error("Malformed ZMAPINFO lump: \"endpic\" specified without lump!\n");
				else
				{
					tok = lex.Next(value);
					if (value.size() > 8)
						I_Error("ZMAPINFO: Name for \"endpic\" lump over 8 characters!\n");
					Z_StrNCpy(val->endpic, value.data(), 8);
				}
			}
			else if (epi::case_cmp(value, "endgame1") == 0 || epi::case_cmp(value, "endgame2") == 0 ||
				epi::case_cmp(value, "endgamew") == 0 || epi::case_cmp(value, "endgame4") == 0 ||
				epi::case_cmp(value, "enddemon") == 0 || epi::case_cmp(value, "endtitle") == 0)
			{
				val->endgame = true;
			}
			else if (epi::case_cmp(value, "endsequence") == 0)
			{
				val->endgame = true;
				SkipToNextLine(lex, tok, value);
			}
			else if (epi::case_cmp(value, "endgamec") == 0)
				val->docast = true;
			else if (epi::case_cmp(value, "endgame3") == 0)
				val->dobunny = true;
			else if (epi::case_cmp(value, "endgames") == 0 || epi::case_cmp(value, "endchess") == 0)
				I_Warning("ZMAPINFO: Hexen/Strife ending sequence referenced!\n");
			else if (epi::case_cmp(value, "endgame") == 0)
			{
				val->endgame = true;
				if (lex.Match("{"))
				{
					for (;;)
					{
						if (lex.Match("}"))
							break;
						tok = lex.Next(key);
						if (tok == epi::TOK_EOF)
							I_Error("Malformed ZMAPINFO lump: unclosed block\n");
						if (epi::case_cmp(key, "cast") == 0)
						{
							val->docast = true;
							continue;
						}
						if (epi::case_cmp(key, "pic") == 0)
						{
							lex.Match("="); // optional
							tok = lex.Next(value);
							if (tok == epi::TOK_EOF || tok == epi::TOK_ERROR || value == "}")
								I_Error("Malformed ZMAPINFO lump: missing value\n");
							Z_Clear(val->endpic, char, 9);
							if (value.size() > 8)
								I_Error("ZMAPINFO: Name for \"endpic\" lump over 8 characters!\n");
							Z_StrNCpy(val->endpic, value.data(), 8);
						}
						else
							continue;
					}
				}
			}
			else
			{
				if (value.size() > 8)
					I_Error("ZMAPINFO: Mapname for \"next\" over 8 characters!\n");
				Z_StrNCpy(val->nextmap, value.data(), 8);
			}
		}
		else if (epi::case_cmp(key, "secretnext") == 0 || epi::case_cmp(key, "secret") == 0)
		{
			Z_Clear(val->nextsecret, char, 9);
			// We will only actually use a map lumpname, if present, but consume and ignore
			// the various end* options for secretnext since they don't seem to have
			// a UMAPINFO equivalent
			if (epi::case_cmp(value, "endpic") == 0)
			{
				SkipToNextLine(lex, tok, value);
			}
			else if (epi::case_cmp(value, "endgame1") == 0 || epi::case_cmp(value, "endgame2") == 0 ||
				epi::case_cmp(value, "endgamew") == 0 || epi::case_cmp(value, "endgame4") == 0 ||
				epi::case_cmp(value, "enddemon") == 0 || epi::case_cmp(value, "endtitle") == 0)
			{
				// nothing needed
			}
			else if (epi::case_cmp(value, "endsequence") == 0)
			{
				SkipToNextLine(lex, tok, value);
			}
			else if (epi::case_cmp(value, "endgamec") == 0)
			{
				// nothing needed
			}
			else if (epi::case_cmp(value, "endgame3") == 0)
			{
				// nothing needed
			}
			else if (epi::case_cmp(value, "endgames") == 0 || epi::case_cmp(value, "endchess") == 0)
				I_Warning("ZMAPINFO: Hexen/Strife ending sequence referenced!\n");
			else if (epi::case_cmp(value, "endgame") == 0)
			{
				if (lex.Match("{"))
				{
					for (;;)
					{
						if (lex.Match("}"))
							break;
						tok = lex.Next(value);
						if (tok == epi::TOK_EOF || tok == epi::TOK_ERROR)
							I_Error("Malformed MAPINFO lump: missing closing bracket!\n");
					}
				}
			}
			else
			{
				if (value.size() > 8)
					I_Error("ZMAPINFO: Mapname for \"secretnext\" over 8 characters!\n");
				Z_StrNCpy(val->nextsecret, value.data(), 8);
			}
		}
		else if (epi::case_cmp(key, "titlepatch") == 0)
		{
			Z_Clear(val->levelpic, char, 9);
			if (value.size() > 8)
				I_Error("ZMAPINFO: Image name for \"titlepatch\" over 8 characters!\n");
			Z_StrNCpy(val->levelpic, value.data(), 8);
			SkipToNextLine(lex, tok, value);
		}
		else if (epi::case_cmp(key, "sky1") == 0 || epi::case_cmp(key, "skybox") == 0)
		{
			Z_Clear(val->skytexture, char, 9);
			if (value.size() > 8)
				I_Error("ZMAPINFO: Image name for sky over 8 characters!\n");
			Z_StrNCpy(val->skytexture, value.data(), 8);
			SkipToNextLine(lex, tok, value);
		}
		else if (epi::case_cmp(key, "music") == 0)
		{
			Z_Clear(val->music, char, 9);
			if (epi::strncmp(value, "$MUSIC_", 7) == 0)
			{
				std::string music_sub = language["MusicPrefix"];
				music_sub.append(value.substr(7));
				if (music_sub.size() > 8)
					I_Error("ZMAPINFO: Song name for \"music\" over 8 characters!\n");
				Z_StrNCpy(val->music, music_sub.data(), 8);
			}
			else if (value.size() > 8)
				I_Error("ZMAPINFO: Song name for \"music\" over 8 characters!\n");
			else
				Z_StrNCpy(val->music, value.data(), 8);
		}
		else if (epi::case_cmp(key, "exitpic") == 0)
		{
			Z_Clear(val->exitpic, char, 9);
			if (value[0] == '$')
			{
				// Do nothing; this references intermission script lump stuff
			}
			else if (value.size() > 8)
				I_Error("UMAPINFO: Mapname for \"exitpic\" over 8 characters!\n");
			else
				Z_StrNCpy(val->exitpic, value.data(), 8);
		}
		else if (epi::case_cmp(key, "enterpic") == 0)
		{
			Z_Clear(val->enterpic, char, 9);
			if (value[0] == '$')
			{
				// Do nothing; this references intermission script lump stuff
			}
			else if (value.size() > 8)
				I_Error("UMAPINFO: Mapname for \"exitpic\" over 8 characters!\n");
			else
				Z_StrNCpy(val->enterpic, value.data(), 8);
		}
		else if (epi::case_cmp(key, "par") == 0)
		{
			val->partime = 35 * epi::LEX_Int(value);
		}
		else if (epi::case_cmp(key, "author") == 0)
		{
			if (val->authorname) free(val->authorname);
			val->authorname = (char *)calloc(value.size()+1, sizeof(char));
			Z_StrNCpy(val->authorname, value.c_str(), value.size());
		}
		else if (epi::case_cmp(key, "map07special") == 0)
		{
			val->map07special = true;
		}
		else if (epi::case_cmp(key, "baronspecial") == 0)
		{
			val->baronspecial = true;
		}
		else if (epi::case_cmp(key, "cyberdemonspecial") == 0)
		{
			val->cyberdemonspecial = true;
		}
		else if (epi::case_cmp(key, "spidermastermindspecial") == 0)
		{
			val->spidermastermindspecial = true;
		}
		else if (epi::prefix_case_cmp(key, "specialaction_") == 0)
		{
			// Only one specialaction_* should be used at a time
			if (val->specialaction) free(val->specialaction);
			val->specialaction = (char *)calloc(key.size()+1, sizeof(char));
			Z_StrNCpy(val->specialaction, key.c_str(), key.size());
		}
		else
		{
			SkipToNextLine(lex, tok, value);
		}
	}
	// Some fallback handling
	if (!val->nextsecret[0])
	{
		if (val->nextmap[0])
			Z_StrNCpy(val->nextsecret, val->nextmap, 8);
	}
	if (!val->enterpic[0])
	{
		for(size_t i = 0; i < Maps.mapcount; i++)
		{
			if (!strcmp(val->mapname, Maps.maps[i].nextmap))
			{
				if (Maps.maps[i].exitpic[0])
					Z_StrNCpy(val->enterpic, Maps.maps[i].exitpic, 8);
				break;
			}
		}
	}
}

// -----------------------------------------------
//
// Parses a complete ZMAPINFO lump
//
// -----------------------------------------------

void Parse_ZMAPINFO(const std::string& buffer)
{
	epi::lexer_c lex(buffer);

	for (;;)
	{
		std::string section;
		epi::token_kind_e tok = lex.Next(section);

		if (tok == epi::TOK_EOF)
			break;

		// skip default/cluster/etc; we just want map entries
		if (epi::case_cmp(section, "map") != 0)
			continue;

		tok = lex.Next(section);

		if (tok != epi::TOK_Ident)
			I_Error("ZMAPINFO: No mapname for map entry!\n");

		unsigned int i = 0;
		MapEntry parsed = { 0 };
		parsed.mapname = (char *)calloc(section.size()+1, sizeof(char));
		Z_StrNCpy(parsed.mapname, section.data(), section.size());

		tok = lex.Next(section);

		if (epi::case_cmp(section, "lookup") == 0)
		{
			tok = lex.Next(section);
			if (tok != epi::TOK_String)
				I_Error("Malformed ZMAPINFO lump: missing lookup string for for %s!\n", parsed.mapname);
			else
			{
				const char *ldf_check = language.GetRefOrNull(Deh_Edge::TextStr::GetLDFForBex(section.c_str()));
				if (ldf_check)
				{
					parsed.levelname = (char *)calloc(strlen(ldf_check)+1, sizeof(char));
					Z_StrNCpy(parsed.levelname, ldf_check, strlen(ldf_check));
				}
			}
		}
		else if (tok == epi::TOK_String)
		{
			if (section[0] == '$')
			{
				const char *ldf_check = language.GetRefOrNull(Deh_Edge::TextStr::GetLDFForBex(section.substr(1).c_str()));
				if (ldf_check)
				{
					parsed.levelname = (char *)calloc(strlen(ldf_check)+1, sizeof(char));
					Z_StrNCpy(parsed.levelname, ldf_check, strlen(ldf_check));
				}
			}
			else
			{
				parsed.levelname = (char *)calloc(section.size()+1, sizeof(char));
				Z_StrNCpy(parsed.levelname, section.data(), section.size());
			}
		}
		else
			I_Error("Malformed ZMAPINFO lump: missing mapname for %s!\n", parsed.mapname);

		if (! lex.Match("{"))
			I_Error("Malformed ZMAPINFO lump: missing '{'\n");

		ParseZMAPINFOEntry(lex, &parsed);
		// Does this map entry already exist? If yes, replace it.
		for (i = 0; i < Maps.mapcount; i++)
		{
			if (epi::case_cmp(parsed.mapname, Maps.maps[i].mapname) == 0)
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
			Maps.maps = (MapEntry*)realloc(Maps.maps, sizeof(MapEntry)*Maps.mapcount);
			Maps.maps[Maps.mapcount-1] = parsed;
		}
	}
}