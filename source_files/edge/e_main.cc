//---------------------------------------------------------------------------
//  EDGE Main Init + Program Loop Code
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2009  The EDGE Team.
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
//  Based on the DOOM source code, released by Id Software under the
//  following copyright:
//
//    Copyright (C) 1993-1996 by id Software, Inc.
//
//----------------------------------------------------------------------------
//
// DESCRIPTION:
//      EDGE main program (E_Main),
//      game loop (E_Loop) and startup functions.
//
// -MH- 1998/07/02 "shootupdown" --> "true3dgameplay"
// -MH- 1998/08/19 added up/down movement variables
//

#include "i_defs.h"
#include "e_main.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <algorithm>
#include <array>
#include <vector>

#include "exe_path.h"
#include "file.h"
#include "filesystem.h"
#include "path.h"
#include "utility.h"

#include "am_map.h"
#include "con_gui.h"
#include "con_main.h"
#include "con_var.h"
#include "dm_defs.h"
#include "dm_state.h"
#include "dstrings.h"
#include "e_input.h"
#include "f_finale.h"
#include "f_interm.h"
#include "g_game.h"
#include "hu_draw.h"
#include "hu_stuff.h"
#include "l_ajbsp.h"
#include "m_argv.h"
#include "m_bbox.h"
#include "m_cheat.h"
#include "m_misc.h"
#include "m_menu.h"
#include "m_random.h"
#include "n_network.h"
#include "p_setup.h"
#include "p_spec.h"
#include "r_local.h"
#include "rad_trig.h"
#include "r_gldefs.h"
#include "r_wipe.h"
#include "s_sound.h"
#include "s_music.h"
#include "sv_chunk.h"
#include "sv_main.h"
#include "r_colormap.h"
#include "r_draw.h"
#include "r_modes.h"
#include "r_image.h"
#include "w_model.h"
#include "w_sprite.h"
#include "w_texture.h"
#include "w_wad.h"
#include "version.h"
#include "vm_coal.h"
#include "z_zone.h"

// Application active?
int app_state = APP_STATE_ACTIVE;

bool singletics = false;  // debug flag to cancel adaptiveness

// -ES- 2000/02/13 Takes screenshot every screenshot_rate tics.
// Must be used in conjunction with singletics.
static int screenshot_rate;

// For screenies...
bool m_screenshot_required = false;
bool need_save_screenshot  = false;

bool custom_MenuMain = false;
bool custom_MenuEpisode = false;
bool custom_MenuDifficulty = false;

FILE *logfile = NULL;
FILE *debugfile = NULL;

// Combination of unique lumps needed to best identify an IWAD
const std::array<std::array<const char*, 3>, 11> iwad_unique_lumps =
{
	{
		{ "BLASPHEMER", "BLASPHEM", "E1M1"  },
		{ "FREEDOOM1",  "FREEDOOM", "E1M1"  },
		{ "FREEDOOM2",  "FREEDOOM", "MAP01" },
		{ "HACX",       "HACX-R",   "MAP01" },
		{ "HARMONY",    "0HAWK01",  "MAP01" },
		{ "HERETIC",    "MUS_E1M1", "E1M1"  },
		{ "PLUTONIA",   "CAMO1",    "MAP01" },
		{ "TNT",        "REDTNT2",  "MAP01" },
		{ "DOOM",       "BFGGA0",   "E2M1"  },
		{ "DOOM1",      "SHOTA0",   "E1M1"  },
		{ "DOOM2",      "BFGGA0",   "MAP01" }
	}
};

gameflags_t default_gameflags =
{
	false,  // nomonsters
	false,  // fastparm

	false,  // respawn
	false,  // res_respawn
	false,  // item respawn

	false,  // true 3d gameplay
	MENU_GRAV_NORMAL, // gravity
	false,  // more blood

	true,   // jump
	true,   // crouch
	true,   // mlook
	AA_ON,  // autoaim
     
	true,   // cheats
	true,   // have_extra
	false,  // limit_zoom

	false,    // edge_compat
	true,     // kicking
	true,     // weapon_switch
	true,     // pass_missile
	false,    // team_damage
};

// -KM- 1998/12/16 These flags are the users prefs and are copied to
//   gameflags when a new level is started.
// -AJA- 2000/02/02: Removed initialisation (done in code using
//       `default_gameflags').

gameflags_t global_flags;

int newnmrespawn = 0;

bool swapstereo = false;
bool mus_pause_stop = false;
bool png_scrshots = false;
bool autoquickload = false;

std::string cfgfile;
std::string ewadfile;
std::string iwad_base;

std::string cache_dir;
std::string ddf_dir;
std::string game_dir;
std::string home_dir;
std::string save_dir;
std::string shot_dir;

extern cvar_c m_language;
extern cvar_c g_aggression;

cvar_c ddf_strict;
cvar_c ddf_lax;
cvar_c ddf_quiet;

static void E_TitleDrawer(void);

class startup_progress_c
{
private:
	std::vector<std::string> startup_messages;

public:
	startup_progress_c() { }

	~startup_progress_c() { }

	void addMessage(const char *message)
	{
		if (startup_messages.size() >= (SCREENHEIGHT / 2 / 10)) // These numbers are derived from the RGL_DrawProgress routine
			startup_messages.erase(startup_messages.begin());
		startup_messages.push_back(message);
	}

	void drawIt()
	{
		RGL_DrawProgress(startup_messages);
	}
};

static startup_progress_c s_progress;

void E_ProgressMessage(const char *message)
{
	s_progress.addMessage(message);
	s_progress.drawIt();
}

//
// -ACB- 1999/09/20 Created. Sets Global Stuff.
//
static void SetGlobalVars(void)
{
	int p;
	const char *s;

	// Screen Resolution Check...
	s = M_GetParm("-width");
	if (s)
		SCREENWIDTH = atoi(s);

	s = M_GetParm("-height");
	if (s)
		SCREENHEIGHT = atoi(s);

	p = M_CheckParm("-res");
	if (p && p + 2 < M_GetArgCount())
	{
		SCREENWIDTH  = atoi(M_GetArgument(p + 1));
		SCREENHEIGHT = atoi(M_GetArgument(p + 2));
	}

	// Bits per pixel check....
	s = M_GetParm("-bpp");
	if (s)
	{
		SCREENBITS = atoi(s);

		if (SCREENBITS <= 4) // backwards compat
			SCREENBITS *= 8;
	}

	// restrict depth to allowable values
	if (SCREENBITS < 15) SCREENBITS = 15;
	else if (SCREENBITS > 32) SCREENBITS = 32;

	M_CheckBooleanParm("windowed",   &FULLSCREEN, true);
	M_CheckBooleanParm("fullscreen", &FULLSCREEN, false);

	// sprite kludge (TrueBSP)
	p = M_CheckParm("-spritekludge");
	if (p)
	{
		if (p + 1 < M_GetArgCount())
			sprite_kludge = atoi(M_GetArgument(p + 1));

		if (!sprite_kludge)
			sprite_kludge = 1;
	}

	s = M_GetParm("-screenshot");
	if (s)
	{
		screenshot_rate = atoi(s);
		singletics = true;
	}

	// -AJA- 1999/10/18: Reworked these with M_CheckBooleanParm
	M_CheckBooleanParm("rotatemap", &rotatemap, false);
	M_CheckBooleanParm("sound", &nosound, true);
	M_CheckBooleanParm("music", &nomusic, true);
	M_CheckBooleanParm("itemrespawn", &global_flags.itemrespawn, false);
	M_CheckBooleanParm("mlook", &global_flags.mlook, false);
	M_CheckBooleanParm("monsters", &global_flags.nomonsters, true);
	M_CheckBooleanParm("fast", &global_flags.fastparm, false);
	M_CheckBooleanParm("extras", &global_flags.have_extra, false);
	M_CheckBooleanParm("kick", &global_flags.kicking, false);
	M_CheckBooleanParm("singletics", &singletics, false);
	M_CheckBooleanParm("true3d", &global_flags.true3dgameplay, false);
	M_CheckBooleanParm("blood", &global_flags.more_blood, false);
	M_CheckBooleanParm("cheats", &global_flags.cheats, false);
	M_CheckBooleanParm("jumping", &global_flags.jump, false);
	M_CheckBooleanParm("crouching", &global_flags.crouch, false);
	M_CheckBooleanParm("weaponswitch", &global_flags.weapon_switch, false);
	M_CheckBooleanParm("autoload", &autoquickload, false);

	if (M_CheckParm("-infight"))
		g_aggression = 1;

	if (M_CheckParm("-dlights"))
		use_dlights = 1;
	else if (M_CheckParm("-nodlights"))
		use_dlights = 0;

	if (M_CheckParm("-ecompat"))
		global_flags.edge_compat = true;

	if (!global_flags.respawn)
	{
		if (M_CheckParm("-newnmrespawn"))
		{
			global_flags.res_respawn = true;
			global_flags.respawn = true;
		}
		else if (M_CheckParm("-respawn"))
		{
			global_flags.respawn = true;
		}
	}

	// check for strict and no-warning options
	M_CheckBooleanCVar("strict", &ddf_strict, false);
	M_CheckBooleanCVar("lax",    &ddf_lax,    false);
	M_CheckBooleanCVar("warn",   &ddf_quiet,  true);

	strict_errors = ddf_strict.d ? true : false;
	lax_errors    = ddf_lax.d    ? true : false;
	no_warnings   = ddf_quiet.d  ? true : false;
}

//
// SetLanguage
//
void SetLanguage(void)
{
	const char *want_lang = M_GetParm("-lang");
	if (! want_lang)
		want_lang = M_GetParm("-language");

	if (want_lang)
		m_language = want_lang;

	if (language.Select(m_language.str))
		return;

	I_Warning("Invalid language: '%s'\n", m_language.str);

	if (! language.IsValid())
		if (! language.Select(0))
			I_Error("Unable to select any language!");

	m_language = language.GetName();
}

//
// SpecialWadVerify
//
static void SpecialWadVerify(void)
{
	int lump = W_CheckNumForName("EDGEVER");
	if (lump < 0)
		I_Error("EDGEVER lump not found. Get EDGE-DEFS.WAD at https://github.com/dashodanger/EDGE-classic");

	const void *data = W_CacheLumpNum(lump);

	// parse version number
	const char *s = (const char*)data;
	int wad_ver = atoi(s) * 100;

	while (isdigit(*s)) s++;
	s++;
	wad_ver += atoi(s);

	W_DoneWithLump(data);

	I_Printf("EDGE-DEFS.WAD version %1.2f found.\n", wad_ver / 100.0);

	if (wad_ver < EDGE_WAD_VERSION)
	{
		I_Error("EDGE-DEFS.WAD is an older version (expected %1.2f)\n",
		          EDGE_WAD_VERSION / 100.0);
	}
	else if (wad_ver > EDGE_WAD_VERSION)
	{
		I_Warning("EDGE-DEFS.WAD is a newer version (expected %1.2f)\n",
		          EDGE_WAD_VERSION / 100.0);
	}
}

//
// ShowNotice
//
static void ShowNotice(void)
{
	CON_MessageColor(RGB_MAKE(64,192,255));

	I_Printf("%s", language["Notice"]);
}


static void DoSystemStartup(void)
{
	// startup the system now
	W_InitImages();

	I_Debugf("- System startup begun.\n");

	I_SystemStartup();

	// -ES- 1998/09/11 Use R_ChangeResolution to enter gfx mode

	R_DumpResList();

	// -KM- 1998/09/27 Change res now, so music doesn't start before
	// screen.  Reset clock too.
	I_Debugf("- Changing Resolution...\n");

	R_InitialResolution();

	RGL_Init();
	R_SoftInitResolution();

	I_Debugf("- System startup done.\n");
}


static void M_DisplayPause(void)
{
	static const image_c *pause_image = NULL;

	if (! pause_image)
		pause_image = W_ImageLookup("M_PAUSE");

	// make sure image is centered horizontally

	float w = IM_WIDTH(pause_image);
	float h = IM_HEIGHT(pause_image);

	float x = 160 - w / 2;
	float y = 10;

	HUD_StretchImage(x, y, w, h, pause_image);
}


wipetype_e wipe_method = WIPE_Melt;
int wipe_reverse = 0;

static bool need_wipe = false;

void E_ForceWipe(void)
{
	if (gamestate == GS_NOTHING)
		return;

	if (wipe_method == WIPE_None)
		return;

	need_wipe = true;

	// capture screen now (before new level is loaded etc..)
	E_Display();
}

//
// E_Display
//
// Draw current display, possibly wiping it from the previous
//
// -ACB- 1998/07/27 Removed doublebufferflag check (unneeded).  

static bool wipe_gl_active = false;

void E_Display(void)
{
	if (nodrawers)
		return;  // for comparative timing / profiling

	// Start the frame - should we need to.
	I_StartFrame();

	HUD_FrameSetup();

	// -AJA- 1999/08/02: Make sure palette/gamma is OK. This also should
	//       fix (finally !) the "gamma too late on walls" bug.
	V_ColourNewFrame();

	switch (gamestate)
	{
		case GS_LEVEL:
			HU_Erase();

			R_PaletteStuff();

			VM_RunHud();

			if (need_save_screenshot)
			{
				M_MakeSaveScreenShot();
				need_save_screenshot = false;
			}

			HU_Drawer();
			RAD_Drawer();
			break;

		case GS_INTERMISSION:
			WI_Drawer();
			break;

		case GS_FINALE:
			F_Drawer();
			break;

		case GS_TITLESCREEN:
			E_TitleDrawer();
			break;

		case GS_NOTHING:
			break;
	}

	if (wipe_gl_active)
	{
		// -AJA- Wipe code for GL.  Sorry for all this ugliness, but it just
		//       didn't fit into the existing wipe framework.
		//
		if (RGL_DoWipe())
		{
			RGL_StopWipe();
			wipe_gl_active = false;
		}
	}

	// save the current screen if about to wipe
	if (need_wipe)
	{
		need_wipe = false;
		wipe_gl_active = true;

		RGL_InitWipe(wipe_reverse, wipe_method);
	}

	if (paused)
		M_DisplayPause();

	// menus go directly to the screen
	M_Drawer();  // menu is drawn even on top of everything (except console)

	N_NetUpdate();  // send out any new accumulation

	CON_Drawer();

	if (m_screenshot_required)
	{
		m_screenshot_required = false;
		M_ScreenShot(true);
	}
	else if (screenshot_rate && (gamestate >= GS_LEVEL))
	{
		SYS_ASSERT(singletics);

		if (leveltime % screenshot_rate == 0)
			M_ScreenShot(false);
	}

	I_FinishFrame();  // page flip or blit buffer
}


//
//  TITLE LOOP
//
static int title_game;
static int title_pic;
static int title_countdown;

static const image_c *title_image = NULL;


static void E_TitleDrawer(void)
{
	if (title_image)
	{
		HUD_DrawImageTitleWS(title_image); //Lobo: Widescreen titlescreen support
	}	
	else
	{
		HUD_SolidBox(0, 0, 320, 200, RGB_MAKE(64,64,64));
	}
}


//
// This cycles through the title sequences.
// -KM- 1998/12/16 Fixed for DDF.
//
void E_AdvanceTitle(void)
{
	title_pic++;

	// prevent an infinite loop
	for (int loop=0; loop < 100; loop++)
	{
		gamedef_c *g = gamedefs[title_game];
		SYS_ASSERT(g);

		if (title_pic >= g->titlepics.GetSize())
		{
			title_game = (title_game + 1) % gamedefs.GetSize();
			title_pic  = 0;
			continue;
		}

		// ignore non-existing episodes.  Doesn't include title-only ones
		// like [EDGE].
		if (title_pic == 0 && g->firstmap && g->firstmap[0] &&
			W_CheckNumForName(g->firstmap) == -1)
		{
			title_game = (title_game + 1) % gamedefs.GetSize();
			title_pic  = 0;
			continue;
		}

		// ignore non-existing images
		title_image = W_ImageLookup(g->titlepics[title_pic], INS_Graphic, ILF_Null);

		if (! title_image)
		{
			title_pic++;
			continue;
		}

		// found one !!

		if (title_pic == 0 && g->titlemusic > 0)
			S_ChangeMusic(g->titlemusic, false);

		title_countdown = g->titletics;
		return;
	}

	// not found

	title_image = NULL;
	title_countdown = TICRATE;
}

void E_StartTitle(void)
{
	gameaction = ga_nothing;
	gamestate  = GS_TITLESCREEN;

	paused = false;

	// force pic overflow -> first available titlepic
	title_game = gamedefs.GetSize() - 1;
	title_pic = 29999;
	title_countdown = 1;
 
	E_AdvanceTitle();
}


void E_TitleTicker(void)
{
	if (title_countdown > 0)
	{
		title_countdown--;

		if (title_countdown == 0)
			E_AdvanceTitle();
	}
}


//
// Detects which directories to search for DDFs, WADs and other files in.
//
// -ES- 2000/01/01 Written.
//
void InitDirectories(void)
{
    std::string path;

	const char *s = M_GetParm("-home");
    if (s)
        home_dir = s;

	// Get the Home Directory from environment if set
    if (home_dir.empty())
    {
        s = getenv("HOME");
        if (s)
        {
            home_dir = epi::PATH_Join(s, EDGEHOMESUBDIR); 

			if (! epi::FS_IsDir(home_dir.c_str()))
			{
                epi::FS_MakeDir(home_dir.c_str());

                // Check whether the directory was created
                if (! epi::FS_IsDir(home_dir.c_str()))
                    home_dir.clear();
			}
        }
    }

    if (home_dir.empty()) home_dir = "."; // Default to current directory

	// Get the Game Directory from parameter.
	s = epi::GetResourcePath();
	game_dir = s;
	free((void*)s);

	s = M_GetParm("-game");
	if (s)
		game_dir = s;

	// add parameter file "gamedir/parms" if it exists.
	std::string parms = epi::PATH_Join(game_dir.c_str(), "parms");

	if (epi::FS_Access(parms.c_str(), epi::file_c::ACCESS_READ))
	{
		// Insert it right after the game parameter
		M_ApplyResponseFile(parms.c_str(), M_CheckParm("-game") + 2);
	}

	s = M_GetParm("-ddf");
	if (s)
	{
		ddf_dir = std::string(s);
		DDF_SetWhere(ddf_dir);
	} 
	{
		ddf_dir = "";
	}

	// config file
	s = M_GetParm("-config");
	if (s)
	{
		cfgfile = M_ComposeFileName(home_dir.c_str(), s);
	}
	else
    {
        cfgfile = epi::PATH_Join(home_dir.c_str(), EDGECONFIGFILE);
	}

	// edge.wad file
	s = M_GetParm("-ewad");
	if (s)
	{
		ewadfile = M_ComposeFileName(home_dir.c_str(), s);
	}
	else
    {
        ewadfile = epi::PATH_Join(home_dir.c_str(), "edge-defs.wad");
	}

	// cache directory
    cache_dir = epi::PATH_Join(home_dir.c_str(), CACHEDIR);

    if (! epi::FS_IsDir(cache_dir.c_str()))
        epi::FS_MakeDir(cache_dir.c_str());

	// savegame directory
    save_dir = epi::PATH_Join(home_dir.c_str(), SAVEGAMEDIR);
	
    if (! epi::FS_IsDir(save_dir.c_str())) epi::FS_MakeDir(save_dir.c_str());

	SV_ClearSlot("current");

	// screenshot directory
    shot_dir = epi::PATH_Join(home_dir.c_str(), SCRNSHOTDIR);

    if (!epi::FS_IsDir(shot_dir.c_str()))
        epi::FS_MakeDir(shot_dir.c_str());
}


//
// Adds an IWAD and EDGE.WAD
// First checks agains known Adler-32 checksums, then attempts to find unique lumps as a fallback
//

static void IdentifyVersion(void)
{
	I_Debugf("- Identify Version\n");

	// Check -iwad parameter, find out if it is the IWADs directory
    std::string iwad_par;
    std::string iwad_file;
    std::string iwad_dir;
	std::vector<std::string> iwad_dir_vector;

	const char *s = M_GetParm("-iwad");

    iwad_par = std::string(s ? s : "");

    if (! iwad_par.empty())
    {
        if (epi::FS_IsDir(iwad_par.c_str()))
        {
            iwad_dir = iwad_par;
            iwad_par.clear(); // Discard 
        }
    }   

    // If we haven't yet set the IWAD directory, then we check
    // the DOOMWADDIR environment variable
    if (iwad_dir.empty())
    {
        s = getenv("DOOMWADDIR");

        if (s && epi::FS_IsDir(s))
            iwad_dir = std::string(s);
    }

    // Should the IWAD directory not be set by now, then we
    // use our standby option of the current directory.
    if (iwad_dir.empty())
        iwad_dir = ".";

	// Add DOOMWADPATH directories if they exist
	s = getenv("DOOMWADPATH");
	if (s)
	{
		std::string dir_test = s;
    	std::string::size_type oldpos = 0;
    	std::string::size_type pos = 0;
    	while (pos != std::string::npos) {
#ifdef WIN32
        	pos = dir_test.find(';', oldpos);
#else
			pos = dir_test.find(':', oldpos);
#endif
        	std::string dir_string = dir_test.substr(oldpos, (pos == std::string::npos ? dir_test.size() : pos) - oldpos);
        	if (!dir_string.empty() && epi::FS_IsDir(dir_string.c_str()))
            	iwad_dir_vector.push_back(dir_string);
        	if (pos != std::string::npos)
            	oldpos = pos + 1;
    	}
	}

    // Should the IWAD Parameter not be empty then it means
    // that one was given which is not a directory. Therefore
    // we assume it to be a name
    if (!iwad_par.empty())
    {
        std::string fn = iwad_par;
        
        // Is it missing the extension?
        std::string ext = epi::PATH_GetExtension(iwad_par.c_str());
        if (ext.empty())
        {
            fn += ("." EDGEWADEXT);
        }

        // If no directory given use the IWAD directory
        std::string dir = epi::PATH_GetDir(fn.c_str());
        if (dir.empty())
            iwad_file = epi::PATH_Join(iwad_dir.c_str(), fn.c_str()); 
        else
            iwad_file = fn;

        if (!epi::FS_Access(iwad_file.c_str(), epi::file_c::ACCESS_READ))
        {
			// Check DOOMWADPATH directories if present
			if (!iwad_dir_vector.empty())
			{
				for (int i=0; i < iwad_dir_vector.size(); i++)
				{
					iwad_file = epi::PATH_Join(iwad_dir_vector[i].c_str(), fn.c_str());
					if (epi::FS_Access(iwad_file.c_str(), epi::file_c::ACCESS_READ))
						goto foundindoomwadpath;
				}
				I_Error("IdentifyVersion: Unable to access specified '%s'", fn.c_str());
			}
			else
				I_Error("IdentifyVersion: Unable to access specified '%s'", fn.c_str());
        }

		foundindoomwadpath:

		epi::file_c *iwad_test = epi::FS_Open(iwad_file.c_str(), epi::file_c::ACCESS_READ | epi::file_c::ACCESS_BINARY);
		bool unique_lump_match = false;
		for (int i=0; i < iwad_unique_lumps.size(); i++) {
			if (W_CheckForUniqueLumps(iwad_test, iwad_unique_lumps[i][1], iwad_unique_lumps[i][2]))
			{
				unique_lump_match = true;
				iwad_base = iwad_unique_lumps[i][0];
				break;
			}
    	}
		delete iwad_test;
		if (unique_lump_match)
		{
			W_AddRawFilename(iwad_file.c_str(), FLKIND_IWad);
		}
		else
			I_Error("IdentifyVersion: Could not identify '%s' as a valid IWAD!\n", fn.c_str());
    }
    else
    {
        const char *location;
		
        int max = 1;

        if (stricmp(iwad_dir.c_str(), game_dir.c_str()) != 0) 
        {
            // IWAD directory & game directory differ 
            // therefore do a second loop which will
            // mean we check both.
            max++;
        } 

		for (int i = 0; i < max; i++)
		{
			location = (i == 0 ? iwad_dir.c_str() : game_dir.c_str());

			//
			// go through the available *.wad files, attempting IWAD
			// detection for each, adding the file if they exist.
			//
			// -ACB- 2000/06/08 Quit after we found a file - don't load
			//                  more than one IWAD
			//
			epi::filesystem_dir_c fsd;

			if (!FS_ReadDir(&fsd, location, "*.wad"))
			{
				I_Warning("IdenfityVersion: Failed to read '%s' directory!\n", location);
			}
			else
			{
				for (int i = 0; i < fsd.GetSize(); i++) 
				{
					if(!fsd[i]->is_dir)
					{
						epi::file_c *iwad_test = epi::FS_Open(fsd[i]->name.c_str(), epi::file_c::ACCESS_READ | epi::file_c::ACCESS_BINARY);
						bool unique_lump_match = false;
						for (int i = 0; i < iwad_unique_lumps.size(); i++) {
							if (W_CheckForUniqueLumps(iwad_test, iwad_unique_lumps[i][1], iwad_unique_lumps[i][2]))
							{
								unique_lump_match = true;
								iwad_base = iwad_unique_lumps[i][0];
								break;
							}
						}
						if (unique_lump_match)
						{
							W_AddRawFilename(fsd[i]->name.c_str(), FLKIND_IWad);
							delete iwad_test;
							goto foundlooseiwad;
						}					
					}
				}
			}
		}

		// Separate check for DOOMWADPATH stuff if it exists - didn't want to mess with the existing stuff above

		if (!iwad_dir_vector.empty())
		{
			for (int i=0; i < iwad_dir_vector.size(); i++)
			{
				location = iwad_dir_vector[i].c_str();

				epi::filesystem_dir_c fsd;

				if (!FS_ReadDir(&fsd, location, "*.wad"))
				{
					I_Warning("IdenfityVersion: Failed to read '%s' directory!\n", location);
				}
				else
				{
					for (int i = 0; i < fsd.GetSize(); i++) 
					{
						if(!fsd[i]->is_dir)
						{
							epi::file_c *iwad_test = epi::FS_Open(fsd[i]->name.c_str(), epi::file_c::ACCESS_READ | epi::file_c::ACCESS_BINARY);
							bool unique_lump_match = false;
							for (int i = 0; i < iwad_unique_lumps.size(); i++) {
								if (W_CheckForUniqueLumps(iwad_test, iwad_unique_lumps[i][1], iwad_unique_lumps[i][2]))
								{
									unique_lump_match = true;
									iwad_base = iwad_unique_lumps[i][0];
									break;
								}
							}
							if (unique_lump_match)
							{
								W_AddRawFilename(fsd[i]->name.c_str(), FLKIND_IWad);
								delete iwad_test;
								goto foundlooseiwad;
							}					
						}
					}
				}
			}
		}

		// Should only get here if no loose IWADs were found - Dasho
		I_Error("IdentifyVersion: No IWADs found!\n");
    }

	foundlooseiwad:

	I_Debugf("IWAD BASE = [%s]\n", iwad_base.c_str());

    // Look for the required wad in the IWADs dir and then the gamedir
    std::string reqwad(epi::PATH_Join(iwad_dir.c_str(), REQUIREDWAD "." EDGEWADEXT));

    if (! epi::FS_Access(reqwad.c_str(), epi::file_c::ACCESS_READ))
    {
        reqwad = epi::PATH_Join(game_dir.c_str(), REQUIREDWAD "." EDGEWADEXT);

        if (! epi::FS_Access(reqwad.c_str(), epi::file_c::ACCESS_READ))
        {
            I_Error("IdentifyVersion: Could not find required %s.%s!\n", 
                    REQUIREDWAD, EDGEWADEXT);
        }
    }

    W_AddRawFilename(reqwad.c_str(), FLKIND_EWad);
}

// Add game-specific base EWADs (widepix, skyboxes, etc) - Dasho
static void Add_Base(void) 
{
	std::string base_path = epi::PATH_Join(game_dir.c_str(), "edge_base");
	std::string base_wad = iwad_base;
	std::transform(base_wad.begin(), base_wad.end(), base_wad.begin(), ::tolower);
	base_path = epi::PATH_Join(base_path.c_str(), base_wad.append("_base.wad").c_str());
	if (epi::FS_Access(base_path.c_str(), epi::file_c::ACCESS_READ)) 
		W_AddRawFilename(base_path.c_str(), FLKIND_EWad);
	else
		I_Warning("Base WAD not found for the %s IWAD! Check the /edge_base folder of your EDGE-Classic install!\n", iwad_base.c_str());
}

static void CheckTurbo(void)
{
	int turbo_scale = 100;

	int p = M_CheckParm("-turbo");

	if (p)
	{
		if (p + 1 < M_GetArgCount())
			turbo_scale = atoi(M_GetArgument(p + 1));
		else
			turbo_scale = 200;

		if (turbo_scale < 10)  turbo_scale = 10;
		if (turbo_scale > 400) turbo_scale = 400;

		CON_MessageLDF("TurboScale", turbo_scale);
	}

	E_SetTurboScale(turbo_scale);
}


static void ShowDateAndVersion(void)
{
	time_t cur_time;
	char timebuf[100];

	time(&cur_time);
	strftime(timebuf, 99, "%I:%M %p on %d/%b/%Y", localtime(&cur_time));

	I_Debugf("[Log file created at %s]\n\n", timebuf);
	I_Debugf("[Debug file created at %s]\n\n", timebuf);

	// 23-6-98 KM Changed to hex to allow versions such as 0.65a etc
	I_Printf("EDGE-Classic v" EDGEVERSTR " compiled on " __DATE__ " at " __TIME__ "\n");
	I_Printf("EDGE-Classic homepage is at https://github.com/dashodanger/EDGE-classic/\n");
	I_Printf("EDGE-Classic is based on DOOM by id Software http://www.idsoftware.com/\n");

#ifdef WIN32
	I_Printf("Executable path: '%s'\n", win32_exe_path);
#endif

	M_DebugDumpArgs();
}

static void SetupLogAndDebugFiles(void)
{
	// -AJA- 2003/11/08 The log file gets all CON_Printfs, I_Printfs,
	//                  I_Warnings and I_Errors.

	std::string log_fn  (epi::PATH_Join(home_dir.c_str(), EDGELOGFILE));
	std::string debug_fn(epi::PATH_Join(home_dir.c_str(), "debug.txt"));

	logfile = NULL;
	debugfile = NULL;

	if (! M_CheckParm("-nolog"))
	{

		logfile = fopen(log_fn.c_str(), "w");

		if (!logfile)
			I_Error("[E_Startup] Unable to create log file\n");
	}

	//
	// -ACB- 1998/09/06 Only used for debugging.
	//                  Moved here to setup debug file for DDF Parsing...
	//
	// -ES- 1999/08/01 Debugfiles can now be used without -DDEVELOPERS, and
	//                 then logs all the CON_Printfs, I_Printfs and I_Errors.
	//
	// -ACB- 1999/10/02 Don't print to console, since we don't have a console yet.

	/// int p = M_CheckParm("-debug");
	if (true)
	{
		debugfile = fopen(debug_fn.c_str(), "w");

		if (!debugfile)
			I_Error("[E_Startup] Unable to create debugfile");
	}
}

static void AddSingleCmdLineFile(const char *name)
{
    std::string ext = epi::PATH_GetExtension(name);
	int kind = FLKIND_Lump;

	if (stricmp(ext.c_str(), ".edm") == 0)
		I_Error("Demos are no longer supported\n");

	// no need to check for GWA (shouldn't be added manually)

	if (stricmp(ext.c_str(), ".wad") == 0)
		kind = FLKIND_PWad;
	else if (stricmp(ext.c_str(), ".hwa") == 0)
		kind = FLKIND_HWad;
	else if (stricmp(ext.c_str(), ".rts") == 0)
		kind = FLKIND_RTS;
	else if (stricmp(ext.c_str(), ".ddf") == 0 ||
			 stricmp(ext.c_str(), ".ldf") == 0)
		kind = FLKIND_DDF;
	else if (stricmp(ext.c_str(), ".deh") == 0 ||
			 stricmp(ext.c_str(), ".bex") == 0)
		kind = FLKIND_Deh;

	if (kind != FLKIND_Lump)
	{
		std::string fn = M_ComposeFileName(game_dir.c_str(), name);
		W_AddRawFilename(fn.c_str(), kind);
	}
}

static void AddCommandLineFiles(void)
{
	// first handle "loose" files (arguments before the first option)

	int p;
	const char *ps;

	for (p = 1; p < M_GetArgCount() && '-' != (ps = M_GetArgument(p))[0]; p++)
	{
		AddSingleCmdLineFile(ps);
	}

	// next handle the -file option (we allow multiple uses)

	p = M_CheckNextParm("-file", 0);
	
	while (p)
	{
		// the parms after p are wadfile/lump names,
		// go until end of parms or another '-' preceded parm

		for (p++; p < M_GetArgCount() && '-' != (ps = M_GetArgument(p))[0]; p++)
		{
			AddSingleCmdLineFile(ps);
		}

		p = M_CheckNextParm("-file", p-1);
	}

	// scripts....

	p = M_CheckNextParm("-script", 0);
	
	while (p)
	{
		// the parms after p are script filenames,
		// go until end of parms or another '-' preceded parm

		for (p++; p < M_GetArgCount() && '-' != (ps = M_GetArgument(p))[0]; p++)
		{
			std::string ext = epi::PATH_GetExtension(ps);

			// sanity check...
			if (stricmp(ext.c_str(), ".wad") == 0 || 
                stricmp(ext.c_str(), ".gwa") == 0 ||
			    stricmp(ext.c_str(), ".hwa") == 0 ||
                stricmp(ext.c_str(), ".ddf") == 0 ||
			    stricmp(ext.c_str(), ".deh") == 0 ||
			    stricmp(ext.c_str(), ".bex") == 0)
			{
				I_Error("Illegal filename for -script: %s\n", ps);
			}

			std::string fn = M_ComposeFileName(game_dir.c_str(), ps);

			W_AddRawFilename(fn.c_str(), FLKIND_RTS);
		}

		p = M_CheckNextParm("-script", p-1);
	}


	// finally handle the -deh option(s)

	p = M_CheckNextParm("-deh", 0);
	
	while (p)
	{
		// the parms after p are Dehacked/BEX filenames,
		// go until end of parms or another '-' preceded parm

		for (p++; p < M_GetArgCount() && '-' != (ps = M_GetArgument(p))[0]; p++)
		{
			std::string ext(epi::PATH_GetExtension(ps));

			// sanity check...
			if (stricmp(ext.c_str(), ".wad") == 0 || 
                stricmp(ext.c_str(), ".gwa") == 0 ||
			    stricmp(ext.c_str(), ".hwa") == 0 ||
                stricmp(ext.c_str(), ".ddf") == 0 ||
			    stricmp(ext.c_str(), ".rts") == 0)
			{
				I_Error("Illegal filename for -deh: %s\n", ps);
			}

			std::string fn = M_ComposeFileName(game_dir.c_str(), ps);

			epi::file_c *fn_file = epi::FS_Open(fn.c_str(), epi::file_c::ACCESS_READ | epi::file_c::ACCESS_BINARY);

			if (fn_file)
			{
				W_AddRawFilename(fn.c_str(), FLKIND_Deh);
				delete fn_file;
			}
		}

		p = M_CheckNextParm("-deh", p-1);
	}
}

static void Add_Autoload(void) {
	
	epi::filesystem_dir_c fsd;
	std::string folder = "autoload";

	if (!FS_ReadDir(&fsd, folder.c_str(), "*.*"))
	{
		I_Warning("Failed to read autoload directory!\n");
	}
	else
	{
		for (int i = 0; i < fsd.GetSize(); i++) 
		{
			if(!fsd[i]->is_dir)
			{
				AddSingleCmdLineFile(epi::PATH_Join(folder.c_str(), fsd[i]->name.c_str()).c_str());
			}
		}
	}
	fsd.Clear();
	std::string lowercase_base = iwad_base;
	std::transform(lowercase_base.begin(), lowercase_base.end(), lowercase_base.begin(), ::tolower);
	folder = epi::PATH_Join(folder.c_str(), lowercase_base.c_str());
	if (!FS_ReadDir(&fsd, folder.c_str(), "*.*"))
	{
		I_Warning("Failed to read game-specific autoload directory!\n");
	}
	else
	{
		for (int i = 0; i < fsd.GetSize(); i++) 
		{
			if(!fsd[i]->is_dir)
			{
				AddSingleCmdLineFile(epi::PATH_Join(folder.c_str(), fsd[i]->name.c_str()).c_str());
			}
		}		
	}
}

static void InitDDF(void)
{
	I_Debugf("- Initialising DDF\n");

	DDF_Init(EDGEVER);
}


void E_EngineShutdown(void)
{
	N_QuitNetGame();

	S_StopMusic();

	// Pause to allow sounds to finish
	for (int loop=0; loop < 30; loop++)
	{
		S_SoundTicker(); 
		I_Sleep(50);
	}

    S_Shutdown();
}

void (*startcode[])() =
{
	InitDDF,
	IdentifyVersion,
	Add_Base,
	Add_Autoload,
	AddCommandLineFiles,
	CheckTurbo,
	RAD_Init,
	W_InitMultipleFiles,
	V_InitPalette,
	HU_Init,
	W_ReadDDF,
	W_CheckWADFixes,
	W_InitFlats,
	W_InitTextures,
	CON_Start,
	SpecialWadVerify,
	M_InitMiscConVars,
	DDF_CleanUp,
	SetLanguage,
	ShowNotice,
	SV_MainInit,
	S_PrecacheSounds,
	W_ImageCreateUser,
	W_InitSprites,
	W_ProcessTX_HI,
	W_InitModels,
	M_Init,
	R_Init,
	P_Init,
	P_MapInit,
	P_InitSwitchList,
	W_InitPicAnims,
	S_Init,
	N_InitNetwork,
	M_CheatInit,
	VM_InitCoal,
	VM_LoadScripts,
	NULL
};

extern void WLF_InitMaps(void); //!!!

// Local Prototypes
static void E_Startup();
static void E_Shutdown(void);


static void E_Startup(void)
{
	int p;

	// Version check ?
	if (M_CheckParm("-version"))
	{
		// -AJA- using I_Error here, since I_Printf crashes this early on
		I_Error("\nEDGE version is " EDGEVERSTR "\n");
	}

	// -AJA- 2000/02/02: initialise global gameflags to defaults
	global_flags = default_gameflags;

	InitDirectories();

	SetupLogAndDebugFiles();

	CON_InitConsole();
	CON_ResetAllVars();

	ShowDateAndVersion();

	M_LoadDefaults();

	CON_HandleProgramArgs();
	SetGlobalVars();

	DoSystemStartup();

	// Cycle through all the startup functions
	for (p=0; startcode[p] != NULL; p++)
	{
		startcode[p]();
	}
}


static void E_Shutdown(void)
{
	/* TODO: E_Shutdown */
}


static void E_InitialState(void)
{
	I_Debugf("- Setting up Initial State...\n");

	const char *ps;

	// do loadgames first, as they contain all of the
	// necessary state already (in the savegame).

	if (M_CheckParm("-playdemo") || M_CheckParm("-timedemo") ||
	    M_CheckParm("-record"))
	{
		I_Error("Demos are no longer supported\n");
	}

	ps = M_GetParm("-loadgame");
	if (ps)
	{
		G_DeferredLoadGame(atoi(ps));
		return;
	}

	bool warp = false;

	// get skill / episode / map from parms
	std::string warp_map;
	skill_t     warp_skill = sk_medium;
	int         warp_deathmatch = 0;

	int bots = 0;

	ps = M_GetParm("-bots");
	if (ps)
		bots = atoi(ps);

	ps = M_GetParm("-warp");
	if (ps)
	{
		warp = true;
		warp_map = std::string(ps);
	}

	// -KM- 1999/01/29 Use correct skill: 1 is easiest, not 0
	ps = M_GetParm("-skill");
	if (ps)
	{
		warp = true;
		warp_skill = (skill_t)(atoi(ps) - 1);
	}

	// deathmatch check...
	int pp = M_CheckParm("-deathmatch");
	if (pp)
	{
		warp_deathmatch = 1;

		if (pp + 1 < M_GetArgCount())
			warp_deathmatch = MAX(1, atoi(M_GetArgument(pp + 1)));
	}
	else if (M_CheckParm("-altdeath") > 0)
	{
		warp_deathmatch = 2;
	}


	if (M_GetParm("-record"))
		warp = true;

	// start the appropriate game based on parms
	if (! warp)
	{
		I_Debugf("- Startup: showing title screen.\n");
		E_StartTitle();
		return;
	}

	newgame_params_c params;

	params.skill = warp_skill;	
	params.deathmatch = warp_deathmatch;	

	if (warp_map.length() > 0)
		params.map = G_LookupMap(warp_map.c_str());
	else
		params.map = G_LookupMap("1");

	if (! params.map)
		I_Error("-warp: no such level '%s'\n", warp_map.c_str());

	SYS_ASSERT(G_MapExists(params.map));
	SYS_ASSERT(params.map->episode);

	params.random_seed = I_PureRandom();

	params.SinglePlayer(bots);

	G_DeferredNewGame(params);
}


//
// ---- MAIN ----
//
// -ACB- 1998/08/10 Removed all reference to a gamemap, episode and mission
//                  Used LanguageLookup() for lang specifics.
//
// -ACB- 1998/09/06 Removed all the unused code that no longer has
//                  relevance.    
//
// -ACB- 1999/09/04 Removed statcopy parm check - UNUSED
//
// -ACB- 2004/05/31 Moved into a namespace, the c++ revolution begins....
//
void E_Main(int argc, const char **argv)
{
	// Seed M_Random RNG
	M_Random_Init();

	// Start memory allocation system at the very start (SCHEDULED FOR REMOVAL)
	Z_Init();

	// Implemented here - since we need to bring the memory manager up first
	// -ACB- 2004/05/31
	M_InitArguments(argc, argv);

	try
	{
		E_Startup();

		E_InitialState();

		CON_MessageColor(RGB_MAKE(255,255,0));
		I_Printf("EDGE-Classic v" EDGEVERSTR " initialisation complete.\n");

		I_Debugf("- Entering game loop...\n");

		while (! (app_state & APP_STATE_PENDING_QUIT))
		{
			// We always do this once here, although the engine may
			// makes in own calls to keep on top of the event processing
			I_ControlGetEvents(); 

			if (app_state & APP_STATE_ACTIVE)
				E_Tick();
		}
	}
	catch(const std::exception& e)
	{
		I_Printf("EXCEPTION THROWN: %s\n", e.what());
		I_Error("Unexpected internal failure occurred!\n");
	}

	E_Shutdown();    // Shutdown whatever at this point
}


//
// Called when this application has lost focus (i.e. an ALT+TAB event)
//
void E_Idle(void)
{
	E_ReleaseAllKeys();
}


//
// This Function is called for a single loop in the system.
//
// -ACB- 1999/09/24 Written
// -ACB- 2004/05/31 Namespace'd
//
void E_Tick(void)
{
	// -ES- 1998/09/11 It's a good idea to frequently check the heap
#ifdef DEVELOPERS
	//Z_CheckHeap();
#endif

	G_BigStuff();

	// Update display, next frame, with current state.
	E_Display();

	bool fresh_game_tic;

	// this also runs the responder chain via E_ProcessEvents
	int counts = N_TryRunTics(&fresh_game_tic);

	SYS_ASSERT(counts > 0);

	for (; counts > 0; counts--)  // run the tics
	{
		CON_Ticker();
		M_Ticker();

		if (fresh_game_tic)
			G_Ticker();

		S_SoundTicker(); 
		S_MusicTicker(); // -ACB- 1999/11/13 Improved music update routines

		N_NetUpdate();  // check for new console commands
	}
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
