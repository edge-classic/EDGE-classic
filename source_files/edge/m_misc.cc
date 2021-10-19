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

#include "utility.h"
#include "endianess.h"
#include "file.h"
#include "filesystem.h"
#include "path.h"
#include "str_format.h"

#include "image_data.h"
#include "image_jpeg.h"
#include "image_png.h"

#include "con_main.h"
#include "dm_defs.h"
#include "dm_state.h"
#include "e_input.h"
#include "e_player.h"
#include "hu_draw.h"
#include "hu_stuff.h"  // only for showMessages
#include "m_argv.h"
#include "m_misc.h"
#include "m_option.h"
#include "n_network.h"
#include "p_spec.h"
#include "r_gldefs.h"
#include "s_music.h"  // mus_volume
#include "s_sound.h"
#include "am_map.h"
#include "r_colormap.h"
#include "r_draw.h"
#include "r_modes.h"
#include "r_image.h"
#include "r_wipe.h"
#include "version.h"

#include "defaults.h"

//
// DEFAULTS
//
int monitor_size;

cvar_c m_diskicon;

bool display_disk = false;
int  display_desync = 0;

static const image_c *disk_image = NULL;

bool force_directx = false;
bool force_waveout = false;

unsigned short save_screenshot[160][100];
bool save_screenshot_valid = false;

int var_sample_rate = 0;
int var_sound_bits = 0;
int var_sound_stereo = 0;
int var_mix_channels = 0;
int var_quiet_factor = 0;
int var_timid_factor = 0;

static int edge_version;
static bool done_first_init = false;

extern int joystick_device;



static default_t defaults[] =
{
    {CFGT_Int,		"edge_version",		 &edge_version,	  0},
    {CFGT_Int,		"screenwidth",		 &SCREENWIDTH,	  CFGDEF_SCREENWIDTH},
    {CFGT_Int,		"screenheight",		 &SCREENHEIGHT,	  CFGDEF_SCREENHEIGHT},
    {CFGT_Int,		"screendepth",		 &SCREENBITS,	  CFGDEF_SCREENBITS},
    {CFGT_Boolean,	"fullscreen",		 &FULLSCREEN,	  CFGDEF_FULLSCREEN},
    {CFGT_Boolean,	"directx",			 &force_directx,  0},
    {CFGT_Boolean,	"waveout",			 &force_waveout,  0},
    {CFGT_Int,      "usegamma",          &var_gamma,  CFGDEF_CURRENT_GAMMA},
 
    {CFGT_Int,      "sfx_volume",        &sfx_volume,     CFGDEF_SOUND_VOLUME},
    {CFGT_Int,      "music_volume",      &mus_volume,     CFGDEF_MUSIC_VOLUME},
    {CFGT_Int,      "music_device",      &var_music_dev,  CFGDEF_MUSIC_DEVICE},
    {CFGT_Int,      "sample_rate",       &var_sample_rate,  CFGDEF_SAMPLE_RATE},
    {CFGT_Int,      "sound_bits",        &var_sound_bits,   CFGDEF_SOUND_BITS},
    {CFGT_Int,      "sound_stereo",      &var_sound_stereo, CFGDEF_SOUND_STEREO},
    {CFGT_Int,      "mix_channels",      &var_mix_channels, CFGDEF_MIX_CHANNELS},
    {CFGT_Int,      "quiet_factor",      &var_quiet_factor, CFGDEF_QUIET_FACTOR},
    {CFGT_Int,      "timidity_quiet",    &var_timid_factor, CFGDEF_TIMID_FACTOR},

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

    // -KM- 1998/07/21 Save the blood setting
    {CFGT_Boolean,  "blood",             &global_flags.more_blood, CFGDEF_MORE_BLOOD},
    {CFGT_Boolean,  "extra",             &global_flags.have_extra, CFGDEF_HAVE_EXTRA},
    {CFGT_Boolean,  "shadows",           &global_flags.shadows, CFGDEF_SHADOWS},
    {CFGT_Boolean,  "halos",             &global_flags.halos, 0},
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

    {CFGT_Int,      "joystick_device",   &joystick_device, 1},
    {CFGT_Int,      "joy_axis1",         &joy_axis[0],    CFGDEF_JOY_XAXIS},
    {CFGT_Int,      "joy_axis2",         &joy_axis[1],    CFGDEF_JOY_YAXIS},
    {CFGT_Int,      "joy_axis3",         &joy_axis[2],    AXIS_DISABLE},
    {CFGT_Int,      "joy_axis4",         &joy_axis[3],    AXIS_DISABLE},
    {CFGT_Int,      "joy_axis5",         &joy_axis[4],    AXIS_DISABLE},
    {CFGT_Int,      "joy_axis6",         &joy_axis[5],    AXIS_DISABLE},

    {CFGT_Int,      "monitor_size",      &monitor_size,   1},
    {CFGT_Int,      "screen_hud",        &screen_hud,     CFGDEF_SCREEN_HUD},
    {CFGT_Int,      "save_page",         &save_page, 0},
    {CFGT_Boolean,  "png_scrshots",      &png_scrshots,   CFGDEF_PNG_SCRSHOTS},

	// -------------------- VARS --------------------

	{CFGT_Boolean,  "var_obituaries",    &var_obituaries, 1},

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
};


void M_SaveDefaults(void)
{
	edge_version = EDGEVER;

	int numdefaults = sizeof(defaults) / sizeof(defaults[0]);

	// -ACB- 1999/09/24 idiot proof checking as required by MSVC
	SYS_ASSERT(! cfgfile.empty());

	FILE *f = fopen(cfgfile.c_str(), "w");
	if (!f)
	{
		I_Warning("Couldn't open config file %s for writing.", cfgfile.c_str());
		return;  // can't write the file, but don't complain
	}

	// console variables
	for (int k = 0; all_cvars[k].name; k++)
	{
		cvar_c *var = all_cvars[k].var;

		if (strchr(all_cvars[k].flags, 'c'))
			fprintf(f, "/%s\t\"%s\"\n", all_cvars[k].name, var->str);
	}

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
	int numdefaults = sizeof(defaults) / sizeof(defaults[0]);

	for (int i = 0; i < numdefaults; i++)
	{
		// don't reset the first five entries except at startup
		if (done_first_init && i < 5)
			continue;

		SetToBaseValue(defaults + i);
	}

	done_first_init = true;
}


void M_LoadDefaults(void)
{
	int i;

	// set everything to base values
	int numdefaults = sizeof(defaults) / sizeof(defaults[0]);

	M_ResetDefaults(0);

	I_Printf("M_LoadDefaults from %s\n", cfgfile.c_str());

	// read the file in, overriding any set defaults
	FILE *f = fopen(cfgfile.c_str(), "r");

	if (! f)
	{
		I_Warning("Couldn't open config file %s for reading.\n", cfgfile.c_str());
		I_Warning("Resetting config to RECOMMENDED values...\n");
		return;
	}

	while (!feof(f))
	{
		char def[80];
		char strparm[100];

		int parm;

		std::string newstr;
		bool isstring = false;

		if (fscanf(f, "%79s %[^\n]\n", def, strparm) != 2)
			continue;

		// console var?
		if (def[0] == '/')
		{
			std::string con_line;

			con_line += (def+1);
			con_line += " ";
			con_line += strparm;

			CON_TryCommand(con_line.c_str());
			continue;
		}

		if (strparm[0] == '"')
		{
			// get a string default
			isstring = true;
			// overwrite the last "
			strparm[strlen(strparm) - 1] = 0;
			// skip the first "
			newstr = std::string(strparm + 1);
		}
		else if (strparm[0] == '0' && strparm[1] == 'x')
			sscanf(strparm + 2, "%x", &parm);
		else
			sscanf(strparm, "%i", &parm);

		for (i = 0; i < numdefaults; i++)
		{
			if (0 == strcmp(def, defaults[i].name))
			{
				if (defaults[i].type == CFGT_Boolean)
				{
					*(bool*)defaults[i].location = parm?true:false;
				}
				else /* CFGT_Int and CFGT_Key */
				{
					*(int*)defaults[i].location = parm;
				}
				break;
			}
		}
	}

	fclose(f);

	if (edge_version == 0)
	{
		// config file is from an older version (< 1.31)
		// Hence fix some things up here...

		key_console = KEYD_TILDE;
	}

	return;
}


void M_InitMiscConVars(void)
{
	if (M_CheckParm("-hqscale") || M_CheckParm("-hqall"))
		hq2x_scaling = 3;
	else if (M_CheckParm("-nohqscale"))
		hq2x_scaling = 0;
}


void M_DisplayDisk(void)
{
	/* displays disk icon during loading... */

	if (!m_diskicon.d || !display_disk)
		return;
   
	if (!disk_image)
		disk_image = W_ImageLookup("STDISK");

	// reset flag
	display_disk = false;

	float w = IM_WIDTH(disk_image);
	float h = IM_HEIGHT(disk_image);

	HUD_StretchImage(314 - w, 164 - h, w, h, disk_image);
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

	std::string fn;

	// find a file name to save it to
	for (int i = 1; i <= 9999; i++)
	{
		std::string base(epi::STR_Format("shot%02d.%s", i, extension));

		fn = epi::PATH_Join(shot_dir.c_str(), base.c_str());
  
		if (! epi::FS_Access(fn.c_str(), epi::file_c::ACCESS_READ))
        {
			break; // file doesn't exist
        }
	}

	FILE *fp = fopen(fn.c_str(), "wb");
	if (fp == NULL)
	{
		if (show_msg)
			I_Printf("Unable to create file: %s\n", fn.c_str());

		return;
	}

	epi::image_data_c *img = new epi::image_data_c(SCREENWIDTH, SCREENHEIGHT, 3);

	RGL_ReadScreen(0, 0, SCREENWIDTH, SCREENHEIGHT, img->PixelAt(0,0));

	bool result;
    if (png_scrshots) {
        result = epi::PNG_Save(fp, img);
	} else {
        result = epi::JPEG_Save(fp, img);
	}

	if (show_msg)
	{
		if (result)
			I_Printf("Captured to file: %s\n", fn.c_str());
		else
			I_Printf("Error saving file: %s\n", fn.c_str());
	}

	delete img;

	fclose(fp);
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

#if 0  // OLD SOFTWARE VERSION
	int x, y;
	int ax, ay;

	byte pixel;
	unsigned short rgb;
	truecol_info_t colinfo;

	save_screenshot_valid = true;

	I_GetTruecolInfo(&colinfo);

	switch (main_scr->bytepp)
	{
		case 1:
		{
			byte *top = (byte *) main_scr->data;

			top += viewwindowy * main_scr->pitch + viewwindowx;

			for (y=0; y < 100; y++)
				for (x=0; x < 160; x++)    
				{
					ax = x * viewwindowwidth  / 160;
					ay = y * viewwindowheight / 100;

					SYS_ASSERT(0 <= ax && ax < SCREENWIDTH);
					SYS_ASSERT(0 <= ay && ay < SCREENHEIGHT);

					pixel = top[ay * main_scr->pitch + ax];

					rgb = ((PIXEL_RED(pixel) & 0xF8) << 7) |
						((PIXEL_GRN(pixel) & 0xF8) << 2) |
						((PIXEL_BLU(pixel) & 0xF8) >> 3);
        
					save_screenshot[x][y] = rgb;
				}
		}
		break;

		case 2:
		{
			unsigned short *top = (unsigned short *) main_scr->data;

			top += viewwindowy * main_scr->pitch/2 + viewwindowx;

			for (y=0; y < 100; y++)
				for (x=0; x < 160; x++)    
				{
					ax = x * viewwindowwidth  / 160;
					ay = y * viewwindowheight / 100;

					SYS_ASSERT(0 <= ax && ax < SCREENWIDTH);
					SYS_ASSERT(0 <= ay && ay < SCREENHEIGHT);

					rgb = top[ay * main_scr->pitch/2 + ax];

					// hack !
					if (colinfo.green_bits == 6)
						rgb = ((rgb & 0xFFC0) >> 1) | (rgb & 0x001F);
        
					save_screenshot[x][y] = rgb;
				}
		}
		break;
	}
#endif
}

//
// Creates the file name "dir/file", or
// just "file" in the given string if it 
// was an absolute address.
//
std::string M_ComposeFileName(const char *dir, const char *file)
{
	if (epi::PATH_IsAbsolute(file))
		return std::string(file);

	return epi::PATH_Join(dir, file);
}


epi::file_c *M_OpenComposedEPIFile(const char *dir, const char *file)
{
	std::string fullname = M_ComposeFileName(dir, file);

	return epi::FS_Open(fullname.c_str(),
		epi::file_c::ACCESS_READ | epi::file_c::ACCESS_BINARY);
}

//
// Loads file into memory. This sets a pointer to the data and
// the length.
//
// NOTE: The data must be freed by delete[] when not used.
//
// Returns NULL on failure.
//
// -ACB- 2000/01/08 Written
// -ES-  2000/06/12 Now returns the allocated pointer, or NULL on failure.
//
byte* M_GetFileData(const char *filename, int *length)
{
	FILE *lumpfile;
	byte *data;

	// Sanity Checks..
	SYS_ASSERT(filename);
	SYS_ASSERT(length);

	lumpfile = fopen(filename, "rb");  
	if (!lumpfile)
	{
		I_Warning("M_GetFileData: Cannot open '%s'\n", filename);
		return NULL;
	}

	fseek(lumpfile, 0, SEEK_END);                   // get the end of the file
	(*length) = ftell(lumpfile);                    // get the size
	fseek(lumpfile, 0, SEEK_SET);                   // reset to beginning

	data = new byte[*length];						// malloc the size
	fread(data, sizeof(char), (*length), lumpfile); // read file
	fclose(lumpfile);                               // close the file

	return data;
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


//
// Just like L_CompareTimeStamps above, but give the filenames.
//
int L_CompareFileTimes(const char *A, const char *B)
{
	epi::timestamp_c A_time;
	epi::timestamp_c B_time;

	if (! FS_GetModifiedTime(A, A_time))
		I_Error("AddFile: I_GetModifiedTime failed on %s\n", A);

	if (! FS_GetModifiedTime(B, B_time))
		I_Error("AddFile: I_GetModifiedTime failed on %s\n", B);

	return A_time.Cmp(B_time);
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
