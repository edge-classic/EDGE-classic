//----------------------------------------------------------------------------
//  EDGE Misc: Screenshots, Menu and defaults Code
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
// -MH- 1998/07/02  Added key_fly_up and key_fly_down
// -MH- 1998/07/02 "shootupdown" --> "true_3d_gameplay"
// -ACB- 2000/06/02 Removed Control Defaults
//

#include "m_misc.h"

#include <stdarg.h>

#include "am_map.h"
#include "con_main.h"
#include "defaults.h"
#include "dm_defs.h"
#include "dm_state.h"
#include "e_input.h"
#include "e_player.h"
#include "epi.h"
#include "epi_endian.h"
#include "epi_file.h"
#include "epi_filesystem.h"
#include "epi_scanner.h"
#include "epi_str_compare.h"
#include "epi_str_util.h"
#include "g_game.h"
#include "hu_draw.h"
#include "im_data.h"
#include "im_funcs.h"
#include "m_argv.h"
#include "m_menu.h"
#include "m_option.h"
#include "n_network.h"
#include "p_spec.h"
#include "r_backend.h"
#include "r_colormap.h"
#include "r_draw.h"
#include "r_gldefs.h"
#include "r_image.h"
#include "r_modes.h"
#include "r_wipe.h"
#include "s_blit.h"
#include "s_sound.h"
#include "stb_sprintf.h"
#include "version.h"
//
// DEFAULTS
//
bool show_old_config_warning = false;

extern ConsoleVariable midi_soundfont;
extern bool            pc_speaker_mode;

static bool done_first_init = false;

static ConfigurationDefault defaults[] = {
    {kConfigInteger, "screenwidth", &current_screen_width, EDGE_DEFAULT_SCREENWIDTH},
    {kConfigInteger, "screenheight", &current_screen_height, EDGE_DEFAULT_SCREENHEIGHT},
    {kConfigInteger, "screendepth", &current_screen_depth, EDGE_DEFAULT_SCREENBITS},
    {kConfigInteger, "displaymode", &current_window_mode, EDGE_DEFAULT_DISPLAYMODE},

    {kConfigBoolean, "pc_speaker_mode", &pc_speaker_mode, 0},
    {kConfigBoolean, "dynamic_reverb", &dynamic_reverb, 0},

    // -ES- 1998/11/28 Save fade settings
    {kConfigInteger, "reduce_flash", &reduce_flash, 0},
    {kConfigInteger, "invuln_fx", &invulnerability_effect, EDGE_DEFAULT_INVUL_FX},
    {kConfigEnum, "wipe_method", &wipe_method, EDGE_DEFAULT_WIPE_METHOD},
    {kConfigBoolean, "rotate_map", &rotate_map, EDGE_DEFAULT_ROTATEMAP},
    {kConfigBoolean, "respawnsetting", &global_flags.enemy_respawn_mode, EDGE_DEFAULT_RES_RESPAWN},
    {kConfigBoolean, "items_respawn", &global_flags.items_respawn, EDGE_DEFAULT_ITEMRESPAWN},
    {kConfigBoolean, "respawn", &global_flags.enemies_respawn, EDGE_DEFAULT_RESPAWN},
    {kConfigBoolean, "fast_monsters", &global_flags.fast_monsters, EDGE_DEFAULT_FASTPARM},
    {kConfigBoolean, "true_3d_gameplay", &global_flags.true_3d_gameplay, EDGE_DEFAULT_TRUE3DGAMEPLAY},
    {kConfigEnum, "autoaim", &global_flags.autoaim, EDGE_DEFAULT_AUTOAIM},
    {kConfigBoolean, "shootthru_scenery", &global_flags.pass_missile, EDGE_DEFAULT_PASS_MISSILE},
    {kConfigInteger, "swirling_flats", &swirling_flats, 0},

    {kConfigBoolean, "pistol_starts", &pistol_starts, 0},
    {kConfigBoolean, "automap_keydoor_blink", &automap_keydoor_blink, EDGE_DEFAULT_AM_KEYDOORBLINK},

    // -KM- 1998/07/21 Save the blood setting
    {kConfigBoolean, "blood", &global_flags.more_blood, EDGE_DEFAULT_MORE_BLOOD},
    {kConfigBoolean, "extra", &global_flags.have_extra, EDGE_DEFAULT_HAVE_EXTRA},
    {kConfigBoolean, "weaponkick", &global_flags.kicking, EDGE_DEFAULT_KICKING},
    {kConfigBoolean, "weaponswitch", &global_flags.weapon_switch, EDGE_DEFAULT_WEAPON_SWITCH},
    {kConfigBoolean, "mlook", &global_flags.mouselook, EDGE_DEFAULT_MLOOK},
    {kConfigBoolean, "jumping", &global_flags.jump, EDGE_DEFAULT_JUMP},
    {kConfigBoolean, "crouching", &global_flags.crouch, EDGE_DEFAULT_CROUCH},
    {kConfigInteger, "smoothing", &image_smoothing, EDGE_DEFAULT_USE_SMOOTHING},
    {kConfigInteger, "mipmapping", &image_mipmapping, EDGE_DEFAULT_USE_MIPMAPPING},
    {kConfigInteger, "dlights", &use_dynamic_lights, EDGE_DEFAULT_USE_DLIGHTS},
    {kConfigInteger, "detail_level", &detail_level, EDGE_DEFAULT_DETAIL_LEVEL},
    {kConfigInteger, "hq2x_scaling", &hq2x_scaling, EDGE_DEFAULT_HQ2X_SCALING},

    // -KM- 1998/09/01 Useless mouse/joy stuff removed,
    //                 analogue binding added
    {kConfigInteger, "mouse_axis_x", &mouse_x_axis, EDGE_DEFAULT_MOUSE_XAXIS},
    {kConfigInteger, "mouse_axis_y", &mouse_y_axis, EDGE_DEFAULT_MOUSE_YAXIS},

    {kConfigInteger, "joystick_axis1", &joystick_axis[0], 7},
    {kConfigInteger, "joystick_axis2", &joystick_axis[1], 6},
    {kConfigInteger, "joystick_axis3", &joystick_axis[2], 1},
    {kConfigInteger, "joystick_axis4", &joystick_axis[3], 4},

    {kConfigInteger, "screen_hud", &screen_hud, EDGE_DEFAULT_SCREEN_HUD},
    {kConfigInteger, "save_page", &save_page, 0},

    // -------------------- VARS --------------------

    {kConfigBoolean, "show_obituaries", &show_obituaries, 1},

    // -------------------- KEYS --------------------

    {kConfigKey, "key_right", &key_right, EDGE_DEFAULT_KEY_RIGHT},
    {kConfigKey, "key_left", &key_left, EDGE_DEFAULT_KEY_LEFT},
    {kConfigKey, "key_up", &key_up, EDGE_DEFAULT_KEY_UP},
    {kConfigKey, "key_down", &key_down, EDGE_DEFAULT_KEY_DOWN},
    {kConfigKey, "key_look_up", &key_look_up, EDGE_DEFAULT_KEY_LOOKUP},
    {kConfigKey, "key_look_down", &key_look_down, EDGE_DEFAULT_KEY_LOOKDOWN},
    {kConfigKey, "key_look_center", &key_look_center, EDGE_DEFAULT_KEY_LOOKCENTER},

    // -ES- 1999/03/28 Zoom Key
    {kConfigKey, "key_zoom", &key_zoom, EDGE_DEFAULT_KEY_ZOOM},
    {kConfigKey, "key_strafe_left", &key_strafe_left, EDGE_DEFAULT_KEY_STRAFELEFT},
    {kConfigKey, "key_strafe_right", &key_strafe_right, EDGE_DEFAULT_KEY_STRAFERIGHT},

    // -ACB- for -MH- 1998/07/02 Flying Keys
    {kConfigKey, "key_fly_up", &key_fly_up, EDGE_DEFAULT_KEY_FLYUP},
    {kConfigKey, "key_fly_down", &key_fly_down, EDGE_DEFAULT_KEY_FLYDOWN},

    {kConfigKey, "key_fire", &key_fire, EDGE_DEFAULT_KEY_FIRE},
    {kConfigKey, "key_use", &key_use, EDGE_DEFAULT_KEY_USE},
    {kConfigKey, "key_strafe", &key_strafe, EDGE_DEFAULT_KEY_STRAFE},
    {kConfigKey, "key_speed", &key_speed, EDGE_DEFAULT_KEY_SPEED},
    {kConfigKey, "key_autorun", &key_autorun, EDGE_DEFAULT_KEY_AUTORUN},
    {kConfigKey, "key_next_weapon", &key_next_weapon, EDGE_DEFAULT_KEY_NEXTWEAPON},
    {kConfigKey, "key_previous_weapon", &key_previous_weapon, EDGE_DEFAULT_KEY_PREVWEAPON},

    {kConfigKey, "key_180", &key_180, EDGE_DEFAULT_KEY_180},
    {kConfigKey, "key_map", &key_map, EDGE_DEFAULT_KEY_MAP},
    {kConfigKey, "key_talk", &key_talk, EDGE_DEFAULT_KEY_TALK},
    {kConfigKey, "key_console", &key_console, EDGE_DEFAULT_KEY_CONSOLE},               // -AJA- 2007/08/15.
    {kConfigKey, "key_pause", &key_pause, kPause},                                     // -AJA- 2010/06/13.

    {kConfigKey, "key_mouselook", &key_mouselook, EDGE_DEFAULT_KEY_MLOOK},             // -AJA- 1999/07/27.
    {kConfigKey, "key_second_attack", &key_second_attack, EDGE_DEFAULT_KEY_SECONDATK}, // -AJA- 2000/02/08.
    {kConfigKey, "key_third_attack", &key_third_attack, 0},                            //
    {kConfigKey, "key_fourth_attack", &key_fourth_attack, 0},                          //
    {kConfigKey, "key_reload", &key_reload, EDGE_DEFAULT_KEY_RELOAD},                  // -AJA- 2004/11/11.
    {kConfigKey, "key_action1", &key_action1, EDGE_DEFAULT_KEY_ACTION1},               // -AJA- 2009/09/07
    {kConfigKey, "key_action2", &key_action2, EDGE_DEFAULT_KEY_ACTION2},               // -AJA- 2009/09/07

    // -AJA- 2010/06/13: weapon and automap keys
    {kConfigKey, "key_weapon1", &key_weapons[1], '1'},
    {kConfigKey, "key_weapon2", &key_weapons[2], '2'},
    {kConfigKey, "key_weapon3", &key_weapons[3], '3'},
    {kConfigKey, "key_weapon4", &key_weapons[4], '4'},
    {kConfigKey, "key_weapon5", &key_weapons[5], '5'},
    {kConfigKey, "key_weapon6", &key_weapons[6], '6'},
    {kConfigKey, "key_weapon7", &key_weapons[7], '7'},
    {kConfigKey, "key_weapon8", &key_weapons[8], '8'},
    {kConfigKey, "key_weapon9", &key_weapons[9], '9'},
    {kConfigKey, "key_weapon0", &key_weapons[0], '0'},

    {kConfigKey, "key_automap_up", &key_automap_up, kUpArrow},
    {kConfigKey, "key_automap_down", &key_automap_down, kDownArrow},
    {kConfigKey, "key_automap_left", &key_automap_left, kLeftArrow},
    {kConfigKey, "key_automap_right", &key_automap_right, kRightArrow},
    {kConfigKey, "key_automap_zoom_in", &key_automap_zoom_in, '='},
    {kConfigKey, "key_automap_zoom_out", &key_automap_zoom_out, '-'},
    {kConfigKey, "key_automap_follow", &key_automap_follow, 'f'},
    {kConfigKey, "key_automap_grid", &key_automap_grid, 'g'},
    {kConfigKey, "key_automap_mark", &key_automap_mark, 'm'},
    {kConfigKey, "key_automap_clear", &key_automap_clear, 'c'},

    {kConfigKey, "key_inventory_previous", &key_inventory_previous, EDGE_DEFAULT_KEY_PREVINV},
    {kConfigKey, "key_inventory_use", &key_inventory_use, EDGE_DEFAULT_KEY_USEINV},
    {kConfigKey, "key_inventory_next", &key_inventory_next, EDGE_DEFAULT_KEY_NEXTINV},

    {kConfigKey, "key_screenshot", &key_screenshot, kFunction1},
    {kConfigKey, "key_save_game", &key_save_game, kFunction2},
    {kConfigKey, "key_load_game", &key_load_game, kFunction3},
    {kConfigKey, "key_sound_controls", &key_sound_controls, kFunction4},
    {kConfigKey, "key_options_menu", &key_options_menu, kFunction5},
    {kConfigKey, "key_quick_save", &key_quick_save, kFunction6},
    {kConfigKey, "key_end_game", &key_end_game, kFunction7},
    {kConfigKey, "key_message_toggle", &key_message_toggle, kFunction8},
    {kConfigKey, "key_quick_load", &key_quick_load, kFunction9},
    {kConfigKey, "key_quit_edge", &key_quit_edge, kFunction10},
    {kConfigKey, "key_gamma_toggle", &key_gamma_toggle, kFunction11},
    {kConfigKey, "key_show_players", &key_show_players, kFunction12},
};

static int total_defaults = sizeof(defaults) / sizeof(defaults[0]);

void SaveDefaults(void)
{
    // -ACB- 1999/09/24 idiot proof checking as required by MSVC
    EPI_ASSERT(!configuration_file.empty());

    FILE *f = epi::FileOpenRaw(configuration_file, epi::kFileAccessWrite | epi::kFileAccessBinary);

    if (!f)
    {
        LogWarning("Couldn't open config file %s for writing.", configuration_file.c_str());
        return; // can't write the file, but don't complain
    }

    fprintf(f, "#VERSION %d\n", kInternalConfigVersion);

    // console variables
    WriteConsoleVariables(f);

    // normal variables
    for (int i = 0; i < total_defaults; i++)
    {
        int v;

        switch (defaults[i].type)
        {
        case kConfigInteger:
            fprintf(f, "%s\t\t%i\n", defaults[i].name, *(int *)defaults[i].location);
            break;

        case kConfigBoolean:
            fprintf(f, "%s\t\t%i\n", defaults[i].name, *(bool *)defaults[i].location ? 1 : 0);
            break;

        case kConfigKey:
            v = *(int *)defaults[i].location;
            fprintf(f, "%s\t\t0x%X\n", defaults[i].name, v);
            break;
        }
    }

    epi::SyncFilesystem();

    fclose(f);
}

static void SetToBaseValue(ConfigurationDefault *def)
{
    switch (def->type)
    {
    case kConfigInteger:
    case kConfigKey:
        *(int *)(def->location) = def->default_value;
        break;

    case kConfigBoolean:
        *(bool *)(def->location) = def->default_value ? true : false;
        break;
    }
}

void ResetDefaults(int dummy, ConsoleVariable *dummy_cvar)
{
    EPI_UNUSED(dummy);
    EPI_UNUSED(dummy_cvar);
    for (int i = 0; i < total_defaults; i++)
    {
        // don't reset the first five entries except at startup
        if (done_first_init && i < 5)
            continue;

        SetToBaseValue(defaults + i);
    }

    ResetAllConsoleVariables();

    // Needed so that Smoothing/Upscaling is properly reset
    DeleteAllImages();

    done_first_init = true;
}

static void ParseConfig(const std::string &data, bool check_config_version)
{
    epi::Scanner lex(data);

    // Check the first line of a config file for the #VERSION entry. If not
    // present, assume it is from a version that predates this concept
    if (check_config_version)
    {
        if (!lex.GetNextToken() || lex.state_.token != '#')
        {
            show_old_config_warning = true;
        }

        if (!lex.GetNextToken() || lex.state_.token != epi::Scanner::kIdentifier || lex.state_.string != "VERSION")
        {
            show_old_config_warning = true;
        }

        if (!lex.GetNextToken() || lex.state_.token != epi::Scanner::kIntConst ||
            lex.state_.number < kInternalConfigVersion)
        {
            show_old_config_warning = true;
        }
    }

    while (lex.TokensLeft())
    {
        std::string key;
        std::string value;

        if (!lex.GetNextToken())
            FatalError("ParseConfig: error parsing file!\n");

        // Discard leading / for cvars
        // Todo: Convert everything to CVARs and then get
        // rid of the leading slash
        if (lex.state_.token == '/')
        {
            if (!lex.GetNextToken())
                FatalError("ParseConfig: error parsing file!\n");
        }

        key = lex.state_.string;

        if (!lex.GetNextToken())
            FatalError("ParseConfig: missing value for key %s!\n", key.c_str());

        value = lex.state_.string;

        if (lex.state_.token == epi::Scanner::kStringConst)
        {
            std::string try_cvar = key;
            try_cvar.append(" ").append(value);
            TryConsoleCommand(try_cvar.c_str());
        }
        else if (lex.state_.token == epi::Scanner::kIntConst)
        {
            for (int i = 0; i < total_defaults; i++)
            {
                if (0 == epi::StringCompare(key.c_str(), defaults[i].name))
                {
                    if (defaults[i].type == kConfigBoolean)
                    {
                        *(bool *)defaults[i].location = lex.state_.boolean ? true : false;
                    }
                    else /* kConfigInteger and
                            kConfigKey */
                    {
                        *(int *)defaults[i].location = lex.state_.number;
                    }
                    break;
                }
            }
        }
    }
}

void LoadDefaults(void)
{
    // set everything to base values
    ResetDefaults(0);

    LogPrint("LoadDefaults from %s\n", configuration_file.c_str());

    epi::File *file = epi::FileOpen(configuration_file, epi::kFileAccessRead);

    if (!file)
    {
        LogWarning("Couldn't open config file %s for reading.\n", configuration_file.c_str());
        LogWarning("Resetting config to RECOMMENDED values...\n");
        return;
    }
    // load the file into this string
    std::string data = file->ReadText();

    delete file;

    ParseConfig(data, true);

    return;
}

void LoadBranding(void)
{
    epi::File *file = FileOpen(branding_file, epi::kFileAccessRead);

    // Just use hardcoded values if no branding file present
    if (!file)
        return;

    // load the file into this string
    std::string data = file->ReadText();

    delete file;

    ParseConfig(data, false);
}

void TakeScreenshot(bool show_msg)
{
    std::string fn;

    // find a file name to save it to
    for (int i = 1; i <= 9999; i++)
    {
        std::string base(epi::StringFormat("shot%02d.png", i));

        fn = epi::PathAppend(screenshot_directory, base);

        if (!epi::TestFileAccess(fn))
        {
            break; // file doesn't exist
        }
    }

    ImageData *img = new ImageData(current_screen_width, current_screen_height, 4);

    render_backend->CaptureScreen(current_screen_width, current_screen_height, current_screen_width * 4,
                                  img->PixelAt(0, 0));

    // ReadScreen produces a bottom-up image, need to invert it
    img->Invert();

    bool result = SavePNG(fn, img);

    if (show_msg)
    {
        if (result)
            LogPrint("Captured to file: %s\n", fn.c_str());
        else
            LogPrint("Error saving file: %s\n", fn.c_str());
    }

    delete img;
}

void CreateSaveScreenshot(void)
{
    render_backend->OnFrameFinished([]() -> void {
        std::string temp(epi::StringFormat("%s/%s.png", "current", "head"));
        std::string filename = epi::PathAppend(save_directory, temp);

        epi::FileDelete(filename);

        ImageData *img = new ImageData(current_screen_width, current_screen_height, 4);

        render_backend->CaptureScreen(current_screen_width, current_screen_height, current_screen_width * 4,
                                      img->PixelAt(0, 0));

        // ReadScreen produces a bottom-up image, need to invert it
        img->Invert();

        bool result = SavePNG(filename, img);

        if (result)
            LogPrint("Captured to file: %s\n", filename.c_str());
        else
            LogPrint("Error saving file: %s\n", filename.c_str());

        delete img;

        epi::ReplaceExtension(filename, ".replace");

        epi::File *replace_touch = epi::FileOpen(filename, epi::kFileAccessWrite);

        delete replace_touch;
    });
}

void WarningOrError(const char *error, ...)
{
    // Either displays a warning or produces a fatal error, depending
    // on whether the "-strict" option is used.

    char message_buf[4096];

    message_buf[4095] = 0;

    va_list argptr;

    va_start(argptr, error);
    stbsp_vsprintf(message_buf, error, argptr);
    va_end(argptr);

    // I hope nobody is printing strings longer than 4096 chars...
    EPI_ASSERT(message_buf[4095] == 0);

    if (strict_errors)
        FatalError("%s", message_buf);
    else if (!no_warnings)
        LogWarning("%s", message_buf);
}

void DebugOrError(const char *error, ...)
{
    // Either writes a debug message or produces a fatal error, depending
    // on whether the "-strict" option is used.

    char message_buf[4096];

    message_buf[4095] = 0;

    va_list argptr;

    va_start(argptr, error);
    stbsp_vsprintf(message_buf, error, argptr);
    va_end(argptr);

    // I hope nobody is printing strings longer than 4096 chars...
    EPI_ASSERT(message_buf[4095] == 0);

    if (strict_errors)
        FatalError("%s", message_buf);
    else if (!no_warnings)
        LogDebug("%s", message_buf);
}

extern FILE *debug_file; // FIXME

void LogDebug(const char *message, ...)
{
    // Write into the debug file.
    //
    // -ACB- 1999/09/22: From #define to Procedure
    // -AJA- 2001/02/07: Moved here from platform codes.
    //
    if (!debug_file)
        return;

    char message_buf[4096];

    message_buf[4095] = 0;

    // Print the message into a text string
    va_list argptr;

    va_start(argptr, message);
    stbsp_vsprintf(message_buf, message, argptr);
    va_end(argptr);

    // I hope nobody is printing strings longer than 4096 chars...
    EPI_ASSERT(message_buf[4095] == 0);

    fprintf(debug_file, "%s", message_buf);
    fflush(debug_file);
}

extern FILE *log_file; // FIXME: make file_c and unify with debug_file

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
