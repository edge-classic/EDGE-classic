//----------------------------------------------------------------------------
//  EDGE Misc: Screenshots, Menu and defaults Code
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
// -MH- 1998/07/02  Added key_flyup and key_flydown
// -MH- 1998/07/02 "shootupdown" --> "true3dgameplay"
// -ACB- 2000/06/02 Removed Control Defaults
//

#include "i_defs.h"

#include "endianess.h"
#include "file.h"
#include "filesystem.h"
#include "path.h"
#include "str_lexer.h"
#include "str_util.h"

#include "image_data.h"
#include "image_funcs.h"

#include "con_main.h"
#include "dm_defs.h"
#include "dm_state.h"
#include "e_input.h"
#include "e_player.h"
#include "g_game.h"
#include "hu_draw.h"
#include "hu_stuff.h"  // only for showMessages
#include "m_argv.h"
#include "m_menu.h"
#include "m_misc.h"
#include "m_option.h"
#include "n_network.h"
#include "p_spec.h"
#include "r_gldefs.h"
#include "s_blit.h"
#include "s_music.h"  // mus_volume
#include "s_sound.h"
#include "am_map.h"
#include "r_colormap.h"
#include "r_draw.h"
#include "r_modes.h"
#include "r_image.h"
#include "r_wipe.h"
#include "version.h"

#include "i_ctrl.h"

#include "defaults.h"

//
// DEFAULTS
//
bool force_directx = false;
bool force_waveout = false;

unsigned short save_screenshot[160][100];
bool save_screenshot_valid = false;

bool var_pc_speaker_mode = false;
int var_midi_player    = 0;
int var_sound_stereo = 0;
int var_mix_channels = 0;

static int edge_version;
static bool done_first_init = false;

extern int joystick_device;

static default_t defaults[] =
{
    {CFGT_Int,		"edge_version",		 &edge_version,	  0},
    {CFGT_Int,		"screenwidth",		 &SCREENWIDTH,	  CFGDEF_SCREENWIDTH},
    {CFGT_Int,		"screenheight",		 &SCREENHEIGHT,	  CFGDEF_SCREENHEIGHT},
    {CFGT_Int,		"screendepth",		 &SCREENBITS,	  CFGDEF_SCREENBITS},
    {CFGT_Int,	    "displaymode",		 &DISPLAYMODE,	  CFGDEF_DISPLAYMODE},
    {CFGT_Boolean,	"directx",			 &force_directx,  0},
    {CFGT_Boolean,	"waveout",			 &force_waveout,  0},
 
    {CFGT_Int,      "sfx_volume",        &sfx_volume,     CFGDEF_SOUND_VOLUME},
    {CFGT_Int,      "music_volume",      &mus_volume,     CFGDEF_MUSIC_VOLUME},
    {CFGT_Int,      "sound_stereo",      &var_sound_stereo, CFGDEF_SOUND_STEREO},
    {CFGT_Boolean,	"pc_speaker_mode",	 &var_pc_speaker_mode,  0},
    {CFGT_Int,		"midi_player",		 &var_midi_player,   0},
    {CFGT_Boolean,	"dynamic_reverb",	 &dynamic_reverb,  0},
    {CFGT_Int,      "mix_channels",      &var_mix_channels, CFGDEF_MIX_CHANNELS},

    {CFGT_Int,      "show_messages",     &showMessages,   CFGDEF_SHOWMESSAGES},

    // -ES- 1998/11/28 Save fade settings
    {CFGT_Enum,     "telept_effect",     &telept_effect,  CFGDEF_TELEPT_EFFECT},
    {CFGT_Int,      "telept_reverse",    &telept_reverse, CFGDEF_TELEPT_REVERSE},
    {CFGT_Int,      "telept_flash",      &telept_flash,   CFGDEF_TELEPT_FLASH},
    {CFGT_Int,      "invuln_fx",         &var_invul_fx,   CFGDEF_INVUL_FX},
    {CFGT_Enum,     "wipe_method",       &wipe_method,    CFGDEF_WIPE_METHOD},
    {CFGT_Int,      "wipe_reverse",      &wipe_reverse,   CFGDEF_WIPE_REVERSE},
    {CFGT_Boolean,  "rotatemap",         &rotatemap,      CFGDEF_ROTATEMAP},
    {CFGT_Boolean,  "respawnsetting",    &global_flags.res_respawn, CFGDEF_RES_RESPAWN},
    {CFGT_Boolean,  "itemrespawn",       &global_flags.itemrespawn, CFGDEF_ITEMRESPAWN},
    {CFGT_Boolean,  "respawn",           &global_flags.respawn, CFGDEF_RESPAWN},
    {CFGT_Boolean,  "fastparm",          &global_flags.fastparm, CFGDEF_FASTPARM},
    {CFGT_Int,      "grav",              &global_flags.menu_grav, CFGDEF_MENU_GRAV},
    {CFGT_Boolean,  "true3dgameplay",    &global_flags.true3dgameplay, CFGDEF_TRUE3DGAMEPLAY},
    {CFGT_Enum,     "autoaim",           &global_flags.autoaim, CFGDEF_AUTOAIM},
    {CFGT_Int,      "doom_fading",       &doom_fading,    CFGDEF_DOOM_FADING},
    {CFGT_Boolean,  "shootthru_scenery", &global_flags.pass_missile, CFGDEF_PASS_MISSILE},
	{CFGT_Boolean,  "splash_screen",     &splash_screen, true},
	{CFGT_Int,  "swirling_flats",     &swirling_flats, 0},

	{CFGT_Boolean,  "pistol_starts",             &pistol_starts, 0},
    // -KM- 1998/07/21 Save the blood setting
    {CFGT_Boolean,  "blood",             &global_flags.more_blood, CFGDEF_MORE_BLOOD},
    {CFGT_Boolean,  "extra",             &global_flags.have_extra, CFGDEF_HAVE_EXTRA},
    {CFGT_Boolean,  "weaponkick",        &global_flags.kicking, CFGDEF_KICKING},
    {CFGT_Boolean,  "weaponswitch",      &global_flags.weapon_switch, CFGDEF_WEAPON_SWITCH},
    {CFGT_Boolean,  "mlook",             &global_flags.mlook, CFGDEF_MLOOK},
    {CFGT_Boolean,  "jumping",           &global_flags.jump, CFGDEF_JUMP},
    {CFGT_Boolean,  "crouching",         &global_flags.crouch, CFGDEF_CROUCH},
    {CFGT_Int,      "mipmapping",        &var_mipmapping, CFGDEF_USE_MIPMAPPING},
    {CFGT_Int,      "smoothing",         &var_smoothing,  CFGDEF_USE_SMOOTHING},
    {CFGT_Boolean,  "dither",            &var_dithering, 0},
    {CFGT_Int,      "dlights",           &use_dlights,    CFGDEF_USE_DLIGHTS},
    {CFGT_Int,      "detail_level",      &detail_level,   CFGDEF_DETAIL_LEVEL},
	{CFGT_Int,      "hq2x_scaling",      &hq2x_scaling,   CFGDEF_HQ2X_SCALING},

    // -KM- 1998/09/01 Useless mouse/joy stuff removed,
    //                 analogue binding added
    {CFGT_Int,      "mouse_axis_x",      &mouse_xaxis,  CFGDEF_MOUSE_XAXIS},
    {CFGT_Int,      "mouse_axis_y",      &mouse_yaxis,  CFGDEF_MOUSE_YAXIS},
    {CFGT_Int,      "mouse_sens_x",      &mouse_xsens,  CFGDEF_MOUSESENSITIVITY},
    {CFGT_Int,      "mouse_sens_y",      &mouse_ysens,  CFGDEF_MOUSESENSITIVITY},

    // -ACB- 1998/09/06 Two-stage turning & Speed controls added
    {CFGT_Int,      "var_turnspeed",     &var_turnspeed,    CFGDEF_TURNSPEED},
    {CFGT_Int,      "var_mlookspeed",    &var_mlookspeed,   CFGDEF_MLOOKSPEED},
    {CFGT_Int,      "var_forwardspeed",  &var_forwardspeed, CFGDEF_FORWARDMOVESPEED},
    {CFGT_Int,      "var_sidespeed",     &var_sidespeed,    CFGDEF_SIDEMOVESPEED},
    {CFGT_Int,      "var_flyspeed",      &var_flyspeed,     CFGDEF_SIDEMOVESPEED},
	{CFGT_Int, 		"var_triggerthreshold", &var_triggerthreshold, CFGDEF_TRIGGERTHRESHOLD},

    {CFGT_Int,      "joystick_device",   &joystick_device, 1},
    {CFGT_Int,      "joy_axis1",         &joy_axis[0],    7},
    {CFGT_Int,      "joy_axis2",         &joy_axis[1],    6},
    {CFGT_Int,      "joy_axis3",         &joy_axis[2],    1},
    {CFGT_Int,      "joy_axis4",         &joy_axis[3],    4},
    {CFGT_Int,      "joy_axis5",         &joy_axis[4],    AXIS_DISABLE},
    {CFGT_Int,      "joy_axis6",         &joy_axis[5],    AXIS_DISABLE},

    {CFGT_Int,      "screen_hud",        &screen_hud,     CFGDEF_SCREEN_HUD},
    {CFGT_Int,      "save_page",         &save_page, 0},
    {CFGT_Boolean,  "png_scrshots",      &png_scrshots,   CFGDEF_PNG_SCRSHOTS},

	// -------------------- VARS --------------------

	{CFGT_Boolean,  "var_obituaries",    &var_obituaries, 1},
	{CFGT_Boolean,  "var_cache_sfx",    &var_cache_sfx, 1},

	// -------------------- KEYS --------------------

    {CFGT_Key,      "key_right",         &key_right,      CFGDEF_KEY_RIGHT},
    {CFGT_Key,      "key_left",          &key_left,       CFGDEF_KEY_LEFT},
    {CFGT_Key,      "key_up",            &key_up,         CFGDEF_KEY_UP},
    {CFGT_Key,      "key_down",          &key_down,       CFGDEF_KEY_DOWN},
    {CFGT_Key,      "key_lookup",        &key_lookup,     CFGDEF_KEY_LOOKUP},
    {CFGT_Key,      "key_lookdown",      &key_lookdown,   CFGDEF_KEY_LOOKDOWN},
    {CFGT_Key,      "key_lookcenter",    &key_lookcenter, CFGDEF_KEY_LOOKCENTER},

    // -ES- 1999/03/28 Zoom Key
    {CFGT_Key,      "key_zoom",          &key_zoom,        CFGDEF_KEY_ZOOM},
    {CFGT_Key,      "key_strafeleft",    &key_strafeleft,  CFGDEF_KEY_STRAFELEFT},
    {CFGT_Key,      "key_straferight",   &key_straferight, CFGDEF_KEY_STRAFERIGHT},

    // -ACB- for -MH- 1998/07/02 Flying Keys
    {CFGT_Key,      "key_flyup",         &key_flyup,      CFGDEF_KEY_FLYUP},
    {CFGT_Key,      "key_flydown",       &key_flydown,    CFGDEF_KEY_FLYDOWN},

    {CFGT_Key,      "key_fire",          &key_fire,       CFGDEF_KEY_FIRE},
    {CFGT_Key,      "key_use",           &key_use,        CFGDEF_KEY_USE},
    {CFGT_Key,      "key_strafe",        &key_strafe,     CFGDEF_KEY_STRAFE},
    {CFGT_Key,      "key_speed",         &key_speed,      CFGDEF_KEY_SPEED},
    {CFGT_Key,      "key_autorun",       &key_autorun,    CFGDEF_KEY_AUTORUN},
    {CFGT_Key,      "key_nextweapon",    &key_nextweapon, CFGDEF_KEY_NEXTWEAPON},
    {CFGT_Key,      "key_prevweapon",    &key_prevweapon, CFGDEF_KEY_PREVWEAPON},

    {CFGT_Key,      "key_180",           &key_180,        CFGDEF_KEY_180},
    {CFGT_Key,      "key_map",           &key_map,        CFGDEF_KEY_MAP},
    {CFGT_Key,      "key_talk",          &key_talk,       CFGDEF_KEY_TALK},
    {CFGT_Key,      "key_console",       &key_console,    CFGDEF_KEY_CONSOLE},  // -AJA- 2007/08/15.
    {CFGT_Key,      "key_pause",         &key_pause,      KEYD_PAUSE},          // -AJA- 2010/06/13.

    {CFGT_Key,      "key_mlook",         &key_mlook,      CFGDEF_KEY_MLOOK},  // -AJA- 1999/07/27.
    {CFGT_Key,      "key_secondatk",     &key_secondatk,  CFGDEF_KEY_SECONDATK},  // -AJA- 2000/02/08.
    {CFGT_Key,      "key_reload",        &key_reload,     CFGDEF_KEY_RELOAD},  // -AJA- 2004/11/11.
    {CFGT_Key,      "key_action1",       &key_action1,    CFGDEF_KEY_ACTION1},  // -AJA- 2009/09/07
    {CFGT_Key,      "key_action2",       &key_action2,    CFGDEF_KEY_ACTION2},  // -AJA- 2009/09/07

	// -AJA- 2010/06/13: weapon and automap keys
	{CFGT_Key,      "key_weapon1",       &key_weapons[1], '1'},
	{CFGT_Key,      "key_weapon2",       &key_weapons[2], '2'},
	{CFGT_Key,      "key_weapon3",       &key_weapons[3], '3'},
	{CFGT_Key,      "key_weapon4",       &key_weapons[4], '4'},
	{CFGT_Key,      "key_weapon5",       &key_weapons[5], '5'},
	{CFGT_Key,      "key_weapon6",       &key_weapons[6], '6'},
	{CFGT_Key,      "key_weapon7",       &key_weapons[7], '7'},
	{CFGT_Key,      "key_weapon8",       &key_weapons[8], '8'},
	{CFGT_Key,      "key_weapon9",       &key_weapons[9], '9'},
	{CFGT_Key,      "key_weapon0",       &key_weapons[0], '0'},

	{CFGT_Key,      "key_am_up",         &key_am_up,      KEYD_UPARROW},
	{CFGT_Key,      "key_am_down",       &key_am_down,    KEYD_DOWNARROW},
	{CFGT_Key,      "key_am_left",       &key_am_left,    KEYD_LEFTARROW},
	{CFGT_Key,      "key_am_right",      &key_am_right,   KEYD_RIGHTARROW},
	{CFGT_Key,      "key_am_zoomin",     &key_am_zoomin,  '='},
	{CFGT_Key,      "key_am_zoomout",    &key_am_zoomout, '-'},
	{CFGT_Key,      "key_am_follow",     &key_am_follow,  'f'},
	{CFGT_Key,      "key_am_grid",       &key_am_grid,    'g'},
	{CFGT_Key,      "key_am_mark",       &key_am_mark,    'm'},
	{CFGT_Key,      "key_am_clear",      &key_am_clear,   'c'},

	{CFGT_Key,      "key_menu_open",      &key_menu_open,   0},
	{CFGT_Key,      "key_menu_up",      &key_menu_up,   0},
	{CFGT_Key,      "key_menu_down",      &key_menu_down,  0},
	{CFGT_Key,      "key_menu_left",      &key_menu_left,   0},
	{CFGT_Key,      "key_menu_right",      &key_menu_right,   0},
	{CFGT_Key,      "key_menu_select",      &key_menu_select,   0},
	{CFGT_Key,      "key_menu_cancel",      &key_menu_cancel,   0},

	{CFGT_Key,      "key_inv_prev",      &key_inv_prev,   0},
	{CFGT_Key,      "key_inv_use",      &key_inv_use,   0},
	{CFGT_Key,      "key_inv_next",      &key_inv_next,   0},

	{CFGT_Key,      "key_screenshot",      &key_screenshot,   KEYD_F1},
	{CFGT_Key,      "key_save_game",      &key_save_game,   KEYD_F2},
	{CFGT_Key,      "key_load_game",      &key_load_game,  KEYD_F3},
	{CFGT_Key,      "key_sound_controls",      &key_sound_controls,   KEYD_F4},
	{CFGT_Key,      "key_options_menu",      &key_options_menu,   KEYD_F5},
	{CFGT_Key,      "key_quick_save",      &key_quick_save,   KEYD_F6},
	{CFGT_Key,      "key_end_game",      &key_end_game,   KEYD_F7},
	{CFGT_Key,      "key_message_toggle",      &key_message_toggle,   KEYD_F8},
	{CFGT_Key,      "key_quick_load",      &key_quick_load,   KEYD_F9},
	{CFGT_Key,      "key_quit_edge",      &key_quit_edge,   KEYD_F10},
	{CFGT_Key,      "key_gamma_toggle",      &key_gamma_toggle,   KEYD_F11},
	{CFGT_Key,      "key_show_players",      &key_show_players,   KEYD_F12},
};

static int numdefaults = sizeof(defaults) / sizeof(defaults[0]);

void M_SaveDefaults(void)
{
	edge_version = EDGEVER;

	// -ACB- 1999/09/24 idiot proof checking as required by MSVC
	SYS_ASSERT(! cfgfile.empty());

	FILE *f = EPIFOPEN(cfgfile, "w");
	if (!f)
	{
		I_Warning("Couldn't open config file %s for writing.", cfgfile.u8string().c_str());
		return;  // can't write the file, but don't complain
	}

	// console variables
	CON_WriteVars(f);

	// normal variables
	for (int i = 0; i < numdefaults; i++)
	{
		int v;

		switch (defaults[i].type)
		{
			case CFGT_Int:
				fprintf(f, "%s\t\t%i\n", defaults[i].name, *(int*)defaults[i].location);
				break;

			case CFGT_Boolean:
				fprintf(f, "%s\t\t%i\n", defaults[i].name, *(bool*)defaults[i].location ?1:0);
				break;

			case CFGT_Key:
				v = *(int*)defaults[i].location;
				fprintf(f,  "%s\t\t0x%X\n", defaults[i].name, v);
				break;
		}
	}

	epi::FS_Sync();

	fclose(f);
}


static void SetToBaseValue(default_t *def)
{
	switch (def->type)
	{
		case CFGT_Int:
		case CFGT_Key:
			*(int*)(def->location) = def->defaultvalue;
			break;

		case CFGT_Boolean:
			*(bool*)(def->location) = def->defaultvalue?true:false;
			break;
	}
}

void M_ResetDefaults(int _dummy)
{
	
	for (int i = 0; i < numdefaults; i++)
	{
		// don't reset the first five entries except at startup
		if (done_first_init && i < 5)
			continue;

		SetToBaseValue(defaults + i);
	}

	done_first_init = true;
}

static void ParseConfigBlock(epi::lexer_c& lex)
{
	for (;;)
	{
		std::string key;
		std::string value;

		epi::token_kind_e tok = lex.Next(key);

		if (key == "/") // CVAR keys will start with this, but we need to discard it
			continue;

		if (tok == epi::TOK_EOF)
			return;

		if (tok == epi::TOK_ERROR)
			I_Error("ParseConfig: error parsing file!\n");

		tok = lex.Next(value);

		// The last line of the config writer causes a weird blank key with an EOF value, so just return here
		if (tok == epi::TOK_EOF) 
			return;

		if (tok == epi::TOK_ERROR)
			I_Error("ParseConfig: malformed value for key %s!\n", key.c_str());

		if (tok == epi::TOK_String)
		{
			std::string try_cvar = key;
			try_cvar.append(" ").append(value);
			CON_TryCommand(try_cvar.c_str());
		}
		else if (tok == epi::TOK_Number)
		{
			for (int i = 0; i < numdefaults; i++)
			{
				if (0 == strcmp(key.c_str(), defaults[i].name))
				{
					if (defaults[i].type == CFGT_Boolean)
					{
						*(bool*)defaults[i].location = epi::LEX_Int(value) ? true : false;
					}
					else /* CFGT_Int and CFGT_Key */
					{
						*(int*)defaults[i].location = epi::LEX_Int(value);
					}
					break;
				}
			}
		}
	}
}

static void ParseConfig(const std::string& data)
{
	epi::lexer_c lex(data);

	for (;;)
	{
		std::string section;
		epi::token_kind_e tok = lex.Next(section);

		if (tok == epi::TOK_EOF)
			return;

		// process the block
		ParseConfigBlock(lex);
	}
}

void M_LoadDefaults(void)
{
	// set everything to base values
	M_ResetDefaults(0);

	I_Printf("M_LoadDefaults from %s\n", cfgfile.u8string().c_str());

	epi::file_c* file = FS_Open(cfgfile, epi::file_c::ACCESS_READ);

	if (!file)
	{
		I_Warning("Couldn't open config file %s for reading.\n", cfgfile.u8string().c_str());
		I_Warning("Resetting config to RECOMMENDED values...\n");
		return;
	}
	// load the file into this string
	std::string data = file->ReadText();

	delete file;

	ParseConfig(data);

	return;
}

void M_LoadBranding(void)
{
	epi::file_c* file = FS_Open(brandingfile, epi::file_c::ACCESS_READ);

	// Just use hardcoded values if no branding file present
	if (!file)
		return;

	// load the file into this string
	std::string data = file->ReadText();

	delete file;

	ParseConfig(data);
}

void M_InitMiscConVars(void)
{
	if (argv::Find("hqscale") > 0 || argv::Find("hqall") > 0)
		hq2x_scaling = 3;
	else if (argv::Find("nohqscale") > 0)
		hq2x_scaling = 0;
}


#define PIXEL_RED(pix)  (playpal_data[0][pix][0])
#define PIXEL_GRN(pix)  (playpal_data[0][pix][1])
#define PIXEL_BLU(pix)  (playpal_data[0][pix][2])


void M_ScreenShot(bool show_msg)
{
	const char *extension;

	if (png_scrshots) 
		extension = "png";
	else
		extension = "jpg";

	std::filesystem::path fn;

	// find a file name to save it to
	for (int i = 1; i <= 9999; i++)
	{
		std::string base(epi::STR_Format("shot%02d.%s", i, extension));

		fn = epi::PATH_Join(shot_dir, UTFSTR(base));

		if (! epi::FS_Access(fn, epi::file_c::ACCESS_READ))
		{
			break; // file doesn't exist
		}
	}

	epi::image_data_c *img = new epi::image_data_c(SCREENWIDTH, SCREENHEIGHT, 3);

	RGL_ReadScreen(0, 0, SCREENWIDTH, SCREENHEIGHT, img->PixelAt(0,0));

	// ReadScreen produces a bottom-up image, need to invert it
	img->Invert();

	bool result;

	if (png_scrshots) {
		result = epi::PNG_Save(fn, img);
	} else {
		result = epi::JPEG_Save(fn, img);
	}

	if (show_msg)
	{
		if (result)
			I_Printf("Captured to file: %s\n", fn.u8string().c_str());
		else
			I_Printf("Error saving file: %s\n", fn.u8string().c_str());
	}

	delete img;
}


void M_MakeSaveScreenShot(void)
{
#if 0 /// FIXME:
	// byte* buffer = new byte[SCREENWIDTH*SCREENHEIGHT*4];
	// glReadBuffer(GL_FRONT);
	// glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	// glReadPixels(0, 0, SCREENWIDTH, SCREENHEIGHT, GL_RGB, GL_UNSIGNED_BYTE, buffer);
	// ...
	// delete [] buffer;
#endif

const char *extension = "jpg";

std::filesystem::path filename;
std::string temp(epi::STR_Format("%s/%s.%s", "current", "head", extension));
filename= epi::PATH_Join(save_dir, UTFSTR(temp));

epi::FS_Delete(filename);

epi::image_data_c *img = new epi::image_data_c(SCREENWIDTH, SCREENHEIGHT, 3);

/*
	epi::file_c* file = FS_Open(filename, epi::file_c::ACCESS_READ);
	if (!file)
	{
	I_Warning("Couldn't open image %s for reading.\n", filename.u8string().c_str());
	return;
	}
	else
	{
	epi::image_data_c *tempimg = epi::Image_Load(file);
	}
*/
RGL_ReadScreen(0, 0, SCREENWIDTH, SCREENHEIGHT, img->PixelAt(0,0));

// ReadScreen produces a bottom-up image, need to invert it
img->Invert();

bool result;
result = epi::JPEG_Save(filename, img);

if (result)
	I_Printf("Captured to file: %s\n", filename.u8string().c_str());
else
	I_Printf("Error saving file: %s\n", filename.u8string().c_str());

delete img;


}

//
// Creates the file name "dir/file", or
// just "file" in the given string if it 
// was an absolute address.
//
std::filesystem::path M_ComposeFileName(std::filesystem::path dir, std::filesystem::path file)
{
	if (epi::PATH_IsAbsolute(file))
		return file;
#ifdef _WIN32
	return epi::PATH_Join(dir, file.u32string());
#else
	return epi::PATH_Join(dir, file.string());
#endif
}


epi::file_c *M_OpenComposedEPIFile(std::filesystem::path dir, std::filesystem::path file)
{
	std::filesystem::path fullname = M_ComposeFileName(dir, file);

	return epi::FS_Open(fullname,
		epi::file_c::ACCESS_READ | epi::file_c::ACCESS_BINARY);
}

void M_WarnError(const char *error,...)
{
	// Either displays a warning or produces a fatal error, depending
	// on whether the "-strict" option is used.

	char message_buf[4096];

	message_buf[4095] = 0;

	va_list argptr;

	va_start(argptr, error);
	vsprintf(message_buf, error, argptr);
	va_end(argptr);

	// I hope nobody is printing strings longer than 4096 chars...
	SYS_ASSERT(message_buf[4095] == 0);

	if (strict_errors)
		I_Error("%s", message_buf);
	else if (! no_warnings)
		I_Warning("%s", message_buf);
}

void M_DebugError(const char *error,...)
{
	// Either writes a debug message or produces a fatal error, depending
	// on whether the "-strict" option is used.

	char message_buf[4096];

	message_buf[4095] = 0;

	va_list argptr;

	va_start(argptr, error);
	vsprintf(message_buf, error, argptr);
	va_end(argptr);

	// I hope nobody is printing strings longer than 4096 chars...
	SYS_ASSERT(message_buf[4095] == 0);

	if (strict_errors)
		I_Error("%s", message_buf);
	else if (! no_warnings)
		I_Debugf("%s", message_buf);
}


extern FILE *debugfile; // FIXME

void I_Debugf(const char *message,...)
{
	// Write into the debug file.
	//
	// -ACB- 1999/09/22: From #define to Procedure
	// -AJA- 2001/02/07: Moved here from platform codes.
	//
	if (!debugfile)
		return;

	char message_buf[4096];

	message_buf[4095] = 0;

	// Print the message into a text string
	va_list argptr;

	va_start(argptr, message);
	vsprintf(message_buf, message, argptr);
	va_end(argptr);

	// I hope nobody is printing strings longer than 4096 chars...
	SYS_ASSERT(message_buf[4095] == 0);

	fprintf(debugfile, "%s", message_buf);
	fflush(debugfile);
}


extern FILE *logfile; // FIXME: make file_c and unify with debugfile

void I_Logf(const char *message,...)
{
	if (!logfile)
		return;

	char message_buf[4096];

	message_buf[4095] = 0;

	// Print the message into a text string
	va_list argptr;

	va_start(argptr, message);
	vsprintf(message_buf, message, argptr);
	va_end(argptr);

	// I hope nobody is printing strings longer than 4096 chars...
	SYS_ASSERT(message_buf[4095] == 0);

	fprintf(logfile, "%s", message_buf);
	fflush(logfile);
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
