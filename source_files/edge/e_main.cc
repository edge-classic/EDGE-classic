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
// -MH- 1998/07/02 "shootupdown" --> "true3dgameplay"
// -MH- 1998/08/19 added up/down movement variables
//


#include "epi_sdl.h"
#include "e_main.h"
#include "i_defs_gl.h"
#include "i_movie.h"
#include "i_system.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <algorithm>
#include <vector>

#include "file.h"
#include "filesystem.h"
#include "str_util.h"
#include "str_compare.h"
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
#include "m_argv.h"
#include "m_bbox.h"
#include "m_cheat.h"
#include "m_misc.h"
#include "m_menu.h"
#include "m_random.h"
#include "n_network.h"
#include "p_setup.h"
#include "p_spec.h"
#include "rad_trig.h"
#include "r_gldefs.h"
#include "r_misc.h"
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
#include "script/compat/lua_compat.h"
#include "edge_profiling.h"

extern cvar_c r_doubleframes;
extern cvar_c n_busywait;

extern cvar_c v_gamma;

ECFrameStats ecframe_stats;

// Application active?
int app_state = APP_STATE_ACTIVE;

bool singletics = false; // debug flag to cancel adaptiveness

// -ES- 2000/02/13 Takes screenshot every screenshot_rate tics.
// Must be used in conjunction with singletics.
static int screenshot_rate;

// For screenies...
bool m_screenshot_required = false;
bool need_save_screenshot  = false;

bool custom_MenuMain       = false;
bool custom_MenuEpisode    = false;
bool custom_MenuDifficulty = false;

FILE *logfile   = nullptr;
FILE *debugfile = nullptr;

gameflags_t default_gameflags = {
    false, // nomonsters
    false, // fastparm

    false, // respawn
    false, // res_respawn
    false, // item respawn

    false, // true 3d gameplay
    8,     // gravity
    false, // more blood

    true,  // jump
    true,  // crouch
    true,  // mlook
    AA_ON, // autoaim

    true,  // cheats
    true,  // have_extra
    false, // limit_zoom

    true,  // kicking
    true,  // weapon_switch
    true,  // pass_missile
    false, // team_damage
};

// -KM- 1998/12/16 These flags are the users prefs and are copied to
//   gameflags when a new level is started.
// -AJA- 2000/02/02: Removed initialisation (done in code using
//       `default_gameflags').

gameflags_t global_flags;

int newnmrespawn = 0;

bool swapstereo     = false;
bool mus_pause_stop = false;
bool png_scrshots   = false;
bool autoquickload  = false;

std::string brandingfile;
std::string cfgfile;
std::string epkfile;
std::string           game_base;

std::string cache_dir;
std::string game_dir;
std::string home_dir;
std::string save_dir;
std::string shot_dir;

// not using DEF_CVAR here since var name != cvar name
cvar_c m_language("language", "ENGLISH", CVAR_ARCHIVE);

DEF_CVAR(logfilename, "edge-classic.log", CVAR_NO_RESET)
DEF_CVAR(configfilename, "edge-classic.cfg", CVAR_NO_RESET)
DEF_CVAR(debugfilename, "debug.txt", CVAR_NO_RESET)
DEF_CVAR(windowtitle, "EDGE-Classic", CVAR_NO_RESET)
DEF_CVAR(edgeversion, "1.37", CVAR_NO_RESET)
DEF_CVAR(orgname, "EDGE Team", CVAR_NO_RESET)
DEF_CVAR(appname, "EDGE-Classic", CVAR_NO_RESET)
DEF_CVAR(homepage, "https://edge-classic.github.io", CVAR_NO_RESET)

DEF_CVAR_CLAMPED(r_overlay, "0", CVAR_ARCHIVE, 0, 6)

DEF_CVAR_CLAMPED(r_titlescaling, "0", CVAR_ARCHIVE, 0, 1)

DEF_CVAR(g_aggression, "0", CVAR_ARCHIVE)

DEF_CVAR(ddf_strict, "0", CVAR_ARCHIVE)
DEF_CVAR(ddf_lax, "0", CVAR_ARCHIVE)
DEF_CVAR(ddf_quiet, "0", CVAR_ARCHIVE)

DEF_CVAR(skip_intros, "0", CVAR_ARCHIVE)

static const image_c *loading_image = nullptr;
const image_c        *menu_backdrop = nullptr;

static void E_TitleDrawer(void);

class startup_progress_c
{
  private:
    std::vector<std::string> startup_messages;

  public:
    startup_progress_c()
    {
    }

    ~startup_progress_c()
    {
    }

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
            if (r_titlescaling.d) // Fill Border
            {
                if (!loading_image->blurred_version)
                    W_ImageStoreBlurred(loading_image, 0.75f);
                HUD_StretchImage(-320, -200, 960, 600, loading_image->blurred_version, 0, 0);
            }
            HUD_DrawImageTitleWS(loading_image);
            HUD_SolidBox(25, 25, 295, 175, SG_BLACK_RGBA32);
        }
        int y = 26;
        for (int i = 0; i < (int)startup_messages.size(); i++)
        {
            if (startup_messages[i].size() > 32)
                HUD_DrawText(26, y, startup_messages[i].substr(0, 29).append("...").c_str());
            else
                HUD_DrawText(26, y, startup_messages[i].c_str());
            y += 10;
        }

        if (!hud_overlays.at(r_overlay.d).empty())
        {
            const image_c *overlay = W_ImageLookup(hud_overlays.at(r_overlay.d).c_str(), kImageNamespaceGraphic, ILF_Null);
            if (overlay)
                HUD_RawImage(0, 0, SCREENWIDTH, SCREENHEIGHT, overlay, 0, 0, SCREENWIDTH / IM_WIDTH(overlay),
                             SCREENHEIGHT / IM_HEIGHT(overlay));
        }

        if (v_gamma.f < 0)
        {
            int col = (1.0f + v_gamma.f) * 255;
            glEnable(GL_BLEND);
            glBlendFunc(GL_ZERO, GL_SRC_COLOR);
            HUD_SolidBox(hud_x_left, 0, hud_x_right, 200, epi::MakeRGBA(col, col, col));
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDisable(GL_BLEND);
        }
        else if (v_gamma.f > 0)
        {
            int col = v_gamma.f * 255;
            glEnable(GL_BLEND);
            glBlendFunc(GL_DST_COLOR, GL_ONE);
            HUD_SolidBox(hud_x_left, 0, hud_x_right, 200, epi::MakeRGBA(col, col, col));
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
    int         p;
    std::string s;

    // Screen Resolution Check...
    if (argv::Find("borderless") > 0)
        DISPLAYMODE = 2;
    else if (argv::Find("fullscreen") > 0)
        DISPLAYMODE = 1;
    else if (argv::Find("windowed") > 0)
        DISPLAYMODE = 0;

    s = argv::Value("width");
    if (!s.empty())
    {
        if (DISPLAYMODE == 2)
            I_Warning("Current display mode set to borderless fullscreen. Provided width of %d will be ignored!\n",
                      atoi(s.c_str()));
        else
            SCREENWIDTH = atoi(s.c_str());
    }

    s = argv::Value("height");
    if (!s.empty())
    {
        if (DISPLAYMODE == 2)
            I_Warning("Current display mode set to borderless fullscreen. Provided height of %d will be ignored!\n",
                      atoi(s.c_str()));
        else
            SCREENHEIGHT = atoi(s.c_str());
    }

    p = argv::Find("res");
    if (p > 0 && p + 2 < int(argv::list.size()) && !argv::IsOption(p + 1) && !argv::IsOption(p + 2))
    {
        if (DISPLAYMODE == 2)
            I_Warning(
                "Current display mode set to borderless fullscreen. Provided resolution of %dx%d will be ignored!\n",
                atoi(argv::list[p + 1].c_str()), atoi(argv::list[p + 2].c_str()));
        else
        {
            SCREENWIDTH  = atoi(argv::list[p + 1].c_str());
            SCREENHEIGHT = atoi(argv::list[p + 2].c_str());
        }
    }

    // Bits per pixel check....
    s = argv::Value("bpp");
    if (!s.empty())
    {
        SCREENBITS = atoi(s.c_str());

        if (SCREENBITS <= 4) // backwards compat
            SCREENBITS *= 8;
    }

    // restrict depth to allowable values
    if (SCREENBITS < 15)
        SCREENBITS = 15;
    if (SCREENBITS > 32)
        SCREENBITS = 32;

    // If borderless fullscreen mode, override any provided dimensions so I_StartupGraphics will scale to native res
    if (DISPLAYMODE == 2)
    {
        SCREENWIDTH  = 100000;
        SCREENHEIGHT = 100000;
    }

    // sprite kludge (TrueBSP)
    p = argv::Find("spritekludge");
    if (p > 0)
    {
        if (p + 1 < int(argv::list.size()) && !argv::IsOption(p + 1))
            sprite_kludge = atoi(argv::list[p + 1].c_str());

        if (!sprite_kludge)
            sprite_kludge = 1;
    }

    s = argv::Value("screenshot");
    if (!s.empty())
    {
        screenshot_rate = atoi(s.c_str());
        singletics      = true;
    }

    // -AJA- 1999/10/18: Reworked these with argv::CheckBooleanParm
    argv::CheckBooleanParm("rotate_map", &rotate_map, false);
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

    argv::CheckBooleanParm("automap_keydoor_blink", &automap_keydoor_blink, false);

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
            global_flags.respawn     = true;
        }
        else if (argv::Find("respawn") > 0)
        {
            global_flags.respawn = true;
        }
    }

    // check for strict and no-warning options
    argv::CheckBooleanCVar("strict", &ddf_strict, false);
    argv::CheckBooleanCVar("lax", &ddf_lax, false);
    argv::CheckBooleanCVar("warn", &ddf_quiet, true);

    strict_errors = ddf_strict.d ? true : false;
    lax_errors    = ddf_lax.d ? true : false;
    no_warnings   = ddf_quiet.d ? true : false;
}

//
// SetLanguage
//
void SetLanguage(void)
{
    std::string want_lang = argv::Value("lang");
    if (!want_lang.empty())
        m_language = want_lang;

    if (language.Select(m_language.c_str()))
        return;

    I_Warning("Invalid language: '%s'\n", m_language.c_str());

    if (!language.Select(0))
        I_Error("Unable to select any language!");

    m_language = language.GetName();
}

//
// SpecialWadVerify
//
static void SpecialWadVerify(void)
{
    E_ProgressMessage("Verifying EDGE_DEFS version...");

    epi::File *data = W_OpenPackFile("/version.txt");

    if (!data)
        I_Error("Version file not found. Get edge_defs.epk at https://github.com/edge-classic/EDGE-classic");

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

    I_Printf("EDGE_DEFS.EPK version %1.2f found.\n", real_ver);

    if (real_ver < edgeversion.f)
    {
        I_Error("EDGE_DEFS.EPK is an older version (got %1.2f, expected %1.2f)\n", real_ver, edgeversion.f);
    }
    else if (real_ver > edgeversion.f)
    {
        I_Warning("EDGE_DEFS.EPK is a newer version (got %1.2f, expected %1.2f)\n", real_ver, edgeversion.f);
    }
}

//
// ShowNotice
//
static void ShowNotice(void)
{
    CON_MessageColor(epi::MakeRGBA(64, 192, 255));

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
    static const image_c *pause_image = nullptr;

    if (!pause_image)
        pause_image = W_ImageLookup("M_PAUSE");

    // make sure image is centered horizontally

    float w = IM_WIDTH(pause_image);
    float h = IM_HEIGHT(pause_image);

    float x = 160 - w / 2;
    float y = 10;

    HUD_StretchImage(x, y, w, h, pause_image, 0.0, 0.0);
}

wipetype_e wipe_method = WIPE_Melt;

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
    EDGE_ZoneScoped;

    if (nodrawers)
        return; // for comparative timing / profiling

    // Start the frame - should we need to.
    I_StartFrame();

    HUD_FrameSetup();

    switch (gamestate)
    {
    case GS_LEVEL:
        R_PaletteStuff();

        if (LUA_UseLuaHud())
            LUA_RunHud();            
        else
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
        need_wipe      = false;
        wipe_gl_active = true;

        RGL_InitWipe(wipe_method);
    }

    if (paused)
        M_DisplayPause();

    // menus go directly to the screen
    M_Drawer(); // menu is drawn even on top of everything (except console)

    // process mouse and keyboard events
    N_NetUpdate();

    CON_Drawer();

    if (!hud_overlays.at(r_overlay.d).empty())
    {
        const image_c *overlay = W_ImageLookup(hud_overlays.at(r_overlay.d).c_str(), kImageNamespaceGraphic, ILF_Null);
        if (overlay)
            HUD_RawImage(0, 0, SCREENWIDTH, SCREENHEIGHT, overlay, 0, 0, SCREENWIDTH / IM_WIDTH(overlay),
                         SCREENHEIGHT / IM_HEIGHT(overlay));
    }

    if (v_gamma.f < 0)
    {
        int col = (1.0f + v_gamma.f) * 255;
        glEnable(GL_BLEND);
        glBlendFunc(GL_ZERO, GL_SRC_COLOR);
        HUD_SolidBox(hud_x_left, 0, hud_x_right, 200, epi::MakeRGBA(col, col, col));
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_BLEND);
    }
    else if (v_gamma.f > 0)
    {
        int col = v_gamma.f * 255;
        glEnable(GL_BLEND);
        glBlendFunc(GL_DST_COLOR, GL_ONE);
        HUD_SolidBox(hud_x_left, 0, hud_x_right, 200, epi::MakeRGBA(col, col, col));
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

    I_FinishFrame(); // page flip or blit buffer
}

//
//  TITLE LOOP
//
static int title_game;
static int title_pic;
static int title_countdown;

static const image_c *title_image = nullptr;

static void E_TitleDrawer(void)
{
    if (title_image)
    {
        if (r_titlescaling.d) // Fill Border
        {
            if (!title_image->blurred_version)
                W_ImageStoreBlurred(title_image, 0.75f);
            HUD_StretchImage(-320, -200, 960, 600, title_image->blurred_version, 0, 0);
        }
        HUD_DrawImageTitleWS(title_image);
    }
    else
    {
        HUD_SolidBox(0, 0, 320, 200, SG_BLACK_RGBA32);
    }
}

//
// This cycles through the title sequences.
// -KM- 1998/12/16 Fixed for DDF.
//
void E_PickLoadingScreen(void)
{
    // force pic overflow -> first available titlepic
    title_game = gamedefs.size() - 1;
    title_pic  = 29999;

    // prevent an infinite loop
    for (int loop = 0; loop < 100; loop++)
    {
        GameDefinition *g = gamedefs[title_game];
        SYS_ASSERT(g);

        if (title_pic >= (int)g->titlepics_.size())
        {
            title_game = (title_game + 1) % (int)gamedefs.size();
            title_pic  = 0;
            continue;
        }

        // ignore non-existing episodes.  Doesn't include title-only ones
        // like [EDGE].
        if (title_pic == 0 && g->firstmap_ != "" && W_CheckNumForName(g->firstmap_.c_str()) == -1)
        {
            title_game = (title_game + 1) % gamedefs.size();
            title_pic  = 0;
            continue;
        }

        // ignore non-existing images
        loading_image = W_ImageLookup(g->titlepics_[title_pic].c_str(), kImageNamespaceGraphic, ILF_Null);

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
void E_PickMenuScreen(void)
{
    // force pic overflow -> first available titlepic
    title_game = gamedefs.size() - 1;
    title_pic  = 29999;

    // prevent an infinite loop
    for (int loop = 0; loop < 100; loop++)
    {
        GameDefinition *g = gamedefs[title_game];
        SYS_ASSERT(g);

        if (title_pic >= (int)g->titlepics_.size())
        {
            title_game = (title_game + 1) % (int)gamedefs.size();
            title_pic  = 0;
            continue;
        }

        // ignore non-existing episodes.
        if (title_pic == 0 && (g->firstmap_ == "" || W_CheckNumForName(g->firstmap_.c_str()) == -1))
        {
            title_game = (title_game + 1) % gamedefs.size();
            title_pic  = 0;
            continue;
        }

        // ignore non-existing images
        const image_c *menu_image = W_ImageLookup(g->titlepics_[title_pic].c_str(), kImageNamespaceGraphic, ILF_Null);

        if (!menu_image)
        {
            title_pic++;
            continue;
        }

        // found one !!
        title_game                   = gamedefs.size() - 1;
        title_pic                    = 29999;
        image_c *new_backdrop        = new image_c;
        new_backdrop->name           = menu_image->name;
        new_backdrop->actual_h       = menu_image->actual_h;
        new_backdrop->actual_w       = menu_image->actual_w;
        new_backdrop->cache          = menu_image->cache;
        new_backdrop->is_empty       = menu_image->is_empty;
        new_backdrop->is_font        = menu_image->is_font;
        new_backdrop->liquid_type    = menu_image->liquid_type;
        new_backdrop->offset_x       = menu_image->offset_x;
        new_backdrop->offset_y       = menu_image->offset_y;
        new_backdrop->opacity        = menu_image->opacity;
        new_backdrop->ratio_h        = menu_image->ratio_h;
        new_backdrop->ratio_w        = menu_image->ratio_w;
        new_backdrop->scale_x        = menu_image->scale_x;
        new_backdrop->scale_y        = menu_image->scale_y;
        new_backdrop->source         = menu_image->source;
        new_backdrop->source_palette = menu_image->source_palette;
        new_backdrop->source_type    = menu_image->source_type;
        new_backdrop->total_h        = menu_image->total_h;
        new_backdrop->total_w        = menu_image->total_w;
        new_backdrop->anim.cur       = new_backdrop;
        new_backdrop->grayscale      = true;
        menu_backdrop                = new_backdrop;
        return;
    }

    // if we get here just use the loading image if it exists
    title_game = gamedefs.size() - 1;
    title_pic  = 29999;
    if (loading_image)
    {
        image_c *new_backdrop        = new image_c;
        new_backdrop->name           = loading_image->name;
        new_backdrop->actual_h       = loading_image->actual_h;
        new_backdrop->actual_w       = loading_image->actual_w;
        new_backdrop->cache          = loading_image->cache;
        new_backdrop->is_empty       = loading_image->is_empty;
        new_backdrop->is_font        = loading_image->is_font;
        new_backdrop->liquid_type    = loading_image->liquid_type;
        new_backdrop->offset_x       = loading_image->offset_x;
        new_backdrop->offset_y       = loading_image->offset_y;
        new_backdrop->opacity        = loading_image->opacity;
        new_backdrop->ratio_h        = loading_image->ratio_h;
        new_backdrop->ratio_w        = loading_image->ratio_w;
        new_backdrop->scale_x        = loading_image->scale_x;
        new_backdrop->scale_y        = loading_image->scale_y;
        new_backdrop->source         = loading_image->source;
        new_backdrop->source_palette = loading_image->source_palette;
        new_backdrop->source_type    = loading_image->source_type;
        new_backdrop->total_h        = loading_image->total_h;
        new_backdrop->total_w        = loading_image->total_w;
        new_backdrop->anim.cur       = new_backdrop;
        new_backdrop->grayscale      = true;
        menu_backdrop                = new_backdrop;
    }
    else
        menu_backdrop = nullptr;
}

//
// This cycles through the title sequences.
// -KM- 1998/12/16 Fixed for DDF.
//
void E_AdvanceTitle(void)
{
    title_pic++;

    // prevent an infinite loop
    for (int loop = 0; loop < 100; loop++)
    {
        GameDefinition *g = gamedefs[title_game];
        SYS_ASSERT(g);

        // Only play title movies once
        if (!g->titlemovie_.empty() && !g->movie_played_)
        {
            if (skip_intros.d)
                g->movie_played_ = true;
            else
            {
                E_PlayMovie(g->titlemovie_);
                g->movie_played_ = true;
            }
            continue;
        }

        if (title_pic >= (int)g->titlepics_.size())
        {
            title_game = (title_game + 1) % (int)gamedefs.size();
            title_pic  = 0;
            continue;
        }

        // ignore non-existing episodes.  Doesn't include title-only ones
        // like [EDGE].
        if (title_pic == 0 && g->firstmap_ != "" && W_CheckNumForName(g->firstmap_.c_str()) == -1)
        {
            title_game = (title_game + 1) % gamedefs.size();
            title_pic  = 0;
            continue;
        }

        // ignore non-existing images
        title_image = W_ImageLookup(g->titlepics_[title_pic].c_str(), kImageNamespaceGraphic, ILF_Null);

        if (!title_image)
        {
            title_pic++;
            continue;
        }

        // found one !!
        if (title_pic == 0 && g->titlemusic_ > 0)
            S_ChangeMusic(g->titlemusic_, false);

        title_countdown = g->titletics_ * (r_doubleframes.d ? 2 : 1);
        return;
    }

    // not found

    title_image     = nullptr;
    title_countdown = kTicRate * (r_doubleframes.d ? 2 : 1);
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
    // Get the App Directory from parameter.

    // Note: This might need adjusting for Apple
    std::string s = SDL_GetBasePath();

    game_dir = s;
    s        = argv::Value("game");
    if (!s.empty())
        game_dir = s;

    brandingfile = epi::PathAppend(game_dir, EDGEBRANDINGFILE);

    M_LoadBranding();

    // add parameter file "appdir/parms" if it exists.
    std::string parms = epi::PathAppend(game_dir, "parms");

    if (epi::TestFileAccess(parms))
    {
        // Insert it right after the app parameter
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
        cfgfile = epi::PathAppend(game_dir, configfilename.s);
        if (epi::TestFileAccess(cfgfile) || argv::Find("portable") > 0)
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

#ifdef _WIN32
    if (home_dir.empty())
        home_dir = SDL_GetPrefPath(nullptr, appname.c_str());
#else
    if (home_dir.empty())
        home_dir = SDL_GetPrefPath(orgname.c_str(), appname.c_str());
#endif

    if (!epi::IsDirectory(home_dir))
    {
        if (!epi::MakeDirectory(home_dir))
            I_Error("InitDirectories: Could not create directory at %s!\n", home_dir.c_str());
    }

    if (cfgfile.empty())
        cfgfile = epi::PathAppend(home_dir, configfilename.s);

    // edge_defs.epk file
    s = argv::Value("defs");
    if (!s.empty())
    {
        epkfile = s;
    }
    else
    {
        std::string defs_test = epi::PathAppend(game_dir, "edge_defs");
        if (epi::IsDirectory(defs_test))
            epkfile = defs_test;
        else
            epkfile.append(".epk");
    }

    // cache directory
    cache_dir = epi::PathAppend(home_dir, CACHEDIR);

    if (!epi::IsDirectory(cache_dir))
        epi::MakeDirectory(cache_dir);

    // savegame directory
    save_dir = epi::PathAppend(home_dir, SAVEGAMEDIR);

    if (!epi::IsDirectory(save_dir))
        epi::MakeDirectory(save_dir);

    SV_ClearSlot("current");

    // screenshot directory
    shot_dir = epi::PathAppend(home_dir, SCRNSHOTDIR);

    if (!epi::IsDirectory(shot_dir))
        epi::MakeDirectory(shot_dir);
}

// Get rid of legacy GWA/HWA files

static void PurgeCache(void)
{
    std::vector<epi::DirectoryEntry> fsd;

    if (!ReadDirectory(fsd, cache_dir, "*.*"))
    {
        I_Error("PurgeCache: Failed to read '%s' directory!\n", cache_dir.c_str());
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

// If a valid IWAD (or EDGEGAME) is found, return the appopriate game_base string ("doom2", "heretic", etc)
static int CheckPackForGameFiles(std::string check_pack, filekind_e check_kind)
{
    data_file_c *check_pack_df = new data_file_c(check_pack, check_kind);
    SYS_ASSERT(check_pack_df);
    Pack_PopulateOnly(check_pack_df);
    if (Pack_FindStem(check_pack_df->pack, "EDGEGAME"))
    {
        delete check_pack_df;
        return 0; // Custom game index value in game_checker vector
    }
    else
    {
        int check_base = Pack_CheckForIWADs(check_pack_df);
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
        W_AddFilename(epkfile, FLKIND_EFolder);
    else
    {
        if (!epi::TestFileAccess(epkfile))
            I_Error("IdentifyVersion: Could not find required %s.%s!\n", REQUIREDEPK, "epk");
        W_AddFilename(epkfile, FLKIND_EEPK);
    }

    I_Debugf("- Identify Version\n");

    // Check -iwad parameter, find out if it is the IWADs directory
    std::string              iwad_par;
    std::string              iwad_file;
    std::string              iwad_dir;
    std::vector<std::string> iwad_dir_vector;

    std::string s = argv::Value("iwad");

    iwad_par = s;

    if (!iwad_par.empty())
    {
        // Treat directories passed via -iwad as a pack file and check accordingly
        if (epi::IsDirectory(iwad_par))
        {
            int game_check = CheckPackForGameFiles(iwad_par, FLKIND_IFolder);
            if (game_check < 0)
                I_Error("Folder %s passed via -iwad parameter, but no IWAD or EDGEGAME file detected!\n",
                        iwad_par.c_str());
            else
            {
                game_base = game_checker[game_check].base;
                W_AddFilename(iwad_par, FLKIND_IFolder);
                I_Debugf("GAME BASE = [%s]\n", game_base.c_str());
                return;
            }
        }
    }
    else
    {
        // In the absence of the -iwad parameter, check files/dirs added via drag-and-drop for valid IWADs
        // Remove them from the arg list if they are valid to avoid them potentially being added as PWADs
        std::vector<SDL_MessageBoxButtonData> game_buttons;
        std::unordered_map<int, std::pair<std::string, filekind_e>> game_paths;
        for (size_t p = 1; p < argv::list.size() && !argv::IsOption(p); p++)
        {
            std::string dnd = argv::list[p];
            int test_index = -1;
            if (epi::IsDirectory(dnd))
            {
                test_index = CheckPackForGameFiles(dnd, FLKIND_IFolder);
                if (test_index >= 0)
                {
                    if (!game_paths.count(test_index))
                    {
                        game_paths.try_emplace(test_index, std::make_pair(dnd, FLKIND_IFolder));
                        SDL_MessageBoxButtonData temp_button;
                        temp_button.buttonid = test_index;
                        temp_button.text = game_checker[test_index].display_name.c_str();
                        game_buttons.push_back(temp_button);
                    }
                    argv::list.erase(argv::list.begin()+p--);
                }
            }
            else if (epi::StringCaseCompareASCII(epi::GetExtension(dnd), ".epk") == 0)
            {
                test_index = CheckPackForGameFiles(dnd, FLKIND_IPK);
                if (test_index >= 0)
                {
                    if (!game_paths.count(test_index))
                    {
                        game_paths.try_emplace(test_index, std::make_pair(dnd, FLKIND_IPK));
                        SDL_MessageBoxButtonData temp_button;
                        temp_button.buttonid = test_index;
                        temp_button.text = game_checker[test_index].display_name.c_str();
                        game_buttons.push_back(temp_button);
                    }
                    argv::list.erase(argv::list.begin()+p--);
                }  
            }
            else if (epi::StringCaseCompareASCII(epi::GetExtension(dnd), ".wad") == 0)
            {
                epi::File *game_test =
                    epi::FileOpen(dnd, epi::kFileAccessRead | epi::kFileAccessBinary);
                test_index  = W_CheckForUniqueLumps(game_test);
                delete game_test;
                if (test_index >= 0)
                {
                    if (!game_paths.count(test_index))
                    {
                        game_paths.try_emplace(test_index, std::make_pair(dnd, FLKIND_IWad));
                        SDL_MessageBoxButtonData temp_button;
                        temp_button.buttonid = test_index;
                        temp_button.text = game_checker[test_index].display_name.c_str();
                        game_buttons.push_back(temp_button);
                    }
                    argv::list.erase(argv::list.begin()+p--);
                }
            }
        }
        if (game_paths.size() == 1)
        {
            auto selected_game = game_paths.begin();
            game_base = game_checker[selected_game->first].base;
            W_AddFilename(selected_game->second.first, selected_game->second.second);
            I_Debugf("GAME BASE = [%s]\n", game_base.c_str());
            return;
        }
        else if (!game_paths.empty())
        {
            SYS_ASSERT(game_paths.size() == game_buttons.size());
            SDL_MessageBoxData picker_data;
            SDL_memset(&picker_data, 0, sizeof(SDL_MessageBoxData));
            picker_data.title = "EDGE-Classic Game Selector";
            picker_data.message = "No game was specified, but EDGE-Classic found multiple valid game files. Please select one or press Escape to cancel.";
            picker_data.numbuttons = game_buttons.size();
            picker_data.buttons = game_buttons.data();
            int button_hit = 0;
            if (SDL_ShowMessageBox(&picker_data, &button_hit) != 0)
                I_Error("Error in game selection dialog!\n");
            else if (button_hit == -1)
                I_Error("Game selection cancelled.\n");
            else
            {
                game_base = game_checker[button_hit].base;
                auto selected_game = game_paths.at(button_hit);
                W_AddFilename(selected_game.first, selected_game.second);
                I_Debugf("GAME BASE = [%s]\n", game_base.c_str());
                return;
            }
        }
    }

    // If we haven't yet set the IWAD directory, then we check
    // the DOOMWADDIR environment variable
    check = SDL_getenv("DOOMWADDIR");
    if (check) s = check;

    if (!s.empty() && epi::IsDirectory(s))
        iwad_dir_vector.push_back(s);

    // Should the IWAD directory not be set by now, then we
    // use our standby option of the current directory.
    if (iwad_dir.empty())
        iwad_dir = "."; // should this be hardcoded to the game or home directory instead? - Dasho

    // Add DOOMWADPATH directories if they exist
    s.clear();
    check = SDL_getenv("DOOMWADPATH");
    if (check) s = check;

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
            // We will still be checking EPKs if needed; but by the numbers .wad is a good initial search
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

        // If we get here, try .epk and error out if we still can't access what was passed to us
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
                I_Error("IdentifyVersion: Unable to access specified '%s'", fn.c_str());
            }
            else
                I_Error("IdentifyVersion: Unable to access specified '%s'", fn.c_str());
        }

    foundindoomwadpath:

        int test_score = -1;

        if (epi::StringCaseCompareASCII(epi::GetExtension(iwad_file), ".wad") == 0)
        {
            epi::File *game_test = epi::FileOpen(iwad_file, epi::kFileAccessRead | epi::kFileAccessBinary);
            test_score              = W_CheckForUniqueLumps(game_test);
            delete game_test;
            if (test_score >= 0)
            {
                game_base = game_checker[test_score].base;
                W_AddFilename(iwad_file, FLKIND_IWad);
            }
            else
                I_Error("IdentifyVersion: Could not identify '%s' as a valid IWAD!\n", fn.c_str());
        }
        else
        {
            test_score = CheckPackForGameFiles(iwad_file, FLKIND_IPK);
            if (test_score >= 0)
            {
                game_base = game_checker[test_score].base;
                W_AddFilename(iwad_file, FLKIND_IPK);
            }
            else
                I_Error("IdentifyVersion: Could not identify '%s' as a valid IWAD!\n", fn.c_str());
        }
    }
    else
    {
        std::string location;

        std::vector<SDL_MessageBoxButtonData> game_buttons;
        std::unordered_map<int, std::pair<std::string, filekind_e>> game_paths;

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
            std::vector<epi::DirectoryEntry> fsd;

            if (!ReadDirectory(fsd, location, "*.wad"))
            {
                I_Debugf("IdentifyVersion: No WADs found in '%s' directory!\n", location.c_str());
            }
            else
            {
                for (size_t j = 0; j < fsd.size(); j++)
                {
                    if (!fsd[j].is_dir)
                    {
                        epi::File *game_test =
                            epi::FileOpen(fsd[j].name, epi::kFileAccessRead | epi::kFileAccessBinary);
                        int         test_score = W_CheckForUniqueLumps(game_test);
                        delete game_test;
                        if (test_score >= 0)
                        {
                            if (!game_paths.count(test_score))
                            {
                                game_paths.try_emplace(test_score, std::make_pair(fsd[j].name, FLKIND_IWad));
                                SDL_MessageBoxButtonData temp_button;
                                temp_button.buttonid = test_score;
                                temp_button.text = game_checker[test_score].display_name.c_str();
                                game_buttons.push_back(temp_button);
                            }
                        }
                    }
                }
            }
            if (!ReadDirectory(fsd, location, "*.epk"))
            {
                I_Debugf("IdentifyVersion: No EPKs found in '%s' directory!\n", location.c_str());
            }
            else
            {
                for (size_t j = 0; j < fsd.size(); j++)
                {
                    if (!fsd[j].is_dir)
                    {
                        int         test_score = CheckPackForGameFiles(fsd[j].name, FLKIND_IPK);
                        if (test_score >= 0)
                        {
                            if (!game_paths.count(test_score))
                            {
                                game_paths.try_emplace(test_score, std::make_pair(fsd[j].name, FLKIND_IPK));
                                SDL_MessageBoxButtonData temp_button;
                                temp_button.buttonid = test_score;
                                temp_button.text = game_checker[test_score].display_name.c_str();
                                game_buttons.push_back(temp_button);
                            }
                        }
                    }
                }
            }
        }

        // Separate check for DOOMWADPATH stuff if it exists - didn't want to mess with the existing stuff above

        if (!iwad_dir_vector.empty())
        {
            for (size_t i = 0; i < iwad_dir_vector.size(); i++)
            {
                location = iwad_dir_vector[i].c_str();

                std::vector<epi::DirectoryEntry> fsd;

                if (!ReadDirectory(fsd, location, "*.wad"))
                {
                    I_Debugf("IdentifyVersion: No WADs found in '%s' directory!\n", location.c_str());
                }
                else
                {
                    for (size_t j = 0; j < fsd.size(); j++)
                    {
                        if (!fsd[j].is_dir)
                        {
                            epi::File *game_test =
                            epi::FileOpen(fsd[j].name, epi::kFileAccessRead | epi::kFileAccessBinary);
                            int         test_score = W_CheckForUniqueLumps(game_test);
                            delete game_test;
                            if (test_score >= 0)
                            {
                                if (!game_paths.count(test_score))
                                {
                                    game_paths.try_emplace(test_score, std::make_pair(fsd[j].name, FLKIND_IWad));
                                    SDL_MessageBoxButtonData temp_button;
                                    temp_button.buttonid = test_score;
                                    temp_button.text = game_checker[test_score].display_name.c_str();
                                    game_buttons.push_back(temp_button);
                                }
                            }
                        }
                    }
                }
                if (!ReadDirectory(fsd, location, "*.epk"))
                {
                    I_Debugf("IdentifyVersion: No EPKs found in '%s' directory!\n", location.c_str());
                }
                else
                {
                    for (size_t j = 0; j < fsd.size(); j++)
                    {
                        if (!fsd[j].is_dir)
                        {
                            int         test_score = CheckPackForGameFiles(fsd[j].name, FLKIND_IPK);
                            if (test_score >= 0)
                            {
                                if (!game_paths.count(test_score))
                                {
                                    game_paths.try_emplace(test_score, std::make_pair(fsd[j].name, FLKIND_IPK));
                                    SDL_MessageBoxButtonData temp_button;
                                    temp_button.buttonid = test_score;
                                    temp_button.text = game_checker[test_score].display_name.c_str();
                                    game_buttons.push_back(temp_button);
                                }
                            }
                        }
                    }
                }
            }
        }

        if (game_paths.empty())
            I_Error("IdentifyVersion: No IWADs or standalone packs found!\n");
        else if (game_paths.size() == 1)
        {
            auto selected_game = game_paths.begin();
            game_base = game_checker[selected_game->first].base;
            W_AddFilename(selected_game->second.first, selected_game->second.second);
        }
        else
        {
            SYS_ASSERT(game_paths.size() == game_buttons.size());
            SDL_MessageBoxData picker_data;
            SDL_memset(&picker_data, 0, sizeof(SDL_MessageBoxData));
            picker_data.title = "EDGE-Classic Game Selector";
            picker_data.message = "No game was specified, but EDGE-Classic found multiple valid game files. Please select one or press Escape to cancel.";
            picker_data.numbuttons = game_buttons.size();
            picker_data.buttons = game_buttons.data();
            int button_hit = 0;
            if (SDL_ShowMessageBox(&picker_data, &button_hit) != 0)
                I_Error("Error in game selection dialog!\n");
            else if (button_hit == -1)
                I_Error("Game selection cancelled.\n");
            else
            {
                game_base = game_checker[button_hit].base;
                auto selected_game = game_paths.at(button_hit);
                W_AddFilename(selected_game.first, selected_game.second);
            }
        }
    }

    I_Debugf("GAME BASE = [%s]\n", game_base.c_str());
}

// Add game-specific base EPKs (widepix, skyboxes, etc) - Dasho
static void Add_Base(void)
{
    if (epi::StringCaseCompareASCII("CUSTOM", game_base) == 0)
        return; // Standalone EDGE IWADs/EPKs should already contain their necessary resources and definitions - Dasho
    std::string base_path = epi::PathAppend(game_dir, "edge_base");
    std::string base_wad  = game_base;
    epi::StringLowerASCII(base_wad);
    base_path = epi::PathAppend(base_path, base_wad);
    if (epi::IsDirectory(base_path))
        W_AddFilename(base_path, FLKIND_EFolder);
    else
    {
        epi::ReplaceExtension(base_path, ".epk");
        if (epi::TestFileAccess(base_path))
            W_AddFilename(base_path, FLKIND_EEPK);
        else
        {
            I_Error("%s not found for the %s IWAD! Check the /edge_base folder of your %s install!\n",
                epi::GetFilename(base_path).c_str(), game_base.c_str(), appname.c_str());
        }
    }
}

static void CheckTurbo(void)
{
    int turbo_scale = 100;

    int p = argv::Find("turbo");

    if (p > 0)
    {
        if (p + 1 < int(argv::list.size()) && !argv::IsOption(p + 1))
            turbo_scale = atoi(argv::list[p + 1].c_str());
        else
            turbo_scale = 200;

        if (turbo_scale < 10)
            turbo_scale = 10;
        if (turbo_scale > 400)
            turbo_scale = 400;

        CON_MessageLDF("TurboScale", turbo_scale);
    }

    E_SetTurboScale(turbo_scale);
}

static void ShowDateAndVersion(void)
{
    time_t cur_time;
    char   timebuf[100];

    time(&cur_time);
    strftime(timebuf, 99, "%I:%M %p on %d/%b/%Y", localtime(&cur_time));

    I_Debugf("[Log file created at %s]\n\n", timebuf);
    I_Debugf("[Debug file created at %s]\n\n", timebuf);

    I_Printf("%s v%s compiled on " __DATE__ " at " __TIME__ "\n", appname.c_str(), edgeversion.c_str());
    I_Printf("%s homepage is at %s\n", appname.c_str(), homepage.c_str());

    I_Printf("Executable path: '%s'\n", exe_path.c_str());

    argv::DebugDumpArgs();
}

static void SetupLogAndDebugFiles(void)
{
    // -AJA- 2003/11/08 The log file gets all CON_Printfs, I_Printfs,
    //                  I_Warnings and I_Errors.

    std::string log_fn = epi::PathAppend(home_dir, logfilename.s);
    std::string debug_fn = epi::PathAppend(home_dir, debugfilename.s);


    logfile   = nullptr;
    debugfile = nullptr;

    if (argv::Find("nolog") < 0)
    {
        logfile = epi::FileOpenRaw(log_fn, epi::kFileAccessWrite);

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
        debugfile = epi::FileOpenRaw(debug_fn, epi::kFileAccessWrite);

        if (!debugfile)
            I_Error("[E_Startup] Unable to create debugfile");
    }
}

static void AddSingleCmdLineFile(std::string name, bool ignore_unknown)
{
    if (epi::IsDirectory(name))
    {
        W_AddFilename(name, FLKIND_Folder);
        return;
    }

    std::string ext = epi::GetExtension(name);

    epi::StringLowerASCII(ext);

    if (ext == ".edm")
        I_Error("Demos are not supported\n");

    filekind_e kind;

    if (ext == ".wad")
        kind = FLKIND_PWad;
    else if (ext == ".pk3" || ext == ".epk" || ext == ".zip" || ext == ".vwad")
        kind = FLKIND_EPK;
    else if (ext == ".rts")
        kind = FLKIND_RTS;
    else if (ext == ".ddf" || ext == ".ldf")
        kind = FLKIND_DDF;
    else if (ext == ".deh" || ext == ".bex")
        kind = FLKIND_Deh;
    else
    {
        if (!ignore_unknown)
            I_Error("unknown file type: %s\n", name.c_str());
        return;
    }

    std::string filename = M_ComposeFileName(game_dir, name);
    W_AddFilename(filename, kind);
}

static void AddCommandLineFiles(void)
{
    // first handle "loose" files (arguments before the first option)

    int p;

    for (p = 1; p < int(argv::list.size()) && !argv::IsOption(p); p++)
    {
        AddSingleCmdLineFile(argv::list[p], false);
    }

    // next handle the -file option (we allow multiple uses)

    p = argv::Find("file");

    while (p > 0 && p < int(argv::list.size()) && (!argv::IsOption(p) || epi::StringCompare(argv::list[p], "-file") == 0))
    {
        // the parms after p are wadfile/lump names,
        // go until end of parms or another '-' preceded parm
        if (!argv::IsOption(p))
            AddSingleCmdLineFile(argv::list[p], false);

        p++;
    }

    // scripts....

    p = argv::Find("script");

    while (p > 0 && p < int(argv::list.size()) && (!argv::IsOption(p) || epi::StringCompare(argv::list[p], "-script") == 0))
    {
        // the parms after p are script filenames,
        // go until end of parms or another '-' preceded parm
        if (!argv::IsOption(p))
        {
            std::string ext = epi::GetExtension(argv::list[p]);
            // sanity check...
            if (epi::StringCaseCompareASCII(ext, ".wad") == 0 || epi::StringCaseCompareASCII(ext, ".pk3") == 0 || epi::StringCaseCompareASCII(ext, ".zip") == 0 ||
                epi::StringCaseCompareASCII(ext, ".epk") == 0 || epi::StringCaseCompareASCII(ext, ".vwad") == 0 ||
                epi::StringCaseCompareASCII(ext, ".ddf") == 0 || epi::StringCaseCompareASCII(ext, ".deh") == 0 || epi::StringCaseCompareASCII(ext, ".bex") == 0)
            {
                I_Error("Illegal filename for -script: %s\n", argv::list[p].c_str());
            }

            std::string filename = M_ComposeFileName(game_dir, argv::list[p]);
            W_AddFilename(filename, FLKIND_RTS);
        }

        p++;
    }

    // dehacked/bex....

    p = argv::Find("deh");

    while (p > 0 && p < int(argv::list.size()) && (!argv::IsOption(p) || epi::StringCompare(argv::list[p], "-deh") == 0))
    {
        // the parms after p are Dehacked/BEX filenames,
        // go until end of parms or another '-' preceded parm
        if (!argv::IsOption(p))
        {
            std::string ext = epi::GetExtension(argv::list[p]);
            // sanity check...
            if (epi::StringCaseCompareASCII(ext, ".wad") == 0 || epi::StringCaseCompareASCII(ext, ".epk") == 0 || epi::StringCaseCompareASCII(ext, ".pk3") == 0 ||
                epi::StringCaseCompareASCII(ext, ".zip") == 0 || epi::StringCaseCompareASCII(ext, ".vwad") == 0 ||
                epi::StringCaseCompareASCII(ext, ".ddf") == 0 || epi::StringCaseCompareASCII(ext, ".rts") == 0)
            {
                I_Error("Illegal filename for -deh: %s\n", argv::list[p].c_str());
            }

            std::string filename = M_ComposeFileName(game_dir, argv::list[p]);
            W_AddFilename(filename, FLKIND_Deh);
        }

        p++;
    }

    // directories....

    p = argv::Find("dir");

    while (p > 0 && p < int(argv::list.size()) && (!argv::IsOption(p) || epi::StringCompare(argv::list[p], "-dir") == 0))
    {
        // the parms after p are directory names,
        // go until end of parms or another '-' preceded parm
        if (!argv::IsOption(p))
        {
            std::string dirname = M_ComposeFileName(game_dir, argv::list[p]);
            W_AddFilename(dirname, FLKIND_Folder);
        }

        p++;
    }

    // handle -ddf option (backwards compatibility)

    std::string ps = argv::Value("ddf");

    if (!ps.empty())
    {
        std::string filename = M_ComposeFileName(game_dir, ps);
        W_AddFilename(filename, FLKIND_Folder);
    }
}

static void Add_Autoload(void)
{

    std::vector<epi::DirectoryEntry> fsd;
    std::string folder = epi::PathAppend(game_dir, "autoload");

    if (!ReadDirectory(fsd, folder, "*.*"))
    {
        I_Warning("Failed to read %s directory!\n", folder.c_str());
    }
    else
    {
        for (size_t i = 0; i < fsd.size(); i++)
        {
            if (!fsd[i].is_dir)
                AddSingleCmdLineFile(fsd[i].name, true);
        }
    }
    fsd.clear();
    folder = epi::PathAppend(folder, game_base);
    if (!ReadDirectory(fsd, folder, "*.*"))
    {
        I_Warning("Failed to read %s directory!\n", folder.c_str());
    }
    else
    {
        for (size_t i = 0; i < fsd.size(); i++)
        {
            if (!fsd[i].is_dir)
                AddSingleCmdLineFile(fsd[i].name, true);
        }
    }
    fsd.clear();

    // Check if autoload folder stuff is in home_dir as well, make the folder/subfolder if they don't exist (in home_dir
    // only)
    folder = epi::PathAppend(home_dir, "autoload");
    if (!epi::IsDirectory(folder))
        epi::MakeDirectory(folder);

    if (!ReadDirectory(fsd, folder, "*.*"))
    {
        I_Warning("Failed to read %s directory!\n", folder.c_str());
    }
    else
    {
        for (size_t i = 0; i < fsd.size(); i++)
        {
            if (!fsd[i].is_dir)
                AddSingleCmdLineFile(fsd[i].name, true);
        }
    }
    fsd.clear();
    folder = epi::PathAppend(folder, game_base);
    if (!epi::IsDirectory(folder))
        epi::MakeDirectory(folder);
    if (!ReadDirectory(fsd, folder, "*.*"))
    {
        I_Warning("Failed to read %s directory!\n", folder.c_str());
    }
    else
    {
        for (size_t i = 0; i < fsd.size(); i++)
        {
            if (!fsd[i].is_dir)
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
    S_StopMusic();

    // Pause to allow sounds to finish
    for (int loop = 0; loop < 30; loop++)
    {
        S_SoundTicker();
        I_Sleep(50);
    }

    P_Shutdown();

    S_Shutdown();
    R_Shutdown();
    N_Shutdown();
}

// Local Prototypes
static void E_Startup();
static void E_Shutdown(void);

static void E_Startup(void)
{
    CON_InitConsole();

    // -AJA- 2000/02/02: initialise global gameflags to defaults
    global_flags = default_gameflags;

    InitDirectories();

    // Version check ?
    if (argv::Find("version") > 0)
    {
        // -AJA- using I_Error here, since I_Printf crashes this early on
        I_Error("\n%s version is %s\n", appname.c_str(), edgeversion.c_str());
    }

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
    // Must be done after WAD and DDF loading to check for potential
    // overrides of lump-specific image/sound/DDF defines
    W_DoPackSubstitutions();
    I_StartupMusic(); // Must be done after all files loaded to locate appropriate GENMIDI lump
    V_InitPalette();

    DDF_CleanUp();
    SetLanguage();
    W_ReadUMAPINFOLumps();

    W_InitFlats();
    W_InitTextures();
    W_ImageCreateUser();
    E_PickLoadingScreen();
    E_PickMenuScreen();

    HU_Init();
    CON_Start();
    CON_CreateQuitScreen();
    SpecialWadVerify();
    W_BuildNodes();
    M_InitMiscConVars();
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
    if (LUA_UseLuaHud())
    {
        LUA_Init();
        LUA_LoadScripts();
    }
    else
    {
        VM_InitCoal();
        VM_LoadScripts();
    }
}

static void E_Shutdown(void)
{
    /* TODO: E_Shutdown */
}

static void E_InitialState(void)
{
    I_Debugf("- Setting up Initial State...\n");

    std::string ps;

    // do loadgames first, as they contain all of the
    // necessary state already (in the savegame).

    if (argv::Find("playdemo") > 0 || argv::Find("timedemo") > 0 || argv::Find("record") > 0)
    {
        I_Error("Demos are no longer supported\n");
    }

    ps = argv::Value("loadgame");
    if (!ps.empty())
    {
        G_DeferredLoadGame(atoi(ps.c_str()));
        return;
    }

    bool warp = false;

    // get skill / episode / map from parms
    std::string warp_map;
    skill_t     warp_skill      = sk_medium;
    int         warp_deathmatch = 0;

    int bots = 0;

    ps = argv::Value("bots");
    if (!ps.empty())
        bots = atoi(ps.c_str());

    ps = argv::Value("warp");
    if (!ps.empty())
    {
        warp     = true;
        warp_map = ps;
    }

    // -KM- 1999/01/29 Use correct skill: 1 is easiest, not 0
    ps = argv::Value("skill");
    if (!ps.empty())
    {
        warp       = true;
        warp_skill = (skill_t)(atoi(ps.c_str()) - 1);
    }

    // deathmatch check...
    int pp = argv::Find("deathmatch");
    if (pp > 0)
    {
        warp_deathmatch = 1;

        if (pp + 1 < int(argv::list.size()) && !argv::IsOption(pp + 1))
            warp_deathmatch = HMM_MAX(1, atoi(argv::list[pp + 1].c_str()));

        warp = true;
    }
    else if (argv::Find("altdeath") > 0)
    {
        warp_deathmatch = 2;

        warp = true;
    }

    // start the appropriate game based on parms
    if (!warp)
    {
        I_Debugf("- Startup: showing title screen.\n");
        E_StartTitle();
        return;
    }

    newgame_params_c params;

    params.skill      = warp_skill;
    params.deathmatch = warp_deathmatch;
    params.level_skip = true;

    if (warp_map.length() > 0)
        params.map = G_LookupMap(warp_map.c_str());
    else
        params.map = G_LookupMap("1");

    if (!params.map)
        I_Error("-warp: no such level '%s'\n", warp_map.c_str());

    SYS_ASSERT(G_MapExists(params.map));
    SYS_ASSERT(params.map->episode_);

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

    E_Startup();

    E_InitialState();

    CON_MessageColor(SG_YELLOW_RGBA32);
    I_Printf("%s v%s initialisation complete.\n", appname.c_str(), edgeversion.c_str());

    I_Debugf("- Entering game loop...\n");

#ifndef EDGE_WEB
    while (!(app_state & APP_STATE_PENDING_QUIT))
    {
        // We always do this once here, although the engine may
        // makes in own calls to keep on top of the event processing
        I_ControlGetEvents();

        if (app_state & APP_STATE_ACTIVE)
            E_Tick();
        else if (!n_busywait.d)
        {
            I_Sleep(5);
        }
    }
#else
    return;
#endif

    E_Shutdown(); // Shutdown whatever at this point
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
    EDGE_ZoneScoped;
    
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
    for (; counts > 0; counts--)
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
