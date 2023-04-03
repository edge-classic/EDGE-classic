//----------------------------------------------------------------------------
//  EDGE Option Menu Modification
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
// Original Author: Chi Hoang
//
// -ACB- 1998/06/15 All functions are now m_* to follow doom standard.
//
// -MH-  1998/07/01 Shoot Up/Down becomes "True 3D Gameplay"
//                  Added keys for fly up and fly down
//
// -KM-  1998/07/10 Used better names :-) (Controls Menu)
//
// -ACB- 1998/07/10 Print screen is now possible in this menu
//
// -ACB- 1998/07/12 Removed Lost Soul/Spectre Ability Menu Entries
//
// -ACB- 1998/07/15 Changed menu structure for graphic titles
//
// -ACB- 1998/07/30 Remove M_SetRespawn and the newnmrespawn &
//                  respawnmonsters. Used new respawnsetting variable.
//
// -ACB- 1998/08/10 Edited the menu's to reflect the fact that currmap
//                  flags can prevent changes.
//
// -ES-  1998/08/21 Added resolution options
//
// -ACB- 1998/08/29 Modified Resolution menus for user-friendlyness
//
// -ACB- 1998/09/06 MouseOptions renamed to AnalogueOptions
//
// -ACB- 1998/09/11 Cleaned up and used new white text colour for menu
//                  selections
//
// -KM- 1998/11/25 You can scroll backwards through the resolution list!
//
// -ES- 1998/11/28 Added faded teleportation option
//
// -ACB- 1999/09/19 Removed All CD Audio References.
//
// -ACB- 1999/10/11 Reinstated all sound volume controls.
//
// -ACB- 1999/11/19 Reinstated all music volume controls.
//
// -ACB- 2000/03/11 All menu functions now have the keypress passed
//                  that called them.
//
// -ACB- 2000/03/12 Menu resolution hack now allow user to cycle both
//                  ways through the resolution list.
//                  
// -AJA- 2001/07/26: Reworked colours, key config, and other code.
//
//

#include "i_defs.h"

#include "font.h"
#include "path.h"
#include "str_util.h"

#include "playlist.h"

#include "main.h"

#include "dm_state.h"
#include "e_input.h"
#include "g_game.h"
#include "hu_draw.h"
#include "hu_stuff.h"
#include "hu_style.h"
#include "i_ctrl.h"
#include "m_menu.h"
#include "m_misc.h"
#include "m_netgame.h"
#include "m_option.h"
#include "n_network.h"
#include "p_local.h"
#include "r_misc.h"
#include "r_gldefs.h"
#include "s_blit.h"
#include "s_cache.h"
#include "s_sound.h"
#include "s_music.h"
#include "am_map.h"
#include "r_draw.h"
#include "r_modes.h"
#include "r_colormap.h"
#include "r_image.h"
#include "w_wad.h"
#include "r_wipe.h"
#include "s_opl.h"
#include "s_fluid.h"
#include "s_fmm.h"

#include "i_ctrl.h"

#include "defaults.h"


int option_menuon = 0;
bool fkey_menu = false;

extern cvar_c m_language;
extern cvar_c r_crosshair;
extern cvar_c r_crosscolor;
extern cvar_c r_crosssize;
extern cvar_c s_genmidi;
extern cvar_c s_soundfont;
extern cvar_c r_overlay;
extern cvar_c g_erraticism;
extern cvar_c r_doubleframes;
extern cvar_c g_mbf21compat;
extern cvar_c r_culling;
extern cvar_c r_culldist;
extern cvar_c r_cullfog;
extern cvar_c g_cullthinkers;
extern cvar_c r_maxdlights;
extern cvar_c v_sync;
extern cvar_c g_bobbing;
extern cvar_c v_secbright;
extern cvar_c v_gamma;

static int menu_crosshair;
static int menu_crosscolor;
static int menu_crosssize;
static int monitor_size;

extern int joystick_device;

extern int entry_playing;

//submenus
static void M_KeyboardOptions(int keypressed);
static void M_VideoOptions(int keypressed);
static void M_GameplayOptions(int keypressed);
static void M_PerformanceOptions(int keypressed);
static void M_AnalogueOptions(int keypressed);
static void M_SoundOptions(int keypressed);

static void M_Key2String(int key, char *deststring);

// -ACB- 1998/08/09 "Does Map allow these changes?" procedures.
static void M_ChangeMonsterRespawn(int keypressed);
static void M_ChangeItemRespawn(int keypressed);
static void M_ChangeTrue3d(int keypressed);
static void M_ChangeAutoAim(int keypressed);
static void M_ChangeFastparm(int keypressed);
static void M_ChangeRespawn(int keypressed);
static void M_ChangePassMissile(int keypressed);
static void M_ChangeCrossHair(int keypressed);
static void M_ChangeCrossColor(int keypressed);
static void M_ChangeCrossSize(int keypressed);

static void M_ChangeMaxDLights(int keypressed);
static void M_ChangeCulling(int keypressed);
static void M_ChangeCullDist(int keypressed);
static void M_ChangeCullFog(int keypressed);
static void M_ChangeThinkerCulling(int keypressed);
static void M_ChangeMBF21Compat(int keypressed);
static void M_ChangeBobbing(int keypressed);
static void M_ChangeBlood(int keypressed);
static void M_ChangeMLook(int keypressed);
static void M_ChangeJumping(int keypressed);
static void M_ChangeCrouching(int keypressed);
static void M_ChangeExtra(int keypressed);
static void M_ChangeSecBright(int keypressed);
static void M_ChangeGamma(int keypressed);
static void M_ChangeMonitorSize(int keypressed);
static void M_ChangeKicking(int keypressed);
static void M_ChangeWeaponSwitch(int keypressed);
static void M_ChangeMipMap(int keypressed);
static void M_SaveOverlay(int keypressed);
static void M_ChangeDLights(int keypressed);
static void M_ChangePCSpeakerMode(int keypressed);

// -ES- 1998/08/20 Added resolution options
// -ACB- 1998/08/29 Moved to top and tried different system

static void M_ResOptDrawer(style_c *style, int topy, int bottomy, int dy, int centrex);
static void M_ResolutionOptions(int keypressed);
static void M_OptionSetResolution(int keypressed);
static void M_ChangeResSize(int keypressed);
static void M_ChangeResFull(int keypressed);

 void M_HostNetGame(int keypressed);
void M_JoinNetGame(int keypressed);

static void M_LanguageDrawer(int x, int y, int deltay);
static void M_ChangeLanguage(int keypressed);
static void M_ChangeMIDIPlayer(int keypressed);
static void M_ChangeSoundfont(int keypressed);
static void M_ChangeGENMIDI(int keypressed);
static void M_ChangeErraticism(int keypressed);
static void M_ChangeDoubleFrames(int keypressed);
static void M_ChangeVSync(int keypressed);
static void InitMonitorSize();

static char YesNo[]     = "Off/On";  // basic on/off
static char CrossH[]    = "None/Dot/Angle/Plus/Spiked/Thin/Cross/Carat/Circle/Double";
static char Respw[]     = "Teleport/Resurrect";  // monster respawning
static char Axis[]      = "Off/+Turn/-Turn/+MLook/-MLook/+Forward/-Forward/+Strafe/-Strafe/+Fly/-Fly/Left Trigger/Right Trigger";
static char JoyDevs[]   = "None/1/2/3/4/5/6";
static char DLMode[]    = "Off/On";
static char JpgPng[]    = "JPEG/PNG";  // basic on/off
static char AAim[]      = "Off/On/Mlook";
static char MipMaps[]   = "None/Good/Best";
static char Details[]   = "Low/Medium/High";
static char Hq2xMode[]  = "Off/UI Only/UI & Sprites/All";
static char Invuls[]    = "Simple/Textured";
static char MonitSiz[]  = "5:4/4:3/3:2/16:10/16:9/21:9";
static char VidOverlays[]  = "None/Lines 1x/Lines 2x/Checkered/Vertical/Grill";

// for CVar enums
const char WIPE_EnumStr[] = "None/Melt/Crossfade/Pixelfade/Top/Bottom/Left/Right/Spooky/Doors";

static char StereoNess[]  = "Off/On/Swapped";
static char MixChans[]    = "32/64/96/128/160/192/224/256";

static char CrosshairColor[] = "White/Blue/Green/Cyan/Red/Pink/Yellow/Orange";

static char SecBrights[]  = "-50/-40/-30/-20/-10/Default/+10/+20/+30/+40/+50";

static char DLightMax[]  = "Unlimited/20/40/60/80/100";

// Screen resolution changes
static scrmode_c new_scrmode;

bool splash_screen;

extern std::vector<std::filesystem::path> available_soundfonts;
extern std::vector<std::filesystem::path> available_genmidis;

// -ES- 1998/11/28 Wipe and Faded teleportation options
//static char FadeT[] = "Off/On, flash/On, no flash";


//
//  OPTION STRUCTURES
//

typedef enum
{
	OPT_Plain     = 0,  // 0 means plain text,
	OPT_Switch    = 1,  // 1 is change a switch,
	OPT_Function  = 2,  // 2 is call a function,
	OPT_Slider    = 3,  // 3 is a slider,
	OPT_KeyConfig = 4,  // 4 is a key config,
	OPT_Boolean   = 5,  // 5 is change a boolean switch
	OPT_NumTypes
}
opt_type_e;

typedef struct optmenuitem_s
{
	opt_type_e type;

	char name[48];
	const char *typenames;
  
	int numtypes;
	void *switchvar;
  
	void (*routine)(int keypressed);

	const char *help;
}
optmenuitem_t;


typedef struct menuinfo_s
{
	// array of menu items
	optmenuitem_t *items;
	int item_num;

	style_c **style_var;

	int menu_center;

	// title information
	int title_x;
	char title_name[10];
	const image_c *title_image;

	// current position
	int pos;

	// key config, with left and right sister menus ?
	char key_page[20];
	
	// Printed name test
	const char *name = "DEFAULT";
}
menuinfo_t;

// current menu and position
static menuinfo_t *curr_menu;
static optmenuitem_t *curr_item;
static int curr_key_menu;

static int keyscan;

static style_c *opt_def_style;
//static style_c *keyboard_style;
//static style_c *mouse_style;
//static style_c *gameplay_style;
//static style_c *video_style;
//static style_c *setres_style;



static void M_ChangeMusVol(int keypressed)
{
	S_ChangeMusicVolume();
}

static void M_ChangeSfxVol(int keypressed)
{
	S_ChangeSoundVolume();
}

static void M_ChangeMixChan(int keypressed)
{
	S_ChangeChannelNum();
}

static int M_GetCurrentSwitchValue(optmenuitem_t *item)
{
	int retval = 0;

	switch(item->type)
	{
		case OPT_Boolean:
		{
			retval = *(bool*)item->switchvar ? 1 : 0;
			break;
		}

		case OPT_Switch:
		{
			retval = *(int*)(item->switchvar);
			break;
		}

		default:
		{
			I_Error("M_GetCurrentSwitchValue: Menu item type is not a switch!\n");
			break;
		}
	}

	return retval;
}

//
//  MAIN MENU
//
#define LANGUAGE_POS  9
#define HOSTNET_POS   12

static optmenuitem_t mainoptions[] =
{
	{OPT_Function, "Key Bindings", NULL,  0, NULL, M_KeyboardOptions, "Controls"},
	{OPT_Function, "Mouse / Controller",  NULL,  0, NULL, M_AnalogueOptions, "AnalogueOptions"},
	{OPT_Function, "Gameplay Options",  NULL,  0, NULL, M_GameplayOptions, "GameplayOptions"},
	{OPT_Function, "Performance Options",  NULL,  0, NULL, M_PerformanceOptions, "PerformanceOptions"},
	{OPT_Plain,    "",                  NULL,  0, NULL, NULL, NULL},
	{OPT_Function, "Sound Options",     NULL,  0, NULL, M_SoundOptions, "SoundOptions"},
	{OPT_Function, "Video Options",     NULL,  0, NULL, M_VideoOptions, "VideoOptions"},
#ifndef EDGE_WEB
	{OPT_Function, "Screen Options",    NULL,  0, NULL, M_ResolutionOptions, "ChangeRes"},
#endif
	{OPT_Plain,    "",                  NULL,  0, NULL, NULL, NULL},
	{OPT_Function, "Language",          NULL,  0, NULL, M_ChangeLanguage, NULL},
	{OPT_Switch,   "Messages",          YesNo, 2, &showMessages, NULL, "Messages"},
	{OPT_Plain,    "",                  NULL,  0, NULL, NULL, NULL},
	{OPT_Function, "Advanced Start",    NULL,  0, NULL, M_HostNetGame, NULL},
	{OPT_Plain,    "",                  NULL,  0, NULL, NULL, NULL},
	{OPT_Function, "Reset to Defaults", NULL,  0, NULL, M_ResetDefaults, NULL}
};

static menuinfo_t main_optmenu = 
{
	mainoptions, sizeof(mainoptions) / sizeof(optmenuitem_t), 
	&opt_def_style, 164, 108, "M_OPTTTL", NULL, 0, "", language["MenuOptions"]
};

//
//  VIDEO OPTIONS
//
// -ACB- 1998/07/15 Altered menu structure

static optmenuitem_t vidoptions[] =
{
	{OPT_Slider,  "Gamma Adjustment",    NULL,  21,  &v_gamma.d, M_ChangeGamma, NULL},
	{OPT_Switch,  "Sector Brightness",    SecBrights,  11,  &v_secbright.d, M_ChangeSecBright, NULL},
	{OPT_Switch,  "Framerate Target", "35 FPS/70 FPS", 2, &r_doubleframes.d, M_ChangeDoubleFrames, NULL},
	{OPT_Switch,  "Smoothing",         YesNo, 2, &var_smoothing, M_ChangeMipMap, NULL},
	{OPT_Switch,  "H.Q.2x Scaling", Hq2xMode, 4, &hq2x_scaling, M_ChangeMipMap, NULL},
	{OPT_Switch,  "Dynamic Lighting", DLMode, 2, &use_dlights, M_ChangeDLights, NULL},
	{OPT_Switch,  "Detail Level",   Details,  3, &detail_level, M_ChangeMipMap, NULL},
	{OPT_Switch,  "Mipmapping",     MipMaps,  3, &var_mipmapping, M_ChangeMipMap, NULL},
	{OPT_Switch,  "Overlay",  		VidOverlays, 6, &r_overlay.d, M_SaveOverlay, NULL},
	{OPT_Switch,  "Crosshair",       CrossH, 10, &menu_crosshair, M_ChangeCrossHair, NULL},
	{OPT_Switch,  "Crosshair Color", CrosshairColor,  8, &menu_crosscolor, M_ChangeCrossColor, NULL},
	{OPT_Slider,  "Crosshair Size",    NULL,  6,  &menu_crosssize, M_ChangeCrossSize, NULL},
	{OPT_Boolean, "Map Rotation",    YesNo,   2, &rotatemap, NULL, NULL},
	{OPT_Switch,  "Teleport Flash",  YesNo,   2, &telept_flash, NULL, NULL},
	{OPT_Switch,  "Invulnerability", Invuls, NUM_INVULFX,  &var_invul_fx, NULL, NULL},
#ifndef EDGE_WEB
	{OPT_Switch,  "Wipe method",     WIPE_EnumStr, WIPE_NUMWIPES, &wipe_method, NULL, NULL},
#endif
	{OPT_Boolean, "Screenshot Format", JpgPng, 2, &png_scrshots, NULL, NULL},
	{OPT_Boolean, "Splash Screen",    YesNo,   2, &splash_screen, NULL, NULL},
	{OPT_Switch, "Animated Liquid Type", "Vanilla/SMMU/SMMU+Swirl/Parallax",   4, &swirling_flats, NULL, NULL}
	

#if 0  // TEMPORARILY DISABLED (we need an `Advanced Options' menu)
	{OPT_Switch,  "Teleportation effect", WIPE_EnumStr, WIPE_NUMWIPES, 
                                              &telept_effect, NULL, NULL},

    {OPT_Switch,  "Reverse effect", YesNo, 2, &telept_reverse, NULL, NULL},
    {OPT_Switch,  "Reversed wipe",  YesNo, 2, &wipe_reverse, NULL, NULL},
#endif
};

static menuinfo_t video_optmenu = 
{
	vidoptions, sizeof(vidoptions) / sizeof(optmenuitem_t),
	&opt_def_style, 150, 77, "M_VIDEO", NULL, 0, "", language["MenuVideo"]
};

//
//  SCREEN OPTIONS MENU
//
static optmenuitem_t resoptions[] =
{
	{OPT_Plain,    "",          NULL, 0, NULL, NULL, NULL},
	{OPT_Switch,  "V-Sync", "Off/Standard/Adaptive", 3, &v_sync.d, M_ChangeVSync, "Will fallback to Standard if Adaptive is not supported"},
	{OPT_Switch,  "Aspect Ratio",  MonitSiz,  6, &monitor_size, M_ChangeMonitorSize, "Only applies to Fullscreen/Borderless Fullscreen Modes"},
	{OPT_Function, "New Mode",  NULL, 0, NULL, M_ChangeResFull, NULL},
	{OPT_Function, "New Resolution",  NULL, 0, NULL, M_ChangeResSize, NULL},
	{OPT_Function, "Apply Mode/Resolution", NULL, 0, NULL, M_OptionSetResolution, NULL},
	{OPT_Plain,    "",          NULL, 0, NULL, NULL, NULL},
	{OPT_Plain,    "",          NULL, 0, NULL, NULL, NULL},
	{OPT_Plain,    "",          NULL, 0, NULL, NULL, NULL}
};

static menuinfo_t res_optmenu = 
{
	resoptions, sizeof(resoptions) / sizeof(optmenuitem_t),
	&opt_def_style, 150, 77, "M_SETRES", NULL, 3, "", language["MenuResolution"]
};

//
//  MOUSE OPTIONS
//
// -ACB- 1998/06/15 Added new mouse menu
// -KM- 1998/09/01 Changed to an analogue menu.  Must change those names
// -ACB- 1998/07/15 Altered menu structure
//
static optmenuitem_t analogueoptions[] =
{
	{OPT_Switch,   "Mouse X Axis",       Axis, 11, &mouse_xaxis, NULL, NULL},
	{OPT_Switch,   "Mouse Y Axis",       Axis, 11, &mouse_yaxis, NULL, NULL},
	{OPT_Slider,   "X Sensitivity",      NULL, 16, &mouse_xsens, NULL, NULL},
	{OPT_Slider,   "Y Sensitivity",      NULL, 16, &mouse_ysens, NULL, NULL},
	{OPT_Plain,    "",                   NULL, 0,  NULL, NULL, NULL},

	{OPT_Switch,   "Joystick Device", JoyDevs, 7,  &joystick_device, NULL, NULL},
	{OPT_Switch,   "First Axis",         Axis, 13, &joy_axis[0], NULL, NULL},
	{OPT_Switch,   "Second Axis",        Axis, 13, &joy_axis[1], NULL, NULL},
	{OPT_Switch,   "Third Axis",         Axis, 13, &joy_axis[2], NULL, NULL},
	{OPT_Switch,   "Fourth Axis",        Axis, 13, &joy_axis[3], NULL, NULL},
	{OPT_Switch,   "Fifth Axis",         Axis, 13, &joy_axis[4], NULL, NULL},
	{OPT_Switch,   "Sixth Axis",         Axis, 13, &joy_axis[5], NULL, NULL},

	{OPT_Plain,    "",                   NULL, 0,  NULL, NULL, NULL},
	{OPT_Slider,   "Turning Speed",      NULL, 12, &var_turnspeed,    NULL, NULL},
	{OPT_Slider,   "MLook Speed",        NULL, 12, &var_mlookspeed,   NULL, NULL},
	{OPT_Slider,   "Forward Move Speed", NULL, 8,  &var_forwardspeed, NULL, NULL},
	{OPT_Slider,   "Side Move Speed",    NULL, 8,  &var_sidespeed,    NULL, NULL},
	{OPT_Slider,   "Trigger Sensitivity",    NULL, 7,  &var_triggerthreshold,    NULL, NULL},
};

static menuinfo_t analogue_optmenu = 
{
	analogueoptions, sizeof(analogueoptions) / sizeof(optmenuitem_t),
	&opt_def_style, 150, 75, "M_MSETTL", NULL, 0, "", language["MenuMouse"]
};

//
//  SOUND OPTIONS
//
// -AJA- 2007/03/14 Added new sound menu
//
static optmenuitem_t soundoptions[] =
{
	{OPT_Slider,  "Sound Volume", NULL, SND_SLIDER_NUM, &sfx_volume, M_ChangeSfxVol, NULL},
	{OPT_Slider,  "Music Volume", NULL, SND_SLIDER_NUM, &mus_volume, M_ChangeMusVol, NULL},
	{OPT_Plain,   "",             NULL, 0,  NULL, NULL, NULL},
	{OPT_Switch,  "Stereo",       StereoNess, 3,  &var_sound_stereo, NULL, "NeedRestart"},
	{OPT_Plain,   "",             NULL, 0,  NULL, NULL, NULL},
	{OPT_Switch,  "MIDI Player",  "Fluidlite (Soundfont)/YMFM (OPL3)/FMMIDI (OPNA)", 3,  &var_midi_player, M_ChangeMIDIPlayer, NULL},
	{OPT_Function, "Fluidlite Soundfont", NULL,  0, NULL, M_ChangeSoundfont, "Warning! SF3 Soundfonts may have long loading times!"},
	{OPT_Function, "YMFM Instrument Bank", NULL,  0, NULL, M_ChangeGENMIDI, NULL},
	{OPT_Boolean, "PC Speaker Mode", YesNo, 2,  &var_pc_speaker_mode, M_ChangePCSpeakerMode, "Will affect both Sounds and Music"},
	{OPT_Plain,   "",             NULL, 0,  NULL, NULL, NULL},
	{OPT_Boolean, "Dynamic Reverb",       YesNo, 2, &dynamic_reverb, NULL, NULL},
	{OPT_Plain,   "",                NULL, 0,  NULL, NULL, NULL},
	{OPT_Switch,  "Mix Channels",    MixChans,  8, &var_mix_channels, M_ChangeMixChan, NULL},
	{OPT_Boolean, "Precache SFX",       YesNo, 2, &var_cache_sfx, NULL, "NeedRestart"},
	{OPT_Plain,   "",                NULL, 0,  NULL, NULL, NULL},
};

static menuinfo_t sound_optmenu = 
{
	soundoptions, sizeof(soundoptions) / sizeof(optmenuitem_t),
	&opt_def_style, 150, 75, "M_SFXOPT", NULL, 0, "", language["MenuSound"]
};

//
//  F4 SOUND OPTIONS
//
//
static optmenuitem_t f4soundoptions[] =
{
	{OPT_Slider,  "Sound Volume", NULL, SND_SLIDER_NUM, &sfx_volume, M_ChangeSfxVol, NULL},
	{OPT_Slider,  "Music Volume", NULL, SND_SLIDER_NUM, &mus_volume, M_ChangeMusVol, NULL},
};

static menuinfo_t f4sound_optmenu = 
{
	f4soundoptions, sizeof(f4soundoptions) / sizeof(optmenuitem_t),
	&opt_def_style, 150, 75, "M_SFXOPT", NULL, 0, "", language["MenuSound"]
};

//
//  GAMEPLAY OPTIONS
//
// -ACB- 1998/07/15 Altered menu structure
// -KM- 1998/07/21 Change blood to switch
//
static optmenuitem_t playoptions[] =
{
	{OPT_Boolean, "MBF21 Map Compatibility", YesNo, 2, 
     &g_mbf21compat.d, M_ChangeMBF21Compat, "Toggle support for MBF21 lines and sectors"},

	{OPT_Boolean, "Pistol Starts",         YesNo, 2, 
     &pistol_starts, NULL, NULL},

	{OPT_Switch,  "Bobbing", "Full/Head Only/Weapon Only/None", 4, 
     &g_bobbing.d, M_ChangeBobbing, NULL},

	{OPT_Boolean, "Mouse Look",         YesNo, 2, 
     &global_flags.mlook, M_ChangeMLook, NULL},

	{OPT_Switch,  "Autoaim",         AAim, 3, 
     &global_flags.autoaim, M_ChangeAutoAim, NULL},

	{OPT_Boolean, "Jumping",            YesNo, 2, 
     &global_flags.jump, M_ChangeJumping, NULL},

	{OPT_Boolean, "Crouching",          YesNo, 2, 
     &global_flags.crouch, M_ChangeCrouching, NULL},

	{OPT_Boolean, "Weapon Kick",        YesNo, 2, 
     &global_flags.kicking, M_ChangeKicking, NULL},

	{OPT_Boolean, "Weapon Auto-Switch", YesNo, 2, 
     &global_flags.weapon_switch, M_ChangeWeaponSwitch, NULL},

	{OPT_Boolean, "Obituary Messages",  YesNo, 2, 
     &var_obituaries, NULL, NULL},

	{OPT_Boolean, "More Blood",         YesNo, 2, 
     &global_flags.more_blood, M_ChangeBlood, "Blood"},

	{OPT_Boolean, "Extras",             YesNo, 2, 
     &global_flags.have_extra, M_ChangeExtra, NULL},

	{OPT_Boolean, "True 3D Gameplay",   YesNo, 2, 
     &global_flags.true3dgameplay, M_ChangeTrue3d, "True3d"},

	{OPT_Boolean, "Shoot-thru Scenery",   YesNo, 2, 
     &global_flags.pass_missile, M_ChangePassMissile, NULL},

	{OPT_Boolean, "Erraticism",   YesNo, 2, 
     &g_erraticism.d, M_ChangeErraticism, "Time only advances when you move or fire"},

	{OPT_Slider,  "Gravity",            NULL, 20, 
     &global_flags.menu_grav, NULL, "Gravity"},

    {OPT_Boolean, "Respawn Enemies",            YesNo, 2, 
     &global_flags.respawn, M_ChangeRespawn, NULL},

	{OPT_Boolean, "Enemy Respawn Mode", Respw, 2, 
     &global_flags.res_respawn, M_ChangeMonsterRespawn, NULL},

	{OPT_Boolean, "Item Respawn",       YesNo, 2, 
     &global_flags.itemrespawn, M_ChangeItemRespawn, NULL},
	
    {OPT_Boolean, "Fast Monsters",      YesNo, 2, 
     &global_flags.fastparm, M_ChangeFastparm, NULL}
};

static menuinfo_t gameplay_optmenu = 
{
	playoptions, sizeof(playoptions) / sizeof(optmenuitem_t),
	&opt_def_style, 160, 46, "M_GAMEPL", NULL, 0, "", language["MenuGameplay"]
};

//
//  PERFORMANCE OPTIONS
//
//
static optmenuitem_t perfoptions[] =
{
	{OPT_Boolean, "Draw Distance Culling", YesNo, 2, 
     &r_culling.d, M_ChangeCulling, NULL},
	{OPT_Switch, "Maximum Draw Distance", "2000/3000/4000/5000/6000/7000/8000", 7, 
     &r_culldist.d, M_ChangeCullDist, "Only effective when Draw Distance Culling is On"},
	{OPT_Switch, "Outdoor Culling Fog Color", "Match Sky/White/Grey/Black", 4, 
     &r_cullfog.d, M_ChangeCullFog, "Only effective when Draw Distance Culling is On"},
	{OPT_Boolean, "Slow Thinkers Over Distance", YesNo, 2, 
     &g_cullthinkers.d, M_ChangeThinkerCulling, "Only recommended for extreme monster/projectile counts"},
	{OPT_Switch, "Maximum Dynamic Lights", DLightMax, 6, 
     &r_maxdlights.d, M_ChangeMaxDLights, "Control how many dynamic lights are rendered per tick"},
};

static menuinfo_t perf_optmenu = 
{
	perfoptions, sizeof(perfoptions) / sizeof(optmenuitem_t),
	&opt_def_style, 160, 46, "M_PRFOPT", NULL, 0, "", language["MenuPerformance"]
};

//
//  KEY CONFIG : MOVEMENT
//
// -ACB- 1998/07/15 Altered menuinfo struct
// -KM- 1998/07/10 Used better names :-)
//
static optmenuitem_t move_keyconfig[] =
{
	{OPT_KeyConfig, "Walk Forward",   NULL, 0, &key_up, NULL, NULL},
	{OPT_KeyConfig, "Walk Backwards", NULL, 0, &key_down, NULL, NULL},
	{OPT_Plain,      "",              NULL, 0, NULL, NULL, NULL},
	{OPT_KeyConfig, "Strafe Left",    NULL, 0, &key_strafeleft, NULL, NULL},
	{OPT_KeyConfig, "Strafe Right",   NULL, 0, &key_straferight, NULL, NULL},
	{OPT_Plain,      "",              NULL, 0, NULL, NULL, NULL},
	{OPT_KeyConfig, "Turn Left",      NULL, 0, &key_left, NULL, NULL},
	{OPT_KeyConfig, "Turn Right",     NULL, 0, &key_right, NULL, NULL},
	{OPT_Plain,      "",              NULL, 0, NULL, NULL, NULL},
	{OPT_KeyConfig, "Up / Jump",      NULL, 0, &key_flyup, NULL, NULL},
	{OPT_KeyConfig, "Down / Crouch",  NULL, 0, &key_flydown, NULL, NULL},
};

static menuinfo_t movement_optmenu = 
{
	move_keyconfig, sizeof(move_keyconfig) / sizeof(optmenuitem_t),
	&opt_def_style, 140, 98, "M_CONTRL", NULL, 0,
	"Movement", language["MenuBinding"]
};

//
//  KEY CONFIG : ATTACK + LOOK
//
// -ACB- 1998/07/15 Altered menuinfo struct
// -ES- 1999/03/28 Added Zoom Key
//
static optmenuitem_t attack_keyconfig[] =
{
	{OPT_KeyConfig, "Primary Attack",   NULL, 0, &key_fire, NULL, NULL},
	{OPT_KeyConfig, "Secondary Attack", NULL, 0, &key_secondatk, NULL, NULL},
	{OPT_KeyConfig, "Next Weapon",      NULL, 0, &key_nextweapon, NULL, NULL},
	{OPT_KeyConfig, "Previous Weapon",  NULL, 0, &key_prevweapon, NULL, NULL},
	{OPT_KeyConfig, "Weapon Reload",    NULL, 0, &key_reload, NULL, NULL},
	{OPT_Plain,     "",                 NULL, 0, NULL, NULL, NULL},
	{OPT_KeyConfig, "Look Up",          NULL, 0, &key_lookup, NULL, NULL},
	{OPT_KeyConfig, "Look Down",        NULL, 0, &key_lookdown, NULL, NULL},
	{OPT_KeyConfig, "Center View",      NULL, 0, &key_lookcenter, NULL, NULL},
	{OPT_KeyConfig, "Mouse Look",       NULL, 0, &key_mlook, NULL, NULL},
	{OPT_KeyConfig, "Zoom in/out",      NULL, 0, &key_zoom, NULL, NULL},
};

static menuinfo_t attack_optmenu = 
{
	attack_keyconfig, sizeof(attack_keyconfig) / sizeof(optmenuitem_t),
	&opt_def_style, 140, 98, "M_CONTRL", NULL, 0,
	"Attack / Look", language["MenuBinding"]
};

//
//  KEY CONFIG : OTHER STUFF
//
static optmenuitem_t other_keyconfig[] =
{
	{OPT_KeyConfig, "Use Item",         NULL, 0, &key_use, NULL, NULL},
	{OPT_KeyConfig, "Strafe",           NULL, 0, &key_strafe, NULL, NULL},
	{OPT_KeyConfig, "Run",              NULL, 0, &key_speed, NULL, NULL},
	{OPT_KeyConfig, "Toggle Autorun",   NULL, 0, &key_autorun, NULL, NULL},
	{OPT_KeyConfig, "180 degree turn",  NULL, 0, &key_180, NULL, NULL},
	{OPT_Plain,     "",                 NULL, 0, NULL, NULL, NULL},
	{OPT_KeyConfig, "Map Toggle",       NULL, 0, &key_map, NULL, NULL},
	{OPT_KeyConfig, "Console",          NULL, 0, &key_console, NULL, NULL},
	{OPT_KeyConfig, "Pause",            NULL, 0, &key_pause, NULL, NULL},
	{OPT_KeyConfig, "Action 1",         NULL, 0, &key_action1, NULL, NULL},
	{OPT_KeyConfig, "Action 2",         NULL, 0, &key_action2, NULL, NULL},

///	{OPT_KeyConfig, "Multiplayer Talk", NULL, 0, &key_talk, NULL, NULL},
};

static menuinfo_t otherkey_optmenu = 
{
	other_keyconfig, sizeof(other_keyconfig) / sizeof(optmenuitem_t),
	&opt_def_style, 140, 98, "M_CONTRL", NULL, 0,
	"Other Keys", language["MenuBinding"]
};

//
//  KEY CONFIG : WEAPONS
//
static optmenuitem_t weapon_keyconfig[] =
{
	{OPT_KeyConfig, "Weapon 1",  NULL, 0, &key_weapons[1], NULL, NULL},
	{OPT_KeyConfig, "Weapon 2",  NULL, 0, &key_weapons[2], NULL, NULL},
	{OPT_KeyConfig, "Weapon 3",  NULL, 0, &key_weapons[3], NULL, NULL},
	{OPT_KeyConfig, "Weapon 4",  NULL, 0, &key_weapons[4], NULL, NULL},
	{OPT_KeyConfig, "Weapon 5",  NULL, 0, &key_weapons[5], NULL, NULL},
	{OPT_Plain,     "",          NULL, 0, NULL, NULL, NULL},
	{OPT_KeyConfig, "Weapon 6",  NULL, 0, &key_weapons[6], NULL, NULL},
	{OPT_KeyConfig, "Weapon 7",  NULL, 0, &key_weapons[7], NULL, NULL},
	{OPT_KeyConfig, "Weapon 8",  NULL, 0, &key_weapons[8], NULL, NULL},
	{OPT_KeyConfig, "Weapon 9",  NULL, 0, &key_weapons[9], NULL, NULL},
	{OPT_KeyConfig, "Weapon 0",  NULL, 0, &key_weapons[0], NULL, NULL},
};

static menuinfo_t weapon_optmenu = 
{
	weapon_keyconfig, sizeof(weapon_keyconfig) / sizeof(optmenuitem_t),
	&opt_def_style, 140, 98, "M_CONTRL", NULL, 0,
	"Weapon Keys", language["MenuBinding"]
};

//
//  KEY CONFIG : AUTOMAP
//
static optmenuitem_t automap_keyconfig[] =
{
	{OPT_KeyConfig, "Pan Up",        NULL, 0, &key_am_up, NULL, NULL},
	{OPT_KeyConfig, "Pan Down",      NULL, 0, &key_am_down, NULL, NULL},
	{OPT_KeyConfig, "Pan Left",      NULL, 0, &key_am_left, NULL, NULL},
	{OPT_KeyConfig, "Pan Right",     NULL, 0, &key_am_right, NULL, NULL},
	{OPT_Plain,     "",              NULL, 0, NULL, NULL, NULL},
	{OPT_KeyConfig, "Follow Mode",   NULL, 0, &key_am_follow, NULL, NULL},
	{OPT_KeyConfig, "Show Grid",     NULL, 0, &key_am_grid, NULL, NULL},
	{OPT_KeyConfig, "Zoom In",       NULL, 0, &key_am_zoomin, NULL, NULL},
	{OPT_KeyConfig, "Zoom Out",      NULL, 0, &key_am_zoomout, NULL, NULL},
	{OPT_KeyConfig, "Add Mark",      NULL, 0, &key_am_mark, NULL, NULL},
	{OPT_KeyConfig, "Clear Marks",   NULL, 0, &key_am_clear, NULL, NULL},
};

static menuinfo_t automap_optmenu = 
{
	automap_keyconfig, sizeof(automap_keyconfig) / sizeof(optmenuitem_t),
	&opt_def_style, 140, 98, "M_CONTRL", NULL, 0,
	"Automap Keys", language["MenuBinding"]
};

//
//  KEY CONFIG : MENU NAVIGATION
//
static optmenuitem_t menu_nav_keyconfig[] =
{
	{OPT_KeyConfig, "Open/Close Menu",   NULL, 0, &key_menu_open, NULL, NULL},
	{OPT_Plain,     "",                 NULL, 0, NULL, NULL, NULL},
	{OPT_KeyConfig, "Menu Up",         NULL, 0, &key_menu_up, NULL, NULL},
	{OPT_KeyConfig, "Menu Down",           NULL, 0, &key_menu_down, NULL, NULL},
	{OPT_KeyConfig, "Menu Left",              NULL, 0, &key_menu_left, NULL, NULL},
	{OPT_KeyConfig, "Menu Right",   NULL, 0, &key_menu_right, NULL, NULL},
	{OPT_Plain,     "",                 NULL, 0, NULL, NULL, NULL},
	{OPT_KeyConfig, "Select Option",       NULL, 0, &key_menu_select, NULL, NULL},
	{OPT_KeyConfig, "Back/Cancel",          NULL, 0, &key_menu_cancel, NULL, NULL},
};

static menuinfo_t menu_nav_optmenu = 
{
	menu_nav_keyconfig, sizeof(menu_nav_keyconfig) / sizeof(optmenuitem_t),
	&opt_def_style, 140, 98, "M_CONTRL", NULL, 0,
	"Menu Navigation", language["MenuBinding"]
};

//
//  KEY CONFIG : INVENTORY
//
static optmenuitem_t menu_nav_inventory[] =
{
	{OPT_KeyConfig, "Previous Item",   NULL, 0, &key_inv_prev, NULL, NULL},
	{OPT_KeyConfig, "Use Item",         NULL, 0, &key_inv_use, NULL, NULL},
	{OPT_KeyConfig, "Next Item",           NULL, 0, &key_inv_next, NULL, NULL},
};

static menuinfo_t inventory_optmenu = 
{
	menu_nav_inventory, sizeof(menu_nav_inventory) / sizeof(optmenuitem_t),
	&opt_def_style, 140, 98, "M_CONTRL", NULL, 0,
	"Inventory", language["MenuBinding"]
};

//
//  KEY CONFIG : PROGRAM
//
static optmenuitem_t program_keyconfig1[] =
{
	{OPT_KeyConfig, "Screenshot",   NULL, 0, &key_screenshot, NULL, NULL},
	{OPT_KeyConfig, "Save Game",         NULL, 0, &key_save_game, NULL, NULL},
	{OPT_KeyConfig, "Load Game",           NULL, 0, &key_load_game, NULL, NULL},
	{OPT_Plain,     "",                 NULL, 0, NULL, NULL, NULL},
	{OPT_KeyConfig, "Sound Controls",              NULL, 0, &key_sound_controls, NULL, NULL},
	{OPT_KeyConfig, "Options",   NULL, 0, &key_options_menu, NULL, NULL},
	{OPT_KeyConfig, "Quicksave",       NULL, 0, &key_quick_save, NULL, NULL},
};

static menuinfo_t program_optmenu1 = 
{
	program_keyconfig1, sizeof(program_keyconfig1) / sizeof(optmenuitem_t),
	&opt_def_style, 140, 98, "M_CONTRL", NULL, 0,
	"Program (1/2)", language["MenuBinding"]
};

//
//  KEY CONFIG : PROGRAM
//
static optmenuitem_t program_keyconfig2[] =
{
	{OPT_KeyConfig, "End Game",          NULL, 0, &key_end_game, NULL, NULL},
	{OPT_KeyConfig, "Toggle Messages",       NULL, 0, &key_message_toggle, NULL, NULL},
	{OPT_KeyConfig, "Quickload",          NULL, 0, &key_quick_load, NULL, NULL},
	{OPT_Plain,     "",                 NULL, 0, NULL, NULL, NULL},
	{OPT_KeyConfig, "Quit EDGE",          NULL, 0, &key_quit_edge, NULL, NULL},
	{OPT_KeyConfig, "Toggle Gamma",       NULL, 0, &key_gamma_toggle, NULL, NULL},
	{OPT_KeyConfig, "Show Players",          NULL, 0, &key_show_players, NULL, NULL},
};

static menuinfo_t program_optmenu2 = 
{
	program_keyconfig2, sizeof(program_keyconfig2) / sizeof(optmenuitem_t),
	&opt_def_style, 140, 98, "M_CONTRL", NULL, 0,
	"Program (2/2)", language["MenuBinding"]
};

/*
 * ALL KEYBOARD MENUS
 */
#define NUM_KEY_MENUS  9

static menuinfo_t * all_key_menus[NUM_KEY_MENUS] =
{
	&movement_optmenu,
	&attack_optmenu,
	&otherkey_optmenu,
	&weapon_optmenu,
	&automap_optmenu,
	&menu_nav_optmenu,
	&inventory_optmenu,
	&program_optmenu1,
	&program_optmenu2
};

static char keystring1[] = "Enter to change, Backspace to Clear";
static char keystring2[] = "Press a key for this action";

//
// M_OptCheckNetgame
//
// Sets the first option to be "Leave Game" or "Multiplayer Game"
// depending on whether we are playing a game or not.
//
void M_OptCheckNetgame(void)
{
	if (gamestate >= GS_LEVEL)
	{
		strcpy(mainoptions[HOSTNET_POS+0].name, "Leave Game");
		mainoptions[HOSTNET_POS+0].routine = &M_EndGame;
		mainoptions[HOSTNET_POS+0].help = NULL;

//		strcpy(mainoptions[HOSTNET_POS+1].name, "");
//		mainoptions[HOSTNET_POS+1].type = OPT_Plain;
//		mainoptions[HOSTNET_POS+1].routine = NULL;
//		mainoptions[HOSTNET_POS+1].help = NULL;
	}
	else
	{
		strcpy(mainoptions[HOSTNET_POS+0].name, "Advanced Start");
		mainoptions[HOSTNET_POS+0].routine = &M_HostNetGame;
		mainoptions[HOSTNET_POS+0].help = NULL;

//		strcpy(mainoptions[HOSTNET_POS+1].name, "Join Net Game");
//		mainoptions[HOSTNET_POS+1].type = OPT_Function;
//		mainoptions[HOSTNET_POS+1].routine = &M_JoinNetGame;
//		mainoptions[HOSTNET_POS+1].help = NULL;
	}
}


void M_OptMenuInit()
{
	option_menuon = 0;
	curr_menu = &main_optmenu;
	curr_item = curr_menu->items + curr_menu->pos;
	curr_key_menu = 0;
	keyscan = 0;

	InitMonitorSize();

	// load styles
	styledef_c *def;

	def = styledefs.Lookup("OPTIONS");
	if (! def) def = default_style;
	opt_def_style = hu_styles.Lookup(def);

	//Lobo 2022: load our ddflang stuff
	main_optmenu.name=language["MenuOptions"];
	video_optmenu.name=language["MenuVideo"];
	res_optmenu.name=language["MenuResolution"];
	analogue_optmenu.name=language["MenuMouse"];
	sound_optmenu.name=language["MenuSound"];
	f4sound_optmenu.name=language["MenuSound"];
	gameplay_optmenu.name=language["MenuGameplay"];
	perf_optmenu.name=language["MenuPerformance"];
	movement_optmenu.name=language["MenuBinding"];
	attack_optmenu.name=language["MenuBinding"];
	otherkey_optmenu.name=language["MenuBinding"];
	weapon_optmenu.name=language["MenuBinding"];
	automap_optmenu.name=language["MenuBinding"];
	menu_nav_optmenu.name=language["MenuBinding"];
	inventory_optmenu.name=language["MenuBinding"];
	program_optmenu1.name=language["MenuBinding"];
	program_optmenu2.name=language["MenuBinding"];

	// Restore the config setting.
	M_ChangeBlood(-1);
}


void M_OptTicker(void)
{
	// nothing needed
}


void M_OptDrawer()
{
	char tempstring[80];
	int curry, deltay, menutop;	
	unsigned int k;

	style_c *style = curr_menu->style_var[0];
	SYS_ASSERT(style);

	style->DrawBackground();

	if (! style->fonts[styledef_c::T_TEXT])
		return;

	int fontType;

	if (! style->fonts[styledef_c::T_HEADER])
		fontType=styledef_c::T_TEXT;
	else
		fontType=styledef_c::T_HEADER;

	int font_h;
	int CenterX;
	float TEXTscale;

	TEXTscale = 1.0;

	if(style->def->text[fontType].scale)
	{
		TEXTscale=style->def->text[fontType].scale;
	}
	
	font_h = style->fonts[fontType]->NominalHeight();
	font_h *=TEXTscale;
	menutop = font_h / 2;

	const image_c *image;

	if (! curr_menu->title_image) 
		curr_menu->title_image = W_ImageLookup(curr_menu->title_name);

	image = curr_menu->title_image;

	CenterX = 160;
	CenterX -= (style->fonts[fontType]->StringWidth(curr_menu->name) * 1.5) / 2;
	
	//Lobo 2022
	bool custom_optionmenu = false;
	if (custom_optionmenu==false) 
	{
		HL_WriteText(style,fontType, CenterX, menutop, curr_menu->name,1.5);
	} 
	else
	{		
		const colourmap_c *colmap = style->def->text[fontType].colmap;
		HUD_DrawImage(curr_menu->title_x, menutop, image, colmap);
	}

	fontType=styledef_c::T_TEXT;
	TEXTscale=style->def->text[fontType].scale;
	font_h = style->fonts[fontType]->NominalHeight();
	font_h *=TEXTscale;
	menutop = 68 - ((curr_menu->item_num * font_h) / 2);
	if (curr_menu->key_page[0]) 
		menutop = 9 * font_h / 2;
	
	// These don't seem used after this point in the function - Dasho
	//CenterX = 160;
	//CenterX -= (style->fonts[fontType]->StringWidth(curr_menu->name) * 1.5) / 2;


	//now, draw all the menuitems
	deltay = 1 + font_h;

	curry = menutop + 25;

	if (curr_menu->key_page[0])
	{
		if (curr_key_menu > 0)
			HL_WriteText(style,styledef_c::T_TITLE, 60, 200-deltay*4, "< PREV");

		if (curr_key_menu < NUM_KEY_MENUS-1)
			HL_WriteText(style,styledef_c::T_TITLE, 260 - style->fonts[2]->StringWidth("NEXT >"), 200-deltay*4, 
							  "NEXT >");

		HL_WriteText(style,styledef_c::T_HELP, 160 - style->fonts[3]->StringWidth(curr_menu->key_page)/2, 
					 curry, curr_menu->key_page);
		curry += font_h*2;

		if (keyscan)
			HL_WriteText(style,styledef_c::T_HELP, 160 - (style->fonts[3]->StringWidth(keystring2) / 2), 
							  200-deltay*2, keystring2);
		else
			HL_WriteText(style,styledef_c::T_HELP, 160 - (style->fonts[3]->StringWidth(keystring1) / 2), 
							  200-deltay*2, keystring1);
	}
	else if (curr_menu == &res_optmenu)
	{
		M_ResOptDrawer(style, curry, curry + (deltay * res_optmenu.item_num - 2), 
					   deltay, curr_menu->menu_center);
	}
	else if (curr_menu == &main_optmenu)
	{
		M_LanguageDrawer(curr_menu->menu_center, curry, deltay);
	}

	for (int i = 0; i < curr_menu->item_num; i++)
	{
		bool is_selected = (i == curr_menu->pos);

		if (curr_menu == &res_optmenu && curr_menu->items[i].routine == M_ChangeResSize)
		{
			if (new_scrmode.display_mode == 2)
			{
				curry += deltay;
				continue;
			}
		}

		HL_WriteText(style, is_selected ? styledef_c::T_TITLE : styledef_c::T_TEXT,
		             (curr_menu->menu_center) - 
					 (style->fonts[is_selected ? styledef_c::T_TITLE : styledef_c::T_TEXT]->StringWidth(curr_menu->items[i].name) * TEXTscale),
					 curry, curr_menu->items[i].name);
		
		if (curr_menu == &analogue_optmenu && curr_menu->items[i].switchvar == &joystick_device)
		{
			HL_WriteText(style, styledef_c::T_TEXT, 225, curry, "Axis Test");
			std::string axis = "None";
			for (int j = 0; j < 6; j++)
			{
				int joy = I_JoyGetAxis(j);
				if (abs(joy) < 0x8000 && abs(joy) > 5000)
				{
					switch (j)
					{
						case 0:
							axis = "First";
							break;
						case 1:
							axis = "Second";
							break;
						case 2:
							axis = "Third";
							break;
						case 3:
							axis = "Fourth";
							break;
						case 4:
							axis = "Fifth";
							break;
						case 5:
							axis = "Sixth";
							break;
						default:
							break;
					}
				}
			}
			HL_WriteText(style, styledef_c::T_ALT, 225 + (style->fonts[0]->StringWidth("AXIS TEST") / 2) - (style->fonts[0]->StringWidth(axis.c_str()) / 2), curry + deltay, axis.c_str());
		}

		// Draw current soundfont
		if (curr_menu == &sound_optmenu && curr_menu->items[i].routine == M_ChangeSoundfont)
		{
			HL_WriteText(style,styledef_c::T_ALT, (curr_menu->menu_center) + 15, curry, epi::PATH_GetBasename(UTFSTR(s_soundfont.s)).u8string().c_str());
		}

		// Draw current GENMIDI
		if (curr_menu == &sound_optmenu && curr_menu->items[i].routine == M_ChangeGENMIDI)
		{
			HL_WriteText(style,styledef_c::T_ALT, (curr_menu->menu_center) + 15, curry, 
				s_genmidi.s.empty() ? "default" : epi::PATH_GetBasename(UTFSTR(s_genmidi.s)).u8string().c_str());
		}

		// -ACB- 1998/07/15 Menu Cursor is colour indexed.
		if (is_selected)
		{
			if (style->fonts[styledef_c::T_ALT]->def->type == FNTYP_Image)
			{
				int cursor = 16;
				HL_WriteText(style,styledef_c::T_TITLE, (curr_menu->menu_center + 4), curry, (const char *)&cursor);
			}
			else
				HL_WriteText(style,styledef_c::T_TITLE, (curr_menu->menu_center + 4), curry, "*");

			if (curr_menu->items[i].help)
			{
				const char *help = language[curr_menu->items[i].help];

				HL_WriteText(style,styledef_c::T_HELP, 160 - (style->fonts[3]->StringWidth(help) * TEXTscale / 2), 200 - deltay*2, 
								  help);
			}
		}

		switch (curr_menu->items[i].type)
		{
			case OPT_Boolean:
			case OPT_Switch:
			{
				k = 0;
				for (int j = 0; j < M_GetCurrentSwitchValue(&curr_menu->items[i]); j++)
				{
					while ((curr_menu->items[i].typenames[k] != '/') && (k < strlen(curr_menu->items[i].typenames)))
						k++;

					k++;
				}

				if (k < strlen(curr_menu->items[i].typenames))
				{
					int j = 0;
					while ((curr_menu->items[i].typenames[k] != '/') && (k < strlen(curr_menu->items[i].typenames)))
					{
						tempstring[j] = curr_menu->items[i].typenames[k];
						j++;
						k++;
					}
					tempstring[j] = 0;
				}
				else
				{
					sprintf(tempstring, "Invalid");
				}

				HL_WriteText(style,styledef_c::T_ALT, (curr_menu->menu_center) + 15, curry, tempstring);
				break;
			}

			case OPT_Slider:
			{
				M_DrawThermo(curr_menu->menu_center + 15, curry,
							 curr_menu->items[i].numtypes,
							  *(int*)(curr_menu->items[i].switchvar), 2);
              
				break;
			}

			case OPT_KeyConfig:
			{
				k = *(int*)(curr_menu->items[i].switchvar);
				M_Key2String(k, tempstring);
				HL_WriteText(style,styledef_c::T_ALT, (curr_menu->menu_center + 15), curry, tempstring);
				break;
			}

			default:
				break;
		}
		curry += deltay;
	}
}

//
// M_OptResDrawer
//
// Something of a hack, but necessary to give a better way of changing
// resolution
//
// -ACB- 1999/10/03 Written
//
static void M_ResOptDrawer(style_c *style, int topy, int bottomy, int dy, int centrex)
{
	char tempstring[80];
	
	// Draw current resolution
	int y = topy;

	// Draw resolution selection option
	y += (dy*3);

	sprintf(tempstring, "%s", new_scrmode.display_mode == 2 ? "Borderless Fullscreen" :
		(new_scrmode.display_mode == new_scrmode.SCR_FULLSCREEN ? "Fullscreen" : "Windowed"));
	HL_WriteText(style,styledef_c::T_ALT, centrex+15, y, tempstring);

	if (new_scrmode.display_mode < 2)
	{
		y += dy;
		sprintf(tempstring, "%dx%d", new_scrmode.width, new_scrmode.height);
		HL_WriteText(style,styledef_c::T_ALT, centrex+15, y, tempstring);
	}

	// Draw selected resolution and mode:
	y = bottomy;

	sprintf(tempstring, "Current Resolution:");
	HL_WriteText(style,styledef_c::T_HELP, 160 - (style->fonts[0]->StringWidth(tempstring) / 2), y, tempstring);

	y += dy;
	y += 5;
	if (DISPLAYMODE == 2)
		sprintf(tempstring, "%s", "Borderless Fullscreen");
	else
		sprintf(tempstring, "%d x %d %s", SCREENWIDTH, SCREENHEIGHT, DISPLAYMODE == 1 ? "Fullscreen" : "Windowed");

	HL_WriteText(style,styledef_c::T_ALT, 160 - (style->fonts[1]->StringWidth(tempstring) / 2), y, tempstring);
}

//
// M_LanguageDrawer
//
// Yet another hack (this stuff badly needs rewriting) to draw the
// current language name.
//
// -AJA- 2000/04/16 Written

static void M_LanguageDrawer(int x, int y, int deltay)
{
	// This seems unused for now - Dasho
	/*float ALTscale = 1.0;

	if(opt_def_style->def->text[styledef_c::T_ALT].scale)
	{
		ALTscale=opt_def_style->def->text[styledef_c::T_ALT].scale;
	}*/
	HL_WriteText(opt_def_style,styledef_c::T_ALT, x+15, y + deltay * LANGUAGE_POS, language.GetName());
}


static void KeyMenu_Next()
{
	if (curr_key_menu >= NUM_KEY_MENUS-1)
		return;
	
	curr_key_menu++;

	curr_menu = all_key_menus[curr_key_menu];

	S_StartFX(sfx_pstop);
}

static void KeyMenu_Prev()
{
	if (curr_key_menu <= 0)
		return;
	
	curr_key_menu--;

	curr_menu = all_key_menus[curr_key_menu];

	S_StartFX(sfx_pstop);
}

//
// M_OptResponder
//
bool M_OptResponder(event_t * ev, int ch)
{
///--  	if (testticker != -1)
///--  		return true;

	curr_item = curr_menu->items + curr_menu->pos; // Should help the accidental key binding to other options - Dasho

	// Scan for keycodes
	if (keyscan)
	{
		int *blah;

		if (ev->type != ev_keydown)
			return false;
		int key = ev->value.key.sym;

		keyscan = 0;

		if (ch == KEYD_ESCAPE || ch == KEYD_MENU_CANCEL)
			return true;
     
		blah = (int*)(curr_item->switchvar);
		if (((*blah) >> 16) == key)
		{
			(*blah) &= 0xffff;
			return true;
		}
		if (((*blah) & 0xffff) == key)
		{
			(*blah) >>= 16;
			return true;
		}

		if (((*blah) & 0xffff) == 0)
			*blah = key;
		else if (((*blah) >> 16) == 0)
			*blah |= key << 16;
		else
		{
			*blah >>= 16;
			*blah |= key << 16;
		}
		return true;
	}

	switch (ch)
	{
		case KEYD_BACKSPACE:
		{
			if (curr_item->type == OPT_KeyConfig)
				*(int*)(curr_item->switchvar) = 0;
			return true;
		}

		case KEYD_DOWNARROW:
		case KEYD_DPAD_DOWN:
		case KEYD_MENU_DOWN:
		{
			do
			{
				curr_menu->pos++;
				if (curr_menu == &res_optmenu && new_scrmode.display_mode == 2)
				{
					if (curr_menu->pos >= 0 && curr_menu->pos < curr_menu->item_num)
					{
						if (curr_menu->items[curr_menu->pos].routine == M_ChangeResSize)
							curr_menu->pos++;
					}
				}
				if (curr_menu->pos >= curr_menu->item_num)
					curr_menu->pos = 0;
				curr_item = curr_menu->items + curr_menu->pos;
			}
			while (curr_item->type == 0);

			S_StartFX(sfx_pstop);
			return true;
		}

		case KEYD_WHEEL_DN:
		{
			do
			{
				curr_menu->pos++;
				if (curr_menu == &res_optmenu && new_scrmode.display_mode == 2)
				{
					if (curr_menu->pos >= 0 && curr_menu->pos < curr_menu->item_num)
					{
						if (curr_menu->items[curr_menu->pos].routine == M_ChangeResSize)
							curr_menu->pos++;
					}
				}
				if (curr_menu->pos >= curr_menu->item_num)
				{
					if (curr_menu->key_page[0])
					{
						KeyMenu_Next();
						curr_menu->pos = 0;
						return true;
					}
					curr_menu->pos = 0;
				}
				curr_item = curr_menu->items + curr_menu->pos;
			}
			while (curr_item->type == 0);

			S_StartFX(sfx_pstop);
			return true;
		}

		case KEYD_UPARROW:
		case KEYD_DPAD_UP:
		case KEYD_MENU_UP:
		{
			do
			{
				curr_menu->pos--;
				if (curr_menu == &res_optmenu && new_scrmode.display_mode == 2)
				{
					if (curr_menu->pos >= 0 && curr_menu->pos < curr_menu->item_num)
					{
						if (curr_menu->items[curr_menu->pos].routine == M_ChangeResSize)
							curr_menu->pos--;
					}
				}
				if (curr_menu->pos < 0)
					curr_menu->pos = curr_menu->item_num - 1;
				curr_item = curr_menu->items + curr_menu->pos;
			}
			while (curr_item->type == 0);

			S_StartFX(sfx_pstop);
			return true;
		}

		case KEYD_WHEEL_UP:
		{
			do
			{
				curr_menu->pos--;
				if (curr_menu == &res_optmenu && new_scrmode.display_mode == 2)
				{
					if (curr_menu->pos >= 0 && curr_menu->pos < curr_menu->item_num)
					{
						if (curr_menu->items[curr_menu->pos].routine == M_ChangeResSize)
							curr_menu->pos--;
					}
				}
				if (curr_menu->pos < 0)
				{
					if (curr_menu->key_page[0])
					{
						KeyMenu_Prev();
						curr_menu->pos = curr_menu->item_num - 1;
						return true;
					}
					curr_menu->pos = curr_menu->item_num - 1;
				}
				curr_item = curr_menu->items + curr_menu->pos;
			}
			while (curr_item->type == 0);

			S_StartFX(sfx_pstop);
			return true;
		}

		case KEYD_LEFTARROW:
		case KEYD_DPAD_LEFT:
		case KEYD_MENU_LEFT:
		{
			if (curr_menu->key_page[0])
			{
				KeyMenu_Prev();
				return true;
			}
       
			switch (curr_item->type)
			{
				case OPT_Plain:
				{
					return false;
				}

				case OPT_Boolean:
				{
					bool *boolptr = (bool*)curr_item->switchvar;

					*boolptr = !(*boolptr);

					S_StartFX(sfx_pistol);

					if (curr_item->routine != NULL)
						curr_item->routine(ch);

					return true;
				}

				case OPT_Switch:
				{
					int *val_ptr = (int*)curr_item->switchvar;

					*val_ptr -= 1;

					if (*val_ptr < 0)
						*val_ptr = curr_item->numtypes - 1;

					S_StartFX(sfx_pistol);

					if (curr_item->routine != NULL)
						curr_item->routine(ch);

					return true;
				}

				case OPT_Function:
				{
					if (curr_item->routine != NULL)
						curr_item->routine(ch);

					S_StartFX(sfx_pistol);
					return true;
				}

				case OPT_Slider:
				{
					int *val_ptr = (int*)curr_item->switchvar;

					if (*val_ptr > 0)
					{
						*val_ptr -= 1;

						S_StartFX(sfx_stnmov);
					}

					if (curr_item->routine != NULL)
						curr_item->routine(ch);

					return true;
				}

				default:
					break;
			}
		}

		case KEYD_RIGHTARROW:
		case KEYD_DPAD_RIGHT:
		case KEYD_MENU_RIGHT:
			if (curr_menu->key_page[0])
			{
				KeyMenu_Next();
				return true;
			}

      /* FALL THROUGH... */
     
		case KEYD_ENTER:
		case KEYD_MOUSE1:
		case KEYD_MENU_SELECT:
		{
			switch (curr_item->type)
			{
				case OPT_Plain:
					return false;

				case OPT_Boolean:
				{
					bool *boolptr = (bool*)curr_item->switchvar;

					*boolptr = !(*boolptr);

					S_StartFX(sfx_pistol);

					if (curr_item->routine != NULL)
						curr_item->routine(ch);

					return true;
				}

				case OPT_Switch:
				{
					int *val_ptr = (int*)curr_item->switchvar;

					*val_ptr += 1;

					if (*val_ptr >= curr_item->numtypes)
						*val_ptr = 0;

					S_StartFX(sfx_pistol);

					if (curr_item->routine != NULL)
						curr_item->routine(ch);

					return true;
				}

				case OPT_Function:
				{
					if (curr_item->routine != NULL)
						curr_item->routine(ch);

					S_StartFX(sfx_pistol);
					return true;
				}

				case OPT_Slider:
				{
					int *val_ptr = (int*)curr_item->switchvar;

					if (*val_ptr < (curr_item->numtypes - 1))
					{
						*val_ptr += 1;

						S_StartFX(sfx_stnmov);
					}

					if (curr_item->routine != NULL)
						curr_item->routine(ch);

					return true;
				}

				case OPT_KeyConfig:
				{
					keyscan = 1;
					return true;
				}

				default:
					break;
			}
			I_Error("Invalid menu type!");
		}
		case KEYD_ESCAPE:
		case KEYD_MOUSE2:
		case KEYD_MOUSE3:
		case KEYD_MENU_CANCEL:
		{
			if (curr_menu == &f4sound_optmenu)
			{
				curr_menu = &main_optmenu;
				M_ClearMenus();
			}
			else if (curr_menu == &main_optmenu)
			{
				if (fkey_menu)
					M_ClearMenus();
				else
					option_menuon = 0;
			}
			else
			{
				curr_menu = &main_optmenu;
				curr_item = curr_menu->items + curr_menu->pos;
			}
			S_StartFX(sfx_swtchx);
			return true;
		}
	}
	return false;
}

// ===== SUB-MENU SETUP ROUTINES =====

//
// M_VideoOptions
//
static void M_VideoOptions(int keypressed)
{
	curr_menu = &video_optmenu;
	curr_item = curr_menu->items + curr_menu->pos;
}

//
// M_ResolutionOptions
//
// This menu is different in the fact that is most be calculated at runtime,
// this is because different resolutions are available on different machines.
//
// -ES- 1998/08/20 Added
// -ACB 1999/10/03 rewrote to Use scrmodes array.
//
static void M_ResolutionOptions(int keypressed)
{
	new_scrmode.width  = SCREENWIDTH;
	new_scrmode.height = SCREENHEIGHT;
	new_scrmode.depth  = SCREENBITS;
	new_scrmode.display_mode = DISPLAYMODE;

	curr_menu = &res_optmenu;
	curr_item = curr_menu->items + curr_menu->pos;
}

//
// M_AnalogueOptions
//
static void M_AnalogueOptions(int keypressed)
{
	curr_menu = &analogue_optmenu;
	curr_item = curr_menu->items + curr_menu->pos;
}

//
// M_SoundOptions
//
static void M_SoundOptions(int keypressed)
{
	curr_menu = &sound_optmenu;
	curr_item = curr_menu->items + curr_menu->pos;
}

//
// M_F4SoundOptions
//
void M_F4SoundOptions(int choice)
{
	option_menuon = 1;
	curr_menu = &f4sound_optmenu;
	curr_item = curr_menu->items + curr_menu->pos;
}

//
// M_GameplayOptions
//
static void M_GameplayOptions(int keypressed)
{
	// not allowed in netgames (changing most of these options would
	// break synchronisation with the other machines).
	if (netgame)
		return;

	curr_menu = &gameplay_optmenu;
	curr_item = curr_menu->items + curr_menu->pos;
}

//
// M_PerformanceOptions
//
static void M_PerformanceOptions(int keypressed)
{
	// not allowed in netgames (changing most of these options would
	// break synchronisation with the other machines).
	if (netgame)
		return;

	curr_menu = &perf_optmenu;
	curr_item = curr_menu->items + curr_menu->pos;
}

//
// M_KeyboardOptions
//
static void M_KeyboardOptions(int keypressed)
{
	curr_menu = all_key_menus[curr_key_menu];

	curr_item = curr_menu->items + curr_menu->pos;
}

// ===== END OF SUB-MENUS =====

//
// M_Key2String
//
static void M_Key2String(int key, char *deststring)
{
	int key1 = key & 0xffff;
	int key2 = key >> 16;

	if (key1 == 0)
	{
		strcpy(deststring, "---");
		return;
	}

	strcpy(deststring, E_GetKeyName(key1));

	if (key2 != 0)
	{
		strcat(deststring, " or ");
		strcat(deststring, E_GetKeyName(key2));
	}
}


static void InitMonitorSize()
{
	     if (v_monitorsize.f > 2.00) monitor_size = 5;
	else if (v_monitorsize.f > 1.70) monitor_size = 4;
	else if (v_monitorsize.f > 1.55) monitor_size = 3;
	else if (v_monitorsize.f > 1.40) monitor_size = 2;
	else if (v_monitorsize.f > 1.30) monitor_size = 1;
	else                             monitor_size = 0;
}


static void M_ChangeMonitorSize(int key)
{
	static const float ratios[6] =
	{
		1.25000, 1.33333, 1.50000,   // 5:4     4:3   3:2
		1.60000, 1.77777, 2.33333    // 16:10  16:9  21:9
	};

	monitor_size = CLAMP(0, monitor_size, 5);

	v_monitorsize = ratios[monitor_size];
}


//
// M_ChangeBlood
//
// -KM- 1998/07/21 Change blood to a bool
// -ACB- 1998/08/09 Check map setting allows this
//
static void M_ChangeBlood(int keypressed)
{
	if (currmap && ((currmap->force_on | currmap->force_off) & MPF_MoreBlood))
		return;

	level_flags.more_blood = global_flags.more_blood;
}

static void M_ChangeMLook(int keypressed)
{
	if (currmap && ((currmap->force_on | currmap->force_off) & MPF_Mlook))
		return;

	level_flags.mlook = global_flags.mlook;
}

static void M_ChangeJumping(int keypressed)
{
	if (currmap && ((currmap->force_on | currmap->force_off) & MPF_Jumping))
		return;

	level_flags.jump = global_flags.jump;
}

static void M_ChangeCrouching(int keypressed)
{
	if (currmap && ((currmap->force_on | currmap->force_off) & MPF_Crouching))
		return;

	level_flags.crouch = global_flags.crouch;
}

static void M_ChangeExtra(int keypressed)
{
	if (currmap && ((currmap->force_on | currmap->force_off) & MPF_Extras))
		return;

	level_flags.have_extra = global_flags.have_extra;
}

//
// M_ChangeMonsterRespawn
//
// -ACB- 1998/08/09 New DDF settings, check that map allows the settings
//
static void M_ChangeMonsterRespawn(int keypressed)
{
	if (currmap && ((currmap->force_on | currmap->force_off) & MPF_ResRespawn))
		return;

	level_flags.res_respawn = global_flags.res_respawn;
}

static void M_ChangeItemRespawn(int keypressed)
{
	if (currmap && ((currmap->force_on | currmap->force_off) & MPF_ItemRespawn))
		return;

	level_flags.itemrespawn = global_flags.itemrespawn;
}

static void M_ChangeTrue3d(int keypressed)
{
	if (currmap && ((currmap->force_on | currmap->force_off) & MPF_True3D))
		return;

	level_flags.true3dgameplay = global_flags.true3dgameplay;
}

static void M_ChangeAutoAim(int keypressed)
{
	if (currmap && ((currmap->force_on | currmap->force_off) & MPF_AutoAim))
		return;

	level_flags.autoaim = global_flags.autoaim;
}

static void M_ChangeRespawn(int keypressed)
{
	if (gameskill == sk_nightmare)
		return;

	if (currmap && ((currmap->force_on | currmap->force_off) & MPF_Respawn))
		return;

	level_flags.respawn = global_flags.respawn;
}

static void M_ChangeFastparm(int keypressed)
{
	if (gameskill == sk_nightmare)
		return;

	if (currmap && ((currmap->force_on | currmap->force_off) & MPF_FastParm))
		return;

	level_flags.fastparm = global_flags.fastparm;
}

static void M_ChangePassMissile(int keypressed)
{
	level_flags.pass_missile = global_flags.pass_missile;
}

// this used by both MIPMIP, SMOOTHING and DETAIL options
static void M_ChangeMipMap(int keypressed)
{
	W_DeleteAllImages();
}

// Need to make a generic CVAR save function
static void M_SaveOverlay(int keypressed)
{
	r_overlay = r_overlay.d;
}

// See above
static void M_ChangeErraticism(int keypressed)
{
	g_erraticism = g_erraticism.d;
}

// See above the above
static void M_ChangeDoubleFrames(int keypressed)
{
	r_doubleframes = r_doubleframes.d;
}

// See above the above the above
static void M_ChangeMBF21Compat(int keypressed)
{
	g_mbf21compat = g_mbf21compat.d;
}

// See above the above the above the above
static void M_ChangeCulling(int keypressed)
{
	r_culling = r_culling.d;
}

// See above the above the above the above the above
static void M_ChangeCullDist(int keypressed)
{
	r_culldist = r_culldist.d;
}

// See above the above the above the above the above the above
static void M_ChangeCullFog(int keypressed)
{
	r_cullfog = r_cullfog.d;
}

// See above the above the above the above the above the above the above
static void M_ChangeThinkerCulling(int keypressed)
{
	g_cullthinkers = g_cullthinkers.d;
}

// See above the above the above the above the above the above the above the above
static void M_ChangeMaxDLights(int keypressed)
{
	r_maxdlights = r_maxdlights.d;
}

// See above the above the above the above the above the above the above the above the above
static void M_ChangeVSync(int keypressed)
{
	v_sync = v_sync.d;
}

// See above the above the above the above the above the above the above the above the above the above
static void M_ChangeBobbing(int keypressed)
{
	g_bobbing = g_bobbing.d;
	player_t *player = players[consoleplayer];
	if (player)
	{
		player->bob = 0;
		pspdef_t *psp = &player->psprites[player->action_psp];
		if (psp)
		{
			psp->sx = 0;
			psp->sy = 0;
		}
	}
}

// See above the above the above the above the above the above the above the above the above the above the above
static void M_ChangeSecBright(int keypressed)
{
	v_secbright = v_secbright.d;
}

// See above the above the above the above the above the above the above the above the above the above the above the above
static void M_ChangeGamma(int keypressed)
{
	v_gamma = v_gamma.d;
}

static void M_ChangeKicking(int keypressed)
{
	if (currmap && ((currmap->force_on | currmap->force_off) & MPF_Kicking))
		return;

	level_flags.kicking = global_flags.kicking;
}

static void M_ChangeWeaponSwitch(int keypressed)
{
	if (currmap && ((currmap->force_on | currmap->force_off) & MPF_WeaponSwitch))
		return;

	level_flags.weapon_switch = global_flags.weapon_switch;
}

static void M_ChangeDLights(int keypressed)
{
	/* nothing to do */
}

static void M_ChangeCrossHair(int keypressed)
{
	r_crosshair = menu_crosshair;
}

static void M_ChangeCrossColor(int keypressed)
{
	r_crosscolor = menu_crosscolor;
}

static void M_ChangeCrossSize(int keypressed)
{
	r_crosssize = 8 + (menu_crosssize * 8);
}

static void M_ChangePCSpeakerMode(int keypressed)
{
	// Clear SFX cache and restart music
	S_StopAllFX();
	S_CacheClearAll();
	M_ChangeMIDIPlayer(0);
}

//
// M_ChangeLanguage
//
// -AJA- 2000/04/16 Run-time language changing...
//
static void M_ChangeLanguage(int keypressed)
{
	if (keypressed == KEYD_LEFTARROW || keypressed == KEYD_DPAD_LEFT || keypressed == KEYD_MENU_LEFT)
	{
		int idx, max;
		
		idx = language.GetChoice();
		max = language.GetChoiceCount();

		idx--;
		if (idx < 0) { idx += max; }
			
		language.Select(idx);
	}
	else if (keypressed == KEYD_RIGHTARROW || keypressed == KEYD_DPAD_RIGHT || keypressed == KEYD_MENU_RIGHT)
	{
		int idx, max;
		
		idx = language.GetChoice();
		max = language.GetChoiceCount();

		idx++;
		if (idx >= max) { idx = 0; }
			
		language.Select(idx);
	}

	// update cvar
	m_language = language.GetName();
}

//
// M_ChangeMIDIPlayer
//
//
static void M_ChangeMIDIPlayer(int keypressed)
{
	pl_entry_c *playing = playlist.Find(entry_playing);
	if (!var_pc_speaker_mode && (var_midi_player == 1 || (playing && 
		(playing->type == MUS_IMF280 || playing->type == MUS_IMF560 || playing->type == MUS_IMF700))))
		S_RestartOPL();
	else if (var_midi_player == 0 || var_pc_speaker_mode)
		S_RestartFluid();
	else
		S_RestartFMM();
}

//
// M_ChangeSoundfont
//
//
static void M_ChangeSoundfont(int keypressed)
{
	int sf_pos = -1;
	for(int i=0; i < (int)available_soundfonts.size(); i++)
	{
		if (epi::case_cmp(s_soundfont.s, available_soundfonts.at(i).generic_u8string()) == 0)
		{
			sf_pos = i;
			break;
		}
	}

	if (sf_pos < 0)
	{
		I_Warning("M_ChangeSoundfont: Could not read list of available soundfonts. Falling back to default!\n");
		s_soundfont = epi::PATH_Join(game_dir, UTFSTR("soundfont/default.sf2")).generic_u8string();
		return;
	}

	if (keypressed == KEYD_LEFTARROW || keypressed == KEYD_DPAD_LEFT || keypressed == KEYD_MENU_LEFT)
	{
		if (sf_pos - 1 >= 0)
			sf_pos--;
		else
			sf_pos = available_soundfonts.size() - 1;
	}
	else if (keypressed == KEYD_RIGHTARROW || keypressed == KEYD_DPAD_RIGHT || keypressed == KEYD_MENU_RIGHT)
	{
		if (sf_pos + 1 >= (int)available_soundfonts.size())
			sf_pos = 0;
		else
			sf_pos++;
	}

	// update cvar
	s_soundfont = available_soundfonts.at(sf_pos).generic_u8string();
	S_RestartFluid();
}

//
// M_ChangeGENMIDI
//
//
static void M_ChangeGENMIDI(int keypressed)
{
	int op2_pos = -1;
	for(int i=0; i < (int)available_genmidis.size(); i++)
	{
		if (epi::case_cmp(s_genmidi.s, available_genmidis.at(i).generic_u8string()) == 0)
		{
			op2_pos = i;
			break;
		}
	}

	if (op2_pos < 0)
	{
		I_Warning("M_ChangeGENMIDI: Could not read list of available GENMIDIs. Falling back to default!\n");
		s_genmidi.s = "";
		return;
	}

	if (keypressed == KEYD_LEFTARROW || keypressed == KEYD_DPAD_LEFT || keypressed == KEYD_MENU_LEFT)
	{
		if (op2_pos - 1 >= 0)
			op2_pos--;
		else
			op2_pos = available_genmidis.size() - 1;
	}
	else if (keypressed == KEYD_RIGHTARROW || keypressed == KEYD_DPAD_RIGHT || keypressed == KEYD_MENU_RIGHT)
	{
		if (op2_pos + 1 >= (int)available_genmidis.size())
			op2_pos = 0;
		else
			op2_pos++;
	}

	// update cvar
	s_genmidi = available_genmidis.at(op2_pos).generic_u8string();
	S_RestartOPL();
}

//
// M_ChangeResSize
//
// -ACB- 1998/08/29 Resolution Changes...
//
static void M_ChangeResSize(int keypressed)
{
	if (keypressed == KEYD_LEFTARROW || keypressed == KEYD_DPAD_LEFT || keypressed == KEYD_MENU_LEFT)
	{
		R_IncrementResolution(&new_scrmode, RESINC_Size, -1);
	}
	else if (keypressed == KEYD_RIGHTARROW || keypressed == KEYD_DPAD_RIGHT || keypressed == KEYD_MENU_RIGHT)
	{
		R_IncrementResolution(&new_scrmode, RESINC_Size, +1);
	}
}

//
// M_ChangeResFull
//
// -AJA- 2005/01/02: Windowed vs Fullscreen
//
static void M_ChangeResFull(int keypressed)
{
	if (keypressed == KEYD_LEFTARROW || keypressed == KEYD_DPAD_LEFT || keypressed == KEYD_MENU_LEFT)
	{
		R_IncrementResolution(&new_scrmode, RESINC_DisplayMode, +1);
	}
	else if (keypressed == KEYD_RIGHTARROW || keypressed == KEYD_DPAD_RIGHT || keypressed == KEYD_MENU_RIGHT)
	{
		R_IncrementResolution(&new_scrmode, RESINC_DisplayMode, +1);
	}
}

//
// M_OptionSetResolution
//
static void M_OptionSetResolution(int keypressed)
{
	if (R_ChangeResolution(&new_scrmode))
	{
		R_SoftInitResolution();
	}
	else
	{
		std::string msg(epi::STR_Format(language["ModeSelErr"],
				new_scrmode.width, new_scrmode.height,
				(new_scrmode.depth < 20) ? 16 : 32));

		M_StartMessage(msg.c_str(), NULL, false);

///--  		testticker = -1;
		
///??	selectedscrmode = prevscrmode;
	}
}

///--  //
///--  // M_OptionTestResolution
///--  //
///--  static void M_OptionTestResolution(int keypressed)
///--  {
///--      R_ChangeResolution(selectedscrmode);
///--  	testticker = TICRATE * 3;
///--  }
///--  
///--  //
///--  // M_RestoreResSettings
///--  //
///--  static void M_RestoreResSettings(int keypressed)
///--  {
///--      R_ChangeResolution(prevscrmode);
///--  }

extern void M_NetHostBegun(void);
extern void M_NetJoinBegun(void);

void M_HostNetGame(int keypressed)
{
	option_menuon  = 0;
	netgame_menuon = 1;

	M_NetHostBegun();
}

void M_JoinNetGame(int keypressed)
{
#if 0
	option_menuon  = 0;
	netgame_menuon = 2;

	M_NetJoinBegun();
#endif
}

void M_Options(int choice)
{
	option_menuon = 1;
	fkey_menu = (choice == 1);
	// hack
	menu_crosshair = CLAMP(0, r_crosshair.d, 9);
	menu_crosscolor = CLAMP(0, r_crosscolor.d, 7);
	menu_crosssize = CLAMP(0, (r_crosssize.d / 8 - 1), 5);
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
