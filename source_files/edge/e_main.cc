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
#include "i_sdlinc.h"
#include "e_main.h"
#include "i_defs_gl.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <algorithm>
#include <array>
#include <vector>
#include <chrono> // PurgeCache

#include "file.h"
#include "filesystem.h"
#include "path.h"
#include "str_util.h"

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
#include "w_files.h"
#include "w_model.h"
#include "w_sprite.h"
#include "w_texture.h"
#include "w_wad.h"
#include "version.h"
#include "vm_coal.h"

extern cvar_c r_doubleframes;

extern cvar_c v_gamma;

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

typedef struct
{
	// used to determine IWAD priority if no -iwad parameter provided 
	// and multiple IWADs are detected in various paths
	u8_t score;

	// iwad_base to set if this IWAD is used
	const char *base;

	// (usually) unique lumps to check for in a potential IWAD
	std::array<const char*, 2> unique_lumps;
}
iwad_check_t;

// Combination of unique lumps needed to best identify an IWAD
const std::array<iwad_check_t, 13> iwad_checker =
{
	{
		{ 13,  "CUSTOM",    {"EDGEIWAD", "EDGEIWAD"} },
		{ 1,  "BLASPHEMER", {"BLASPHEM", "E1M1"}     },
		{ 7,  "FREEDOOM1",  {"FREEDOOM", "E1M1"}     },
		{ 11, "FREEDOOM2",  {"FREEDOOM", "MAP01"}    },
		{ 5,  "REKKR",      {"REKCREDS", "E1M1"}     },
		{ 4,  "HACX",       {"HACX-R",   "MAP01"}    },
		{ 3,  "HARMONY",    {"0HAWK01",  "MAP01"}    },
		{ 2,  "HERETIC",    {"MUS_E1M1", "E1M1"}     },
		{ 9, "PLUTONIA",    {"CAMO1",    "MAP01"}    },
		{ 10, "TNT",        {"REDTNT2",  "MAP01"}    },
		{ 8,  "DOOM",       {"BFGGA0",   "E2M1"}     },
		{ 6,  "DOOM1",      {"SHOTA0",   "E1M1"}     },
		{ 12, "DOOM2",      {"BFGGA0",   "MAP01"}    },
		//{ 0, "STRIFE",    {"VELLOGO",  "RGELOGO"}  }// Dev/internal use - Definitely nowhwere near playable
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

std::filesystem::path brandingfile;
std::filesystem::path cfgfile;
std::filesystem::path epkfile;
std::string iwad_base;

std::filesystem::path cache_dir;
std::filesystem::path game_dir;
std::filesystem::path home_dir;
std::filesystem::path save_dir;
std::filesystem::path shot_dir;

// not using DEF_CVAR here since var name != cvar name
cvar_c m_language("language", "ENGLISH", CVAR_ARCHIVE);

DEF_CVAR(logfilename, "edge-classic.log", 0)
DEF_CVAR(configfilename, "edge-classic.cfg", 0)
DEF_CVAR(debugfilename, "debug.txt", 0)
DEF_CVAR(windowtitle, "EDGE-Classic", 0)
DEF_CVAR(versionstring, "1.33", 0)
DEF_CVAR(orgname, "EDGE Team", 0)
DEF_CVAR(appname, "EDGE-Classic", 0)
DEF_CVAR(homepage, "https://edge-classic.github.io", 0)

DEF_CVAR(r_overlay, "0", CVAR_ARCHIVE)

DEF_CVAR(g_aggression, "0", CVAR_ARCHIVE)

DEF_CVAR(ddf_strict, "0", CVAR_ARCHIVE)
DEF_CVAR(ddf_lax,    "0", CVAR_ARCHIVE)
DEF_CVAR(ddf_quiet,  "0", CVAR_ARCHIVE)

static const image_c *loading_image = NULL;

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
		if (startup_messages.size() >= 15)
			startup_messages.erase(startup_messages.begin());
		startup_messages.push_back(message);
	}

	void drawIt()
	{
		I_StartFrame();
		HUD_FrameSetup();
		if (loading_image)
		{
			HUD_DrawImageTitleWS(loading_image);
			HUD_SolidBox(25, 25, 295, 175, RGB_MAKE(0, 0, 0));
		}
		int y = 26;
		for (int i=0; i < (int)startup_messages.size(); i++)
		{
			if (startup_messages[i].size() > 32)
				HUD_DrawText(26, y, startup_messages[i].substr(0, 29).append("...").c_str());
			else
				HUD_DrawText(26, y, startup_messages[i].c_str());
			y += 10;
		}

		if (!hud_overlays.at(r_overlay.d).empty())
		{
			const image_c *overlay = W_ImageLookup(hud_overlays.at(r_overlay.d).c_str(), INS_Graphic, ILF_Null);
			if (overlay)
				HUD_RawImage(0, 0, SCREENWIDTH, SCREENHEIGHT, overlay, 0, 0, SCREENWIDTH / IM_WIDTH(overlay), \
					SCREENHEIGHT / IM_HEIGHT(overlay));
		}

	if (v_gamma.d < 10)
	{
		int col = (1.0f - (0.1f * (10 - v_gamma.d))) * 255;
		glEnable(GL_BLEND);
		glBlendFunc(GL_ZERO, GL_SRC_COLOR);
		HUD_SolidBox(hud_x_left, 0, hud_x_right, 200, RGB_MAKE(col, col, col));
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDisable(GL_BLEND);
	}
	else if (v_gamma.d > 10)
	{
		int col = (0.1f * -(10 - v_gamma.d)) * 255;
		glEnable(GL_BLEND);
		glBlendFunc(GL_DST_COLOR, GL_ONE);
		HUD_SolidBox(hud_x_left, 0, hud_x_right, 200, RGB_MAKE(col, col, col));
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDisable(GL_BLEND);
	}

		I_FinishFrame();
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
	std::string s;

	// Screen Resolution Check...
	if (argv::Find("borderless") > 0)
		DISPLAYMODE = 2;
	else if (argv::Find("fullscreen") > 0)
		DISPLAYMODE = 1;
	else if (argv::Find("windowed") > 0)
		DISPLAYMODE = 0;

	s = epi::to_u8string(argv::Value("width"));
	if (!s.empty())
	{
		if (DISPLAYMODE == 2)
			I_Warning("Current display mode set to borderless fullscreen. Provided width of %d will be ignored!\n", atoi(s.c_str()));
		else
			SCREENWIDTH = atoi(s.c_str());
	}

	s = epi::to_u8string(argv::Value("height"));
	if (!s.empty())
	{
		if (DISPLAYMODE == 2)
			I_Warning("Current display mode set to borderless fullscreen. Provided height of %d will be ignored!\n", atoi(s.c_str()));
		else
			SCREENHEIGHT = atoi(s.c_str());
	}

	p = argv::Find("res");
	if (p > 0 && p + 2 < argv::list.size() && !argv::IsOption(p+1) && !argv::IsOption(p+2))
	{
		if (DISPLAYMODE == 2)
			I_Warning("Current display mode set to borderless fullscreen. Provided resolution of %dx%d will be ignored!\n", 
				atoi(epi::to_u8string(argv::list[p+1]).c_str()), atoi(epi::to_u8string(argv::list[p+2]).c_str()));
		else
		{
			SCREENWIDTH  = atoi(epi::to_u8string(argv::list[p+1]).c_str());
			SCREENHEIGHT = atoi(epi::to_u8string(argv::list[p+2]).c_str());
		}
	}

	// Bits per pixel check....
	s = epi::to_u8string(argv::Value("bpp"));
	if (!s.empty())
	{
		SCREENBITS = atoi(s.c_str());

		if (SCREENBITS <= 4) // backwards compat
			SCREENBITS *= 8;
	}

	// restrict depth to allowable values
	if (SCREENBITS < 15) SCREENBITS = 15;
	if (SCREENBITS > 32) SCREENBITS = 32;

	// If borderless fullscreen mode, override any provided dimensions so I_StartupGraphics will scale to native res
	if (DISPLAYMODE == 2)
	{
		SCREENWIDTH = 100000;
		SCREENHEIGHT = 100000;
	}

	// sprite kludge (TrueBSP)
	p = argv::Find("spritekludge");
	if (p > 0)
	{
		if (p + 1 < argv::list.size() && !argv::IsOption(p+1))
			sprite_kludge = atoi(epi::to_u8string(argv::list[p+1]).c_str());

		if (!sprite_kludge)
			sprite_kludge = 1;
	}

	s = epi::to_u8string(argv::Value("screenshot"));
	if (!s.empty())
	{
		screenshot_rate = atoi(s.c_str());
		singletics = true;
	}

	// -AJA- 1999/10/18: Reworked these with argv::CheckBooleanParm
	argv::CheckBooleanParm("rotatemap", &rotatemap, false);
	argv::CheckBooleanParm("sound", &nosound, true);
	argv::CheckBooleanParm("music", &nomusic, true);
	argv::CheckBooleanParm("itemrespawn", &global_flags.itemrespawn, false);
	argv::CheckBooleanParm("mlook", &global_flags.mlook, false);
	argv::CheckBooleanParm("monsters", &global_flags.nomonsters, true);
	argv::CheckBooleanParm("fast", &global_flags.fastparm, false);
	argv::CheckBooleanParm("extras", &global_flags.have_extra, false);
	argv::CheckBooleanParm("kick", &global_flags.kicking, false);
	argv::CheckBooleanParm("singletics", &singletics, false);
	argv::CheckBooleanParm("true3d", &global_flags.true3dgameplay, false);
	argv::CheckBooleanParm("blood", &global_flags.more_blood, false);
	argv::CheckBooleanParm("cheats", &global_flags.cheats, false);
	argv::CheckBooleanParm("jumping", &global_flags.jump, false);
	argv::CheckBooleanParm("crouching", &global_flags.crouch, false);
	argv::CheckBooleanParm("weaponswitch", &global_flags.weapon_switch, false);
	argv::CheckBooleanParm("autoload", &autoquickload, false);

	if (argv::Find("infight") > 0)
		g_aggression = 1;

	if (argv::Find("dlights") > 0)
		use_dlights = 1;
	else if (argv::Find("nodlights") > 0)
		use_dlights = 0;

	if (!global_flags.respawn)
	{
		if (argv::Find("newnmrespawn") > 0)
		{
			global_flags.res_respawn = true;
			global_flags.respawn = true;
		}
		else if (argv::Find("respawn") > 0)
		{
			global_flags.respawn = true;
		}
	}

	// check for strict and no-warning options
	argv::CheckBooleanCVar("strict", &ddf_strict, false);
	argv::CheckBooleanCVar("lax",    &ddf_lax,    false);
	argv::CheckBooleanCVar("warn",   &ddf_quiet,  true);

	strict_errors = ddf_strict.d ? true : false;
	lax_errors    = ddf_lax.d    ? true : false;
	no_warnings   = ddf_quiet.d  ? true : false;
}

//
// SetLanguage
//
void SetLanguage(void)
{
	std::string want_lang = epi::to_u8string(argv::Value("lang"));
	if (!want_lang.empty())
		m_language = want_lang;

	if (language.Select(m_language.c_str()))
		return;

	I_Warning("Invalid language: '%s'\n", m_language.c_str());

	if (! language.Select(0))
		I_Error("Unable to select any language!");

	m_language = language.GetName();
}

//
// SpecialWadVerify
//
static void SpecialWadVerify(void)
{
	E_ProgressMessage("Verifying EDGE-DEFS version...");

	epi::file_c *data = W_OpenPackFile("version.txt");

	if (!data)
		I_Error("Version file not found. Get edge_defs.epk at https://github.com/edge-classic/EDGE-classic");

	// parse version number
	std::string verstring = data->ReadText();
	const char *s = verstring.data();
	int epk_ver = atoi(s) * 100;

	while (isdigit(*s)) s++;
	s++;
	epk_ver += atoi(s);

	delete data;

	I_Printf("EDGE-DEFS.EPK version %1.2f found.\n", epk_ver / 100.0);

	if (epk_ver < EDGE_EPK_VERSION)
	{
		I_Error("EDGE-DEFS.EPK is an older version (expected %1.2f)\n",
		          EDGE_EPK_VERSION / 100.0);
	}
	else if (epk_ver > EDGE_EPK_VERSION)
	{
		I_Warning("EDGE-DEFS.EPK is a newer version (expected %1.2f)\n",
		          EDGE_EPK_VERSION / 100.0);
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

	HUD_StretchImage(x, y, w, h, pause_image, 0.0, 0.0);
}


wipetype_e wipe_method = WIPE_Melt;
int wipe_reverse = 0;

static bool need_wipe = false;

void E_ForceWipe(void)
{

#ifdef EDGE_WEB
	// Wiping blocks the main thread while rendering outside of the main loop tick
	// Disabled on the platform until can be better integrated
	return;
#endif	
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

	// process mouse and keyboard events
	N_NetUpdate();

	CON_Drawer();

	if (!hud_overlays.at(r_overlay.d).empty())
	{
		const image_c *overlay = W_ImageLookup(hud_overlays.at(r_overlay.d).c_str(), INS_Graphic, ILF_Null);
		if (overlay)
			HUD_RawImage(0, 0, SCREENWIDTH, SCREENHEIGHT, overlay, 0, 0, SCREENWIDTH / IM_WIDTH(overlay), \
				SCREENHEIGHT / IM_HEIGHT(overlay));
	}

	if (v_gamma.d < 10)
	{
		int col = (1.0f - (0.1f * (10 - v_gamma.d))) * 255;
		glEnable(GL_BLEND);
		glBlendFunc(GL_ZERO, GL_SRC_COLOR);
		HUD_SolidBox(hud_x_left, 0, hud_x_right, 200, RGB_MAKE(col, col, col));
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDisable(GL_BLEND);
	}
	else if (v_gamma.d > 10)
	{
		int col = (0.1f * -(10 - v_gamma.d)) * 255;
		glEnable(GL_BLEND);
		glBlendFunc(GL_DST_COLOR, GL_ONE);
		HUD_SolidBox(hud_x_left, 0, hud_x_right, 200, RGB_MAKE(col, col, col));
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDisable(GL_BLEND);
	}

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
void E_PickLoadingScreen(void)
{
	// force pic overflow -> first available titlepic
	title_game = gamedefs.GetSize() - 1;
	title_pic = 29999;

	// prevent an infinite loop
	for (int loop=0; loop < 100; loop++)
	{
		gamedef_c *g = gamedefs[title_game];
		SYS_ASSERT(g);

		if (title_pic >= (int)g->titlepics.size())
		{
			title_game = (title_game + 1) % (int)gamedefs.GetSize();
			title_pic  = 0;
			continue;
		}

		// ignore non-existing episodes.  Doesn't include title-only ones
		// like [EDGE].
		if (title_pic == 0 && g->firstmap != "" &&
			W_CheckNumForName(g->firstmap.c_str()) == -1)
		{
			title_game = (title_game + 1) % gamedefs.GetSize();
			title_pic  = 0;
			continue;
		}

		// ignore non-existing images
		loading_image = W_ImageLookup(g->titlepics[title_pic].c_str(), INS_Graphic, ILF_Null);

		if (! loading_image)
		{
			title_pic++;
			continue;
		}

		// found one !!
		title_game = gamedefs.GetSize() - 1;
		title_pic = 29999;
		return;
	}

	// not found
	title_game = gamedefs.GetSize() - 1;
	title_pic = 29999;
	loading_image = NULL;
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

		if (title_pic >= (int)g->titlepics.size())
		{
			title_game = (title_game + 1) % (int)gamedefs.GetSize();
			title_pic  = 0;
			continue;
		}

		// ignore non-existing episodes.  Doesn't include title-only ones
		// like [EDGE].
		if (title_pic == 0 && g->firstmap != "" &&
			W_CheckNumForName(g->firstmap.c_str()) == -1)
		{
			title_game = (title_game + 1) % gamedefs.GetSize();
			title_pic  = 0;
			continue;
		}

		// ignore non-existing images
		title_image = W_ImageLookup(g->titlepics[title_pic].c_str(), INS_Graphic, ILF_Null);

		if (! title_image)
		{
			title_pic++;
			continue;
		}

		// found one !!

		if (title_pic == 0 && g->titlemusic > 0)
			S_ChangeMusic(g->titlemusic, false);

		title_countdown = g->titletics * (r_doubleframes.d ? 2 : 1);
		return;
	}

	// not found

	title_image = NULL;
	title_countdown = TICRATE * (r_doubleframes.d ? 2 : 1);
}

void E_StartTitle(void)
{
	gameaction = ga_nothing;
	gamestate  = GS_TITLESCREEN;

	paused = false;

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
	// Get the Game Directory from parameter.
	
	// Note: This might need adjusting for Apple
	std::filesystem::path s = UTFSTR(SDL_GetBasePath());

	game_dir = s;
	s = argv::Value("game");
	if (!s.empty())
		game_dir = s;

	brandingfile = epi::PATH_Join(game_dir, UTFSTR(EDGEBRANDINGFILE));

	M_LoadBranding();

	// add parameter file "gamedir/parms" if it exists.
	std::filesystem::path parms = epi::PATH_Join(game_dir, UTFSTR("parms"));

	if (epi::FS_Access(parms, epi::file_c::ACCESS_READ))
	{
		// Insert it right after the game parameter
		argv::ApplyResponseFile(parms);
	}

	// config file - check for portable config
	s = argv::Value("config");
	if (!s.empty())
	{
		cfgfile = s;
	}
	else
    {
		cfgfile = epi::PATH_Join(game_dir, UTFSTR(configfilename.s));
		if (epi::FS_Access(cfgfile, epi::file_c::ACCESS_READ) || argv::Find("portable") > 0)
			home_dir = game_dir;
		else
        	cfgfile.clear();
	}

	if (home_dir.empty())
	{
		s = argv::Value("home");
		if (!s.empty())
			home_dir = s;
	}

	// Get the Home Directory from environment if set
    if (home_dir.empty())
    {
		s.clear();
		if (getenv("HOME"))
			s = UTFSTR(getenv("HOME"));
        if (!s.empty())
        {
            home_dir = epi::PATH_Join(s, UTFSTR(appname.c_str())); 

			if (! epi::FS_IsDir(home_dir))
			{
                epi::FS_MakeDir(home_dir);

                // Check whether the directory was created
                if (! epi::FS_IsDir(home_dir))
                    home_dir.clear();
			}
        }
    }

#ifdef _WIN32
    if (home_dir.empty()) home_dir = UTFSTR(SDL_GetPrefPath(nullptr, appname.c_str()));
#else
	if (home_dir.empty()) home_dir = UTFSTR(SDL_GetPrefPath(orgname.c_str(), appname.c_str()));
#endif

	if (! epi::FS_IsDir(home_dir))
        epi::FS_MakeDir(home_dir);

	if (cfgfile.empty())
		cfgfile = epi::PATH_Join(home_dir, UTFSTR(configfilename.s));

	// edge-defs.epk file
	s = argv::Value("eepk");
	if (!s.empty())
	{
		epkfile = s;
	}
	else
    {
		if (epi::FS_IsDir(epi::PATH_Join(game_dir, UTFSTR("edge-defs"))))
			epkfile = epi::PATH_Join(game_dir, UTFSTR("edge-defs"));
		else
        	epkfile = epi::PATH_Join(game_dir, UTFSTR("edge-defs.epk"));
	}

	// cache directory
    cache_dir = epi::PATH_Join(home_dir, UTFSTR(CACHEDIR));

    if (! epi::FS_IsDir(cache_dir))
        epi::FS_MakeDir(cache_dir);

	// savegame directory
    save_dir = epi::PATH_Join(home_dir, UTFSTR(SAVEGAMEDIR));
	
    if (! epi::FS_IsDir(save_dir)) epi::FS_MakeDir(save_dir);

	SV_ClearSlot("current");

	// screenshot directory
    shot_dir = epi::PATH_Join(home_dir, UTFSTR(SCRNSHOTDIR));

    if (!epi::FS_IsDir(shot_dir))
        epi::FS_MakeDir(shot_dir);
}

// Get rid of legacy GWA/HWA files or XWA files older than 6 months

static void PurgeCache(void)
{
	const std::filesystem::file_time_type expiry = std::filesystem::file_time_type::clock::now() - std::chrono::hours(4320);

	std::vector<epi::dir_entry_c> fsd;

	if (!FS_ReadDir(fsd, cache_dir, UTFSTR("*.*")))
	{
		I_Error("PurgeCache: Failed to read '%s' directory!\n", cache_dir.u8string().c_str());
	}
	else
	{
		for (size_t i = 0 ; i < fsd.size() ; i++) 
		{
			if(!fsd[i].is_dir)
			{
				if (fsd[i].name.extension().compare(UTFSTR(".gwa")) == 0)
					epi::FS_Delete(fsd[i].name);
				else if (fsd[i].name.extension().compare(UTFSTR(".hwa")) == 0)
					epi::FS_Delete(fsd[i].name);
				else if (fsd[i].name.extension().compare(UTFSTR(".xwa")) == 0)
				{
					if(std::filesystem::last_write_time(fsd[i].name) < expiry)
					{
						epi::FS_Delete(fsd[i].name);
					}
				}		
			}
		}
	}	
}

//
// Adds an IWAD and edge-defs.epk
//

static void IdentifyVersion(void)
{
	if (epi::FS_IsDir(epkfile))
    	W_AddFilename(epkfile, FLKIND_EFolder);
	else
	{
		if (! epi::FS_Access(epkfile, epi::file_c::ACCESS_READ))
			I_Error("IdentifyVersion: Could not find required %s.%s!\n", 
				REQUIREDEPK, "epk");
		W_AddFilename(epkfile, FLKIND_EEPK);
	}

	I_Debugf("- Identify Version\n");

	// Check -iwad parameter, find out if it is the IWADs directory
    std::filesystem::path iwad_par;
    std::filesystem::path iwad_file;
    std::filesystem::path iwad_dir;
	std::vector<std::filesystem::path> iwad_dir_vector;

	std::filesystem::path s = argv::Value("iwad");

    iwad_par = s;

    if (! iwad_par.empty())
    {
        if (epi::FS_IsDir(iwad_par))
        {
            iwad_dir = iwad_par;
            iwad_par.clear(); // Discard 
        }
    }   

    // If we haven't yet set the IWAD directory, then we check
    // the DOOMWADDIR environment variable
    if (iwad_dir.empty())
    {
		if (getenv("DOOMWADDIR"))
        	s = UTFSTR(getenv("DOOMWADDIR"));

        if (!s.empty() && epi::FS_IsDir(s))
            iwad_dir_vector.push_back(s);
    }

    // Should the IWAD directory not be set by now, then we
    // use our standby option of the current directory.
    if (iwad_dir.empty()) iwad_dir = UTFSTR(".");

	// Add DOOMWADPATH directories if they exist
	s.clear();
	if (getenv("DOOMWADPATH"))
		s = UTFSTR(getenv("DOOMWADPATH"));
	if (!s.empty())
	{
#ifdef _WIN32
        for (auto dir : epi::STR_SepStringVector(s.u32string(), ';'))
#else
		for (auto dir : epi::STR_SepStringVector(s.string(), ':'))
#endif	
			iwad_dir_vector.push_back(dir);
	}

    // Should the IWAD Parameter not be empty then it means
    // that one was given which is not a directory. Therefore
    // we assume it to be a name
    if (!iwad_par.empty())
    {
        std::filesystem::path fn = iwad_par;
        
        // Is it missing the extension?
        std::filesystem::path ext = epi::PATH_GetExtension(iwad_par);
        if (ext.empty())
        {
            fn += UTFSTR(".wad");
        }

        // If no directory given use the IWAD directory
        std::filesystem::path dir = epi::PATH_GetDir(fn);
        if (dir.empty())
#ifdef _WIN32
            iwad_file = epi::PATH_Join(iwad_dir, fn.u32string());
#else
			iwad_file = epi::PATH_Join(iwad_dir, fn.string());
#endif
        else
            iwad_file = fn;

        if (!epi::FS_Access(iwad_file, epi::file_c::ACCESS_READ))
        {
			// Check DOOMWADPATH directories if present
			if (!iwad_dir_vector.empty())
			{
				for (size_t i=0; i < iwad_dir_vector.size(); i++)
				{
#ifdef _WIN32
					iwad_file = epi::PATH_Join(iwad_dir_vector[i], fn.u32string());
#else
					iwad_file = epi::PATH_Join(iwad_dir_vector[i], fn.string());
#endif
					if (epi::FS_Access(iwad_file, epi::file_c::ACCESS_READ))
						goto foundindoomwadpath;
				}
				I_Error("IdentifyVersion: Unable to access specified '%s'", fn.u8string().c_str());
			}
			else
				I_Error("IdentifyVersion: Unable to access specified '%s'", fn.u8string().c_str());
        }

		foundindoomwadpath:

		epi::file_c *iwad_test = epi::FS_Open(iwad_file, epi::file_c::ACCESS_READ | epi::file_c::ACCESS_BINARY);
		bool unique_lump_match = false;
		for (size_t i=0; i < iwad_checker.size(); i++) {
			if (W_CheckForUniqueLumps(iwad_test, iwad_checker[i].unique_lumps[0], iwad_checker[i].unique_lumps[1]))
			{
				unique_lump_match = true;
				iwad_base = iwad_checker[i].base;
				break;
			}
    	}
		if (iwad_test) delete iwad_test;
		if (unique_lump_match)
			W_AddFilename(iwad_file, FLKIND_IWad);
		else
			I_Error("IdentifyVersion: Could not identify '%s' as a valid IWAD!\n", fn.u8string().c_str());
    }
    else
    {
        std::filesystem::path location;
		
		// Track the "best" IWAD found throughout the various paths based on scores stored in iwad_checker
		u8_t best_score = 0;
		std::filesystem::path best_match;

        int max = 1;

        if (iwad_dir.compare(game_dir) != 0) 
        {
            // IWAD directory & game directory differ 
            // therefore do a second loop which will
            // mean we check both.
            max++;
        } 

		for (int i = 0; i < max; i++)
		{
			location = (i == 0 ? iwad_dir : game_dir);

			//
			// go through the available *.wad files, attempting IWAD
			// detection for each, adding the file if they exist.
			//
			// -ACB- 2000/06/08 Quit after we found a file - don't load
			//                  more than one IWAD
			//
			std::vector<epi::dir_entry_c> fsd;

			if (!FS_ReadDir(fsd, location, UTFSTR("*.wad")))
			{
				I_Warning("IdenfityVersion: Failed to read '%s' directory!\n", location.u8string().c_str());
			}
			else
			{
				for (size_t j = 0 ; j < fsd.size() ; j++) 
				{
					if(!fsd[j].is_dir)
					{
						epi::file_c *iwad_test = epi::FS_Open(fsd[j].name, epi::file_c::ACCESS_READ | epi::file_c::ACCESS_BINARY);
						for (size_t k = 0; k < iwad_checker.size(); k++) 
						{
							if (W_CheckForUniqueLumps(iwad_test, iwad_checker[k].unique_lumps[0], iwad_checker[k].unique_lumps[1]))
							{
								if (iwad_checker[k].score > best_score)
								{
									best_score = iwad_checker[k].score;
									best_match = fsd[j].name;
									iwad_base = iwad_checker[k].base;
								}
								break;
							}
						}
						if (iwad_test) delete iwad_test;				
					}
				}
			}
		}

		// Separate check for DOOMWADPATH stuff if it exists - didn't want to mess with the existing stuff above

		if (!iwad_dir_vector.empty())
		{
			for (size_t i=0; i < iwad_dir_vector.size(); i++)
			{
				location = iwad_dir_vector[i].c_str();

				std::vector<epi::dir_entry_c> fsd;

				if (!FS_ReadDir(fsd, location, UTFSTR("*.wad")))
				{
					I_Warning("IdenfityVersion: Failed to read '%s' directory!\n", location.u8string().c_str());
				}
				else
				{
					for (size_t j = 0 ; j < fsd.size() ; j++) 
					{
						if(!fsd[j].is_dir)
						{
							epi::file_c *iwad_test = epi::FS_Open(fsd[j].name, epi::file_c::ACCESS_READ | epi::file_c::ACCESS_BINARY);
							for (size_t k = 0; k < iwad_checker.size(); k++) 
							{
								if (W_CheckForUniqueLumps(iwad_test, iwad_checker[k].unique_lumps[0], iwad_checker[k].unique_lumps[1]))
								{
									if (iwad_checker[k].score > best_score)
									{
										best_score = iwad_checker[k].score;
										best_match = fsd[j].name;
										iwad_base = iwad_checker[k].base;
									}
									break;
								}
							}
							if (iwad_test) delete iwad_test;			
						}
					}
				}
			}
		}

		if (best_score == 0)
			I_Error("IdentifyVersion: No IWADs found!\n");
		else
			W_AddFilename(best_match, FLKIND_IWad);
    }

	I_Debugf("IWAD BASE = [%s]\n", iwad_base.c_str());
}

// Add game-specific base EPKs (widepix, skyboxes, etc) - Dasho
static void Add_Base(void) 
{
	if (epi::case_cmp("CUSTOM", iwad_base) == 0)
		return; // Custom standalone EDGE IWADs should already contain their necessary resources and definitions - Dasho
	std::filesystem::path base_path = epi::PATH_Join(game_dir, UTFSTR("edge_base"));
	std::string base_wad = iwad_base;
	std::transform(base_wad.begin(), base_wad.end(), base_wad.begin(), ::tolower);
	base_path = epi::PATH_Join(base_path, UTFSTR(base_wad.append("_base")));
	if (epi::FS_IsDir(base_path))
		W_AddFilename(base_path, FLKIND_EFolder);
	else if (epi::FS_Access(base_path.replace_extension(".epk"), epi::file_c::ACCESS_READ)) 
		W_AddFilename(base_path, FLKIND_EEPK);
	else
		I_Error("%s not found for the %s IWAD! Check the /edge_base folder of your %s install!\n", base_path.filename().u8string().c_str(),
			iwad_base.c_str(), appname.c_str());
}

static void CheckTurbo(void)
{
	int turbo_scale = 100;

	int p = argv::Find("turbo");

	if (p > 0)
	{
		if (p + 1 < argv::list.size() && !argv::IsOption(p+1))
			turbo_scale = atoi(epi::to_u8string(argv::list[p+1]).c_str());
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
	I_Printf("%s v%s compiled on " __DATE__ " at " __TIME__ "\n", appname.c_str(), versionstring.c_str());
	I_Printf("%s homepage is at %s\n", appname.c_str(), homepage.c_str());
	//I_Printf("EDGE-Classic is based on DOOM by id Software http://www.idsoftware.com/\n");

	I_Printf("Executable path: '%s'\n", exe_path.u8string().c_str());

	argv::DebugDumpArgs();
}

static void SetupLogAndDebugFiles(void)
{
	// -AJA- 2003/11/08 The log file gets all CON_Printfs, I_Printfs,
	//                  I_Warnings and I_Errors.

	std::filesystem::path log_fn  (epi::PATH_Join(home_dir, UTFSTR(logfilename.s)));
	std::filesystem::path debug_fn(epi::PATH_Join(home_dir, UTFSTR(debugfilename.s)));

	logfile = NULL;
	debugfile = NULL;

	if (argv::Find("nolog") < 0)
	{
		logfile = EPIFOPEN(log_fn, "w");

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

	/// int p = argv::Find("debug");
	if (true)
	{
		debugfile = EPIFOPEN(debug_fn, "w");

		if (!debugfile)
			I_Error("[E_Startup] Unable to create debugfile");
	}
}

static void AddSingleCmdLineFile(std::filesystem::path name, bool ignore_unknown)
{
	std::string ext = epi::PATH_GetExtension(name).u8string();

	epi::str_lower(ext);

	if (ext == ".edm")
		I_Error("Demos are not supported\n");

	// no need to check for GWA (shouldn't be added manually)

	filekind_e kind;

	if (ext == ".wad")
		kind = FLKIND_PWad;
	else if (ext == ".pk3" || ext == ".epk")
		kind = FLKIND_EPK;
	else if (ext == ".rts")
		kind = FLKIND_RTS;
	else if (ext == ".ddf" || ext == ".ldf")
		kind = FLKIND_DDF;
	else if (ext == ".deh" || ext == ".bex")
		kind = FLKIND_Deh;
	else
	{
		if (! ignore_unknown)
			I_Error("unknown file type: %s\n", name.u8string().c_str());
		return;
	}

	std::filesystem::path filename = M_ComposeFileName(game_dir, name);
	W_AddFilename(filename, kind);
}

static void AddCommandLineFiles(void)
{
	// first handle "loose" files (arguments before the first option)

	int p;

	for (p = 1; p < argv::list.size() && !argv::IsOption(p); p++)
	{
		AddSingleCmdLineFile(argv::list[p], false);
	}

	// next handle the -file option (we allow multiple uses)

	p = argv::Find("file");

	while (p > 0 && p < argv::list.size() && (!argv::IsOption(p) || epi::strcmp(epi::to_u8string(argv::list[p]), "-file") == 0))
	{
		// the parms after p are wadfile/lump names,
		// go until end of parms or another '-' preceded parm
		if (!argv::IsOption(p))
			AddSingleCmdLineFile(argv::list[p], false);

		p++;
	}

	// scripts....

	p = argv::Find("script");

	while (p > 0 && p < argv::list.size() && (!argv::IsOption(p) || epi::strcmp(epi::to_u8string(argv::list[p]), "-script") == 0))
	{
		// the parms after p are script filenames,
		// go until end of parms or another '-' preceded parm
		if (!argv::IsOption(p))
		{
			std::string ext = epi::PATH_GetExtension(argv::list[p]).u8string();
			// sanity check...
			if (epi::case_cmp(ext, ".wad") == 0 || 
				epi::case_cmp(ext, ".pk3") == 0 ||
				epi::case_cmp(ext, ".epk") == 0 ||
				epi::case_cmp(ext, ".ddf") == 0 ||
				epi::case_cmp(ext, ".deh") == 0 ||
				epi::case_cmp(ext, ".bex") == 0)
			{
				I_Error("Illegal filename for -script: %s\n", epi::to_u8string(argv::list[p]).c_str());
			}

			std::filesystem::path filename = M_ComposeFileName(game_dir, argv::list[p]);
			W_AddFilename(filename, FLKIND_RTS);
		}

		p++;
	}

	// dehacked/bex....

	p = argv::Find("deh");

	while (p > 0 && p < argv::list.size() && (!argv::IsOption(p) || epi::strcmp(epi::to_u8string(argv::list[p]), "-deh") == 0))
	{
		// the parms after p are Dehacked/BEX filenames,
		// go until end of parms or another '-' preceded parm
		if (!argv::IsOption(p))
		{
			std::string ext = epi::PATH_GetExtension(argv::list[p]).u8string();
			// sanity check...
			if (epi::case_cmp(ext, ".wad") == 0 ||
				epi::case_cmp(ext, ".epk") == 0 ||
				epi::case_cmp(ext, ".pk3") == 0 ||
				epi::case_cmp(ext, ".ddf") == 0 ||
				epi::case_cmp(ext, ".rts") == 0)
			{
				I_Error("Illegal filename for -deh: %s\n", epi::to_u8string(argv::list[p]).c_str());
			}

			std::filesystem::path filename = M_ComposeFileName(game_dir, argv::list[p]);
			W_AddFilename(filename, FLKIND_Deh);
		}

		p++;
	}

	// directories....

	p = argv::Find("dir");

	while (p > 0 && p < argv::list.size() && (!argv::IsOption(p) || epi::strcmp(epi::to_u8string(argv::list[p]),"-dir") == 0))
	{
		// the parms after p are directory names,
		// go until end of parms or another '-' preceded parm
		if (!argv::IsOption(p))
		{
			std::filesystem::path dirname = M_ComposeFileName(game_dir, argv::list[p]);
			W_AddFilename(dirname, FLKIND_Folder);
		}

		p++;
	}

	// handle -ddf option (backwards compatibility)

	std::filesystem::path ps = argv::Value("ddf");

	if (!ps.empty())
	{
		std::filesystem::path filename = M_ComposeFileName(game_dir, ps);
		W_AddFilename(filename.c_str(), FLKIND_Folder);
	}
}

static void Add_Autoload(void) {
	
	std::vector<epi::dir_entry_c> fsd;
	std::filesystem::path folder = epi::PATH_Join(game_dir, UTFSTR("autoload"));

	if (!FS_ReadDir(fsd, folder, UTFSTR("*.*")))
	{
		I_Warning("Failed to read %s directory!\n", folder.u8string().c_str());
	}
	else
	{
		for (size_t i = 0 ; i < fsd.size() ; i++) 
		{
			if(!fsd[i].is_dir)
				AddSingleCmdLineFile(fsd[i].name, true);
		}
	}
	fsd.clear();
	std::string lowercase_base = iwad_base;
	std::transform(lowercase_base.begin(), lowercase_base.end(), lowercase_base.begin(), ::tolower);
	folder = epi::PATH_Join(folder, UTFSTR(lowercase_base));
	if (!FS_ReadDir(fsd, folder, UTFSTR("*.*")))
	{
		I_Warning("Failed to read %s directory!\n", folder.u8string().c_str());
	}
	else
	{
		for (size_t i = 0 ; i < fsd.size() ; i++) 
		{
			if(!fsd[i].is_dir)
				AddSingleCmdLineFile(fsd[i].name, true);
		}		
	}
	fsd.clear();

	// Check if autoload folder stuff is in home_dir as well, make the folder/subfolder if they don't exist (in home_dir only)
	folder = epi::PATH_Join(home_dir, UTFSTR("autoload"));
	if (!epi::FS_IsDir(folder))
		epi::FS_MakeDir(folder);

	if (!FS_ReadDir(fsd, folder, UTFSTR("*.*")))
	{
		I_Warning("Failed to read %s directory!\n", folder.u8string().c_str());
	}
	else
	{
		for (size_t i = 0 ; i < fsd.size() ; i++) 
		{
			if(!fsd[i].is_dir)
				AddSingleCmdLineFile(fsd[i].name, true);
		}
	}
	fsd.clear();
	folder = epi::PATH_Join(folder, UTFSTR(lowercase_base));
	if (!epi::FS_IsDir(folder))
		epi::FS_MakeDir(folder);
	if (!FS_ReadDir(fsd, folder, UTFSTR("*.*")))
	{
		I_Warning("Failed to read %s directory!\n", folder.u8string().c_str());
	}
	else
	{
		for (size_t i = 0 ; i < fsd.size() ; i++) 
		{
			if(!fsd[i].is_dir)
				AddSingleCmdLineFile(fsd[i].name, true);
		}		
	}
}

static void InitDDF(void)
{
	I_Debugf("- Initialising DDF\n");

	DDF_Init();
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

	P_Shutdown();

    S_Shutdown();
	R_Shutdown();

}

// Local Prototypes
static void E_Startup();
static void E_Shutdown(void);


static void E_Startup(void)
{
	CON_InitConsole();

	// Version check ?
	if (argv::Find("version") > 0)
	{
		// -AJA- using I_Error here, since I_Printf crashes this early on
		I_Error("\n%s version is %s\n", appname.c_str(), versionstring.c_str());
	}

	// -AJA- 2000/02/02: initialise global gameflags to defaults
	global_flags = default_gameflags;

	InitDirectories();

	SetupLogAndDebugFiles();

	PurgeCache();

	ShowDateAndVersion();

	M_LoadDefaults();

	CON_HandleProgramArgs();
	SetGlobalVars();

	DoSystemStartup();

	InitDDF();
	IdentifyVersion();
	Add_Base();
	Add_Autoload();
	AddCommandLineFiles();
	CheckTurbo();

	RAD_Init();
	W_ProcessMultipleFiles();
	DDF_ParseEverything();
	W_ImageAddPackImages();
	W_AddPackSoundsAndMusic(); // Must be done after all DDF is parse to check for SFX/Music replacements
	I_StartupMusic(); // Must be done after all files loaded to locate appropriate GENMIDI lump
	V_InitPalette();

	DDF_CleanUp();
	W_ReadUMAPINFOLumps();

	W_InitFlats();
	W_InitTextures();
	W_ImageCreateUser();
	E_PickLoadingScreen();

	HU_Init();
	CON_Start();
	CON_CreateQuitScreen();
	SpecialWadVerify();
	W_BuildNodes();
	M_InitMiscConVars();
	SetLanguage();
	ShowNotice();

	SV_MainInit();
	S_PrecacheSounds();
	W_InitSprites();
	W_ProcessTX_HI();
	W_InitModels();

	M_Init();
	R_Init();
	P_Init();
	P_MapInit();
	P_InitSwitchList();
	W_InitPicAnims();
	S_Init();
	N_InitNetwork();
	M_CheatInit();
	VM_InitCoal();
	VM_LoadScripts();
}


static void E_Shutdown(void)
{
	/* TODO: E_Shutdown */
}


static void E_InitialState(void)
{
	I_Debugf("- Setting up Initial State...\n");

#ifdef _WIN32
	std::u32string ps;
#else
	std::string ps;
#endif

	// do loadgames first, as they contain all of the
	// necessary state already (in the savegame).

	if (argv::Find("playdemo") > 0 || argv::Find("timedemo") > 0 ||
	    argv::Find("record") > 0)
	{
		I_Error("Demos are no longer supported\n");
	}

	ps = argv::Value("loadgame");
	if (!ps.empty())
	{
		G_DeferredLoadGame(atoi(epi::to_u8string(ps).c_str()));
		return;
	}

	bool warp = false;

	// get skill / episode / map from parms
	std::string warp_map;
	skill_t     warp_skill = sk_medium;
	int         warp_deathmatch = 0;

	int bots = 0;

	ps = argv::Value("bots");
	if (!ps.empty())
		bots = atoi(epi::to_u8string(ps).c_str());

	ps = argv::Value("warp");
	if (!ps.empty())
	{
		warp = true;
		warp_map = epi::to_u8string(ps);
	}

	// -KM- 1999/01/29 Use correct skill: 1 is easiest, not 0
	ps = argv::Value("skill");
	if (!ps.empty())
	{
		warp = true;
		warp_skill = (skill_t)(atoi(epi::to_u8string(ps).c_str()) - 1);
	}

	// deathmatch check...
	int pp = argv::Find("deathmatch");
	if (pp > 0)
	{
		warp_deathmatch = 1;

		if (pp + 1 < argv::list.size() && !argv::IsOption(pp+1))
			warp_deathmatch = MAX(1, atoi(epi::to_u8string(argv::list[pp+1]).c_str()));

		warp = true;
	}
	else if (argv::Find("altdeath") > 0)
	{
		warp_deathmatch = 2;

		warp = true;
	}

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

	// Implemented here - since we need to bring the memory manager up first
	// -ACB- 2004/05/31
	argv::Init(argc, argv);

	try
	{
		E_Startup();

		E_InitialState();

		CON_MessageColor(RGB_MAKE(255,255,0));
		I_Printf("%s v%s initialisation complete.\n", appname.c_str(), versionstring.c_str());

		I_Debugf("- Entering game loop...\n");

#ifndef EDGE_WEB
		while (! (app_state & APP_STATE_PENDING_QUIT))
		{
			// We always do this once here, although the engine may
			// makes in own calls to keep on top of the event processing
			I_ControlGetEvents(); 

			if (app_state & APP_STATE_ACTIVE)
				E_Tick();
		}
#else
		return;
#endif		
	}
	catch(const std::exception& e)
	{
		I_Error("Unexpected internal failure occurred: %s\n", e.what());
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

	// this also runs the responder chain via E_ProcessEvents
	int counts = N_TryRunTics();

	// ignore this assertion if in a menu; switching between 35/70FPS
	// in Video Options can occasionally produce a 'valid'
	// zero count for N_TryRunTics()

	if (!menuactive)
		SYS_ASSERT(counts > 0);

	// run the tics
	for (; counts > 0 ; counts--)
	{
		// run a step in the physics (etc)
		G_Ticker();

		// user interface stuff (skull anim, etc)
		CON_Ticker();
		M_Ticker();
		S_SoundTicker(); 
		S_MusicTicker();

		// process mouse and keyboard events
		N_NetUpdate();
	}
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
