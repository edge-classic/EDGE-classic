//----------------------------------------------------------------------------
//  EDGE Option Menu Modification
//----------------------------------------------------------------------------
//
//  Copyright (c) 1999-2024 The EDGE Team.
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
// -ACB- 1998/08/10 Edited the menu's to reflect the fact that current_map
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


#include "epi_sdl.h"

#include "filesystem.h"
#include "font.h"
#include "str_util.h"
#include "str_compare.h"
#include "playlist.h"

#include "main.h"

#include "dm_state.h"
#include "e_input.h"
#include "g_game.h"
#include "hu_draw.h"
#include "hu_stuff.h"
#include "hu_style.h"
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

#include "defaults.h"

#include <math.h>

int  option_menuon = 0;
bool fkey_menu     = false;

extern ConsoleVariable m_language;
extern ConsoleVariable r_crosshair;
extern ConsoleVariable r_crosscolor;
extern ConsoleVariable r_crosssize;
extern ConsoleVariable s_genmidi;
extern ConsoleVariable midi_soundfont;
extern ConsoleVariable r_overlay;
extern ConsoleVariable g_erraticism;
extern ConsoleVariable r_doubleframes;
extern ConsoleVariable r_culling;
extern ConsoleVariable r_culldist;
extern ConsoleVariable r_cullfog;
extern ConsoleVariable g_cullthinkers;
extern ConsoleVariable r_maxdlights;
extern ConsoleVariable vsync;
extern ConsoleVariable g_bobbing;
extern ConsoleVariable g_gore;

// extern cvar_c automap_keydoor_text;

extern ConsoleVariable v_secbright;
extern ConsoleVariable gamma_correction;
extern ConsoleVariable r_titlescaling;
extern ConsoleVariable r_skystretch;
extern ConsoleVariable r_forceflatlighting;

static int monitor_size;

extern int    joystick_device;
extern ConsoleVariable joy_dead0;
extern ConsoleVariable joy_dead1;
extern ConsoleVariable joy_dead2;
extern ConsoleVariable joy_dead3;
extern ConsoleVariable joy_dead4;
extern ConsoleVariable joy_dead5;

extern SDL_Joystick *joystick_info;
extern int           I_JoyGetAxis(int n);

extern int entry_playing;

// submenus
static void M_KeyboardOptions(int keypressed, ConsoleVariable *cvar = nullptr);
static void M_VideoOptions(int keypressed, ConsoleVariable *cvar = nullptr);
static void M_GameplayOptions(int keypressed, ConsoleVariable *cvar = nullptr);
static void M_PerformanceOptions(int keypressed, ConsoleVariable *cvar = nullptr);
static void M_AccessibilityOptions(int keypressed, ConsoleVariable *cvar = nullptr);
static void M_AnalogueOptions(int keypressed, ConsoleVariable *cvar = nullptr);
static void M_SoundOptions(int keypressed, ConsoleVariable *cvar = nullptr);

static void M_Key2String(int key, char *deststring);

// Use these when a menu option does nothing special other than update a value
static void M_UpdateCVARFromFloat(int keypressed, ConsoleVariable *cvar = nullptr);
static void M_UpdateCVARFromInt(int keypressed, ConsoleVariable *cvar = nullptr);

// -ACB- 1998/08/09 "Does Map allow these changes?" procedures.
static void M_ChangeMonsterRespawn(int keypressed, ConsoleVariable *cvar = nullptr);
static void M_ChangeItemRespawn(int keypressed, ConsoleVariable *cvar = nullptr);
static void M_ChangeTrue3d(int keypressed, ConsoleVariable *cvar = nullptr);
static void M_ChangeAutoAim(int keypressed, ConsoleVariable *cvar = nullptr);
static void M_ChangeFastparm(int keypressed, ConsoleVariable *cvar = nullptr);
static void M_ChangeRespawn(int keypressed, ConsoleVariable *cvar = nullptr);
static void M_ChangePassMissile(int keypressed, ConsoleVariable *cvar = nullptr);
static void M_ChangeBobbing(int keypressed, ConsoleVariable *cvar = nullptr);
// static void M_ChangeBlood(int keypressed, cvar_c *cvar = nullptr);
static void M_ChangeMLook(int keypressed, ConsoleVariable *cvar = nullptr);
static void M_ChangeJumping(int keypressed, ConsoleVariable *cvar = nullptr);
static void M_ChangeCrouching(int keypressed, ConsoleVariable *cvar = nullptr);
static void M_ChangeExtra(int keypressed, ConsoleVariable *cvar = nullptr);
static void M_ChangeMonitorSize(int keypressed, ConsoleVariable *cvar = nullptr);
static void M_ChangeKicking(int keypressed, ConsoleVariable *cvar = nullptr);
static void M_ChangeWeaponSwitch(int keypressed, ConsoleVariable *cvar = nullptr);
static void M_ChangeMipMap(int keypressed, ConsoleVariable *cvar = nullptr);
static void M_ChangePCSpeakerMode(int keypressed, ConsoleVariable *cvar = nullptr);

// -ES- 1998/08/20 Added resolution options
// -ACB- 1998/08/29 Moved to top and tried different system

static void M_ResOptDrawer(Style *style, int topy, int bottomy, int dy, int centrex);
static void M_ResolutionOptions(int keypressed, ConsoleVariable *cvar = nullptr);
static void M_OptionSetResolution(int keypressed, ConsoleVariable *cvar = nullptr);
static void M_ChangeResSize(int keypressed, ConsoleVariable *cvar = nullptr);
static void M_ChangeResFull(int keypressed, ConsoleVariable *cvar = nullptr);

void M_HostNetGame(int keypressed, ConsoleVariable *cvar = nullptr);
void M_JoinNetGame(int keypressed, ConsoleVariable *cvar = nullptr);

static void M_LanguageDrawer(int x, int y, int deltay);
static void M_ChangeLanguage(int keypressed, ConsoleVariable *cvar = nullptr);
static void M_ChangeMIDIPlayer(int keypressed, ConsoleVariable *cvar = nullptr);
static void M_ChangeSoundfont(int keypressed, ConsoleVariable *cvar = nullptr);
static void M_ChangeGENMIDI(int keypressed, ConsoleVariable *cvar = nullptr);
static void InitMonitorSize();

static char YesNo[]  = "Off/On"; // basic on/off
static char CrossH[] = "None/Dot/Angle/Plus/Spiked/Thin/Cross/Carat/Circle/Double";
static char Respw[]  = "Teleport/Resurrect"; // monster respawning
static char MouseAxis[] =
    "Off/Turn/Turn (Reversed)/Look/Look (Inverted)/Walk/Walk (Reversed)/Strafe/Strafe (Reversed)/Fly/Fly (Inverted)";
static char JoyAxis[]        = "Off/Turn/Turn (Reversed)/Look (Inverted)/Look/Walk (Reversed)/Walk/Strafe/Strafe "
                               "(Reversed)/Fly (Inverted)/Fly/Left Trigger/Right Trigger";
static char JoyDevs[]        = "None/1/2/3/4/5/6";
static char JpgPng[]         = "JPEG/PNG"; // basic on/off
static char AAim[]           = "Off/On/Mlook";
static char Details[]        = "Low/Medium/High";
static char Hq2xMode[]       = "Off/UI Only/UI & Sprites/All";
static char SkyScaleMode[]   = "Mirror/Repeat/Stretch/Vanilla";
static char Invuls[]         = "Simple/Textured";
static char MonitSiz[]       = "5:4/4:3/3:2/16:10/16:9/21:9";
static char VidOverlays[]    = "None/Lines 1x/Lines 2x/Vertical 1x/Vertical 2x/Grill 1x/Grill 2x";

// for CVar enums
const char WIPE_EnumStr[] = "None/Melt/Crossfade/Pixelfade/Top/Bottom/Left/Right/Spooky/Doors";

static char StereoNess[] = "Off/On/Swapped";
static char MixChans[]   = "32/64/96/128/160/192/224/256";

static char CrosshairColor[] = "White/Blue/Green/Cyan/Red/Pink/Yellow/Orange";

static char SecBrights[] = "-50/-40/-30/-20/-10/Default/+10/+20/+30/+40/+50";

static char DLightMax[] = "Unlimited/20/40/60/80/100";

// Screen resolution changes
static DisplayMode new_scrmode;

extern std::vector<std::string> available_soundfonts;
extern std::vector<std::string> available_opl_banks;

//
//  OPTION STRUCTURES
//

typedef enum
{
    OPT_Plain      = 0, // 0 means plain text,
    OPT_Switch     = 1, // 1 is change a switch,
    OPT_Function   = 2, // 2 is call a function,
    OPT_Slider     = 3, // 3 is a slider,
    OPT_KeyConfig  = 4, // 4 is a key config,
    OPT_Boolean    = 5, // 5 is change a boolean switch
    OPT_FracSlider = 6, // 6 is a slider tied to a float
    OPT_NumTypes
} opt_type_e;

typedef struct optmenuitem_s
{
    opt_type_e type;

    char        name[48];
    const char *typenames;

    int   numtypes;
    void *switchvar;

    void (*routine)(int keypressed, ConsoleVariable *cvar);

    const char *help;

    ConsoleVariable *cvar_to_change;

    // For options tracking a floating point variable/range
    float increment;
    float min;
    float max;

    // Formatting string (only used for fracsliders atm)
    std::string fmt_string = "";
} optmenuitem_t;

typedef struct menuinfo_s
{
    // array of menu items
    optmenuitem_t *items;
    int            item_num;

    Style **style_var;

    int menu_center;

    // title information
    int title_x;

    // current position
    int pos;

    // key config, with left and right sister menus ?
    char key_page[20];

    // Printed name test
    const char *name = "DEFAULT";
} menuinfo_t;

// current menu and position
static menuinfo_t    *curr_menu;
static optmenuitem_t *curr_item;
static int            curr_key_menu;

static int keyscan;

static Style *opt_def_style;

static void M_ChangeMixChan(int keypressed, ConsoleVariable *cvar)
{
    S_ChangeChannelNum();
}

static int M_GetCurrentSwitchValue(optmenuitem_t *item)
{
    int retval = 0;

    switch (item->type)
    {
    case OPT_Boolean: {
        retval = *(bool *)item->switchvar ? 1 : 0;
        break;
    }

    case OPT_Switch: {
        retval = *(int *)(item->switchvar);
        break;
    }

    default: {
        EDGEError("M_GetCurrentSwitchValue: Menu item type is not a switch!\n");
        break;
    }
    }

    return retval;
}

//
//  MAIN MENU
//
#define LANGUAGE_POS 10
#define HOSTNET_POS  13

static optmenuitem_t mainoptions[] = {
    {OPT_Function, "Key Bindings", nullptr, 0, nullptr, M_KeyboardOptions, "Controls"},
    {OPT_Function, "Mouse / Controller", nullptr, 0, nullptr, M_AnalogueOptions, "AnalogueOptions"},
    {OPT_Function, "Gameplay Options", nullptr, 0, nullptr, M_GameplayOptions, "GameplayOptions"},
    {OPT_Function, "Performance Options", nullptr, 0, nullptr, M_PerformanceOptions, "PerformanceOptions"},
    {OPT_Function, "Accessibility Options", nullptr, 0, nullptr, M_AccessibilityOptions, "AccessibilityOptions"},
    {OPT_Plain, "", nullptr, 0, nullptr, nullptr, nullptr},
    {OPT_Function, "Sound Options", nullptr, 0, nullptr, M_SoundOptions, "SoundOptions"},
    {OPT_Function, "Video Options", nullptr, 0, nullptr, M_VideoOptions, "VideoOptions"},
#ifndef EDGE_WEB
    {OPT_Function, "Screen Options", nullptr, 0, nullptr, M_ResolutionOptions, "ChangeRes"},
#endif
    {OPT_Plain, "", nullptr, 0, nullptr, nullptr, nullptr},
    {OPT_Function, "Language", nullptr, 0, nullptr, M_ChangeLanguage, nullptr},
    {OPT_Switch, "Messages", YesNo, 2, &show_messages, nullptr, "Messages"},
    {OPT_Plain, "", nullptr, 0, nullptr, nullptr, nullptr},
    {OPT_Function, "Start Bot Match", nullptr, 0, nullptr, M_HostNetGame, nullptr},
    {OPT_Plain, "", nullptr, 0, nullptr, nullptr, nullptr},
    {OPT_Function, "Reset to Defaults", nullptr, 0, nullptr, M_ResetDefaults, nullptr}};

static menuinfo_t main_optmenu = {
    mainoptions, sizeof(mainoptions) / sizeof(optmenuitem_t), &opt_def_style, 164, 108, 0, "", language["MenuOptions"]};

//
//  VIDEO OPTIONS
//
// -ACB- 1998/07/15 Altered menu structure

static optmenuitem_t vidoptions[] = {
    {OPT_FracSlider, "Gamma Adjustment", nullptr, 0, &gamma_correction.f_, M_UpdateCVARFromFloat, nullptr, &gamma_correction, 0.10f, -1.0f, 1.0f,
     "%0.2f"},
    {OPT_Switch, "Sector Brightness", SecBrights, 11, &v_secbright.d_, M_UpdateCVARFromInt, nullptr, &v_secbright},
    {OPT_Boolean, "Lighting Mode", "Indexed/Flat", 2, &r_forceflatlighting.d_, M_UpdateCVARFromInt, nullptr,
     &r_forceflatlighting},
    {OPT_Switch, "Framerate Target", "35 FPS/70 FPS", 2, &r_doubleframes.d_, M_UpdateCVARFromInt, nullptr, &r_doubleframes},
    {OPT_Switch, "Smoothing", YesNo, 2, &var_smoothing, M_ChangeMipMap, nullptr},
    {OPT_Switch, "Upscale Textures", Hq2xMode, 4, &hq2x_scaling, M_ChangeMipMap,
     "Only affects paletted (Doom format) textures"},
    {OPT_Switch, "Title/Intermission Scaling", "Normal/Fill Border", 2, &r_titlescaling.d_, M_UpdateCVARFromInt, nullptr,
     &r_titlescaling},
    {OPT_Switch, "Sky Scaling", SkyScaleMode, 4, &r_skystretch.d_, M_UpdateCVARFromInt,
     "Vanilla will be forced when Mouselook is Off", &r_skystretch},
    {OPT_Switch, "Dynamic Lighting", YesNo, 2, &use_dlights, nullptr, nullptr},
    {OPT_Switch, "Overlay", VidOverlays, 7, &r_overlay.d_, M_UpdateCVARFromInt, nullptr, &r_overlay},
    {OPT_Switch, "Crosshair", CrossH, 10, &r_crosshair.d_, M_UpdateCVARFromInt, nullptr, &r_crosshair},
    {OPT_Switch, "Crosshair Color", CrosshairColor, 8, &r_crosscolor.d_, M_UpdateCVARFromInt, nullptr, &r_crosscolor},
    {OPT_FracSlider, "Crosshair Size", nullptr, 0, &r_crosssize.f_, M_UpdateCVARFromFloat, nullptr, &r_crosssize, 1.0f, 2.0f,
     64.0f, "%g Pixels"},
    {OPT_Boolean, "Map Rotation", YesNo, 2, &rotate_map, nullptr, nullptr},
    {OPT_Switch, "Invulnerability", Invuls, NUM_INVULFX, &var_invul_fx, nullptr, nullptr},
#ifndef EDGE_WEB
    {OPT_Switch, "Wipe method", WIPE_EnumStr, WIPE_NUMWIPES, &wipe_method, nullptr, nullptr},
#endif
    {OPT_Boolean, "Screenshot Format", JpgPng, 2, &png_scrshots, nullptr, nullptr},
    {OPT_Switch, "Animated Liquid Type", "Vanilla/SMMU/SMMU+Swirl/Parallax", 4, &swirling_flats, nullptr, nullptr},
};

static menuinfo_t video_optmenu = {
    vidoptions, sizeof(vidoptions) / sizeof(optmenuitem_t), &opt_def_style, 150, 77, 0, "", language["MenuVideo"]};

//
//  SCREEN OPTIONS MENU
//
static optmenuitem_t resoptions[] = {
    {OPT_Plain, "", nullptr, 0, nullptr, nullptr, nullptr},
    {OPT_Switch, "V-Sync", "Off/Standard/Adaptive", 3, &vsync.d_, M_UpdateCVARFromInt,
     "Will fallback to Standard if Adaptive is not supported", &vsync},
    {OPT_Switch, "Aspect Ratio", MonitSiz, 6, &monitor_size, M_ChangeMonitorSize, "Only applies to Fullscreen Modes"},
    {OPT_Function, "New Mode", nullptr, 0, nullptr, M_ChangeResFull, nullptr},
    {OPT_Function, "New Resolution", nullptr, 0, nullptr, M_ChangeResSize, nullptr},
    {OPT_Function, "Apply Mode/Resolution", nullptr, 0, nullptr, M_OptionSetResolution, nullptr},
    {OPT_Plain, "", nullptr, 0, nullptr, nullptr, nullptr},
    {OPT_Plain, "", nullptr, 0, nullptr, nullptr, nullptr},
    {OPT_Plain, "", nullptr, 0, nullptr, nullptr, nullptr}};

static menuinfo_t res_optmenu = {
    resoptions, sizeof(resoptions) / sizeof(optmenuitem_t), &opt_def_style, 150, 77, 3, "", language["MenuResolution"]};

//
//  MOUSE OPTIONS
//
// -ACB- 1998/06/15 Added new mouse menu
// -KM- 1998/09/01 Changed to an analogue menu.  Must change those names
// -ACB- 1998/07/15 Altered menu structure
//

static optmenuitem_t analogueoptions[] = {
    {OPT_Switch, "Mouse X Axis", MouseAxis, 11, &mouse_x_axis, nullptr, nullptr},
    {OPT_Switch, "Mouse Y Axis", MouseAxis, 11, &mouse_y_axis, nullptr, nullptr},
    {OPT_FracSlider, "X Sensitivity", nullptr, 0, &mouse_x_sensitivity.f_, M_UpdateCVARFromFloat, nullptr, &mouse_x_sensitivity, 0.25f, 1.0f,
     15.0f, "%0.2f"},
    {OPT_FracSlider, "Y Sensitivity", nullptr, 0, &mouse_y_sensitivity.f_, M_UpdateCVARFromFloat, nullptr, &mouse_y_sensitivity, 0.25f, 1.0f,
     15.0f, "%0.2f"},
    {OPT_Plain, "", nullptr, 0, nullptr, nullptr, nullptr},
    {OPT_Switch, "Gamepad", JoyDevs, 5, &joystick_device, nullptr, nullptr},
    {OPT_Switch, "Left Stick X", JoyAxis, 13, &joystick_axis[0], nullptr, nullptr},
    {OPT_Switch, "Left Stick Y", JoyAxis, 13, &joystick_axis[1], nullptr, nullptr},
    {OPT_Switch, "Right Stick X", JoyAxis, 13, &joystick_axis[2], nullptr, nullptr},
    {OPT_Switch, "Right Stick Y", JoyAxis, 13, &joystick_axis[3], nullptr, nullptr},
    {OPT_FracSlider, "Left X Deadzone", nullptr, 0, &joy_dead0.f_, M_UpdateCVARFromFloat, nullptr, &joy_dead0, 0.01f, 0.0f,
     0.99f, "%0.2f"},
    {OPT_FracSlider, "Left Y Deadzone", nullptr, 0, &joy_dead1.f_, M_UpdateCVARFromFloat, nullptr, &joy_dead1, 0.01f, 0.0f,
     0.99f, "%0.2f"},
    {OPT_FracSlider, "Right X Deadzone", nullptr, 0, &joy_dead2.f_, M_UpdateCVARFromFloat, nullptr, &joy_dead2, 0.01f, 0.0f,
     0.99f, "%0.2f"},
    {OPT_FracSlider, "Right Y Deadzone", nullptr, 0, &joy_dead3.f_, M_UpdateCVARFromFloat, nullptr, &joy_dead3, 0.01f, 0.0f,
     0.99f, "%0.2f"},
    {OPT_FracSlider, "Left Trigger Deadzone", nullptr, 0, &joy_dead4.f_, M_UpdateCVARFromFloat, nullptr, &joy_dead4, 0.01f,
     0.0f, 0.99f, "%0.2f"},
    {OPT_FracSlider, "Right Trigger Deadzone", nullptr, 0, &joy_dead5.f_, M_UpdateCVARFromFloat, nullptr, &joy_dead5, 0.01f,
     0.0f, 0.99f, "%0.2f"},
    {OPT_Plain, "", nullptr, 0, nullptr, nullptr, nullptr},
    {OPT_FracSlider, "Turning Speed", nullptr, 0, &turn_speed.f_, M_UpdateCVARFromFloat, nullptr, &turn_speed, 0.10f, 0.10f,
     3.0f, "%0.2f"},
    {OPT_FracSlider, "Vertical Look Speed", nullptr, 0, &vertical_look_speed.f_, M_UpdateCVARFromFloat, nullptr, &vertical_look_speed, 0.10f,
     0.10f, 3.0f, "%0.2f"},
    {OPT_FracSlider, "Forward Move Speed", nullptr, 0, &forward_speed.f_, M_UpdateCVARFromFloat, nullptr, &forward_speed, 0.10f,
     0.10f, 3.0f, "%0.2f"},
    {OPT_FracSlider, "Side Move Speed", nullptr, 0, &side_speed.f_, M_UpdateCVARFromFloat, nullptr, &side_speed, 0.10f, 0.10f,
     3.0f, "%0.2f"},
};

static menuinfo_t analogue_optmenu = {
    analogueoptions,      sizeof(analogueoptions) / sizeof(optmenuitem_t), &opt_def_style, 150, 75, 1, "",
    language["MenuMouse"]};

//
//  SOUND OPTIONS
//
// -AJA- 2007/03/14 Added new sound menu
//
static optmenuitem_t soundoptions[] = {
    {OPT_FracSlider, "Sound Volume", nullptr, 0, &sfx_volume.f_, M_UpdateCVARFromFloat, nullptr, &sfx_volume, 0.05f, 0.0f,
     1.0f, "%0.2f"},
    {OPT_FracSlider, "Movie/Music Volume", nullptr, 0, &mus_volume.f_, M_UpdateCVARFromFloat, nullptr, &mus_volume, 0.05f, 0.0f,
     1.0f, "%0.2f"},
    {OPT_Plain, "", nullptr, 0, nullptr, nullptr, nullptr},
    {OPT_Switch, "Stereo", StereoNess, 3, &var_sound_stereo, nullptr, "NeedRestart"},
    {OPT_Plain, "", nullptr, 0, nullptr, nullptr, nullptr},
    {OPT_Switch, "MIDI Player", "Fluidlite/Opal", 2, &var_midi_player, M_ChangeMIDIPlayer, nullptr},
    {OPT_Function, "Fluidlite Soundfont", nullptr, 0, nullptr, M_ChangeSoundfont, nullptr},
    {OPT_Function, "Opal Instrument Bank", nullptr, 0, nullptr, M_ChangeGENMIDI, nullptr},
    {OPT_Boolean, "PC Speaker Mode", YesNo, 2, &var_pc_speaker_mode, M_ChangePCSpeakerMode,
     "Music will be Off while this is enabled"},
    {OPT_Plain, "", nullptr, 0, nullptr, nullptr, nullptr},
    {OPT_Boolean, "Dynamic Reverb", YesNo, 2, &dynamic_reverb, nullptr, nullptr},
    {OPT_Plain, "", nullptr, 0, nullptr, nullptr, nullptr},
    {OPT_Switch, "Mix Channels", MixChans, 8, &var_mix_channels, M_ChangeMixChan, nullptr},
    {OPT_Boolean, "Precache SFX", YesNo, 2, &var_cache_sfx, nullptr, "NeedRestart"},
    {OPT_Plain, "", nullptr, 0, nullptr, nullptr, nullptr},
};

static menuinfo_t sound_optmenu = {
    soundoptions, sizeof(soundoptions) / sizeof(optmenuitem_t), &opt_def_style, 150, 75, 0, "", language["MenuSound"]};

//
//  F4 SOUND OPTIONS
//
//
static optmenuitem_t f4soundoptions[] = {
    {OPT_FracSlider, "Sound Volume", nullptr, 0, &sfx_volume.f_, M_UpdateCVARFromFloat, nullptr, &sfx_volume, 0.05f, 0.0f,
     1.0f, "%0.2f"},
    {OPT_FracSlider, "Music Volume", nullptr, 0, &mus_volume.f_, M_UpdateCVARFromFloat, nullptr, &mus_volume, 0.05f, 0.0f,
     1.0f, "%0.2f"},
};

static menuinfo_t f4sound_optmenu = {
    f4soundoptions,       sizeof(f4soundoptions) / sizeof(optmenuitem_t), &opt_def_style, 150, 75, 0, "",
    language["MenuSound"]};

//
//  GAMEPLAY OPTIONS
//
// -ACB- 1998/07/15 Altered menu structure
// -KM- 1998/07/21 Change blood to switch
//
static optmenuitem_t playoptions[] = {
    {OPT_Boolean, "Pistol Starts", YesNo, 2, &pistol_starts, nullptr, nullptr},

    {OPT_Boolean, "Mouse Look", YesNo, 2, &global_flags.mlook, M_ChangeMLook, nullptr},

    {OPT_Switch, "Autoaim", AAim, 3, &global_flags.autoaim, M_ChangeAutoAim, nullptr},

    {OPT_Boolean, "Jumping", YesNo, 2, &global_flags.jump, M_ChangeJumping, nullptr},

    {OPT_Boolean, "Crouching", YesNo, 2, &global_flags.crouch, M_ChangeCrouching, nullptr},

    {OPT_Boolean, "Weapon Kick", YesNo, 2, &global_flags.kicking, M_ChangeKicking, nullptr},

    {OPT_Boolean, "Weapon Auto-Switch", YesNo, 2, &global_flags.weapon_switch, M_ChangeWeaponSwitch, nullptr},

    {OPT_Boolean, "Obituary Messages", YesNo, 2, &var_obituaries, nullptr, nullptr},

    {OPT_Switch, "Blood Level", "Normal/Extra/None", 3, &g_gore.d_, M_UpdateCVARFromInt, "Blood", &g_gore},

    {OPT_Boolean, "Extras", YesNo, 2, &global_flags.have_extra, M_ChangeExtra, nullptr},

    {OPT_Boolean, "True 3D Gameplay", YesNo, 2, &global_flags.true3dgameplay, M_ChangeTrue3d, "True3d"},

    {OPT_Boolean, "Shoot-thru Scenery", YesNo, 2, &global_flags.pass_missile, M_ChangePassMissile, nullptr},

    {OPT_Boolean, "Erraticism", YesNo, 2, &g_erraticism.d_, M_UpdateCVARFromInt,
     "Time only advances when you move or fire", &g_erraticism},

    {OPT_FracSlider, "Gravity", nullptr, 0, &g_gravity.f_, M_UpdateCVARFromFloat, "Gravity", &g_gravity, 0.10f, 0.0f, 2.0f,
     "%gx"},

    {OPT_Boolean, "Respawn Enemies", YesNo, 2, &global_flags.respawn, M_ChangeRespawn, nullptr},

    {OPT_Boolean, "Enemy Respawn Mode", Respw, 2, &global_flags.res_respawn, M_ChangeMonsterRespawn, nullptr},

    {OPT_Boolean, "Item Respawn", YesNo, 2, &global_flags.itemrespawn, M_ChangeItemRespawn, nullptr},

    {OPT_Boolean, "Fast Monsters", YesNo, 2, &global_flags.fastparm, M_ChangeFastparm, nullptr}};

static menuinfo_t gameplay_optmenu = {
    playoptions, sizeof(playoptions) / sizeof(optmenuitem_t), &opt_def_style, 160, 46, 0, "", language["MenuGameplay"]};

//
//  PERFORMANCE OPTIONS
//
//
static optmenuitem_t perfoptions[] = {
    {OPT_Switch, "Detail Level", Details, 3, &detail_level, M_ChangeMipMap, nullptr},
    {OPT_Boolean, "Draw Distance Culling", YesNo, 2, &r_culling.d_, M_UpdateCVARFromInt,
     "Sector/Level Fog will be disabled when this is On", &r_culling},
    {OPT_FracSlider, "Maximum Draw Distance", nullptr, 0, &r_culldist.f_, M_UpdateCVARFromFloat,
     "Only effective when Draw Distance Culling is On", &r_culldist, 200.0f, 1000.0f, 8000.0f, "%g Units"},
    {OPT_Switch, "Outdoor Culling Fog Color", "Match Sky/White/Grey/Black", 4, &r_cullfog.d_, M_UpdateCVARFromInt,
     "Only effective when Draw Distance Culling is On", &r_cullfog},
    {OPT_Boolean, "Slow Thinkers Over Distance", YesNo, 2, &g_cullthinkers.d_, M_UpdateCVARFromInt,
     "Only recommended for extreme monster/projectile counts", &g_cullthinkers},
    {OPT_Switch, "Maximum Dynamic Lights", DLightMax, 6, &r_maxdlights.d_, M_UpdateCVARFromInt,
     "Control how many dynamic lights are rendered per tick", &r_maxdlights},
};

static menuinfo_t perf_optmenu = {perfoptions,
                                  sizeof(perfoptions) / sizeof(optmenuitem_t),
                                  &opt_def_style,
                                  160,
                                  46,
                                  0,
                                  "",
                                  language["MenuPerformance"]};

//
//  ACCESSIBILITY OPTIONS
//
//
static optmenuitem_t accessibilityoptions[] = {
    {OPT_Switch, "View Bobbing", "Full/Head Only/Weapon Only/None", 4, &g_bobbing.d_, M_ChangeBobbing,
     "May help with motion sickness"},
    {OPT_Switch, "Reduce Flashing", YesNo, 2, &reduce_flash, nullptr, "May help with epilepsy or photosensitivity"},
    {OPT_Boolean, "Automap: Keyed Doors Pulse", YesNo, 2, &automap_keydoor_blink, nullptr, "Can help locate doors more easily"},
    {OPT_Switch, "Automap: Keyed Doors Overlay", "Nothing/Text/Graphic", 3, &automap_keydoor_text.d_, M_UpdateCVARFromInt,
     "Required key shown visually", &automap_keydoor_text},
};

static menuinfo_t accessibility_optmenu = {
    accessibilityoptions,         sizeof(accessibilityoptions) / sizeof(optmenuitem_t), &opt_def_style, 160, 46, 0, "",
    language["MenuAccessibility"]};

//
//  KEY CONFIG : MOVEMENT
//
// -ACB- 1998/07/15 Altered menuinfo struct
// -KM- 1998/07/10 Used better names :-)
//
static optmenuitem_t move_keyconfig[] = {
    {OPT_KeyConfig, "Walk Forward", nullptr, 0, &key_up, nullptr, nullptr},
    {OPT_KeyConfig, "Walk Backwards", nullptr, 0, &key_down, nullptr, nullptr},
    {OPT_Plain, "", nullptr, 0, nullptr, nullptr, nullptr},
    {OPT_KeyConfig, "Strafe Left", nullptr, 0, &key_strafe_left, nullptr, nullptr},
    {OPT_KeyConfig, "Strafe Right", nullptr, 0, &key_strafe_right, nullptr, nullptr},
    {OPT_Plain, "", nullptr, 0, nullptr, nullptr, nullptr},
    {OPT_KeyConfig, "Turn Left", nullptr, 0, &key_left, nullptr, nullptr},
    {OPT_KeyConfig, "Turn Right", nullptr, 0, &key_right, nullptr, nullptr},
    {OPT_Plain, "", nullptr, 0, nullptr, nullptr, nullptr},
    {OPT_KeyConfig, "Up / Jump", nullptr, 0, &key_fly_up, nullptr, nullptr},
    {OPT_KeyConfig, "Down / Crouch", nullptr, 0, &key_fly_down, nullptr, nullptr},
};

static menuinfo_t movement_optmenu = {
    move_keyconfig,         sizeof(move_keyconfig) / sizeof(optmenuitem_t), &opt_def_style, 140, 98, 0, "Movement",
    language["MenuBinding"]};

//
//  KEY CONFIG : ATTACK + LOOK
//
// -ACB- 1998/07/15 Altered menuinfo struct
// -ES- 1999/03/28 Added Zoom Key
//
static optmenuitem_t attack_keyconfig[] = {
    {OPT_KeyConfig, "Primary Attack", nullptr, 0, &key_fire, nullptr, nullptr},
    {OPT_KeyConfig, "Secondary Attack", nullptr, 0, &key_second_attack, nullptr, nullptr},
    {OPT_KeyConfig, "Third Attack", nullptr, 0, &key_third_attack, nullptr, nullptr},
    {OPT_KeyConfig, "Fourth Attack", nullptr, 0, &key_fourth_attack, nullptr, nullptr},
    {OPT_KeyConfig, "Next Weapon", nullptr, 0, &key_next_weapon, nullptr, nullptr},
    {OPT_KeyConfig, "Previous Weapon", nullptr, 0, &key_previous_weapon, nullptr, nullptr},
    {OPT_KeyConfig, "Weapon Reload", nullptr, 0, &key_reload, nullptr, nullptr},
    {OPT_Plain, "", nullptr, 0, nullptr, nullptr, nullptr},
    {OPT_KeyConfig, "Zoom in/out", nullptr, 0, &key_zoom, nullptr, nullptr},
};

static menuinfo_t attack_optmenu = {
    attack_keyconfig,       sizeof(attack_keyconfig) / sizeof(optmenuitem_t), &opt_def_style, 140, 98, 0, "Attack",
    language["MenuBinding"]};

static optmenuitem_t look_keyconfig[] = {
    {OPT_KeyConfig, "Look Up", nullptr, 0, &key_look_up, nullptr, nullptr},
    {OPT_KeyConfig, "Look Down", nullptr, 0, &key_look_down, nullptr, nullptr},
    {OPT_KeyConfig, "Center View", nullptr, 0, &key_look_center, nullptr, nullptr},
    {OPT_KeyConfig, "Mouse Look", nullptr, 0, &key_mouselook, nullptr, nullptr},
};

static menuinfo_t look_optmenu = {
    look_keyconfig,         sizeof(look_keyconfig) / sizeof(optmenuitem_t), &opt_def_style, 140, 98, 0, "Look",
    language["MenuBinding"]};

//
//  KEY CONFIG : OTHER STUFF
//
static optmenuitem_t other_keyconfig[] = {
    {OPT_KeyConfig, "Use Item", nullptr, 0, &key_use, nullptr, nullptr},
    {OPT_KeyConfig, "Strafe", nullptr, 0, &key_strafe, nullptr, nullptr},
    {OPT_KeyConfig, "Run", nullptr, 0, &key_speed, nullptr, nullptr},
    {OPT_KeyConfig, "Toggle Autorun", nullptr, 0, &key_autorun, nullptr, nullptr},
    {OPT_KeyConfig, "180 degree turn", nullptr, 0, &key_180, nullptr, nullptr},
    {OPT_Plain, "", nullptr, 0, nullptr, nullptr, nullptr},
    {OPT_KeyConfig, "Map Toggle", nullptr, 0, &key_map, nullptr, nullptr},
    {OPT_KeyConfig, "Action 1", nullptr, 0, &key_action1, nullptr, nullptr},
    {OPT_KeyConfig, "Action 2", nullptr, 0, &key_action2, nullptr, nullptr},

    ///	{OPT_KeyConfig, "Multiplayer Talk", nullptr, 0, &key_talk, nullptr, nullptr},
};

static menuinfo_t otherkey_optmenu = {
    other_keyconfig,        sizeof(other_keyconfig) / sizeof(optmenuitem_t), &opt_def_style, 140, 98, 0, "Other Keys",
    language["MenuBinding"]};

//
//  KEY CONFIG : WEAPONS
//
static optmenuitem_t weapon_keyconfig[] = {
    {OPT_KeyConfig, "Weapon 1", nullptr, 0, &key_weapons[1], nullptr, nullptr},
    {OPT_KeyConfig, "Weapon 2", nullptr, 0, &key_weapons[2], nullptr, nullptr},
    {OPT_KeyConfig, "Weapon 3", nullptr, 0, &key_weapons[3], nullptr, nullptr},
    {OPT_KeyConfig, "Weapon 4", nullptr, 0, &key_weapons[4], nullptr, nullptr},
    {OPT_KeyConfig, "Weapon 5", nullptr, 0, &key_weapons[5], nullptr, nullptr},
    {OPT_Plain, "", nullptr, 0, nullptr, nullptr, nullptr},
    {OPT_KeyConfig, "Weapon 6", nullptr, 0, &key_weapons[6], nullptr, nullptr},
    {OPT_KeyConfig, "Weapon 7", nullptr, 0, &key_weapons[7], nullptr, nullptr},
    {OPT_KeyConfig, "Weapon 8", nullptr, 0, &key_weapons[8], nullptr, nullptr},
    {OPT_KeyConfig, "Weapon 9", nullptr, 0, &key_weapons[9], nullptr, nullptr},
    {OPT_KeyConfig, "Weapon 0", nullptr, 0, &key_weapons[0], nullptr, nullptr},
};

static menuinfo_t weapon_optmenu = {
    weapon_keyconfig,       sizeof(weapon_keyconfig) / sizeof(optmenuitem_t), &opt_def_style, 140, 98, 0, "Weapon Keys",
    language["MenuBinding"]};

//
//  KEY CONFIG : AUTOMAP
//
static optmenuitem_t automap_keyconfig[] = {
    {OPT_KeyConfig, "Pan Up", nullptr, 0, &key_automap_up, nullptr, nullptr},
    {OPT_KeyConfig, "Pan Down", nullptr, 0, &key_automap_down, nullptr, nullptr},
    {OPT_KeyConfig, "Pan Left", nullptr, 0, &key_automap_left, nullptr, nullptr},
    {OPT_KeyConfig, "Pan Right", nullptr, 0, &key_automap_right, nullptr, nullptr},
    {OPT_Plain, "", nullptr, 0, nullptr, nullptr, nullptr},
    {OPT_KeyConfig, "Follow Mode", nullptr, 0, &key_automap_follow, nullptr, nullptr},
    {OPT_KeyConfig, "Show Grid", nullptr, 0, &key_automap_grid, nullptr, nullptr},
    {OPT_KeyConfig, "Zoom In", nullptr, 0, &key_automap_zoom_in, nullptr, nullptr},
    {OPT_KeyConfig, "Zoom Out", nullptr, 0, &key_automap_zoom_out, nullptr, nullptr},
    {OPT_KeyConfig, "Add Mark", nullptr, 0, &key_automap_mark, nullptr, nullptr},
    {OPT_KeyConfig, "Clear Marks", nullptr, 0, &key_automap_clear, nullptr, nullptr},
};

static menuinfo_t automap_optmenu = {automap_keyconfig,
                                     sizeof(automap_keyconfig) / sizeof(optmenuitem_t),
                                     &opt_def_style,
                                     140,
                                     98,
                                     0,
                                     "Automap Keys",
                                     language["MenuBinding"]};

//
//  KEY CONFIG : INVENTORY
//
static optmenuitem_t inventory_keyconfig[] = {
    {OPT_KeyConfig, "Previous Item", nullptr, 0, &key_inventory_previous, nullptr, nullptr},
    {OPT_KeyConfig, "Use Item", nullptr, 0, &key_inventory_use, nullptr, nullptr},
    {OPT_KeyConfig, "Next Item", nullptr, 0, &key_inventory_next, nullptr, nullptr},
};

static menuinfo_t inventory_optmenu = {inventory_keyconfig,
                                       sizeof(inventory_keyconfig) / sizeof(optmenuitem_t),
                                       &opt_def_style,
                                       140,
                                       98,
                                       0,
                                       "Inventory",
                                       language["MenuBinding"]};

//
//  KEY CONFIG : PROGRAM
//
static optmenuitem_t program_keyconfig1[] = {
    {OPT_KeyConfig, "Screenshot", nullptr, 0, &key_screenshot, nullptr, nullptr},
    {OPT_KeyConfig, "Console", nullptr, 0, &key_console, nullptr, nullptr},
    {OPT_KeyConfig, "Pause", nullptr, 0, &key_pause, nullptr, nullptr},
    {OPT_KeyConfig, "Save Game", nullptr, 0, &key_save_game, nullptr, nullptr},
    {OPT_KeyConfig, "Load Game", nullptr, 0, &key_load_game, nullptr, nullptr},
    {OPT_Plain, "", nullptr, 0, nullptr, nullptr, nullptr},
    {OPT_KeyConfig, "Sound Controls", nullptr, 0, &key_sound_controls, nullptr, nullptr},
    {OPT_KeyConfig, "Options", nullptr, 0, &key_options_menu, nullptr, nullptr},
    {OPT_KeyConfig, "Quicksave", nullptr, 0, &key_quick_save, nullptr, nullptr},
};

static menuinfo_t program_optmenu1 = {program_keyconfig1,
                                      sizeof(program_keyconfig1) / sizeof(optmenuitem_t),
                                      &opt_def_style,
                                      140,
                                      98,
                                      0,
                                      "Program (1/2)",
                                      language["MenuBinding"]};

//
//  KEY CONFIG : PROGRAM
//
static optmenuitem_t program_keyconfig2[] = {
    {OPT_KeyConfig, "End Game", nullptr, 0, &key_end_game, nullptr, nullptr},
    {OPT_KeyConfig, "Toggle Messages", nullptr, 0, &key_message_toggle, nullptr, nullptr},
    {OPT_KeyConfig, "Quickload", nullptr, 0, &key_quick_load, nullptr, nullptr},
    {OPT_Plain, "", nullptr, 0, nullptr, nullptr, nullptr},
    {OPT_KeyConfig, "Quit EDGE", nullptr, 0, &key_quit_edge, nullptr, nullptr},
    {OPT_KeyConfig, "Toggle Gamma", nullptr, 0, &key_gamma_toggle, nullptr, nullptr},
    {OPT_KeyConfig, "Show Players", nullptr, 0, &key_show_players, nullptr, nullptr},
};

static menuinfo_t program_optmenu2 = {program_keyconfig2,
                                      sizeof(program_keyconfig2) / sizeof(optmenuitem_t),
                                      &opt_def_style,
                                      140,
                                      98,
                                      0,
                                      "Program (2/2)",
                                      language["MenuBinding"]};

/*
 * ALL KEYBOARD MENUS
 */
#define NUM_KEY_MENUS 9

static menuinfo_t *all_key_menus[NUM_KEY_MENUS] = {&movement_optmenu,  &attack_optmenu,   &look_optmenu,
                                                   &otherkey_optmenu,  &weapon_optmenu,   &automap_optmenu,
                                                   &inventory_optmenu, &program_optmenu1, &program_optmenu2};

static char keystring1[] = "Enter/A Button to change, Backspace/Back Button to clear";
static char keystring2[] = "Press a key for this action";

//
// M_OptCheckNetgame
//
// Sets the first option to be "Leave Game" or "Multiplayer Game"
// depending on whether we are playing a game or not.
//
void M_OptCheckNetgame(void)
{
    if (game_state >= GS_LEVEL)
    {
        strcpy(mainoptions[HOSTNET_POS + 0].name, "Leave Game");
        mainoptions[HOSTNET_POS + 0].routine = &M_EndGame;
        mainoptions[HOSTNET_POS + 0].help    = nullptr;

        //		strcpy(mainoptions[HOSTNET_POS+1].name, "");
        //		mainoptions[HOSTNET_POS+1].type = OPT_Plain;
        //		mainoptions[HOSTNET_POS+1].routine = nullptr;
        //		mainoptions[HOSTNET_POS+1].help = nullptr;
    }
    else
    {
        strcpy(mainoptions[HOSTNET_POS + 0].name, "Start Bot Match");
        mainoptions[HOSTNET_POS + 0].routine = &M_HostNetGame;
        mainoptions[HOSTNET_POS + 0].help    = nullptr;

        //		strcpy(mainoptions[HOSTNET_POS+1].name, "Join Net Game");
        //		mainoptions[HOSTNET_POS+1].type = OPT_Function;
        //		mainoptions[HOSTNET_POS+1].routine = &M_JoinNetGame;
        //		mainoptions[HOSTNET_POS+1].help = nullptr;
    }
}

void M_OptMenuInit()
{
    option_menuon = 0;
    curr_menu     = &main_optmenu;
    curr_item     = curr_menu->items + curr_menu->pos;
    curr_key_menu = 0;
    keyscan       = 0;

    InitMonitorSize();

    // load styles
    StyleDefinition *def;

    def = styledefs.Lookup("OPTIONS");
    if (!def)
        def = default_style;
    opt_def_style = hud_styles.Lookup(def);

    // Lobo 2022: load our ddflang stuff
    main_optmenu.name          = language["MenuOptions"];
    video_optmenu.name         = language["MenuVideo"];
    res_optmenu.name           = language["MenuResolution"];
    analogue_optmenu.name      = language["MenuMouse"];
    sound_optmenu.name         = language["MenuSound"];
    f4sound_optmenu.name       = language["MenuSound"];
    gameplay_optmenu.name      = language["MenuGameplay"];
    perf_optmenu.name          = language["MenuPerformance"];
    accessibility_optmenu.name = language["MenuAccessibility"];
    movement_optmenu.name      = language["MenuBinding"];
    attack_optmenu.name        = language["MenuBinding"];
    look_optmenu.name          = language["MenuBinding"];
    otherkey_optmenu.name      = language["MenuBinding"];
    weapon_optmenu.name        = language["MenuBinding"];
    automap_optmenu.name       = language["MenuBinding"];
    inventory_optmenu.name     = language["MenuBinding"];
    program_optmenu1.name      = language["MenuBinding"];
    program_optmenu2.name      = language["MenuBinding"];

    // Restore the config setting.
    // M_ChangeBlood(-1);
}

void M_OptTicker(void)
{
    // nothing needed
}

void M_OptDrawer()
{
    char         tempstring[80];
    int          curry, deltay, menutop;
    unsigned int k;

    Style *style = curr_menu->style_var[0];
    SYS_ASSERT(style);

    style->DrawBackground();

    if (!style->fonts_[StyleDefinition::kTextSectionText])
        return;

    int fontType;

    if (!style->fonts_[StyleDefinition::kTextSectionHeader])
        fontType = StyleDefinition::kTextSectionText;
    else
        fontType = StyleDefinition::kTextSectionHeader;

    int   font_h;
    int   CenterX;
    float TEXTscale = style->definition_->text_[fontType].scale_;

    font_h = style->fonts_[fontType]->NominalHeight();
    font_h *= TEXTscale;
    menutop = font_h / 2;

    CenterX = 160;
    CenterX -= (style->fonts_[fontType]->StringWidth(curr_menu->name) * TEXTscale * 1.5) / 2;

    // Lobo 2022
    HUDWriteText(style, fontType, CenterX, menutop, curr_menu->name, 1.5);

    fontType  = StyleDefinition::kTextSectionText;
    TEXTscale = style->definition_->text_[fontType].scale_;
    font_h    = style->fonts_[fontType]->NominalHeight();
    font_h *= TEXTscale;
    menutop = 68 - ((curr_menu->item_num * font_h) / 2);
    if (curr_menu->key_page[0])
        menutop = 9 * font_h / 2;

    // These don't seem used after this point in the function - Dasho
    // CenterX = 160;
    // CenterX -= (style->fonts_[fontType]->StringWidth(curr_menu->name) * 1.5) / 2;

    // now, draw all the menuitems
    deltay = 1 + font_h + style->definition_->entry_spacing_;

    curry = menutop + 25;

    if (curr_menu->key_page[0])
    {
        fontType = StyleDefinition::kTextSectionTitle;
        TEXTscale = style->definition_->text_[fontType].scale_;

        if (curr_key_menu > 0)
            HUDWriteText(style, fontType, 60, 200 - deltay * 4, "< PREV");

        if (curr_key_menu < NUM_KEY_MENUS - 1)
            HUDWriteText(style, fontType, 260 - style->fonts_[fontType]->StringWidth("NEXT >") * TEXTscale, 200 - deltay * 4,
                         "NEXT >");

        fontType = StyleDefinition::kTextSectionHelp;
        TEXTscale = style->definition_->text_[fontType].scale_;

        HUDWriteText(style, fontType, 160 - style->fonts_[fontType]->StringWidth(curr_menu->key_page) * TEXTscale / 2, curry,
                     curr_menu->key_page);
        curry += font_h * 2;

        if (keyscan)
            HUDWriteText(style, fontType, 160 - (style->fonts_[fontType]->StringWidth(keystring2) * TEXTscale / 2),
                         200 - deltay * 2, keystring2);
        else
            HUDWriteText(style, fontType, 160 - (style->fonts_[fontType]->StringWidth(keystring1) * TEXTscale / 2),
                         200 - deltay * 2, keystring1);
    }
    else if (curr_menu == &res_optmenu)
    {
        M_ResOptDrawer(style, curry, curry + (deltay * res_optmenu.item_num - 2), deltay, curr_menu->menu_center);
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

        if (is_selected)
        {
            fontType  = StyleDefinition::kTextSectionTitle;
            TEXTscale = style->definition_->text_[fontType].scale_;
        }
        else
        {
            fontType  = StyleDefinition::kTextSectionText;
            TEXTscale = style->definition_->text_[fontType].scale_;
        }

        HUDWriteText(style, fontType,
                     (curr_menu->menu_center) -
                         (style->fonts_[fontType]->StringWidth(
                              curr_menu->items[i].name) *
                          TEXTscale),
                     curry, curr_menu->items[i].name);

        // Draw current soundfont
        if (curr_menu == &sound_optmenu && curr_menu->items[i].routine == M_ChangeSoundfont)
        {
            fontType  = StyleDefinition::kTextSectionAlternate;
            TEXTscale = style->definition_->text_[fontType].scale_;
            HUDWriteText(style, fontType, (curr_menu->menu_center) + 15, curry,
                         epi::GetStem(midi_soundfont.s_).c_str());
        }

        // Draw current GENMIDI
        if (curr_menu == &sound_optmenu && curr_menu->items[i].routine == M_ChangeGENMIDI)
        {
            fontType  = StyleDefinition::kTextSectionAlternate;
            TEXTscale = style->definition_->text_[fontType].scale_;
            HUDWriteText(style, fontType, (curr_menu->menu_center) + 15, curry,
                         s_genmidi.s_.empty() ? "Default" : epi::GetStem(s_genmidi.s_).c_str());
        }

        // -ACB- 1998/07/15 Menu Cursor is colour indexed.
        if (is_selected)
        {
            fontType  = StyleDefinition::kTextSectionTitle;
            TEXTscale = style->definition_->text_[fontType].scale_;
            if (style->fonts_[fontType]->definition_->type_ == kFontTypeImage)
            {
                int cursor = 16;
                HUDWriteText(style, fontType, (curr_menu->menu_center + 4), curry, (const char *)&cursor);
            }
            else if (style->fonts_[fontType]->definition_->type_ == kFontTypeTrueType)
                HUDWriteText(style, fontType, (curr_menu->menu_center + 4), curry, "+");
            else
                HUDWriteText(style, fontType, (curr_menu->menu_center + 4), curry, "*");

            if (curr_menu->items[i].help)
            {
                fontType  = StyleDefinition::kTextSectionHelp;
                TEXTscale = style->definition_->text_[fontType].scale_;
                const char *help = language[curr_menu->items[i].help];

                HUDWriteText(style, fontType, 160 - (style->fonts_[fontType]->StringWidth(help) * TEXTscale / 2),
                             200 - deltay * 2, help);
            }
        }

        // I believe it's all T_ALT
        fontType  = StyleDefinition::kTextSectionAlternate;
        TEXTscale = style->definition_->text_[fontType].scale_;

        switch (curr_menu->items[i].type)
        {
        case OPT_Boolean:
        case OPT_Switch: {
            if (curr_menu == &analogue_optmenu && curr_menu->items[i].switchvar == &joystick_device)
            {
                if (joystick_device == 0)
                {
                    HUDWriteText(style, fontType, (curr_menu->menu_center) + 15, curry, "None");
                    break;
                }
                else
                {
                    const char *joyname = SDL_JoystickNameForIndex(joystick_device - 1);
                    if (joyname)
                    {
                        HUDWriteText(style, fontType, (curr_menu->menu_center) + 15, curry,
                                     epi::StringFormat("%d - %s", joystick_device, joyname).c_str());
                        break;
                    }
                    else
                    {
                        HUDWriteText(style, fontType, (curr_menu->menu_center) + 15, curry,
                                     epi::StringFormat("%d - Not Connected", joystick_device).c_str());
                        break;
                    }
                }
            }

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

            HUDWriteText(style, StyleDefinition::kTextSectionAlternate, (curr_menu->menu_center) + 15, curry, tempstring);
            break;
        }

        case OPT_Slider: {
            M_DrawThermo(curr_menu->menu_center + 15, curry, curr_menu->items[i].numtypes,
                         *(int *)(curr_menu->items[i].switchvar), 2);

            break;
        }

        case OPT_FracSlider: {
            M_DrawFracThermo(curr_menu->menu_center + 15, curry, *(float *)curr_menu->items[i].switchvar,
                             curr_menu->items[i].increment, 2, curr_menu->items[i].min, curr_menu->items[i].max,
                             curr_menu->items[i].fmt_string);

            break;
        }

        case OPT_KeyConfig: {
            k = *(int *)(curr_menu->items[i].switchvar);
            M_Key2String(k, tempstring);
            HUDWriteText(style, fontType, (curr_menu->menu_center + 15), curry, tempstring);
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
static void M_ResOptDrawer(Style *style, int topy, int bottomy, int dy, int centrex)
{
    char tempstring[80];

    // Draw current resolution
    int y = topy;

    // Draw resolution selection option
    y += (dy * 3);

    int fontType  = StyleDefinition::kTextSectionAlternate;
    float TEXTscale = style->definition_->text_[fontType].scale_;

    sprintf(tempstring, "%s",
            new_scrmode.display_mode == 2
                ? "Borderless Fullscreen"
                : (new_scrmode.display_mode == DisplayMode::SCR_FULLSCREEN ? "Exclusive Fullscreen" : "Windowed"));
    HUDWriteText(style, fontType, centrex + 15, y, tempstring);

    if (new_scrmode.display_mode < 2)
    {
        y += dy;
        sprintf(tempstring, "%dx%d", new_scrmode.width, new_scrmode.height);
        HUDWriteText(style, fontType, centrex + 15, y, tempstring);
    }

    // Draw selected resolution and mode:
    y = bottomy;

    fontType  = StyleDefinition::kTextSectionHelp;
    TEXTscale = style->definition_->text_[fontType].scale_;

    sprintf(tempstring, "Current Resolution:");
    HUDWriteText(style, fontType, 160 - (style->fonts_[fontType]->StringWidth(tempstring) * TEXTscale / 2), y, tempstring);

    fontType  = StyleDefinition::kTextSectionAlternate;
    TEXTscale = style->definition_->text_[fontType].scale_;

    y += dy;
    y += 5;
    if (DISPLAYMODE == 2)
        sprintf(tempstring, "%s", "Borderless Fullscreen");
    else
        sprintf(tempstring, "%d x %d %s", SCREENWIDTH, SCREENHEIGHT,
                DISPLAYMODE == 1 ? "Exclusive Fullscreen" : "Windowed");

    HUDWriteText(style, fontType, 160 - (style->fonts_[fontType]->StringWidth(tempstring) * TEXTscale / 2), y, tempstring);
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

    if(opt_def_style->definition_->text[StyleDefinition::kTextSectionAlternate].scale)
    {
        ALTscale=opt_def_style->definition_->text[StyleDefinition::kTextSectionAlternate].scale;
    }*/
    HUDWriteText(opt_def_style, StyleDefinition::kTextSectionAlternate, x + 15, y + deltay * LANGUAGE_POS, language.GetName());
}

static void KeyMenu_Next()
{
    if (curr_key_menu >= NUM_KEY_MENUS - 1)
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
bool M_OptResponder(InputEvent *ev, int ch)
{
    ///--  	if (testticker != -1)
    ///--  		return true;

    curr_item = curr_menu->items + curr_menu->pos; // Should help the accidental key binding to other options - Dasho

    // Scan for keycodes
    if (keyscan)
    {
        int *blah;

        if (ev->type != kInputEventKeyDown)
            return false;
        int key = ev->value.key.sym;

        keyscan = 0;

        // Eat the gamepad's "Start" button here to keep the user from
        // binding their menu opening key to an action
        if (ch == KEYD_ESCAPE || ch == KEYD_GP_START)
            return true;

        blah = (int *)(curr_item->switchvar);
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
    case KEYD_GP_BACK: {
        if (curr_item->type == OPT_KeyConfig)
            *(int *)(curr_item->switchvar) = 0;
        return true;
    }

    case KEYD_DOWNARROW:
    case KEYD_GP_DOWN: {
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
        } while (curr_item->type == 0);

        S_StartFX(sfx_pstop);
        return true;
    }

    case KEYD_WHEEL_DN: {
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
        } while (curr_item->type == 0);

        S_StartFX(sfx_pstop);
        return true;
    }

    case KEYD_UPARROW:
    case KEYD_GP_UP: {
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
        } while (curr_item->type == 0);

        S_StartFX(sfx_pstop);
        return true;
    }

    case KEYD_WHEEL_UP: {
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
        } while (curr_item->type == 0);

        S_StartFX(sfx_pstop);
        return true;
    }

    case KEYD_LEFTARROW:
    case KEYD_GP_LEFT: {
        if (curr_menu->key_page[0])
        {
            KeyMenu_Prev();
            return true;
        }

        switch (curr_item->type)
        {
        case OPT_Plain: {
            return false;
        }

        case OPT_Boolean: {
            bool *boolptr = (bool *)curr_item->switchvar;

            *boolptr = !(*boolptr);

            S_StartFX(sfx_pistol);

            if (curr_item->routine != nullptr)
                curr_item->routine(ch, curr_item->cvar_to_change);

            return true;
        }

        case OPT_Switch: {
            int *val_ptr = (int *)curr_item->switchvar;

            *val_ptr -= 1;

            if (*val_ptr < 0)
                *val_ptr = curr_item->numtypes - 1;

            S_StartFX(sfx_pistol);

            if (curr_item->routine != nullptr)
                curr_item->routine(ch, curr_item->cvar_to_change);

            return true;
        }

        case OPT_Function: {
            if (curr_item->routine != nullptr)
                curr_item->routine(ch, curr_item->cvar_to_change);

            S_StartFX(sfx_pistol);
            return true;
        }

        case OPT_Slider: {
            int *val_ptr = (int *)curr_item->switchvar;

            if (*val_ptr > 0)
            {
                *val_ptr -= 1;

                S_StartFX(sfx_stnmov);
            }

            if (curr_item->routine != nullptr)
                curr_item->routine(ch, curr_item->cvar_to_change);

            return true;
        }

        case OPT_FracSlider: {
            float *val_ptr = (float *)curr_item->switchvar;

            *val_ptr = *val_ptr - (remainderf(*val_ptr, curr_item->increment));

            if (*val_ptr > curr_item->min)
            {
                *val_ptr = *val_ptr - curr_item->increment;

                S_StartFX(sfx_stnmov);
            }

            if (curr_item->routine != nullptr)
                curr_item->routine(ch, curr_item->cvar_to_change);

            return true;
        }

        default:
            break;
        }
    }

    case KEYD_RIGHTARROW:
    case KEYD_GP_RIGHT:
        if (curr_menu->key_page[0])
        {
            KeyMenu_Next();
            return true;
        }

        /* FALL THROUGH... */

    case KEYD_ENTER:
    case KEYD_MOUSE1:
    case KEYD_GP_A: {
        switch (curr_item->type)
        {
        case OPT_Plain:
            return false;

        case OPT_Boolean: {
            bool *boolptr = (bool *)curr_item->switchvar;

            *boolptr = !(*boolptr);

            S_StartFX(sfx_pistol);

            if (curr_item->routine != nullptr)
                curr_item->routine(ch, curr_item->cvar_to_change);

            return true;
        }

        case OPT_Switch: {
            int *val_ptr = (int *)curr_item->switchvar;

            *val_ptr += 1;

            if (*val_ptr >= curr_item->numtypes)
                *val_ptr = 0;

            S_StartFX(sfx_pistol);

            if (curr_item->routine != nullptr)
                curr_item->routine(ch, curr_item->cvar_to_change);

            return true;
        }

        case OPT_Function: {
            if (curr_item->routine != nullptr)
                curr_item->routine(ch, curr_item->cvar_to_change);

            S_StartFX(sfx_pistol);
            return true;
        }

        case OPT_Slider: {
            int *val_ptr = (int *)curr_item->switchvar;

            if (*val_ptr < (curr_item->numtypes - 1))
            {
                *val_ptr += 1;

                S_StartFX(sfx_stnmov);
            }

            if (curr_item->routine != nullptr)
                curr_item->routine(ch, curr_item->cvar_to_change);

            return true;
        }

        case OPT_FracSlider: {
            float *val_ptr = (float *)curr_item->switchvar;

            *val_ptr = *val_ptr - (remainderf(*val_ptr, curr_item->increment));

            if (*val_ptr < curr_item->max)
            {
                *val_ptr = *val_ptr + curr_item->increment;

                S_StartFX(sfx_stnmov);
            }

            if (curr_item->routine != nullptr)
                curr_item->routine(ch, curr_item->cvar_to_change);

            return true;
        }

        case OPT_KeyConfig: {
            keyscan = 1;
            return true;
        }

        default:
            break;
        }
        EDGEError("Invalid menu type!");
    }
    case KEYD_ESCAPE:
    case KEYD_MOUSE2:
    case KEYD_MOUSE3:
    case KEYD_GP_B: {
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
static void M_VideoOptions(int keypressed, ConsoleVariable *cvar)
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
static void M_ResolutionOptions(int keypressed, ConsoleVariable *cvar)
{
    new_scrmode.width        = SCREENWIDTH;
    new_scrmode.height       = SCREENHEIGHT;
    new_scrmode.depth        = SCREENBITS;
    new_scrmode.display_mode = DISPLAYMODE;

    curr_menu = &res_optmenu;
    curr_item = curr_menu->items + curr_menu->pos;
}

//
// M_AnalogueOptions
//
static void M_AnalogueOptions(int keypressed, ConsoleVariable *cvar)
{
    curr_menu = &analogue_optmenu;
    curr_item = curr_menu->items + curr_menu->pos;
}

//
// M_SoundOptions
//
static void M_SoundOptions(int keypressed, ConsoleVariable *cvar)
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
    curr_menu     = &f4sound_optmenu;
    curr_item     = curr_menu->items + curr_menu->pos;
}

//
// M_GameplayOptions
//
static void M_GameplayOptions(int keypressed, ConsoleVariable *cvar)
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
static void M_PerformanceOptions(int keypressed, ConsoleVariable *cvar)
{
    // not allowed in netgames (changing most of these options would
    // break synchronisation with the other machines).
    if (netgame)
        return;

    curr_menu = &perf_optmenu;
    curr_item = curr_menu->items + curr_menu->pos;
}

//
// M_AccessibilityOptions
//
static void M_AccessibilityOptions(int keypressed, ConsoleVariable *cvar)
{
    // not allowed in netgames (changing most of these options would
    // break synchronisation with the other machines).
    if (netgame)
        return;

    curr_menu = &accessibility_optmenu;
    curr_item = curr_menu->items + curr_menu->pos;
}

//
// M_KeyboardOptions
//
static void M_KeyboardOptions(int keypressed, ConsoleVariable *cvar)
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

    strcpy(deststring, EventGetKeyName(key1));

    if (key2 != 0)
    {
        strcat(deststring, " or ");
        strcat(deststring, EventGetKeyName(key2));
    }
}

static void InitMonitorSize()
{
    if (monitor_aspect_ratio.f_ > 2.00)
        monitor_size = 5;
    else if (monitor_aspect_ratio.f_ > 1.70)
        monitor_size = 4;
    else if (monitor_aspect_ratio.f_ > 1.55)
        monitor_size = 3;
    else if (monitor_aspect_ratio.f_ > 1.40)
        monitor_size = 2;
    else if (monitor_aspect_ratio.f_ > 1.30)
        monitor_size = 1;
    else
        monitor_size = 0;
}

static void M_ChangeMonitorSize(int key, ConsoleVariable *cvar)
{
    static const float ratios[6] = {
        1.25000, 1.33333, 1.50000, // 5:4     4:3   3:2
        1.60000, 1.77777, 2.33333  // 16:10  16:9  21:9
    };

    monitor_size = HMM_Clamp(0, monitor_size, 5);

    monitor_aspect_ratio = ratios[monitor_size];
}

//
// M_ChangeBlood
//
// -KM- 1998/07/21 Change blood to a bool
// -ACB- 1998/08/09 Check map setting allows this
//
/*static void M_ChangeBlood(int keypressed, cvar_c *cvar)
{
    if (current_map && ((current_map->force_on_ | current_map->force_off_) & kMapFlagMoreBlood))
        return;

    level_flags.more_blood = global_flags.more_blood;
}*/

static void M_ChangeMLook(int keypressed, ConsoleVariable *cvar)
{
    if (current_map && ((current_map->force_on_ | current_map->force_off_) & kMapFlagMlook))
        return;

    level_flags.mlook = global_flags.mlook;
}

static void M_ChangeJumping(int keypressed, ConsoleVariable *cvar)
{
    if (current_map && ((current_map->force_on_ | current_map->force_off_) & kMapFlagJumping))
        return;

    level_flags.jump = global_flags.jump;
}

static void M_ChangeCrouching(int keypressed, ConsoleVariable *cvar)
{
    if (current_map && ((current_map->force_on_ | current_map->force_off_) & kMapFlagCrouching))
        return;

    level_flags.crouch = global_flags.crouch;
}

static void M_ChangeExtra(int keypressed, ConsoleVariable *cvar)
{
    if (current_map && ((current_map->force_on_ | current_map->force_off_) & kMapFlagExtras))
        return;

    level_flags.have_extra = global_flags.have_extra;
}

//
// M_ChangeMonsterRespawn
//
// -ACB- 1998/08/09 New DDF settings, check that map allows the settings
//
static void M_ChangeMonsterRespawn(int keypressed, ConsoleVariable *cvar)
{
    if (current_map && ((current_map->force_on_ | current_map->force_off_) & kMapFlagResRespawn))
        return;

    level_flags.res_respawn = global_flags.res_respawn;
}

static void M_ChangeItemRespawn(int keypressed, ConsoleVariable *cvar)
{
    if (current_map && ((current_map->force_on_ | current_map->force_off_) & kMapFlagItemRespawn))
        return;

    level_flags.itemrespawn = global_flags.itemrespawn;
}

static void M_ChangeTrue3d(int keypressed, ConsoleVariable *cvar)
{
    if (current_map && ((current_map->force_on_ | current_map->force_off_) & kMapFlagTrue3D))
        return;

    level_flags.true3dgameplay = global_flags.true3dgameplay;
}

static void M_ChangeAutoAim(int keypressed, ConsoleVariable *cvar)
{
    if (current_map && ((current_map->force_on_ | current_map->force_off_) & kMapFlagAutoAim))
        return;

    level_flags.autoaim = global_flags.autoaim;
}

static void M_ChangeRespawn(int keypressed, ConsoleVariable *cvar)
{
    if (game_skill == sk_nightmare)
        return;

    if (current_map && ((current_map->force_on_ | current_map->force_off_) & kMapFlagRespawn))
        return;

    level_flags.respawn = global_flags.respawn;
}

static void M_ChangeFastparm(int keypressed, ConsoleVariable *cvar)
{
    if (game_skill == sk_nightmare)
        return;

    if (current_map && ((current_map->force_on_ | current_map->force_off_) & kMapFlagFastParm))
        return;

    level_flags.fastparm = global_flags.fastparm;
}

static void M_ChangePassMissile(int keypressed, ConsoleVariable *cvar)
{
    level_flags.pass_missile = global_flags.pass_missile;
}

// this used by both MIPMIP, SMOOTHING and DETAIL options
static void M_ChangeMipMap(int keypressed, ConsoleVariable *cvar)
{
    W_DeleteAllImages();
}

static void M_ChangeBobbing(int keypressed, ConsoleVariable *cvar)
{
    g_bobbing        = g_bobbing.d_;
    player_t *player = players[consoleplayer];
    if (player)
    {
        player->bob   = 0;
        pspdef_t *psp = &player->psprites[player->action_psp];
        if (psp)
        {
            psp->sx = 0;
            psp->sy = 0;
        }
    }
}

static void M_UpdateCVARFromFloat(int keypressed, ConsoleVariable *cvar)
{
    SYS_ASSERT(cvar);
    cvar->operator=(cvar->f_);
}

static void M_UpdateCVARFromInt(int keypressed, ConsoleVariable *cvar)
{
    SYS_ASSERT(cvar);
    cvar->operator=(cvar->d_);
}

static void M_ChangeKicking(int keypressed, ConsoleVariable *cvar)
{
    if (current_map && ((current_map->force_on_ | current_map->force_off_) & kMapFlagKicking))
        return;

    level_flags.kicking = global_flags.kicking;
}

static void M_ChangeWeaponSwitch(int keypressed, ConsoleVariable *cvar)
{
    if (current_map && ((current_map->force_on_ | current_map->force_off_) & kMapFlagWeaponSwitch))
        return;

    level_flags.weapon_switch = global_flags.weapon_switch;
}

static void M_ChangePCSpeakerMode(int keypressed, ConsoleVariable *cvar)
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
static void M_ChangeLanguage(int keypressed, ConsoleVariable *cvar)
{
    if (keypressed == KEYD_LEFTARROW || keypressed == KEYD_GP_LEFT)
    {
        int idx, max;

        idx = language.GetChoice();
        max = language.GetChoiceCount();

        idx--;
        if (idx < 0)
        {
            idx += max;
        }

        language.Select(idx);
    }
    else if (keypressed == KEYD_RIGHTARROW || keypressed == KEYD_GP_RIGHT)
    {
        int idx, max;

        idx = language.GetChoice();
        max = language.GetChoiceCount();

        idx++;
        if (idx >= max)
        {
            idx = 0;
        }

        language.Select(idx);
    }

    // update cvar
    m_language = language.GetName();
}

//
// M_ChangeMIDIPlayer
//
//
static void M_ChangeMIDIPlayer(int keypressed, ConsoleVariable *cvar)
{
    PlaylistEntry *playing = playlist.Find(entry_playing);
    if (var_midi_player == 1 ||
        (playing && (playing->type_ == kDDFMusicIMF280 || playing->type_ == kDDFMusicIMF560 || playing->type_ == kDDFMusicIMF700)))
        S_RestartOPL();
    else
        S_RestartFluid();
}

//
// M_ChangeSoundfont
//
//
static void M_ChangeSoundfont(int keypressed, ConsoleVariable *cvar)
{
    int sf_pos = -1;
    for (int i = 0; i < (int)available_soundfonts.size(); i++)
    {
        if (epi::StringCaseCompareASCII(midi_soundfont.s_, available_soundfonts.at(i)) == 0)
        {
            sf_pos = i;
            break;
        }
    }

    if (sf_pos < 0)
    {
        EDGEWarning("M_ChangeSoundfont: Could not read list of available soundfonts. Falling back to default!\n");
        midi_soundfont = epi::SanitizePath(epi::PathAppend(game_directory, "soundfont/Default.sf2"));
        return;
    }

    if (keypressed == KEYD_LEFTARROW || keypressed == KEYD_GP_LEFT)
    {
        if (sf_pos - 1 >= 0)
            sf_pos--;
        else
            sf_pos = available_soundfonts.size() - 1;
    }
    else if (keypressed == KEYD_RIGHTARROW || keypressed == KEYD_GP_RIGHT)
    {
        if (sf_pos + 1 >= (int)available_soundfonts.size())
            sf_pos = 0;
        else
            sf_pos++;
    }

    // update cvar
    midi_soundfont = available_soundfonts.at(sf_pos);
    S_RestartFluid();
}

//
// M_ChangeGENMIDI
//
//
static void M_ChangeGENMIDI(int keypressed, ConsoleVariable *cvar)
{
    int op2_pos = -1;
    for (int i = 0; i < (int)available_opl_banks.size(); i++)
    {
        if (epi::StringCaseCompareASCII(s_genmidi.s_, available_opl_banks.at(i)) == 0)
        {
            op2_pos = i;
            break;
        }
    }

    if (op2_pos < 0)
    {
        EDGEWarning("M_ChangeGENMIDI: Could not read list of available GENMIDIs. Falling back to default!\n");
        s_genmidi.s_ = "";
        return;
    }

    if (keypressed == KEYD_LEFTARROW || keypressed == KEYD_GP_LEFT)
    {
        if (op2_pos - 1 >= 0)
            op2_pos--;
        else
            op2_pos = available_opl_banks.size() - 1;
    }
    else if (keypressed == KEYD_RIGHTARROW || keypressed == KEYD_GP_RIGHT)
    {
        if (op2_pos + 1 >= (int)available_opl_banks.size())
            op2_pos = 0;
        else
            op2_pos++;
    }

    // update cvar
    s_genmidi = available_opl_banks.at(op2_pos);
    S_RestartOPL();
}

//
// M_ChangeResSize
//
// -ACB- 1998/08/29 Resolution Changes...
//
static void M_ChangeResSize(int keypressed, ConsoleVariable *cvar)
{
    if (keypressed == KEYD_LEFTARROW || keypressed == KEYD_GP_LEFT)
    {
        R_IncrementResolution(&new_scrmode, RESINC_Size, -1);
    }
    else if (keypressed == KEYD_RIGHTARROW || keypressed == KEYD_GP_RIGHT)
    {
        R_IncrementResolution(&new_scrmode, RESINC_Size, +1);
    }
}

//
// M_ChangeResFull
//
// -AJA- 2005/01/02: Windowed vs Fullscreen
//
static void M_ChangeResFull(int keypressed, ConsoleVariable *cvar)
{
    if (keypressed == KEYD_LEFTARROW || keypressed == KEYD_GP_LEFT)
    {
        R_IncrementResolution(&new_scrmode, RESINC_DisplayMode, -1);
    }
    else if (keypressed == KEYD_RIGHTARROW || keypressed == KEYD_GP_RIGHT)
    {
        R_IncrementResolution(&new_scrmode, RESINC_DisplayMode, +1);
    }
}

//
// M_OptionSetResolution
//
static void M_OptionSetResolution(int keypressed, ConsoleVariable *cvar)
{
    if (R_ChangeResolution(&new_scrmode))
    {
        if (new_scrmode.display_mode > DisplayMode::SCR_WINDOW)
        {
            tf_screendepth  = new_scrmode.depth;
            tf_screenheight = new_scrmode.height;
            tf_screenwidth  = new_scrmode.width;
            tf_displaymode  = new_scrmode.display_mode;
        }
        else
        {
            tw_screendepth  = new_scrmode.depth;
            tw_screenheight = new_scrmode.height;
            tw_screenwidth  = new_scrmode.width;
            tw_displaymode  = new_scrmode.display_mode;
        }
        R_SoftInitResolution();
    }
    else
    {
        std::string msg(epi::StringFormat(language["ModeSelErr"], new_scrmode.width, new_scrmode.height,
                                        (new_scrmode.depth < 20) ? 16 : 32));

        M_StartMessage(msg.c_str(), nullptr, false);

        ///--  		testticker = -1;

        ///??	selectedscrmode = prevscrmode;
    }
}

///--  //
///--  // M_OptionTestResolution
///--  //
///--  static void M_OptionTestResolution(int keypressed, cvar_c *cvar)
///--  {
///--      R_ChangeResolution(selectedscrmode);
///--  	testticker = TICRATE * 3;
///--  }
///--
///--  //
///--  // M_RestoreResSettings
///--  //
///--  static void M_RestoreResSettings(int keypressed, cvar_c *cvar)
///--  {
///--      R_ChangeResolution(prevscrmode);
///--  }

extern void M_NetHostBegun(void);
extern void M_NetJoinBegun(void);

void M_HostNetGame(int keypressed, ConsoleVariable *cvar)
{
    option_menuon  = 0;
    netgame_menuon = 1;

    M_NetHostBegun();
}

void M_JoinNetGame(int keypressed, ConsoleVariable *cvar)
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
    fkey_menu     = (choice == 1);
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
