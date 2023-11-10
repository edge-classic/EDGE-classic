//---------------------------------------------------------------------------
//  EDGE Main Init + Program Loop Code
//----------------------------------------------------------------------------
//
//  Copyright (c) 1999-2023  The EDGE Team.
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

#include "i_defs.h"
#include "i_sdlinc.h"
#include "e_main.h"
#include "i_defs_gl.h"
#include "i_movie.h"

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

FILE *logfile   = NULL;
FILE *debugfile = NULL;

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

std::filesystem::path brandingfile;
std::filesystem::path cfgfile;
std::filesystem::path epkfile;
std::string           game_base;

std::filesystem::path cache_dir;
std::filesystem::path game_dir;
std::filesystem::path home_dir;
std::filesystem::path save_dir;
std::filesystem::path shot_dir;

// not using DEF_CVAR here since var name != cvar name
cvar_c m_language("language", "ENGLISH", CVAR_ARCHIVE);

DEF_CVAR(logfilename, "edge-classic.log", CVAR_NO_RESET)
DEF_CVAR(configfilename, "edge-classic.cfg", CVAR_NO_RESET)
DEF_CVAR(debugfilename, "debug.txt", CVAR_NO_RESET)
DEF_CVAR(windowtitle, "EDGE-Classic", CVAR_NO_RESET)
DEF_CVAR(edgeversion, "1.36", CVAR_NO_RESET)
DEF_CVAR(orgname, "EDGE Team", CVAR_NO_RESET)
DEF_CVAR(appname, "EDGE-Classic", CVAR_NO_RESET)
DEF_CVAR(homepage, "https://edge-classic.github.io", CVAR_NO_RESET)

DEF_CVAR_CLAMPED(r_overlay, "0", CVAR_ARCHIVE, 0, 6)

DEF_CVAR_CLAMPED(r_titlescaling, "0", CVAR_ARCHIVE, 0, 3)

DEF_CVAR(g_aggression, "0", CVAR_ARCHIVE)

DEF_CVAR(ddf_strict, "0", CVAR_ARCHIVE)
DEF_CVAR(ddf_lax, "0", CVAR_ARCHIVE)
DEF_CVAR(ddf_quiet, "0", CVAR_ARCHIVE)

static const image_c *loading_image = NULL;

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
            if (r_titlescaling.d == 2) // Stretch
                HUD_StretchImage(hud_x_left, 0, hud_x_right - hud_x_left, 200, loading_image, 0, 0);
            else
            {
                if (r_titlescaling.d == 3) // Fill Border
                {
                    if ((float)loading_image->actual_w / loading_image->actual_h < (float)SCREENWIDTH / SCREENHEIGHT)
                    {
                        if (!loading_image->blurred_version)
                            W_ImageStoreBlurred(loading_image, 0.75f);
                        HUD_StretchImage(-320, -200, 960, 600, loading_image->blurred_version, 0, 0);
                    }
                }
                HUD_DrawImageTitleWS(loading_image);
            }
            HUD_SolidBox(25, 25, 295, 175, RGB_MAKE(0, 0, 0));
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
            const image_c *overlay = W_ImageLookup(hud_overlays.at(r_overlay.d).c_str(), INS_Graphic, ILF_Null);
            if (overlay)
                HUD_RawImage(0, 0, SCREENWIDTH, SCREENHEIGHT, overlay, 0, 0, SCREENWIDTH / IM_WIDTH(overlay),
                             SCREENHEIGHT / IM_HEIGHT(overlay));
        }

        if (v_gamma.f < 0)
        {
            int col = (1.0f + v_gamma.f) * 255;
            glEnable(GL_BLEND);
            glBlendFunc(GL_ZERO, GL_SRC_COLOR);
            HUD_SolidBox(hud_x_left, 0, hud_x_right, 200, RGB_MAKE(col, col, col));
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDisable(GL_BLEND);
        }
        else if (v_gamma.f > 0)
        {
            int col = v_gamma.f * 255;
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

    argv::CheckBooleanParm("am_keydoorblink", &am_keydoorblink, false);

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
    E_ProgressMessage("Verifying EDGE-DEFS version...");

    epi::file_c *data = W_OpenPackFile("version.txt");

    if (!data)
        I_Error("Version file not found. Get edge_defs.epk at https://github.com/edge-classic/EDGE-classic");

    // parse version number
    std::string verstring = data->ReadText();
    const char *s         = verstring.data();
    int         epk_ver   = atoi(s) * 100;

    while (isdigit(*s))
        s++;
    s++;
    epk_ver += atoi(s);

    delete data;

    float real_ver = epk_ver / 100.0;

    I_Printf("EDGE-DEFS.EPK version %1.2f found.\n", real_ver);

    if (real_ver < edgeversion.f)
    {
        I_Error("EDGE-DEFS.EPK is an older version (got %1.2f, expected %1.2f)\n", real_ver, edgeversion.f);
    }
    else if (real_ver > edgeversion.f)
    {
        I_Warning("EDGE-DEFS.EPK is a newer version (got %1.2f, expected %1.2f)\n", real_ver, edgeversion.f);
    }
}

//
// ShowNotice
//
static void ShowNotice(void)
{
    CON_MessageColor(RGB_MAKE(64, 192, 255));

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
    if (nodrawers)
        return; // for comparative timing / profiling

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
        const image_c *overlay = W_ImageLookup(hud_overlays.at(r_overlay.d).c_str(), INS_Graphic, ILF_Null);
        if (overlay)
            HUD_RawImage(0, 0, SCREENWIDTH, SCREENHEIGHT, overlay, 0, 0, SCREENWIDTH / IM_WIDTH(overlay),
                         SCREENHEIGHT / IM_HEIGHT(overlay));
    }

    if (v_gamma.f < 0)
    {
        int col = (1.0f + v_gamma.f) * 255;
        glEnable(GL_BLEND);
        glBlendFunc(GL_ZERO, GL_SRC_COLOR);
        HUD_SolidBox(hud_x_left, 0, hud_x_right, 200, RGB_MAKE(col, col, col));
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_BLEND);
    }
    else if (v_gamma.f > 0)
    {
        int col = v_gamma.f * 255;
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

    I_FinishFrame(); // page flip or blit buffer
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
        if (r_titlescaling.d == 2) // Stretch
            HUD_StretchImage(hud_x_left, 0, hud_x_right - hud_x_left, 200, title_image, 0, 0);
        else
        {
            if (r_titlescaling.d == 3) // Fill Border
            {
                if ((float)title_image->actual_w / title_image->actual_h < (float)SCREENWIDTH / SCREENHEIGHT)
                {
                    if (!title_image->blurred_version)
                        W_ImageStoreBlurred(title_image, 0.75f);
                    HUD_StretchImage(-320, -200, 960, 600, title_image->blurred_version, 0, 0);
                }
            }
            HUD_DrawImageTitleWS(title_image);
        }
    }
    else
    {
        HUD_SolidBox(0, 0, 320, 200, RGB_MAKE(0, 0, 0));
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
    title_pic  = 29999;

    // prevent an infinite loop
    for (int loop = 0; loop < 100; loop++)
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
        if (title_pic == 0 && g->firstmap != "" && W_CheckNumForName(g->firstmap.c_str()) == -1)
        {
            title_game = (title_game + 1) % gamedefs.GetSize();
            title_pic  = 0;
            continue;
        }

        // ignore non-existing images
        loading_image = W_ImageLookup(g->titlepics[title_pic].c_str(), INS_Graphic, ILF_Null);

        if (!loading_image)
        {
            title_pic++;
            continue;
        }

        // found one !!
        title_game = gamedefs.GetSize() - 1;
        title_pic  = 29999;
        return;
    }

    // not found
    title_game    = gamedefs.GetSize() - 1;
    title_pic     = 29999;
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
    for (int loop = 0; loop < 100; loop++)
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
        if (title_pic == 0 && g->firstmap != "" && W_CheckNumForName(g->firstmap.c_str()) == -1)
        {
            title_game = (title_game + 1) % gamedefs.GetSize();
            title_pic  = 0;
            continue;
        }

        // ignore non-existing images
        title_image = W_ImageLookup(g->titlepics[title_pic].c_str(), INS_Graphic, ILF_Null);

        if (!title_image)
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

    title_image     = NULL;
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
    // Get the App Directory from parameter.

    // Note: This might need adjusting for Apple
    std::filesystem::path s = std::filesystem::u8path(SDL_GetBasePath());

    game_dir = s;
    s        = std::filesystem::u8path(argv::Value("game"));
    if (!s.empty())
        game_dir = s;

    brandingfile = epi::PATH_Join(game_dir, EDGEBRANDINGFILE);

    M_LoadBranding();

    // add parameter file "appdir/parms" if it exists.
    std::filesystem::path parms = epi::PATH_Join(game_dir, "parms");

    if (epi::FS_Access(parms, epi::file_c::ACCESS_READ))
    {
        // Insert it right after the app parameter
        argv::ApplyResponseFile(parms);
    }

    // config file - check for portable config
    s = std::filesystem::u8path(argv::Value("config"));
    if (!s.empty())
    {
        cfgfile = s;
    }
    else
    {
        cfgfile = epi::PATH_Join(game_dir, configfilename.s);
        if (epi::FS_Access(cfgfile, epi::file_c::ACCESS_READ) || argv::Find("portable") > 0)
            home_dir = game_dir;
        else
            cfgfile.clear();
    }

    if (home_dir.empty())
    {
        s = std::filesystem::u8path(argv::Value("home"));
        if (!s.empty())
            home_dir = s;
    }

#ifdef _WIN32
    if (home_dir.empty())
        home_dir = std::filesystem::u8path(SDL_GetPrefPath(nullptr, appname.c_str()));
#else
    if (home_dir.empty())
        home_dir = std::filesystem::u8path(SDL_GetPrefPath(orgname.c_str(), appname.c_str()));
#endif

    if (!epi::FS_IsDir(home_dir))
    {
        if (!epi::FS_MakeDir(home_dir))
            I_Error("InitDirectories: Could not create directory at %s!\n", home_dir.u8string().c_str());
    }

    if (cfgfile.empty())
        cfgfile = epi::PATH_Join(home_dir, configfilename.s);

    // edge-defs.epk file
    s = std::filesystem::u8path(argv::Value("defs"));
    if (!s.empty())
    {
        epkfile = s;
    }
    else
    {
        if (epi::FS_IsDir(epi::PATH_Join(game_dir, "edge_defs")))
            epkfile = epi::PATH_Join(game_dir, "edge_defs");
        else
            epkfile = epi::PATH_Join(game_dir, "edge-defs.epk");
    }

    // cache directory
    cache_dir = epi::PATH_Join(home_dir, CACHEDIR);

    if (!epi::FS_IsDir(cache_dir))
        epi::FS_MakeDir(cache_dir);

    // savegame directory
    save_dir = epi::PATH_Join(home_dir, SAVEGAMEDIR);

    if (!epi::FS_IsDir(save_dir))
        epi::FS_MakeDir(save_dir);

    SV_ClearSlot("current");

    // screenshot directory
    shot_dir = epi::PATH_Join(home_dir, SCRNSHOTDIR);

    if (!epi::FS_IsDir(shot_dir))
        epi::FS_MakeDir(shot_dir);
}

// Get rid of legacy GWA/HWA files or XWA files older than 6 months

static void PurgeCache(void)
{
    const std::filesystem::file_time_type expiry =
        std::filesystem::file_time_type::clock::now() - std::chrono::hours(4320);

    std::vector<epi::dir_entry_c> fsd;

    if (!FS_ReadDir(fsd, cache_dir, "*.*"))
    {
        I_Error("PurgeCache: Failed to read '%s' directory!\n", cache_dir.u8string().c_str());
    }
    else
    {
        for (size_t i = 0; i < fsd.size(); i++)
        {
            if (!fsd[i].is_dir)
            {
                if (fsd[i].name.extension().compare(".gwa") == 0)
                    epi::FS_Delete(fsd[i].name);
                else if (fsd[i].name.extension().compare(".hwa") == 0)
                    epi::FS_Delete(fsd[i].name);
                else if (fsd[i].name.extension().compare(".xwa") == 0)
                {
                    if (std::filesystem::last_write_time(fsd[i].name) < expiry)
                    {
                        epi::FS_Delete(fsd[i].name);
                    }
                }
            }
        }
    }
}

// If a valid IWAD (or EDGEGAME) is found, return the appopriate game_base string ("doom2", "heretic", etc)
static std::string CheckPackForGameFiles(std::filesystem::path check_pack, filekind_e check_kind, int *check_score)
{
    data_file_c *check_pack_df = new data_file_c(check_pack, check_kind);
    SYS_ASSERT(check_pack_df);
    Pack_PopulateOnly(check_pack_df);
    if (Pack_FindStem(check_pack_df->pack, "EDGEGAME"))
    {
        delete check_pack_df;
        if (check_score)
            *check_score = 15;
        return "CUSTOM";
    }
    else
    {
        std::string check_base = Pack_CheckForIWADs(check_pack_df, check_score);
        delete check_pack_df;
        return check_base;
    }
}

//
// Adds main game content and edge-defs folder/EPK
//
static void IdentifyVersion(void)
{
    if (epi::FS_IsDir(epkfile))
        W_AddFilename(epkfile, FLKIND_EFolder);
    else
    {
        if (!epi::FS_Access(epkfile, epi::file_c::ACCESS_READ))
            I_Error("IdentifyVersion: Could not find required %s.%s!\n", REQUIREDEPK, "epk");
        W_AddFilename(epkfile, FLKIND_EEPK);
    }

    I_Debugf("- Identify Version\n");

    // Check -iwad parameter, find out if it is the IWADs directory
    std::filesystem::path              iwad_par;
    std::filesystem::path              iwad_file;
    std::filesystem::path              iwad_dir;
    std::vector<std::filesystem::path> iwad_dir_vector;

    std::filesystem::path s = argv::Value("iwad");

    iwad_par = s;

    if (!iwad_par.empty())
    {
        // Treat directories passed via -iwad as a pack file and check accordingly
        if (epi::FS_IsDir(iwad_par))
        {
            game_base = CheckPackForGameFiles(iwad_par, FLKIND_IFolder, nullptr);
            if (game_base.empty())
                I_Error("Folder %s passed via -iwad parameter, but no IWAD or EDGEGAME file detected!\n",
                        iwad_par.u8string().c_str());
            else
            {
                W_AddFilename(iwad_par, FLKIND_IFolder);
                return;
            }
        }
    }

    // If we haven't yet set the IWAD directory, then we check
    // the DOOMWADDIR environment variable
#ifdef _WIN32
    s = env::Value("DOOMWADDIR");
#else
    if (getenv("DOOMWADDIR"))
        s = getenv("DOOMWADDIR");
#endif

    if (!s.empty() && epi::FS_IsDir(s))
        iwad_dir_vector.push_back(s);

    // Should the IWAD directory not be set by now, then we
    // use our standby option of the current directory.
    if (iwad_dir.empty())
        iwad_dir = "."; // should this be hardcoded to the game or home directory instead? - Dasho

    // Add DOOMWADPATH directories if they exist
    s.clear();
#ifdef _WIN32
    s = env::Value("DOOMWADPATH");
#else
    if (getenv("DOOMWADPATH"))
        s = getenv("DOOMWADPATH");
#endif
    if (!s.empty())
    {
        for (auto dir : epi::STR_SepStringVector(s.string(), ':'))
            iwad_dir_vector.push_back(dir);
    }

    // Should the IWAD Parameter not be empty then it means
    // that one was given which is not a directory. Therefore
    // we assume it to be a name
    if (!iwad_par.empty())
    {
        std::filesystem::path fn = iwad_par;

        // Is it missing the extension?
        if (epi::PATH_GetExtension(iwad_par).empty())
        {
            fn.replace_extension(
                ".wad"); // We will still be checking EPKs if needed; but by the numbers .wad is a good initial search
        }

        // If no directory given use the IWAD directory
        std::filesystem::path dir = epi::PATH_GetDir(fn);
        if (dir.empty())
            iwad_file = epi::PATH_Join(iwad_dir, fn.string());
        else
            iwad_file = fn;

        if (!epi::FS_Access(iwad_file, epi::file_c::ACCESS_READ))
        {
            // Check DOOMWADPATH directories if present
            if (!iwad_dir_vector.empty())
            {
                for (size_t i = 0; i < iwad_dir_vector.size(); i++)
                {
                    iwad_file = epi::PATH_Join(iwad_dir_vector[i], fn.string());
                    if (epi::FS_Access(iwad_file, epi::file_c::ACCESS_READ))
                        goto foundindoomwadpath;
                }
            }
        }
        else
            goto foundindoomwadpath;

        // If we get here, try .epk and error out if we still can't access what was passed to us
        iwad_file.replace_extension(".epk");

        if (!epi::FS_Access(iwad_file, epi::file_c::ACCESS_READ))
        {
            // Check DOOMWADPATH directories if present
            if (!iwad_dir_vector.empty())
            {
                for (size_t i = 0; i < iwad_dir_vector.size(); i++)
                {
                    iwad_file = epi::PATH_Join(iwad_dir_vector[i], fn.string());
                    if (epi::FS_Access(iwad_file, epi::file_c::ACCESS_READ))
                        goto foundindoomwadpath;
                }
                I_Error("IdentifyVersion: Unable to access specified '%s'", fn.u8string().c_str());
            }
            else
                I_Error("IdentifyVersion: Unable to access specified '%s'", fn.u8string().c_str());
        }

    foundindoomwadpath:

        if (epi::case_cmp(epi::PATH_GetExtension(iwad_file).string(), ".wad") == 0)
        {
            epi::file_c *game_test = epi::FS_Open(iwad_file, epi::file_c::ACCESS_READ | epi::file_c::ACCESS_BINARY);
            game_base              = W_CheckForUniqueLumps(game_test, nullptr);
            delete game_test;
            if (!game_base.empty())
                W_AddFilename(iwad_file, FLKIND_IWad);
            else
                I_Error("IdentifyVersion: Could not identify '%s' as a valid IWAD!\n", fn.u8string().c_str());
        }
        else
        {
            game_base = CheckPackForGameFiles(iwad_file, FLKIND_IPK, nullptr);
            if (!game_base.empty())
                W_AddFilename(iwad_file, FLKIND_IPK);
            else
                I_Error("IdentifyVersion: Could not identify '%s' as a valid IWAD!\n", fn.u8string().c_str());
        }
    }
    else
    {
        std::filesystem::path location;

        // Track the "best" game file found throughout the various paths based on scores stored in game_checker
        int                   best_score = 0;
        std::filesystem::path best_match;
        filekind_e            best_kind = FLKIND_IWad;

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

            if (!FS_ReadDir(fsd, location, "*.wad"))
            {
                I_Warning("IdenfityVersion: Failed to read '%s' directory!\n", location.u8string().c_str());
            }
            else
            {
                for (size_t j = 0; j < fsd.size(); j++)
                {
                    if (!fsd[j].is_dir)
                    {
                        epi::file_c *game_test =
                            epi::FS_Open(fsd[j].name, epi::file_c::ACCESS_READ | epi::file_c::ACCESS_BINARY);
                        int         test_score = 0;
                        std::string test_base  = W_CheckForUniqueLumps(game_test, &test_score);
                        delete game_test;
                        if (test_score > best_score)
                        {
                            best_score = test_score;
                            game_base  = test_base;
                            best_match = fsd[j].name;
                            best_kind  = FLKIND_IWad;
                        }
                    }
                }
            }
            if (!FS_ReadDir(fsd, location, "*.epk"))
            {
                I_Warning("IdenfityVersion: Failed to read '%s' directory!\n", location.u8string().c_str());
            }
            else
            {
                for (size_t j = 0; j < fsd.size(); j++)
                {
                    if (!fsd[j].is_dir)
                    {
                        int         test_score = 0;
                        std::string test_base  = CheckPackForGameFiles(fsd[j].name, FLKIND_EPK, &test_score);
                        if (test_score > best_score)
                        {
                            best_score = test_score;
                            game_base  = test_base;
                            best_match = fsd[j].name;
                            best_kind  = FLKIND_IPK;
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

                std::vector<epi::dir_entry_c> fsd;

                if (!FS_ReadDir(fsd, location, "*.wad"))
                {
                    I_Warning("IdenfityVersion: Failed to read '%s' directory!\n", location.u8string().c_str());
                }
                else
                {
                    for (size_t j = 0; j < fsd.size(); j++)
                    {
                        if (!fsd[j].is_dir)
                        {
                            epi::file_c *game_test =
                                epi::FS_Open(fsd[j].name, epi::file_c::ACCESS_READ | epi::file_c::ACCESS_BINARY);
                            int         test_score = 0;
                            std::string test_base  = W_CheckForUniqueLumps(game_test, &test_score);
                            delete game_test;
                            if (test_score > best_score)
                            {
                                best_score = test_score;
                                game_base  = test_base;
                                best_match = fsd[j].name;
                                best_kind  = FLKIND_IWad;
                            }
                        }
                    }
                }
                if (!FS_ReadDir(fsd, location, "*.epk"))
                {
                    I_Warning("IdenfityVersion: Failed to read '%s' directory!\n", location.u8string().c_str());
                }
                else
                {
                    for (size_t j = 0; j < fsd.size(); j++)
                    {
                        if (!fsd[j].is_dir)
                        {
                            int         test_score = 0;
                            std::string test_base  = CheckPackForGameFiles(fsd[j].name, FLKIND_EPK, &test_score);
                            if (test_score > best_score)
                            {
                                best_score = test_score;
                                game_base  = test_base;
                                best_match = fsd[j].name;
                                best_kind  = FLKIND_IPK;
                            }
                        }
                    }
                }
            }
        }

        if (best_score == 0)
            I_Error("IdentifyVersion: No IWADs or standalone packs found!\n");
        else
            W_AddFilename(best_match, best_kind);
    }

    I_Debugf("GAME BASE = [%s]\n", game_base.c_str());
}

// Add game-specific base EPKs (widepix, skyboxes, etc) - Dasho
static void Add_Base(void)
{
    if (epi::case_cmp("CUSTOM", game_base) == 0)
        return; // Standalone EDGE IWADs/EPKs should already contain their necessary resources and definitions - Dasho
    std::filesystem::path base_path = epi::PATH_Join(game_dir, "edge_base");
    std::string           base_wad  = game_base;
    epi::str_lower(base_wad);
    base_path = epi::PATH_Join(base_path, base_wad);
    if (epi::FS_IsDir(base_path))
        W_AddFilename(base_path, FLKIND_EFolder);
    else if (epi::FS_Access(base_path.replace_extension(".epk"), epi::file_c::ACCESS_READ))
        W_AddFilename(base_path, FLKIND_EEPK);
    else
        I_Error("%s not found for the %s IWAD! Check the /edge_base folder of your %s install!\n",
                base_path.filename().u8string().c_str(), game_base.c_str(), appname.c_str());
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

    I_Printf("Executable path: '%s'\n", exe_path.u8string().c_str());

    argv::DebugDumpArgs();
}

static void SetupLogAndDebugFiles(void)
{
    // -AJA- 2003/11/08 The log file gets all CON_Printfs, I_Printfs,
    //                  I_Warnings and I_Errors.

    std::filesystem::path log_fn(epi::PATH_Join(home_dir, logfilename.s));
    std::filesystem::path debug_fn(epi::PATH_Join(home_dir, debugfilename.s));

    logfile   = NULL;
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
    std::string ext = epi::PATH_GetExtension(name).string();

    epi::str_lower(ext);

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

    for (p = 1; p < int(argv::list.size()) && !argv::IsOption(p); p++)
    {
        AddSingleCmdLineFile(std::filesystem::u8path(argv::list[p]), false);
    }

    // next handle the -file option (we allow multiple uses)

    p = argv::Find("file");

    while (p > 0 && p < int(argv::list.size()) && (!argv::IsOption(p) || epi::strcmp(argv::list[p], "-file") == 0))
    {
        // the parms after p are wadfile/lump names,
        // go until end of parms or another '-' preceded parm
        if (!argv::IsOption(p))
            AddSingleCmdLineFile(std::filesystem::u8path(argv::list[p]), false);

        p++;
    }

    // scripts....

    p = argv::Find("script");

    while (p > 0 && p < int(argv::list.size()) && (!argv::IsOption(p) || epi::strcmp(argv::list[p], "-script") == 0))
    {
        // the parms after p are script filenames,
        // go until end of parms or another '-' preceded parm
        if (!argv::IsOption(p))
        {
            std::string ext = epi::PATH_GetExtension(argv::list[p]).string();
            // sanity check...
            if (epi::case_cmp(ext, ".wad") == 0 || epi::case_cmp(ext, ".pk3") == 0 || epi::case_cmp(ext, ".zip") == 0 ||
                epi::case_cmp(ext, ".epk") == 0 || epi::case_cmp(ext, ".vwad") == 0 ||
                epi::case_cmp(ext, ".ddf") == 0 || epi::case_cmp(ext, ".deh") == 0 || epi::case_cmp(ext, ".bex") == 0)
            {
                I_Error("Illegal filename for -script: %s\n", argv::list[p].c_str());
            }

            std::filesystem::path filename = M_ComposeFileName(game_dir, argv::list[p]);
            W_AddFilename(filename, FLKIND_RTS);
        }

        p++;
    }

    // dehacked/bex....

    p = argv::Find("deh");

    while (p > 0 && p < int(argv::list.size()) && (!argv::IsOption(p) || epi::strcmp(argv::list[p], "-deh") == 0))
    {
        // the parms after p are Dehacked/BEX filenames,
        // go until end of parms or another '-' preceded parm
        if (!argv::IsOption(p))
        {
            std::string ext = epi::PATH_GetExtension(argv::list[p]).string();
            // sanity check...
            if (epi::case_cmp(ext, ".wad") == 0 || epi::case_cmp(ext, ".epk") == 0 || epi::case_cmp(ext, ".pk3") == 0 ||
                epi::case_cmp(ext, ".zip") == 0 || epi::case_cmp(ext, ".vwad") == 0 ||
                epi::case_cmp(ext, ".ddf") == 0 || epi::case_cmp(ext, ".rts") == 0)
            {
                I_Error("Illegal filename for -deh: %s\n", argv::list[p].c_str());
            }

            std::filesystem::path filename = M_ComposeFileName(game_dir, argv::list[p]);
            W_AddFilename(filename, FLKIND_Deh);
        }

        p++;
    }

    // directories....

    p = argv::Find("dir");

    while (p > 0 && p < int(argv::list.size()) && (!argv::IsOption(p) || epi::strcmp(argv::list[p], "-dir") == 0))
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
        W_AddFilename(filename, FLKIND_Folder);
    }
}

static void Add_Autoload(void)
{

    std::vector<epi::dir_entry_c> fsd;
    std::filesystem::path         folder = epi::PATH_Join(game_dir, "autoload");

    if (!FS_ReadDir(fsd, folder, "*.*"))
    {
        I_Warning("Failed to read %s directory!\n", folder.u8string().c_str());
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
    std::string lowercase_base = game_base;
    epi::str_lower(lowercase_base);
    folder = epi::PATH_Join(folder, lowercase_base);
    if (!FS_ReadDir(fsd, folder, "*.*"))
    {
        I_Warning("Failed to read %s directory!\n", folder.u8string().c_str());
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
    folder = epi::PATH_Join(home_dir, "autoload");
    if (!epi::FS_IsDir(folder))
        epi::FS_MakeDir(folder);

    if (!FS_ReadDir(fsd, folder, "*.*"))
    {
        I_Warning("Failed to read %s directory!\n", folder.u8string().c_str());
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
    folder = epi::PATH_Join(folder, lowercase_base);
    if (!epi::FS_IsDir(folder))
        epi::FS_MakeDir(folder);
    if (!FS_ReadDir(fsd, folder, "*.*"))
    {
        I_Warning("Failed to read %s directory!\n", folder.u8string().c_str());
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
    N_QuitNetGame();

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
            warp_deathmatch = MAX(1, atoi(argv::list[pp + 1].c_str()));

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
        // Throw a pack file called test.mpg in the mix and uncomment this for testing
        // (for now, this will be exposed via DDF shortly)
        //E_PlayMovie("test.mpg");
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

#ifdef _WIN32
    env::Init();
#endif

    try
    {
        E_Startup();

        E_InitialState();

        CON_MessageColor(RGB_MAKE(255, 255, 0));
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
        }
#else
        return;
#endif
    }
    catch (const std::exception &e)
    {
        I_Error("Unexpected internal failure occurred: %s\n", e.what());
    }

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
