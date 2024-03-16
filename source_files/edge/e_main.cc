//---------------------------------------------------------------------------
//  EDGE Main Init + Program Loop Code
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
// DESCRIPTION:
//      EDGE main program (E_Main),
//      game loop (E_Loop) and startup functions.
//
// -MH- 1998/07/02 "shootupdown" --> "true_3d_gameplay"
// -MH- 1998/08/19 added up/down movement variables
//

#include "e_main.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#include <algorithm>
#include <vector>

#include "am_map.h"
#include "con_gui.h"
#include "con_main.h"
#include "con_var.h"
#include "dm_defs.h"
#include "dm_state.h"
#include "dstrings.h"
#include "e_input.h"
#include "edge_profiling.h"
#include "epi_sdl.h"
#include "f_finale.h"
#include "f_interm.h"
#include "file.h"
#include "filesystem.h"
#include "g_game.h"
#include "hu_draw.h"
#include "hu_stuff.h"
#include "i_defs_gl.h"
#include "i_movie.h"
#include "i_system.h"
#include "m_argv.h"
#include "m_bbox.h"
#include "m_cheat.h"
#include "m_menu.h"
#include "m_misc.h"
#include "m_random.h"
#include "n_network.h"
#include "p_setup.h"
#include "p_spec.h"
#include "r_colormap.h"
#include "r_draw.h"
#include "r_gldefs.h"
#include "r_image.h"
#include "r_misc.h"
#include "r_modes.h"
#include "r_wipe.h"
#include "rad_trig.h"
#include "s_music.h"
#include "s_sound.h"
#include "script/compat/lua_compat.h"
#include "str_compare.h"
#include "str_util.h"
#include "sv_chunk.h"
#include "sv_main.h"
#include "version.h"
#include "vm_coal.h"
#include "w_files.h"
#include "w_model.h"
#include "w_sprite.h"
#include "w_texture.h"
#include "w_wad.h"

extern ConsoleVariable double_framerate;
extern ConsoleVariable busy_wait;

extern ConsoleVariable gamma_correction;

ECFrameStats ec_frame_stats;

// Application active?
int app_state = kApplicationActive;

bool single_tics = false; // debug flag to cancel adaptiveness

// -ES- 2000/02/13 Takes screenshot every screenshot_rate tics.
// Must be used in conjunction with single_tics.
static int screenshot_rate;

// For screenies...
bool m_screenshot_required = false;
bool need_save_screenshot  = false;

bool custom_MenuMain       = false;
bool custom_MenuEpisode    = false;
bool custom_MenuDifficulty = false;

FILE *log_file   = nullptr;
FILE *debug_file = nullptr;

GameFlags default_game_flags = {
    false,      // nomonsters
    false,      // fast_monsters

    false,      // respawn
    false,      // enemy_respawn_mode
    false,      // item respawn

    false,      // true 3d gameplay
    8,          // gravity
    false,      // more blood

    true,       // jump
    true,       // crouch
    true,       // mlook
    kAutoAimOn, // autoaim

    true,       // cheats
    true,       // have_extra
    false,      // limit_zoom

    true,       // kicking
    true,       // weapon_switch
    true,       // pass_missile
    false,      // team_damage
};

// -KM- 1998/12/16 These flags are the users prefs and are copied to
//   gameflags when a new level is started.
// -AJA- 2000/02/02: Removed initialisation (done in code using
//       `default_game_flags').

GameFlags global_flags;

bool mus_pause_stop  = false;
bool png_screenshots = false;

std::string branding_file;
std::string configuration_file;
std::string epkfile;
std::string game_base;

std::string cache_directory;
std::string game_directory;
std::string home_directory;
std::string save_directory;
std::string screenshot_directory;

// not using EDGE_DEFINE_CONSOLE_VARIABLE here since var name != cvar name
ConsoleVariable m_language("language", "ENGLISH", kConsoleVariableFlagArchive);

EDGE_DEFINE_CONSOLE_VARIABLE(log_filename, "edge-classic.log", kConsoleVariableFlagNoReset)
EDGE_DEFINE_CONSOLE_VARIABLE(config_filename, "edge-classic.cfg", kConsoleVariableFlagNoReset)
EDGE_DEFINE_CONSOLE_VARIABLE(debug_filename, "debug.txt", kConsoleVariableFlagNoReset)
EDGE_DEFINE_CONSOLE_VARIABLE(window_title, "EDGE-Classic", kConsoleVariableFlagNoReset)
EDGE_DEFINE_CONSOLE_VARIABLE(edge_version, "1.37", kConsoleVariableFlagNoReset)
EDGE_DEFINE_CONSOLE_VARIABLE(team_name, "EDGE Team", kConsoleVariableFlagNoReset)
EDGE_DEFINE_CONSOLE_VARIABLE(application_name, "EDGE-Classic", kConsoleVariableFlagNoReset)
EDGE_DEFINE_CONSOLE_VARIABLE(homepage, "https://edge-classic.github.io", kConsoleVariableFlagNoReset)

EDGE_DEFINE_CONSOLE_VARIABLE_CLAMPED(video_overlay, "0", kConsoleVariableFlagArchive, 0, 6)

EDGE_DEFINE_CONSOLE_VARIABLE_CLAMPED(title_scaling, "0", kConsoleVariableFlagArchive, 0, 1)

EDGE_DEFINE_CONSOLE_VARIABLE(force_infighting, "0", kConsoleVariableFlagArchive)

EDGE_DEFINE_CONSOLE_VARIABLE(ddf_strict, "0", kConsoleVariableFlagArchive)
EDGE_DEFINE_CONSOLE_VARIABLE(ddf_lax, "0", kConsoleVariableFlagArchive)
EDGE_DEFINE_CONSOLE_VARIABLE(ddf_quiet, "0", kConsoleVariableFlagArchive)

EDGE_DEFINE_CONSOLE_VARIABLE(skip_intros, "0", kConsoleVariableFlagArchive)

static const Image *loading_image = nullptr;
const Image        *menu_backdrop = nullptr;

static void TitleDrawer(void);

class StartupProgress
{
  private:
    std::vector<std::string> startup_messages_;

  public:
    StartupProgress()
    {
    }

    ~StartupProgress()
    {
    }

    bool IsEmpty()
    {
        return startup_messages_.empty();
    }

    void Clear()
    {
        startup_messages_.clear();
    }

    void AddMessage(const char *message)
    {
        if (startup_messages_.size() >= 15)
            startup_messages_.erase(startup_messages_.begin());
        startup_messages_.push_back(message);
    }

    void DrawIt()
    {
        StartFrame();
        HudFrameSetup();
        if (loading_image)
        {
            if (title_scaling.d_) // Fill Border
            {
                if (!loading_image->blurred_version_)
                    ImageStoreBlurred(loading_image);
                HudStretchImage(-320, -200, 960, 600, loading_image->blurred_version_, 0, 0);
            }
            HudDrawImageTitleWS(loading_image);
            HudSolidBox(25, 25, 295, 175, SG_BLACK_RGBA32);
        }
        int y = 26;
        for (int i = 0; i < (int)startup_messages_.size(); i++)
        {
            if (startup_messages_[i].size() > 32)
                HudDrawText(26, y, startup_messages_[i].substr(0, 29).append("...").c_str());
            else
                HudDrawText(26, y, startup_messages_[i].c_str());
            y += 10;
        }

        if (!hud_overlays.at(video_overlay.d_).empty())
        {
            const Image *overlay =
                ImageLookup(hud_overlays.at(video_overlay.d_).c_str(), kImageNamespaceGraphic, kImageLookupNull);
            if (overlay)
                HudRawImage(0, 0, current_screen_width, current_screen_height, overlay, 0, 0,
                            current_screen_width / overlay->ScaledWidthActual(),
                            current_screen_height / overlay->ScaledHeightActual());
        }

        if (gamma_correction.f_ < 0)
        {
            int col = (1.0f + gamma_correction.f_) * 255;
            glEnable(GL_BLEND);
            glBlendFunc(GL_ZERO, GL_SRC_COLOR);
            HudSolidBox(hud_x_left, 0, hud_x_right, 200, epi::MakeRGBA(col, col, col));
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDisable(GL_BLEND);
        }
        else if (gamma_correction.f_ > 0)
        {
            int col = gamma_correction.f_ * 255;
            glEnable(GL_BLEND);
            glBlendFunc(GL_DST_COLOR, GL_ONE);
            HudSolidBox(hud_x_left, 0, hud_x_right, 200, epi::MakeRGBA(col, col, col));
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDisable(GL_BLEND);
        }

        FinishFrame();
    }
};

static StartupProgress startup_progress;

void StartupProgressMessage(const char *message)
{
    startup_progress.AddMessage(message);
    startup_progress.DrawIt();
}

//
// -ACB- 1999/09/20 Created. Sets Global Stuff.
//
static void SetGlobalVariables(void)
{
    int         p;
    std::string s;

    // Screen Resolution Check...
    if (ArgumentFind("borderless") > 0)
        current_window_mode = 2;
    else if (ArgumentFind("fullscreen") > 0)
        current_window_mode = 1;
    else if (ArgumentFind("windowed") > 0)
        current_window_mode = 0;

    s = ArgumentValue("width");
    if (!s.empty())
    {
        if (current_window_mode == 2)
            LogWarning("Current display mode set to borderless fullscreen. Provided "
                       "width of %d will be ignored!\n",
                       atoi(s.c_str()));
        else
            current_screen_width = atoi(s.c_str());
    }

    s = ArgumentValue("height");
    if (!s.empty())
    {
        if (current_window_mode == 2)
            LogWarning("Current display mode set to borderless fullscreen. Provided "
                       "height of %d will be ignored!\n",
                       atoi(s.c_str()));
        else
            current_screen_height = atoi(s.c_str());
    }

    p = ArgumentFind("res");
    if (p > 0 && p + 2 < int(program_argument_list.size()) && !ArgumentIsOption(p + 1) && !ArgumentIsOption(p + 2))
    {
        if (current_window_mode == 2)
            LogWarning("Current display mode set to borderless fullscreen. Provided "
                       "resolution of %dx%d will be ignored!\n",
                       atoi(program_argument_list[p + 1].c_str()), atoi(program_argument_list[p + 2].c_str()));
        else
        {
            current_screen_width  = atoi(program_argument_list[p + 1].c_str());
            current_screen_height = atoi(program_argument_list[p + 2].c_str());
        }
    }

    // Bits per pixel check....
    s = ArgumentValue("bpp");
    if (!s.empty())
    {
        current_screen_depth = atoi(s.c_str());

        if (current_screen_depth <= 4) // backwards compat
            current_screen_depth *= 8;
    }

    // restrict depth to allowable values
    if (current_screen_depth < 15)
        current_screen_depth = 15;
    if (current_screen_depth > 32)
        current_screen_depth = 32;

    // If borderless fullscreen mode, override any provided dimensions so
    // StartupGraphics will scale to native res
    if (current_window_mode == 2)
    {
        current_screen_width  = 100000;
        current_screen_height = 100000;
    }

    // sprite kludge (TrueBSP)
    p = ArgumentFind("spritekludge");
    if (p > 0)
    {
        if (p + 1 < int(program_argument_list.size()) && !ArgumentIsOption(p + 1))
            sprite_kludge = atoi(program_argument_list[p + 1].c_str());

        if (!sprite_kludge)
            sprite_kludge = 1;
    }

    s = ArgumentValue("screenshot");
    if (!s.empty())
    {
        screenshot_rate = atoi(s.c_str());
        single_tics     = true;
    }

    // -AJA- 1999/10/18: Reworked these with ArgumentCheckBooleanParameter
    ArgumentCheckBooleanParameter("rotate_map", &rotate_map, false);
    ArgumentCheckBooleanParameter("sound", &no_sound, true);
    ArgumentCheckBooleanParameter("music", &no_music, true);
    ArgumentCheckBooleanParameter("items_respawn", &global_flags.items_respawn, false);
    ArgumentCheckBooleanParameter("mlook", &global_flags.mouselook, false);
    ArgumentCheckBooleanParameter("monsters", &global_flags.no_monsters, true);
    ArgumentCheckBooleanParameter("fast", &global_flags.fast_monsters, false);
    ArgumentCheckBooleanParameter("extras", &global_flags.have_extra, false);
    ArgumentCheckBooleanParameter("kick", &global_flags.kicking, false);
    ArgumentCheckBooleanParameter("single_tics", &single_tics, false);
    ArgumentCheckBooleanParameter("true3d", &global_flags.true_3d_gameplay, false);
    ArgumentCheckBooleanParameter("blood", &global_flags.more_blood, false);
    ArgumentCheckBooleanParameter("cheats", &global_flags.cheats, false);
    ArgumentCheckBooleanParameter("jumping", &global_flags.jump, false);
    ArgumentCheckBooleanParameter("crouching", &global_flags.crouch, false);
    ArgumentCheckBooleanParameter("weaponswitch", &global_flags.weapon_switch, false);

    ArgumentCheckBooleanParameter("automap_keydoor_blink", &automap_keydoor_blink, false);

    if (ArgumentFind("infight") > 0)
        force_infighting = 1;

    if (ArgumentFind("dlights") > 0)
        use_dynamic_lights = 1;
    else if (ArgumentFind("nodlights") > 0)
        use_dynamic_lights = 0;

    if (!global_flags.enemies_respawn)
    {
        if (ArgumentFind("newnmrespawn") > 0)
        {
            global_flags.enemy_respawn_mode = true;
            global_flags.enemies_respawn    = true;
        }
        else if (ArgumentFind("respawn") > 0)
        {
            global_flags.enemies_respawn = true;
        }
    }

    // check for strict and no-warning options
    ArgumentCheckBooleanConsoleVariable("strict", &ddf_strict, false);
    ArgumentCheckBooleanConsoleVariable("lax", &ddf_lax, false);
    ArgumentCheckBooleanConsoleVariable("warn", &ddf_quiet, true);

    strict_errors = ddf_strict.d_ ? true : false;
    lax_errors    = ddf_lax.d_ ? true : false;
    no_warnings   = ddf_quiet.d_ ? true : false;
}

//
// SetLanguage
//
void SetLanguage(void)
{
    std::string want_lang = ArgumentValue("lang");
    if (!want_lang.empty())
        m_language = want_lang;

    if (language.Select(m_language.c_str()))
        return;

    LogWarning("Invalid language: '%s'\n", m_language.c_str());

    if (!language.Select(0))
        FatalError("Unable to select any language!");

    m_language = language.GetName();
}

//
// SpecialWadVerify
//
static void SpecialWadVerify(void)
{
    StartupProgressMessage("Verifying EDGE_DEFS version...");

    epi::File *data = OpenFileFromPack("/version.txt");

    if (!data)
        FatalError("Version file not found. Get edge_defs.epk at "
                   "https://github.com/edge-classic/EDGE-classic");

    // parse version number
    std::string verstring = data->ReadText();
    const char *s         = verstring.data();
    int         epk_ver   = atoi(s) * 100;

    while (epi::IsDigitASCII(*s))
        s++;
    s++;
    epk_ver += atoi(s);

    delete data;

    float real_ver = epk_ver / 100.0;

    LogPrint("EDGE_DEFS.EPK version %1.2f found.\n", real_ver);

    if (real_ver < edge_version.f_)
    {
        FatalError("EDGE_DEFS.EPK is an older version (got %1.2f, expected %1.2f)\n", real_ver, edge_version.f_);
    }
    else if (real_ver > edge_version.f_)
    {
        LogWarning("EDGE_DEFS.EPK is a newer version (got %1.2f, expected %1.2f)\n", real_ver, edge_version.f_);
    }
}

//
// ShowNotice
//
static void ShowNotice(void)
{
    ConsoleMessageColor(epi::MakeRGBA(64, 192, 255));

    LogPrint("%s", language["Notice"]);
}

static void DoSystemStartup(void)
{
    // startup the system now
    InitializeImages();

    LogDebug("- System startup begun.\n");

    SystemStartup();

    // -ES- 1998/09/11 Use ChangeResolution to enter gfx mode

    DumpResolutionList();

    // -KM- 1998/09/27 Change res now, so music doesn't start before
    // screen.  Reset clock too.
    LogDebug("- Changing Resolution...\n");

    SetInitialResolution();

    RendererInit();
    SoftInitializeResolution();

    LogDebug("- System startup done.\n");
}

static void DisplayPauseImage(void)
{
    static const Image *pause_image = nullptr;

    if (!pause_image)
        pause_image = ImageLookup("M_PAUSE");

    // make sure image is centered horizontally

    float w = pause_image->ScaledWidthActual();
    float h = pause_image->ScaledHeightActual();

    float x = 160 - w / 2;
    float y = 10;

    HudStretchImage(x, y, w, h, pause_image, 0.0, 0.0);
}

ScreenWipe wipe_method = kScreenWipeMelt;

static bool need_wipe = false;

void ForceWipe(void)
{
#ifdef EDGE_WEB
    // Wiping blocks the main thread while rendering outside of the main loop
    // tick Disabled on the platform until can be better integrated
    return;
#endif
    if (game_state == kGameStateNothing)
        return;

    if (wipe_method == kScreenWipeNone)
        return;

    need_wipe = true;

    // capture screen now (before new level is loaded etc..)
    EdgeDisplay();
}

//
// E_Display
//
// Draw current display, possibly wiping it from the previous
//
// -ACB- 1998/07/27 Removed doublebufferflag check (unneeded).

static bool wipe_gl_active = false;

void EdgeDisplay(void)
{
    EDGE_ZoneScoped;

    // Start the frame - should we need to.
    StartFrame();

    HudFrameSetup();

    switch (game_state)
    {
    case kGameStateLevel:
        PaletteTicker();

        if (LuaUseLuaHud())
            LuaRunHud();
        else
            CoalRunHud();

        if (need_save_screenshot)
        {
            CreateSaveScreenshot();
            need_save_screenshot = false;
        }

        HudDrawer();
        ScriptDrawer();
        break;

    case kGameStateIntermission:
        IntermissionDrawer();
        break;

    case kGameStateFinale:
        FinaleDrawer();
        break;

    case kGameStateTitleScreen:
        TitleDrawer();
        break;

    case kGameStateNothing:
        break;
    }

    if (wipe_gl_active)
    {
        // -AJA- Wipe code for GL.  Sorry for all this ugliness, but it just
        //       didn't fit into the existing wipe framework.
        //
        if (RendererDoWipe())
        {
            RendererStopWipe();
            wipe_gl_active = false;
        }
    }

    // save the current screen if about to wipe
    if (need_wipe)
    {
        need_wipe      = false;
        wipe_gl_active = true;

        RendererInitializeWipe(wipe_method);
    }

    if (paused)
        DisplayPauseImage();

    // menus go directly to the screen
    MenuDrawer(); // menu is drawn even on top of everything (except console)

    // process mouse and keyboard events
    NetworkUpdate();

    ConsoleDrawer();

    if (!hud_overlays.at(video_overlay.d_).empty())
    {
        const Image *overlay =
            ImageLookup(hud_overlays.at(video_overlay.d_).c_str(), kImageNamespaceGraphic, kImageLookupNull);
        if (overlay)
            HudRawImage(0, 0, current_screen_width, current_screen_height, overlay, 0, 0,
                        current_screen_width / overlay->ScaledWidthActual(),
                        current_screen_height / overlay->ScaledHeightActual());
    }

    if (gamma_correction.f_ < 0)
    {
        int col = (1.0f + gamma_correction.f_) * 255;
        glEnable(GL_BLEND);
        glBlendFunc(GL_ZERO, GL_SRC_COLOR);
        HudSolidBox(hud_x_left, 0, hud_x_right, 200, epi::MakeRGBA(col, col, col));
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_BLEND);
    }
    else if (gamma_correction.f_ > 0)
    {
        int col = gamma_correction.f_ * 255;
        glEnable(GL_BLEND);
        glBlendFunc(GL_DST_COLOR, GL_ONE);
        HudSolidBox(hud_x_left, 0, hud_x_right, 200, epi::MakeRGBA(col, col, col));
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_BLEND);
    }

    if (m_screenshot_required)
    {
        m_screenshot_required = false;
        TakeScreenshot(true);
    }
    else if (screenshot_rate && (game_state >= kGameStateLevel))
    {
        EPI_ASSERT(single_tics);

        if (level_time_elapsed % screenshot_rate == 0)
            TakeScreenshot(false);
    }

    FinishFrame(); // page flip or blit buffer
}

//
//  TITLE LOOP
//
static int title_game;
static int title_pic;
static int title_countdown;

static const Image *title_image = nullptr;

static void TitleDrawer(void)
{
    if (title_image)
    {
        if (title_scaling.d_) // Fill Border
        {
            if (!title_image->blurred_version_)
                ImageStoreBlurred(title_image);
            HudStretchImage(-320, -200, 960, 600, title_image->blurred_version_, 0, 0);
        }
        HudDrawImageTitleWS(title_image);
    }
    else
    {
        HudSolidBox(0, 0, 320, 200, SG_BLACK_RGBA32);
    }
}

//
// This cycles through the title sequences.
// -KM- 1998/12/16 Fixed for DDF.
//
void PickLoadingScreen(void)
{
    // force pic overflow -> first available titlepic
    title_game = gamedefs.size() - 1;
    title_pic  = 29999;

    // prevent an infinite loop
    for (int loop = 0; loop < 100; loop++)
    {
        GameDefinition *g = gamedefs[title_game];
        EPI_ASSERT(g);

        if (title_pic >= (int)g->titlepics_.size())
        {
            title_game = (title_game + 1) % (int)gamedefs.size();
            title_pic  = 0;
            continue;
        }

        // ignore non-existing episodes.  Doesn't include title-only ones
        // like [EDGE].
        if (title_pic == 0 && g->firstmap_ != "" && CheckLumpNumberForName(g->firstmap_.c_str()) == -1)
        {
            title_game = (title_game + 1) % gamedefs.size();
            title_pic  = 0;
            continue;
        }

        // ignore non-existing images
        loading_image = ImageLookup(g->titlepics_[title_pic].c_str(), kImageNamespaceGraphic, kImageLookupNull);

        if (!loading_image)
        {
            title_pic++;
            continue;
        }

        // found one !!
        title_game = gamedefs.size() - 1;
        title_pic  = 29999;
        return;
    }

    // not found
    title_game    = gamedefs.size() - 1;
    title_pic     = 29999;
    loading_image = nullptr;
}

//
// Find and create a desaturated version of the first
// titlepic corresponding to a gamedef with actual maps.
// This is used as the menu backdrop
//
static void PickMenuBackdrop(void)
{
    // force pic overflow -> first available titlepic
    title_game = gamedefs.size() - 1;
    title_pic  = 29999;

    // prevent an infinite loop
    for (int loop = 0; loop < 100; loop++)
    {
        GameDefinition *g = gamedefs[title_game];
        EPI_ASSERT(g);

        if (title_pic >= (int)g->titlepics_.size())
        {
            title_game = (title_game + 1) % (int)gamedefs.size();
            title_pic  = 0;
            continue;
        }

        // ignore non-existing episodes.
        if (title_pic == 0 && (g->firstmap_ == "" || CheckLumpNumberForName(g->firstmap_.c_str()) == -1))
        {
            title_game = (title_game + 1) % gamedefs.size();
            title_pic  = 0;
            continue;
        }

        // ignore non-existing images
        const Image *menu_image =
            ImageLookup(g->titlepics_[title_pic].c_str(), kImageNamespaceGraphic, kImageLookupNull);

        if (!menu_image)
        {
            title_pic++;
            continue;
        }

        // found one !!
        title_game                       = gamedefs.size() - 1;
        title_pic                        = 29999;
        Image *new_backdrop              = new Image;
        new_backdrop->name_              = menu_image->name_;
        new_backdrop->actual_height_     = menu_image->actual_height_;
        new_backdrop->actual_width_      = menu_image->actual_width_;
        new_backdrop->cache_             = menu_image->cache_;
        new_backdrop->is_empty_          = menu_image->is_empty_;
        new_backdrop->is_font_           = menu_image->is_font_;
        new_backdrop->liquid_type_       = menu_image->liquid_type_;
        new_backdrop->offset_x_          = menu_image->offset_x_;
        new_backdrop->offset_y_          = menu_image->offset_y_;
        new_backdrop->opacity_           = menu_image->opacity_;
        new_backdrop->height_ratio_      = menu_image->height_ratio_;
        new_backdrop->width_ratio_       = menu_image->width_ratio_;
        new_backdrop->scale_x_           = menu_image->scale_x_;
        new_backdrop->scale_y_           = menu_image->scale_y_;
        new_backdrop->source_            = menu_image->source_;
        new_backdrop->source_palette_    = menu_image->source_palette_;
        new_backdrop->source_type_       = menu_image->source_type_;
        new_backdrop->total_height_      = menu_image->total_height_;
        new_backdrop->total_width_       = menu_image->total_width_;
        new_backdrop->animation_.current = new_backdrop;
        new_backdrop->grayscale_         = true;
        menu_backdrop                    = new_backdrop;
        return;
    }

    // if we get here just use the loading image if it exists
    title_game = gamedefs.size() - 1;
    title_pic  = 29999;
    if (loading_image)
    {
        Image *new_backdrop              = new Image;
        new_backdrop->name_              = loading_image->name_;
        new_backdrop->actual_height_     = loading_image->actual_height_;
        new_backdrop->actual_width_      = loading_image->actual_width_;
        new_backdrop->cache_             = loading_image->cache_;
        new_backdrop->is_empty_          = loading_image->is_empty_;
        new_backdrop->is_font_           = loading_image->is_font_;
        new_backdrop->liquid_type_       = loading_image->liquid_type_;
        new_backdrop->offset_x_          = loading_image->offset_x_;
        new_backdrop->offset_y_          = loading_image->offset_y_;
        new_backdrop->opacity_           = loading_image->opacity_;
        new_backdrop->height_ratio_      = loading_image->height_ratio_;
        new_backdrop->width_ratio_       = loading_image->width_ratio_;
        new_backdrop->scale_x_           = loading_image->scale_x_;
        new_backdrop->scale_y_           = loading_image->scale_y_;
        new_backdrop->source_            = loading_image->source_;
        new_backdrop->source_palette_    = loading_image->source_palette_;
        new_backdrop->source_type_       = loading_image->source_type_;
        new_backdrop->total_height_      = loading_image->total_height_;
        new_backdrop->total_width_       = loading_image->total_width_;
        new_backdrop->animation_.current = new_backdrop;
        new_backdrop->grayscale_         = true;
        menu_backdrop                    = new_backdrop;
    }
    else
        menu_backdrop = nullptr;
}

//
// This cycles through the title sequences.
// -KM- 1998/12/16 Fixed for DDF.
//
void AdvanceTitle(void)
{
    title_pic++;

    // prevent an infinite loop
    for (int loop = 0; loop < 100; loop++)
    {
        GameDefinition *g = gamedefs[title_game];
        EPI_ASSERT(g);

        // Only play title movies once
        if (!g->titlemovie_.empty() && !g->movie_played_)
        {
            if (skip_intros.d_)
                g->movie_played_ = true;
            else
            {
                PlayMovie(g->titlemovie_);
                g->movie_played_ = true;
            }
            continue;
        }

        if (show_old_config_warning && startup_progress.IsEmpty())
        {
            MenuStartMessage(language["OldConfig"], nullptr, false);
            show_old_config_warning = false;
        }

        if (title_pic >= (int)g->titlepics_.size())
        {
            title_game = (title_game + 1) % (int)gamedefs.size();
            title_pic  = 0;
            continue;
        }

        // ignore non-existing episodes.  Doesn't include title-only ones
        // like [EDGE].
        if (title_pic == 0 && g->firstmap_ != "" && CheckLumpNumberForName(g->firstmap_.c_str()) == -1)
        {
            title_game = (title_game + 1) % gamedefs.size();
            title_pic  = 0;
            continue;
        }

        // ignore non-existing images
        title_image = ImageLookup(g->titlepics_[title_pic].c_str(), kImageNamespaceGraphic, kImageLookupNull);

        if (!title_image)
        {
            title_pic++;
            continue;
        }

        // found one !!
        if (title_pic == 0 && g->titlemusic_ > 0)
            ChangeMusic(g->titlemusic_, false);

        title_countdown = g->titletics_ * (double_framerate.d_ ? 2 : 1);
        return;
    }

    // not found

    title_image     = nullptr;
    title_countdown = kTicRate * (double_framerate.d_ ? 2 : 1);
}

void StartTitle(void)
{
    game_action = kGameActionNothing;
    game_state  = kGameStateTitleScreen;

    paused = false;

    title_countdown = 1;

    AdvanceTitle();
}

void TitleTicker(void)
{
    if (title_countdown > 0)
    {
        title_countdown--;

        if (title_countdown == 0)
            AdvanceTitle();
    }
}

//
// Detects which directories to search for DDFs, WADs and other files in.
//
// -ES- 2000/01/01 Written.
//
static void InitializeDirectories(void)
{
    // Get the App Directory from parameter.

    // Note: This might need adjusting for Apple
    std::string s = SDL_GetBasePath();

    game_directory = s;
    s              = ArgumentValue("game");
    if (!s.empty())
        game_directory = s;

    branding_file = epi::PathAppend(game_directory, kBrandingFileName);

    ConfigurationLoadBranding();

    // add parameter file "appdir/parms" if it exists.
    std::string parms = epi::PathAppend(game_directory, "parms");

    if (epi::TestFileAccess(parms))
    {
        // Insert it right after the app parameter
        ArgumentApplyResponseFile(parms);
    }

    // config file - check for portable config
    s = ArgumentValue("config");
    if (!s.empty())
    {
        configuration_file = s;
    }
    else
    {
        configuration_file = epi::PathAppend(game_directory, config_filename.s_);
        if (epi::TestFileAccess(configuration_file) || ArgumentFind("portable") > 0)
            home_directory = game_directory;
        else
            configuration_file.clear();
    }

    if (home_directory.empty())
    {
        s = ArgumentValue("home");
        if (!s.empty())
            home_directory = s;
    }

#ifdef _WIN32
    if (home_directory.empty())
        home_directory = SDL_GetPrefPath(nullptr, application_name.c_str());
#else
    if (home_directory.empty())
        home_directory = SDL_GetPrefPath(team_name.c_str(), application_name.c_str());
#endif

    if (!epi::IsDirectory(home_directory))
    {
        if (!epi::MakeDirectory(home_directory))
            FatalError("InitializeDirectories: Could not create directory at %s!\n", home_directory.c_str());
    }

    if (configuration_file.empty())
        configuration_file = epi::PathAppend(home_directory, config_filename.s_);

    // edge_defs.epk file
    s = ArgumentValue("defs");
    if (!s.empty())
    {
        epkfile = s;
    }
    else
    {
        std::string defs_test = epi::PathAppend(game_directory, "edge_defs");
        if (epi::IsDirectory(defs_test))
            epkfile = defs_test;
        else
            epkfile.append(".epk");
    }

    // cache directory
    cache_directory = epi::PathAppend(home_directory, kCacheDirectory);

    if (!epi::IsDirectory(cache_directory))
        epi::MakeDirectory(cache_directory);

    // savegame directory
    save_directory = epi::PathAppend(home_directory, kSaveGameDirectory);

    if (!epi::IsDirectory(save_directory))
        epi::MakeDirectory(save_directory);

    SaveClearSlot("current");

    // screenshot directory
    screenshot_directory = epi::PathAppend(home_directory, kScreenshotDirectory);

    if (!epi::IsDirectory(screenshot_directory))
        epi::MakeDirectory(screenshot_directory);
}

// Get rid of legacy GWA/HWA files

static void PurgeCache(void)
{
    std::vector<epi::DirectoryEntry> fsd;

    if (!ReadDirectory(fsd, cache_directory, "*.*"))
    {
        FatalError("PurgeCache: Failed to read '%s' directory!\n", cache_directory.c_str());
    }
    else
    {
        for (size_t i = 0; i < fsd.size(); i++)
        {
            if (!fsd[i].is_dir)
            {
                std::string ext = epi::GetExtension(fsd[i].name);
                epi::StringLowerASCII(ext);
                if (ext == ".gwa" || ext == ".hwa")
                    epi::FileDelete(fsd[i].name);
            }
        }
    }
}

// If a valid IWAD (or EDGEGAME) is found, return the appopriate game_base
// string ("doom2", "heretic", etc)
static int CheckPackForGameFiles(std::string check_pack, FileKind check_kind)
{
    DataFile *check_pack_df = new DataFile(check_pack, check_kind);
    EPI_ASSERT(check_pack_df);
    PackPopulateOnly(check_pack_df);
    if (PackFindStem(check_pack_df->pack_, "EDGEGAME"))
    {
        delete check_pack_df;
        return 0; // Custom game index value in game_checker vector
    }
    else
    {
        int check_base = PackCheckForIwads(check_pack_df);
        delete check_pack_df;
        return check_base;
    }
}

//
// Adds main game content and edge_defs folder/EPK
//
static void IdentifyVersion(void)
{
    // For env checks
    const char *check = nullptr;

    if (epi::IsDirectory(epkfile))
        AddDataFile(epkfile, kFileKindEFolder);
    else
    {
        if (!epi::TestFileAccess(epkfile))
            FatalError("IdentifyVersion: Could not find required %s.%s!\n", kRequiredEPK, "epk");
        AddDataFile(epkfile, kFileKindEEpk);
    }

    LogDebug("- Identify Version\n");

    // Check -iwad parameter, find out if it is the IWADs directory
    std::string              iwad_par;
    std::string              iwad_file;
    std::string              iwad_dir;
    std::vector<std::string> iwad_dir_vector;

    std::string s = ArgumentValue("iwad");

    iwad_par = s;

    if (!iwad_par.empty())
    {
        // Treat directories passed via -iwad as a pack file and check
        // accordingly
        if (epi::IsDirectory(iwad_par))
        {
            int game_check = CheckPackForGameFiles(iwad_par, kFileKindIFolder);
            if (game_check < 0)
                FatalError("Folder %s passed via -iwad parameter, but no IWAD or "
                           "EDGEGAME file detected!\n",
                           iwad_par.c_str());
            else
            {
                game_base = game_checker[game_check].base;
                AddDataFile(iwad_par, kFileKindIFolder);
                LogDebug("GAME BASE = [%s]\n", game_base.c_str());
                return;
            }
        }
    }
    else
    {
        // In the absence of the -iwad parameter, check files/dirs added via
        // drag-and-drop for valid IWADs Remove them from the arg list if they
        // are valid to avoid them potentially being added as PWADs
        std::vector<SDL_MessageBoxButtonData>                     game_buttons;
        std::unordered_map<int, std::pair<std::string, FileKind>> game_paths;
        for (size_t p = 1; p < program_argument_list.size() && !ArgumentIsOption(p); p++)
        {
            std::string dnd        = program_argument_list[p];
            int         test_index = -1;
            if (epi::IsDirectory(dnd))
            {
                test_index = CheckPackForGameFiles(dnd, kFileKindIFolder);
                if (test_index >= 0)
                {
                    if (!game_paths.count(test_index))
                    {
                        game_paths.try_emplace(test_index, std::make_pair(dnd, kFileKindIFolder));
                        SDL_MessageBoxButtonData temp_button;
                        temp_button.buttonid = test_index;
                        temp_button.text     = game_checker[test_index].display_name.c_str();
                        game_buttons.push_back(temp_button);
                    }
                    program_argument_list.erase(program_argument_list.begin() + p--);
                }
            }
            else if (epi::StringCaseCompareASCII(epi::GetExtension(dnd), ".epk") == 0)
            {
                test_index = CheckPackForGameFiles(dnd, kFileKindIpk);
                if (test_index >= 0)
                {
                    if (!game_paths.count(test_index))
                    {
                        game_paths.try_emplace(test_index, std::make_pair(dnd, kFileKindIpk));
                        SDL_MessageBoxButtonData temp_button;
                        temp_button.buttonid = test_index;
                        temp_button.text     = game_checker[test_index].display_name.c_str();
                        game_buttons.push_back(temp_button);
                    }
                    program_argument_list.erase(program_argument_list.begin() + p--);
                }
            }
            else if (epi::StringCaseCompareASCII(epi::GetExtension(dnd), ".wad") == 0)
            {
                epi::File *game_test = epi::FileOpen(dnd, epi::kFileAccessRead | epi::kFileAccessBinary);
                test_index           = CheckForUniqueGameLumps(game_test);
                delete game_test;
                if (test_index >= 0)
                {
                    if (!game_paths.count(test_index))
                    {
                        game_paths.try_emplace(test_index, std::make_pair(dnd, kFileKindIWad));
                        SDL_MessageBoxButtonData temp_button;
                        temp_button.buttonid = test_index;
                        temp_button.text     = game_checker[test_index].display_name.c_str();
                        game_buttons.push_back(temp_button);
                    }
                    program_argument_list.erase(program_argument_list.begin() + p--);
                }
            }
        }
        if (game_paths.size() == 1)
        {
            auto selected_game = game_paths.begin();
            game_base          = game_checker[selected_game->first].base;
            AddDataFile(selected_game->second.first, selected_game->second.second);
            LogDebug("GAME BASE = [%s]\n", game_base.c_str());
            return;
        }
        else if (!game_paths.empty())
        {
            EPI_ASSERT(game_paths.size() == game_buttons.size());
            SDL_MessageBoxData picker_data;
            SDL_memset(&picker_data, 0, sizeof(SDL_MessageBoxData));
            picker_data.title      = "EDGE-Classic Game Selector";
            picker_data.message    = "No game was specified, but EDGE-Classic found multiple valid "
                                     "game files. Please select one or press Escape to cancel.";
            picker_data.numbuttons = game_buttons.size();
            picker_data.buttons    = game_buttons.data();
            int button_hit         = 0;
            if (SDL_ShowMessageBox(&picker_data, &button_hit) != 0)
                FatalError("Error in game selection dialog!\n");
            else if (button_hit == -1)
                FatalError("Game selection cancelled.\n");
            else
            {
                game_base          = game_checker[button_hit].base;
                auto selected_game = game_paths.at(button_hit);
                AddDataFile(selected_game.first, selected_game.second);
                LogDebug("GAME BASE = [%s]\n", game_base.c_str());
                return;
            }
        }
    }

    // If we haven't yet set the IWAD directory, then we check
    // the DOOMWADDIR environment variable
    check = SDL_getenv("DOOMWADDIR");
    if (check)
        s = check;

    if (!s.empty() && epi::IsDirectory(s))
        iwad_dir_vector.push_back(s);

    // Should the IWAD directory not be set by now, then we
    // use our standby option of the current directory.
    if (iwad_dir.empty())
        iwad_dir = "."; // should this be hardcoded to the game or home
                        // directory instead? - Dasho

    // Add DOOMWADPATH directories if they exist
    s.clear();
    check = SDL_getenv("DOOMWADPATH");
    if (check)
        s = check;

    if (!s.empty())
    {
        for (auto dir : epi::SeparatedStringVector(s, ':'))
            iwad_dir_vector.push_back(dir);
    }

    // Should the IWAD Parameter not be empty then it means
    // that one was given which is not a directory. Therefore
    // we assume it to be a name
    if (!iwad_par.empty())
    {
        std::string fn = iwad_par;

        // Is it missing the extension?
        if (epi::GetExtension(fn).empty())
        {
            // We will still be checking EPKs if needed; but by the numbers .wad
            // is a good initial search
            epi::ReplaceExtension(fn, ".wad");
        }

        // If no directory given use the IWAD directory
        std::string dir = epi::GetDirectory(fn);
        if (dir.empty())
            iwad_file = epi::PathAppend(iwad_dir, fn);
        else
            iwad_file = fn;

        if (!epi::TestFileAccess(iwad_file))
        {
            // Check DOOMWADPATH directories if present
            if (!iwad_dir_vector.empty())
            {
                for (size_t i = 0; i < iwad_dir_vector.size(); i++)
                {
                    iwad_file = epi::PathAppend(iwad_dir_vector[i], fn);
                    if (epi::TestFileAccess(iwad_file))
                        goto foundindoomwadpath;
                }
            }
        }
        else
            goto foundindoomwadpath;

        // If we get here, try .epk and error out if we still can't access what
        // was passed to us
        epi::ReplaceExtension(iwad_file, ".epk");

        if (!epi::TestFileAccess(iwad_file))
        {
            // Check DOOMWADPATH directories if present
            if (!iwad_dir_vector.empty())
            {
                for (size_t i = 0; i < iwad_dir_vector.size(); i++)
                {
                    iwad_file = epi::PathAppend(iwad_dir_vector[i], fn);
                    if (epi::TestFileAccess(iwad_file))
                        goto foundindoomwadpath;
                }
                FatalError("IdentifyVersion: Unable to access specified '%s'", fn.c_str());
            }
            else
                FatalError("IdentifyVersion: Unable to access specified '%s'", fn.c_str());
        }

    foundindoomwadpath:

        int test_score = -1;

        if (epi::StringCaseCompareASCII(epi::GetExtension(iwad_file), ".wad") == 0)
        {
            epi::File *game_test = epi::FileOpen(iwad_file, epi::kFileAccessRead | epi::kFileAccessBinary);
            test_score           = CheckForUniqueGameLumps(game_test);
            delete game_test;
            if (test_score >= 0)
            {
                game_base = game_checker[test_score].base;
                AddDataFile(iwad_file, kFileKindIWad);
            }
            else
                FatalError("IdentifyVersion: Could not identify '%s' as a valid "
                           "IWAD!\n",
                           fn.c_str());
        }
        else
        {
            test_score = CheckPackForGameFiles(iwad_file, kFileKindIpk);
            if (test_score >= 0)
            {
                game_base = game_checker[test_score].base;
                AddDataFile(iwad_file, kFileKindIpk);
            }
            else
                FatalError("IdentifyVersion: Could not identify '%s' as a valid "
                           "IWAD!\n",
                           fn.c_str());
        }
    }
    else
    {
        std::string location;

        std::vector<SDL_MessageBoxButtonData>                     game_buttons;
        std::unordered_map<int, std::pair<std::string, FileKind>> game_paths;

        int max = 1;

        if (iwad_dir.compare(game_directory) != 0)
        {
            // IWAD directory & game directory differ
            // therefore do a second loop which will
            // mean we check both.
            max++;
        }

        for (int i = 0; i < max; i++)
        {
            location = (i == 0 ? iwad_dir : game_directory);

            //
            // go through the available *.wad files, attempting IWAD
            // detection for each, adding the file if they exist.
            //
            // -ACB- 2000/06/08 Quit after we found a file - don't load
            //                  more than one IWAD
            //
            std::vector<epi::DirectoryEntry> fsd;

            if (!ReadDirectory(fsd, location, "*.wad"))
            {
                LogDebug("IdentifyVersion: No WADs found in '%s' directory!\n", location.c_str());
            }
            else
            {
                for (size_t j = 0; j < fsd.size(); j++)
                {
                    if (!fsd[j].is_dir)
                    {
                        epi::File *game_test =
                            epi::FileOpen(fsd[j].name, epi::kFileAccessRead | epi::kFileAccessBinary);
                        int test_score = CheckForUniqueGameLumps(game_test);
                        delete game_test;
                        if (test_score >= 0)
                        {
                            if (!game_paths.count(test_score))
                            {
                                game_paths.try_emplace(test_score, std::make_pair(fsd[j].name, kFileKindIWad));
                                SDL_MessageBoxButtonData temp_button;
                                temp_button.buttonid = test_score;
                                temp_button.text     = game_checker[test_score].display_name.c_str();
                                game_buttons.push_back(temp_button);
                            }
                        }
                    }
                }
            }
            if (!ReadDirectory(fsd, location, "*.epk"))
            {
                LogDebug("IdentifyVersion: No EPKs found in '%s' directory!\n", location.c_str());
            }
            else
            {
                for (size_t j = 0; j < fsd.size(); j++)
                {
                    if (!fsd[j].is_dir)
                    {
                        int test_score = CheckPackForGameFiles(fsd[j].name, kFileKindIpk);
                        if (test_score >= 0)
                        {
                            if (!game_paths.count(test_score))
                            {
                                game_paths.try_emplace(test_score, std::make_pair(fsd[j].name, kFileKindIpk));
                                SDL_MessageBoxButtonData temp_button;
                                temp_button.buttonid = test_score;
                                temp_button.text     = game_checker[test_score].display_name.c_str();
                                game_buttons.push_back(temp_button);
                            }
                        }
                    }
                }
            }
        }

        // Separate check for DOOMWADPATH stuff if it exists - didn't want to
        // mess with the existing stuff above

        if (!iwad_dir_vector.empty())
        {
            for (size_t i = 0; i < iwad_dir_vector.size(); i++)
            {
                location = iwad_dir_vector[i].c_str();

                std::vector<epi::DirectoryEntry> fsd;

                if (!ReadDirectory(fsd, location, "*.wad"))
                {
                    LogDebug("IdentifyVersion: No WADs found in '%s' directory!\n", location.c_str());
                }
                else
                {
                    for (size_t j = 0; j < fsd.size(); j++)
                    {
                        if (!fsd[j].is_dir)
                        {
                            epi::File *game_test =
                                epi::FileOpen(fsd[j].name, epi::kFileAccessRead | epi::kFileAccessBinary);
                            int test_score = CheckForUniqueGameLumps(game_test);
                            delete game_test;
                            if (test_score >= 0)
                            {
                                if (!game_paths.count(test_score))
                                {
                                    game_paths.try_emplace(test_score, std::make_pair(fsd[j].name, kFileKindIWad));
                                    SDL_MessageBoxButtonData temp_button;
                                    temp_button.buttonid = test_score;
                                    temp_button.text     = game_checker[test_score].display_name.c_str();
                                    game_buttons.push_back(temp_button);
                                }
                            }
                        }
                    }
                }
                if (!ReadDirectory(fsd, location, "*.epk"))
                {
                    LogDebug("IdentifyVersion: No EPKs found in '%s' directory!\n", location.c_str());
                }
                else
                {
                    for (size_t j = 0; j < fsd.size(); j++)
                    {
                        if (!fsd[j].is_dir)
                        {
                            int test_score = CheckPackForGameFiles(fsd[j].name, kFileKindIpk);
                            if (test_score >= 0)
                            {
                                if (!game_paths.count(test_score))
                                {
                                    game_paths.try_emplace(test_score, std::make_pair(fsd[j].name, kFileKindIpk));
                                    SDL_MessageBoxButtonData temp_button;
                                    temp_button.buttonid = test_score;
                                    temp_button.text     = game_checker[test_score].display_name.c_str();
                                    game_buttons.push_back(temp_button);
                                }
                            }
                        }
                    }
                }
            }
        }

        if (game_paths.empty())
            FatalError("IdentifyVersion: No IWADs or standalone packs found!\n");
        else if (game_paths.size() == 1)
        {
            auto selected_game = game_paths.begin();
            game_base          = game_checker[selected_game->first].base;
            AddDataFile(selected_game->second.first, selected_game->second.second);
        }
        else
        {
            EPI_ASSERT(game_paths.size() == game_buttons.size());
            SDL_MessageBoxData picker_data;
            SDL_memset(&picker_data, 0, sizeof(SDL_MessageBoxData));
            picker_data.title      = "EDGE-Classic Game Selector";
            picker_data.message    = "No game was specified, but EDGE-Classic found multiple valid "
                                     "game files. Please select one or press Escape to cancel.";
            picker_data.numbuttons = game_buttons.size();
            picker_data.buttons    = game_buttons.data();
            int button_hit         = 0;
            if (SDL_ShowMessageBox(&picker_data, &button_hit) != 0)
                FatalError("Error in game selection dialog!\n");
            else if (button_hit == -1)
                FatalError("Game selection cancelled.\n");
            else
            {
                game_base          = game_checker[button_hit].base;
                auto selected_game = game_paths.at(button_hit);
                AddDataFile(selected_game.first, selected_game.second);
            }
        }
    }

    LogDebug("GAME BASE = [%s]\n", game_base.c_str());
}

// Add game-specific base EPKs (widepix, skyboxes, etc) - Dasho
static void AddBasePack(void)
{
    if (epi::StringCaseCompareASCII("CUSTOM", game_base) == 0)
        return; // Standalone EDGE IWADs/EPKs should already contain their
                // necessary resources and definitions - Dasho
    std::string base_path = epi::PathAppend(game_directory, "edge_base");
    std::string base_wad  = game_base;
    epi::StringLowerASCII(base_wad);
    base_path = epi::PathAppend(base_path, base_wad);
    if (epi::IsDirectory(base_path))
        AddDataFile(base_path, kFileKindEFolder);
    else
    {
        epi::ReplaceExtension(base_path, ".epk");
        if (epi::TestFileAccess(base_path))
            AddDataFile(base_path, kFileKindEEpk);
        else
        {
            FatalError("%s not found for the %s IWAD! Check the /edge_base folder of "
                       "your %s install!\n",
                       epi::GetFilename(base_path).c_str(), game_base.c_str(), application_name.c_str());
        }
    }
}

static void CheckTurbo(void)
{
    int turbo_scale = 100;

    int p = ArgumentFind("turbo");

    if (p > 0)
    {
        if (p + 1 < int(program_argument_list.size()) && !ArgumentIsOption(p + 1))
            turbo_scale = atoi(program_argument_list[p + 1].c_str());
        else
            turbo_scale = 200;

        if (turbo_scale < 10)
            turbo_scale = 10;
        if (turbo_scale > 400)
            turbo_scale = 400;

        ConsoleMessageLDF("TurboScale", turbo_scale);
    }

    EventSetTurboScale(turbo_scale);
}

static void ShowDateAndVersion(void)
{
    time_t cur_time;
    char   timebuf[100];

    time(&cur_time);
    strftime(timebuf, 99, "%I:%M %p on %d/%b/%Y", localtime(&cur_time));

    LogDebug("[Log file created at %s]\n\n", timebuf);
    LogDebug("[Debug file created at %s]\n\n", timebuf);

    LogPrint("%s v%s compiled on " __DATE__ " at " __TIME__ "\n", application_name.c_str(), edge_version.c_str());
    LogPrint("%s homepage is at %s\n", application_name.c_str(), homepage.c_str());

    LogPrint("Executable path: '%s'\n", executable_path.c_str());

    ArgumentDebugDump();
}

static void SetupLogAndDebugFiles(void)
{
    // -AJA- 2003/11/08 The log file gets all ConsolePrints, LogPrints,
    //                  LogWarnings and FatalErrors.

    std::string log_fn   = epi::PathAppend(home_directory, log_filename.s_);
    std::string debug_fn = epi::PathAppend(home_directory, debug_filename.s_);

    log_file   = nullptr;
    debug_file = nullptr;

    if (ArgumentFind("nolog") < 0)
    {
        log_file = epi::FileOpenRaw(log_fn, epi::kFileAccessWrite);

        if (!log_file)
            FatalError("[EdgeStartup] Unable to create log file\n");
    }

    //
    // -ACB- 1998/09/06 Only used for debugging.
    //                  Moved here to setup debug file for DDF Parsing...
    //
    // -ES- 1999/08/01 Debugfiles can now be used without -DDEVELOPERS, and
    //                 then logs all the ConsolePrints, LogPrints and
    //                 FatalErrors.
    //
    // -ACB- 1999/10/02 Don't print to console, since we don't have a console
    // yet.

    /// int p = ArgumentFind("debug");
    if (true)
    {
        debug_file = epi::FileOpenRaw(debug_fn, epi::kFileAccessWrite);

        if (!debug_file)
            FatalError("[EdgeStartup] Unable to create debug_file");
    }
}

static void AddSingleCommandLineFile(std::string name, bool ignore_unknown)
{
    if (epi::IsDirectory(name))
    {
        AddDataFile(name, kFileKindFolder);
        return;
    }

    std::string ext = epi::GetExtension(name);

    epi::StringLowerASCII(ext);

    if (ext == ".edm")
        FatalError("Demos are not supported\n");

    FileKind kind;

    if (ext == ".wad")
        kind = kFileKindPWad;
    else if (ext == ".pk3" || ext == ".epk" || ext == ".zip" || ext == ".vwad")
        kind = kFileKindEpk;
    else if (ext == ".rts")
        kind = kFileKindRts;
    else if (ext == ".ddf" || ext == ".ldf")
        kind = kFileKindDdf;
    else if (ext == ".deh" || ext == ".bex")
        kind = kFileKindDehacked;
    else
    {
        if (!ignore_unknown)
            FatalError("unknown file type: %s\n", name.c_str());
        return;
    }

    std::string filename = epi::PathAppendIfNotAbsolute(game_directory, name);
    AddDataFile(filename, kind);
}

static void AddCommandLineFiles(void)
{
    // first handle "loose" files (arguments before the first option)

    int p;

    for (p = 1; p < int(program_argument_list.size()) && !ArgumentIsOption(p); p++)
    {
        AddSingleCommandLineFile(program_argument_list[p], false);
    }

    // next handle the -file option (we allow multiple uses)

    p = ArgumentFind("file");

    while (p > 0 && p < int(program_argument_list.size()) &&
           (!ArgumentIsOption(p) || epi::StringCompare(program_argument_list[p], "-file") == 0))
    {
        // the parms after p are wadfile/lump names,
        // go until end of parms or another '-' preceded parm
        if (!ArgumentIsOption(p))
            AddSingleCommandLineFile(program_argument_list[p], false);

        p++;
    }

    // scripts....

    p = ArgumentFind("script");

    while (p > 0 && p < int(program_argument_list.size()) &&
           (!ArgumentIsOption(p) || epi::StringCompare(program_argument_list[p], "-script") == 0))
    {
        // the parms after p are script filenames,
        // go until end of parms or another '-' preceded parm
        if (!ArgumentIsOption(p))
        {
            std::string ext = epi::GetExtension(program_argument_list[p]);
            // sanity check...
            if (epi::StringCaseCompareASCII(ext, ".wad") == 0 || epi::StringCaseCompareASCII(ext, ".pk3") == 0 ||
                epi::StringCaseCompareASCII(ext, ".zip") == 0 || epi::StringCaseCompareASCII(ext, ".epk") == 0 ||
                epi::StringCaseCompareASCII(ext, ".vwad") == 0 || epi::StringCaseCompareASCII(ext, ".ddf") == 0 ||
                epi::StringCaseCompareASCII(ext, ".deh") == 0 || epi::StringCaseCompareASCII(ext, ".bex") == 0)
            {
                FatalError("Illegal filename for -script: %s\n", program_argument_list[p].c_str());
            }

            std::string filename = epi::PathAppendIfNotAbsolute(game_directory, program_argument_list[p]);
            AddDataFile(filename, kFileKindRts);
        }

        p++;
    }

    // dehacked/bex....

    p = ArgumentFind("deh");

    while (p > 0 && p < int(program_argument_list.size()) &&
           (!ArgumentIsOption(p) || epi::StringCompare(program_argument_list[p], "-deh") == 0))
    {
        // the parms after p are Dehacked/BEX filenames,
        // go until end of parms or another '-' preceded parm
        if (!ArgumentIsOption(p))
        {
            std::string ext = epi::GetExtension(program_argument_list[p]);
            // sanity check...
            if (epi::StringCaseCompareASCII(ext, ".wad") == 0 || epi::StringCaseCompareASCII(ext, ".epk") == 0 ||
                epi::StringCaseCompareASCII(ext, ".pk3") == 0 || epi::StringCaseCompareASCII(ext, ".zip") == 0 ||
                epi::StringCaseCompareASCII(ext, ".vwad") == 0 || epi::StringCaseCompareASCII(ext, ".ddf") == 0 ||
                epi::StringCaseCompareASCII(ext, ".rts") == 0)
            {
                FatalError("Illegal filename for -deh: %s\n", program_argument_list[p].c_str());
            }

            std::string filename = epi::PathAppendIfNotAbsolute(game_directory, program_argument_list[p]);
            AddDataFile(filename, kFileKindDehacked);
        }

        p++;
    }

    // directories....

    p = ArgumentFind("dir");

    while (p > 0 && p < int(program_argument_list.size()) &&
           (!ArgumentIsOption(p) || epi::StringCompare(program_argument_list[p], "-dir") == 0))
    {
        // the parms after p are directory names,
        // go until end of parms or another '-' preceded parm
        if (!ArgumentIsOption(p))
        {
            std::string dirname = epi::PathAppendIfNotAbsolute(game_directory, program_argument_list[p]);
            AddDataFile(dirname, kFileKindFolder);
        }

        p++;
    }

    // handle -ddf option (backwards compatibility)

    std::string ps = ArgumentValue("ddf");

    if (!ps.empty())
    {
        std::string filename = epi::PathAppendIfNotAbsolute(game_directory, ps);
        AddDataFile(filename, kFileKindFolder);
    }
}

static void AddAutoload(void)
{
    std::vector<epi::DirectoryEntry> fsd;
    std::string                      folder = epi::PathAppend(game_directory, "autoload");

    if (!ReadDirectory(fsd, folder, "*.*"))
    {
        LogWarning("Failed to read %s directory!\n", folder.c_str());
    }
    else
    {
        for (size_t i = 0; i < fsd.size(); i++)
        {
            if (!fsd[i].is_dir)
                AddSingleCommandLineFile(fsd[i].name, true);
        }
    }
    fsd.clear();
    folder = epi::PathAppend(folder, game_base);
    if (!ReadDirectory(fsd, folder, "*.*"))
    {
        LogWarning("Failed to read %s directory!\n", folder.c_str());
    }
    else
    {
        for (size_t i = 0; i < fsd.size(); i++)
        {
            if (!fsd[i].is_dir)
                AddSingleCommandLineFile(fsd[i].name, true);
        }
    }
    fsd.clear();

    // Check if autoload folder stuff is in home_directory as well, make the
    // folder/subfolder if they don't exist (in home_directory only)
    folder = epi::PathAppend(home_directory, "autoload");
    if (!epi::IsDirectory(folder))
        epi::MakeDirectory(folder);

    if (!ReadDirectory(fsd, folder, "*.*"))
    {
        LogWarning("Failed to read %s directory!\n", folder.c_str());
    }
    else
    {
        for (size_t i = 0; i < fsd.size(); i++)
        {
            if (!fsd[i].is_dir)
                AddSingleCommandLineFile(fsd[i].name, true);
        }
    }
    fsd.clear();
    folder = epi::PathAppend(folder, game_base);
    if (!epi::IsDirectory(folder))
        epi::MakeDirectory(folder);
    if (!ReadDirectory(fsd, folder, "*.*"))
    {
        LogWarning("Failed to read %s directory!\n", folder.c_str());
    }
    else
    {
        for (size_t i = 0; i < fsd.size(); i++)
        {
            if (!fsd[i].is_dir)
                AddSingleCommandLineFile(fsd[i].name, true);
        }
    }
}

static void InitializeDdf(void)
{
    LogDebug("- Initialising DDF\n");

    DDF_Init();
}

void EdgeShutdown(void)
{
    StopMusic();

    // Pause to allow sounds to finish
    for (int loop = 0; loop < 30; loop++)
    {
        SoundTicker();
        SleepForMilliseconds(50);
    }

    LevelShutdown();
    SoundShutdown();
    RendererShutdown();
    NetworkShutdown();
}

static void EdgeStartup(void)
{
    ConsoleInit();

    // -AJA- 2000/02/02: initialise global gameflags to defaults
    global_flags = default_game_flags;

    InitializeDirectories();

    // Version check ?
    if (ArgumentFind("version") > 0)
    {
        // -AJA- using FatalError here, since LogPrint crashes this early on
        FatalError("\n%s version is %s\n", application_name.c_str(), edge_version.c_str());
    }

    SetupLogAndDebugFiles();

    PurgeCache();

    ShowDateAndVersion();

    ConfigurationLoadDefaults();

    ConsoleHandleProgramArguments();
    SetGlobalVariables();

    DoSystemStartup();

    InitializeDdf();
    IdentifyVersion();
    AddBasePack();
    AddAutoload();
    AddCommandLineFiles();
    CheckTurbo();

    InitializeTriggerScripts();
    ProcessMultipleFiles();
    DDF_ParseEverything();
    // Must be done after WAD and DDF loading to check for potential
    // overrides of lump-specific image/sound/DDF defines
    DoPackSubstitutions();
    StartupMusic(); // Must be done after all files loaded to locate
                    // appropriate GENMIDI lump
    InitializePalette();

    DDF_CleanUp();
    SetLanguage();
    ReadUmapinfoLumps();

    InitializeFlats();
    InitializeTextures();
    CreateUserImages();
    PickLoadingScreen();
    PickMenuBackdrop();

    HudInit();
    ConsoleStart();
    ConsoleCreateQuitScreen();
    SpecialWadVerify();
    BuildXglNodes();
    ShowNotice();

    SaveSystemInitialize();
    PrecacheSounds();
    InitializeSprites();
    ProcessTxHiNamespaces();
    InitializeModels();

    MenuInitialize();
    RendererStartup();
    PlayerStateInit();
    InitializeSwitchList();
    InitializeAnimations();
    SoundInitialize();
    NetworkInitialize();
    CheatInitialize();
    if (LuaUseLuaHud())
    {
        LuaInit();
        LuaLoadScripts();
    }
    else
    {
        CoalInitialize();
        CoalLoadScripts();
    }
}

static void InitialState(void)
{
    LogDebug("- Setting up Initial State...\n");

    std::string ps;

    // do loadgames first, as they contain all of the
    // necessary state already (in the savegame).

    if (ArgumentFind("playdemo") > 0 || ArgumentFind("timedemo") > 0 || ArgumentFind("record") > 0)
    {
        FatalError("Demos are no longer supported\n");
    }

    ps = ArgumentValue("loadgame");
    if (!ps.empty())
    {
        GameDeferredLoadGame(atoi(ps.c_str()));
        return;
    }

    bool warp = false;

    // get skill / episode / map from parms
    std::string warp_map;
    SkillLevel  warp_skill      = kSkillMedium;
    int         warp_deathmatch = 0;

    int bots = 0;

    ps = ArgumentValue("bots");
    if (!ps.empty())
        bots = atoi(ps.c_str());

    ps = ArgumentValue("warp");
    if (!ps.empty())
    {
        warp     = true;
        warp_map = ps;
    }

    // -KM- 1999/01/29 Use correct skill: 1 is easiest, not 0
    ps = ArgumentValue("skill");
    if (!ps.empty())
    {
        warp       = true;
        warp_skill = (SkillLevel)(atoi(ps.c_str()) - 1);
    }

    // deathmatch check...
    int pp = ArgumentFind("deathmatch");
    if (pp > 0)
    {
        warp_deathmatch = 1;

        if (pp + 1 < int(program_argument_list.size()) && !ArgumentIsOption(pp + 1))
            warp_deathmatch = HMM_MAX(1, atoi(program_argument_list[pp + 1].c_str()));

        warp = true;
    }
    else if (ArgumentFind("altdeath") > 0)
    {
        warp_deathmatch = 2;

        warp = true;
    }

    // start the appropriate game based on parms
    if (!warp)
    {
        LogDebug("- Startup: showing title screen.\n");
        StartTitle();
        startup_progress.Clear();
        return;
    }

    NewGameParameters params;

    params.skill_      = warp_skill;
    params.deathmatch_ = warp_deathmatch;
    params.level_skip_ = true;

    if (warp_map.length() > 0)
        params.map_ = GameLookupMap(warp_map.c_str());
    else
        params.map_ = GameLookupMap("1");

    if (!params.map_)
        FatalError("-warp: no such level '%s'\n", warp_map.c_str());

    EPI_ASSERT(GameMapExists(params.map_));
    EPI_ASSERT(params.map_->episode_);

    params.random_seed_ = PureRandomNumber();

    params.SinglePlayer(bots);

    GameDeferredNewGame(params);
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
void EdgeMain(int argc, const char **argv)
{
    // Seed RandomByte RNG
    RandomInit();

    // Implemented here - since we need to bring the memory manager up first
    // -ACB- 2004/05/31
    ArgumentParse(argc, argv);

    EdgeStartup();

    InitialState();

    ConsoleMessageColor(SG_YELLOW_RGBA32);
    LogPrint("%s v%s initialisation complete.\n", application_name.c_str(), edge_version.c_str());

    LogDebug("- Entering game loop...\n");

#ifndef EDGE_WEB
    while (!(app_state & kApplicationPendingQuit))
    {
        // We always do this once here, although the engine may
        // makes in own calls to keep on top of the event processing
        ControlGetEvents();

        if (app_state & kApplicationActive)
            EdgeTicker();
        else if (!busy_wait.d_)
        {
            SleepForMilliseconds(5);
        }
    }
#else
    return;
#endif
}

//
// Called when this application has lost focus (i.e. an ALT+TAB event)
//
void EdgeIdle(void)
{
    EventReleaseAllKeys();
}

//
// This Function is called for a single loop in the system.
//
// -ACB- 1999/09/24 Written
// -ACB- 2004/05/31 Namespace'd
//
void EdgeTicker(void)
{
    EDGE_ZoneScoped;

    GameBigStuff();

    // Update display, next frame, with current state.
    EdgeDisplay();

    // this also runs the responder chain via EventProcessEvents
    int counts = NetworkTryRunTicCommands();

    // run the tics
    for (; counts > 0; counts--)
    {
        // run a step in the physics (etc)
        GameTicker();

        // user interface stuff (skull anim, etc)
        ConsoleTicker();
        MenuTicker();
        SoundTicker();
        MusicTicker();

        // process mouse and keyboard events
        NetworkUpdate();
    }
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
