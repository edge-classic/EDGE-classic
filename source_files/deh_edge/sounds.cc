//------------------------------------------------------------------------
//  SOUND Definitions
//------------------------------------------------------------------------
//
//  DEH_EDGE  Copyright (C) 2004-2005  The EDGE Team
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
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

#include "i_defs.h"
#include "sounds.h"

#include "buffer.h"
#include "dh_plugin.h"
#include "patch.h"
#include "storage.h"
#include "system.h"
#include "util.h"
#include "wad.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>


namespace Deh_Edge
{

//
// Information about all the music
//

musicinfo_t S_music[NUMMUSIC] =
{
    { NULL, -1, NULL },  // dummy entry

    { "e1m1", 33, NULL },
    { "e1m2", 34, NULL },
    { "e1m3", 35, NULL },
    { "e1m4", 36, NULL },
    { "e1m5", 37, NULL },
    { "e1m6", 38, NULL },
    { "e1m7", 39, NULL },
    { "e1m8", 40, NULL },
    { "e1m9", 41, NULL },
    { "e2m1", 42, NULL },
    { "e2m2", 43, NULL },
    { "e2m3", 44, NULL },
    { "e2m4", 45, NULL },
    { "e2m5", 46, NULL },
    { "e2m6", 47, NULL },
    { "e2m7", 48, NULL },
    { "e2m8", 49, NULL },
    { "e2m9", 50, NULL },
    { "e3m1", 51, NULL },
    { "e3m2", 52, NULL },
    { "e3m3", 53, NULL },
    { "e3m4", 54, NULL },
    { "e3m5", 55, NULL },
    { "e3m6", 56, NULL },
    { "e3m7", 57, NULL },
    { "e3m8", 58, NULL },
    { "e3m9", 59, NULL },

    { "inter",  63, NULL },
    { "intro",  62, NULL },
    { "bunny",  67, NULL },
    { "victor", 61, NULL },
    { "introa", 68, NULL },
    { "runnin",  1, NULL },
    { "stalks",  2, NULL },
    { "countd",  3, NULL },
    { "betwee",  4, NULL },
    { "doom",    5, NULL },
    { "the_da",  6, NULL },
    { "shawn",   7, NULL },
    { "ddtblu",  8, NULL },
    { "in_cit",  9, NULL },
    { "dead",   10, NULL },
    { "stlks2", 11, NULL },
    { "theda2", 12, NULL },
    { "doom2",  13, NULL },
    { "ddtbl2", 14, NULL },
    { "runni2", 15, NULL },
    { "dead2",  16, NULL },
    { "stlks3", 17, NULL },
    { "romero", 18, NULL },
    { "shawn2", 19, NULL },
    { "messag", 20, NULL },
    { "count2", 21, NULL },
    { "ddtbl3", 22, NULL },
    { "ampie",  23, NULL },
    { "theda3", 24, NULL },
    { "adrian", 25, NULL },
    { "messg2", 26, NULL },
    { "romer2", 27, NULL },
    { "tense",  28, NULL },
    { "shawn3", 29, NULL },
    { "openin", 30, NULL },
    { "evil",   31, NULL },
    { "ultima", 32, NULL },
    { "read_m", 60, NULL },
    { "dm2ttl", 65, NULL },
    { "dm2int", 64, NULL } 
};


//------------------------------------------------------------------------
//
// Information about all the sfx
//

sfxinfo_t S_sfx[NUMSFX_BEX] =
{
	// S_sfx[0] needs to be a dummy for odd reasons.
	{ "none", 0,  0, 0, -1, -1, NULL },

	{ "pistol", 0,  64, 0, -1, -1, NULL },
	{ "shotgn", 0,  64, 0, -1, -1, NULL },
	{ "sgcock", 0,  64, 0, -1, -1, NULL },
	{ "dshtgn", 0,  64, 0, -1, -1, NULL },
	{ "dbopn",  0,  64, 0, -1, -1, NULL },
	{ "dbcls",  0,  64, 0, -1, -1, NULL },
	{ "dbload", 0,  64, 0, -1, -1, NULL },
	{ "plasma", 0,  64, 0, -1, -1, NULL },
	{ "bfg",    0,  64, 0, -1, -1, NULL },
	{ "sawup",  2,  64, 0, -1, -1, NULL },
	{ "sawidl", 2, 118, 0, -1, -1, NULL },
	{ "sawful", 2,  64, 0, -1, -1, NULL },
	{ "sawhit", 2,  64, 0, -1, -1, NULL },
	{ "rlaunc", 0,  64, 0, -1, -1, NULL },
	{ "rxplod", 0,  70, 0, -1, -1, NULL },
	{ "firsht", 0,  70, 0, -1, -1, NULL },
	{ "firxpl", 0,  70, 0, -1, -1, NULL },
	{ "pstart", 18, 100, 0, -1, -1, NULL },
	{ "pstop",  18, 100, 0, -1, -1, NULL },
	{ "doropn", 0,  100, 0, -1, -1, NULL },
	{ "dorcls", 0,  100, 0, -1, -1, NULL },
	{ "stnmov", 18, 119, 0, -1, -1, NULL },
	{ "swtchn", 0,  78, 0, -1, -1, NULL },
	{ "swtchx", 0,  78, 0, -1, -1, NULL },
	{ "plpain", 0,  96, 0, -1, -1, NULL },
	{ "dmpain", 0,  96, 0, -1, -1, NULL },
	{ "popain", 0,  96, 0, -1, -1, NULL },
	{ "vipain", 0,  96, 0, -1, -1, NULL },
	{ "mnpain", 0,  96, 0, -1, -1, NULL },
	{ "pepain", 0,  96, 0, -1, -1, NULL },
	{ "slop",   0,  78, 0, -1, -1, NULL },
	{ "itemup", 20, 78, 0, -1, -1, NULL },
	{ "wpnup",  21, 78, 0, -1, -1, NULL },
	{ "oof",    0,  96, 0, -1, -1, NULL },
	{ "telept", 0,  32, 0, -1, -1, NULL },
	{ "posit1", 3,  98, 0, -1, -1, NULL },
	{ "posit2", 3,  98, 0, -1, -1, NULL },
	{ "posit3", 3,  98, 0, -1, -1, NULL },
	{ "bgsit1", 4,  98, 0, -1, -1, NULL },
	{ "bgsit2", 4,  98, 0, -1, -1, NULL },
	{ "sgtsit", 5,  98, 0, -1, -1, NULL },
	{ "cacsit", 6,  98, 0, -1, -1, NULL },
	{ "brssit", 7,  94, 0, -1, -1, NULL },
	{ "cybsit", 8,  92, 0, -1, -1, NULL },
	{ "spisit", 9,  90, 0, -1, -1, NULL },
	{ "bspsit", 10, 90, 0, -1, -1, NULL },
	{ "kntsit", 11, 90, 0, -1, -1, NULL },
	{ "vilsit", 12, 90, 0, -1, -1, NULL },
	{ "mansit", 13, 90, 0, -1, -1, NULL },
	{ "pesit",  14, 90, 0, -1, -1, NULL },
	{ "sklatk", 0,  70, 0, -1, -1, NULL },
	{ "sgtatk", 0,  70, 0, -1, -1, NULL },
	{ "skepch", 0,  70, 0, -1, -1, NULL },
	{ "vilatk", 0,  70, 0, -1, -1, NULL },
	{ "claw",   0,  70, 0, -1, -1, NULL },
	{ "skeswg", 0,  70, 0, -1, -1, NULL },
	{ "pldeth", 0,  32, 0, -1, -1, NULL },
	{ "pdiehi", 0,  32, 0, -1, -1, NULL },
	{ "podth1", 0,  70, 0, -1, -1, NULL },
	{ "podth2", 0,  70, 0, -1, -1, NULL },
	{ "podth3", 0,  70, 0, -1, -1, NULL },
	{ "bgdth1", 0,  70, 0, -1, -1, NULL },
	{ "bgdth2", 0,  70, 0, -1, -1, NULL },
	{ "sgtdth", 0,  70, 0, -1, -1, NULL },
	{ "cacdth", 0,  70, 0, -1, -1, NULL },
	{ "skldth", 0,  70, 0, -1, -1, NULL },
	{ "brsdth", 0,  32, 0, -1, -1, NULL },
	{ "cybdth", 0,  32, 0, -1, -1, NULL },
	{ "spidth", 0,  32, 0, -1, -1, NULL },
	{ "bspdth", 0,  32, 0, -1, -1, NULL },
	{ "vildth", 0,  32, 0, -1, -1, NULL },
	{ "kntdth", 0,  32, 0, -1, -1, NULL },
	{ "pedth",  0,  32, 0, -1, -1, NULL },
	{ "skedth", 0,  32, 0, -1, -1, NULL },
	{ "posact", 3,  120, 0, -1, -1, NULL },
	{ "bgact",  4,  120, 0, -1, -1, NULL },
	{ "dmact",  15, 120, 0, -1, -1, NULL },
	{ "bspact", 10, 100, 0, -1, -1, NULL },
	{ "bspwlk", 16, 100, 0, -1, -1, NULL },
	{ "vilact", 12, 100, 0, -1, -1, NULL },
	{ "noway",  0,  78, 0, -1, -1, NULL },
	{ "barexp", 0,  60, 0, -1, -1, NULL },
	{ "punch",  0,  64, 0, -1, -1, NULL },
	{ "hoof",   0,  70, 0, -1, -1, NULL },
	{ "metal",  0,  70, 0, -1, -1, NULL },
	{ "chgun",  0,  64, sfx_pistol, 150, 0, NULL },
	{ "tink",   0,  60, 0, -1, -1, NULL },
	{ "bdopn",  0, 100, 0, -1, -1, NULL },
	{ "bdcls",  0, 100, 0, -1, -1, NULL },
	{ "itmbk",  0, 100, 0, -1, -1, NULL },
	{ "flame",  0,  32, 0, -1, -1, NULL },
	{ "flamst", 0,  32, 0, -1, -1, NULL },
	{ "getpow", 0,  60, 0, -1, -1, NULL },
	{ "bospit", 0,  70, 0, -1, -1, NULL },
	{ "boscub", 0,  70, 0, -1, -1, NULL },
	{ "bossit", 0,  70, 0, -1, -1, NULL },
	{ "bospn",  0,  70, 0, -1, -1, NULL },
	{ "bosdth", 0,  70, 0, -1, -1, NULL },
	{ "manatk", 0,  70, 0, -1, -1, NULL },
	{ "mandth", 0,  70, 0, -1, -1, NULL },
	{ "sssit",  0,  70, 0, -1, -1, NULL },
	{ "ssdth",  0,  70, 0, -1, -1, NULL },
	{ "keenpn", 0,  70, 0, -1, -1, NULL },
	{ "keendt", 0,  70, 0, -1, -1, NULL },
	{ "skeact", 0,  70, 0, -1, -1, NULL },
	{ "skesit", 0,  70, 0, -1, -1, NULL },
	{ "skeatk", 0,  70, 0, -1, -1, NULL },
	{ "radio",  0,  60, 0, -1, -1, NULL },

	// BOOM and MBF sounds...
	{ "dgsit",  0,  98, 0, -1, -1, NULL },
	{ "dgatk",  0,  70, 0, -1, -1, NULL },
	{ "dgact",  0, 120, 0, -1, -1, NULL },
	{ "dgdth",  0,  70, 0, -1, -1, NULL },
	{ "dgpain", 0,  96, 0, -1, -1, NULL },
	{ "secret", 0,  60, 0, -1, -1, NULL },
	{ "gibdth", 0,  60, 0, -1, -1, "gibdth" },
	{ "scrsht", 0,   0, 0, -1, -1, "scrsht" },

	// A LOT of dummies
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },
	{ "none", 0,  0, 0, -1, -1, NULL },

	// DEHEXTRA
	{ "fre000", 0, 127, 0, -1, -1, "fre000" },
	{ "fre001", 0, 127, 0, -1, -1, "fre001" },
	{ "fre002", 0, 127, 0, -1, -1, "fre002" },
	{ "fre003", 0, 127, 0, -1, -1, "fre003" },
	{ "fre004", 0, 127, 0, -1, -1, "fre004" },
	{ "fre005", 0, 127, 0, -1, -1, "fre005" },
	{ "fre006", 0, 127, 0, -1, -1, "fre006" },
	{ "fre007", 0, 127, 0, -1, -1, "fre007" },
	{ "fre008", 0, 127, 0, -1, -1, "fre008" },
	{ "fre009", 0, 127, 0, -1, -1, "fre009" },
	{ "fre010", 0, 127, 0, -1, -1, "fre010" },
	{ "fre011", 0, 127, 0, -1, -1, "fre011" },
	{ "fre012", 0, 127, 0, -1, -1, "fre012" },
	{ "fre013", 0, 127, 0, -1, -1, "fre013" },
	{ "fre014", 0, 127, 0, -1, -1, "fre014" },
	{ "fre015", 0, 127, 0, -1, -1, "fre015" },
	{ "fre016", 0, 127, 0, -1, -1, "fre016" },
	{ "fre017", 0, 127, 0, -1, -1, "fre017" },
	{ "fre018", 0, 127, 0, -1, -1, "fre018" },
	{ "fre019", 0, 127, 0, -1, -1, "fre019" },
	{ "fre020", 0, 127, 0, -1, -1, "fre020" },
	{ "fre021", 0, 127, 0, -1, -1, "fre021" },
	{ "fre022", 0, 127, 0, -1, -1, "fre022" },
	{ "fre023", 0, 127, 0, -1, -1, "fre023" },
	{ "fre024", 0, 127, 0, -1, -1, "fre024" },
	{ "fre025", 0, 127, 0, -1, -1, "fre025" },
	{ "fre026", 0, 127, 0, -1, -1, "fre026" },
	{ "fre027", 0, 127, 0, -1, -1, "fre027" },
	{ "fre028", 0, 127, 0, -1, -1, "fre028" },
	{ "fre029", 0, 127, 0, -1, -1, "fre029" },
	{ "fre030", 0, 127, 0, -1, -1, "fre030" },
	{ "fre031", 0, 127, 0, -1, -1, "fre031" },
	{ "fre032", 0, 127, 0, -1, -1, "fre032" },
	{ "fre033", 0, 127, 0, -1, -1, "fre033" },
	{ "fre034", 0, 127, 0, -1, -1, "fre034" },
	{ "fre035", 0, 127, 0, -1, -1, "fre035" },
	{ "fre036", 0, 127, 0, -1, -1, "fre036" },
	{ "fre037", 0, 127, 0, -1, -1, "fre037" },
	{ "fre038", 0, 127, 0, -1, -1, "fre038" },
	{ "fre039", 0, 127, 0, -1, -1, "fre039" },
	{ "fre040", 0, 127, 0, -1, -1, "fre040" },
	{ "fre041", 0, 127, 0, -1, -1, "fre041" },
	{ "fre042", 0, 127, 0, -1, -1, "fre042" },
	{ "fre043", 0, 127, 0, -1, -1, "fre043" },
	{ "fre044", 0, 127, 0, -1, -1, "fre044" },
	{ "fre045", 0, 127, 0, -1, -1, "fre045" },
	{ "fre046", 0, 127, 0, -1, -1, "fre046" },
	{ "fre047", 0, 127, 0, -1, -1, "fre047" },
	{ "fre048", 0, 127, 0, -1, -1, "fre048" },
	{ "fre049", 0, 127, 0, -1, -1, "fre049" },
	{ "fre050", 0, 127, 0, -1, -1, "fre050" },
	{ "fre051", 0, 127, 0, -1, -1, "fre051" },
	{ "fre052", 0, 127, 0, -1, -1, "fre052" },
	{ "fre053", 0, 127, 0, -1, -1, "fre053" },
	{ "fre054", 0, 127, 0, -1, -1, "fre054" },
	{ "fre055", 0, 127, 0, -1, -1, "fre055" },
	{ "fre056", 0, 127, 0, -1, -1, "fre056" },
	{ "fre057", 0, 127, 0, -1, -1, "fre057" },
	{ "fre058", 0, 127, 0, -1, -1, "fre058" },
	{ "fre059", 0, 127, 0, -1, -1, "fre059" },
	{ "fre060", 0, 127, 0, -1, -1, "fre060" },
	{ "fre061", 0, 127, 0, -1, -1, "fre061" },
	{ "fre062", 0, 127, 0, -1, -1, "fre062" },
	{ "fre063", 0, 127, 0, -1, -1, "fre063" },
	{ "fre064", 0, 127, 0, -1, -1, "fre064" },
	{ "fre065", 0, 127, 0, -1, -1, "fre065" },
	{ "fre066", 0, 127, 0, -1, -1, "fre066" },
	{ "fre067", 0, 127, 0, -1, -1, "fre067" },
	{ "fre068", 0, 127, 0, -1, -1, "fre068" },
	{ "fre069", 0, 127, 0, -1, -1, "fre069" },
	{ "fre070", 0, 127, 0, -1, -1, "fre070" },
	{ "fre071", 0, 127, 0, -1, -1, "fre071" },
	{ "fre072", 0, 127, 0, -1, -1, "fre072" },
	{ "fre073", 0, 127, 0, -1, -1, "fre073" },
	{ "fre074", 0, 127, 0, -1, -1, "fre074" },
	{ "fre075", 0, 127, 0, -1, -1, "fre075" },
	{ "fre076", 0, 127, 0, -1, -1, "fre076" },
	{ "fre077", 0, 127, 0, -1, -1, "fre077" },
	{ "fre078", 0, 127, 0, -1, -1, "fre078" },
	{ "fre079", 0, 127, 0, -1, -1, "fre079" },
	{ "fre080", 0, 127, 0, -1, -1, "fre080" },
	{ "fre081", 0, 127, 0, -1, -1, "fre081" },
	{ "fre082", 0, 127, 0, -1, -1, "fre082" },
	{ "fre083", 0, 127, 0, -1, -1, "fre083" },
	{ "fre084", 0, 127, 0, -1, -1, "fre084" },
	{ "fre085", 0, 127, 0, -1, -1, "fre085" },
	{ "fre086", 0, 127, 0, -1, -1, "fre086" },
	{ "fre087", 0, 127, 0, -1, -1, "fre087" },
	{ "fre088", 0, 127, 0, -1, -1, "fre088" },
	{ "fre089", 0, 127, 0, -1, -1, "fre089" },
	{ "fre090", 0, 127, 0, -1, -1, "fre090" },
	{ "fre091", 0, 127, 0, -1, -1, "fre091" },
	{ "fre092", 0, 127, 0, -1, -1, "fre092" },
	{ "fre093", 0, 127, 0, -1, -1, "fre093" },
	{ "fre094", 0, 127, 0, -1, -1, "fre094" },
	{ "fre095", 0, 127, 0, -1, -1, "fre095" },
	{ "fre096", 0, 127, 0, -1, -1, "fre096" },
	{ "fre097", 0, 127, 0, -1, -1, "fre097" },
	{ "fre098", 0, 127, 0, -1, -1, "fre098" },
	{ "fre099", 0, 127, 0, -1, -1, "fre099" },
	{ "fre100", 0, 127, 0, -1, -1, "fre100" },
	{ "fre101", 0, 127, 0, -1, -1, "fre101" },
	{ "fre102", 0, 127, 0, -1, -1, "fre102" },
	{ "fre103", 0, 127, 0, -1, -1, "fre103" },
	{ "fre104", 0, 127, 0, -1, -1, "fre104" },
	{ "fre105", 0, 127, 0, -1, -1, "fre105" },
	{ "fre106", 0, 127, 0, -1, -1, "fre106" },
	{ "fre107", 0, 127, 0, -1, -1, "fre107" },
	{ "fre108", 0, 127, 0, -1, -1, "fre108" },
	{ "fre109", 0, 127, 0, -1, -1, "fre109" },
	{ "fre110", 0, 127, 0, -1, -1, "fre110" },
	{ "fre111", 0, 127, 0, -1, -1, "fre111" },
	{ "fre112", 0, 127, 0, -1, -1, "fre112" },
	{ "fre113", 0, 127, 0, -1, -1, "fre113" },
	{ "fre114", 0, 127, 0, -1, -1, "fre114" },
	{ "fre115", 0, 127, 0, -1, -1, "fre115" },
	{ "fre116", 0, 127, 0, -1, -1, "fre116" },
	{ "fre117", 0, 127, 0, -1, -1, "fre117" },
	{ "fre118", 0, 127, 0, -1, -1, "fre118" },
	{ "fre119", 0, 127, 0, -1, -1, "fre119" },
	{ "fre120", 0, 127, 0, -1, -1, "fre120" },
	{ "fre121", 0, 127, 0, -1, -1, "fre121" },
	{ "fre122", 0, 127, 0, -1, -1, "fre122" },
	{ "fre123", 0, 127, 0, -1, -1, "fre123" },
	{ "fre124", 0, 127, 0, -1, -1, "fre124" },
	{ "fre125", 0, 127, 0, -1, -1, "fre125" },
	{ "fre126", 0, 127, 0, -1, -1, "fre126" },
	{ "fre127", 0, 127, 0, -1, -1, "fre127" },
	{ "fre128", 0, 127, 0, -1, -1, "fre128" },
	{ "fre129", 0, 127, 0, -1, -1, "fre129" },
	{ "fre130", 0, 127, 0, -1, -1, "fre130" },
	{ "fre131", 0, 127, 0, -1, -1, "fre131" },
	{ "fre132", 0, 127, 0, -1, -1, "fre132" },
	{ "fre133", 0, 127, 0, -1, -1, "fre133" },
	{ "fre134", 0, 127, 0, -1, -1, "fre134" },
	{ "fre135", 0, 127, 0, -1, -1, "fre135" },
	{ "fre136", 0, 127, 0, -1, -1, "fre136" },
	{ "fre137", 0, 127, 0, -1, -1, "fre137" },
	{ "fre138", 0, 127, 0, -1, -1, "fre138" },
	{ "fre139", 0, 127, 0, -1, -1, "fre139" },
	{ "fre140", 0, 127, 0, -1, -1, "fre140" },
	{ "fre141", 0, 127, 0, -1, -1, "fre141" },
	{ "fre142", 0, 127, 0, -1, -1, "fre142" },
	{ "fre143", 0, 127, 0, -1, -1, "fre143" },
	{ "fre144", 0, 127, 0, -1, -1, "fre144" },
	{ "fre145", 0, 127, 0, -1, -1, "fre145" },
	{ "fre146", 0, 127, 0, -1, -1, "fre146" },
	{ "fre147", 0, 127, 0, -1, -1, "fre147" },
	{ "fre148", 0, 127, 0, -1, -1, "fre148" },
	{ "fre149", 0, 127, 0, -1, -1, "fre149" },
	{ "fre150", 0, 127, 0, -1, -1, "fre150" },
	{ "fre151", 0, 127, 0, -1, -1, "fre151" },
	{ "fre152", 0, 127, 0, -1, -1, "fre152" },
	{ "fre153", 0, 127, 0, -1, -1, "fre153" },
	{ "fre154", 0, 127, 0, -1, -1, "fre154" },
	{ "fre155", 0, 127, 0, -1, -1, "fre155" },
	{ "fre156", 0, 127, 0, -1, -1, "fre156" },
	{ "fre157", 0, 127, 0, -1, -1, "fre157" },
	{ "fre158", 0, 127, 0, -1, -1, "fre158" },
	{ "fre159", 0, 127, 0, -1, -1, "fre159" },
	{ "fre160", 0, 127, 0, -1, -1, "fre160" },
	{ "fre161", 0, 127, 0, -1, -1, "fre161" },
	{ "fre162", 0, 127, 0, -1, -1, "fre162" },
	{ "fre163", 0, 127, 0, -1, -1, "fre163" },
	{ "fre164", 0, 127, 0, -1, -1, "fre164" },
	{ "fre165", 0, 127, 0, -1, -1, "fre165" },
	{ "fre166", 0, 127, 0, -1, -1, "fre166" },
	{ "fre167", 0, 127, 0, -1, -1, "fre167" },
	{ "fre168", 0, 127, 0, -1, -1, "fre168" },
	{ "fre169", 0, 127, 0, -1, -1, "fre169" },
	{ "fre170", 0, 127, 0, -1, -1, "fre170" },
	{ "fre171", 0, 127, 0, -1, -1, "fre171" },
	{ "fre172", 0, 127, 0, -1, -1, "fre172" },
	{ "fre173", 0, 127, 0, -1, -1, "fre173" },
	{ "fre174", 0, 127, 0, -1, -1, "fre174" },
	{ "fre175", 0, 127, 0, -1, -1, "fre175" },
	{ "fre176", 0, 127, 0, -1, -1, "fre176" },
	{ "fre177", 0, 127, 0, -1, -1, "fre177" },
	{ "fre178", 0, 127, 0, -1, -1, "fre178" },
	{ "fre179", 0, 127, 0, -1, -1, "fre179" },
	{ "fre180", 0, 127, 0, -1, -1, "fre180" },
	{ "fre181", 0, 127, 0, -1, -1, "fre181" },
	{ "fre182", 0, 127, 0, -1, -1, "fre182" },
	{ "fre183", 0, 127, 0, -1, -1, "fre183" },
	{ "fre184", 0, 127, 0, -1, -1, "fre184" },
	{ "fre185", 0, 127, 0, -1, -1, "fre185" },
	{ "fre186", 0, 127, 0, -1, -1, "fre186" },
	{ "fre187", 0, 127, 0, -1, -1, "fre187" },
	{ "fre188", 0, 127, 0, -1, -1, "fre188" },
	{ "fre189", 0, 127, 0, -1, -1, "fre189" },
	{ "fre190", 0, 127, 0, -1, -1, "fre190" },
	{ "fre191", 0, 127, 0, -1, -1, "fre191" },
	{ "fre192", 0, 127, 0, -1, -1, "fre192" },
	{ "fre193", 0, 127, 0, -1, -1, "fre193" },
	{ "fre194", 0, 127, 0, -1, -1, "fre194" },
	{ "fre195", 0, 127, 0, -1, -1, "fre195" },
	{ "fre196", 0, 127, 0, -1, -1, "fre196" },
	{ "fre197", 0, 127, 0, -1, -1, "fre197" },
	{ "fre198", 0, 127, 0, -1, -1, "fre198" },
	{ "fre199", 0, 127, 0, -1, -1, "fre199" }
};


//------------------------------------------------------------------------

void Sounds::Startup(void)
{

}

namespace Sounds
{
	bool some_sound_modified = false;
	bool got_one;
	bool sound_modified[NUMSFX_BEX];

	
	void MarkSound(int s_num)
	{
		// can happen since the binary patches contain the dummy sound
		if (s_num == sfx_None)
			return;

		assert(1 <= s_num && s_num < NUMSFX_BEX);

		some_sound_modified = true;
	}

	void AlterSound(int new_val)
	{
		int s_num = Patch::active_obj;
		const char *deh_field = Patch::line_buf;

		assert(0 <= s_num && s_num < NUMSFX_BEX);

		if (StrCaseCmpPartial(deh_field, "Zero") == 0 ||
		    StrCaseCmpPartial(deh_field, "Neg. One") == 0)
			return;

		if (StrCaseCmp(deh_field, "Offset") == 0)
		{
			PrintWarn("Line %d: raw sound Offset not supported.\n", Patch::line_num);
			return;
		}

		if (StrCaseCmp(deh_field, "Value") == 0)  // priority
		{
			if (new_val < 0)
			{
				PrintWarn("Line %d: bad sound priority value: %d.\n",
					Patch::line_num, new_val);
				new_val = 0;
			}

			Storage::RememberMod(&S_sfx[s_num].priority, new_val);

			MarkSound(s_num);
			return;
		}

		if (StrCaseCmp(deh_field, "Zero/One") == 0)  // singularity, ignored
			return;

		PrintWarn("UNKNOWN SOUND FIELD: %s\n", deh_field);
	}

	const char *GetEdgeSfxName(int sound_id)
	{
		assert(sound_id != sfx_None);

		switch (sound_id)
		{
			// EDGE uses different names for the DOG sounds
			case sfx_dgsit:  return "DOG_SIGHT";
			case sfx_dgatk:  return "DOG_BITE";
			case sfx_dgact:  return "DOG_LOOK";
			case sfx_dgdth:  return "DOG_DIE";
			case sfx_dgpain: return "DOG_PAIN";

			default: break;
		}

		return S_sfx[sound_id].orig_name;
	}

	const char *GetSound(int sound_id)
	{
		assert(sound_id != sfx_None);
		assert(strlen(S_sfx[sound_id].orig_name) < 16);

		if (sound_id >= 500)
		{
			sound_modified[sound_id] = true;
			MarkSound(sound_id);
		}

		// handle random sounds
		switch (sound_id)
		{
			case sfx_podth1: case sfx_podth2: case sfx_podth3:
				return "\"PODTH?\"";

			case sfx_posit1: case sfx_posit2: case sfx_posit3:
				return "\"POSIT?\"";

			case sfx_bgdth1: case sfx_bgdth2:
				return "\"BGDTH?\"";

			case sfx_bgsit1: case sfx_bgsit2:
				return "\"BGSIT?\"";

			default: break;
		}

		static char name_buf[40];

		sprintf(name_buf, "\"%s\"", StrUpper(GetEdgeSfxName(sound_id)));

		
		return name_buf;
	}

	void BeginSoundLump(void)
	{
		WAD::NewLump("DDFSFX");

		WAD::Printf(GEN_BY_COMMENT);
		WAD::Printf("<SOUNDS>\n\n");
	}

	void FinishSoundLump(void)
	{
		WAD::Printf("\n");
		WAD::FinishLump();
	}

	void BeginMusicLump(void)
	{
		WAD::NewLump("DDFPLAY");

		WAD::Printf(GEN_BY_COMMENT);
		WAD::Printf("<PLAYLISTS>\n\n");
	}

	void FinishMusicLump(void)
	{
		WAD::Printf("\n");
		WAD::FinishLump();
	}

	void WriteSound(int s_num)
	{
		sfxinfo_t *sound = S_sfx + s_num;

		if (! got_one)
		{
			got_one = true;
			BeginSoundLump();
		}


		WAD::Printf("[%s]\n", StrUpper(GetEdgeSfxName(s_num)));

		const char *lump = !sound->new_name.empty() ? sound->new_name.c_str() : sound->orig_name;

		if (sound->link)
		{
			sfxinfo_t *link = S_sfx + sound->link;

			lump = !link->new_name.empty() ? link->new_name.c_str() : GetEdgeSfxName(sound->link);
		}

		WAD::Printf("LUMP_NAME = \"DS%s\";\n", StrUpper(lump));
		WAD::Printf("PRIORITY = %d;\n", sound->priority);

		if (sound->singularity != 0)
			WAD::Printf("SINGULAR = %d;\n", sound->singularity);

		if (s_num == sfx_stnmov)
			WAD::Printf("LOOP = TRUE;\n");

		WAD::Printf("\n");
	}

	void WriteMusic(int m_num)
	{
		musicinfo_t *mus = S_music + m_num;

		if (! got_one)
		{
			got_one = true;
			BeginMusicLump();
		}

		WAD::Printf("[%02d] ", mus->ddf_num);

		const char *lump = !mus->new_name.empty() ? mus->new_name.c_str() : mus->orig_name;

		WAD::Printf("MUSICINFO = MUS:LUMP:\"D_%s\";\n", StrUpper(lump));
	}
}

void Sounds::ConvertSFX(void)
{
	if (! all_mode && ! some_sound_modified)
		return;

	got_one = false;

	for (int i = 1; i < NUMSFX_BEX; i++)
	{
	    if (! all_mode && S_sfx[i].new_name.empty())
			continue;

		if(sound_modified[i] == true)
			WriteSound(i);
	}
		
	if (got_one)
		FinishSoundLump();
}

void Sounds::ConvertMUS(void)
{
	got_one = false;

	for (int i = 1; i < NUMMUSIC; i++)
	{
	    if (! all_mode && S_music[i].new_name.empty())
			continue;

		WriteMusic(i);
	}
		
	if (got_one)
		FinishMusicLump();
}


//------------------------------------------------------------------------

bool Sounds::ReplaceSound(const char *before, const char *after)
{
	for (int i = 1; i < NUMSFX_BEX; i++)
	{
		if (StrCaseCmp(S_sfx[i].orig_name, before) != 0)
			continue;

		if (!S_sfx[i].new_name.empty())
			S_sfx[i].new_name.clear();

		S_sfx[i].new_name = StringDup(after);

		MarkSound(i);

		return true;
	}

	return false;
}

bool Sounds::ReplaceMusic(const char *before, const char *after)
{
	for (int j = 1; j < NUMMUSIC; j++)
	{
		if (StrCaseCmp(S_music[j].orig_name, before) != 0)
			continue;

		if (!S_music[j].new_name.empty())
			S_music[j].new_name.clear();

		S_music[j].new_name = StringDup(after);

		return true;
	}

	return false;
}

void Sounds::AlterBexSound(const char *new_val)
{
	const char *old_val = Patch::line_buf;

	if (strlen(old_val) < 1 || strlen(old_val) > 6)
	{
		PrintWarn("Bad length for sound name '%s'.\n", old_val);
		return;
	}

	if (strlen(new_val) < 1 || strlen(new_val) > 6)
	{
		PrintWarn("Bad length for sound name '%s'.\n", new_val);
		return;
	}

	if (! ReplaceSound(old_val, new_val))
		PrintWarn("Line %d: unknown sound name '%s'.\n",
			Patch::line_num, old_val);
}

void Sounds::AlterBexMusic(const char *new_val)
{
	const char *old_val = Patch::line_buf;

	if (strlen(old_val) < 1 || strlen(old_val) > 6)
	{
		PrintWarn("Bad length for music name '%s'.\n", old_val);
		return;
	}

	if (strlen(new_val) < 1 || strlen(new_val) > 6)
	{
		PrintWarn("Bad length for music name '%s'.\n", new_val);
		return;
	}

	if (! ReplaceMusic(old_val, new_val))
		PrintWarn("Line %d: unknown music name '%s'.\n",
			Patch::line_num, old_val);
}

}  // Deh_Edge
