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

#include "m_option.h"

#include <math.h>

#include <set>

#include "am_map.h"
#include "ddf_font.h"
#include "ddf_main.h"
#include "ddf_playlist.h"
#include "defaults.h"
#include "dm_state.h"
#include "e_input.h"
#include "epi.h"
#include "epi_filesystem.h"
#include "epi_sdl.h"
#include "epi_str_compare.h"
#include "epi_str_util.h"
#include "g_game.h"
#include "hu_draw.h"
#include "hu_stuff.h"
#include "hu_style.h"
#include "i_system.h"
#include "m_menu.h"
#include "m_misc.h"
#include "m_netgame.h"
#include "n_network.h"
#include "p_local.h"
#include "r_colormap.h"
#include "r_draw.h"
#include "r_gldefs.h"
#include "r_image.h"
#include "r_misc.h"
#include "r_modes.h"
#include "r_wipe.h"
#include "s_blit.h"
#include "s_cache.h"
#include "s_midi.h"
#include "s_music.h"
#include "s_sound.h"
#include "stb_sprintf.h"
#include "w_wad.h"

int  option_menu_on    = 0;
bool function_key_menu = false;

extern ConsoleVariable m_language;
extern ConsoleVariable crosshair_image;
extern ConsoleVariable crosshair_color;
extern ConsoleVariable crosshair_size;
extern ConsoleVariable opl_instrument_bank;
extern ConsoleVariable midi_soundfont;
extern ConsoleVariable video_overlay;
extern ConsoleVariable erraticism;
extern ConsoleVariable draw_culling;
extern ConsoleVariable draw_culling_distance;
extern ConsoleVariable cull_fog_color;
extern ConsoleVariable distance_cull_thinkers;
extern ConsoleVariable max_dynamic_lights;
extern ConsoleVariable vsync;
extern ConsoleVariable view_bobbing;
extern ConsoleVariable gore_level;
extern ConsoleVariable skip_intros;
extern ConsoleVariable maximum_pickup_messages;
extern ConsoleVariable show_endoom;
extern ConsoleVariable confirm_quickload;
extern ConsoleVariable confirm_quicksave;

// extern console_variable_c automap_keydoor_text;

extern ConsoleVariable sector_brightness_correction;
extern ConsoleVariable gamma_correction;
extern ConsoleVariable title_scaling;
extern ConsoleVariable sky_stretch_mode;
extern ConsoleVariable force_flat_lighting;

static int monitor_size;

extern int             joystick_device;
extern ConsoleVariable joystick_deadzone_axis_0;
extern ConsoleVariable joystick_deadzone_axis_1;
extern ConsoleVariable joystick_deadzone_axis_2;
extern ConsoleVariable joystick_deadzone_axis_3;
extern ConsoleVariable joystick_deadzone_axis_4;
extern ConsoleVariable joystick_deadzone_axis_5;

extern SDL_Joystick *joystick_info;
extern int           JoystickGetAxis(int n);
extern void          OptionMenuNetworkHostBegun(void);

extern int entry_playing;

// submenus
static void OptionMenuKeyboardOptions(int key_pressed, ConsoleVariable *console_variable);
static void OptionMenuVideoOptions(int key_pressed, ConsoleVariable *console_variable);
static void OptionMenuUIOptions(int key_pressed, ConsoleVariable *console_variable);
static void OptionMenuGameplayOptions(int key_pressed, ConsoleVariable *console_variable);
static void OptionMenuPerformanceOptions(int key_pressed, ConsoleVariable *console_variable);
static void OptionMenuAccessibilityOptions(int key_pressed, ConsoleVariable *console_variable);
static void OptionMenuAnalogueOptions(int key_pressed, ConsoleVariable *console_variable);
static void OptionMenuSoundOptions(int key_pressed, ConsoleVariable *console_variable);

static void OptionMenuKeyToString(int key, char *deststring);

// Use these when a menu option does nothing special other than update a value
static void OptionMenuUpdateConsoleVariableFromFloat(int key_pressed, ConsoleVariable *console_variable);
static void OptionMenuUpdateConsoleVariableFromInt(int key_pressed, ConsoleVariable *console_variable);

// -ACB- 1998/08/09 "Does Map allow these changes?" procedures.
static void OptionMenuChangeMonsterRespawn(int key_pressed, ConsoleVariable *console_variable);
static void OptionMenuChangeItemRespawn(int key_pressed, ConsoleVariable *console_variable);
static void OptionMenuChangeTrue3d(int key_pressed, ConsoleVariable *console_variable);
static void OptionMenuChangeAutoAim(int key_pressed, ConsoleVariable *console_variable);
static void OptionMenuChangeFastparm(int key_pressed, ConsoleVariable *console_variable);
static void OptionMenuChangeRespawn(int key_pressed, ConsoleVariable *console_variable);
static void OptionMenuChangePassMissile(int key_pressed, ConsoleVariable *console_variable);
static void OptionMenuChangeBobbing(int key_pressed, ConsoleVariable *console_variable);
static void OptionMenuChangeMLook(int key_pressed, ConsoleVariable *console_variable);
static void OptionMenuChangeJumping(int key_pressed, ConsoleVariable *console_variable);
static void OptionMenuChangeCrouching(int key_pressed, ConsoleVariable *console_variable);
static void OptionMenuChangeExtra(int key_pressed, ConsoleVariable *console_variable);
static void OptionMenuChangeMonitorSize(int key_pressed, ConsoleVariable *console_variable);
static void OptionMenuChangeKicking(int key_pressed, ConsoleVariable *console_variable);
static void OptionMenuChangeWeaponSwitch(int key_pressed, ConsoleVariable *console_variable);
static void OptionMenuChangeMipMap(int key_pressed, ConsoleVariable *console_variable);
#ifdef EDGE_CLASSIC
static void OptionMenuChangePCSpeakerMode(int key_pressed, ConsoleVariable *console_variable);
#endif

// -ES- 1998/08/20 Added resolution options
// -ACB- 1998/08/29 Moved to top and tried different system

static void OptionMenuResOptDrawer(Style *style, int topy, int bottomy, int dy, int centrex);
static void OptionMenuResolutionOptions(int key_pressed, ConsoleVariable *console_variable);
static void OptionMenuSetResolution(int key_pressed, ConsoleVariable *console_variable);
static void OptionMenuChangeResSize(int key_pressed, ConsoleVariable *console_variable);
static void OptionMenuChangeResFull(int key_pressed, ConsoleVariable *console_variable);

void OptionMenuHostNetGame(int key_pressed, ConsoleVariable *console_variable);
#ifndef EDGE_WEB
static void OptionMenuBrowseHome(int key_pressed, ConsoleVariable *console_variable);
#endif

static void OptionMenuLanguageDrawer(int x, int y, int deltay);
static void OptionMenuChangeLanguage(int key_pressed, ConsoleVariable *console_variable);
static void OptionMenuChangeSoundfont(int key_pressed, ConsoleVariable *console_variable);
static void OptionMenuChangeOverlay(int key_pressed, ConsoleVariable *console_variable);
static void OptionMenuChangeCrosshair(int key_pressed, ConsoleVariable *console_variable);
static void OptionMenuChangeGamepad(int key_pressed, ConsoleVariable *console_variable);
static void InitMonitorSize();

static constexpr char YesNo[]        = "Off/On"; // basic on/off
static constexpr char MouseAxis[]    = "Off/Turn/Turn (Reversed)/Look/Look (Inverted)/Walk/Walk "
                                       "(Reversed)/Strafe/Strafe (Reversed)/Fly/Fly (Inverted)";
static constexpr char JoystickAxis[] = "Off/Turn/Turn (Reversed)/Look (Inverted)/Look/Walk "
                                       "(Reversed)/Walk/Strafe/Strafe "
                                       "(Reversed)/Fly (Inverted)/Fly/Left Trigger/Right Trigger";

// Screen resolution changes
static DisplayMode new_window_mode;

extern std::set<std::string>               available_soundfonts;
extern std::map<std::string, unsigned int> available_crosshairs;

//
//  OPTION STRUCTURES
//

enum OptionMenuItemType
{
    kOptionMenuItemTypePlain     = 0, // 0 means plain text,
    kOptionMenuItemTypeSwitch    = 1, // 1 is change a switch,
    kOptionMenuItemTypeFunction  = 2, // 2 is call a function,
    kOptionMenuItemTypeSlider    = 3, // 3 is a slider,
    kOptionMenuItemTypeKeyConfig = 4, // 4 is a key config,
    kOptionMenuItemTypeBoolean   = 5, // 5 is change a boolean switch
    kTotalOptionMenuItemTypes
};

struct OptionMenuItem
{
    OptionMenuItemType type;

    const char *name;
    const char *type_names;

    int   total_types;
    void *switch_variable;

    void (*routine)(int key_pressed, ConsoleVariable *console_variable);

    const char *help;

    ConsoleVariable *console_variable_to_change;

    // For options tracking a floating point variable/range
    float increment;
    float min;
    float max;

    // Formatting string (only used for fracsliders atm)
    std::string format_string = "";
};

struct OptionMenuDefinition
{
    // array of menu items
    OptionMenuItem *items;
    int             item_number;

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
};

// current menu and position
static OptionMenuDefinition *current_menu;
static OptionMenuItem       *current_item;
static int                   current_key_menu;

static int keyscan;

static Style *options_menu_default_style;

static int OptionMenuGetCurrentSwitchValue(OptionMenuItem *item)
{
    int retval = 0;

    switch (item->type)
    {
    case kOptionMenuItemTypeBoolean: {
        retval = *(bool *)item->switch_variable ? 1 : 0;
        break;
    }

    case kOptionMenuItemTypeSwitch: {
        retval = *(int *)(item->switch_variable);
        break;
    }

    default: {
        FatalError("OptionMenuGetCurrentSwitchValue: Menu item type is not a "
                   "switch!\n");
    }
    }

    return retval;
}

//
//  MAIN MENU
//
#ifdef EDGE_WEB
static constexpr uint8_t kOptionMenuLanguagePosition    = 10;
static constexpr uint8_t kOptionMenuNetworkHostPosition = 12;
#else
static constexpr uint8_t kOptionMenuLanguagePosition    = 11;
static constexpr uint8_t kOptionMenuNetworkHostPosition = 13;
#endif

static OptionMenuItem mainoptions[] = {
    {kOptionMenuItemTypeFunction, "MenuBinding", nullptr, 0, nullptr, OptionMenuKeyboardOptions, "Controls", nullptr, 0,
     0, 0, ""},
    {kOptionMenuItemTypeFunction, "MenuMouse", nullptr, 0, nullptr, OptionMenuAnalogueOptions, "AnalogueOptions",
     nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeFunction, "MenuGameplay", nullptr, 0, nullptr, OptionMenuGameplayOptions, "GameplayOptions",
     nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeFunction, "MenuPerformance", nullptr, 0, nullptr, OptionMenuPerformanceOptions,
     "PerformanceOptions", nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeFunction, "MenuAccessibility", nullptr, 0, nullptr, OptionMenuAccessibilityOptions,
     "AccessibilityOptions", nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeFunction, "MenuUI", nullptr, 0, nullptr, OptionMenuUIOptions, "UIOptions", nullptr, 0, 0, 0,
     ""},
    {kOptionMenuItemTypePlain, "", nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeFunction, "MenuSound", nullptr, 0, nullptr, OptionMenuSoundOptions, "SoundOptions", nullptr, 0,
     0, 0, ""},
    {kOptionMenuItemTypeFunction, "MenuVideo", nullptr, 0, nullptr, OptionMenuVideoOptions, "VideoOptions", nullptr, 0,
     0, 0, ""},
#ifndef EDGE_WEB
    {kOptionMenuItemTypeFunction, "MenuResolution", nullptr, 0, nullptr, OptionMenuResolutionOptions, "ChangeRes",
     nullptr, 0, 0, 0, ""},
#endif
    {kOptionMenuItemTypePlain, "", nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeFunction, "MenuLanguage", nullptr, 0, nullptr, OptionMenuChangeLanguage, nullptr, nullptr, 0, 0,
     0, ""},
    {kOptionMenuItemTypePlain, "", nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeFunction, "MenuStartBotmatch", nullptr, 0, nullptr, OptionMenuHostNetGame, nullptr, nullptr, 0,
     0, 0, ""},
#ifndef EDGE_WEB
    {kOptionMenuItemTypeFunction, "MenuBrowseHome", nullptr, 0, nullptr, OptionMenuBrowseHome, nullptr, nullptr, 0, 0,
     0, ""},
#else
    {kOptionMenuItemTypePlain, "", nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0, 0, ""},
#endif
    {kOptionMenuItemTypeFunction, "MenuResetToDefault", nullptr, 0, nullptr, ResetDefaults, nullptr, nullptr, 0, 0, 0,
     ""}};

static OptionMenuDefinition main_optmenu = {mainoptions,
                                            sizeof(mainoptions) / sizeof(OptionMenuItem),
                                            &options_menu_default_style,
                                            164,
                                            108,
                                            0,
                                            "",
                                            language["MenuOptions"]};

//
//  VIDEO OPTIONS
//
// -ACB- 1998/07/15 Altered menu structure

static OptionMenuItem vidoptions[] = {
    {kOptionMenuItemTypeSlider, "Gamma Adjustment", nullptr, 0, &gamma_correction.f_,
     OptionMenuUpdateConsoleVariableFromFloat, nullptr, &gamma_correction, 0.10f, -1.0f, 1.0f, "%0.2f"},
    {kOptionMenuItemTypeSwitch, "Sector Brightness", "-50/-40/-30/-20/-10/Default/+10/+20/+30/+40/+50", 11,
     &sector_brightness_correction.d_, OptionMenuUpdateConsoleVariableFromInt, nullptr, &sector_brightness_correction,
     0, 0, 0, ""},
    {kOptionMenuItemTypeBoolean, "Lighting Mode", "Indexed/Flat", 2, &force_flat_lighting.d_,
     OptionMenuUpdateConsoleVariableFromInt, nullptr, &force_flat_lighting, 0, 0, 0, ""},
    {kOptionMenuItemTypeSwitch, "Mipmapping", "Off/Bilinear/Trilinear", 3, &image_mipmapping, OptionMenuChangeMipMap,
     nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeSwitch, "Smoothing", YesNo, 2, &image_smoothing, OptionMenuChangeMipMap, nullptr, nullptr, 0, 0,
     0, ""},
    {kOptionMenuItemTypeSwitch, "Upscale Textures", "Off/UI Only/UI & Sprites/All", 4, &hq2x_scaling,
     OptionMenuChangeMipMap, "Only affects paletted (Doom format) textures", nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeSwitch, "Title/Intermission Scaling", "Normal/Border Fill", 2, &title_scaling.d_,
     OptionMenuUpdateConsoleVariableFromInt, nullptr, &title_scaling, 0, 0, 0, ""},
    {kOptionMenuItemTypeSwitch, "Sky Scaling", "Mirror/Repeat/Stretch/Vanilla", 4, &sky_stretch_mode.d_,
     OptionMenuUpdateConsoleVariableFromInt, "Vanilla will be forced when Mouselook is Off", &sky_stretch_mode, 0, 0, 0,
     ""},
    {kOptionMenuItemTypeSwitch, "Dynamic Lighting", YesNo, 2, &use_dynamic_lights, nullptr, nullptr, nullptr, 0, 0, 0,
     ""},
    {kOptionMenuItemTypeFunction, "Overlay", nullptr, 0, nullptr, OptionMenuChangeOverlay, nullptr, nullptr, 0, 0, 0,
     ""},
    {kOptionMenuItemTypeSwitch, "Invulnerability", "Simple/Textured", kTotalInvulnerabilityEffects,
     &invulnerability_effect, nullptr, nullptr, nullptr, 0, 0, 0, ""},
#ifndef EDGE_WEB
    {kOptionMenuItemTypeSwitch, "Wipe method", "None/Melt/Crossfade/Pixelfade/Top/Bottom/Left/Right/Spooky/Doors",
     kTotalScreenWipeTypes, &wipe_method, nullptr, nullptr, nullptr, 0, 0, 0, ""},
#endif
    {kOptionMenuItemTypeSwitch, "Animated Liquid Type", "Vanilla/SMMU/SMMU+Swirl/Parallax", 4, &swirling_flats, nullptr,
     nullptr, nullptr, 0, 0, 0, ""}};

static OptionMenuDefinition video_optmenu = {
    vidoptions,           sizeof(vidoptions) / sizeof(OptionMenuItem), &options_menu_default_style, 150, 77, 0, "",
    language["MenuVideo"]};

//
//  UI OPTIONS
//
// -ACB- 1998/07/15 Altered menu structure

static OptionMenuItem uioptions[] = {
    {kOptionMenuItemTypeBoolean, language["ENDOOMOnQuit"], YesNo, 2, &show_endoom.d_,
     OptionMenuUpdateConsoleVariableFromInt, nullptr, &show_endoom, 0, 0, 0, ""},
    {kOptionMenuItemTypeBoolean, "Confirm Quickloads", YesNo, 2, &confirm_quickload.d_,
     OptionMenuUpdateConsoleVariableFromInt, nullptr, &confirm_quickload, 0, 0, 0, ""},
    {kOptionMenuItemTypeBoolean, "Confirm Quicksaves", YesNo, 2, &confirm_quicksave.d_,
     OptionMenuUpdateConsoleVariableFromInt, nullptr, &confirm_quicksave, 0, 0, 0, ""},
    {kOptionMenuItemTypeBoolean, "Map Rotation", YesNo, 2, &rotate_map, nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeBoolean, "Obituary Messages", YesNo, 2, &show_obituaries, nullptr, nullptr, nullptr, 0, 0, 0,
     ""},
    {kOptionMenuItemTypeBoolean, "Skip Startup Movies", YesNo, 2, &skip_intros.d_,
     OptionMenuUpdateConsoleVariableFromInt, nullptr, &skip_intros, 0, 0, 0, ""},
    {kOptionMenuItemTypeSwitch, "Max Pickup Messages", "1/2/3/4", 4, &maximum_pickup_messages.d_,
     OptionMenuUpdateConsoleVariableFromInt, nullptr, &maximum_pickup_messages, 0, 0, 0, ""},
    {kOptionMenuItemTypeFunction, "Crosshair Image", nullptr, 0, nullptr, OptionMenuChangeCrosshair, nullptr, nullptr,
     0, 0, 0, ""},
    {kOptionMenuItemTypeSwitch, "Crosshair Color", "White/Blue/Green/Cyan/Red/Pink/Yellow/Orange", 8,
     &crosshair_color.d_, OptionMenuUpdateConsoleVariableFromInt, nullptr, &crosshair_color, 0, 0, 0, ""},
    {kOptionMenuItemTypeSlider, "Crosshair Size", nullptr, 0, &crosshair_size.f_,
     OptionMenuUpdateConsoleVariableFromFloat, nullptr, &crosshair_size, 1.0f, 2.0f, 64.0f, "%g Pixels"},
};

static OptionMenuDefinition ui_optmenu = {
    uioptions,         sizeof(uioptions) / sizeof(OptionMenuItem), &options_menu_default_style, 150, 77, 0, "",
    language["MenuUI"]};

//
//  SCREEN OPTIONS MENU
//
static OptionMenuItem resoptions[] = {
    {kOptionMenuItemTypePlain, "", nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeSwitch, "V-Sync", "Off/Standard/Adaptive", 3, &vsync.d_, OptionMenuUpdateConsoleVariableFromInt,
     "Will fallback to Standard if Adaptive is not supported", &vsync, 0, 0, 0, ""},
    {kOptionMenuItemTypeSwitch, "Aspect Ratio", "5:4/4:3/3:2/16:10/16:9/21:9", 6, &monitor_size,
     OptionMenuChangeMonitorSize, "Only applies to Fullscreen Modes", nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeFunction, "New Mode", nullptr, 0, nullptr, OptionMenuChangeResFull, nullptr, nullptr, 0, 0, 0,
     ""},
    {kOptionMenuItemTypeFunction, "New Resolution", nullptr, 0, nullptr, OptionMenuChangeResSize, nullptr, nullptr, 0,
     0, 0, ""},
    {kOptionMenuItemTypeFunction, "Apply Mode/Resolution", nullptr, 0, nullptr, OptionMenuSetResolution, nullptr,
     nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypePlain, "", nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypePlain, "", nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypePlain, "", nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0, 0, ""}};

static OptionMenuDefinition res_optmenu = {resoptions,
                                           sizeof(resoptions) / sizeof(OptionMenuItem),
                                           &options_menu_default_style,
                                           150,
                                           77,
                                           3,
                                           "",
                                           language["MenuResolution"]};

//
//  MOUSE OPTIONS
//
// -ACB- 1998/06/15 Added new mouse menu
// -KM- 1998/09/01 Changed to an analogue menu.  Must change those names
// -ACB- 1998/07/15 Altered menu structure
//

static OptionMenuItem analogueoptions[] = {
    {kOptionMenuItemTypeSwitch, "Mouse X Axis", MouseAxis, 11, &mouse_x_axis, nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeSwitch, "Mouse Y Axis", MouseAxis, 11, &mouse_y_axis, nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeSlider, "X Sensitivity", nullptr, 0, &mouse_x_sensitivity.f_,
     OptionMenuUpdateConsoleVariableFromFloat, nullptr, &mouse_x_sensitivity, 0.25f, 1.0f, 15.0f, "%0.2f"},
    {kOptionMenuItemTypeSlider, "Y Sensitivity", nullptr, 0, &mouse_y_sensitivity.f_,
     OptionMenuUpdateConsoleVariableFromFloat, nullptr, &mouse_y_sensitivity, 0.25f, 1.0f, 15.0f, "%0.2f"},
    {kOptionMenuItemTypePlain, "", nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeSwitch, "Gamepad", "None/1/2/3/4", 5, &joystick_device, OptionMenuChangeGamepad, nullptr,
     nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeSwitch, "Left Stick X", JoystickAxis, 13, &joystick_axis[0], nullptr, nullptr, nullptr, 0, 0, 0,
     ""},
    {kOptionMenuItemTypeSwitch, "Left Stick Y", JoystickAxis, 13, &joystick_axis[1], nullptr, nullptr, nullptr, 0, 0, 0,
     ""},
    {kOptionMenuItemTypeSwitch, "Right Stick X", JoystickAxis, 13, &joystick_axis[2], nullptr, nullptr, nullptr, 0, 0,
     0, ""},
    {kOptionMenuItemTypeSwitch, "Right Stick Y", JoystickAxis, 13, &joystick_axis[3], nullptr, nullptr, nullptr, 0, 0,
     0, ""},
    {kOptionMenuItemTypeSlider, "Left X Deadzone", nullptr, 0, &joystick_deadzone_axis_0.f_,
     OptionMenuUpdateConsoleVariableFromFloat, nullptr, &joystick_deadzone_axis_0, 0.01f, 0.0f, 0.99f, "%0.2f"},
    {kOptionMenuItemTypeSlider, "Left Y Deadzone", nullptr, 0, &joystick_deadzone_axis_1.f_,
     OptionMenuUpdateConsoleVariableFromFloat, nullptr, &joystick_deadzone_axis_1, 0.01f, 0.0f, 0.99f, "%0.2f"},
    {kOptionMenuItemTypeSlider, "Right X Deadzone", nullptr, 0, &joystick_deadzone_axis_2.f_,
     OptionMenuUpdateConsoleVariableFromFloat, nullptr, &joystick_deadzone_axis_2, 0.01f, 0.0f, 0.99f, "%0.2f"},
    {kOptionMenuItemTypeSlider, "Right Y Deadzone", nullptr, 0, &joystick_deadzone_axis_3.f_,
     OptionMenuUpdateConsoleVariableFromFloat, nullptr, &joystick_deadzone_axis_3, 0.01f, 0.0f, 0.99f, "%0.2f"},
    {kOptionMenuItemTypeSlider, "Left Trigger Deadzone", nullptr, 0, &joystick_deadzone_axis_4.f_,
     OptionMenuUpdateConsoleVariableFromFloat, nullptr, &joystick_deadzone_axis_4, 0.01f, 0.0f, 0.99f, "%0.2f"},
    {kOptionMenuItemTypeSlider, "Right Trigger Deadzone", nullptr, 0, &joystick_deadzone_axis_5.f_,
     OptionMenuUpdateConsoleVariableFromFloat, nullptr, &joystick_deadzone_axis_5, 0.01f, 0.0f, 0.99f, "%0.2f"},
    {kOptionMenuItemTypePlain, "", nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeSlider, "Turning Speed", nullptr, 0, &turn_speed.f_, OptionMenuUpdateConsoleVariableFromFloat,
     nullptr, &turn_speed, 0.10f, 0.10f, 3.0f, "%0.2f"},
    {kOptionMenuItemTypeSlider, "Vertical Look Speed", nullptr, 0, &vertical_look_speed.f_,
     OptionMenuUpdateConsoleVariableFromFloat, nullptr, &vertical_look_speed, 0.10f, 0.10f, 3.0f, "%0.2f"},
    {kOptionMenuItemTypeSlider, "Forward Move Speed", nullptr, 0, &forward_speed.f_,
     OptionMenuUpdateConsoleVariableFromFloat, nullptr, &forward_speed, 0.10f, 0.10f, 3.0f, "%0.2f"},
    {kOptionMenuItemTypeSlider, "Side Move Speed", nullptr, 0, &side_speed.f_, OptionMenuUpdateConsoleVariableFromFloat,
     nullptr, &side_speed, 0.10f, 0.10f, 3.0f, "%0.2f"},
};

static OptionMenuDefinition analogue_optmenu = {
    analogueoptions,      sizeof(analogueoptions) / sizeof(OptionMenuItem), &options_menu_default_style, 150, 75, 1, "",
    language["MenuMouse"]};

//
//  SOUND OPTIONS
//
// -AJA- 2007/03/14 Added new sound menu
//
static OptionMenuItem soundoptions[] = {
    {kOptionMenuItemTypeSlider, "Sound Volume", nullptr, 0, &sound_effect_volume.f_,
     OptionMenuUpdateConsoleVariableFromFloat, nullptr, &sound_effect_volume, 0.05f, 0.0f, 1.0f, "%0.2f"},
    {kOptionMenuItemTypeSlider, "Movie/Music Volume", nullptr, 0, &music_volume.f_,
     OptionMenuUpdateConsoleVariableFromFloat, nullptr, &music_volume, 0.05f, 0.0f, 1.0f, "%0.2f"},
    {kOptionMenuItemTypePlain, "", nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeFunction, "MIDI Instrument Set", nullptr, 0, nullptr, OptionMenuChangeSoundfont, nullptr,
     nullptr, 0, 0, 0, ""},
#ifdef EDGE_CLASSIC
    {kOptionMenuItemTypeBoolean, "PC Speaker Mode", YesNo, 2, &pc_speaker_mode, OptionMenuChangePCSpeakerMode,
     "Music will be Off while this is enabled", nullptr, 0, 0, 0, ""},
#endif
    {kOptionMenuItemTypePlain, "", nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeSwitch, "Dynamic Reverb", "None/Headphones/Speakers", 3, &dynamic_reverb.d_,
     OptionMenuUpdateConsoleVariableFromInt, "Adds reverb to sounds in normal areas. Headphones-Weak, Speakers-Strong",
     &dynamic_reverb, 0, 0, 0, ""},
    {kOptionMenuItemTypePlain, "", nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0, 0, ""},
};

static OptionMenuDefinition sound_optmenu = {
    soundoptions,         sizeof(soundoptions) / sizeof(OptionMenuItem), &options_menu_default_style, 150, 75, 0, "",
    language["MenuSound"]};

//
//  F4 SOUND OPTIONS
//
//
static OptionMenuItem f4soundoptions[] = {
    {kOptionMenuItemTypeSlider, "Sound Volume", nullptr, 0, &sound_effect_volume.f_,
     OptionMenuUpdateConsoleVariableFromFloat, nullptr, &sound_effect_volume, 0.05f, 0.0f, 1.0f, "%0.2f"},
    {kOptionMenuItemTypeSlider, "Music Volume", nullptr, 0, &music_volume.f_, OptionMenuUpdateConsoleVariableFromFloat,
     nullptr, &music_volume, 0.05f, 0.0f, 1.0f, "%0.2f"},
};

static OptionMenuDefinition f4sound_optmenu = {
    f4soundoptions,       sizeof(f4soundoptions) / sizeof(OptionMenuItem), &options_menu_default_style, 150, 75, 0, "",
    language["MenuSound"]};

//
//  GAMEPLAY OPTIONS
//
// -ACB- 1998/07/15 Altered menu structure
// -KM- 1998/07/21 Change blood to switch
//
static OptionMenuItem playoptions[] = {
    {kOptionMenuItemTypeBoolean, "Pistol Starts", YesNo, 2, &pistol_starts, nullptr, nullptr, nullptr, 0, 0, 0, ""},

    {kOptionMenuItemTypeBoolean, "Mouse Look", YesNo, 2, &global_flags.mouselook, OptionMenuChangeMLook, nullptr,
     nullptr, 0, 0, 0, ""},

    {kOptionMenuItemTypeSwitch, "Aim Assist",
     "Off/Vertical/Vertical+Snap To/Vertical+Horizontal/Vertical+Horizontal+Snap To", 5, &global_flags.autoaim,
     OptionMenuChangeAutoAim, "Vertical assist is forced when mouselook is off", nullptr, 0, 0, 0, ""},

    {kOptionMenuItemTypeBoolean, "Jumping", YesNo, 2, &global_flags.jump, OptionMenuChangeJumping, nullptr, nullptr, 0,
     0, 0, ""},

    {kOptionMenuItemTypeBoolean, "Crouching", YesNo, 2, &global_flags.crouch, OptionMenuChangeCrouching, nullptr,
     nullptr, 0, 0, 0, ""},

    {kOptionMenuItemTypeBoolean, "Weapon Kick", YesNo, 2, &global_flags.kicking, OptionMenuChangeKicking, nullptr,
     nullptr, 0, 0, 0, ""},

    {kOptionMenuItemTypeBoolean, "Weapon Auto-Switch", YesNo, 2, &global_flags.weapon_switch,
     OptionMenuChangeWeaponSwitch, nullptr, nullptr, 0, 0, 0, ""},

    {kOptionMenuItemTypeSwitch, "Blood Level", "Normal/Extra/None", 3, &gore_level.d_,
     OptionMenuUpdateConsoleVariableFromInt, "Blood", &gore_level, 0, 0, 0, ""},

    {kOptionMenuItemTypeBoolean, "Extras", YesNo, 2, &global_flags.have_extra, OptionMenuChangeExtra, nullptr, nullptr,
     0, 0, 0, ""},

    {kOptionMenuItemTypeBoolean, "True 3D Gameplay", YesNo, 2, &global_flags.true_3d_gameplay, OptionMenuChangeTrue3d,
     "True3d", nullptr, 0, 0, 0, ""},

    {kOptionMenuItemTypeBoolean, "Shoot-thru Scenery", YesNo, 2, &global_flags.pass_missile,
     OptionMenuChangePassMissile, nullptr, nullptr, 0, 0, 0, ""},

    {kOptionMenuItemTypeBoolean, "Erraticism", YesNo, 2, &erraticism.d_, OptionMenuUpdateConsoleVariableFromInt,
     "Time only advances when you move or fire", &erraticism, 0, 0, 0, ""},

    {kOptionMenuItemTypeSlider, "OptGravity", nullptr, 0, &gravity_factor.f_, OptionMenuUpdateConsoleVariableFromFloat,
     "Gravity", &gravity_factor, 0.10f, 0.0f, 2.0f, "%gx"},

    {kOptionMenuItemTypeBoolean, "Respawn Enemies", YesNo, 2, &global_flags.enemies_respawn, OptionMenuChangeRespawn,
     nullptr, nullptr, 0, 0, 0, ""},

    {kOptionMenuItemTypeBoolean, "Enemy Respawn Mode", "Teleport/Resurrect", 2, &global_flags.enemy_respawn_mode,
     OptionMenuChangeMonsterRespawn, nullptr, nullptr, 0, 0, 0, ""},

    {kOptionMenuItemTypeBoolean, "Item Respawn", YesNo, 2, &global_flags.items_respawn, OptionMenuChangeItemRespawn,
     nullptr, nullptr, 0, 0, 0, ""},

    {kOptionMenuItemTypeBoolean, "Fast Monsters", YesNo, 2, &global_flags.fast_monsters, OptionMenuChangeFastparm,
     nullptr, nullptr, 0, 0, 0, ""}};

static OptionMenuDefinition gameplay_optmenu = {playoptions,
                                                sizeof(playoptions) / sizeof(OptionMenuItem),
                                                &options_menu_default_style,
                                                160,
                                                46,
                                                0,
                                                "",
                                                language["MenuGameplay"]};

//
//  PERFORMANCE OPTIONS
//
//
static OptionMenuItem perfoptions[] = {
    {kOptionMenuItemTypeSwitch, "Detail Level", "Low/Medium/High", 3, &detail_level, OptionMenuChangeMipMap, nullptr,
     nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeBoolean, "Simple Skies", YesNo, 2, &renderer_dumb_sky.d_,
     OptionMenuUpdateConsoleVariableFromInt, "Speeds up sky drawing, but breaks sky flooding and other hacks",
     &renderer_dumb_sky, 0, 0, 0, ""},
    {kOptionMenuItemTypeBoolean, "Draw Distance Culling", YesNo, 2, &draw_culling.d_,
     OptionMenuUpdateConsoleVariableFromInt, "Sector/Level Fog will be disabled when this is On", &draw_culling, 0, 0,
     0, ""},
    {kOptionMenuItemTypeSlider, "Maximum Draw Distance", nullptr, 0, &draw_culling_distance.f_,
     OptionMenuUpdateConsoleVariableFromFloat, "Only effective when Draw Distance Culling is On",
     &draw_culling_distance, 200.0f, 1000.0f, 8000.0f, "%g Units"},
    {kOptionMenuItemTypeSwitch, "Outdoor Culling Fog Color", "Match Sky/White/Grey/Black", 4, &cull_fog_color.d_,
     OptionMenuUpdateConsoleVariableFromInt, "Only effective when Draw Distance Culling is On", &cull_fog_color, 0, 0,
     0, ""},
    {kOptionMenuItemTypeBoolean, "Slow Thinkers Over Distance", YesNo, 2, &distance_cull_thinkers.d_,
     OptionMenuUpdateConsoleVariableFromInt, "Only recommended for extreme monster/projectile counts",
     &distance_cull_thinkers, 0, 0, 0, ""},
    {kOptionMenuItemTypeSwitch, "Maximum Dynamic Lights", "Unlimited/20/40/60/80/100", 6, &max_dynamic_lights.d_,
     OptionMenuUpdateConsoleVariableFromInt, "Control how many dynamic lights are rendered per tick",
     &max_dynamic_lights, 0, 0, 0, ""},
};

static OptionMenuDefinition perf_optmenu = {perfoptions,
                                            sizeof(perfoptions) / sizeof(OptionMenuItem),
                                            &options_menu_default_style,
                                            160,
                                            46,
                                            0,
                                            "",
                                            language["MenuPerformance"]};

//
//  ACCESSIBILITY OPTIONS
//
//
static OptionMenuItem accessibilityoptions[] = {
    {kOptionMenuItemTypeSwitch, "View Bobbing", "Full/Head Only/Weapon Only/None", 4, &view_bobbing.d_,
     OptionMenuChangeBobbing, "May help with motion sickness", nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeSwitch, "Reduce Flashing", YesNo, 2, &reduce_flash, nullptr,
     "May help with epilepsy or photosensitivity", nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeBoolean, "Automap: Keyed Doors Pulse", YesNo, 2, &automap_keydoor_blink, nullptr,
     "Can help locate doors more easily", nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeSwitch, "Automap: Keyed Doors Overlay", "Nothing/Text/Graphic", 3, &automap_keydoor_text.d_,
     OptionMenuUpdateConsoleVariableFromInt, "Required key shown visually", &automap_keydoor_text, 0, 0, 0, ""},
};

static OptionMenuDefinition accessibility_optmenu = {accessibilityoptions,
                                                     sizeof(accessibilityoptions) / sizeof(OptionMenuItem),
                                                     &options_menu_default_style,
                                                     160,
                                                     46,
                                                     0,
                                                     "",
                                                     language["MenuAccessibility"]};

//
//  KEY CONFIG : MOVEMENT
//
// -ACB- 1998/07/15 Altered menuinfo struct
// -KM- 1998/07/10 Used better names :-)
//
static OptionMenuItem move_keyconfig[] = {
    {kOptionMenuItemTypeKeyConfig, "Walk Forward", nullptr, 0, &key_up, nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeKeyConfig, "Walk Backwards", nullptr, 0, &key_down, nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypePlain, "", nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeKeyConfig, "Strafe Left", nullptr, 0, &key_strafe_left, nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeKeyConfig, "Strafe Right", nullptr, 0, &key_strafe_right, nullptr, nullptr, nullptr, 0, 0, 0,
     ""},
    {kOptionMenuItemTypePlain, "", nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeKeyConfig, "Turn Left", nullptr, 0, &key_left, nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeKeyConfig, "Turn Right", nullptr, 0, &key_right, nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypePlain, "", nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeKeyConfig, "Up / Jump", nullptr, 0, &key_fly_up, nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeKeyConfig, "Down / Crouch", nullptr, 0, &key_fly_down, nullptr, nullptr, nullptr, 0, 0, 0, ""},
};

static OptionMenuDefinition movement_optmenu = {move_keyconfig,
                                                sizeof(move_keyconfig) / sizeof(OptionMenuItem),
                                                &options_menu_default_style,
                                                140,
                                                98,
                                                0,
                                                "Movement",
                                                language["MenuBinding"]};

//
//  KEY CONFIG : ATTACK + LOOK
//
// -ACB- 1998/07/15 Altered menuinfo struct
// -ES- 1999/03/28 Added Zoom Key
//
static OptionMenuItem attack_keyconfig[] = {
    {kOptionMenuItemTypeKeyConfig, "Primary Attack", nullptr, 0, &key_fire, nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeKeyConfig, "Secondary Attack", nullptr, 0, &key_second_attack, nullptr, nullptr, nullptr, 0, 0,
     0, ""},
    {kOptionMenuItemTypeKeyConfig, "Third Attack", nullptr, 0, &key_third_attack, nullptr, nullptr, nullptr, 0, 0, 0,
     ""},
    {kOptionMenuItemTypeKeyConfig, "Fourth Attack", nullptr, 0, &key_fourth_attack, nullptr, nullptr, nullptr, 0, 0, 0,
     ""},
    {kOptionMenuItemTypeKeyConfig, "Next Weapon", nullptr, 0, &key_next_weapon, nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeKeyConfig, "Previous Weapon", nullptr, 0, &key_previous_weapon, nullptr, nullptr, nullptr, 0, 0,
     0, ""},
    {kOptionMenuItemTypeKeyConfig, "Weapon Reload", nullptr, 0, &key_reload, nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypePlain, "", nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeKeyConfig, "Zoom in/out", nullptr, 0, &key_zoom, nullptr, nullptr, nullptr, 0, 0, 0, ""},
};

static OptionMenuDefinition attack_optmenu = {attack_keyconfig,
                                              sizeof(attack_keyconfig) / sizeof(OptionMenuItem),
                                              &options_menu_default_style,
                                              140,
                                              98,
                                              0,
                                              "Attack",
                                              language["MenuBinding"]};

static OptionMenuItem look_keyconfig[] = {
    {kOptionMenuItemTypeKeyConfig, "Look Up", nullptr, 0, &key_look_up, nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeKeyConfig, "Look Down", nullptr, 0, &key_look_down, nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeKeyConfig, "Center View", nullptr, 0, &key_look_center, nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeKeyConfig, "Mouse Look", nullptr, 0, &key_mouselook, nullptr, nullptr, nullptr, 0, 0, 0, ""},
};

static OptionMenuDefinition look_optmenu = {look_keyconfig,
                                            sizeof(look_keyconfig) / sizeof(OptionMenuItem),
                                            &options_menu_default_style,
                                            140,
                                            98,
                                            0,
                                            "Look",
                                            language["MenuBinding"]};

//
//  KEY CONFIG : OTHER STUFF
//
static OptionMenuItem other_keyconfig[] = {
    {kOptionMenuItemTypeKeyConfig, "Use Item", nullptr, 0, &key_use, nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeKeyConfig, "Strafe", nullptr, 0, &key_strafe, nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeKeyConfig, "Run", nullptr, 0, &key_speed, nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeKeyConfig, "Toggle Autorun", nullptr, 0, &key_autorun, nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeKeyConfig, "180 degree turn", nullptr, 0, &key_180, nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypePlain, "", nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeKeyConfig, "Map Toggle", nullptr, 0, &key_map, nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeKeyConfig, "Action 1", nullptr, 0, &key_action1, nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeKeyConfig, "Action 2", nullptr, 0, &key_action2, nullptr, nullptr, nullptr, 0, 0, 0, ""},

    ///	{kOptionMenuItemTypeKeyConfig, "Multiplayer Talk", nullptr, 0,
    ///&key_talk, nullptr, nullptr},
};

static OptionMenuDefinition otherkey_optmenu = {other_keyconfig,
                                                sizeof(other_keyconfig) / sizeof(OptionMenuItem),
                                                &options_menu_default_style,
                                                140,
                                                98,
                                                0,
                                                "Other Keys",
                                                language["MenuBinding"]};

//
//  KEY CONFIG : WEAPONS
//
static OptionMenuItem weapon_keyconfig[] = {
    {kOptionMenuItemTypeKeyConfig, "Weapon 1", nullptr, 0, &key_weapons[1], nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeKeyConfig, "Weapon 2", nullptr, 0, &key_weapons[2], nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeKeyConfig, "Weapon 3", nullptr, 0, &key_weapons[3], nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeKeyConfig, "Weapon 4", nullptr, 0, &key_weapons[4], nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeKeyConfig, "Weapon 5", nullptr, 0, &key_weapons[5], nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypePlain, "", nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeKeyConfig, "Weapon 6", nullptr, 0, &key_weapons[6], nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeKeyConfig, "Weapon 7", nullptr, 0, &key_weapons[7], nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeKeyConfig, "Weapon 8", nullptr, 0, &key_weapons[8], nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeKeyConfig, "Weapon 9", nullptr, 0, &key_weapons[9], nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeKeyConfig, "Weapon 0", nullptr, 0, &key_weapons[0], nullptr, nullptr, nullptr, 0, 0, 0, ""},
};

static OptionMenuDefinition weapon_optmenu = {weapon_keyconfig,
                                              sizeof(weapon_keyconfig) / sizeof(OptionMenuItem),
                                              &options_menu_default_style,
                                              140,
                                              98,
                                              0,
                                              "Weapon Keys",
                                              language["MenuBinding"]};

//
//  KEY CONFIG : AUTOMAP
//
static OptionMenuItem automap_keyconfig[] = {
    {kOptionMenuItemTypeKeyConfig, "Pan Up", nullptr, 0, &key_automap_up, nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeKeyConfig, "Pan Down", nullptr, 0, &key_automap_down, nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeKeyConfig, "Pan Left", nullptr, 0, &key_automap_left, nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeKeyConfig, "Pan Right", nullptr, 0, &key_automap_right, nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypePlain, "", nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeKeyConfig, "Follow Mode", nullptr, 0, &key_automap_follow, nullptr, nullptr, nullptr, 0, 0, 0,
     ""},
    {kOptionMenuItemTypeKeyConfig, "Show Grid", nullptr, 0, &key_automap_grid, nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeKeyConfig, "Zoom In", nullptr, 0, &key_automap_zoom_in, nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeKeyConfig, "Zoom Out", nullptr, 0, &key_automap_zoom_out, nullptr, nullptr, nullptr, 0, 0, 0,
     ""},
    {kOptionMenuItemTypeKeyConfig, "Add Mark", nullptr, 0, &key_automap_mark, nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeKeyConfig, "Clear Marks", nullptr, 0, &key_automap_clear, nullptr, nullptr, nullptr, 0, 0, 0,
     ""},
};

static OptionMenuDefinition automap_optmenu = {automap_keyconfig,
                                               sizeof(automap_keyconfig) / sizeof(OptionMenuItem),
                                               &options_menu_default_style,
                                               140,
                                               98,
                                               0,
                                               "Automap Keys",
                                               language["MenuBinding"]};

//
//  KEY CONFIG : INVENTORY
//
static OptionMenuItem inventory_keyconfig[] = {
    {kOptionMenuItemTypeKeyConfig, "Previous Item", nullptr, 0, &key_inventory_previous, nullptr, nullptr, nullptr, 0,
     0, 0, ""},
    {kOptionMenuItemTypeKeyConfig, "Use Item", nullptr, 0, &key_inventory_use, nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeKeyConfig, "Next Item", nullptr, 0, &key_inventory_next, nullptr, nullptr, nullptr, 0, 0, 0,
     ""},
};

static OptionMenuDefinition inventory_optmenu = {inventory_keyconfig,
                                                 sizeof(inventory_keyconfig) / sizeof(OptionMenuItem),
                                                 &options_menu_default_style,
                                                 140,
                                                 98,
                                                 0,
                                                 "Inventory",
                                                 language["MenuBinding"]};

//
//  KEY CONFIG : PROGRAM
//
static OptionMenuItem program_keyconfig1[] = {
    {kOptionMenuItemTypeKeyConfig, "Screenshot", nullptr, 0, &key_screenshot, nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeKeyConfig, "Console", nullptr, 0, &key_console, nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeKeyConfig, "Pause", nullptr, 0, &key_pause, nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeKeyConfig, "Save Game", nullptr, 0, &key_save_game, nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeKeyConfig, "Load Game", nullptr, 0, &key_load_game, nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypePlain, "", nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeKeyConfig, "Sound Controls", nullptr, 0, &key_sound_controls, nullptr, nullptr, nullptr, 0, 0,
     0, ""},
    {kOptionMenuItemTypeKeyConfig, "Options", nullptr, 0, &key_options_menu, nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeKeyConfig, "Quicksave", nullptr, 0, &key_quick_save, nullptr, nullptr, nullptr, 0, 0, 0, ""},
};

static OptionMenuDefinition program_optmenu1 = {program_keyconfig1,
                                                sizeof(program_keyconfig1) / sizeof(OptionMenuItem),
                                                &options_menu_default_style,
                                                140,
                                                98,
                                                0,
                                                "Program (1/2)",
                                                language["MenuBinding"]};

//
//  KEY CONFIG : PROGRAM
//
static OptionMenuItem program_keyconfig2[] = {
    {kOptionMenuItemTypeKeyConfig, "End Game", nullptr, 0, &key_end_game, nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeKeyConfig, "Toggle Messages", nullptr, 0, &key_message_toggle, nullptr, nullptr, nullptr, 0, 0,
     0, ""},
    {kOptionMenuItemTypeKeyConfig, "OptQuickLoad", nullptr, 0, &key_quick_load, nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypePlain, "", nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeKeyConfig, "Quit EDGE", nullptr, 0, &key_quit_edge, nullptr, nullptr, nullptr, 0, 0, 0, ""},
    {kOptionMenuItemTypeKeyConfig, "Toggle Gamma", nullptr, 0, &key_gamma_toggle, nullptr, nullptr, nullptr, 0, 0, 0,
     ""},
    {kOptionMenuItemTypeKeyConfig, "Show Players", nullptr, 0, &key_show_players, nullptr, nullptr, nullptr, 0, 0, 0,
     ""},
};

static OptionMenuDefinition program_optmenu2 = {program_keyconfig2,
                                                sizeof(program_keyconfig2) / sizeof(OptionMenuItem),
                                                &options_menu_default_style,
                                                140,
                                                98,
                                                0,
                                                "Program (2/2)",
                                                language["MenuBinding"]};

/*
 * ALL KEYBOARD MENUS
 */
static constexpr uint8_t kTotalKeyMenus = 9;

static OptionMenuDefinition *all_key_menus[kTotalKeyMenus] = {&movement_optmenu,  &attack_optmenu,   &look_optmenu,
                                                              &otherkey_optmenu,  &weapon_optmenu,   &automap_optmenu,
                                                              &inventory_optmenu, &program_optmenu1, &program_optmenu2};

static char keystring1[] = "Enter/A Button to change, Backspace/Back Button to clear";
static char keystring2[] = "Press a key for this action";

//
// OptionMenuCheckNetworkGame
//
// Sets the first option to be "Leave Game" or "Multiplayer Game"
// depending on whether we are playing a game or not.
//
void OptionMenuCheckNetworkGame(void)
{
    if (game_state >= kGameStateLevel)
    {
        mainoptions[kOptionMenuNetworkHostPosition + 0].name    = language["MainEndBotMatch"];
        mainoptions[kOptionMenuNetworkHostPosition + 0].routine = &MenuEndGame;
        mainoptions[kOptionMenuNetworkHostPosition + 0].help    = nullptr;
    }
    else
    {
        mainoptions[kOptionMenuNetworkHostPosition + 0].name    = language["MenuStartBotmatch"]; //"Start Bot Match";
        mainoptions[kOptionMenuNetworkHostPosition + 0].routine = &OptionMenuHostNetGame;
        mainoptions[kOptionMenuNetworkHostPosition + 0].help    = nullptr;
    }
}

void OptionMenuInitialize()
{
    option_menu_on   = 0;
    current_menu     = &main_optmenu;
    current_item     = current_menu->items + current_menu->pos;
    current_key_menu = 0;
    keyscan          = 0;

    InitMonitorSize();

    // load styles
    StyleDefinition *def;

    def = styledefs.Lookup("OPTIONS");
    if (!def)
        def = default_style;
    options_menu_default_style = hud_styles.Lookup(def);

    // Lobo 2022: load our ddflang stuff
    main_optmenu.name          = language["MenuOptions"];
    video_optmenu.name         = language["MenuVideo"];
    ui_optmenu.name            = language["MenuUI"];
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
}

void OptionMenuTicker(void)
{
    // nothing needed
}

void OptionMenuDrawer()
{
    char         tempstring[80];
    int          curry, deltay, menutop;
    unsigned int k;

    Style *style = current_menu->style_var[0];
    EPI_ASSERT(style);

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
    CenterX -= (style->fonts_[fontType]->StringWidth(current_menu->name) * TEXTscale * 1.5) / 2;

    // Lobo 2022
    HUDWriteText(style, fontType, CenterX, menutop, current_menu->name, 1.5);

    fontType  = StyleDefinition::kTextSectionText;
    TEXTscale = style->definition_->text_[fontType].scale_;
    font_h    = style->fonts_[fontType]->NominalHeight();
    font_h *= TEXTscale;
    menutop = 68 - ((current_menu->item_number * font_h) / 2);
    if (current_menu->key_page[0])
        menutop = 9 * font_h / 2;

    // These don't seem used after this point in the function - Dasho
    // CenterX = 160;
    // CenterX -= (style->fonts_[fontType]->StringWidth(current_menu->name)
    // * 1.5) / 2;

    // now, draw all the menuitems
    deltay = 1 + font_h + style->definition_->entry_spacing_;

    curry = menutop + 25;

    if (current_menu->key_page[0])
    {
        fontType  = StyleDefinition::kTextSectionTitle;
        TEXTscale = style->definition_->text_[fontType].scale_;

        if (current_key_menu > 0)
            HUDWriteText(style, fontType, 60, 200 - deltay * 4, "< PREV");

        if (current_key_menu < kTotalKeyMenus - 1)
            HUDWriteText(style, fontType, 260 - style->fonts_[fontType]->StringWidth("NEXT >") * TEXTscale,
                         200 - deltay * 4, "NEXT >");

        fontType  = StyleDefinition::kTextSectionHelp;
        TEXTscale = style->definition_->text_[fontType].scale_;

        HUDWriteText(style, fontType,
                     160 - style->fonts_[fontType]->StringWidth(current_menu->key_page) * TEXTscale / 2, curry,
                     current_menu->key_page);
        curry += font_h * 2;

        if (keyscan)
            HUDWriteText(style, fontType, 160 - (style->fonts_[fontType]->StringWidth(keystring2) * TEXTscale / 2),
                         200 - deltay * 2, keystring2);
        else
            HUDWriteText(style, fontType, 160 - (style->fonts_[fontType]->StringWidth(keystring1) * TEXTscale / 2),
                         200 - deltay * 2, keystring1);
    }
    else if (current_menu == &res_optmenu)
    {
        OptionMenuResOptDrawer(style, curry, curry + (deltay * res_optmenu.item_number - 2), deltay,
                               current_menu->menu_center);
    }
    else if (current_menu == &main_optmenu)
    {
        OptionMenuLanguageDrawer(current_menu->menu_center, curry, deltay);
    }

    for (int i = 0; i < current_menu->item_number; i++)
    {
        bool is_selected = (i == current_menu->pos);

        if (current_menu == &res_optmenu && current_menu->items[i].routine == OptionMenuChangeResSize)
        {
            if (new_window_mode.window_mode == kWindowModeBorderless)
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

        const char *name_entry = language[current_menu->items[i].name];

        HUDWriteText(style, fontType,
                     (current_menu->menu_center) - (style->fonts_[fontType]->StringWidth(name_entry) * TEXTscale),
                     curry, name_entry);

        // Draw current soundfont
        if (current_menu == &sound_optmenu && current_menu->items[i].routine == OptionMenuChangeSoundfont)
        {
            fontType  = StyleDefinition::kTextSectionAlternate;
            TEXTscale = style->definition_->text_[fontType].scale_;
            HUDWriteText(style, fontType, (current_menu->menu_center) + 15, curry, midi_soundfont.s_.c_str());
        }

        // Draw current overlay
        if (current_menu == &video_optmenu && current_menu->items[i].routine == OptionMenuChangeOverlay)
        {
            fontType  = StyleDefinition::kTextSectionAlternate;
            TEXTscale = style->definition_->text_[fontType].scale_;
            HUDWriteText(style, fontType, (current_menu->menu_center) + 15, curry, video_overlay.s_.c_str());
        }

        // Draw current crosshair
        if (current_menu == &ui_optmenu && current_menu->items[i].routine == OptionMenuChangeCrosshair)
        {
            fontType  = StyleDefinition::kTextSectionAlternate;
            TEXTscale = style->definition_->text_[fontType].scale_;
            HUDWriteText(style, fontType, (current_menu->menu_center) + 15, curry, crosshair_image.s_.c_str());
        }

        // -ACB- 1998/07/15 Menu Cursor is colour indexed.
        if (is_selected)
        {
            fontType  = StyleDefinition::kTextSectionTitle;
            TEXTscale = style->definition_->text_[fontType].scale_;
            if (style->fonts_[fontType]->definition_->type_ == kFontTypeImage)
            {
                int cursor = 16;
                HUDWriteText(style, fontType, (current_menu->menu_center + 4), curry, (const char *)&cursor);
            }
            else if (style->fonts_[fontType]->definition_->type_ == kFontTypeTrueType)
                HUDWriteText(style, fontType, (current_menu->menu_center + 4), curry, "+");
            else
                HUDWriteText(style, fontType, (current_menu->menu_center + 4), curry, "*");

            if (current_menu->items[i].help)
            {
                fontType         = StyleDefinition::kTextSectionHelp;
                TEXTscale        = style->definition_->text_[fontType].scale_;
                const char *help = language[current_menu->items[i].help];

                HUDWriteText(style, fontType, 160 - (style->fonts_[fontType]->StringWidth(help) * TEXTscale / 2),
                             200 - deltay * 2, help);
            }
        }

        // I believe it's all T_ALT
        fontType  = StyleDefinition::kTextSectionAlternate;
        TEXTscale = style->definition_->text_[fontType].scale_;

        switch (current_menu->items[i].type)
        {
        case kOptionMenuItemTypeBoolean:
        case kOptionMenuItemTypeSwitch: {
            if (current_menu == &analogue_optmenu && current_menu->items[i].switch_variable == &joystick_device)
            {
                if (joystick_device == 0)
                {
                    HUDWriteText(style, fontType, (current_menu->menu_center) + 15, curry, "None");
                    break;
                }
                else
                {
                    const char *joyname = SDL_JoystickNameForIndex(joystick_device - 1);
                    if (joyname)
                    {
                        HUDWriteText(style, fontType, (current_menu->menu_center) + 15, curry,
                                     epi::StringFormat("%d - %s", joystick_device, joyname).c_str());
                        break;
                    }
                    else
                    {
                        HUDWriteText(style, fontType, (current_menu->menu_center) + 15, curry,
                                     epi::StringFormat("%d - Not Connected", joystick_device).c_str());
                        break;
                    }
                }
            }

            k = 0;
            for (int j = 0; j < OptionMenuGetCurrentSwitchValue(&current_menu->items[i]); j++)
            {
                while ((current_menu->items[i].type_names[k] != '/') && (k < strlen(current_menu->items[i].type_names)))
                    k++;

                k++;
            }

            if (k < strlen(current_menu->items[i].type_names))
            {
                int j = 0;
                while ((current_menu->items[i].type_names[k] != '/') && (k < strlen(current_menu->items[i].type_names)))
                {
                    tempstring[j] = current_menu->items[i].type_names[k];
                    j++;
                    k++;
                }
                tempstring[j] = 0;
            }
            else
            {
                stbsp_sprintf(tempstring, "Invalid");
            }

            HUDWriteText(style, StyleDefinition::kTextSectionAlternate, (current_menu->menu_center) + 15, curry,
                         tempstring);
            break;
        }

        case kOptionMenuItemTypeSlider: {
            DrawMenuSlider(current_menu->menu_center + 15, curry, *(float *)current_menu->items[i].switch_variable,
                           current_menu->items[i].increment, 2, current_menu->items[i].min, current_menu->items[i].max,
                           current_menu->items[i].format_string);

            break;
        }

        case kOptionMenuItemTypeKeyConfig: {
            k = *(int *)(current_menu->items[i].switch_variable);
            OptionMenuKeyToString(k, tempstring);
            HUDWriteText(style, fontType, (current_menu->menu_center + 15), curry, tempstring);
            break;
        }

        default:
            break;
        }
        curry += deltay;
    }
}

//
// OptionMenuResDrawer
//
// Something of a hack, but necessary to give a better way of changing
// resolution
//
// -ACB- 1999/10/03 Written
//
static void OptionMenuResOptDrawer(Style *style, int topy, int bottomy, int dy, int centrex)
{
    char tempstring[80];

    // Draw current resolution
    int y = topy;

    // Draw resolution selection option
    y += (dy * 3);

    int   fontType  = StyleDefinition::kTextSectionAlternate;
    float TEXTscale = style->definition_->text_[fontType].scale_;

    stbsp_sprintf(tempstring, "%s",
                  new_window_mode.window_mode == kWindowModeBorderless ? "Borderless Fullscreen" : "Windowed");
    HUDWriteText(style, fontType, centrex + 15, y, tempstring);

    if (new_window_mode.window_mode == kWindowModeWindowed)
    {
        y += dy;
        stbsp_sprintf(tempstring, "%dx%d", new_window_mode.width, new_window_mode.height);
        HUDWriteText(style, fontType, centrex + 15, y, tempstring);
    }

    // Draw selected resolution and mode:
    y = bottomy;

    fontType  = StyleDefinition::kTextSectionHelp;
    TEXTscale = style->definition_->text_[fontType].scale_;

    stbsp_sprintf(tempstring, "Current Resolution:");
    HUDWriteText(style, fontType, 160 - (style->fonts_[fontType]->StringWidth(tempstring) * TEXTscale / 2), y,
                 tempstring);

    fontType  = StyleDefinition::kTextSectionAlternate;
    TEXTscale = style->definition_->text_[fontType].scale_;

    y += dy;
    y += 5;
    if (current_window_mode == kWindowModeBorderless)
        stbsp_sprintf(tempstring, "%s", "Borderless Fullscreen");
    else
        stbsp_sprintf(tempstring, "%d x %d %s", current_screen_width, current_screen_height, "Windowed");

    HUDWriteText(style, fontType, 160 - (style->fonts_[fontType]->StringWidth(tempstring) * TEXTscale / 2), y,
                 tempstring);
}

//
// OptionMenuLanguageDrawer
//
// Yet another hack (this stuff badly needs rewriting) to draw the
// current language name.
//
// -AJA- 2000/04/16 Written

static void OptionMenuLanguageDrawer(int x, int y, int deltay)
{
    HUDWriteText(options_menu_default_style, StyleDefinition::kTextSectionAlternate, x + 15,
                 y + deltay * kOptionMenuLanguagePosition, language.GetName());
}

static void KeyMenu_Next()
{
    if (current_key_menu >= kTotalKeyMenus - 1)
        return;

    current_key_menu++;

    current_menu = all_key_menus[current_key_menu];

    StartSoundEffect(sound_effect_pstop);
}

static void KeyMenu_Prev()
{
    if (current_key_menu <= 0)
        return;

    current_key_menu--;

    current_menu = all_key_menus[current_key_menu];

    StartSoundEffect(sound_effect_pstop);
}

//
// OptionMenuResponder
//
bool OptionMenuResponder(InputEvent *ev, int ch)
{
    ///--  	if (testticker != -1)
    ///--  		return true;

    current_item = current_menu->items + current_menu->pos; // Should help the accidental key
                                                            // binding to other options - Dasho

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
        if (ch == kEscape || ch == kGamepadStart)
            return true;

        blah = (int *)(current_item->switch_variable);
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
    case kBackspace:
    case kGamepadBack: {
        if (current_item->type == kOptionMenuItemTypeKeyConfig)
            *(int *)(current_item->switch_variable) = 0;
        return true;
    }

    case kDownArrow:
    case kGamepadDown: {
        do
        {
            current_menu->pos++;
            if (current_menu == &res_optmenu && new_window_mode.window_mode == kWindowModeBorderless)
            {
                if (current_menu->pos >= 0 && current_menu->pos < current_menu->item_number)
                {
                    if (current_menu->items[current_menu->pos].routine == OptionMenuChangeResSize)
                        current_menu->pos++;
                }
            }
            if (current_menu->pos >= current_menu->item_number)
                current_menu->pos = 0;
            current_item = current_menu->items + current_menu->pos;
        } while (current_item->type == 0);

        StartSoundEffect(sound_effect_pstop);
        return true;
    }

    case kMouseWheelDown: {
        do
        {
            current_menu->pos++;
            if (current_menu == &res_optmenu && new_window_mode.window_mode == kWindowModeBorderless)
            {
                if (current_menu->pos >= 0 && current_menu->pos < current_menu->item_number)
                {
                    if (current_menu->items[current_menu->pos].routine == OptionMenuChangeResSize)
                        current_menu->pos++;
                }
            }
            if (current_menu->pos >= current_menu->item_number)
            {
                if (current_menu->key_page[0])
                {
                    KeyMenu_Next();
                    current_menu->pos = 0;
                    return true;
                }
                current_menu->pos = 0;
            }
            current_item = current_menu->items + current_menu->pos;
        } while (current_item->type == 0);

        StartSoundEffect(sound_effect_pstop);
        return true;
    }

    case kUpArrow:
    case kGamepadUp: {
        do
        {
            current_menu->pos--;
            if (current_menu == &res_optmenu && new_window_mode.window_mode == kWindowModeBorderless)
            {
                if (current_menu->pos >= 0 && current_menu->pos < current_menu->item_number)
                {
                    if (current_menu->items[current_menu->pos].routine == OptionMenuChangeResSize)
                        current_menu->pos--;
                }
            }
            if (current_menu->pos < 0)
                current_menu->pos = current_menu->item_number - 1;
            current_item = current_menu->items + current_menu->pos;
        } while (current_item->type == 0);

        StartSoundEffect(sound_effect_pstop);
        return true;
    }

    case kMouseWheelUp: {
        do
        {
            current_menu->pos--;
            if (current_menu == &res_optmenu && new_window_mode.window_mode == kWindowModeBorderless)
            {
                if (current_menu->pos >= 0 && current_menu->pos < current_menu->item_number)
                {
                    if (current_menu->items[current_menu->pos].routine == OptionMenuChangeResSize)
                        current_menu->pos--;
                }
            }
            if (current_menu->pos < 0)
            {
                if (current_menu->key_page[0])
                {
                    KeyMenu_Prev();
                    current_menu->pos = current_menu->item_number - 1;
                    return true;
                }
                current_menu->pos = current_menu->item_number - 1;
            }
            current_item = current_menu->items + current_menu->pos;
        } while (current_item->type == 0);

        StartSoundEffect(sound_effect_pstop);
        return true;
    }

    case kLeftArrow:
    case kGamepadLeft: {
        if (current_menu->key_page[0])
        {
            KeyMenu_Prev();
            return true;
        }

        switch (current_item->type)
        {
        case kOptionMenuItemTypePlain: {
            return false;
        }

        case kOptionMenuItemTypeBoolean: {
            bool *boolptr = (bool *)current_item->switch_variable;

            *boolptr = !(*boolptr);

            StartSoundEffect(sound_effect_pistol);

            if (current_item->routine != nullptr)
                current_item->routine(ch, current_item->console_variable_to_change);

            return true;
        }

        case kOptionMenuItemTypeSwitch: {
            int *val_ptr = (int *)current_item->switch_variable;

            *val_ptr -= 1;

            if (*val_ptr < 0)
                *val_ptr = current_item->total_types - 1;

            StartSoundEffect(sound_effect_pistol);

            if (current_item->routine != nullptr)
                current_item->routine(ch, current_item->console_variable_to_change);

            return true;
        }

        case kOptionMenuItemTypeFunction: {
            if (current_item->routine != nullptr)
                current_item->routine(ch, current_item->console_variable_to_change);

            StartSoundEffect(sound_effect_pistol);
            return true;
        }

        case kOptionMenuItemTypeSlider: {
            float *val_ptr = (float *)current_item->switch_variable;

            *val_ptr = *val_ptr - (remainderf(*val_ptr, current_item->increment));

            if (*val_ptr > current_item->min)
            {
                *val_ptr = *val_ptr - current_item->increment;

                StartSoundEffect(sound_effect_stnmov);
            }

            if (current_item->routine != nullptr)
                current_item->routine(ch, current_item->console_variable_to_change);

            return true;
        }

        default:
            break;
        }
        FatalError("Invalid menu type!");
    }

    case kRightArrow:
    case kGamepadRight:
        if (current_menu->key_page[0])
        {
            KeyMenu_Next();
            return true;
        }

        /* FALL THROUGH... */

    case kEnter:
    case kMouse1:
    case kGamepadA: {
        switch (current_item->type)
        {
        case kOptionMenuItemTypePlain:
            return false;

        case kOptionMenuItemTypeBoolean: {
            bool *boolptr = (bool *)current_item->switch_variable;

            *boolptr = !(*boolptr);

            StartSoundEffect(sound_effect_pistol);

            if (current_item->routine != nullptr)
                current_item->routine(ch, current_item->console_variable_to_change);

            return true;
        }

        case kOptionMenuItemTypeSwitch: {
            int *val_ptr = (int *)current_item->switch_variable;

            *val_ptr += 1;

            if (*val_ptr >= current_item->total_types)
                *val_ptr = 0;

            StartSoundEffect(sound_effect_pistol);

            if (current_item->routine != nullptr)
                current_item->routine(ch, current_item->console_variable_to_change);

            return true;
        }

        case kOptionMenuItemTypeFunction: {
            if (current_item->routine != nullptr)
                current_item->routine(ch, current_item->console_variable_to_change);

            StartSoundEffect(sound_effect_pistol);
            return true;
        }

        case kOptionMenuItemTypeSlider: {
            float *val_ptr = (float *)current_item->switch_variable;

            *val_ptr = *val_ptr - (remainderf(*val_ptr, current_item->increment));

            if (*val_ptr < current_item->max)
            {
                *val_ptr = *val_ptr + current_item->increment;

                StartSoundEffect(sound_effect_stnmov);
            }

            if (current_item->routine != nullptr)
                current_item->routine(ch, current_item->console_variable_to_change);

            return true;
        }

        case kOptionMenuItemTypeKeyConfig: {
            keyscan = 1;
            return true;
        }

        default:
            break;
        }
        FatalError("Invalid menu type!");
    }
    case kEscape:
    case kMouse2:
    case kMouse3:
    case kGamepadB: {
        if (current_menu == &f4sound_optmenu)
        {
            current_menu = &main_optmenu;
            MenuClear();
        }
        else if (current_menu == &main_optmenu)
        {
            if (function_key_menu)
                MenuClear();
            else
                option_menu_on = 0;
        }
        else
        {
            current_menu = &main_optmenu;
            current_item = current_menu->items + current_menu->pos;
        }
        StartSoundEffect(sound_effect_swtchx);
        return true;
    }
    }
    return false;
}

// ===== SUB-MENU SETUP ROUTINES =====

//
// OptionMenuVideoOptions
//
static void OptionMenuVideoOptions(int key_pressed, ConsoleVariable *console_variable)
{
    EPI_UNUSED(key_pressed);
    EPI_UNUSED(console_variable);
    current_menu = &video_optmenu;
    current_item = current_menu->items + current_menu->pos;
}

// ===== SUB-MENU SETUP ROUTINES =====

//
// OptionMenuVideoOptions
//
static void OptionMenuUIOptions(int key_pressed, ConsoleVariable *console_variable)
{
    EPI_UNUSED(key_pressed);
    EPI_UNUSED(console_variable);
    current_menu = &ui_optmenu;
    current_item = current_menu->items + current_menu->pos;
}

//
// OptionMenuResolutionOptions
//
// This menu is different in the fact that is most be calculated at runtime,
// this is because different resolutions are available on different machines.
//
// -ES- 1998/08/20 Added
// -ACB 1999/10/03 rewrote to Use scrmodes array.
//
static void OptionMenuResolutionOptions(int key_pressed, ConsoleVariable *console_variable)
{
    EPI_UNUSED(key_pressed);
    EPI_UNUSED(console_variable);
    new_window_mode.width       = current_screen_width;
    new_window_mode.height      = current_screen_height;
    new_window_mode.depth       = current_screen_depth;
    new_window_mode.window_mode = current_window_mode;

    current_menu = &res_optmenu;
    current_item = current_menu->items + current_menu->pos;
}

//
// OptionMenuAnalogueOptions
//
static void OptionMenuAnalogueOptions(int key_pressed, ConsoleVariable *console_variable)
{
    EPI_UNUSED(key_pressed);
    EPI_UNUSED(console_variable);
    current_menu = &analogue_optmenu;
    current_item = current_menu->items + current_menu->pos;
}

//
// OptionMenuSoundOptions
//
static void OptionMenuSoundOptions(int key_pressed, ConsoleVariable *console_variable)
{
    EPI_UNUSED(key_pressed);
    EPI_UNUSED(console_variable);
    current_menu = &sound_optmenu;
    current_item = current_menu->items + current_menu->pos;
}

//
// MenuF4SoundOptions
//
void MenuF4SoundOptions(int choice)
{
    EPI_UNUSED(choice);
    option_menu_on = 1;
    current_menu   = &f4sound_optmenu;
    current_item   = current_menu->items + current_menu->pos;
}

//
// OptionMenuGameplayOptions
//
static void OptionMenuGameplayOptions(int key_pressed, ConsoleVariable *console_variable)
{
    EPI_UNUSED(key_pressed);
    EPI_UNUSED(console_variable);
    // not allowed in netgames (changing most of these options would
    // break synchronisation with the other machines).
    if (network_game)
        return;

    current_menu = &gameplay_optmenu;
    current_item = current_menu->items + current_menu->pos;
}

//
// OptionMenuPerformanceOptions
//
static void OptionMenuPerformanceOptions(int key_pressed, ConsoleVariable *console_variable)
{
    EPI_UNUSED(key_pressed);
    EPI_UNUSED(console_variable);
    // not allowed in netgames (changing most of these options would
    // break synchronisation with the other machines).
    if (network_game)
        return;

    current_menu = &perf_optmenu;
    current_item = current_menu->items + current_menu->pos;
}

//
// OptionMenuAccessibilityOptions
//
static void OptionMenuAccessibilityOptions(int key_pressed, ConsoleVariable *console_variable)
{
    EPI_UNUSED(key_pressed);
    EPI_UNUSED(console_variable);
    // not allowed in netgames (changing most of these options would
    // break synchronisation with the other machines).
    if (network_game)
        return;

    current_menu = &accessibility_optmenu;
    current_item = current_menu->items + current_menu->pos;
}

//
// OptionMenuKeyboardOptions
//
static void OptionMenuKeyboardOptions(int key_pressed, ConsoleVariable *console_variable)
{
    EPI_UNUSED(key_pressed);
    EPI_UNUSED(console_variable);
    current_menu = all_key_menus[current_key_menu];

    current_item = current_menu->items + current_menu->pos;
}

// ===== END OF SUB-MENUS =====

//
// OptionMenuKeyToString
//
static void OptionMenuKeyToString(int key, char *deststring)
{
    int key1 = key & 0xffff;
    int key2 = key >> 16;

    if (key1 == 0)
    {
        strcpy(deststring, "---");
        return;
    }

    strcpy(deststring, GetKeyName(key1));

    if (key2 != 0)
    {
        strcat(deststring, " or ");
        strcat(deststring, GetKeyName(key2));
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

static void OptionMenuChangeMonitorSize(int key_pressed, ConsoleVariable *console_variable)
{
    EPI_UNUSED(key_pressed);
    EPI_UNUSED(console_variable);
    static const float ratios[6] = {
        1.25000, 1.33333, 1.50000, // 5:4     4:3   3:2
        1.60000, 1.77777, 2.33333  // 16:10  16:9  21:9
    };

    monitor_size = HMM_Clamp(0, monitor_size, 5);

    monitor_aspect_ratio = ratios[monitor_size];
}

static void OptionMenuChangeMLook(int key_pressed, ConsoleVariable *console_variable)
{
    EPI_UNUSED(key_pressed);
    EPI_UNUSED(console_variable);
    if (current_map && ((current_map->force_on_ | current_map->force_off_) & kMapFlagMlook))
        return;

    level_flags.mouselook = global_flags.mouselook;
}

static void OptionMenuChangeJumping(int key_pressed, ConsoleVariable *console_variable)
{
    EPI_UNUSED(key_pressed);
    EPI_UNUSED(console_variable);
    if (current_map && ((current_map->force_on_ | current_map->force_off_) & kMapFlagJumping))
        return;

    level_flags.jump = global_flags.jump;
}

static void OptionMenuChangeCrouching(int key_pressed, ConsoleVariable *console_variable)
{
    EPI_UNUSED(key_pressed);
    EPI_UNUSED(console_variable);
    if (current_map && ((current_map->force_on_ | current_map->force_off_) & kMapFlagCrouching))
        return;

    level_flags.crouch = global_flags.crouch;
}

static void OptionMenuChangeExtra(int key_pressed, ConsoleVariable *console_variable)
{
    EPI_UNUSED(key_pressed);
    EPI_UNUSED(console_variable);
    if (current_map && ((current_map->force_on_ | current_map->force_off_) & kMapFlagExtras))
        return;

    level_flags.have_extra = global_flags.have_extra;
}

//
// OptionMenuChangeMonsterRespawn
//
// -ACB- 1998/08/09 New DDF settings, check that map allows the settings
//
static void OptionMenuChangeMonsterRespawn(int key_pressed, ConsoleVariable *console_variable)
{
    EPI_UNUSED(key_pressed);
    EPI_UNUSED(console_variable);
    if (current_map && ((current_map->force_on_ | current_map->force_off_) & kMapFlagResRespawn))
        return;

    level_flags.enemy_respawn_mode = global_flags.enemy_respawn_mode;
}

static void OptionMenuChangeItemRespawn(int key_pressed, ConsoleVariable *console_variable)
{
    EPI_UNUSED(key_pressed);
    EPI_UNUSED(console_variable);
    if (current_map && ((current_map->force_on_ | current_map->force_off_) & kMapFlagItemRespawn))
        return;

    level_flags.items_respawn = global_flags.items_respawn;
}

static void OptionMenuChangeTrue3d(int key_pressed, ConsoleVariable *console_variable)
{
    EPI_UNUSED(key_pressed);
    EPI_UNUSED(console_variable);
    if (current_map && ((current_map->force_on_ | current_map->force_off_) & kMapFlagTrue3D))
        return;

    level_flags.true_3d_gameplay = global_flags.true_3d_gameplay;
}

static void OptionMenuChangeAutoAim(int key_pressed, ConsoleVariable *console_variable)
{
    EPI_UNUSED(key_pressed);
    EPI_UNUSED(console_variable);
    if (current_map &&
        ((current_map->force_on_ | current_map->force_off_) &
         (kMapFlagAutoAimFull | kMapFlagAutoAimFullSnap | kMapFlagAutoAimVertical | kMapFlagAutoAimVerticalSnap)))
        return;

    level_flags.autoaim = global_flags.autoaim;
}

static void OptionMenuChangeRespawn(int key_pressed, ConsoleVariable *console_variable)
{
    EPI_UNUSED(key_pressed);
    EPI_UNUSED(console_variable);
    if (game_skill == kSkillNightmare)
        return;

    if (current_map && ((current_map->force_on_ | current_map->force_off_) & kMapFlagRespawn))
        return;

    level_flags.enemies_respawn = global_flags.enemies_respawn;
}

static void OptionMenuChangeFastparm(int key_pressed, ConsoleVariable *console_variable)
{
    EPI_UNUSED(key_pressed);
    EPI_UNUSED(console_variable);
    if (game_skill == kSkillNightmare)
        return;

    if (current_map && ((current_map->force_on_ | current_map->force_off_) & kMapFlagFastParm))
        return;

    level_flags.fast_monsters = global_flags.fast_monsters;
}

static void OptionMenuChangePassMissile(int key_pressed, ConsoleVariable *console_variable)
{
    EPI_UNUSED(key_pressed);
    EPI_UNUSED(console_variable);
    level_flags.pass_missile = global_flags.pass_missile;
}

// this used by both MIPMIP, SMOOTHING and DETAIL options
static void OptionMenuChangeMipMap(int key_pressed, ConsoleVariable *console_variable)
{
    EPI_UNUSED(key_pressed);
    EPI_UNUSED(console_variable);
    DeleteAllImages();
}

static void OptionMenuChangeBobbing(int key_pressed, ConsoleVariable *console_variable)
{
    EPI_UNUSED(key_pressed);
    EPI_UNUSED(console_variable);
    view_bobbing   = view_bobbing.d_;
    Player *player = players[console_player];
    if (player)
    {
        player->bob_factor_ = 0;
        PlayerSprite *psp   = &player->player_sprites_[player->action_player_sprite_];
        if (psp)
        {
            psp->screen_x     = 0;
            psp->screen_y     = 0;
            psp->old_screen_x = 0;
            psp->old_screen_y = 0;
        }
    }
}

static void OptionMenuUpdateConsoleVariableFromFloat(int key_pressed, ConsoleVariable *console_variable)
{
    EPI_UNUSED(key_pressed);
    EPI_ASSERT(console_variable);
    console_variable->operator=(console_variable->f_);
}

static void OptionMenuUpdateConsoleVariableFromInt(int key_pressed, ConsoleVariable *console_variable)
{
    EPI_UNUSED(key_pressed);
    EPI_ASSERT(console_variable);
    console_variable->operator=(console_variable->d_);
}

static void OptionMenuChangeKicking(int key_pressed, ConsoleVariable *console_variable)
{
    EPI_UNUSED(key_pressed);
    EPI_UNUSED(console_variable);
    if (current_map && ((current_map->force_on_ | current_map->force_off_) & kMapFlagKicking))
        return;

    level_flags.kicking = global_flags.kicking;
}

static void OptionMenuChangeWeaponSwitch(int key_pressed, ConsoleVariable *console_variable)
{
    EPI_UNUSED(key_pressed);
    EPI_UNUSED(console_variable);
    if (current_map && ((current_map->force_on_ | current_map->force_off_) & kMapFlagWeaponSwitch))
        return;

    level_flags.weapon_switch = global_flags.weapon_switch;
}

#ifdef EDGE_CLASSIC
static void OptionMenuChangePCSpeakerMode(int key_pressed, ConsoleVariable *console_variable)
{
    EPI_UNUSED(key_pressed);
    EPI_UNUSED(console_variable);
    // Clear SFX cache and restart music
    StopAllSoundEffects();
    SoundCacheClearAll();
    RestartMIDI();
}
#endif

//
// OptionMenuChangeLanguage
//
// -AJA- 2000/04/16 Run-time language changing...
//
static void OptionMenuChangeLanguage(int key_pressed, ConsoleVariable *console_variable)
{
    EPI_UNUSED(console_variable);
    if (key_pressed == kLeftArrow || key_pressed == kGamepadLeft)
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
    else if (key_pressed == kRightArrow || key_pressed == kGamepadRight)
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

    // update console_variable
    m_language = language.GetName();
}

//
// OptionMenuChangeSoundfont
//
//
static void OptionMenuChangeSoundfont(int key_pressed, ConsoleVariable *console_variable)
{
    EPI_UNUSED(console_variable);

    std::set<std::string>::iterator sf_pos;

    if (!available_soundfonts.count(midi_soundfont.s_)) // Something is weird
    {
        LogWarning("OptionMenuChangeSoundfont: Could not read list of available "
                   "soundfonts. "
                   "Falling back to default!\n");
        midi_soundfont = "Default";
        return;
    }
    else
        sf_pos = available_soundfonts.find(midi_soundfont.s_);

    if (key_pressed == kLeftArrow || key_pressed == kGamepadLeft)
    {
        if (sf_pos != available_soundfonts.begin())
            sf_pos--;
        else
        {
            sf_pos = available_soundfonts.end();
            sf_pos--;
        }
    }
    else if (key_pressed == kRightArrow || key_pressed == kGamepadRight)
    {
        sf_pos++;
        if (sf_pos == available_soundfonts.end())
            sf_pos = available_soundfonts.begin();
    }

    // update console_variable
    midi_soundfont = *sf_pos;
    RestartMIDI();
}

//
// OptionMenuChangeOverlay
//
//
static void OptionMenuChangeOverlay(int key_pressed, ConsoleVariable *console_variable)
{
    EPI_UNUSED(console_variable);

    std::map<std::string, std::pair<ImageData *, unsigned int>>::iterator ov_pos;

    if (!available_overlays.count(video_overlay.s_)) // Something is weird
    {
        LogWarning("OptionMenuChangeOverlay: Could not read list of available "
                   "overlays. "
                   "Falling back to default (none)!\n");
        video_overlay = "None";
        return;
    }
    else
        ov_pos = available_overlays.find(video_overlay.s_);

    if (key_pressed == kLeftArrow || key_pressed == kGamepadLeft)
    {
        if (ov_pos != available_overlays.begin())
            ov_pos--;
        else
        {
            ov_pos = available_overlays.end();
            ov_pos--;
        }
    }
    else if (key_pressed == kRightArrow || key_pressed == kGamepadRight)
    {
        ov_pos++;
        if (ov_pos == available_overlays.end())
            ov_pos = available_overlays.begin();
    }

    // update console_variable
    video_overlay = ov_pos->first;
}

//
// OptionMenuChangeCrosshair
//
//
static void OptionMenuChangeCrosshair(int key_pressed, ConsoleVariable *console_variable)
{
    EPI_UNUSED(console_variable);

    std::map<std::string, unsigned int>::iterator ch_pos;

    if (!available_crosshairs.count(crosshair_image.s_)) // Something is weird
    {
        LogWarning("OptionMenuChangeOverlay: Could not read list of available "
                   "overlays. "
                   "Falling back to default (none)!\n");
        crosshair_image = "None";
        return;
    }
    else
        ch_pos = available_crosshairs.find(crosshair_image.s_);

    if (key_pressed == kLeftArrow || key_pressed == kGamepadLeft)
    {
        if (ch_pos != available_crosshairs.begin())
            ch_pos--;
        else
        {
            ch_pos = available_crosshairs.end();
            ch_pos--;
        }
    }
    else if (key_pressed == kRightArrow || key_pressed == kGamepadRight)
    {
        ch_pos++;
        if (ch_pos == available_crosshairs.end())
            ch_pos = available_crosshairs.begin();
    }

    // update console_variable
    crosshair_image = ch_pos->first;
}

//
// OptionMenuChangeGamepad
//
//
static void OptionMenuChangeGamepad(int key_pressed, ConsoleVariable *console_variable)
{
    EPI_UNUSED(key_pressed);
    EPI_UNUSED(console_variable);
    CheckJoystickChanged();
}

//
// OptionMenuChangeResSize
//
// -ACB- 1998/08/29 Resolution Changes...
//
static void OptionMenuChangeResSize(int key_pressed, ConsoleVariable *console_variable)
{
    EPI_UNUSED(console_variable);
    if (key_pressed == kLeftArrow || key_pressed == kGamepadLeft)
    {
        IncrementResolution(&new_window_mode, kIncrementSize, -1);
    }
    else if (key_pressed == kRightArrow || key_pressed == kGamepadRight)
    {
        IncrementResolution(&new_window_mode, kIncrementSize, +1);
    }
}

//
// OptionMenuChangeResFull
//
// -AJA- 2005/01/02: Windowed vs Fullscreen
//
static void OptionMenuChangeResFull(int key_pressed, ConsoleVariable *console_variable)
{
    EPI_UNUSED(console_variable);
    if (key_pressed == kLeftArrow || key_pressed == kGamepadLeft)
    {
        IncrementResolution(&new_window_mode, kIncrementWindowMode, -1);
    }
    else if (key_pressed == kRightArrow || key_pressed == kGamepadRight)
    {
        IncrementResolution(&new_window_mode, kIncrementWindowMode, +1);
    }
}

//
// OptionMenuSetResolution
//
static void OptionMenuSetResolution(int key_pressed, ConsoleVariable *console_variable)
{
    EPI_UNUSED(key_pressed);
    EPI_UNUSED(console_variable);
    if (ChangeResolution(&new_window_mode))
    {
        if (new_window_mode.window_mode == kWindowModeWindowed)
        {
            toggle_windowed_depth       = new_window_mode.depth;
            toggle_windowed_height      = new_window_mode.height;
            toggle_windowed_width       = new_window_mode.width;
            toggle_windowed_window_mode = new_window_mode.window_mode;
        }
        SoftInitializeResolution();
    }
    else
    {
        StartMenuMessage(epi::StringFormat("%s %d x %d x %dbpp! %s\n", language["ModeSelErr"], new_window_mode.width,
                                           new_window_mode.height, (new_window_mode.depth < 20) ? 16 : 32,
                                           language["PressKey"])
                             .c_str(),
                         nullptr, false);
    }
}

void OptionMenuHostNetGame(int key_pressed, ConsoleVariable *console_variable)
{
    EPI_UNUSED(key_pressed);
    EPI_UNUSED(console_variable);
    option_menu_on       = 0;
    network_game_menu_on = 1;

    OptionMenuNetworkHostBegun();
}
#ifndef EDGE_WEB
void OptionMenuBrowseHome(int key_pressed, ConsoleVariable *console_variable)
{
    EPI_UNUSED(key_pressed);
    EPI_UNUSED(console_variable);

    epi::OpenDirectory(home_directory);
}
#endif
void MenuOptions(int choice)
{
    option_menu_on    = 1;
    function_key_menu = (choice == 1);
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
