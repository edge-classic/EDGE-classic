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
#include "endianess.h"
#include "epi.h"
#include "file.h"
#include "filesystem.h"
#include "g_game.h"
#include "hu_draw.h"
#include "hu_stuff.h"  // only for show_messages
#include "im_data.h"
#include "im_funcs.h"
#include "m_argv.h"
#include "m_menu.h"
#include "m_option.h"
#include "n_network.h"
#include "p_spec.h"
#include "r_colormap.h"
#include "r_draw.h"
#include "r_gldefs.h"
#include "r_image.h"
#include "r_modes.h"
#include "r_wipe.h"
#include "s_blit.h"
#include "s_sound.h"
#include "str_compare.h"
#include "str_lexer.h"
#include "str_util.h"
#include "version.h"
//
// DEFAULTS
//
bool save_screenshot_valid = false;

extern ConsoleVariable midi_soundfont;
extern bool            pc_speaker_mode;
int                    var_midi_player  = 0;
int                    var_sound_stereo = 0;
int                    sound_mixing_channels = 0;

static bool done_first_init = false;

static ConfigurationDefault defaults[] = {
    {kConfigInteger, "screenwidth", &current_screen_width, CFGDEF_SCREENWIDTH},
    {kConfigInteger, "screenheight", &current_screen_height, CFGDEF_SCREENHEIGHT},
    {kConfigInteger, "screendepth", &current_screen_depth, CFGDEF_SCREENBITS},
    {kConfigInteger, "displaymode", &current_window_mode, CFGDEF_DISPLAYMODE},

    {kConfigInteger, "sound_stereo", &var_sound_stereo, CFGDEF_SOUND_STEREO},
    {kConfigBoolean, "pc_speaker_mode", &pc_speaker_mode, 0},
    {kConfigInteger, "midi_player", &var_midi_player, 0},
    {kConfigBoolean, "dynamic_reverb", &dynamic_reverb, 0},
    {kConfigInteger, "mix_channels", &sound_mixing_channels, CFGDEF_MIX_CHANNELS},

    {kConfigInteger, "show_messages", &show_messages, CFGDEF_SHOWMESSAGES},

    // -ES- 1998/11/28 Save fade settings
    {kConfigInteger, "reduce_flash", &reduce_flash, 0},
    {kConfigInteger, "invuln_fx", &invulnerability_effect, CFGDEF_INVUL_FX},
    {kConfigEnum, "wipe_method", &wipe_method, CFGDEF_WIPE_METHOD},
    {kConfigBoolean, "rotate_map", &rotate_map, CFGDEF_ROTATEMAP},
    {kConfigBoolean, "respawnsetting", &global_flags.enemy_respawn_mode,
     CFGDEF_RES_RESPAWN},
    {kConfigBoolean, "items_respawn", &global_flags.items_respawn,
     CFGDEF_ITEMRESPAWN},
    {kConfigBoolean, "respawn", &global_flags.enemies_respawn, CFGDEF_RESPAWN},
    {kConfigBoolean, "fast_monsters", &global_flags.fast_monsters, CFGDEF_FASTPARM},
    {kConfigBoolean, "true_3d_gameplay", &global_flags.true_3d_gameplay,
     CFGDEF_TRUE3DGAMEPLAY},
    {kConfigEnum, "autoaim", &global_flags.autoaim, CFGDEF_AUTOAIM},
    {kConfigBoolean, "shootthru_scenery", &global_flags.pass_missile,
     CFGDEF_PASS_MISSILE},
    {kConfigInteger, "swirling_flats", &swirling_flats, 0},

    {kConfigBoolean, "pistol_starts", &pistol_starts, 0},
    {kConfigBoolean, "automap_keydoor_blink", &automap_keydoor_blink,
     CFGDEF_AM_KEYDOORBLINK},

    // -KM- 1998/07/21 Save the blood setting
    {kConfigBoolean, "blood", &global_flags.more_blood, CFGDEF_MORE_BLOOD},
    {kConfigBoolean, "extra", &global_flags.have_extra, CFGDEF_HAVE_EXTRA},
    {kConfigBoolean, "weaponkick", &global_flags.kicking, CFGDEF_KICKING},
    {kConfigBoolean, "weaponswitch", &global_flags.weapon_switch,
     CFGDEF_WEAPON_SWITCH},
    {kConfigBoolean, "mlook", &global_flags.mouselook, CFGDEF_MLOOK},
    {kConfigBoolean, "jumping", &global_flags.jump, CFGDEF_JUMP},
    {kConfigBoolean, "crouching", &global_flags.crouch, CFGDEF_CROUCH},
    {kConfigInteger, "smoothing", &image_smoothing, CFGDEF_USE_SMOOTHING},
    {kConfigInteger, "dlights", &use_dynamic_lights, CFGDEF_USE_DLIGHTS},
    {kConfigInteger, "detail_level", &detail_level, CFGDEF_DETAIL_LEVEL},
    {kConfigInteger, "hq2x_scaling", &hq2x_scaling, CFGDEF_HQ2X_SCALING},

    // -KM- 1998/09/01 Useless mouse/joy stuff removed,
    //                 analogue binding added
    {kConfigInteger, "mouse_axis_x", &mouse_x_axis, CFGDEF_MOUSE_XAXIS},
    {kConfigInteger, "mouse_axis_y", &mouse_y_axis, CFGDEF_MOUSE_YAXIS},

    {kConfigInteger, "joystick_axis1", &joystick_axis[0], 7},
    {kConfigInteger, "joystick_axis2", &joystick_axis[1], 6},
    {kConfigInteger, "joystick_axis3", &joystick_axis[2], 1},
    {kConfigInteger, "joystick_axis4", &joystick_axis[3], 4},

    {kConfigInteger, "screen_hud", &screen_hud, CFGDEF_SCREEN_HUD},
    {kConfigInteger, "save_page", &save_page, 0},
    {kConfigBoolean, "png_screenshots", &png_screenshots, CFGDEF_PNG_SCRSHOTS},

    // -------------------- VARS --------------------

    {kConfigBoolean, "show_obituaries", &show_obituaries, 1},
    {kConfigBoolean, "precache_sound_effects", &precache_sound_effects, 1},

    // -------------------- KEYS --------------------

    {kConfigKey, "key_right", &key_right, CFGDEF_KEY_RIGHT},
    {kConfigKey, "key_left", &key_left, CFGDEF_KEY_LEFT},
    {kConfigKey, "key_up", &key_up, CFGDEF_KEY_UP},
    {kConfigKey, "key_down", &key_down, CFGDEF_KEY_DOWN},
    {kConfigKey, "key_look_up", &key_look_up, CFGDEF_KEY_LOOKUP},
    {kConfigKey, "key_look_down", &key_look_down, CFGDEF_KEY_LOOKDOWN},
    {kConfigKey, "key_look_center", &key_look_center, CFGDEF_KEY_LOOKCENTER},

    // -ES- 1999/03/28 Zoom Key
    {kConfigKey, "key_zoom", &key_zoom, CFGDEF_KEY_ZOOM},
    {kConfigKey, "key_strafe_left", &key_strafe_left, CFGDEF_KEY_STRAFELEFT},
    {kConfigKey, "key_strafe_right", &key_strafe_right, CFGDEF_KEY_STRAFERIGHT},

    // -ACB- for -MH- 1998/07/02 Flying Keys
    {kConfigKey, "key_fly_up", &key_fly_up, CFGDEF_KEY_FLYUP},
    {kConfigKey, "key_fly_down", &key_fly_down, CFGDEF_KEY_FLYDOWN},

    {kConfigKey, "key_fire", &key_fire, CFGDEF_KEY_FIRE},
    {kConfigKey, "key_use", &key_use, CFGDEF_KEY_USE},
    {kConfigKey, "key_strafe", &key_strafe, CFGDEF_KEY_STRAFE},
    {kConfigKey, "key_speed", &key_speed, CFGDEF_KEY_SPEED},
    {kConfigKey, "key_autorun", &key_autorun, CFGDEF_KEY_AUTORUN},
    {kConfigKey, "key_next_weapon", &key_next_weapon, CFGDEF_KEY_NEXTWEAPON},
    {kConfigKey, "key_previous_weapon", &key_previous_weapon,
     CFGDEF_KEY_PREVWEAPON},

    {kConfigKey, "key_180", &key_180, CFGDEF_KEY_180},
    {kConfigKey, "key_map", &key_map, CFGDEF_KEY_MAP},
    {kConfigKey, "key_talk", &key_talk, CFGDEF_KEY_TALK},
    {kConfigKey, "key_console", &key_console,
     CFGDEF_KEY_CONSOLE},                               // -AJA- 2007/08/15.
    {kConfigKey, "key_pause", &key_pause, kPause},  // -AJA- 2010/06/13.

    {kConfigKey, "key_mouselook", &key_mouselook,
     CFGDEF_KEY_MLOOK},  // -AJA- 1999/07/27.
    {kConfigKey, "key_second_attack", &key_second_attack,
     CFGDEF_KEY_SECONDATK},  // -AJA- 2000/02/08.
    {kConfigKey, "key_third_attack", &key_third_attack, 0},    //
    {kConfigKey, "key_fourth_attack", &key_fourth_attack, 0},  //
    {kConfigKey, "key_reload", &key_reload,
     CFGDEF_KEY_RELOAD},  // -AJA- 2004/11/11.
    {kConfigKey, "key_action1", &key_action1,
     CFGDEF_KEY_ACTION1},  // -AJA- 2009/09/07
    {kConfigKey, "key_action2", &key_action2,
     CFGDEF_KEY_ACTION2},  // -AJA- 2009/09/07

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

    {kConfigKey, "key_inventory_previous", &key_inventory_previous,
     CFGDEF_KEY_PREVINV},
    {kConfigKey, "key_inventory_use", &key_inventory_use, CFGDEF_KEY_USEINV},
    {kConfigKey, "key_inventory_next", &key_inventory_next, CFGDEF_KEY_NEXTINV},

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

void ConfigurationSaveDefaults(void)
{
    // -ACB- 1999/09/24 idiot proof checking as required by MSVC
    EPI_ASSERT(!configuration_file.empty());

    FILE *f = epi::FileOpenRaw(configuration_file,
                               epi::kFileAccessWrite | epi::kFileAccessBinary);
    if (!f)
    {
        LogWarning("Couldn't open config file %s for writing.",
                   configuration_file.c_str());
        return;  // can't write the file, but don't complain
    }

    // console variables
    ConsoleWriteVariables(f);

    // normal variables
    for (int i = 0; i < total_defaults; i++)
    {
        int v;

        switch (defaults[i].type)
        {
            case kConfigInteger:
                fprintf(f, "%s\t\t%i\n", defaults[i].name,
                        *(int *)defaults[i].location);
                break;

            case kConfigBoolean:
                fprintf(f, "%s\t\t%i\n", defaults[i].name,
                        *(bool *)defaults[i].location ? 1 : 0);
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

void ConfigurationResetDefaults(int dummy, ConsoleVariable *dummy_cvar)
{
    (void)dummy;
    (void)dummy_cvar;
    for (int i = 0; i < total_defaults; i++)
    {
        // don't reset the first five entries except at startup
        if (done_first_init && i < 5) continue;

        SetToBaseValue(defaults + i);
    }

    ConsoleResetAllVariables();

    // Set default SF2 location in midi_soundfont CVAR
    // We can't store this as a CVAR default since it is path-dependent
    midi_soundfont = epi::SanitizePath(
        epi::PathAppend(game_directory, "soundfont/Default.sf2"));

    // Needed so that Smoothing/Upscaling is properly reset
    DeleteAllImages();

    done_first_init = true;
}

static void ParseConfigBlock(epi::Lexer &lex)
{
    for (;;)
    {
        std::string key;
        std::string value;

        epi::TokenKind tok = lex.Next(key);

        if (key ==
            "/")  // CVAR keys will start with this, but we need to discard it
            continue;

        if (tok == epi::kTokenEOF) return;

        if (tok == epi::kTokenError)
            FatalError("ParseConfig: error parsing file!\n");

        tok = lex.Next(value);

        // The last line of the config writer causes a weird blank key with an
        // EOF value, so just return here
        if (tok == epi::kTokenEOF) return;

        if (tok == epi::kTokenError)
            FatalError("ParseConfig: malformed value for key %s!\n",
                       key.c_str());

        if (tok == epi::kTokenString)
        {
            std::string try_cvar = key;
            try_cvar.append(" ").append(value);
            ConsoleTryCommand(try_cvar.c_str());
        }
        else if (tok == epi::kTokenNumber)
        {
            for (int i = 0; i < total_defaults; i++)
            {
                if (0 == epi::StringCompare(key.c_str(), defaults[i].name))
                {
                    if (defaults[i].type == kConfigBoolean)
                    {
                        *(bool *)defaults[i].location =
                            epi::LexInteger(value) ? true : false;
                    }
                    else /* kConfigInteger and
                            kConfigKey */
                    {
                        *(int *)defaults[i].location = epi::LexInteger(value);
                    }
                    break;
                }
            }
        }
    }
}

static void ParseConfig(const std::string &data)
{
    epi::Lexer lex(data);

    for (;;)
    {
        std::string    section;
        epi::TokenKind tok = lex.Next(section);

        if (tok == epi::kTokenEOF) return;

        // process the block
        ParseConfigBlock(lex);
    }
}

void ConfigurationLoadDefaults(void)
{
    // set everything to base values
    ConfigurationResetDefaults(0);

    LogPrint("ConfigurationLoadDefaults from %s\n", configuration_file.c_str());

    epi::File *file = epi::FileOpen(configuration_file, epi::kFileAccessRead);

    if (!file)
    {
        LogWarning("Couldn't open config file %s for reading.\n",
                   configuration_file.c_str());
        LogWarning("Resetting config to RECOMMENDED values...\n");
        return;
    }
    // load the file into this string
    std::string data = file->ReadText();

    delete file;

    ParseConfig(data);

    return;
}

void ConfigurationLoadBranding(void)
{
    epi::File *file = FileOpen(branding_file, epi::kFileAccessRead);

    // Just use hardcoded values if no branding file present
    if (!file) return;

    // load the file into this string
    std::string data = file->ReadText();

    delete file;

    ParseConfig(data);
}

#define PIXEL_RED(pix) (playpal_data[0][pix][0])
#define PIXEL_GRN(pix) (playpal_data[0][pix][1])
#define PIXEL_BLU(pix) (playpal_data[0][pix][2])

void TakeScreenshot(bool show_msg)
{
    const char *extension;

    if (png_screenshots)
        extension = "png";
    else
        extension = "jpg";

    std::string fn;

    // find a file name to save it to
    for (int i = 1; i <= 9999; i++)
    {
        std::string base(epi::StringFormat("shot%02d.%s", i, extension));

        fn = epi::PathAppend(screenshot_directory, base);

        if (!epi::TestFileAccess(fn))
        {
            break;  // file doesn't exist
        }
    }

    ImageData *img = new ImageData(current_screen_width, current_screen_height, 3);

    RendererReadScreen(0, 0, current_screen_width, current_screen_height, img->PixelAt(0, 0));

    // ReadScreen produces a bottom-up image, need to invert it
    img->Invert();

    bool result;

    if (png_screenshots) { result = ImageSavePng(fn, img); }
    else { result = ImageSaveJpeg(fn, img); }

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
    const char *extension = "jpg";

    std::string temp(
        epi::StringFormat("%s/%s.%s", "current", "head", extension));
    std::string filename = epi::PathAppend(save_directory, temp);

    epi::FileDelete(filename);

    ImageData *img = new ImageData(current_screen_width, current_screen_height, 3);

    RendererReadScreen(0, 0, current_screen_width, current_screen_height, img->PixelAt(0, 0));

    // ReadScreen produces a bottom-up image, need to invert it
    img->Invert();

    bool result;
    result = ImageSaveJpeg(filename, img);

    if (result)
        LogPrint("Captured to file: %s\n", filename.c_str());
    else
        LogPrint("Error saving file: %s\n", filename.c_str());

    delete img;

    epi::ReplaceExtension(filename, ".replace");

    epi::File *replace_touch = epi::FileOpen(filename, epi::kFileAccessWrite);

    delete replace_touch;
}

void PrintWarningOrError(const char *error, ...)
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
    EPI_ASSERT(message_buf[4095] == 0);

    if (strict_errors)
        FatalError("%s", message_buf);
    else if (!no_warnings)
        LogWarning("%s", message_buf);
}

void PrintDebugOrError(const char *error, ...)
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
    EPI_ASSERT(message_buf[4095] == 0);

    if (strict_errors)
        FatalError("%s", message_buf);
    else if (!no_warnings)
        LogDebug("%s", message_buf);
}

extern FILE *debug_file;  // FIXME

void LogDebug(const char *message, ...)
{
    // Write into the debug file.
    //
    // -ACB- 1999/09/22: From #define to Procedure
    // -AJA- 2001/02/07: Moved here from platform codes.
    //
    if (!debug_file) return;

    char message_buf[4096];

    message_buf[4095] = 0;

    // Print the message into a text string
    va_list argptr;

    va_start(argptr, message);
    vsprintf(message_buf, message, argptr);
    va_end(argptr);

    // I hope nobody is printing strings longer than 4096 chars...
    EPI_ASSERT(message_buf[4095] == 0);

    fprintf(debug_file, "%s", message_buf);
    fflush(debug_file);
}

extern FILE *log_file;  // FIXME: make file_c and unify with debug_file

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
